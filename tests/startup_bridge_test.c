/* Reproduce PreferSystem32Images at process creation, with a test-only original runtime. */
#define WIN32_LEAN_AND_MEAN
#define UNICODE
#define _UNICODE
#include <windows.h>
#include <stdio.h>
#include <wchar.h>
__declspec(dllimport) int clock_getres(int,void*);
#define REQUIRE(x) do{if(!(x)){printf("FAIL %s (Windows error %lu)\n",#x,(unsigned long)GetLastError());return 1;}}while(0)
static DWORD WINAPI small_thread(void *unused){(void)unused;return 0;}
int main(int argc,char **argv){
    (void)argv;
    if(argc>1){
        PROCESS_MITIGATION_IMAGE_LOAD_POLICY policy={0};
        REQUIRE(GetProcessMitigationPolicy(GetCurrentProcess(),ProcessImageLoadPolicy,&policy,sizeof(policy)));
        REQUIRE(policy.PreferSystem32Images);
        REQUIRE(clock_getres(0,NULL)==7331);
        if(argc>2){REQUIRE(!GetEnvironmentVariableW(L"VK_INSTANCE_LAYERS",NULL,0));puts("PASS: missing overlay files leave original runtime forwarding active without a broken Vulkan layer.");return 0;}
        wchar_t value[1024];REQUIRE(GetEnvironmentVariableW(L"VK_INSTANCE_LAYERS",value,1024));
        REQUIRE(wcsstr(value,L"VK_LAYER_UFC6_quit_menu")!=NULL);
        HMODULE system=LoadLibraryW(L"version.dll");REQUIRE(system);
        REQUIRE(!GetProcAddress(system,"Ufc6RenderMenu"));
        wchar_t path[MAX_PATH];REQUIRE(GetModuleFileNameW(NULL,path,MAX_PATH));
        wchar_t *slash=wcsrchr(path,L'\\');REQUIRE(slash);wcscpy(slash+1,L"version.dll");
        HMODULE core=LoadLibraryExW(path,NULL,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32);REQUIRE(core);
        REQUIRE(GetProcAddress(core,"Ufc6RenderMenu")&&GetProcAddress(core,"MenuTestScale"));
        REQUIRE(core!=system&&GetModuleHandleW(path)==core);
        HANDLE tiny=CreateThread(NULL,65536,small_thread,NULL,STACK_SIZE_PARAM_IS_A_RESERVATION,NULL);REQUIRE(tiny);
        REQUIRE(WaitForSingleObject(tiny,5000)==WAIT_OBJECT_0);CloseHandle(tiny);
        puts("PASS: startup under PreferSystem32Images; original API forwarded, layer enabled, app core loaded beside system VERSION.dll.");return 0;
    }
    wchar_t self[MAX_PATH],command[MAX_PATH+30];REQUIRE(GetModuleFileNameW(NULL,self,MAX_PATH));
    swprintf(command,MAX_PATH+30,L"\"%ls\" --child",self);
    SIZE_T size=0;InitializeProcThreadAttributeList(NULL,1,0,&size);
    LPPROC_THREAD_ATTRIBUTE_LIST list=HeapAlloc(GetProcessHeap(),0,size);REQUIRE(list);
    REQUIRE(InitializeProcThreadAttributeList(list,1,0,&size));
    DWORD64 mitigation=PROCESS_CREATION_MITIGATION_POLICY_IMAGE_LOAD_PREFER_SYSTEM32_ALWAYS_ON;
    REQUIRE(UpdateProcThreadAttribute(list,0,PROC_THREAD_ATTRIBUTE_MITIGATION_POLICY,&mitigation,sizeof(mitigation),NULL,NULL));
    STARTUPINFOEXW start={.StartupInfo={.cb=sizeof(start)},.lpAttributeList=list};PROCESS_INFORMATION process={0};
    SetEnvironmentVariableW(L"VK_INSTANCE_LAYERS",NULL);SetEnvironmentVariableW(L"VK_ADD_LAYER_PATH",NULL);SetEnvironmentVariableW(L"VK_LAYER_PATH",NULL);
    wchar_t core[MAX_PATH],disabled[MAX_PATH+24];wcscpy(core,self);wchar_t *slash=wcsrchr(core,L'\\');REQUIRE(slash);wcscpy(slash+1,L"version.dll");
    swprintf(disabled,MAX_PATH+24,L"%ls.disabled.test",core);
    for(int missing=0;missing<2;++missing){
        if(missing)REQUIRE(MoveFileW(core,disabled));
        swprintf(command,MAX_PATH+30,L"\"%ls\" --child%ls",self,missing?L" missing":L"");
        REQUIRE(CreateProcessW(self,command,NULL,NULL,FALSE,EXTENDED_STARTUPINFO_PRESENT,NULL,NULL,&start.StartupInfo,&process));
        REQUIRE(WaitForSingleObject(process.hProcess,15000)==WAIT_OBJECT_0);
        DWORD code;REQUIRE(GetExitCodeProcess(process.hProcess,&code));CloseHandle(process.hProcess);CloseHandle(process.hThread);
        if(missing)REQUIRE(MoveFileW(disabled,core));
        REQUIRE(code==0);
    }
    DeleteProcThreadAttributeList(list);HeapFree(GetProcessHeap(),0,list);return 0;
}
