
/* $Id: database.cpp 522 2016-04-08 08:05:58Z sausage $
 * EOSERV is released under the zlib license.
 * See LICENSE.txt for more info.
 */

#ifdef CLANG_MODULES_WORKAROUND
#include <ctime>
#include <csignal>
#endif // CLANG_MODULES_WORKAROUND

#include "database.hpp"

#include "console.hpp"
#include "util.hpp"
#include "util/variant.hpp"

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <exception>
#include <list>
#include <string>
#include <unordered_map>

#include "database_impl.hpp"

#ifndef ER_LOCK_WAIT_TIMEOUT
#define ER_LOCK_WAIT_TIMEOUT 1205
#endif

#ifndef DATABASE_MYSQL
#ifndef DATABASE_SQLITE
#ifndef DATABASE_SQLSERVER
#error At least one database driver must be selected
#endif // DATABASE_SQLSERVER
#endif // DATABASE_SQLITE
#endif // DATABASE_MYSQL

#ifdef DATABASE_SQLITE
#pragma comment(lib, "..\\sqlite\\sqlite3.lib")
#endif

#ifdef DATABASE_MYSQL
#ifdef _DEBUG
#pragma comment(lib, "..\\mysql\\lib\\debug\\libmariadb.lib")
#else
#pragma comment(lib, "..\\mysql\\lib\\libmariadb.lib")
#endif
#endif

struct Database::impl_
{
	union
	{
#ifdef DATABASE_MYSQL
		MYSQL *mysql_handle;
#endif // DATABASE_MYSQL
#ifdef DATABASE_SQLITE
		sqlite3 *sqlite_handle;
#endif // DATABASE_SQLITE
#ifdef DATABASE_SQLSERVER
		SQLHENV hEnv;
		SQLHDBC hConn;
		HSTMT hstmt;
#endif //DATABASE_SQLSERVER
	};
};


#ifdef DATABASE_SQLSERVER
void HandleSqlServerError(SQLSMALLINT handleType, SQLHANDLE handle, SQLRETURN code)
{
    SQLSMALLINT iRec = 0;
    SQLINTEGER iError;
    SQLCHAR messageBuff[1000];
    SQLCHAR stateBuff[SQL_SQLSTATE_SIZE+1];

    if (code == SQL_INVALID_HANDLE)
    {
        fwprintf(stderr, L"Invalid handle!\n");
        return;
    }

    while (SQLGetDiagRec(handleType,
                         handle,
                         ++iRec,
                         stateBuff,
                         &iError,
                         messageBuff,
                         (SQLSMALLINT)1000,
                         (SQLSMALLINT*)NULL) == SQL_SUCCESS)
    {
        // Hide data truncated..
        if (strncmp((const char *)stateBuff, "01004", 5))
        {
            Console::Err("[%5.5s] %s (%d)\n", stateBuff, messageBuff, iError);
        }
    }
}
#endif

static int sqlite_callback(void *data, int num, char *fields[], char *columns[])
{
	std::unordered_map<std::string, util::variant> result;
	std::string column;
	util::variant field;
	int i;

	for (i = 0; i < num; ++i)
	{
		if (columns[i] == NULL)
		{
			column = "";
		}
		else
		{
			column = columns[i];
		}

		if (fields[i] == NULL)
		{
			field = "";
		}
		else
		{
			field = fields[i];
		}

		result.insert(result.begin(), make_pair(column, field));
	}

	static_cast<Database *>(data)->callbackdata.push_back(result);
	return 0;
}

int Database_Result::AffectedRows()
{
	return this->affected_rows;
}

bool Database_Result::Error()
{
	return this->error;
}

Database::Bulk_Query_Context::Bulk_Query_Context(Database& db)
	: db(db)
	, pending(false)
{
	pending = db.BeginTransaction();
}

bool Database::Bulk_Query_Context::Pending() const
{
	return pending;
}

void Database::Bulk_Query_Context::RawQuery(const std::string& query)
{
	db.RawQuery(query.c_str());
}

void Database::Bulk_Query_Context::Commit()
{
	if (pending)
		db.Commit();

	pending = false;
}

void Database::Bulk_Query_Context::Rollback()
{
	if (pending)
		db.Rollback();

	pending = false;
}

Database::Bulk_Query_Context::~Bulk_Query_Context()
{
	if (pending)
		db.Rollback();
}

Database::Database()
	: impl(new impl_)
	, connected(false)
	, engine(Engine(0))
	, in_transaction(false)
{ }

