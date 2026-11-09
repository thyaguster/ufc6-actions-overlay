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
    if(argc>=2&&!wcscmp(argv[1],L"--custom-test")){
        wchar_t path[1100],cwd[1100],message[8192];GetModuleFileNameW(NULL,path,1100);wchar_t *slash=wcsrchr(path,L'\\');if(!slash)return 2;wcscpy(slash+1,L"launcher-started.txt");
        GetCurrentDirectoryW(1100,cwd);swprintf(message,8192,L"cwd=%ls\nargc=%d\n",cwd,argc);
        for(int i=1;i<argc;++i){wchar_t arg[2048];swprintf(arg,2048,L"arg%d=%ls\n",i,argv[i]);wcscat(message,arg);}
        char utf8[32768];int bytes=WideCharToMultiByte(CP_UTF8,0,message,-1,utf8,sizeof(utf8),NULL,NULL);if(!bytes)return 2;
        HANDLE file=CreateFileW(path,GENERIC_WRITE,0,NULL,CREATE_NEW,0,NULL);if(file==INVALID_HANDLE_VALUE)return 2;DWORD written;
        BOOL ok=WriteFile(file,utf8,(DWORD)bytes-1,&written,NULL);CloseHandle(file);return ok?0:2;
    }
    return 2;
}
