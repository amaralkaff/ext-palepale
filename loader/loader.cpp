#include <Windows.h>
#include <winhttp.h>
#include <tlhelp32.h>
#include <cstdio>
#include <cstring>

#pragma comment(lib, "winhttp.lib")

#define BASE_URL L"palepale-server-402005787427.asia-southeast1.run.app"
#define BASE_PATH L"/download/"
#define API_KEY L"palepale_2026_key"

static const char KEY[] = "pAlEpAlE_2026_sEcReT_kEy_X9Z3!";
static const size_t KEY_LEN = sizeof(KEY) - 1; // 31 bytes

struct FileEntry {
    const char* encName;   // encrypted filename on server
    const wchar_t* encNameW; // wide version for URL
    const char* decName;   // decrypted filename on disk
};

static const FileEntry FILES[] = {
    { "cormem_mapper.enc", L"cormem_mapper.enc", "cormem_mapper.exe" },
    { "kdumper.enc",       L"kdumper.enc",       "kdumper.sys"       },
    { "Cormem.enc",        L"Cormem.enc",        "Cormem.sys"        },
    { "client.enc",        L"client.enc",        "client.exe"        },
};
static const int FILE_COUNT = sizeof(FILES) / sizeof(FILES[0]);

// ---------------------------------------------------------------------------
// Utility
// ---------------------------------------------------------------------------

static bool IsAdmin()
{
    BOOL admin = FALSE;
    PSID group = NULL;
    SID_IDENTIFIER_AUTHORITY auth = SECURITY_NT_AUTHORITY;
    if (AllocateAndInitializeSid(&auth, 2,
            SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS,
            0, 0, 0, 0, 0, 0, &group))
    {
        CheckTokenMembership(NULL, group, &admin);
        FreeSid(group);
    }
    return admin != FALSE;
}