Database::Database(Database::Engine type, const std::string& host, unsigned short port, const std::string& user, const std::string& pass, const std::string& db, bool connectnow)
{
	this->connected = false;

	if (connectnow)
	{
		this->Connect(type, host, port, user, pass, db);
	}
	else
	{
		this->engine = type;
		this->host = host;
		this->user = user;
		this->pass = pass;
		this->port = port;
		this->db = db;
	}
}

void Database::Connect(Database::Engine type, const std::string& host, unsigned short port, const std::string& user, const std::string& pass, const std::string& db)
{
	this->engine = type;
	this->host = host;
	this->user = user;
	this->pass = pass;
	this->port = port;
	this->db = db;

	if (this->connected)
	{
		return;
	}

	switch (type)
	{
#ifdef DATABASE_MYSQL
		case MySQL:
			if ((this->impl->mysql_handle = mysql_init(0)) == 0)
			{
				throw Database_OpenFailed(mysql_error(this->impl->mysql_handle));
			}

// Linux uses the ABI version number as part of the shared library name
#ifdef WIN32
			if (mysql_get_client_version() != MYSQL_VERSION_ID)
			{
				unsigned int client_version = mysql_get_client_version();

				Console::Err("MySQL client library version mismatch! Please recompile EOSERV with the correct MySQL client library.");
				Console::Err("  Expected version: %i.%i.%i", (MYSQL_VERSION_ID) / 10000, ((MYSQL_VERSION_ID) / 100) % 100, (MYSQL_VERSION_ID) % 100);
				Console::Err("  Library version:  %i.%i.%i", (client_version) / 10000, ((client_version) / 100) % 100, (client_version) % 100);
				Console::Err("Make sure EOSERV is using the correct version of libmariadb.dll");
				throw Database_OpenFailed("MySQL client library version mismatch");
			}
#endif

			if (mysql_real_connect(this->impl->mysql_handle, host.c_str(), user.c_str(), pass.c_str(), 0, this->port, 0, 0) != this->impl->mysql_handle)
			{
				throw Database_OpenFailed(mysql_error(this->impl->mysql_handle));
			}

// This check isn't really neccessary for EOSERV
#if 0
			if ((mysql_get_server_version(this->impl->mysql_handle) / 100) != (MYSQL_VERSION_ID) / 100)
			{
				Console::Wrn("MySQL server version mismatch.");
				Console::Wrn("  Server version: %s", mysql_get_server_info(this->impl->mysql_handle));
				Console::Wrn("  Client version: %s", mysql_get_client_info());
			}
#endif

			this->connected = true;

			if (mysql_select_db(this->impl->mysql_handle, db.c_str()) != 0)
			{
				this->Close();
				throw Database_OpenFailed(mysql_error(this->impl->mysql_handle));
			}

			break;
#endif // DATABASE_MYSQL

#ifdef DATABASE_SQLITE
		case SQLite:
			if (sqlite3_libversion_number() != SQLITE_VERSION_NUMBER)
			{
				Console::Err("SQLite library version mismatch! Please recompile EOSERV with the correct SQLite library.");
				Console::Err("  Expected version: %s", SQLITE_VERSION);
				Console::Err("  Library version:  %s", sqlite3_libversion());
#ifdef WIN32
				Console::Err("Make sure EOSERV is using the correct version of sqlite3.dll");
#endif // WIN32
				throw Database_OpenFailed("SQLite library version mismatch");
			}

			if (sqlite3_open(host.c_str(), &this->impl->sqlite_handle) != SQLITE_OK)
			{
				throw Database_OpenFailed(sqlite3_errmsg(this->impl->sqlite_handle));
			}

			this->connected = true;

			break;
#endif // DATABASE_SQLITE

#ifdef DATABASE_SQLSERVER
		case SqlServer:
		{
			// Set connected so that if a failure occurs the handles get closed properly in the destructor
			this->connected = true;

			SQLRETURN ret = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &this->impl->hEnv);
			if (ret != SQL_SUCCESS)
			{
				if (ret == SQL_ERROR)
					HandleSqlServerError(SQL_HANDLE_ENV, this->impl->hEnv, ret);
				throw Database_OpenFailed("Unable to allocate ODBC environment handle");
			}

			ret = SQLSetEnvAttr(this->impl->hEnv, SQL_ATTR_ODBC_VERSION, reinterpret_cast<SQLPOINTER>(SQL_OV_ODBC3), 0);
			if (ret != SQL_SUCCESS)
			{
				if (ret == SQL_ERROR)
					HandleSqlServerError(SQL_HANDLE_ENV, this->impl->hEnv, ret);
				throw Database_OpenFailed("Unable to set ODBC version attribute");
			}

			ret = SQLAllocHandle(SQL_HANDLE_DBC, this->impl->hEnv, &this->impl->hConn);
			if (ret != SQL_SUCCESS)
			{
				if (ret == SQL_ERROR)
					HandleSqlServerError(SQL_HANDLE_DBC, this->impl->hConn, ret);
				throw Database_OpenFailed("Unable to allocate ODBC connection handle");
			}

			char connStrBuff[4096] = { 0 };
			sprintf_s(connStrBuff, "DRIVER={SQL Server};SERVER={%s,%d};DATABASE={%s};UID={%s};PWD={%s}",
				this->host.c_str(),
				this->port,
				this->db.c_str(),
				this->user.c_str(),
				this->pass.c_str());

			char sqlRetConnStr[1024] = { 0 };
			ret = SQLDriverConnect(this->impl->hConn, NULL,
				reinterpret_cast<SQLCHAR*>(connStrBuff),
				SQL_NTS,
				reinterpret_cast<SQLCHAR*>(sqlRetConnStr),
				1024,
				NULL,
				SQL_DRIVER_NOPROMPT);
			if (ret != SQL_SUCCESS)
			{
				if (ret == SQL_ERROR)
					HandleSqlServerError(SQL_HANDLE_DBC, this->impl->hConn, ret);
				throw Database_OpenFailed("Unable to connect to target server");
			}

			ret = SQLAllocHandle(SQL_HANDLE_STMT, this->impl->hConn, &this->impl->hstmt);
			if (ret != SQL_SUCCESS)
			{
				if (ret == SQL_ERROR)
					HandleSqlServerError(SQL_HANDLE_STMT, this->impl->hstmt, ret);
				throw Database_OpenFailed("Unable to allocate ODBC statement handle");
			}

			break;
		}
#endif // DATABASE_SQLSERVER

		default:
			throw Database_OpenFailed("Invalid database engine.");
	}
}

