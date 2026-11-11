#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <wchar.h>
#include "launch_config.h"
static BOOL absolute(const wchar_t *path){return (path[0]&&path[1]==L':'&&(path[2]==L'\\'||path[2]==L'/'))||(path[0]==L'\\'&&path[1]==L'\\'&&path[2]);}
void launch_config_default(const wchar_t *bin,LaunchConfig *c){
    ZeroMemory(c,sizeof(*c));lstrcpynW(c->directory,bin,LAUNCH_PATH);
    wchar_t *slash=wcsrchr(c->directory,L'\\');if(slash)*slash=0;
    swprintf(c->executable,LAUNCH_PATH,L"%ls\\UFC6.exe",c->directory);
    wcscpy(c->arguments,L"--play --headless --ui-lang en");c->wait_parent=1;
}
static BOOL read_value(const wchar_t *ini,const wchar_t *key,const wchar_t *fallback,wchar_t *out,DWORD count){return GetPrivateProfileStringW(L"Launch",key,fallback,out,count,ini)<count-1;}
BOOL launch_config_read(const wchar_t *ini,const wchar_t *bin,LaunchConfig *c){
    launch_config_default(bin,c);wchar_t mode[32];
    if(!read_value(ini,L"Mode",L"PORTHEON",mode,32))return FALSE;
    if(!lstrcmpiW(mode,L"PORTHEON"))return TRUE;
    if(lstrcmpiW(mode,L"Custom"))return FALSE;
    c->custom=1;wchar_t arguments[LAUNCH_ARGS+5];
    if(!read_value(ini,L"Executable",L"",c->executable,LAUNCH_PATH)||!read_value(ini,L"Arguments",L"raw:",arguments,LAUNCH_ARGS+5)||!read_value(ini,L"WorkingDirectory",L"",c->directory,LAUNCH_PATH))return FALSE;
    /* Prefix prevents INI APIs from removing quotes around a single argument. */
    const wchar_t *raw=!wcsncmp(arguments,L"raw:",4)?arguments+4:arguments;
    if(wcslen(raw)>=LAUNCH_ARGS)return FALSE;wcscpy(c->arguments,raw);
    if(!c->directory[0]){lstrcpynW(c->directory,c->executable,LAUNCH_PATH);wchar_t *slash=wcsrchr(c->directory,L'\\');if(slash)*slash=0;}
    c->wait_parent=GetPrivateProfileIntW(L"Launch",L"WaitForLauncherExit",0,ini)!=0;
    return TRUE;
}
BOOL launch_config_load(const wchar_t *bin,LaunchConfig *c){wchar_t ini[LAUNCH_PATH+40];swprintf(ini,LAUNCH_PATH+40,L"%ls\\ufc6-quit-menu.ini",bin);return launch_config_read(ini,bin,c);}
BOOL launch_config_valid(const LaunchConfig *c){
    if(!absolute(c->executable)||!absolute(c->directory)||wcspbrk(c->arguments,L"\r\n"))return FALSE;
    const wchar_t *ext=wcsrchr(c->executable,L'.');DWORD file=GetFileAttributesW(c->executable),folder=GetFileAttributesW(c->directory),type;
    return ext&&!lstrcmpiW(ext,L".exe")&&file!=INVALID_FILE_ATTRIBUTES&&!(file&FILE_ATTRIBUTE_DIRECTORY)&&folder!=INVALID_FILE_ATTRIBUTES&&(folder&FILE_ATTRIBUTE_DIRECTORY)&&GetBinaryTypeW(c->executable,&type);
}
BOOL launch_config_command(const LaunchConfig *c,wchar_t *command,size_t count){
    size_t needed=wcslen(c->executable)+wcslen(c->arguments)+4;
    if(needed>count||wcschr(c->executable,L'"'))return FALSE;
    swprintf(command,count,L"\"%ls\"%ls%ls",c->executable,c->arguments[0]?L" ":L"",c->arguments);return TRUE;
}
/* Preserve unrelated settings and Unicode paths/arguments in a UTF-16 INI.
 * Build a separate file, then replace atomically only after all writes succeed. */
