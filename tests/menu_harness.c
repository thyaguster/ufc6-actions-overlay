/* Uses the real DLL UI and hooks with fake controller callbacks, never a game. */
#define UNICODE
#define _UNICODE
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>

typedef void (*StartFn)(HWND);
typedef void (*ButtonFn)(uint32_t,int);
typedef void (*ToggleFn)(void);
static ButtonFn button;
static ToggleFn toggle;
static HWND owner;
static int stage, closed;
static LRESULT CALLBACK proc(HWND w, UINT m, WPARAM wp, LPARAM lp) {
    if (m == WM_PAINT) {
        PAINTSTRUCT ps; HDC dc = BeginPaint(w,&ps); RECT r; GetClientRect(w,&r);
        HBRUSH b = CreateSolidBrush(RGB(40,53,68)); FillRect(dc,&r,b); DeleteObject(b);
        SetBkMode(dc,TRANSPARENT); SetTextColor(dc,RGB(230,230,230));
        DrawTextW(dc,L"Kyty quit menu - isolated test host",-1,&r,DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        EndPaint(w,&ps); return 0;
    }
    if (m == WM_TIMER) {
        ++stage;
        if (stage == 2) toggle();
        if (stage == 5) {button(0x40,1);button(0x40,0);}
        if (stage == 8) {button(0x2000,1);button(0x2000,0);}
        if (stage == 10) toggle();
        if (stage == 13) {for(int i=0;i<5;++i){button(0x40,1);button(0x40,0);}}
        if (stage == 16) {button(0x4000,1);button(0x4000,0);}
        if (stage == 20) {puts("FAIL: quit did not close host"); PostQuitMessage(1);}
        return 0;
    }
    if (m == WM_CLOSE) {closed=1; puts("PASS: real menu posted WM_CLOSE to its host"); DestroyWindow(w);return 0;}
    if (m == WM_DESTROY) {PostQuitMessage(closed ? 0 : 1);return 0;}
    return DefWindowProcW(w,m,wp,lp);
}
int main(void) {
    HINSTANCE app = GetModuleHandleW(NULL);
    WNDCLASSW wc={0}; wc.lpfnWndProc=proc;wc.hInstance=app;wc.lpszClassName=L"QuitMenuTestHost";
    RegisterClassW(&wc);
    owner=CreateWindowW(wc.lpszClassName,L"Quit menu test host",WS_OVERLAPPEDWINDOW|WS_VISIBLE,
                        100,100,1100,720,NULL,NULL,app,NULL);
    SetForegroundWindow(owner);
    HMODULE dll=LoadLibraryW(L"quit_menu_test.dll");
    if (!dll) {printf("Load failed: %lu\n",GetLastError());return 1;}
    StartFn start=(StartFn)GetProcAddress(dll,"MenuTestStart");
    button=(ButtonFn)GetProcAddress(dll,"MenuTestButton");
    toggle=(ToggleFn)GetProcAddress(dll,"MenuTestToggle");
    if (!start || !button || !toggle) return 1;
    start(owner); SetTimer(owner,1,1000,NULL);
    MSG msg;
    while (GetMessageW(&msg,NULL,0,0)>0) {TranslateMessage(&msg);DispatchMessageW(&msg);}
    return (int)msg.wParam;
}