void Database::Close()
{
	if (!this->connected)
	{
		return;
	}

	this->connected = false;

	switch (this->engine)
	{
#ifdef DATABASE_MYSQL
		case MySQL:
			mysql_close(this->impl->mysql_handle);
			break;
#endif // DATABASE_MYSQL

#ifdef DATABASE_SQLITE
		case SQLite:
			sqlite3_close(this->impl->sqlite_handle);
			break;
#endif // DATABASE_SQLITE

#ifdef DATABASE_SQLSERVER
		case SqlServer:
			SQLFreeHandle(SQL_HANDLE_STMT, this->impl->hstmt);
			SQLDisconnect(this->impl->hConn);
			SQLFreeHandle(SQL_HANDLE_DBC, this->impl->hConn);
			SQLFreeHandle(SQL_HANDLE_ENV, this->impl->hEnv);
			break;
#endif // DATABASE_SQLSERVER
	}
}

Database_Result Database::RawQuery(const char* query, bool tx_control)
{
	if (!this->connected)
	{
		throw Database_QueryFailed("Not connected to database.");
	}

	std::size_t query_length = std::strlen(query);

	Database_Result result;

#ifdef DATABASE_DEBUG
	Console::Dbg("%s", query);
#endif // DATABASE_DEBUG

	switch (this->engine)
	{
#ifdef DATABASE_MYSQL
		case MySQL:
		{
			MYSQL_RES* mresult = nullptr;
			MYSQL_FIELD* fields = nullptr;
			int num_fields = 0;

			exec_query:
			if (mysql_real_query(this->impl->mysql_handle, query, query_length) != 0)
			{
				int myerr = mysql_errno(this->impl->mysql_handle);
				int recovery_attempt = 0;

				if (myerr == CR_SERVER_GONE_ERROR || myerr == CR_SERVER_LOST || myerr == ER_LOCK_WAIT_TIMEOUT)
				{
					server_gone:

					if (++recovery_attempt > 10)
					{
						Console::Err("Could not re-connect to database. Halting server.");
						std::terminate();
					}

					Console::Wrn("Connection to database lost! Attempting to reconnect... (Attempt %i / 10)", recovery_attempt);
					this->Close();
					util::sleep(2.0 * recovery_attempt);

					try
					{
						this->Connect(this->engine, this->host, this->port, this->user, this->pass, this->db);
					}
					catch (const Database_OpenFailed& e)
					{
						Console::Err("Connection failed: %s", e.error());
					}

					if (!this->connected)
					{
						goto server_gone;
					}

					if (this->in_transaction)
					{
#ifdef DEBUG
						Console::Dbg("Replaying %i queries.", this->transaction_log.size());
#endif // DEBUG

						if (mysql_real_query(this->impl->mysql_handle, "START TRANSACTION", std::strlen("START TRANSACTION")) != 0)
						{
							int myerr = mysql_errno(this->impl->mysql_handle);

							if (myerr == CR_SERVER_GONE_ERROR || myerr == CR_SERVER_LOST || myerr == ER_LOCK_WAIT_TIMEOUT)
							{
								goto server_gone;
							}
							else
							{
								Console::Err("Error during recovery: %s", mysql_error(this->impl->mysql_handle));
								Console::Err("Halting server.");
								std::terminate();
							}
						}

						for (const std::string& q : this->transaction_log)
						{
#ifdef DATABASE_DEBUG
							Console::Dbg("%s", q.c_str());
#endif // DATABASE_DEBUG
							int myerr = 0;
							int query_result = mysql_real_query(this->impl->mysql_handle, q.c_str(), q.length());

							if (query_result == 0)
							{
								mresult = mysql_use_result(this->impl->mysql_handle);
							}

							myerr = mysql_errno(this->impl->mysql_handle);

							if (myerr == CR_SERVER_GONE_ERROR || myerr == CR_SERVER_LOST || myerr == ER_LOCK_WAIT_TIMEOUT)
							{
								goto server_gone;
							}
							else if (myerr)
							{
								Console::Err("Error during recovery: %s", mysql_error(this->impl->mysql_handle));
								Console::Err("Halting server.");
								std::terminate();
							}
							else
							{
								if (mresult)
								{
									mysql_free_result(mresult);
									mresult = nullptr;
								}
							}
						}
					}

					goto exec_query;
				}
				else
				{
					throw Database_QueryFailed(mysql_error(this->impl->mysql_handle));
				}
			}

			if (this->in_transaction && !tx_control)
			{
				using namespace std;

				if (strncmp(query, "SELECT", 6) != 0)
					this->transaction_log.emplace_back(std::string(query));
			}

			num_fields = mysql_field_count(this->impl->mysql_handle);

			if ((mresult = mysql_store_result(this->impl->mysql_handle)) == 0)
			{
				if (num_fields == 0)
				{
					result.affected_rows = static_cast<int>(mysql_affected_rows(this->impl->mysql_handle));
					return result;
				}
				else
				{
					throw Database_QueryFailed(mysql_error(this->impl->mysql_handle));
				}
			}

			fields = mysql_fetch_fields(mresult);

			for (int i = 0; i < num_fields; ++i)
			{
				if (!fields[i].name)
				{
					throw Database_QueryFailed("libMySQL critical failure!");
				}
			}

			result.resize(static_cast<size_t>(mysql_num_rows(mresult)));
			int i = 0;
			for (MYSQL_ROW row = mysql_fetch_row(mresult); row != 0; row = mysql_fetch_row(mresult))
			{
				std::unordered_map<std::string, util::variant> resrow;
				for (int ii = 0; ii < num_fields; ++ii)
				{
					util::variant rescell;
					if (IS_NUM(fields[ii].type))
					{
						if (row[ii])
						{
							rescell = util::to_int(row[ii]);
						}
						else
						{
							rescell = 0;
						}
					}
					else
					{
						if (row[ii])
						{
							rescell = row[ii];
						}
						else
						{
							rescell = "";
						}
					}
					resrow[fields[ii].name] = rescell;
				}
				result[i++] = resrow;
			}

			mysql_free_result(mresult);
		}
		break;
#endif // DATABASE_MYSQL

#ifdef DATABASE_SQLITE
		case SQLite:
			if (sqlite3_exec(this->impl->sqlite_handle, query, sqlite_callback, (void *)this, 0) != SQLITE_OK)
			{
				throw Database_QueryFailed(sqlite3_errmsg(this->impl->sqlite_handle));
			}
			result = this->callbackdata;
			this->callbackdata.clear();
			break;
#endif // DATABASE_SQLITE

#ifdef DATABASE_SQLSERVER
		case SqlServer:
		{
			SQLRETURN ret = SQLExecDirect(this->impl->hstmt, (SQLCHAR*)query, SQL_NTS);

			if (ret == SQL_SUCCESS)
			{
				SQLSMALLINT numCols = 0;
				ret = SQLNumResultCols(this->impl->hstmt, &numCols);
				if (ret == SQL_SUCCESS)
				{
					if (numCols > 0)
					{
						// select data - get column names
						std::vector<std::string> fields;
						fields.reserve(numCols);

						for (int colNdx = 1; colNdx <= numCols; ++colNdx)
						{
							char titleBuf[50] = { 0 };
							SQLColAttribute(this->impl->hstmt, colNdx, SQL_DESC_NAME, titleBuf, sizeof(titleBuf), NULL, NULL);
							fields.push_back(std::string(titleBuf));
						}

						// get data values
						while (SQLFetch(this->impl->hstmt) == SQL_SUCCESS && ret == SQL_SUCCESS)
						{
							std::unordered_map<std::string, util::variant> resrow;

							for (SQLUSMALLINT colNdx = 1; colNdx <= numCols && ret == SQL_SUCCESS; ++colNdx)
							{
								SQLLEN colType;
								ret = SQLColAttribute(this->impl->hstmt, colNdx, SQL_DESC_CONCISE_TYPE, NULL, 0, NULL, &colType);
								if (ret == SQL_SUCCESS)
								{
									if (colType == SQL_CHAR || colType == SQL_VARCHAR || colType == SQL_LONGVARCHAR)
									{
										// handle string data
										SQLCHAR resultStr[2048] = { 0 };
										ret = SQLGetData(this->impl->hstmt, colNdx, SQL_C_CHAR, resultStr, 2048, NULL);
										if (ret == SQL_SUCCESS)
											resrow[fields[colNdx]] = util::variant(resultStr);
									}
									else
									{
										// handle numeric data
										SQLINTEGER resultInt;
										ret = SQLGetData(this->impl->hstmt, colNdx, static_cast<SQLSMALLINT>(colType), &resultInt, 0, NULL);
										if (ret == SQL_SUCCESS)
											resrow[fields[colNdx]] = util::variant(resultInt);
									}
								}
							}

							if (ret == SQL_SUCCESS)
								result.push_back(resrow);
						}
					}
					else
					{
						// insert/update/delete data
						SQLLEN rowCount = 0;
						ret = SQLRowCount(this->impl->hstmt, &rowCount);
						if (ret == SQL_SUCCESS && rowCount >= 0)
						{
							result.affected_rows = static_cast<int>(rowCount);
						}
					}
				}
			}

			if (ret == SQL_ERROR || ret == SQL_SUCCESS_WITH_INFO)
			{
				HandleSqlServerError(SQL_HANDLE_STMT, this->impl->hstmt, ret);
			}

			SQLFreeStmt(this->impl->hstmt, SQL_CLOSE);
			break;
		}
#endif // DATABASE_SQLSERVER

		default:
			throw Database_QueryFailed("Unknown database engine");
	}

	return result;
}

