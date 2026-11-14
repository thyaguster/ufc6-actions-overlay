/* SPDX-License-Identifier: GPL-2.0-only
 * Forward the new PORTHEON engine's imported WINMM timing calls. Its local
 * WINMM dependency gives the overlay a per-game Vulkan bootstrap without
 * changing the executable, launcher, registry, or other Vulkan applications.
 */
#define WIN32_LEAN_AND_MEAN
#define UNICODE
#define _UNICODE
#include <windows.h>
#include <mmsystem.h>
#include "overlay_bootstrap.h"

static INIT_ONCE winmm_once=INIT_ONCE_STATIC_INIT;
static HMODULE real_winmm;

static BOOL CALLBACK load_winmm(PINIT_ONCE once,PVOID parameter,PVOID *context) {
    (void)once;(void)parameter;(void)context;
    wchar_t path[MAX_PATH];
    UINT length=GetSystemDirectoryW(path,MAX_PATH);
    if(!length||length+11>=MAX_PATH)return FALSE;
    lstrcatW(path,L"\\winmm.dll");
    real_winmm=LoadLibraryW(path);
    return real_winmm!=NULL;
}

static FARPROC system_proc(const char *name) {
    if(!InitOnceExecuteOnce(&winmm_once,load_winmm,NULL,NULL))return NULL;
    return GetProcAddress(real_winmm,name);
}

MMRESULT WINAPI ProxyTimeBeginPeriod(UINT period) {
    typedef MMRESULT (WINAPI *Fn)(UINT);
    Fn fn=(Fn)system_proc("timeBeginPeriod");
    return fn?fn(period):TIMERR_NOCANDO;
}

MMRESULT WINAPI ProxyTimeEndPeriod(UINT period) {
    typedef MMRESULT (WINAPI *Fn)(UINT);
    Fn fn=(Fn)system_proc("timeEndPeriod");
    return fn?fn(period):TIMERR_NOCANDO;
}

BOOL WINAPI DllMain(HINSTANCE module,DWORD reason,LPVOID reserved) {
    (void)reserved;
    if(reason==DLL_PROCESS_ATTACH){DisableThreadLibraryCalls(module);overlay_bootstrap(module);}
    return TRUE;
}
