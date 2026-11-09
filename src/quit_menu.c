/* In-process quit menu for the verified Kyty UFC6 3309ab23f build. */
#define WIN32_LEAN_AND_MEAN
#define UNICODE
#define _UNICODE
#include <windows.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <xinput.h>
#include "exact_hooks.h"
#include "menu_state.h"
#include "restart_client.h"
#include "xinput_menu.h"
#include "gamebar_shortcut.h"
#include "gamebar_monitor.h"
#include "launcher_dialog.h"
#ifndef QUIT_MENU_TEST
#include "engine_fingerprint.h"
#endif

#define WM_MENU_ACTION (WM_APP + 40)
#define WM_MENU_TOGGLE (WM_APP + 41)
#define BUTTON_RVA 0x126f040
#define AXIS_RVA 0x126f270
#define DISCONNECT_RVA 0x126ef60
#define CONTROLLER_RVA 0x3937c90

typedef void (*ButtonFn)(void *, int, uint32_t, bool);
typedef void (*AxisFn)(void *, int, int, int);
typedef void (*DisconnectFn)(void *, int);
typedef DWORD (WINAPI *XStateFn)(DWORD, XINPUT_STATE *);
static ButtonFn original_button;
static AxisFn original_axis;
static DisconnectFn original_disconnect;
static CRITICAL_SECTION lock;
static MenuState menu;
static HINSTANCE instance;
static HWND game_window, overlay;
static DWORD ui_thread;
static HFONT font_title, font_body, font_small;
static XStateFn xstate;
static wchar_t log_path[MAX_PATH];
static wchar_t settings_path[MAX_PATH];
static int settings_save_failed;
static int guide_was_down, f10_was_down;
static int test_mode;
#ifdef QUIT_MENU_TEST
static int test_guide;
static int test_gamebar_shortcut=1;
static volatile LONG test_gamebar_redirected;
static int test_raw_input;
static int test_primary_id=1,test_secondary_id=2;
#endif
static XINPUT_STATE raw_states[4];
static int raw_connected[4],guide_source=-1,raw_owner=-1,raw_navigation;
static uint32_t raw_previous;
static uint32_t raw_idle_previous[4];
static volatile LONG vulkan_active;
static volatile LONG screenshot_request,screenshot_busy,screenshot_capable;
static wchar_t status_text[120];
static uint64_t status_until;
static uint32_t render_width=MENU_BASE_WIDTH,render_height=MENU_BASE_HEIGHT;

static void status(const wchar_t *message) {
    EnterCriticalSection(&lock);lstrcpynW(status_text,message,120);status_until=GetTickCount64()+5000;LeaveCriticalSection(&lock);
}

typedef struct {
    void *controller;
    int id, used;
    uint32_t buttons;
    int axes[6];
} InputDevice;
static InputDevice devices[16];
static int menu_device = -1;
static int wait_guide_release;
static int wait_access_release;
static int wait_access_guide_release;
static uint64_t next_gamebar_read;
static int gamebar_conflict;
static int launcher_setup_active;
static uintptr_t engine_base;
static int engine_parsec;
static void *native_manager;
static int native_primary_id=-1,native_secondary_id=-1;
static int input_redirected(void){
#ifdef QUIT_MENU_TEST
    if(test_mode)return InterlockedCompareExchange(&test_gamebar_redirected,0,0)!=0;
#endif
    return gamebar_input_redirected();
}

static void log_line(const char *line) {
    HANDLE h = CreateFileW(log_path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE,
                           NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h != INVALID_HANDLE_VALUE) {
        DWORD n;
        WriteFile(h, line, (DWORD)strlen(line), &n, NULL);
        WriteFile(h, "\r\n", 2, &n, NULL);
        CloseHandle(h);
    }
}
static int foreground(void);
static int read_gamebar(void){
#ifdef QUIT_MENU_TEST
    if(test_mode)return test_gamebar_shortcut;
#endif
    return gamebar_shortcut_read();
}
static void refresh_gamebar(void){
    int value=read_gamebar();
    EnterCriticalSection(&lock);menu.gamebar_shortcut=value;LeaveCriticalSection(&lock);
}
static void change_gamebar(void){
    if(!test_mode&&!foreground())return;
    int before=read_gamebar();LONG result=ERROR_INVALID_DATA;
    if(before>=0){
#ifdef QUIT_MENU_TEST
        if(test_mode){test_gamebar_shortcut=!before;result=ERROR_SUCCESS;}else
#endif
        result=gamebar_shortcut_write(!before);
    }
    refresh_gamebar();
    if(result==ERROR_SUCCESS){
        status(before?L"Game Bar controller shortcut Off. Win+G still works.":L"Game Bar controller shortcut On. Guide may conflict.");
        log_line(before?"Windows Game Bar controller shortcut disabled.":"Windows Game Bar controller shortcut enabled.");
    }else status(L"Windows Game Bar setting could not be changed.");
}
static InputDevice *device_for(void *controller, int id) {
    for (int i = 0; i < 16; ++i)
        if (devices[i].used && devices[i].controller == controller && devices[i].id == id)
            return &devices[i];
    for (int i = 0; i < 16; ++i) if (!devices[i].used) {
        memset(&devices[i],0,sizeof(devices[i]));
        devices[i].used = 1; devices[i].controller = controller; devices[i].id = id;
        for (int a = 0; a < 6; ++a) devices[i].axes[a] = a < 4 ? 128 : 0;
        return &devices[i];
    }
    return NULL;
}
/* Only exact, verified engine profiles enable these read-only identity offsets.
 * SDL instance IDs are arbitrary; they are not player numbers. */
