#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#undef NDEBUG
#include <assert.h>
#include <stdio.h>
#include <wchar.h>
#include "../src/avatar_readback.h"
static void empty_file(const wchar_t *path){
    HANDLE h=CreateFileW(path,GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
    assert(h!=INVALID_HANDLE_VALUE);CloseHandle(h);
}
int main(void){
    wchar_t temp[MAX_PATH],root[MAX_PATH],bin[MAX_PATH],novo[MAX_PATH],ini[MAX_PATH],wrapper[MAX_PATH],bfile[MAX_PATH],nfile[MAX_PATH],state[MAX_PATH],error[160],value[80];
    assert(GetTempPathW(MAX_PATH,temp));swprintf(root,MAX_PATH,L"%lsavatar-readback-test-%lu",temp,GetCurrentProcessId());
    assert(CreateDirectoryW(root,NULL));
    swprintf(bin,MAX_PATH,L"%ls\\bin",root);swprintf(novo,MAX_PATH,L"%ls\\novo",root);
    assert(CreateDirectoryW(bin,NULL)&&CreateDirectoryW(novo,NULL));
    swprintf(ini,MAX_PATH,L"%ls\\UFC6.ini",root);swprintf(wrapper,MAX_PATH,L"%ls\\UFC6.exe",root);
    swprintf(bfile,MAX_PATH,L"%ls\\kyty_settings.ini",bin);swprintf(nfile,MAX_PATH,L"%ls\\kyty_settings.ini",novo);
    swprintf(state,MAX_PATH,L"%ls\\ufc6-avatar-readback.state",root);empty_file(wrapper);
    assert(WritePrivateProfileStringW(L"UFC6",L"dlss",L"7",ini));
    assert(avatar_readback_mode(novo)==AVATAR_READBACK_OFF);
    assert(avatar_readback_set(novo,AVATAR_READBACK_REPAIR,error,160));
    assert(avatar_readback_mode(bin)==AVATAR_READBACK_REPAIR);
    assert(GetFileAttributesW(bfile)!=INVALID_FILE_ATTRIBUTES&&GetFileAttributesW(nfile)!=INVALID_FILE_ATTRIBUTES);
    assert(GetPrivateProfileIntW(L"UFC6",L"keep_env",0,ini)==1);
    assert(WritePrivateProfileStringW(L"UFC6",L"keep_env",L"0",ini));
    assert(!avatar_readback_set(bin,AVATAR_READBACK_OFF,error,160));
    assert(GetFileAttributesW(bfile)!=INVALID_FILE_ATTRIBUTES&&GetFileAttributesW(nfile)!=INVALID_FILE_ATTRIBUTES);
    assert(WritePrivateProfileStringW(L"UFC6",L"keep_env",L"1",ini));
    assert(avatar_readback_set(bin,AVATAR_READBACK_ALWAYS,error,160));
    assert(avatar_readback_mode(novo)==AVATAR_READBACK_ALWAYS);
    assert(avatar_readback_set(novo,AVATAR_READBACK_OFF,error,160));
    assert(GetFileAttributesW(bfile)==INVALID_FILE_ATTRIBUTES&&GetFileAttributesW(nfile)==INVALID_FILE_ATTRIBUTES);
    assert(GetFileAttributesW(state)==INVALID_FILE_ATTRIBUTES);
    GetPrivateProfileStringW(L"UFC6",L"keep_env",L"missing",value,80,ini);assert(!wcscmp(value,L"missing"));
    GetPrivateProfileStringW(L"UFC6",L"dlss",L"",value,80,ini);assert(!wcscmp(value,L"7"));
    assert(WritePrivateProfileStringW(L"UFC6",L"keep_env",L"1",ini));
    assert(avatar_readback_set(bin,AVATAR_READBACK_ALWAYS,error,160));
    assert(avatar_readback_set(bin,AVATAR_READBACK_OFF,error,160));
    assert(GetPrivateProfileIntW(L"UFC6",L"keep_env",0,ini)==1);
    empty_file(bfile);assert(!avatar_readback_set(bin,AVATAR_READBACK_REPAIR,error,160));
    assert(GetFileAttributesW(state)==INVALID_FILE_ATTRIBUTES&&GetFileAttributesW(nfile)==INVALID_FILE_ATTRIBUTES);
    assert(DeleteFileW(bfile)&&DeleteFileW(wrapper)&&DeleteFileW(ini));
    assert(RemoveDirectoryW(bin)&&RemoveDirectoryW(novo)&&RemoveDirectoryW(root));
    puts("PASS: portrait readback repair/always/off, both engines, prior settings restoration and existing-file protection.");
    return 0;
}