static BOOL copy_ini(const wchar_t *ini,const wchar_t *temp){
    WritePrivateProfileStringW(NULL,NULL,NULL,ini);
    HANDLE source=CreateFileW(ini,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,0,NULL);
    DWORD size=0,read=0;BYTE *bytes=NULL;wchar_t *wide=NULL;BOOL ok=FALSE;
    if(source!=INVALID_HANDLE_VALUE){
        size=GetFileSize(source,NULL);if(size==INVALID_FILE_SIZE||size>1024*1024){CloseHandle(source);return FALSE;}
        bytes=HeapAlloc(GetProcessHeap(),0,size+2);if(!bytes){CloseHandle(source);return FALSE;}
        ok=ReadFile(source,bytes,size,&read,NULL)&&read==size;CloseHandle(source);if(!ok)goto done;ok=FALSE;
    }else if(GetLastError()!=ERROR_FILE_NOT_FOUND)return FALSE;
    if(size>=2&&bytes[0]==0xff&&bytes[1]==0xfe){
        if(size%2)goto done;
        wide=(wchar_t*)(bytes+2);size-=2;
    }else {
        if(size>=2&&bytes[0]==0xfe&&bytes[1]==0xff)goto done;
        DWORD offset=size>=3&&bytes[0]==0xef&&bytes[1]==0xbb&&bytes[2]==0xbf?3:0;
        int count=size?MultiByteToWideChar(offset?CP_UTF8:CP_ACP,offset?MB_ERR_INVALID_CHARS:0,(const char*)bytes+offset,(int)(size-offset),NULL,0):0;
        if(size&&!count)goto done;
        wide=HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,(count+1)*sizeof(wchar_t));if(!wide)goto done;
        if(count&&!MultiByteToWideChar(offset?CP_UTF8:CP_ACP,offset?MB_ERR_INVALID_CHARS:0,(const char*)bytes+offset,(int)(size-offset),wide,count))goto done;
        size=(DWORD)(count*sizeof(wchar_t));
    }
    HANDLE target=CreateFileW(temp,GENERIC_WRITE,0,NULL,CREATE_NEW,0,NULL);
    if(target==INVALID_HANDLE_VALUE)goto done;
    WORD bom=0xfeff;DWORD written;
    ok=WriteFile(target,&bom,2,&written,NULL)&&written==2&&WriteFile(target,wide,size,&written,NULL)&&written==size;
    CloseHandle(target);
 done:
    if(wide&&(!bytes||wide!=(wchar_t*)(bytes+2)))HeapFree(GetProcessHeap(),0,wide);
    if(bytes)HeapFree(GetProcessHeap(),0,bytes);
    return ok;
}
BOOL launch_config_save(const wchar_t *ini,const LaunchConfig *c){
    if(!launch_config_valid(c))return FALSE;
    wchar_t temp[LAUNCH_PATH+80],arguments[LAUNCH_ARGS+5];swprintf(temp,LAUNCH_PATH+80,L"%ls.launch-%lu-%llu.tmp",ini,GetCurrentProcessId(),(unsigned long long)GetTickCount64());
    if(!copy_ini(ini,temp)){DeleteFileW(temp);return FALSE;}
    swprintf(arguments,LAUNCH_ARGS+5,L"raw:%ls",c->arguments);
    BOOL ok=WritePrivateProfileStringW(L"Launch",L"Mode",c->custom?L"Custom":L"PORTHEON",temp)&&WritePrivateProfileStringW(L"Launch",L"Executable",c->executable,temp)&&WritePrivateProfileStringW(L"Launch",L"Arguments",arguments,temp)&&WritePrivateProfileStringW(L"Launch",L"WorkingDirectory",c->directory,temp)&&WritePrivateProfileStringW(L"Launch",L"WaitForLauncherExit",c->wait_parent?L"1":L"0",temp);
    WritePrivateProfileStringW(NULL,NULL,NULL,temp);
    if(ok)ok=MoveFileExW(temp,ini,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH);
    if(!ok)DeleteFileW(temp);
    WritePrivateProfileStringW(NULL,NULL,NULL,ini);return ok;
}

BOOL overlay_settings_reset(const wchar_t *ini){
    wchar_t temp[LAUNCH_PATH+80];swprintf(temp,LAUNCH_PATH+80,L"%ls.reset-%lu-%llu.tmp",ini,GetCurrentProcessId(),(unsigned long long)GetTickCount64());
    if(!copy_ini(ini,temp)){DeleteFileW(temp);return FALSE;}
    const wchar_t *keys[]={L"ButtonIcons",L"OpeningShortcut",L"UIScale",L"AutoLaunchAfterRestart",L"GuideButtonOpensMenu",L"ControllerAccess",L"LongevityProtection",L"TrainingRewardMultiplier",L"ThemeChoice",L"FontSize",L"BoxStyle",L"WindowSize",L"StartupShortMovie",L"StartupSkipPopups",L"SkipStartupSplashes"};
    const wchar_t *values[]={L"Xbox",L"0x00000c08",L"100",L"1",L"1",L"Either",L"0",L"1",L"0",L"18",L"0",L"0",L"0",L"0",L"0"};
    _Static_assert(sizeof(keys)==sizeof(values),"Reset keys and values must match");BOOL ok=TRUE;
    for(unsigned i=0;i<sizeof(keys)/sizeof(keys[0]);++i)ok=WritePrivateProfileStringW(L"Menu",keys[i],values[i],temp)&&ok;
    ok=WritePrivateProfileStringW(L"Keybinds",NULL,NULL,temp)&&ok;
    ok=WritePrivateProfileStringW(L"Launch",NULL,NULL,temp)&&ok;WritePrivateProfileStringW(NULL,NULL,NULL,temp);
    if(ok)ok=MoveFileExW(temp,ini,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH);
    if(!ok)DeleteFileW(temp);WritePrivateProfileStringW(NULL,NULL,NULL,ini);return ok;
}
