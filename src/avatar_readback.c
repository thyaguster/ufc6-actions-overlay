#define WIN32_LEAN_AND_MEAN
#include "avatar_readback.h"
#include <stdio.h>
#include <string.h>
#include <wchar.h>

static const char flag_text[]="readback-linear-images = true\r\n";
static void reason(wchar_t *out,size_t count,const wchar_t *message){if(out&&count)lstrcpynW(out,message,(int)count);}
static BOOL paths(const wchar_t *engine,wchar_t *root,wchar_t *ini,wchar_t *state,wchar_t files[2][MAX_PATH],int *available){
    if(!engine||wcslen(engine)>=MAX_PATH-64)return FALSE;
    wcscpy(root,engine);wchar_t *slash=wcsrchr(root,L'\\');if(!slash)return FALSE;
    if(lstrcmpiW(slash+1,L"bin")&&lstrcmpiW(slash+1,L"novo"))return FALSE;
    *slash=0;
    swprintf(ini,MAX_PATH,L"%ls\\UFC6.ini",root);
    swprintf(state,MAX_PATH,L"%ls\\ufc6-avatar-readback.state",root);
    wchar_t wrapper[MAX_PATH];swprintf(wrapper,MAX_PATH,L"%ls\\UFC6.exe",root);
    if(GetFileAttributesW(wrapper)==INVALID_FILE_ATTRIBUTES||GetFileAttributesW(ini)==INVALID_FILE_ATTRIBUTES)return FALSE;
    const wchar_t *names[]={L"bin",L"novo"};*available=0;
    for(int i=0;i<2;++i){wchar_t dir[MAX_PATH];swprintf(dir,MAX_PATH,L"%ls\\%ls",root,names[i]);
        files[i][0]=0;if(GetFileAttributesW(dir)==INVALID_FILE_ATTRIBUTES)continue;
        swprintf(files[i],MAX_PATH,L"%ls\\kyty_settings.ini",dir);++*available;
    }
    return *available>0;
}
static int stored_mode(const wchar_t *state){
    wchar_t marker[32];GetPrivateProfileStringW(L"AvatarReadback",L"Owner",L"",marker,32,state);
    if(lstrcmpW(marker,L"UFC6ActionsOverlay"))return -1;
    int mode=GetPrivateProfileIntW(L"AvatarReadback",L"Mode",-1,state);
    return mode>=AVATAR_READBACK_REPAIR&&mode<=AVATAR_READBACK_ALWAYS?mode:-1;
}
int avatar_readback_mode(const wchar_t *engine){
    wchar_t root[MAX_PATH],ini[MAX_PATH],state[MAX_PATH],files[2][MAX_PATH];int available;
    if(!paths(engine,root,ini,state,files,&available))return AVATAR_READBACK_OFF;
    return GetFileAttributesW(state)==INVALID_FILE_ATTRIBUTES?AVATAR_READBACK_OFF:stored_mode(state);
}
static BOOL owned_file(const wchar_t *path){
    HANDLE h=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,NULL);
    if(h==INVALID_HANDLE_VALUE)return FALSE;
    char bytes[sizeof(flag_text)]={0};DWORD n=0;BY_HANDLE_FILE_INFORMATION info;
    BOOL ok=GetFileInformationByHandle(h,&info)&&!(info.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT)&&
        info.nFileSizeHigh==0&&info.nFileSizeLow==sizeof(flag_text)-1&&
        ReadFile(h,bytes,sizeof(bytes),&n,NULL)&&n==sizeof(flag_text)-1&&
        !memcmp(bytes,flag_text,sizeof(flag_text)-1);
    CloseHandle(h);return ok;
}
static BOOL create_flag(const wchar_t *path){
    HANDLE h=CreateFileW(path,GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
    if(h==INVALID_HANDLE_VALUE)return FALSE;DWORD n=0;
    BOOL ok=WriteFile(h,flag_text,sizeof(flag_text)-1,&n,NULL)&&n==sizeof(flag_text)-1&&FlushFileBuffers(h);
    CloseHandle(h);if(!ok)DeleteFileW(path);return ok;
}
BOOL avatar_readback_set(const wchar_t *engine,int mode,wchar_t *error,size_t count){
    wchar_t root[MAX_PATH],ini[MAX_PATH],state[MAX_PATH],files[2][MAX_PATH];int available;
    reason(error,count,L"Could not locate the PORTHEON UFC 6 launcher.");
    if(mode<0||mode>AVATAR_READBACK_ALWAYS||!paths(engine,root,ini,state,files,&available))return FALSE;
    BOOL has_state=GetFileAttributesW(state)!=INVALID_FILE_ATTRIBUTES;
    int current=has_state?stored_mode(state):AVATAR_READBACK_OFF;
    if(current<0){reason(error,count,L"Unknown avatar settings file; no changes made.");return FALSE;}
    if(mode==current){reason(error,count,L"Avatar readback is already in this mode.");return TRUE;}
    if(current){
        for(int i=0;i<2;++i)if(files[i][0]&&!owned_file(files[i])){
            reason(error,count,L"An engine settings file changed; previous settings were kept.");return FALSE;}
        if(GetPrivateProfileIntW(L"UFC6",L"keep_env",-1,ini)!=1){
            reason(error,count,L"PORTHEON settings changed during repair; no changes made.");return FALSE;}
        if(mode){wchar_t value[8];swprintf(value,8,L"%d",mode);
            if(!WritePrivateProfileStringW(L"AvatarReadback",L"Mode",value,state)){reason(error,count,L"Could not update avatar mode.");return FALSE;}
            return TRUE;}
        int old=GetPrivateProfileIntW(L"AvatarReadback",L"OriginalKeepEnv",-2,state);
        if(old< -1||old>1){reason(error,count,L"Original launcher setting is unavailable.");return FALSE;}
        if(!WritePrivateProfileStringW(L"UFC6",L"keep_env",old<0?NULL:old?L"1":L"0",ini)){
            reason(error,count,L"Could not restore PORTHEON settings.");return FALSE;}
        int removed[2]={0,0};
        for(int i=0;i<2;++i)if(files[i][0]){
            if(!DeleteFileW(files[i])){
                for(int j=0;j<i;++j)if(removed[j])create_flag(files[j]);
                WritePrivateProfileStringW(L"UFC6",L"keep_env",L"1",ini);
                reason(error,count,L"Could not remove the temporary engine settings.");return FALSE;}
            removed[i]=1;
        }
        if(!DeleteFileW(state)){
            for(int i=0;i<2;++i)if(removed[i])create_flag(files[i]);
            WritePrivateProfileStringW(L"UFC6",L"keep_env",L"1",ini);
            reason(error,count,L"Could not clear the avatar repair marker.");return FALSE;}
        reason(error,count,L"Original launcher settings restored.");return TRUE;
    }
    if(!mode)return TRUE;
    for(int i=0;i<2;++i)if(files[i][0]&&GetFileAttributesW(files[i])!=INVALID_FILE_ATTRIBUTES){
        reason(error,count,L"An engine settings file already exists; no changes made.");return FALSE;}
    wchar_t old_value[32];GetPrivateProfileStringW(L"UFC6",L"keep_env",L"__missing__",old_value,32,ini);
    int old=!lstrcmpW(old_value,L"__missing__")?-1:!lstrcmpW(old_value,L"0")?0:!lstrcmpW(old_value,L"1")?1:-2;
    if(old==-2){reason(error,count,L"Unknown PORTHEON environment setting; no changes made.");return FALSE;}
    HANDLE marker=CreateFileW(state,GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
    if(marker==INVALID_HANDLE_VALUE){reason(error,count,L"Could not create avatar repair marker.");return FALSE;}
    CloseHandle(marker);wchar_t value[8];swprintf(value,8,L"%d",old);
    BOOL ok=WritePrivateProfileStringW(L"AvatarReadback",L"Owner",L"UFC6ActionsOverlay",state)&&
        WritePrivateProfileStringW(L"AvatarReadback",L"OriginalKeepEnv",value,state);
    swprintf(value,8,L"%d",mode);ok=WritePrivateProfileStringW(L"AvatarReadback",L"Mode",value,state)&&ok;
    int created[2]={0,0};
    for(int i=0;i<2&&ok;++i)if(files[i][0]){ok=create_flag(files[i]);created[i]=ok;}
    if(ok)ok=WritePrivateProfileStringW(L"UFC6",L"keep_env",L"1",ini);
    if(!ok){for(int i=0;i<2;++i)if(created[i])DeleteFileW(files[i]);DeleteFileW(state);
        reason(error,count,L"Could not enable image readback; changes were rolled back.");return FALSE;}
    reason(error,count,mode==AVATAR_READBACK_REPAIR?L"Portrait repair enabled for the next launch.":L"Image readback enabled for future launches.");
    return TRUE;
}