Database_Result Database::Query(const char *format, ...)
{
	if (!this->connected)
	{
		throw Database_QueryFailed("Not connected to database.");
	}

	std::va_list ap;
	va_start(ap, format);

	std::string finalquery;
	int tempi;
	char *tempc;
	char *escret;

#ifdef DATABASE_MYSQL
	unsigned long esclen;
#endif

	for (const char *p = format; *p != '\0'; ++p)
	{
		if (*p == '#')
		{
			tempi = va_arg(ap,int);
			finalquery += util::to_string(tempi);
		}
		else if (*p == '@')
		{
			tempc = va_arg(ap,char *);
			finalquery += static_cast<std::string>(tempc);
		}
		else if (*p == '$')
		{
			tempc = va_arg(ap,char *);
			switch (this->engine)
			{
#ifdef DATABASE_MYSQL
				case MySQL:
					tempi = strlen(tempc);
					escret = new char[tempi*2+1];
					esclen = mysql_real_escape_string(this->impl->mysql_handle, escret, tempc, tempi);
					finalquery += std::string(escret, esclen);
					delete[] escret;
					break;
#endif // DATABASE_MYSQL

#ifdef DATABASE_SQLITE
				case SQLite:
					escret = sqlite3_mprintf("%q",tempc);
					finalquery += escret;
					sqlite3_free(escret);
					break;
#endif // DATABASE_SQLITE

#ifdef DATABASE_SQLSERVER
		case SqlServer:
			//todo: query SQL server
			break;
#endif // DATABASE_SQLSERVER
			}
		}
		else
		{
			finalquery += *p;
		}
	}

	va_end(ap);

	return this->RawQuery(finalquery.c_str());
}

