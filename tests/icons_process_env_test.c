#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#undef NDEBUG
#include <assert.h>
#include <stdio.h>
#include "../src/icons_process_env.h"
int main(void){
    assert(SetEnvironmentVariableW(L"PYINSTALLER_RESET_ENVIRONMENT",L"0"));
    assert(SetEnvironmentVariableW(L"_PYI_APPLICATION_HOME_DIR",L"missing runtime"));
    assert(SetEnvironmentVariableW(L"UFC6_TEST_PRESERVE",L"100% unchanged"));
    wchar_t *block=icons_process_environment();assert(block);int resets=0,preserved=0;const wchar_t *previous=NULL;
    for(const wchar_t *p=block;*p;p+=wcslen(p)+1){
        if(previous)assert(_wcsicmp(previous,p)<=0);previous=p;
        if(!_wcsicmp(p,L"PYINSTALLER_RESET_ENVIRONMENT=1"))++resets;
        if(!_wcsicmp(p,L"UFC6_TEST_PRESERVE=100% unchanged"))++preserved;
        assert(_wcsicmp(p,L"PYINSTALLER_RESET_ENVIRONMENT=0"));
    }
    wchar_t value[64];assert(GetEnvironmentVariableW(L"PYINSTALLER_RESET_ENVIRONMENT",value,64)&&!wcscmp(value,L"0"));
    assert(resets==1&&preserved==1);HeapFree(GetProcessHeap(),0,block);
    puts("PASS: independent helper environment, sorted Unicode block, single reset flag, unrelated values preserved and parent environment unchanged.");return 0;
}
