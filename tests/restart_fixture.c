#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <wchar.h>

int wmain(int argc,wchar_t **argv) {
    if(argc==3&&!wcscmp(argv[1],L"--wait-event")){
        HANDLE event=OpenEventW(SYNCHRONIZE,FALSE,argv[2]);if(!event)return 3;
        DWORD result=WaitForSingleObject(event,15000);CloseHandle(event);return result==WAIT_OBJECT_0?0:4;
    }
    if(argc==5&&!wcscmp(argv[1],L"--play")&&!wcscmp(argv[2],L"--headless")&&!wcscmp(argv[3],L"--ui-lang")&&!wcscmp(argv[4],L"en")){
        wchar_t path[1100];GetModuleFileNameW(NULL,path,1100);wchar_t *slash=wcsrchr(path,L'\\');if(!slash)return 2;
        wcscpy(slash+1,L"launcher-started.txt");
        HANDLE file=CreateFileW(path,GENERIC_WRITE,0,NULL,CREATE_NEW,0,NULL);if(file==INVALID_HANDLE_VALUE)return 2;
        const char text[]="--play --headless --ui-lang en";DWORD n;WriteFile(file,text,sizeof(text)-1,&n,NULL);CloseHandle(file);return 0;
    }
    return 2;
}
