#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <wchar.h>
int wmain(int argc,wchar_t **argv){
    if(argc==3&&!wcscmp(argv[1],L"--wait-event")){
        HANDLE e=OpenEventW(SYNCHRONIZE,FALSE,argv[2]);if(!e)return 3;
        DWORD r=WaitForSingleObject(e,15000);CloseHandle(e);return r==WAIT_OBJECT_0?0:4;
    }
    if(argc!=6||wcscmp(argv[1],L"--probe"))return 2;
    HANDLE event=OpenEventW(SYNCHRONIZE,FALSE,argv[2]);if(!event)return 3;
    DWORD waited=WaitForSingleObject(event,15000);CloseHandle(event);if(waited!=WAIT_OBJECT_0)return 4;
    wchar_t value[2048];const wchar_t *private_keys[]={L"_PYI_APPLICATION_HOME_DIR",L"_PYI_ARCHIVE_FILE",L"_PYI_PARENT_PROCESS_LEVEL"};
    for(unsigned i=0;i<3;++i)if(GetEnvironmentVariableW(private_keys[i],value,2048))return 5;
    if(!GetEnvironmentVariableW(L"PYINSTALLER_RESET_ENVIRONMENT",value,2048)||wcscmp(value,L"1"))return 6;
    if(GetDllDirectoryW(2048,value))return 7;
    wchar_t command[7000];swprintf(command,7000,L"\"%ls\" --status --game-dir \"%ls\" --report \"%ls\"",argv[3],argv[4],argv[5]);
    STARTUPINFOW si={.cb=sizeof(si)};PROCESS_INFORMATION pi={0};if(!CreateProcessW(argv[3],command,NULL,NULL,FALSE,CREATE_NO_WINDOW,NULL,NULL,&si,&pi))return 8;
    CloseHandle(pi.hThread);waited=WaitForSingleObject(pi.hProcess,12000);DWORD code=99;GetExitCodeProcess(pi.hProcess,&code);CloseHandle(pi.hProcess);if(waited!=WAIT_OBJECT_0||code)return 9;
    wchar_t marker[2048];swprintf(marker,2048,L"%ls.relay-ok",argv[5]);HANDLE file=CreateFileW(marker,GENERIC_WRITE,0,NULL,CREATE_NEW,0,NULL);if(file==INVALID_HANDLE_VALUE)return 10;CloseHandle(file);return 0;
}