std::string Database::Escape(const std::string& raw)
{
	char *escret;
	std::string result;

#ifdef DATABASE_MYSQL
	unsigned long esclen;
#endif

	switch (this->engine)
	{
#ifdef DATABASE_MYSQL
		case MySQL:
			escret = new char[raw.length()*2+1];
			esclen = mysql_real_escape_string(this->impl->mysql_handle, escret, raw.c_str(), raw.length());
			result.assign(escret, esclen);
			delete[] escret;
			break;
#endif // DATABASE_MYSQL

#ifdef DATABASE_SQLITE
		case SQLite:
			escret = sqlite3_mprintf("%q", raw.c_str());
			result = escret;
			sqlite3_free(escret);
			break;
#endif // DATABASE_SQLITE

#ifdef DATABASE_SQLSERVER
		case SqlServer:
			//todo: escape query string
			break;
#endif // DATABASE_SQLSERVER
	}

	for (std::string::iterator it = result.begin(); it != result.end(); ++it)
	{
		if (*it == '@' || *it == '#' || *it == '$')
		{
			*it = '?';
		}
	}

	return result;
}

void Database::ExecuteFile(const std::string& filename)
{
	std::list<std::string> queries;
	std::string query;

	FILE* fh;
	auto error = fopen_s(&fh, filename.c_str(), "rt");
	if (error || !fh)
		throw std::exception("Unable to open file");

	try
	{
		while (true)
		{
			char buf[4096];

			const char *result = std::fgets(buf, 4096, fh);

			if (!result || std::feof(fh))
				break;

			for (char* p = buf; *p != '\0'; ++p)
			{
				if (*p == ';')
				{
					queries.push_back(query);
					query.erase();
				}
				else
				{
					query += *p;
				}
			}
		}
	}
	catch (std::exception)
	{
		if (fh)
			std::fclose(fh);
		throw;
	}

	if (fh)
		std::fclose(fh);

	queries.push_back(query);
	query.erase();

	std::remove_if(UTIL_RANGE(queries), [&](const std::string& s) { return util::trim(s).length() == 0; });

	this->ExecuteQueries(UTIL_RANGE(queries));
}

