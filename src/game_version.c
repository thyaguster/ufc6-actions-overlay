#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <stdio.h>
#include <string.h>
#include "game_version.h"

int game_version_parse_json(const char *json,size_t length,GameVersion *out){
    if(!out)return 0;
    *out=(GameVersion){0};
    if(!json||length<24)return 0;
    const char key[]="\"contentVersion\"";
    for(size_t i=0;i+sizeof(key)-1<length;++i){
        if(memcmp(json+i,key,sizeof(key)-1))continue;
        size_t p=i+sizeof(key)-1;
        while(p<length&&(json[p]==' '||json[p]=='\t'||json[p]=='\r'||json[p]=='\n'))++p;
        if(p>=length||json[p++]!=':')return 0;
        while(p<length&&(json[p]==' '||json[p]=='\t'||json[p]=='\r'||json[p]=='\n'))++p;
        if(p>=length||json[p++]!='\"')return 0;
        unsigned major=0,minor=0,patch=0;
        if(p+10>=length||json[p+2]!='.'||json[p+6]!='.'||json[p+10]!='\"')return 0;
        for(int j=0;j<2;++j){if(json[p+j]<'0'||json[p+j]>'9')return 0;major=major*10+(unsigned)(json[p+j]-'0');}
        for(int j=3;j<6;++j){if(json[p+j]<'0'||json[p+j]>'9')return 0;minor=minor*10+(unsigned)(json[p+j]-'0');}
        for(int j=7;j<10;++j){if(json[p+j]<'0'||json[p+j]>'9')return 0;patch=patch*10+(unsigned)(json[p+j]-'0');}
        out->known=1;out->older_than_01006=major<1||(major==1&&minor<6);
        swprintf(out->label,20,L"%02u.%03u.%03u",major,minor,patch);
        return 1;
    }
    return 0;
}

GameVersion game_version_detect(void){
    GameVersion result={0};int argc=0;LPWSTR *argv=CommandLineToArgvW(GetCommandLineW(),&argc);
    if(!argv)return result;
    wchar_t path[2048]=L"",root[1900]=L"";
    for(int i=1;i+1<argc;++i)if(!lstrcmpiW(argv[i],L"--game")){
        DWORD n=GetFullPathNameW(argv[i+1],1900,root,NULL);
        if(n>0&&n<1900)
            swprintf(path,2048,L"%ls\\sce_sys\\param.json",root);
        break;
    }
    LocalFree(argv);
    if(!path[0])return result;
    HANDLE file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
    if(file==INVALID_HANDLE_VALUE)return result;
    LARGE_INTEGER size={0};
    if(GetFileSizeEx(file,&size)&&size.QuadPart>0&&size.QuadPart<=131072){
        char *data=HeapAlloc(GetProcessHeap(),0,(SIZE_T)size.QuadPart);
        if(data){DWORD read=0;if(ReadFile(file,data,(DWORD)size.QuadPart,&read,NULL)&&read==(DWORD)size.QuadPart)game_version_parse_json(data,read,&result);HeapFree(GetProcessHeap(),0,data);}
    }
    CloseHandle(file);return result;
}
