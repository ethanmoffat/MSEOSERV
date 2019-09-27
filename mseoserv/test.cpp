#include <windows.h>
#include <sql.h>
#include <sqlext.h>
#include <iostream>

#pragma comment(lib, "odbc32.lib")

void HandleDiagnosticRecord (SQLHANDLE      hHandle,
                             SQLSMALLINT    hType,
                             RETCODE        RetCode);

void Execute(SQLHDBC hConn, const char * query);

int main()
{
#define STR_LEN 1024
    char sqlretconnstr[STR_LEN];

    SQLHENV hEnv;
    SQLRETURN val = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &hEnv);
    std::cout << "Handle alloc: " << val << std::endl;

    val = SQLSetEnvAttr(hEnv, SQL_ATTR_ODBC_VERSION, (SQLPOINTER)SQL_OV_ODBC3, 0);
    std::cout << "Version register: " << val << std::endl;

    SQLHDBC hConn;
    val = SQLAllocHandle(SQL_HANDLE_DBC, hEnv, &hConn);
    std::cout << "Connection alloc: " << val << std::endl;

    val = SQLDriverConnect(hConn,
        NULL,
        (SQLCHAR*)"DRIVER={SQL Server};SERVER=***,1433;DATABASE=master;UID=sa;PWD=***",
        SQL_NTS,
        (SQLCHAR*)sqlretconnstr,
        STR_LEN,
        NULL,
        SQL_DRIVER_NOPROMPT);
    std::cout << "Connection string: " << sqlretconnstr << std::endl;
    std::cout << "Connection: " << val << std::endl;
    HandleDiagnosticRecord(hConn, SQL_HANDLE_DBC, val);

    Execute(hConn, "SELECT @@SERVERNAME");
    Execute(hConn, "SELECT @@VERSION");

    val = SQLDisconnect(hConn);
    std::cout << "Disconnect: " << val << std::endl;
    val = SQLFreeHandle(SQL_HANDLE_DBC, hConn);
    std::cout << "Free conn: " << val << std::endl;
    val = SQLFreeHandle(SQL_HANDLE_ENV, hEnv);
    std::cout << "Free handle: " << val << std::endl;

    return 0;
}

void HandleDiagnosticRecord (SQLHANDLE      hHandle,
                             SQLSMALLINT    hType,
                             RETCODE        RetCode)
{
    SQLSMALLINT iRec = 0;
    SQLINTEGER  iError;
    SQLCHAR       wszMessage[1000];
    SQLCHAR       wszState[SQL_SQLSTATE_SIZE+1];

    if (RetCode == SQL_INVALID_HANDLE)
    {
        fwprintf(stderr, L"Invalid handle!\n");
        return;
    }

    while (SQLGetDiagRec(hType,
                         hHandle,
                         ++iRec,
                         wszState,
                         &iError,
                         wszMessage,
                         (SQLSMALLINT)(sizeof(wszMessage) / sizeof(SQLCHAR)),
                         (SQLSMALLINT *)NULL) == SQL_SUCCESS)
    {
        // Hide data truncated..
        if (strncmp((const char *)wszState, "01004", 5))
        {
            fprintf(stderr, "[%5.5s] %s (%d)\n", wszState, wszMessage, iError);
        }
    }
}

void Execute(SQLHDBC hConn, const char * query)
{
    SQLHSTMT hStatement;
    SQLRETURN val = SQLAllocHandle(SQL_HANDLE_STMT, hConn, &hStatement);
    std::cout << "Statement: " << val << std::endl;

    val = SQLExecDirect(hStatement, (SQLCHAR*)query, SQL_NTS);
    std::cout << "select statements: " << val << std::endl;
    HandleDiagnosticRecord(hStatement, SQL_HANDLE_STMT, val);

    if (val == SQL_SUCCESS)
    {
        SQLCHAR result[STR_LEN];
        SQLINTEGER sqlVersion;

        while (SQLFetch(hStatement) == SQL_SUCCESS)
        {
            val = SQLGetData(hStatement, 1, SQL_CHAR, result, STR_LEN, &sqlVersion);
            std::cout << "Get data: " << val << std::endl;
            std::cout << result << std::endl;
        }
    }

    val = SQLFreeHandle(SQL_HANDLE_STMT, hStatement);
    std::cout << "Free Statement: " << val << std::endl;
}