static void refresh_native_players(void){
#ifdef QUIT_MENU_TEST
    if(test_mode){native_manager=(void*)1;native_primary_id=test_primary_id;native_secondary_id=test_secondary_id;return;}
#endif
    native_primary_id=-1;native_secondary_id=-1;native_manager=NULL;
    if(!engine_base)return;
    HANDLE self=GetCurrentProcess();void *manager=NULL;int primary=-1;
    if(!ReadProcessMemory(self,(void*)(engine_base+CONTROLLER_RVA),&manager,sizeof(manager),NULL)||!manager)return;
    if(!ReadProcessMemory(self,(BYTE*)manager+0x28,&primary,sizeof(primary),NULL))return;
    native_manager=manager;native_primary_id=primary;
    if(engine_parsec){int state[2];if(ReadProcessMemory(self,(void*)(engine_base+0x3c04008),state,sizeof(state),NULL)&&state[1])native_secondary_id=state[0];}
}
static int native_player(const InputDevice *d){
    if(d->controller!=native_manager||d->id<0)return 0;
    return d->id==native_primary_id?1:d->id==native_secondary_id?2:0;
}
static int native_allowed(const InputDevice *d){return menu_controller_allowed(&menu,native_player(d));}
static int raw_allowed(int slot){return menu_controller_allowed(&menu,slot+1);}
static uint32_t raw_menu_buttons(int slot);
static void reset_menu_owner(void){
    menu_device=-1;raw_owner=-1;raw_previous=0;raw_navigation=0;
    for(int slot=0;slot<4;++slot){raw_idle_previous[slot]=raw_menu_buttons(slot);if(raw_connected[slot])raw_navigation=1;}
    menu.armed=0;wait_access_release=1;wait_access_guide_release=1;
}
static int foreground(void) {
    HWND f = GetForegroundWindow();
    return f && (f == game_window || f == overlay);
}
/* All callers hold lock, including calls into the original controller mutex. */
static void neutralize(void) {
    for (int i = 0; i < 16; ++i) if (devices[i].used) {
        InputDevice *d = &devices[i];
        original_button(d->controller, d->id, 0x0010fffeu, false);
        for (int a = 0; a < 6; ++a)
            original_axis(d->controller, d->id, a, a < 4 ? 128 : 0);
    }
}
static void action(int result) {
    if(result==MENU_ACCESS_CHANGED)reset_menu_owner();
    if (result != MENU_NONE) PostThreadMessageW(ui_thread, WM_MENU_ACTION, result, 0);
}
static void navigate_device(int index,uint32_t pressed){
    const uint32_t navigation[]={PAD_B,PAD_UP,PAD_DOWN,PAD_LEFT,PAD_RIGHT,PAD_A};
    if(wait_access_release||!native_allowed(&devices[index])||launcher_setup_active||input_redirected()||!menu.visible||!menu.armed||menu.capturing||!(pressed&(PAD_B|PAD_UP|PAD_DOWN|PAD_LEFT|PAD_RIGHT|PAD_A)))return;
    if(raw_navigation){
        if(raw_owner>=0)return;
        /* Guide can be reported by an XInput bridge while actual navigation
         * arrives through SDL/HID. Whichever path sees a fresh action owns it. */
        raw_navigation=0;raw_owner=-1;
        log_line("Guide navigation acquired through engine controller input.");
    }
    if(menu_device<0)menu_device=index;
    if(menu_device!=index)return;
    for(unsigned i=0;i<sizeof(navigation)/sizeof(navigation[0]);++i)if(pressed&navigation[i]){action(menu_button(&menu,navigation[i],1));break;}
}
static void hooked_button(void *controller, int id, uint32_t button, bool down) {
    EnterCriticalSection(&lock);
    InputDevice *d = device_for(controller, id);
    if (d) {
        uint32_t before = d->buttons;
        d->buttons = down ? before | button : before & ~button;
        int index = (int)(d - devices);
        if(down)navigate_device(index,button&~before);
    }
    if (!menu.visible && !menu.draining)
        original_button(controller, id, button, down);
    LeaveCriticalSection(&lock);
}
static void hooked_axis(void *controller, int id, int axis, int value) {
    EnterCriticalSection(&lock);
    InputDevice *d = device_for(controller, id);
    if (d && axis >= 0 && axis < 6){
        uint32_t before=native_menu_stick(d->axes[0],d->axes[1]);d->axes[axis]=value;
        if(axis<2)navigate_device((int)(d-devices),native_menu_stick(d->axes[0],d->axes[1])&~before);
    }
    if (!menu.visible && !menu.draining)
        original_axis(controller, id, axis, value);
    LeaveCriticalSection(&lock);
}
static void hooked_disconnect(void *controller, int id) {
    EnterCriticalSection(&lock);
    for (int i = 0; i < 16; ++i)
        if (devices[i].used && devices[i].controller == controller && devices[i].id == id) {
            devices[i].used = 0;
            if (menu.visible && menu_device == i) {
                action(menu_close(&menu,MENU_RESUME));
            }
        }
    original_disconnect(controller, id);
    LeaveCriticalSection(&lock);
}

