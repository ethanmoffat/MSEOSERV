#include <windows.h>
#include <sql.h>
#include <sqlext.h>
#include <iostream>

#pragma comment(lib, "odbc32.lib")

void HandleDiagnosticRecord (SQLHANDLE      hHandle,
                             SQLSMALLINT    hType,
                             RETCODE        RetCode);

int main()
{
    SQLHENV hEnv;
    SQLRETURN val = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &hEnv);
    std::cout << "Handle alloc: " << val << std::endl;

    val = SQLSetEnvAttr(hEnv, SQL_ATTR_ODBC_VERSION, (SQLPOINTER)SQL_OV_ODBC3, 0);
    std::cout << "Version register: " << val << std::endl;

    SQLHDBC hConn;
    val = SQLAllocHandle(SQL_HANDLE_DBC, hEnv, &hConn);
    std::cout << "Connection alloc: " << val << std::endl;

    val = SQLSetConnectAttr(hConn, SQL_LOGIN_TIMEOUT, (SQLPOINTER)5, 0);
    std::cout << "Connection attribute: " << val << std::endl;

    val = SQLConnect(hConn,
        (SQLCHAR*)"10.91.136.181,1433",
        (SQLSMALLINT)17,
        (SQLCHAR*)"sa",
        (SQLSMALLINT)2,
        (SQLCHAR*)"SecurityEngineer1",
        (SQLSMALLINT)17);
    std::cout << "Connection: " << val << std::endl;
    HandleDiagnosticRecord(hConn, SQL_HANDLE_DBC, val);

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