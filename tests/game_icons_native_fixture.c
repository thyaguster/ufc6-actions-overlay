/* Hidden fixture engine for the real C -> packaged Python helper handoff.
   Run only with a disposable, already-PlayStation game-data fixture. */
#define WIN32_LEAN_AND_MEAN
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#include <windows.h>
#include "../src/game_icons.h"
static LRESULT CALLBACK proc(HWND window,UINT message,WPARAM wp,LPARAM lp){
    if(message==WM_CLOSE){PostQuitMessage(0);return 0;}
    return DefWindowProcW(window,message,wp,lp);
}
int wmain(void){
    HINSTANCE module=GetModuleHandleW(NULL);WNDCLASSW cls={.lpfnWndProc=proc,.hInstance=module,.lpszClassName=L"UFC6IconsHandoffFixture"};
    if(!RegisterClassW(&cls))return 2;
    HWND window=CreateWindowW(cls.lpszClassName,L"Hidden fixture",0,0,0,10,10,NULL,NULL,module,NULL);if(!window)return 3;
    game_icons_init(module);game_icons_check();ULONGLONG until=GetTickCount64()+15000;
    while(game_icons_status(NULL,0)==GAME_ICONS_CHECKING&&GetTickCount64()<until)Sleep(10);
    if(game_icons_status(NULL,0)!=GAME_ICONS_PS)return 4;
    if(!game_icons_apply(window,FALSE))return 5;
    MSG message;while(GetMessageW(&message,NULL,0,0)>0){TranslateMessage(&message);DispatchMessageW(&message);}
    DestroyWindow(window);return 0;
}
