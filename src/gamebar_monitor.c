/* Read-only Windows.Gaming.UI.GameBar ABI. Poll outside the UI/render thread:
 * activation/property calls can block on Windows services. No Game Bar settings
 * are changed, and unavailable APIs leave the normal overlay path intact. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include "gamebar_monitor.h"
typedef HRESULT (WINAPI *RoInit)(unsigned);
typedef void (WINAPI *RoUninit)(void);
typedef HRESULT (WINAPI *StringCreate)(const wchar_t*,unsigned,void**);
typedef HRESULT (WINAPI *StringDelete)(void*);
typedef HRESULT (WINAPI *FactoryGet)(void*,const GUID*,void**);
typedef struct { void *slots[11]; HRESULT (WINAPI *redirected)(void*,BYTE*); } GameBarVtable;
static volatile LONG redirected,stop_monitor;
static HANDLE monitor_thread;
static DWORD WINAPI monitor(void *unused){
    (void)unused;
    HMODULE api=LoadLibraryExW(L"combase.dll",NULL,LOAD_LIBRARY_SEARCH_SYSTEM32);
    if(!api)return 0;
    RoInit init=(RoInit)GetProcAddress(api,"RoInitialize");
    RoUninit uninit=(RoUninit)GetProcAddress(api,"RoUninitialize");
    StringCreate create=(StringCreate)GetProcAddress(api,"WindowsCreateString");
    StringDelete destroy=(StringDelete)GetProcAddress(api,"WindowsDeleteString");
    FactoryGet factory=(FactoryGet)GetProcAddress(api,"RoGetActivationFactory");
    if(!init||!uninit||!create||!destroy||!factory){FreeLibrary(api);return 0;}
    HRESULT initialized=init(1); /* RO_INIT_MULTITHREADED */
    if(FAILED(initialized)){FreeLibrary(api);return 0;}
    static const wchar_t name[]=L"Windows.Gaming.UI.GameBar";
    static const GUID iid={0x1db9a292,0xcc78,0x4173,{0xbe,0x45,0xb6,0x1e,0x67,0x28,0x3e,0xa7}};
    void *string=NULL,*object=NULL;
    if(SUCCEEDED(create(name,(unsigned)(sizeof(name)/sizeof(name[0])-1),&string))){
        factory(string,&iid,&object);destroy(string);
    }
    if(object){
        const GameBarVtable *vt=*(const GameBarVtable**)object;
        while(!InterlockedCompareExchange(&stop_monitor,0,0)){
            BYTE value=0;HRESULT result=vt->redirected(object,&value);
            InterlockedExchange(&redirected,SUCCEEDED(result)&&value!=0);
            Sleep(250);
        }
        ((ULONG (WINAPI*)(void*))vt->slots[2])(object);
    }
    InterlockedExchange(&redirected,0);uninit();FreeLibrary(api);return 0;
}
void gamebar_monitor_start(void){
    if(monitor_thread)return;
    InterlockedExchange(&stop_monitor,0);
    monitor_thread=CreateThread(NULL,0,monitor,NULL,0,NULL);
}
void gamebar_monitor_stop(void){
    InterlockedExchange(&stop_monitor,1);InterlockedExchange(&redirected,0);
    /* Never wait on a Windows-service call during game shutdown. The DLL and
     * worker code remain mapped until process exit, just like controller hooks. */
    if(monitor_thread){CloseHandle(monitor_thread);monitor_thread=NULL;}
}
int gamebar_input_redirected(void){return InterlockedCompareExchange(&redirected,0,0)!=0;}