static bool DownloadWithKey(const wchar_t* host, const wchar_t* path,
                            const wchar_t* apiKey, const char* outPath)
{
    bool success = false;

    HINTERNET hSession = WinHttpOpen(L"palepale/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return false;

    HINTERNET hConnect = WinHttpConnect(hSession, host,
        INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        return false;
    }

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", path,
        NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
        WINHTTP_FLAG_SECURE);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    // Add API key header
    wchar_t header[256];
    wsprintfW(header, L"X-API-Key: %s", apiKey);
    WinHttpAddRequestHeaders(hRequest, header, (DWORD)-1, WINHTTP_ADDREQ_FLAG_ADD);

    if (WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                           WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
        WinHttpReceiveResponse(hRequest, NULL))
    {
        // Check status code
        DWORD statusCode = 0;
        DWORD statusSize = sizeof(statusCode);
        WinHttpQueryHeaders(hRequest,
            WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusSize,
            WINHTTP_NO_HEADER_INDEX);

        if (statusCode == 200) {
            FILE* fout = NULL;
            fopen_s(&fout, outPath, "wb");
            if (fout) {
                DWORD bytesAvail = 0;
                DWORD bytesRead = 0;
                unsigned char buf[8192];

                success = true;
                while (WinHttpQueryDataAvailable(hRequest, &bytesAvail) && bytesAvail > 0) {
                    DWORD toRead = (bytesAvail > sizeof(buf)) ? sizeof(buf) : bytesAvail;
                    if (WinHttpReadData(hRequest, buf, toRead, &bytesRead)) {
                        fwrite(buf, 1, bytesRead, fout);
                    } else {
                        success = false;
                        break;
                    }
                }
                fclose(fout);
                if (!success) DeleteFileA(outPath);
            }
        }
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return success;
}

static bool DecryptFile(const char* encPath, const char* outPath)
{
    FILE* fin = NULL;
    fopen_s(&fin, encPath, "rb");
    if (!fin) return false;

    fseek(fin, 0, SEEK_END);
    long size = ftell(fin);
    fseek(fin, 0, SEEK_SET);

    if (size <= 0) { fclose(fin); return false; }

    unsigned char* buf = (unsigned char*)malloc(size);
    if (!buf) { fclose(fin); return false; }

    fread(buf, 1, size, fin);
    fclose(fin);

    for (long i = 0; i < size; i++)
        buf[i] ^= (unsigned char)KEY[i % KEY_LEN];

    FILE* fout = NULL;
    fopen_s(&fout, outPath, "wb");
    if (!fout) { free(buf); return false; }

    fwrite(buf, 1, size, fout);
    fclose(fout);
    free(buf);
    return true;
}

static DWORD FindApex()
{
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return 0;

    PROCESSENTRY32W entry = {};
    entry.dwSize = sizeof(entry);
    DWORD pid = 0;

    if (Process32FirstW(snap, &entry)) {
        do {
            if (_wcsicmp(entry.szExeFile, L"r5apex_dx12.exe") == 0 ||
                _wcsicmp(entry.szExeFile, L"r5apex.exe") == 0)
            {
                pid = entry.th32ProcessID;
                break;
            }
        } while (Process32NextW(snap, &entry));
    }
    CloseHandle(snap);
    return pid;
}

static void RunAndWait(const char* cmdLine)
{
    STARTUPINFOA si = {};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi = {};
    char buf[2048];
    strcpy_s(buf, cmdLine);

    if (CreateProcessA(NULL, buf, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, 60000);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
}

static void LaunchProcess(const char* path)
{
    STARTUPINFOA si = {};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi = {};
    CreateProcessA(path, NULL, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
}

static void Cleanup(const char* tempDir)
{
    char path[MAX_PATH];
    for (int i = 0; i < FILE_COUNT; i++) {
        // Delete encrypted files
        snprintf(path, MAX_PATH, "%s%s", tempDir, FILES[i].encName);
        DeleteFileA(path);
        // Delete decrypted files
        snprintf(path, MAX_PATH, "%s%s", tempDir, FILES[i].decName);
        DeleteFileA(path);
    }
    RemoveDirectoryA(tempDir);
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

int main()
{
    SetConsoleTitleA("palepale");
    printf("\n  palepale v1.0\n\n");

    // Check admin
    if (!IsAdmin()) {
        printf("  [X] Run as Administrator.\n");
        getchar(); return 1;
    }
    printf("  [OK] Admin\n");

    // Check Windows version
    OSVERSIONINFOW osvi = {}; osvi.dwOSVersionInfoSize = sizeof(osvi);
    typedef LONG(WINAPI* RtlGetVersion)(OSVERSIONINFOW*);
    RtlGetVersion pRtlGetVersion = (RtlGetVersion)GetProcAddress(GetModuleHandleA("ntdll"), "RtlGetVersion");
    if (pRtlGetVersion) pRtlGetVersion(&osvi);
    if (osvi.dwMajorVersion < 10) {
        printf("  [X] Windows 10/11 required (found %lu.%lu)\n", osvi.dwMajorVersion, osvi.dwMinorVersion);
        getchar(); return 1;
    }
    printf("  [OK] Windows %lu (build %lu)\n", osvi.dwMajorVersion, osvi.dwBuildNumber);

    // Check 64-bit
    SYSTEM_INFO si = {};
    GetNativeSystemInfo(&si);
    if (si.wProcessorArchitecture != PROCESSOR_ARCHITECTURE_AMD64) {
        printf("  [X] 64-bit Windows required\n");
        getchar(); return 1;
    }
    printf("  [OK] 64-bit\n");

    // Check internet
    printf("  [..] Checking internet...");
    HINTERNET hTest = WinHttpOpen(L"test", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, NULL, NULL, 0);
    if (hTest) {
        HINTERNET hConn = WinHttpConnect(hTest, BASE_URL, INTERNET_DEFAULT_HTTPS_PORT, 0);
        HINTERNET hReq = hConn ? WinHttpOpenRequest(hConn, L"GET", L"/health", NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE) : NULL;
        bool online = false;
        if (hReq && WinHttpSendRequest(hReq, NULL, 0, NULL, 0, 0, 0) && WinHttpReceiveResponse(hReq, NULL)) {
            DWORD code = 0, sz = sizeof(code);
            WinHttpQueryHeaders(hReq, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, NULL, &code, &sz, NULL);
            online = (code == 200);
        }
        if (hReq) WinHttpCloseHandle(hReq);
        if (hConn) WinHttpCloseHandle(hConn);
        WinHttpCloseHandle(hTest);
        if (!online) {
            printf("\r  [X] No internet or server offline   \n");
            getchar(); return 1;
        }
    }
    printf("\r  [OK] Server online                  \n");

    // Check Apex installed (search common paths)
    bool apexFound = false;
    const char* apexPaths[] = {
        "D:\\Apex\\r5apex_dx12.exe",
        // Steam
        "C:\\Program Files (x86)\\Steam\\steamapps\\common\\Apex Legends\\r5apex_dx12.exe",
        "D:\\SteamLibrary\\steamapps\\common\\Apex Legends\\r5apex_dx12.exe",
        "E:\\SteamLibrary\\steamapps\\common\\Apex Legends\\r5apex_dx12.exe",
        "F:\\SteamLibrary\\steamapps\\common\\Apex Legends\\r5apex_dx12.exe",
        // EA App
        "C:\\Program Files\\EA Games\\Apex Legends\\r5apex_dx12.exe",
        "C:\\Program Files (x86)\\EA Games\\Apex Legends\\r5apex_dx12.exe",
        "D:\\EA Games\\Apex Legends\\r5apex_dx12.exe",
        "E:\\EA Games\\Apex Legends\\r5apex_dx12.exe",
        // Origin (legacy)
        "C:\\Program Files (x86)\\Origin Games\\Apex\\r5apex_dx12.exe",
        "D:\\Origin Games\\Apex\\r5apex_dx12.exe",
    };
    for (auto p : apexPaths) {
        if (GetFileAttributesA(p) != INVALID_FILE_ATTRIBUTES) { apexFound = true; break; }
    }
    if (FindApex()) apexFound = true; // already running
    printf("  [%s] Apex Legends %s\n", apexFound ? "OK" : "!!", apexFound ? "found" : "not found (may still work)");

    printf("\n");

    char tempDir[MAX_PATH];
    GetTempPathA(MAX_PATH, tempDir);
    strcat_s(tempDir, "pp\\");
    CreateDirectoryA(tempDir, NULL);

    // Download + Decrypt with progress
    int totalSteps = FILE_COUNT * 2; // download + decrypt for each
    int step = 0;

    for (int i = 0; i < FILE_COUNT; i++) {
        wchar_t urlPath[512];
        wsprintfW(urlPath, L"%s%s", BASE_PATH, FILES[i].encNameW);
        char outPath[MAX_PATH];
        snprintf(outPath, MAX_PATH, "%s%s", tempDir, FILES[i].encName);

        int pct = (step * 100) / totalSteps;
        printf("\r  [%3d%%] Downloading %s...     ", pct, FILES[i].encName);
        if (!DownloadWithKey(BASE_URL, urlPath, API_KEY, outPath)) {
            printf("\n  Download failed.\n");
            Cleanup(tempDir); getchar(); return 1;
        }
        step++;
    }

    for (int i = 0; i < FILE_COUNT; i++) {
        char enc[MAX_PATH], dec[MAX_PATH];
        snprintf(enc, MAX_PATH, "%s%s", tempDir, FILES[i].encName);
        snprintf(dec, MAX_PATH, "%s%s", tempDir, FILES[i].decName);

        int pct = (step * 100) / totalSteps;
        printf("\r  [%3d%%] Decrypting %s...      ", pct, FILES[i].decName);
        if (!DecryptFile(enc, dec)) {
            printf("\n  Decrypt failed.\n");
            Cleanup(tempDir); getchar(); return 1;
        }
        DeleteFileA(enc);
        step++;
    }
    printf("\r  [100%%] Ready                         \n");

    char mapperPath[MAX_PATH], driverPath[MAX_PATH], cormemPath[MAX_PATH], clientPath[MAX_PATH];
    snprintf(mapperPath, MAX_PATH, "%s%s", tempDir, FILES[0].decName);
    snprintf(driverPath, MAX_PATH, "%s%s", tempDir, FILES[1].decName);
    snprintf(cormemPath, MAX_PATH, "%s%s", tempDir, FILES[2].decName);
    snprintf(clientPath, MAX_PATH, "%s%s", tempDir, FILES[3].decName);

    // Load driver (hidden)
    printf("  Loading driver...\n");
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "\"%s\" \"%s\" \"%s\"", mapperPath, driverPath, cormemPath);
    RunAndWait(cmd);
    Sleep(2000);

    // Wait for Apex
    printf("  Waiting for Apex...");
    while (!FindApex()) { printf("."); Sleep(2000); }
    printf("\n");

    printf("  Waiting for lobby...");
    // Wait until game is fully loaded (lobby = LocalPlayer exists but HP=0)
    Sleep(5000); // initial delay for game to init
    while (true)
    {
        // Try opening driver to check if game is in lobby
        HANDLE dev = CreateFileA("\\\\.\\KMemDrv5", GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
        if (dev != INVALID_HANDLE_VALUE)
        {
            CloseHandle(dev);
            break; // driver responsive = game loaded enough
        }
        printf(".");
        Sleep(2000);
    }
    Sleep(3000); // extra settle time
    printf("\n");

    LaunchProcess(clientPath);
    printf("  Running. Press Enter to cleanup and exit.\n");
    getchar();

    Cleanup(tempDir);
    return 0;
}