bool Database::Pending() const
{
	return this->in_transaction;
}

bool Database::BeginTransaction()
{
	if (this->in_transaction)
		return false;

	for (int attempt = 1; ; ++attempt)
	{
		try
		{
			if (attempt > 1)
			{
				Console::Wrn("Start transaction failed. Trying again... (Attempt %i / 10)", attempt);
				util::sleep(1.0 * attempt);
			}

			switch (this->engine)
			{
#ifdef DATABASE_MYSQL
				case MySQL:
					this->RawQuery("START TRANSACTION", true);
					break;
#endif // DATABASE_MYSQL

#ifdef DATABASE_SQLITE
				case SQLite:
					this->RawQuery("BEGIN", true);
					break;
#endif // DATABASE_SQLITE

#ifdef DATABASE_SQLSERVER
				case SqlServer:
					this->RawQuery("BEGIN TRANSACTION", true);
					break;
#endif // DATABASE_SQLSERVER
			}

			break;
		}
		catch (...)
		{
			if (attempt >= 10)
			{
				Console::Err("Failed to begin transaction. Halting server.");
				std::terminate();
			}
		}
	}

	this->in_transaction = true;

	return true;
}

void Database::Commit()
{
	if (!this->in_transaction)
		throw Database_Exception("No transaction to commit");

	if (this->engine != SqlServer)
	{
		this->RawQuery("COMMIT", true);
	}
	else
	{
		this->RawQuery("COMMIT TRANSACTION", true);
	}

	this->in_transaction = false;
	this->transaction_log.clear();
}

void Database::Rollback()
{
	if (!this->in_transaction)
		throw Database_Exception("No transaction to rollback");

	if (this->engine != SqlServer)
	{
		this->RawQuery("ROLLBACK", true);
	}
	else
	{
		this->RawQuery("ROLLBACK TRANSACTION", true);
	}

	this->in_transaction = false;
	this->transaction_log.clear();
}

Database::~Database()
{
	this->Close();
}
