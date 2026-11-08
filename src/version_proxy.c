/* Forward the real Windows version APIs. No filesystem search for the system DLL. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winver.h>

static INIT_ONCE once = INIT_ONCE_STATIC_INIT;
static HMODULE real_version;
static BOOL CALLBACK load_version(PINIT_ONCE o, PVOID p, PVOID *c) {
    wchar_t path[MAX_PATH];
    UINT n = GetSystemDirectoryW(path, MAX_PATH);
    (void)o; (void)p; (void)c;
    if (!n || n + 13 >= MAX_PATH) return FALSE;
    lstrcatW(path, L"\\version.dll");
    real_version = LoadLibraryW(path);
    return real_version != NULL;
}
static FARPROC proc(const char *name) {
    if (!InitOnceExecuteOnce(&once, load_version, NULL, NULL)) return NULL;
    return GetProcAddress(real_version, name);
}
#define FORWARD(ret, name, args, call) \
ret WINAPI Proxy##name args { \
    typedef ret (WINAPI *Fn) args; Fn f = (Fn)proc(#name); \
    if (!f) { SetLastError(ERROR_PROC_NOT_FOUND); return 0; } return f call; \
}
FORWARD(BOOL, GetFileVersionInfoA, (LPCSTR a,DWORD b,DWORD c,LPVOID d), (a,b,c,d))
FORWARD(BOOL, GetFileVersionInfoW, (LPCWSTR a,DWORD b,DWORD c,LPVOID d), (a,b,c,d))
FORWARD(DWORD, GetFileVersionInfoSizeA, (LPCSTR a,LPDWORD b), (a,b))
FORWARD(DWORD, GetFileVersionInfoSizeW, (LPCWSTR a,LPDWORD b), (a,b))
FORWARD(BOOL, VerQueryValueA, (LPCVOID a,LPCSTR b,LPVOID *c,PUINT d), (a,b,c,d))
FORWARD(BOOL, VerQueryValueW, (LPCVOID a,LPCWSTR b,LPVOID *c,PUINT d), (a,b,c,d))
FORWARD(BOOL, GetFileVersionInfoExA, (DWORD a,LPCSTR b,DWORD c,DWORD d,LPVOID e), (a,b,c,d,e))
FORWARD(BOOL, GetFileVersionInfoExW, (DWORD a,LPCWSTR b,DWORD c,DWORD d,LPVOID e), (a,b,c,d,e))
FORWARD(DWORD, GetFileVersionInfoSizeExA, (DWORD a,LPCSTR b,LPDWORD c), (a,b,c))
FORWARD(DWORD, GetFileVersionInfoSizeExW, (DWORD a,LPCWSTR b,LPDWORD c), (a,b,c))
FORWARD(DWORD, VerLanguageNameA, (DWORD a,LPSTR b,DWORD c), (a,b,c))
FORWARD(DWORD, VerLanguageNameW, (DWORD a,LPWSTR b,DWORD c), (a,b,c))