static BOOL CALLBACK find_window(HWND w, LPARAM unused) {
    DWORD pid = 0;
    wchar_t cls[80];
    (void)unused;
    GetWindowThreadProcessId(w, &pid);
    GetClassNameW(w, cls, 80);
    if (pid == GetCurrentProcessId() && IsWindowVisible(w) &&
        GetWindow(w, GW_OWNER) == NULL && lstrcmpW(cls, L"SDL_app") == 0) {
        game_window = w;
        return FALSE;
    }
    return TRUE;
}
static void text(HDC dc, HFONT f, const wchar_t *str, RECT r, COLORREF color, UINT flags) {
    HFONT previous = SelectObject(dc, f);
    SetTextColor(dc, color);
    DrawTextW(dc, str, -1, &r, flags);
    SelectObject(dc, previous);
}
static void button_icon(HDC dc, int x, int y, int back, int playstation) {
    COLORREF color = playstation ? (back ? RGB(244,135,148) : RGB(134,192,255))
                                 : (back ? RGB(242,105,103) : RGB(139,218,100));
    HPEN pen = CreatePen(PS_SOLID, 2, color);
    HGDIOBJ old_pen = SelectObject(dc, pen);
    HGDIOBJ old_brush = SelectObject(dc, GetStockObject(NULL_BRUSH));
    if (playstation && !back) {
        MoveToEx(dc, x+6, y+6, NULL); LineTo(dc, x+23, y+23);
        MoveToEx(dc, x+23, y+6, NULL); LineTo(dc, x+6, y+23);
    } else {
        Ellipse(dc, x+2, y+2, x+28, y+28);
        if (!playstation) {
            RECT label={x,y,x+30,y+29};
            text(dc,font_small,back ? L"B" : L"A",label,color,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
        }
    }
    SelectObject(dc,old_brush); SelectObject(dc,old_pen); DeleteObject(pen);
}
static void dpad_icon(HDC dc, int x, int y) {
    POINT outline[]={{x+10,y+1},{x+20,y+1},{x+20,y+10},{x+29,y+10},
        {x+29,y+20},{x+20,y+20},{x+20,y+29},{x+10,y+29},
        {x+10,y+20},{x+1,y+20},{x+1,y+10},{x+10,y+10}};
    HPEN pen=CreatePen(PS_SOLID,1,RGB(187,196,209));
    HBRUSH brush=CreateSolidBrush(RGB(51,61,75));
    HGDIOBJ old_pen=SelectObject(dc,pen),old_brush=SelectObject(dc,brush);
    Polygon(dc,outline,12);
    SelectObject(dc,old_pen); SelectObject(dc,old_brush); DeleteObject(pen); DeleteObject(brush);
}
static void save_settings(void) {
    EnterCriticalSection(&lock); int ps=menu.playstation_icons,scale=menu_scale(&menu),auto_launch=menu_auto_launch(&menu),guide_enabled=menu_guide_enabled(&menu),first_only=menu.first_player_only;uint32_t chord=menu_shortcut(&menu); LeaveCriticalSection(&lock);
    wchar_t value[24];swprintf(value,24,L"0x%08lx",(unsigned long)chord);
    BOOL saved=WritePrivateProfileStringW(L"Menu",L"ButtonIcons",ps?L"PlayStation":L"Xbox",settings_path);
    saved=WritePrivateProfileStringW(L"Menu",L"OpeningShortcut",value,settings_path)&&saved;
    swprintf(value,24,L"%d",scale);
    saved=WritePrivateProfileStringW(L"Menu",L"UIScale",value,settings_path)&&saved;
    saved=WritePrivateProfileStringW(L"Menu",L"AutoLaunchAfterRestart",auto_launch?L"1":L"0",settings_path)&&saved;
    saved=WritePrivateProfileStringW(L"Menu",L"GuideButtonOpensMenu",guide_enabled?L"1":L"0",settings_path)&&saved;
    saved=WritePrivateProfileStringW(L"Menu",L"ControllerAccess",first_only?L"Player1":L"Either",settings_path)&&saved;
    EnterCriticalSection(&lock); settings_save_failed=!saved; LeaveCriticalSection(&lock);
    log_line(saved ? "Overlay settings saved." : "Could not save overlay settings.");
}
static void shortcut_label(uint32_t chord,int ps,wchar_t *out,size_t capacity) {
    static const uint32_t buttons[]={2,4,PAD_LB,PAD_RB,PAD_MENU,PAD_UP,PAD_DOWN,PAD_LEFT,PAD_RIGHT,0x1000,PAD_B,PAD_A,0x8000};
    static const wchar_t *xbox[]={L"LS",L"RS",L"LB",L"RB",L"Menu",L"Up",L"Down",L"Left",L"Right",L"Y",L"B",L"A",L"X"};
    static const wchar_t *sony[]={L"L3",L"R3",L"L1",L"R1",L"Options",L"Up",L"Down",L"Left",L"Right",L"Triangle",L"Circle",L"Cross",L"Square"};
    out[0]=0;
    for(size_t i=0;i<sizeof(buttons)/sizeof(buttons[0]);++i)if(chord&buttons[i]){
        const wchar_t *name=ps?sony[i]:xbox[i];
        size_t used=wcslen(out),needed=wcslen(name)+(used?3:0);
        if(used+needed>=capacity)break;
        if(used)wcscat(out,L" + ");wcscat(out,name);
    }
}
static void row_geometry(int page,int index,int *top,int *height) {
    *top=page==PAGE_MAIN?86+29*index+(index>=MAIN_REBOOT?10:0):page==PAGE_SETTINGS?88+26*index:100+56*index;
    *height=page==PAGE_MAIN?27:page==PAGE_SETTINGS?25:50;
}
static void draw_menu(HDC dc, RECT r) {
    HBRUSH bg = CreateSolidBrush(RGB(19, 23, 30));
    FillRect(dc, &r, bg); DeleteObject(bg);
    SetBkMode(dc, TRANSPARENT);
    MenuState state;int save_failed,conflict,owner_player=0,has_owner=0;wchar_t message[120];
    EnterCriticalSection(&lock);
    state=menu;save_failed=settings_save_failed;conflict=gamebar_conflict;
    if(raw_navigation&&raw_owner>=0){has_owner=1;owner_player=raw_owner+1;}
    else if(menu_device>=0&&devices[menu_device].used){has_owner=1;owner_player=native_player(&devices[menu_device]);}
    lstrcpynW(message,GetTickCount64()<status_until?status_text:L"",120);
    LeaveCriticalSection(&lock);
    int selection=state.selection,ps=state.playstation_icons;
    RECT title = {36, 13, r.right - 36, 57};
    text(dc,font_title,state.page==PAGE_SETTINGS?L"Settings":state.page==PAGE_RESTART?L"Restart game?":menu_power_page(&state)?(state.shutdown_countdown?(state.page==PAGE_REBOOT?L"Restarting PC":L"Shutting down PC"):(state.page==PAGE_REBOOT?L"Restart PC?":L"Shut down PC?")):L"UFC 6",title,RGB(246,247,250),DT_LEFT|DT_SINGLELINE);
    wchar_t owner_label[64];
    if(owner_player>0)swprintf(owner_label,64,L"Player %d",owner_player);
    else wcscpy(owner_label,has_owner?L"Controller":state.first_player_only?L"Waiting for Player 1":L"Waiting for controller");
    RECT owner_rect={330,25,r.right-36,48};text(dc,font_small,owner_label,owner_rect,RGB(176,184,197),DT_RIGHT|DT_SINGLELINE);
    RECT sub = {38, 57, r.right - 36, 84};
    text(dc,font_small,conflict?L"Game Bar is using your controller.":state.page==PAGE_SETTINGS?L"Customize your overlay":state.page==PAGE_RESTART?L"Close this session and use the saved launcher settings.":menu_power_page(&state)?(state.shutdown_countdown?L"You can still cancel before the countdown ends.":(state.page==PAGE_REBOOT?(menu_auto_launch(&state)?L"Game auto-launch: On. Change in Settings.":L"Game auto-launch: Off. Change in Settings."):L"Close the game and turn off this PC.")):L"What would you like to do?",sub,conflict?RGB(255,196,94):RGB(176,184,197),DT_LEFT);
    const wchar_t *main_labels[]={L"Resume",L"Take screenshot",L"Restart game",L"Settings",L"Quit game",L"Restart PC",L"Shut down PC"};
    const wchar_t *setting_labels[]={L"Button icons",L"Opening shortcut",L"UI scale",L"Launch UFC 6 after PC restart",L"Xbox / Guide button",L"Windows Game Bar shortcut",L"Launcher setup...",L"Controller access",L"Back"};
    const wchar_t *restart_labels[]={L"Cancel",L"Restart now"};
    const wchar_t *shutdown_labels[]={state.shutdown_countdown?L"Cancel":L"Cancel",state.page==PAGE_REBOOT?L"Hold to restart PC":L"Hold to shut down"};
    int first=state.page==PAGE_SETTINGS&&selection>=7?selection-6:0;
    for(int i=0;i<menu_rows(&state);++i) {
        if(state.page==PAGE_SETTINGS&&(i<first||i>=first+7))continue;
        int top,height;row_geometry(state.page,state.page==PAGE_SETTINGS?i-first:i,&top,&height);
        RECT row={36,top,r.right-36,top+height};
        HBRUSH fill=CreateSolidBrush(i==selection?RGB(106,194,68):RGB(36,43,54));
        FillRect(dc, &row, fill); DeleteObject(fill);
        COLORREF color=i == selection ? RGB(11,24,7) : RGB(234,238,244);
        row.left+=18;row.top+=(state.page==PAGE_MAIN||state.page==PAGE_SETTINGS)?-2:9;
        text(dc,font_body,state.page==PAGE_MAIN?main_labels[i]:state.page==PAGE_SETTINGS?setting_labels[i]:menu_power_page(&state)?shutdown_labels[i]:restart_labels[i],row,color,DT_LEFT);
        if(menu_power_page(&state)&&i==1&&state.shutdown_holding){
            wchar_t progress[40];swprintf(progress,40,L"%.1f / 3 s",state.shutdown_progress/1000.0);
            RECT value={330,row.top+5,r.right-55,row.bottom};text(dc,font_small,progress,value,color,DT_RIGHT|DT_SINGLELINE);
            RECT bar={36,top+height-4,36+(r.right-72)*state.shutdown_progress/3000,top+height};
            HBRUSH fill_bar=CreateSolidBrush(RGB(221,246,197));FillRect(dc,&bar,fill_bar);DeleteObject(fill_bar);
        }
        if((state.page==PAGE_MAIN&&i==MAIN_REBOOT)||(state.page==PAGE_SETTINGS&&i==SETTING_AUTO_LAUNCH)){
            RECT value={350,row.top+4,r.right-50,row.bottom};
            const wchar_t *label=state.page==PAGE_MAIN?(menu_auto_launch(&state)?L"Auto-launch: On":L"Auto-launch: Off"):(menu_auto_launch(&state)?L"< On >":L"< Off >");
            text(dc,font_small,label,value,color,DT_RIGHT|DT_SINGLELINE);
        }
        if(state.page==PAGE_SETTINGS&&(i==SETTING_GUIDE||i==SETTING_GAMEBAR)){
            RECT value={350,row.top+4,r.right-50,row.bottom};
            const wchar_t *label=i==SETTING_GUIDE?(menu_guide_enabled(&state)?L"< On >":L"< Off >"):(state.gamebar_shortcut<0?L"Unavailable":state.gamebar_shortcut?L"< On >":L"< Off >");
            text(dc,font_small,label,value,color,DT_RIGHT|DT_SINGLELINE);
        }
        if(state.page==PAGE_SETTINGS&&i==SETTING_ACCESS){
            RECT value={330,row.top+4,r.right-50,row.bottom};text(dc,font_small,state.first_player_only?L"< Player 1 only >":L"< Either >",value,color,DT_RIGHT|DT_SINGLELINE);
        }
        if(state.page==PAGE_SETTINGS&&i==SETTING_ICONS) {
            RECT value={300,row.top,r.right-50,row.bottom};
            text(dc,font_body,ps?L"< PlayStation >":L"< Xbox >",value,color,DT_RIGHT|DT_SINGLELINE);
        }
        if(state.page==PAGE_SETTINGS&&i==SETTING_SHORTCUT) {
            RECT value={268,row.top+5,r.right-50,row.bottom};wchar_t chord[100];
            shortcut_label(menu_shortcut(&state),ps,chord,100);
            text(dc,font_small,state.capturing?L"Recording...":chord,value,color,DT_RIGHT|DT_SINGLELINE|DT_END_ELLIPSIS);
        }
        if(state.page==PAGE_SETTINGS&&i==SETTING_SCALE){
            wchar_t scale[40];swprintf(scale,40,L"< %d%% >",menu_scale(&state));
            RECT value={300,row.top,r.right-50,row.bottom};text(dc,font_body,scale,value,color,DT_RIGHT|DT_SINGLELINE);
        }
    }
    if(state.page==PAGE_SETTINGS){
        RECT above={538,87,560,109},below={538,246,560,269};
        if(first>0)text(dc,font_small,L"\x25b2",above,RGB(176,184,197),DT_CENTER|DT_SINGLELINE);
        if(first+7<SETTING_COUNT)text(dc,font_small,L"\x25bc",below,RGB(176,184,197),DT_CENTER|DT_SINGLELINE);
    }
    if(menu_power_page(&state)&&state.shutdown_countdown){
        wchar_t countdown[40];swprintf(countdown,40,L"%d seconds",state.shutdown_seconds);
        RECT number={36,187,r.right-36,239};text(dc,font_title,countdown,number,RGB(246,247,250),DT_CENTER|DT_SINGLELINE);
    }
    int y=state.page==PAGE_MAIN?304:279;
    if(conflict){
        wchar_t chord[100],fallback[150];shortcut_label(menu_shortcut(&state),ps,chord,100);
        swprintf(fallback,150,L"Fallback: %ls  |  F10",chord);
        RECT help={30,y,r.right-30,y+22},keys={30,y+22,r.right-30,y+44};
        text(dc,font_small,L"Press Guide again or click the game to restore input.",help,RGB(255,196,94),DT_CENTER|DT_SINGLELINE);
        text(dc,font_small,fallback,keys,RGB(255,196,94),DT_CENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
    }else if(state.capturing) {
        RECT prompt={36,279,r.right-36,307};
        text(dc,font_small,state.capture_error?L"Use 2 to 4 buttons; hold briefly, then release.":L"Hold 2 to 4 buttons, then release to save.",prompt,RGB(220,231,245),DT_CENTER|DT_SINGLELINE);
        RECT cancel={36,308,r.right-36,333};
        text(dc,font_small,ps?L"Circle alone cancels. F10 always closes the overlay.":L"B alone cancels. F10 always closes the overlay.",cancel,RGB(158,170,188),DT_CENTER|DT_SINGLELINE);
    } else {
        dpad_icon(dc,44,y);
        RECT select={84,y+5,190,y+28};text(dc,font_small,L"Select",select,RGB(187,196,209),DT_LEFT);
        button_icon(dc,210,y,0,ps);
        const wchar_t *confirm=state.page==PAGE_SETTINGS?(selection==SETTING_SHORTCUT?L"Record":(selection==SETTING_ICONS||selection==SETTING_AUTO_LAUNCH||selection==SETTING_GUIDE||selection==SETTING_GAMEBAR||selection==SETTING_ACCESS)?L"Change":selection==SETTING_SCALE?L"Reset":L"Confirm"):menu_power_page(&state)?(state.shutdown_countdown?L"Cancel":selection==1?L"Hold 3s":L"Confirm"):L"Confirm";
        RECT yes={250,y+5,367,y+28};text(dc,font_small,confirm,yes,RGB(187,196,209),DT_LEFT);
        button_icon(dc,386,y,1,ps);
        RECT back={426,y+5,530,y+28};text(dc,font_small,state.page==PAGE_MAIN?L"Resume":L"Back",back,RGB(187,196,209),DT_LEFT);
        if(state.page==PAGE_SETTINGS) {
            wchar_t chord[100],line[130];shortcut_label(menu_shortcut(&state),ps,chord,100);
            swprintf(line,130,L"Open: %ls  |  F10",chord);
            if(selection==SETTING_SCALE)wcscpy(line,L"Left/right: resize   |   Confirm: reset to 100%");
            if(selection==SETTING_AUTO_LAUNCH)wcscpy(line,L"Reopen UFC 6 after PC restart and Windows sign-in.");
            if(selection==SETTING_GUIDE)wcscpy(line,L"Turn Game Bar shortcut Off if Guide navigation stalls.");
            if(selection==SETTING_GAMEBAR)wcscpy(line,L"Windows setting for this user. Win+G still opens Game Bar.");
            if(selection==SETTING_LAUNCHER)wcscpy(line,L"Opens desktop launcher settings. Mouse/keyboard required.");
            if(selection==SETTING_ACCESS)wcscpy(line,L"Player 1 only, or either controller. F10 always works.");
            RECT shortcut={30,314,r.right-30,336};text(dc,font_small,line,shortcut,RGB(158,170,188),DT_CENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
        }
    }
    RECT note = {20, 345, r.right - 20, 366};
    text(dc,font_small,conflict?L"In Settings, turn Windows Game Bar shortcut Off.":menu_power_page(&state)?(state.shutdown_countdown?L"Back or F10 cancels. Switching apps also cancels.":L"Unsaved game progress may be lost."):message[0]?message:save_failed?L"Settings could not be saved.":state.page==PAGE_RESTART?L"Unsaved progress may be lost.":L"The game keeps running while this menu is open.",note,
         RGB(137,148,164), DT_CENTER | DT_SINGLELINE);
}
static void paint(HWND w) {
    PAINTSTRUCT ps;
    HDC target = BeginPaint(w, &ps);
    RECT r; GetClientRect(w, &r);
    HDC dc = CreateCompatibleDC(target);
    HBITMAP bitmap = CreateCompatibleBitmap(target, r.right, r.bottom);
    HGDIOBJ previous = SelectObject(dc, bitmap);
    SetMapMode(dc,MM_ANISOTROPIC);SetWindowExtEx(dc,MENU_BASE_WIDTH,MENU_BASE_HEIGHT,NULL);
    SetViewportExtEx(dc,r.right,r.bottom,NULL);RECT logical={0,0,MENU_BASE_WIDTH,MENU_BASE_HEIGHT};draw_menu(dc,logical);
    SetMapMode(dc,MM_TEXT);
    BitBlt(target, 0, 0, r.right, r.bottom, dc, 0, 0, SRCCOPY);
    SelectObject(dc, previous); DeleteObject(bitmap); DeleteDC(dc);
    EndPaint(w, &ps);
}
__declspec(dllexport) void Ufc6VulkanActive(void) {
    InterlockedExchange(&vulkan_active,1);
    log_line("Vulkan in-frame rendering active; desktop popup disabled.");
}
__declspec(dllexport) void Ufc6ScreenshotCapability(BOOL supported) {
    InterlockedExchange(&screenshot_capable,supported?1:-1);
}
__declspec(dllexport) BOOL Ufc6ScreenshotRequest(void) {
    return InterlockedExchange(&screenshot_request,0)!=0;
}
__declspec(dllexport) void Ufc6ScreenshotResult(BOOL saved) {
    InterlockedExchange(&screenshot_busy,0);
    status(saved?L"Saved to Windows Pictures / Screenshots.":L"Screenshot could not be saved.");
    log_line(saved?"Clean game screenshot saved.":"Screenshot capture failed.");
}
static void request_screenshot(void) {
    if(InterlockedCompareExchange(&screenshot_capable,0,0)!=1){status(L"Screenshot is unavailable for this display mode.");return;}
    if(InterlockedCompareExchange(&screenshot_busy,1,0))return;
    status(L"Taking screenshot...");InterlockedExchange(&screenshot_request,1);
}
__declspec(dllexport) void Ufc6MenuExtent(uint32_t viewport_width,uint32_t viewport_height,uint32_t *width,uint32_t *height) {
    EnterCriticalSection(&lock);
    overlay_extent(menu_scale(&menu),viewport_width,viewport_height,width,height);
    render_width=*width;render_height=*height;
    LeaveCriticalSection(&lock);
}
__declspec(dllexport) BOOL Ufc6RenderMenu(void *destination,uint32_t size) {
    EnterCriticalSection(&lock);
    int visible=menu.visible&&(test_mode||foreground())&&font_body!=NULL;
    uint32_t width=render_width,height=render_height;
    LeaveCriticalSection(&lock);
    if(!visible||!width||!height)return FALSE;
    if(!destination)return TRUE;
    size_t bytes=(size_t)width*height*4;
    if(size<bytes)return FALSE;
    /* This callback runs only under the presentation layer's render lock. */
    static HDC dc;
    static HBITMAP bitmap;
    static void *pixels;
    static uint32_t bitmap_width,bitmap_height;
    static HGDIOBJ original_bitmap;
    if(!dc||bitmap_width!=width||bitmap_height!=height){
        if(bitmap){SelectObject(dc,original_bitmap);DeleteObject(bitmap);bitmap=NULL;}
        if(!dc)dc=CreateCompatibleDC(NULL);
        BITMAPINFO info={0};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth=(LONG)width;info.bmiHeader.biHeight=-(LONG)height;
        info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
        bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&pixels,NULL,0);
        if(!dc||!bitmap){if(dc)DeleteDC(dc);dc=NULL;return FALSE;}
        original_bitmap=SelectObject(dc,bitmap);bitmap_width=width;bitmap_height=height;
    }
    SetMapMode(dc,MM_ANISOTROPIC);SetWindowExtEx(dc,MENU_BASE_WIDTH,MENU_BASE_HEIGHT,NULL);
    SetViewportExtEx(dc,(int)width,(int)height,NULL);
    RECT r={0,0,MENU_BASE_WIDTH,MENU_BASE_HEIGHT};draw_menu(dc,r);GdiFlush();
    unsigned char *p=pixels;
    for(size_t i=3;i<bytes;i+=4)p[i]=255;
    memcpy(destination,pixels,bytes);
    return TRUE;
}
static LRESULT CALLBACK overlay_proc(HWND w, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_PAINT) { paint(w); return 0; }
    if (msg == WM_ERASEBKGND) return 1;
    if (msg == WM_MOUSEACTIVATE) return MA_NOACTIVATE;
    if (msg == WM_CLOSE) { PostThreadMessageW(ui_thread, WM_MENU_TOGGLE, 0, 0); return 0; }
    if (msg == WM_LBUTTONUP) {
        int y = (short)HIWORD(lp), x = (short)LOWORD(lp);
        RECT r; GetClientRect(w, &r);
        if(r.right<=0||r.bottom<=0)return 0;
        x=(int)((int64_t)x*MENU_BASE_WIDTH/r.right);y=(int)((int64_t)y*MENU_BASE_HEIGHT/r.bottom);
        EnterCriticalSection(&lock);
        int selected=-1;
        int first=menu.page==PAGE_SETTINGS&&menu.selection>=7?menu.selection-6:0;
        for(int i=0;i<menu_rows(&menu);++i){
            if(menu.page==PAGE_SETTINGS&&(i<first||i>=first+7))continue;
            int top,height;row_geometry(menu.page,menu.page==PAGE_SETTINGS?i-first:i,&top,&height);if(y>=top&&y<top+height)selected=i;
        }
        if (selected >= 0 && x >= 36 && x < (int)MENU_BASE_WIDTH-36 && menu.armed) {
            menu.selection = selected;
            action(menu_button(&menu, PAD_A, 1));
        }
        LeaveCriticalSection(&lock);
        return 0;
    }
    return DefWindowProcW(w, msg, wp, lp);
}
static void position_overlay(void) {
    if(!test_mode){if(overlay)ShowWindow(overlay,SW_HIDE);return;}
    if(InterlockedCompareExchange(&vulkan_active,0,0))return;
    RECT r; GetWindowRect(game_window, &r);
    uint32_t width,height;Ufc6MenuExtent((uint32_t)(r.right-r.left),(uint32_t)(r.bottom-r.top),&width,&height);
    SetWindowPos(overlay, HWND_TOPMOST,
                 r.left + (r.right-r.left-width)/2,
                 r.top + (r.bottom-r.top-height)/2,
                 width, height, SWP_NOACTIVATE | SWP_SHOWWINDOW);
}
static void toggle_menu(void) {
    EnterCriticalSection(&lock);
    if (menu.visible) {
        action(menu_close(&menu,MENU_RESUME));
    } else if (!menu.draining) {
        menu_show(&menu);menu_device=-1;wait_guide_release=0;
        raw_owner=-1;raw_previous=0;raw_navigation=0;
        for(int slot=0;slot<4;++slot){raw_idle_previous[slot]=raw_connected[slot]?raw_menu_buttons(slot):0;if(raw_connected[slot])raw_navigation=1;}
        neutralize(); action(MENU_OPEN);
    }
    LeaveCriticalSection(&lock);
}
static int guide_down(void) {
#ifdef QUIT_MENU_TEST
    if(test_mode&&!test_raw_input){guide_source=-1;return test_guide;}
#endif
    guide_source=-1;
    for(DWORD i=0;i<4;++i){
#ifdef QUIT_MENU_TEST
        if(!test_mode||!test_raw_input)
#endif
        raw_connected[i]=xstate&&xstate(i,&raw_states[i])==ERROR_SUCCESS;
        if(raw_connected[i]&&raw_allowed((int)i)&&(raw_states[i].Gamepad.wButtons&0x0400)&&guide_source<0)guide_source=(int)i;
    }
    return guide_source>=0;
}
static uint32_t raw_menu_buttons(int slot){
    uint32_t buttons=xinput_menu_buttons(raw_states[slot].Gamepad.wButtons);
    if(!menu.capturing)buttons|=xinput_menu_stick(raw_states[slot].Gamepad.sThumbLX,raw_states[slot].Gamepad.sThumbLY);
    return buttons;
}
static void tick(void) {
    int active = test_mode||foreground();
    int controller_active=active&&!input_redirected();
    if(GetTickCount64()>=next_gamebar_read){refresh_gamebar();next_gamebar_read=GetTickCount64()+1000;}
    int f10 = (GetAsyncKeyState(VK_F10) & 0x8000) != 0;
    EnterCriticalSection(&lock);refresh_native_players();int guide=guide_down();if(wait_access_guide_release){guide_was_down=guide;if(!guide)wait_access_guide_release=0;}int guide_pressed=menu_guide_enabled(&menu)&&guide&&!guide_was_down;int hotkey=menu_hotkey_pressed(&menu,guide,guide_was_down,f10,f10_was_down);LeaveCriticalSection(&lock);
    if (active && hotkey) toggle_menu();
    guide_was_down = guide; f10_was_down = f10;
    EnterCriticalSection(&lock);
    gamebar_conflict=menu.visible&&active&&!controller_active;
    if(guide_pressed&&hotkey&&menu.visible&&guide_source>=0){
        raw_navigation=1;raw_owner=-1;raw_previous=0;
        for(int slot=0;slot<4;++slot)raw_idle_previous[slot]=raw_connected[slot]?raw_menu_buttons(slot):0;
        menu_device=-1;
    }
    if(guide_pressed&&hotkey&&menu.visible)wait_guide_release=1;
    if(!guide&&wait_guide_release)wait_guide_release=0;
    uint64_t now = GetTickCount64();
    uint32_t all_buttons = 0;
    for (int i = 0; i < 16; ++i) if (devices[i].used) all_buttons |= devices[i].buttons;
    if(wait_access_release){
        uint32_t held=0;
        for(int i=0;i<16;++i)if(devices[i].used&&native_allowed(&devices[i]))held|=devices[i].buttons|native_menu_stick(devices[i].axes[0],devices[i].axes[1]);
        for(int slot=0;slot<4;++slot)if(raw_connected[slot]&&raw_allowed(slot))held|=raw_menu_buttons(slot);
        if(!held)wait_access_release=0;
    }
    if (!menu.visible && !menu.draining) {
        /* A chord must come entirely from one controller. */
        int owner = -1;
        for (int i = 0; i < 16; ++i)
            if (devices[i].used && native_allowed(&devices[i]) && (devices[i].buttons & menu_shortcut(&menu)) == menu_shortcut(&menu)) { owner = i; break; }
        if (owner != menu_device) { menu.chord_since = 0; menu.chord_latched = 0; }
        menu_device = owner;
        int result = menu_tick(&menu, owner >= 0 ? devices[owner].buttons : 0, now, controller_active);
        if (result == MENU_OPEN) {
            /* XInput chords use the same navigation path as Guide, so SDL focus
             * transitions cannot leave an opened menu without a controller. */
            for(int slot=0;slot<4;++slot)if(raw_connected[slot]&&raw_allowed(slot)&&(xinput_menu_buttons(raw_states[slot].Gamepad.wButtons)&menu_shortcut(&menu))==menu_shortcut(&menu)){
                raw_navigation=1;raw_owner=slot;raw_previous=raw_menu_buttons(slot);menu_device=-1;break;
            }
            neutralize(); action(result);
        }
    } else {
        int was_draining = menu.draining;
        uint32_t source=menu.draining?all_buttons:menu_device>=0&&devices[menu_device].used?devices[menu_device].buttons:0;
        if(raw_navigation){
            source=raw_owner>=0&&raw_connected[raw_owner]?raw_menu_buttons(raw_owner):0;
            if(raw_owner>=0&&!raw_connected[raw_owner]){action(menu_close(&menu,MENU_RESUME));was_draining=1;}
        }
        if(wait_guide_release||wait_access_release)source|=PAD_MENU;
        action(menu_tick(&menu, source, now, controller_active));
        if(raw_navigation){
            if(raw_owner<0&&menu.visible&&!wait_guide_release){
                for(int slot=0;slot<4;++slot){
                    uint32_t buttons=raw_connected[slot]?raw_menu_buttons(slot):0;
                    uint32_t pressed=buttons&~raw_idle_previous[slot];
                    if(raw_owner<0&&raw_allowed(slot)&&controller_active&&menu.armed&&(pressed&(PAD_B|PAD_UP|PAD_DOWN|PAD_LEFT|PAD_RIGHT|PAD_A))){raw_owner=slot;raw_previous=raw_idle_previous[slot];log_line("Guide navigation acquired through XInput.");}
                    raw_idle_previous[slot]=buttons;
                }
            }
            uint32_t buttons=raw_owner>=0&&raw_connected[raw_owner]?raw_menu_buttons(raw_owner):0;
            uint32_t pressed=buttons&~raw_previous;
            const uint32_t navigation[]={PAD_B,PAD_UP,PAD_DOWN,PAD_LEFT,PAD_RIGHT,PAD_A};
            if(controller_active&&menu.visible&&!wait_guide_release)
                for(unsigned i=0;i<sizeof(navigation)/sizeof(navigation[0]);++i)if(pressed&navigation[i]){action(menu_button(&menu,navigation[i],1));break;}
            raw_previous=buttons;
            if(!menu.visible&&!menu.draining){raw_navigation=0;raw_owner=-1;for(int i=0;i<16;++i)devices[i].buttons=0;}
        }
        if (was_draining && !menu.draining) {
            for (int i = 0; i < 16; ++i) if (devices[i].used)
                for (int a = 0; a < 6; ++a)
                    original_axis(devices[i].controller, devices[i].id, a, devices[i].axes[a]);
        }
    }
    int visible = menu.visible;
    LeaveCriticalSection(&lock);
    if (visible) {
        if (active && !IsIconic(game_window)) position_overlay();
        else ShowWindow(overlay, SW_HIDE);
    }
}
static int install_hooks(void) {
    unsigned char *base = (unsigned char *)GetModuleHandleW(NULL);
    IMAGE_DOS_HEADER *dos = (IMAGE_DOS_HEADER *)base;
    IMAGE_NT_HEADERS64 *nt = (IMAGE_NT_HEADERS64 *)(base + dos->e_lfanew);
    static const unsigned char button_bytes[] = {0x55,0x41,0x56,0x56,0x57,0x53,0x48,0x81,0xec,0xd0,0,0,0,0x48,0x8d,0xac,0x24,0x80,0,0,0};
    static const unsigned char axis_bytes[] = {0x55,0x41,0x57,0x41,0x56,0x41,0x54,0x56,0x57,0x53,0x48,0x81,0xec,0xd0,0,0,0,0x48,0x8d,0xac,0x24,0x80,0,0,0};
    static const unsigned char disconnect_bytes[] = {0x55,0x56,0x57,0x48,0x83,0xec,0x30,0x48,0x8d,0x6c,0x24,0x30,0x48,0xc7,0x45,0xf8,0xfe,0xff,0xff,0xff};
#ifndef QUIT_MENU_TEST
    int profile=engine_fingerprint();
    engine_base=(uintptr_t)base;engine_parsec=profile==2;
    if(profile==2&&nt->OptionalHeader.SizeOfImage==0x3c05000&&nt->FileHeader.TimeDateStamp==0x6adf7fd0){
        static const unsigned char pb[]={0xe9,0x56,0x40,0x99,0x02},pa[]={0xe9,0x17,0x3e,0x99,0x02},pd[]={0xe9,0xb0,0x41,0x99,0x02};
        ExactHook hooks[]={
            {base+BUTTON_RVA,hooked_button,(void**)&original_button,pb,5},
            {base+AXIS_RVA,hooked_axis,(void**)&original_axis,pa,5},
            {base+DISCONNECT_RVA,hooked_disconnect,(void**)&original_disconnect,pd,5}
        };
        const char *error;
        if(!install_parsec_hooks(hooks,3,base+0x3c03e00,&error)){log_line(error);return 0;}
        log_line("Controller hooks installed for verified PORTHEON Parsec two-player engine.");return 1;
    }
    if(profile!=1){log_line("Unsupported engine fingerprint: no hooks installed.");return 0;}
#endif
    if (nt->OptionalHeader.SizeOfImage != 0x3c03000 || nt->FileHeader.TimeDateStamp != 0x6adf7fd0 ||
        memcmp(base + BUTTON_RVA, button_bytes, sizeof(button_bytes)) ||
        memcmp(base + AXIS_RVA, axis_bytes, sizeof(axis_bytes)) ||
        memcmp(base + DISCONNECT_RVA, disconnect_bytes, sizeof(disconnect_bytes))) {
        log_line("Unsupported engine: no hooks installed."); return 0;
    }
    ExactHook hooks[] = {
        {base+BUTTON_RVA,hooked_button,(void **)&original_button,button_bytes,sizeof(button_bytes)},
        {base+AXIS_RVA,hooked_axis,(void **)&original_axis,axis_bytes,sizeof(axis_bytes)},
        {base+DISCONNECT_RVA,hooked_disconnect,(void **)&original_disconnect,disconnect_bytes,sizeof(disconnect_bytes)}
    };
    const char *error;
    if (!install_exact_hooks(hooks, 3, &error)) {
        log_line(error); log_line("Hook installation failed."); return 0;
    }
    log_line("Controller hooks installed for 3309ab23f."); return 1;
}
static DWORD WINAPI worker(void *unused) {
    (void)unused;
    MSG msg;
    ui_thread = GetCurrentThreadId();
    PeekMessageW(&msg, NULL, WM_USER, WM_USER, PM_NOREMOVE);
    GetModuleFileNameW(instance, log_path, MAX_PATH);
    wchar_t *slash = wcsrchr(log_path, L'\\');
    if (slash) lstrcpyW(slash+1, L"ufc6-quit-menu.log");
    GetModuleFileNameW(instance,settings_path,MAX_PATH);
    slash=wcsrchr(settings_path,L'\\');
    if(slash)lstrcpyW(slash+1,test_mode?L"ufc6-quit-menu.test.ini":L"ufc6-quit-menu.ini");
    wchar_t icons[32];
    GetPrivateProfileStringW(L"Menu",L"ButtonIcons",L"Xbox",icons,32,settings_path);
    EnterCriticalSection(&lock);menu.playstation_icons=lstrcmpiW(icons,L"PlayStation")==0;LeaveCriticalSection(&lock);
    wchar_t shortcut[40];GetPrivateProfileStringW(L"Menu",L"OpeningShortcut",L"",shortcut,40,settings_path);
    wchar_t *end;unsigned long chord=wcstoul(shortcut,&end,0);
    EnterCriticalSection(&lock);if(shortcut[0]&&!*end&&menu_valid_shortcut((uint32_t)chord))menu.open_chord=(uint32_t)chord;LeaveCriticalSection(&lock);
    int scale=(int)GetPrivateProfileIntW(L"Menu",L"UIScale",MENU_SCALE_DEFAULT,settings_path);
    EnterCriticalSection(&lock);menu.ui_scale_percent=overlay_valid_scale(scale)?scale:MENU_SCALE_DEFAULT;LeaveCriticalSection(&lock);
    int auto_launch=(int)GetPrivateProfileIntW(L"Menu",L"AutoLaunchAfterRestart",1,settings_path);
    EnterCriticalSection(&lock);menu.disable_auto_launch=auto_launch==0;LeaveCriticalSection(&lock);
    wchar_t access[32];GetPrivateProfileStringW(L"Menu",L"ControllerAccess",L"Either",access,32,settings_path);
    EnterCriticalSection(&lock);menu.first_player_only=!lstrcmpiW(access,L"Player1");LeaveCriticalSection(&lock);
    int guide_enabled=(int)GetPrivateProfileIntW(L"Menu",L"GuideButtonOpensMenu",1,settings_path);
    EnterCriticalSection(&lock);menu.disable_guide=guide_enabled==0;LeaveCriticalSection(&lock);
    log_line("UFC6 quit menu loaded inside engine process.");
    if (!test_mode && !install_hooks()) return 0;
    while (!game_window) { EnumWindows(find_window, 0); Sleep(100); }
    WNDCLASSW wc = {0};
    wc.lpfnWndProc = overlay_proc; wc.hInstance = instance;
    wc.lpszClassName = L"UFC6QuitMenu"; wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    RegisterClassW(&wc);
    /* Layered windows participate in desktop composition over Vulkan independent flip.
       A regular topmost popup can remain hidden behind the game's fullscreen surface. */
    overlay = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_TOPMOST | WS_EX_LAYERED,
                              wc.lpszClassName, L"UFC 6 - Quit menu", WS_POPUP,
                              0,0,570,372,game_window,NULL,instance,NULL);
    if (!overlay) { log_line("Overlay creation failed."); return 0; }
    if (!SetLayeredWindowAttributes(overlay, 0, 255, LWA_ALPHA)) {
        log_line("Layered overlay initialization failed."); DestroyWindow(overlay); return 0;
    }
    font_title = CreateFontW(-34,0,0,0,FW_BOLD,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
    font_body = CreateFontW(-22,0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
    font_small = CreateFontW(-16,0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
    HMODULE xi = LoadLibraryExW(L"xinput1_4.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (xi) xstate = (XStateFn)GetProcAddress(xi, (LPCSTR)100);
    log_line(xstate ? "Ready. LB+RB+Menu (800ms), Guide (if available), or F10." : "Ready. LB+RB+Menu (800ms) or F10. Guide unavailable.");
    refresh_gamebar();
    if(!test_mode)gamebar_monitor_start();
    SetTimer(NULL, 1, 16, NULL);
    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        if (msg.message == WM_TIMER) {
            if (!IsWindow(game_window)) break;
            tick();
        } else if (msg.message == WM_MENU_ACTION) {
            if (msg.wParam == MENU_OPEN) { position_overlay(); log_line("Menu opened."); }
            else if (msg.wParam == MENU_RESUME) { ShowWindow(overlay, SW_HIDE); log_line("Resume selected."); }
            else if (msg.wParam == MENU_QUIT) {
                ShowWindow(overlay, SW_HIDE); log_line("Quit selected: posting WM_CLOSE to game window.");
                PostMessageW(game_window, WM_CLOSE, 0, 0);
            }
            else if(msg.wParam==MENU_STYLE_CHANGED||msg.wParam==MENU_SHORTCUT_CHANGED||msg.wParam==MENU_SCALE_CHANGED||msg.wParam==MENU_AUTO_LAUNCH_CHANGED||msg.wParam==MENU_GUIDE_CHANGED||msg.wParam==MENU_ACCESS_CHANGED)save_settings();
            else if(msg.wParam==MENU_LAUNCHER_SETTINGS){
#ifdef QUIT_MENU_TEST
                status(L"Test launcher setup requested; no desktop dialog opened.");
#else
                EnterCriticalSection(&lock);launcher_setup_active=1;LeaveCriticalSection(&lock);
                BOOL saved=launcher_settings_dialog(game_window,instance,settings_path);
                EnterCriticalSection(&lock);launcher_setup_active=0;menu.armed=0;LeaveCriticalSection(&lock);
                status(saved?L"Launcher settings saved.":L"Launcher settings unchanged.");
#endif
            }
            else if(msg.wParam==MENU_GAMEBAR_CHANGED)change_gamebar();
            else if(msg.wParam==MENU_SCREENSHOT)request_screenshot();
            else if(msg.wParam==MENU_RESTART) {
                ShowWindow(overlay,SW_HIDE);
#ifdef QUIT_MENU_TEST
                log_line("Test restart requested; no game was closed.");
                status(L"Test restart requested.");
#else
                if(!menu_restart_game(game_window,instance)) {
                    EnterCriticalSection(&lock);menu.draining=0;menu_show(&menu);LeaveCriticalSection(&lock);
                    status(L"Restart could not start; the game is still running.");
                }
#endif
            }
            else if(msg.wParam==MENU_SHUTDOWN||msg.wParam==MENU_REBOOT) {
                ShowWindow(overlay,SW_HIDE);
                if(!test_mode&&(!foreground()||input_redirected())){
                    EnterCriticalSection(&lock);menu.draining=0;menu_show(&menu);LeaveCriticalSection(&lock);
                    status(L"Power action cancelled because controller input is unavailable.");continue;
                }
#ifdef QUIT_MENU_TEST
                log_line("Test shutdown requested; no Windows power operation was called.");
#else
                EnterCriticalSection(&lock);BOOL auto_launch=menu_auto_launch(&menu);LeaveCriticalSection(&lock);
                if(!(msg.wParam==MENU_REBOOT?menu_reboot_pc(game_window,instance,auto_launch):menu_shutdown_pc(game_window,instance))){
                    EnterCriticalSection(&lock);menu.draining=0;menu_show(&menu);LeaveCriticalSection(&lock);
                    status(L"Power action could not start; the game is still running.");
                }
#endif
            }
            InvalidateRect(overlay, NULL, FALSE);
        } else if (msg.message == WM_MENU_TOGGLE && (test_mode||foreground())) toggle_menu();
        else { TranslateMessage(&msg); DispatchMessageW(&msg); }
    }
    gamebar_monitor_stop();
    DestroyWindow(overlay);
    /* Keep hooks and their code mapped until the engine exits. Never unload this DLL live. */
    return 0;
}

#ifdef QUIT_MENU_TEST
static void fake_button(void *c,int id,uint32_t button,bool down) {(void)c;(void)id;(void)button;(void)down;}
static void fake_disconnect(void *c,int id){(void)c;(void)id;}
static void fake_axis(void *c,int id,int a,int v) {(void)c;(void)id;(void)a;(void)v;}
__declspec(dllexport) void MenuTestStart(HWND owner) {
    test_mode = 1; game_window = owner; original_button = fake_button; original_axis = fake_axis;original_disconnect=fake_disconnect;
    HANDLE h = CreateThread(NULL,0,worker,NULL,0,NULL); if (h) CloseHandle(h);
}
__declspec(dllexport) void MenuTestButton(uint32_t button,int down) {hooked_button((void*)1,1,button,down != 0);}
__declspec(dllexport) void MenuTestButtonDevice(int id,uint32_t button,int down){hooked_button((void*)1,id,button,down!=0);}
__declspec(dllexport) void MenuTestPlayers(int primary,int secondary){EnterCriticalSection(&lock);test_primary_id=primary;test_secondary_id=secondary;refresh_native_players();LeaveCriticalSection(&lock);}
__declspec(dllexport) int MenuTestAccess(void){EnterCriticalSection(&lock);int value=menu.first_player_only;LeaveCriticalSection(&lock);return value;}
__declspec(dllexport) int MenuTestPlayer(void){EnterCriticalSection(&lock);int value=raw_navigation&&raw_owner>=0?raw_owner+1:menu_device>=0?native_player(&devices[menu_device]):0;LeaveCriticalSection(&lock);return value;}
__declspec(dllexport) void MenuTestRedirected(int value){InterlockedExchange(&test_gamebar_redirected,value!=0);PostThreadMessageW(ui_thread,WM_TIMER,1,0);}
__declspec(dllexport) int MenuTestConflict(void){EnterCriticalSection(&lock);int value=gamebar_conflict;LeaveCriticalSection(&lock);return value;}
__declspec(dllexport) int MenuTestGameBar(void){EnterCriticalSection(&lock);int value=menu.gamebar_shortcut;LeaveCriticalSection(&lock);return value;}
__declspec(dllexport) void MenuTestAxisDevice(int id,int axis,int value){hooked_axis((void*)1,id,axis,value);}
__declspec(dllexport) void MenuTestDisconnect(int id){hooked_disconnect((void*)1,id);}
__declspec(dllexport) void MenuTestRawPad(int slot,int buttons,int connected){if(slot<0||slot>=4)return;test_raw_input=1;raw_states[slot].Gamepad.wButtons=(WORD)buttons;raw_connected[slot]=connected;PostThreadMessageW(ui_thread,WM_TIMER,1,0);}
__declspec(dllexport) void MenuTestGuide(int down){test_guide=down;PostThreadMessageW(ui_thread,WM_TIMER,1,0);}
__declspec(dllexport) int MenuTestOwner(void){return menu_device;}
__declspec(dllexport) void MenuTestToggle(void) {PostThreadMessageW(ui_thread,WM_MENU_TOGGLE,0,0);}
__declspec(dllexport) int MenuTestSnapshot(void) {
    EnterCriticalSection(&lock);
    int result=menu.selection|(menu.playstation_icons<<4)|(menu.visible<<8)|(menu.page<<12)|(menu.capturing<<16);
    LeaveCriticalSection(&lock);return result;
}
__declspec(dllexport) uint32_t MenuTestShortcut(void){EnterCriticalSection(&lock);uint32_t result=menu_shortcut(&menu);LeaveCriticalSection(&lock);return result;}
__declspec(dllexport) void MenuTestScreenshot(void){request_screenshot();}
__declspec(dllexport) int MenuTestShutdownRemaining(void){EnterCriticalSection(&lock);int seconds=menu.shutdown_countdown?menu.shutdown_seconds:0;LeaveCriticalSection(&lock);return seconds;}
__declspec(dllexport) int MenuTestGuideEnabled(void){EnterCriticalSection(&lock);int value=menu_guide_enabled(&menu);LeaveCriticalSection(&lock);return value;}
__declspec(dllexport) int MenuTestAutoLaunch(void){EnterCriticalSection(&lock);int value=menu_auto_launch(&menu);LeaveCriticalSection(&lock);return value;}
__declspec(dllexport) int MenuTestScale(void){EnterCriticalSection(&lock);int scale=menu_scale(&menu);LeaveCriticalSection(&lock);return scale;}
#endif
#ifndef QUIT_MENU_TEST
#include "overlay_bootstrap.h"
#endif
BOOL WINAPI DllMain(HINSTANCE dll, DWORD reason, LPVOID reserved) {
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        instance = dll;
        InitializeCriticalSection(&lock);
        DisableThreadLibraryCalls(dll);
#ifndef QUIT_MENU_TEST
        overlay_bootstrap(dll);
        HANDLE h = CreateThread(NULL,0,worker,NULL,0,NULL);
        if (h) CloseHandle(h);
#endif
    }
    return TRUE;
}
