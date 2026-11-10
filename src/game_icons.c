#define WIN32_LEAN_AND_MEAN
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#include <windows.h>
#include <shellapi.h>
#include <tlhelp32.h>
#include <stdio.h>
#include <wchar.h>
#include "game_icons.h"
#include "launch_config.h"
#include "icons_process_env.h"
static wchar_t bin[1024],root[1024],helper[1100],reason[512];
static int state=GAME_ICONS_UNAVAILABLE;
static volatile LONG busy;
static SRWLOCK guard=SRWLOCK_INIT;
static void update(int value,const wchar_t *text){AcquireSRWLockExclusive(&guard);state=value;lstrcpynW(reason,text,512);ReleaseSRWLockExclusive(&guard);}
void game_icons_init(HMODULE module){
    GetModuleFileNameW(module,bin,1024);wchar_t *slash=wcsrchr(bin,L'\\');if(!slash)return;*slash=0;
    swprintf(helper,1100,L"%ls\\ufc6_menu_icons.exe",bin);
    int argc=0;wchar_t **argv=CommandLineToArgvW(GetCommandLineW(),&argc);
    if(argv){for(int i=1;i+1<argc;++i)if(!wcscmp(argv[i],L"--game")){GetFullPathNameW(argv[i+1],1024,root,NULL);break;}LocalFree(argv);}
    update(GAME_ICONS_UNAVAILABLE,root[0]?L"Open this page to check installed game icons.":L"The engine did not provide an extracted game folder.");
}
static DWORD WINAPI check_worker(void *unused){
    (void)unused;wchar_t temp[MAX_PATH],report[MAX_PATH],command[2600];
    if(!GetTempPathW(MAX_PATH,temp)||!GetTempFileNameW(temp,L"u6i",0,report)){update(GAME_ICONS_UNAVAILABLE,L"Cannot create the icon status report.");InterlockedExchange(&busy,0);return 0;}
    swprintf(command,2600,L"\"%ls\" --status --game-dir \"%ls\" --report \"%ls\"",helper,root,report);
    STARTUPINFOW si={.cb=sizeof(si)};PROCESS_INFORMATION pi={0};
    wchar_t *environment=icons_process_environment();
    if(environment&&CreateProcessW(helper,command,NULL,NULL,FALSE,CREATE_NO_WINDOW|CREATE_UNICODE_ENVIRONMENT,environment,bin,&si,&pi)){
        CloseHandle(pi.hThread);DWORD waited=WaitForSingleObject(pi.hProcess,120000);CloseHandle(pi.hProcess);
        wchar_t message[512];GetPrivateProfileStringW(L"Icons",L"Message",L"Could not verify game icons.",message,512,report);
        int result=(int)GetPrivateProfileIntW(L"Icons",L"State",GAME_ICONS_UNAVAILABLE,report);
        update(waited==WAIT_OBJECT_0&&result>=1&&result<=4?result:GAME_ICONS_UNAVAILABLE,message);
    }else update(GAME_ICONS_UNAVAILABLE,L"The game-icons helper is missing or could not start.");
    if(environment)HeapFree(GetProcessHeap(),0,environment);
    DeleteFileW(report);InterlockedExchange(&busy,0);return 0;
}
void game_icons_check(void){
    if(!root[0]||GetFileAttributesW(helper)==INVALID_FILE_ATTRIBUTES){update(GAME_ICONS_UNAVAILABLE,L"Game folder or bundled icons helper unavailable.");return;}
    if(InterlockedCompareExchange(&busy,1,0))return;
    update(GAME_ICONS_CHECKING,L"Checking game icons and the restore backup...");
    HANDLE thread=CreateThread(NULL,0,check_worker,NULL,0,NULL);
    if(thread)CloseHandle(thread);else{InterlockedExchange(&busy,0);update(GAME_ICONS_UNAVAILABLE,L"Cannot start the icons check.");}
}
int game_icons_status(wchar_t *message,size_t count){AcquireSRWLockShared(&guard);int value=state;if(message&&count)lstrcpynW(message,reason,(int)count);ReleaseSRWLockShared(&guard);return value;}
static DWORD verified_parent(const wchar_t *launcher){
    DWORD parent=0;HANDLE snap=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);if(snap==INVALID_HANDLE_VALUE)return 0;
    PROCESSENTRY32W e={.dwSize=sizeof(e)};if(Process32FirstW(snap,&e))do{if(e.th32ProcessID==GetCurrentProcessId()){parent=e.th32ParentProcessID;break;}}while(Process32NextW(snap,&e));CloseHandle(snap);
    HANDLE p=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,parent);if(!p)return 0;
    wchar_t name[1024];DWORD n=1024;BOOL ok=QueryFullProcessImageNameW(p,0,name,&n)&&!lstrcmpiW(name,launcher);CloseHandle(p);return ok?parent:0;
}
static BOOL put(const wchar_t *file,const wchar_t *key,const wchar_t *value){return WritePrivateProfileStringW(L"Job",key,value,file);}
BOOL game_icons_apply(HWND game,int xbox){
    if(!root[0]||GetFileAttributesW(helper)==INVALID_FILE_ATTRIBUTES||InterlockedCompareExchange(&busy,0,0))return FALSE;
    LaunchConfig launch;if(!launch_config_load(bin,&launch)||!launch_config_valid(&launch))return FALSE;
    wchar_t job[1200],event[128],number[32],exe[1024],command[2400],report[1200];
    swprintf(event,128,L"Local\\UFC6GameIcons_%lu_%llu",GetCurrentProcessId(),(unsigned long long)GetTickCount64());
    swprintf(job,1200,L"%ls\\ufc6-game-icons-job-%lu-%llu.ini",bin,GetCurrentProcessId(),(unsigned long long)GetTickCount64());
    swprintf(report,1200,L"%ls\\ufc6-game-icons-result.ini",bin);
    HANDLE ready=CreateEventW(NULL,TRUE,FALSE,event);if(!ready)return FALSE;
    HANDLE f=CreateFileW(job,GENERIC_WRITE,0,NULL,CREATE_NEW,0,NULL);if(f==INVALID_HANDLE_VALUE){CloseHandle(ready);return FALSE;}
    WORD bom=0xfeff;DWORD written;BOOL ok=WriteFile(f,&bom,2,&written,NULL)&&written==2;CloseHandle(f);
    GetModuleFileNameW(NULL,exe,1024);swprintf(number,32,L"%lu",GetCurrentProcessId());
    ok=ok&&put(job,L"PID",number)&&put(job,L"GameExe",exe)&&put(job,L"LaunchExe",launch.executable)&&put(job,L"LaunchArgs",launch.arguments)&&put(job,L"WorkDir",launch.directory)&&put(job,L"GameRoot",root)&&put(job,L"Theme",xbox?L"xbox":L"playstation")&&put(job,L"Report",report)&&put(job,L"Event",event);
    swprintf(number,32,L"%lu",launch.wait_parent?verified_parent(launch.executable):0);ok=ok&&put(job,L"Parent",number);WritePrivateProfileStringW(NULL,NULL,NULL,job);
    STARTUPINFOW si={.cb=sizeof(si)};PROCESS_INFORMATION pi={0};BOOL success=FALSE;
    swprintf(command,2400,L"\"%ls\" --apply-job \"%ls\"",helper,job);
    wchar_t *environment=icons_process_environment();
    if(ok&&environment&&CreateProcessW(helper,command,NULL,NULL,FALSE,CREATE_NO_WINDOW|CREATE_UNICODE_ENVIRONMENT,environment,bin,&si,&pi)){
        CloseHandle(pi.hThread);HANDLE waits[2]={ready,pi.hProcess};
        if(WaitForMultipleObjects(2,waits,FALSE,15000)==WAIT_OBJECT_0)success=PostMessageW(game,WM_CLOSE,0,0);
        CloseHandle(pi.hProcess);
    }
    if(environment)HeapFree(GetProcessHeap(),0,environment);
    CloseHandle(ready);if(!success)DeleteFileW(job);return success;
}
