/* SPDX-License-Identifier: GPL-2.0-only */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#include <stdio.h>
#include <wchar.h>

typedef MMRESULT (WINAPI *TimerFn)(UINT);

int wmain(int argc,wchar_t **argv) {
    if(argc!=2)return 2;
    HMODULE bridge=LoadLibraryW(argv[1]);
    if(!bridge){fprintf(stderr,"bridge load failed: %lu\n",GetLastError());return 1;}
    TimerFn begin=(TimerFn)GetProcAddress(bridge,"timeBeginPeriod");
    TimerFn end=(TimerFn)GetProcAddress(bridge,"timeEndPeriod");
    if(!begin||!end){fprintf(stderr,"timing exports missing\n");return 1;}
    MMRESULT result=begin(1);
    if(result==TIMERR_NOERROR)result=end(1);
    if(result!=TIMERR_NOERROR){fprintf(stderr,"forwarding failed: %u\n",result);return 1;}
    wchar_t layers[1024]={0};
    DWORD count=GetEnvironmentVariableW(L"VK_INSTANCE_LAYERS",layers,1024);
    if(!count||count>=1024||!wcsstr(layers,L"VK_LAYER_UFC6_quit_menu")){
        fprintf(stderr,"Vulkan layer bootstrap missing\n");return 1;
    }
    puts("PASS: local WINMM timing calls forwarded and Vulkan layer enabled for this process.");
    FreeLibrary(bridge);
    return 0;
}
