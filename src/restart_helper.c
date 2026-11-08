/* Short-lived restart handoff. Never terminate the game or its launcher forcibly. */
#define WIN32_LEAN_AND_MEAN
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#include <windows.h>
#include <shellapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <reason.h>

static void log_line(const char *line) {
    wchar_t path[1100];GetModuleFileNameW(NULL,path,1100);wchar_t *s=wcsrchr(path,L'\\');
    if(!s)return;wcscpy(s+1,L"ufc6-quit-menu-restart.log");
    HANDLE h=CreateFileW(path,FILE_APPEND_DATA,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_ALWAYS,0,NULL);
    if(h!=INVALID_HANDLE_VALUE){DWORD n;WriteFile(h,line,(DWORD)strlen(line),&n,NULL);WriteFile(h,"\r\n",2,&n,NULL);CloseHandle(h);}
}
static HANDLE open_matching(DWORD pid,const wchar_t *expected) {
    HANDLE h=OpenProcess(SYNCHRONIZE|PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid);
    if(!h)return NULL;
    wchar_t path[1100];DWORD length=1100;
    if(!QueryFullProcessImageNameW(h,0,path,&length)||lstrcmpiW(path,expected)){CloseHandle(h);return NULL;}
    return h;
}
static BOOL prepare_shutdown(void) {
#ifdef UFC6_POWER_TEST
    return TRUE;
#else
    HANDLE token;
    if(!OpenProcessToken(GetCurrentProcess(),TOKEN_ADJUST_PRIVILEGES|TOKEN_QUERY,&token))return FALSE;
    TOKEN_PRIVILEGES privileges={.PrivilegeCount=1};
    BOOL ready=LookupPrivilegeValueW(NULL,SE_SHUTDOWN_NAME,&privileges.Privileges[0].Luid);
    privileges.Privileges[0].Attributes=SE_PRIVILEGE_ENABLED;
    if(ready){SetLastError(ERROR_SUCCESS);ready=AdjustTokenPrivileges(token,FALSE,&privileges,0,NULL,NULL)&&GetLastError()==ERROR_SUCCESS;}
    CloseHandle(token);return ready;
#endif
}
static BOOL request_shutdown(UINT flags,DWORD reason) {
#ifdef UFC6_POWER_TEST
    if(GetEnvironmentVariableW(L"UFC6_TEST_POWER_FAIL",NULL,0))return FALSE;
    wchar_t path[1100];GetModuleFileNameW(NULL,path,1100);wchar_t *slash=wcsrchr(path,L'\\');if(!slash)return FALSE;
    wcscpy(slash+1,L"shutdown-requested.test.txt");
    HANDLE file=CreateFileW(path,GENERIC_WRITE,0,NULL,CREATE_NEW,0,NULL);if(file==INVALID_HANDLE_VALUE)return FALSE;
    char message[100];int size=snprintf(message,sizeof(message),"flags=0x%08x reason=0x%08lx",flags,(unsigned long)reason);DWORD written;
    BOOL result=WriteFile(file,message,(DWORD)size,&written,NULL);CloseHandle(file);return result;
#else
    return ExitWindowsEx(flags,reason);
#endif
}
/* Only this user's next sign-in is affected. No password or auto-login changes. */
static const wchar_t *resume_value=L"UFC6MenuResume";
static unsigned long long boot_time(void) {
    FILETIME now;GetSystemTimeAsFileTime(&now);
    ULARGE_INTEGER value={.LowPart=now.dwLowDateTime,.HighPart=now.dwHighDateTime};
    return value.QuadPart-GetTickCount64()*10000ULL;
}
static BOOL resume_registration(const wchar_t *command,BOOL remove) {
    if(!remove&&(!command||wcslen(command)>260))return FALSE;
#ifdef UFC6_POWER_TEST
    (void)resume_value;
    wchar_t path[1200];GetModuleFileNameW(NULL,path,1200);wchar_t *slash=wcsrchr(path,L'\\');if(!slash)return FALSE;
    wcscpy(slash+1,L"resume-command.test.txt");
    if(remove)return DeleteFileW(path)||GetLastError()==ERROR_FILE_NOT_FOUND;
    HANDLE file=CreateFileW(path,GENERIC_WRITE,0,NULL,CREATE_NEW,0,NULL);if(file==INVALID_HANDLE_VALUE)return FALSE;
    DWORD written,size=(DWORD)((wcslen(command)+1)*sizeof(wchar_t));
    BOOL ok=WriteFile(file,command,size,&written,NULL)&&written==size;CloseHandle(file);return ok;
#else
    HKEY key;LONG result=RegCreateKeyExW(HKEY_CURRENT_USER,L"Software\\Microsoft\\Windows\\CurrentVersion\\RunOnce",0,NULL,0,KEY_QUERY_VALUE|KEY_SET_VALUE,NULL,&key,NULL);
    if(result!=ERROR_SUCCESS)return FALSE;
    if(remove)result=RegDeleteValueW(key,resume_value);
    else {
        result=RegQueryValueExW(key,resume_value,NULL,NULL,NULL,NULL);
        if(result==ERROR_FILE_NOT_FOUND)result=RegSetValueExW(key,resume_value,0,REG_SZ,(const BYTE*)command,(DWORD)((wcslen(command)+1)*sizeof(wchar_t)));
        else result=ERROR_ALREADY_EXISTS;
    }
    RegCloseKey(key);return result==ERROR_SUCCESS||(remove&&result==ERROR_FILE_NOT_FOUND);
#endif
}
static int launch_game(const wchar_t *launcher,wchar_t *directory) {
    wchar_t command[1400];swprintf(command,1400,L"\"%ls\" --play --headless --ui-lang en",launcher);
    STARTUPINFOW si={.cb=sizeof(si)};PROCESS_INFORMATION pi={0};
    if(!CreateProcessW(launcher,command,NULL,NULL,FALSE,CREATE_NO_WINDOW,NULL,directory,&si,&pi)){
        log_line("Could not relaunch UFC6.exe.");return 5;
    }
    CloseHandle(pi.hThread);CloseHandle(pi.hProcess);return 0;
}
static LRESULT CALLBACK power_window(HWND window,UINT message,WPARAM wparam,LPARAM lparam) {
    if(message==WM_QUERYENDSESSION)return TRUE;
    if(message==WM_ENDSESSION){
        if(!wparam){resume_registration(NULL,TRUE);log_line("Windows reboot was cancelled; one-time resume removed.");}
        PostQuitMessage(0);return 0;
    }
    return DefWindowProcW(window,message,wparam,lparam);
}
static HWND watch_reboot(void) {
    HINSTANCE instance=GetModuleHandleW(NULL);
    WNDCLASSW type={.lpfnWndProc=power_window,.hInstance=instance,.lpszClassName=L"UFC6MenuPowerHandoff"};
    if(!RegisterClassW(&type))return NULL;
    return CreateWindowExW(0,type.lpszClassName,L"UFC 6 restart handoff",WS_POPUP,0,0,0,0,NULL,NULL,instance,NULL);
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE previous,LPWSTR arguments,int show) {
    (void)instance;(void)previous;(void)arguments;(void)show;
    int argc;LPWSTR *argv=CommandLineToArgvW(GetCommandLineW(),&argc);
    if(!argv)return 2;
#ifdef UFC6_POWER_TEST
    if(argc==2&&!wcscmp(argv[1],L"--test-cancel-watcher")){
        if(!resume_registration(L"test resume command",FALSE))return 8;
        HWND watcher=watch_reboot();if(!watcher)return 8;
        if(!SendMessageW(watcher,WM_QUERYENDSESSION,0,0))return 8;
        SendMessageW(watcher,WM_ENDSESSION,FALSE,0);DestroyWindow(watcher);LocalFree(argv);return 0;
    }
#endif
    wchar_t directory[1100],expected_game[1200],expected_launcher[1200],command[1500];
    GetModuleFileNameW(NULL,directory,1100);wchar_t *slash=wcsrchr(directory,L'\\');if(!slash)return 2;*slash=0;
    swprintf(expected_game,1200,L"%ls\\UFC 6.exe",directory);
    wcscpy(expected_launcher,directory);slash=wcsrchr(expected_launcher,L'\\');if(!slash)return 2;
    wcscpy(slash+1,L"UFC6.exe");
    if(argc==3&&!wcscmp(argv[1],L"--resume-game")){
        wchar_t *end;unsigned long long previous=wcstoull(argv[2],&end,10),current=boot_time();
        if(!previous||*end)return 2;
        unsigned long long difference=current>previous?current-previous:previous-current;
        if(difference<20000000ULL){log_line("Same Windows boot: one-time game resume skipped.");LocalFree(argv);return 0;}
        slash=wcsrchr(directory,L'\\');if(!slash)return 2;*slash=0;
#ifndef UFC6_POWER_TEST
        Sleep(5000); /* Give Explorer and controller services time to start. */
#endif
        int result=launch_game(expected_launcher,directory);LocalFree(argv);
        log_line(result?"Post-reboot launch failed.":"One-time post-reboot game launch completed.");return result;
    }
    if(argc!=5&&argc!=6)return 2;
    BOOL resume_game=argc==6&&!wcscmp(argv[5],L"--reboot-pc");
    BOOL reboot_pc=resume_game||(argc==6&&!wcscmp(argv[5],L"--reboot-only"));
    BOOL shutdown_pc=argc==6&&!wcscmp(argv[5],L"--shutdown-pc");
    if(argc==6&&!reboot_pc&&!shutdown_pc)return 2;
    DWORD pid=wcstoul(argv[1],NULL,10),parent=wcstoul(argv[3],NULL,10);
    if(lstrcmpiW(argv[2],expected_launcher)){log_line("Rejected mismatched launcher path.");return 2;}
    HANDLE game=open_matching(pid,expected_game);
    if(!game){log_line("Could not verify the game process; restart cancelled.");return 3;}
    HANDLE launcher=parent?open_matching(parent,expected_launcher):NULL;
    if((shutdown_pc||reboot_pc)&&!prepare_shutdown()){log_line("Windows shutdown privilege is unavailable; game left running.");CloseHandle(game);if(launcher)CloseHandle(launcher);return 6;}
    HANDLE ready=OpenEventW(EVENT_MODIFY_STATE,FALSE,argv[4]);
    if(!ready){CloseHandle(game);if(launcher)CloseHandle(launcher);return 3;}
    if(resume_game){
        wchar_t self[1200];GetModuleFileNameW(NULL,self,1200);
        swprintf(command,1500,L"\"%ls\" --resume-game %llu",self,boot_time());
        if(GetFileAttributesW(expected_launcher)==INVALID_FILE_ATTRIBUTES||!resume_registration(command,FALSE)){
            log_line("Could not register one-time game resume; reboot cancelled and game left running.");
            CloseHandle(ready);CloseHandle(game);if(launcher)CloseHandle(launcher);return 8;
        }
    }
    SetEvent(ready);CloseHandle(ready);
    DWORD result=WaitForSingleObject(game,60000);CloseHandle(game);
    if(result!=WAIT_OBJECT_0){log_line("Game did not close normally; no relaunch attempted.");if(resume_game)resume_registration(NULL,TRUE);if(launcher)CloseHandle(launcher);return 4;}
    if(launcher){result=WaitForSingleObject(launcher,30000);CloseHandle(launcher);
        if(result!=WAIT_OBJECT_0){log_line("Previous launcher is still cleaning up; no duplicate launch attempted.");if(resume_game)resume_registration(NULL,TRUE);return 4;}}
    if(shutdown_pc||reboot_pc){
#ifndef UFC6_POWER_TEST
        HWND watcher=resume_game?watch_reboot():NULL;
        if(resume_game&&!watcher){resume_registration(NULL,TRUE);log_line("Reboot cancellation watcher unavailable; reboot cancelled.");return 8;}
#endif
        BOOL requested=request_shutdown(reboot_pc?EWX_REBOOT:EWX_POWEROFF,SHTDN_REASON_MAJOR_APPLICATION|SHTDN_REASON_MINOR_OTHER|SHTDN_REASON_FLAG_PLANNED);
        if(resume_game&&!requested)resume_registration(NULL,TRUE);
        log_line(requested?(reboot_pc?(resume_game?"Normal Windows reboot requested; one-time game resume registered.":"Normal Windows reboot requested; game auto-launch is off."):"Normal Windows power-off requested; no force flags used."):"Windows did not accept the power request.");
        LocalFree(argv);
#ifndef UFC6_POWER_TEST
        if(requested&&watcher){MSG message;while(GetMessageW(&message,NULL,0,0)>0){TranslateMessage(&message);DispatchMessageW(&message);}}
        if(watcher)DestroyWindow(watcher);
#endif
        return requested?0:7;
    }
    /* The old launcher held its single-instance mutex until exit. */
    slash=wcsrchr(directory,L'\\');if(!slash)return 2;*slash=0;
    int launched=launch_game(expected_launcher,directory);if(launched)return launched;LocalFree(argv);
    log_line("Game and launcher exited normally; UFC6.exe relaunched.");return 0;
}
