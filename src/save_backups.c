#define WIN32_LEAN_AND_MEAN
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#include <windows.h>
#include <shlobj.h>
#include <shellapi.h>
#include <stdio.h>
#include <wchar.h>
#include "save_backups.h"
#include "icons_process_env.h"
static const GUID documents_id={0xfdd39ad0,0x238f,0x46af,{0xad,0xb4,0x6c,0x85,0x48,0x03,0x69,0xc7}};
static wchar_t source[1200],destination[1200],helper[1200],bin[1024],message[512];
static SRWLOCK guard=SRWLOCK_INIT;
static volatile LONG busy;
static int state=BACKUP_ERROR;
static void update(int value,const wchar_t *text){AcquireSRWLockExclusive(&guard);state=value;lstrcpynW(message,text,512);ReleaseSRWLockExclusive(&guard);}
void save_backups_init(HMODULE module){
    DWORD n=GetModuleFileNameW(module,bin,1024);if(!n||n>=1024)return;
    wchar_t *slash=wcsrchr(bin,L'\\');if(!slash)return;*slash=0;
    swprintf(helper,1200,L"%ls\\ufc6_menu_icons.exe",bin);
    wchar_t cwd[1024];n=GetCurrentDirectoryW(1024,cwd);if(!n||n>=1024)return;
    swprintf(source,1200,L"%ls\\_SaveData",cwd);
    PWSTR documents=NULL;
    if(FAILED(SHGetKnownFolderPath(&documents_id,0,NULL,&documents)))return;
    if(wcslen(documents)+40>=1200){CoTaskMemFree(documents);return;}
    swprintf(destination,1200,L"%ls\\UFC6 Actions Overlay\\SaveBackups",documents);CoTaskMemFree(documents);
    update(BACKUP_READY,L"Ready to back up all UFC 6 save files.");
}
int save_backups_state(wchar_t *text,size_t count){AcquireSRWLockShared(&guard);int value=state;if(text&&count)lstrcpynW(text,message,(int)count);ReleaseSRWLockShared(&guard);return value;}
static DWORD WINAPI worker(void *unused){
    (void)unused;wchar_t temp[MAX_PATH],report[MAX_PATH],command[5000];
    if(!GetTempPathW(MAX_PATH,temp)||!GetTempFileNameW(temp,L"u6b",0,report)){update(BACKUP_ERROR,L"Cannot create the backup status report.");InterlockedExchange(&busy,0);return 0;}
    swprintf(command,5000,L"\"%ls\" --backup-saves --save-root \"%ls\" --backup-dir \"%ls\" --report \"%ls\"",helper,source,destination,report);
    STARTUPINFOW si={.cb=sizeof(si)};PROCESS_INFORMATION pi={0};wchar_t *environment=icons_process_environment();
    if(environment&&CreateProcessW(helper,command,NULL,NULL,FALSE,CREATE_NO_WINDOW|CREATE_UNICODE_ENVIRONMENT,environment,bin,&si,&pi)){
        CloseHandle(pi.hThread);DWORD waited=WaitForSingleObject(pi.hProcess,INFINITE),exit_code=1;GetExitCodeProcess(pi.hProcess,&exit_code);CloseHandle(pi.hProcess);
        wchar_t text[512];GetPrivateProfileStringW(L"Backup",L"Message",L"The backup helper did not return a result.",text,512,report);
        int result=(int)GetPrivateProfileIntW(L"Backup",L"State",BACKUP_ERROR,report);
        update(waited==WAIT_OBJECT_0&&exit_code==0&&result==BACKUP_SAVED?BACKUP_SAVED:BACKUP_ERROR,text);
    }else update(BACKUP_ERROR,L"The bundled backup helper is missing or could not start.");
    if(environment)HeapFree(GetProcessHeap(),0,environment);
    DeleteFileW(report);InterlockedExchange(&busy,0);return 0;
}
void save_backups_create(void){
    if(!source[0]||!destination[0]||GetFileAttributesW(helper)==INVALID_FILE_ATTRIBUTES){update(BACKUP_ERROR,L"Save location or bundled backup helper unavailable.");return;}
    if(InterlockedCompareExchange(&busy,1,0))return;
    update(BACKUP_BUSY,L"Copying and verifying saved files...");HANDLE thread=CreateThread(NULL,0,worker,NULL,0,NULL);
    if(thread)CloseHandle(thread);else{InterlockedExchange(&busy,0);update(BACKUP_ERROR,L"Cannot start the save backup.");}
}
BOOL save_backups_open(void){
    if(!destination[0]||GetFileAttributesW(destination)==INVALID_FILE_ATTRIBUTES)return FALSE;
    SHELLEXECUTEINFOW launch={.cbSize=sizeof(launch),.lpVerb=L"open",.lpFile=destination,.nShow=SW_SHOWNORMAL};
    return ShellExecuteExW(&launch);
}
