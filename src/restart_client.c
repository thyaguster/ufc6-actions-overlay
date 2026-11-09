#define WIN32_LEAN_AND_MEAN
#define UNICODE
#define _UNICODE
#include <windows.h>
#include <tlhelp32.h>
#include <stdio.h>
#include <wchar.h>
#include "restart_client.h"
#include "launch_config.h"

static DWORD launcher_parent(const wchar_t *launcher) {
    DWORD parent=0;HANDLE snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);
    if(snapshot==INVALID_HANDLE_VALUE)return 0;
    PROCESSENTRY32W entry={.dwSize=sizeof(entry)};
    if(Process32FirstW(snapshot,&entry))do{
        if(entry.th32ProcessID==GetCurrentProcessId()){parent=entry.th32ParentProcessID;break;}
    }while(Process32NextW(snapshot,&entry));
    CloseHandle(snapshot);
    if(!parent)return 0;
    HANDLE process=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,parent);
    if(!process)return 0;
    wchar_t path[1024];DWORD count=1024;
    BOOL matches=QueryFullProcessImageNameW(process,0,path,&count)&&!lstrcmpiW(path,launcher);
    CloseHandle(process);return matches?parent:0;
}
static BOOL handoff(HWND game,HMODULE module,int power_action) {
    wchar_t directory[1024],helper[1100],event_name[120],command[2600];
    LaunchConfig config;
    if(!GetModuleFileNameW(module,directory,1024))return FALSE;
    wchar_t *slash=wcsrchr(directory,L'\\');if(!slash)return FALSE;*slash=0;
    swprintf(helper,1100,L"%ls\\ufc6_menu_restart.exe",directory);
    BOOL needs_launch=power_action==0||power_action==2;
    BOOL valid_config=launch_config_load(directory,&config);
    if(GetFileAttributesW(helper)==INVALID_FILE_ATTRIBUTES||(needs_launch&&(!valid_config||!launch_config_valid(&config))))return FALSE;
    swprintf(event_name,120,L"Local\\UFC6MenuRestart_%lu_%llu",GetCurrentProcessId(),(unsigned long long)GetTickCount64());
    HANDLE ready=CreateEventW(NULL,TRUE,FALSE,event_name);if(!ready)return FALSE;
    swprintf(command,2600,L"\"%ls\" %lu \"%ls\" %lu \"%ls\"%ls",helper,GetCurrentProcessId(),config.executable,valid_config&&config.wait_parent?launcher_parent(config.executable):0,event_name,power_action==3?L" --reboot-only":power_action==2?L" --reboot-pc":power_action==1?L" --shutdown-pc":L"");
    STARTUPINFOW si={.cb=sizeof(si)};PROCESS_INFORMATION pi={0};
    BOOL started=CreateProcessW(helper,command,NULL,NULL,FALSE,CREATE_NO_WINDOW,NULL,directory,&si,&pi);
    BOOL success=FALSE;
    if(started){
        CloseHandle(pi.hThread);
        HANDLE waits[2]={ready,pi.hProcess};
        if(WaitForMultipleObjects(2,waits,FALSE,5000)==WAIT_OBJECT_0)
            success=PostMessageW(game,WM_CLOSE,0,0);
        CloseHandle(pi.hProcess);
    }
    CloseHandle(ready);return success;
}
BOOL menu_restart_game(HWND game,HMODULE module){return handoff(game,module,FALSE);}
BOOL menu_shutdown_pc(HWND game,HMODULE module){return handoff(game,module,TRUE);}

BOOL menu_reboot_pc(HWND game,HMODULE module,BOOL auto_launch){return handoff(game,module,auto_launch?2:3);}
