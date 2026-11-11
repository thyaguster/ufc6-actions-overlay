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
#include <shellapi.h>
#include <objbase.h>
#include "exact_hooks.h"
#include "menu_state.h"
#include "restart_client.h"
#include "xinput_menu.h"
#include "gamebar_shortcut.h"
#include "gamebar_monitor.h"
#include "launcher_dialog.h"
#include "launch_config.h"
#include "screenshot_folder.h"
#include "live_cheats.h"
#include "career_cheats.h"
#include "game_icons.h"
#include "save_backups.h"
#include "overlay_version.h"
#ifndef QUIT_MENU_TEST
#include "engine_fingerprint.h"
#endif

#define WM_MENU_ACTION (WM_APP + 40)
#define WM_MENU_TOGGLE (WM_APP + 41)
#define WM_SCREENSHOT_READY (WM_APP + 42)
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
static HFONT font_title, font_body, font_small, font_main, font_settings;
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
static int test_folder_requests;
static int test_game_icons=GAME_ICONS_PS,test_game_icon_applies,test_text_clips;
#endif
static XINPUT_STATE raw_states[4];
static int raw_connected[4],guide_source=-1,raw_owner=-1,raw_navigation;
static uint32_t raw_previous;
static uint32_t raw_idle_previous[4];
static volatile LONG vulkan_active;
static volatile LONG screenshot_request,screenshot_busy,screenshot_capable;
static wchar_t status_text[120];
static wchar_t last_screenshot_name[128];
static PWSTR last_screenshot_path;
static uint64_t status_until;
static LiveCheats live_cheats;
static CareerSnapshot career;
static int live_status[LIVE_COUNT],live_enabled[LIVE_COUNT];
static uint64_t next_live_check;
#ifdef QUIT_MENU_TEST
static int test_live_status[LIVE_COUNT]={LIVE_OFF,LIVE_OFF};
static int test_live_requested[LIVE_COUNT];
#endif
static uint32_t render_width=MENU_BASE_WIDTH,render_height=MENU_BASE_HEIGHT;

static void status(const wchar_t *message) {
    EnterCriticalSection(&lock);lstrcpynW(status_text,message,120);status_until=GetTickCount64()+5000;LeaveCriticalSection(&lock);
}

static int foreground(void);
static void log_line(const char *line);
static void refresh_live_cheats(void){
    EnterCriticalSection(&lock);
#ifdef QUIT_MENU_TEST
    if(test_mode){memcpy(live_status,test_live_status,sizeof(live_status));for(int i=0;i<LIVE_COUNT;++i)live_enabled[i]=test_live_requested[i]||test_live_status[i]==LIVE_LAUNCHER||test_live_status[i]==LIVE_GAME;LeaveCriticalSection(&lock);return;}
#endif
    live_cheats_tick(&live_cheats);
    CareerSnapshot before=career;career_cheats_tick();career_cheats_snapshot(&career);
    if(career.ready&&(!before.ready||before.balance[0]!=career.balance[0]||before.balance[1]!=career.balance[1])){
        char line[120];snprintf(line,sizeof(line),"Career balances verified: money=%u skill_points=%u.",career.balance[0],career.balance[1]);log_line(line);
    }
    if(career.ready&&(!before.ready||before.sparring_grades_ready!=career.sparring_grades_ready)){
        char line[120];snprintf(line,sizeof(line),"Sparring grade binding: verified=%d upgradable=%u.",career.sparring_grades_ready,career.sparring_grades_completed);log_line(line);
    }
    if(career.longevity_blocked>before.longevity_blocked)log_line("Career longevity protection blocked a decrease.");
    if(before.status==CAREER_PENDING&&career.status!=CAREER_PENDING){
        status(career.status==CAREER_ADDED?L"Career change applied once. The game saves normally.":career.status==CAREER_LIMIT?L"Addition cancelled: the game's balance limit was reached.":L"Addition cancelled: career changed or refresh timed out.");
        log_line(career.status==CAREER_ADDED?"Career change applied once.":"Pending career addition cancelled or rejected.");
    }
    for(int i=0;i<LIVE_COUNT;++i){live_status[i]=live_cheats.flags[i].status;live_enabled[i]=live_cheats.flags[i].requested||live_status[i]==LIVE_LAUNCHER||live_status[i]==LIVE_GAME;}
    LeaveCriticalSection(&lock);
}
static void change_live_cheat(int which){
    if(!test_mode&&!foreground())return;
    EnterCriticalSection(&lock);
#ifdef QUIT_MENU_TEST
    if(test_mode){if(test_live_status[which]!=LIVE_LAUNCHER&&test_live_status[which]!=LIVE_UNSUPPORTED){test_live_requested[which]=!(test_live_requested[which]||test_live_status[which]==LIVE_GAME);if(test_live_status[which]!=LIVE_WAIT)test_live_status[which]=test_live_requested[which]?LIVE_ON:LIVE_OFF;}}
    else
#endif
    live_cheats_toggle(&live_cheats,which);
    LeaveCriticalSection(&lock);refresh_live_cheats();
    EnterCriticalSection(&lock);int value=live_status[which],enabled=live_enabled[which];LeaveCriticalSection(&lock);
    status((value==LIVE_ON||value==LIVE_WAIT)&&enabled?(which==LIVE_STAMINA?L"Infinite stamina enabled for Player 1.":L"No damage enabled for Player 1."):
        (value==LIVE_OFF||value==LIVE_RESTORING||value==LIVE_WAIT)&&!enabled?L"Cheat disabled for Player 1.":value==LIVE_LAUNCHER?L"Disable this cheat in PORTHEON, then restart the game.":
        value==LIVE_GAME?L"The game/practice settings still enable this flag.":value==LIVE_RESTORING?L"Waiting to disable the controlled fighter flags.":value==LIVE_WAIT?L"Waiting for the fight binding update.":L"Cheat unavailable: loaded game data could not be verified.");
}
static void add_career_value(void){
    if(!test_mode&&!foreground())return;
    EnterCriticalSection(&lock);int kind=menu.career_kind;uint32_t amount=kind==CAREER_GRADES?0:kind==CAREER_DAYS?7:kind==CAREER_FITNESS?10000:menu_career_amount(&menu,kind);int ok=0;
#ifdef QUIT_MENU_TEST
    if(test_mode){if(kind==CAREER_GRADES&&career.sparring_grades_ready){career.sparring_grades_completed=0;career.status=CAREER_ADDED;ok=1;}else if(kind==CAREER_DAYS&&career.training_ready){career.training_days=7;career.status=CAREER_ADDED;ok=1;}else if(kind==CAREER_FITNESS&&career.fitness_ready){career.fitness=10000;career.status=CAREER_ADDED;ok=1;}else if(kind<2&&career.ready&&amount<=career.maximum[kind]-career.balance[kind]){career.balance[kind]+=amount;career.status=CAREER_ADDED;ok=1;}}
    else
#endif
    {ok=kind==CAREER_GRADES?career_cheats_grade_a():kind==CAREER_DAYS?career_cheats_restore_days():kind==CAREER_FITNESS?career_cheats_restore_fitness():career_cheats_add(kind,amount);career_cheats_snapshot(&career);}
    int result=career.status,grades_ready=career.sparring_grades_ready;uint32_t grade_count=career.sparring_grades_completed;LeaveCriticalSection(&lock);
    if(kind==CAREER_GRADES&&!test_mode){char line[140];snprintf(line,sizeof(line),"Grade A request: queued=%d verified=%d upgradable=%u status=%d.",ok,grades_ready,grade_count,result);log_line(line);}
    status(ok?(test_mode?L"Career test addition applied.":L"Addition queued. Resume and reopen a career screen to apply."):
        result==CAREER_LIMIT?L"This amount exceeds the game's balance limit.":result==CAREER_PENDING?L"An addition is already pending. Resume the game to apply it.":kind==CAREER_GRADES?(grades_ready?L"All sparring grades are already A.":L"Sparring grades unavailable: career data could not be verified."):L"Open the career hub first, then try again.");
}
static void format_number(uint32_t value,wchar_t *out){
    wchar_t digits[16];swprintf(digits,16,L"%u",value);size_t n=wcslen(digits),j=0;
    for(size_t i=0;i<n;++i){out[j++]=digits[i];if(i+1<n&&(n-i-1)%3==0)out[j++]=L',';}out[j]=0;
}
static void stop_live_cheats(void){
    EnterCriticalSection(&lock);
#ifdef QUIT_MENU_TEST
    if(test_mode){career.longevity_enabled=0;career.longevity_requested=0;for(int i=0;i<LIVE_COUNT;++i){test_live_requested[i]=0;if(test_live_status[i]==LIVE_ON)test_live_status[i]=LIVE_OFF;}}
    else
#endif
    {live_cheats_stop(&live_cheats);career_cheats_cancel();}
    LeaveCriticalSection(&lock);refresh_live_cheats();
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
    if(!(flags&DT_WORDBREAK))flags|=DT_SINGLELINE;
#ifdef QUIT_MENU_TEST
    if(test_mode&&(flags&DT_SINGLELINE)){TEXTMETRICW metrics;if(GetTextMetricsW(dc,&metrics)&&metrics.tmHeight>r.bottom-r.top){++test_text_clips;char diagnostic[200];snprintf(diagnostic,sizeof(diagnostic),"Text height overflow: %ls height=%ld box=%ld top=%ld",str,metrics.tmHeight,r.bottom-r.top,r.top);log_line(diagnostic);}}
#endif
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
    EnterCriticalSection(&lock); int training_setting=menu_training_multiplier(&menu),longevity_setting=career.longevity_requested,ps=menu.playstation_icons,scale=menu_scale(&menu),auto_launch=menu_auto_launch(&menu),guide_enabled=menu_guide_enabled(&menu),first_only=menu.first_player_only;uint32_t chord=menu_shortcut(&menu); LeaveCriticalSection(&lock);
    wchar_t value[24];swprintf(value,24,L"0x%08lx",(unsigned long)chord);
    BOOL saved=WritePrivateProfileStringW(L"Menu",L"ButtonIcons",ps?L"PlayStation":L"Xbox",settings_path);
    saved=WritePrivateProfileStringW(L"Menu",L"OpeningShortcut",value,settings_path)&&saved;
    swprintf(value,24,L"%d",scale);
    saved=WritePrivateProfileStringW(L"Menu",L"UIScale",value,settings_path)&&saved;
    saved=WritePrivateProfileStringW(L"Menu",L"AutoLaunchAfterRestart",auto_launch?L"1":L"0",settings_path)&&saved;
    saved=WritePrivateProfileStringW(L"Menu",L"GuideButtonOpensMenu",guide_enabled?L"1":L"0",settings_path)&&saved;
    saved=WritePrivateProfileStringW(L"Menu",L"ControllerAccess",first_only?L"Player1":L"Either",settings_path)&&saved;
    saved=WritePrivateProfileStringW(L"Menu",L"LongevityProtection",longevity_setting?L"1":L"0",settings_path)&&saved;
    swprintf(value,24,L"%d",training_setting);saved=WritePrivateProfileStringW(L"Menu",L"TrainingRewardMultiplier",value,settings_path)&&saved;
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
static void rounded_box(HDC dc,RECT box,int radius,COLORREF fill,COLORREF edge){
    HBRUSH brush=CreateSolidBrush(fill);HPEN pen=CreatePen(PS_SOLID,1,edge);
    HGDIOBJ old_brush=SelectObject(dc,brush),old_pen=SelectObject(dc,pen);
    RoundRect(dc,box.left,box.top,box.right,box.bottom,radius,radius);
    SelectObject(dc,old_pen);SelectObject(dc,old_brush);DeleteObject(pen);DeleteObject(brush);
}
static void arrow_mark(HDC dc,int x,int y,int direction,COLORREF color){
    HPEN pen=CreatePen(PS_SOLID,1,color);HGDIOBJ old=SelectObject(dc,pen);
    MoveToEx(dc,x-direction*3,y-4,NULL);LineTo(dc,x+direction*1,y);LineTo(dc,x-direction*3,y+4);
    SelectObject(dc,old);DeleteObject(pen);
}
static void toggle_switch(HDC dc,RECT row,int on,int selected){
    int y=(row.top+row.bottom)/2;RECT track={row.right-54,y-9,row.right-14,y+10};
    rounded_box(dc,track,20,on?RGB(95,202,121):RGB(67,78,94),on?RGB(116,222,140):RGB(88,101,119));
    int x=on?track.right-17:track.left+3;RECT thumb={x,y-6,x+14,y+8};
    rounded_box(dc,thumb,14,on?RGB(17,49,28):RGB(222,229,240),on?RGB(17,49,28):RGB(222,229,240));
    RECT label={row.right-115,row.top,row.right-64,row.bottom};
    text(dc,font_small,on?L"On":L"Off",label,selected?RGB(235,247,239):on?RGB(154,222,173):RGB(157,170,188),DT_RIGHT|DT_VCENTER|DT_SINGLELINE);
}
static void value_chip(HDC dc,RECT row,const wchar_t *label,int selected,int adjustable){
    SIZE extent={0};HFONT old=SelectObject(dc,font_small);GetTextExtentPoint32W(dc,label,(int)wcslen(label),&extent);SelectObject(dc,old);
    int width=extent.cx+(adjustable?46:24);if(width<64)width=64;if(width>226)width=226;
    int y=(row.top+row.bottom)/2;RECT box={row.right-14-width,y-12,row.right-14,y+12};
    rounded_box(dc,box,10,selected?RGB(26,43,37):RGB(28,36,48),selected?RGB(77,112,88):RGB(54,67,83));
    RECT label_box=box;label_box.left+=adjustable?19:10;label_box.right-=adjustable?19:10;
    text(dc,font_small,label,label_box,selected?RGB(222,245,228):RGB(192,207,225),DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
    if(adjustable){arrow_mark(dc,box.left+10,y,-1,RGB(144,170,154));arrow_mark(dc,box.right-10,y,1,RGB(144,170,154));}
}
static void row_geometry(int page,int index,int first,int *top,int *height) {
    int global_index=index;if(page==PAGE_SETTINGS||page==PAGE_CHEATS)index-=first;
    *top=page==PAGE_CHEATS?84+32*index+(first<=2&&global_index>=2?24:0):page==PAGE_MAIN?84+27*index+(index>=MAIN_REBOOT?10:0):page==PAGE_SETTINGS?86+32*index:page==PAGE_BACKUPS?104+40*index:(page==PAGE_SCREENSHOT||page==PAGE_RESET||page==PAGE_CAREER_ADD||page==PAGE_GAME_ICONS_CONFIRM)?182+48*index:100+56*index;
    *height=page==PAGE_BACKUPS?34:page==PAGE_CHEATS?30:page==PAGE_MAIN?25:page==PAGE_SETTINGS?30:(page==PAGE_SCREENSHOT||page==PAGE_RESET||page==PAGE_CAREER_ADD||page==PAGE_GAME_ICONS_CONFIRM)?42:50;
}
static void draw_menu(HDC dc, RECT r) {
    HBRUSH bg = CreateSolidBrush(RGB(17, 22, 30));
    FillRect(dc, &r, bg); DeleteObject(bg);
    SetBkMode(dc, TRANSPARENT);
    CareerSnapshot career_state;MenuState state;int save_failed,conflict,owner_player=0,has_owner=0;int cheat_states[LIVE_COUNT],cheat_enabled[LIVE_COUNT];wchar_t message[120],filename[128];
    EnterCriticalSection(&lock);
    memcpy(cheat_states,live_status,sizeof(cheat_states));memcpy(cheat_enabled,live_enabled,sizeof(cheat_enabled));
    career_state=career;state=menu;save_failed=settings_save_failed;conflict=gamebar_conflict;
    lstrcpynW(filename,last_screenshot_name,128);
    if(raw_navigation&&raw_owner>=0){has_owner=1;owner_player=raw_owner+1;}
    else if(menu_device>=0&&devices[menu_device].used){has_owner=1;owner_player=native_player(&devices[menu_device]);}
    lstrcpynW(message,GetTickCount64()<status_until?status_text:L"",120);
    LeaveCriticalSection(&lock);
    int selection=state.selection,ps=state.playstation_icons;wchar_t icon_reason[512];int icon_state;
#ifdef QUIT_MENU_TEST
    if(test_mode){icon_state=test_game_icons;wcscpy(icon_reason,L"Test game icons; no game files are changed.");}else
#endif
    icon_state=game_icons_status(icon_reason,512);
    RECT title = {36, 10, r.right - 36, 57};
    text(dc,font_title,state.page==PAGE_BACKUPS?L"Savegame backups":state.page==PAGE_GAME_ICONS?L"Controller icon patch":state.page==PAGE_GAME_ICONS_CONFIRM?(state.game_icons_xbox?L"Apply Xbox icons?":L"Restore PS icons?"):state.page==PAGE_CAREER_ADD?(state.career_kind==CAREER_GRADES?L"Set all grades to A?":state.career_kind==CAREER_DAYS?L"Refill training days?":state.career_kind==CAREER_FITNESS?L"Restore fitness?":state.career_kind==0?L"Add money?":L"Add skill points?"):state.page==PAGE_CHEATS?L"Cheats":state.page==PAGE_SETTINGS?L"Settings":state.page==PAGE_SCREENSHOT?L"Screenshot saved":state.page==PAGE_RESET?L"Reset settings?":state.page==PAGE_RESTART?L"Restart game?":menu_power_page(&state)?(state.shutdown_countdown?(state.page==PAGE_REBOOT?L"Restarting PC":L"Shutting down PC"):(state.page==PAGE_REBOOT?L"Restart PC?":L"Shut down PC?")):L"UFC 6",title,RGB(246,247,250),DT_LEFT|DT_SINGLELINE);
    wchar_t owner_label[64];
    if(owner_player>0)swprintf(owner_label,64,L"Player %d",owner_player);
    else wcscpy(owner_label,has_owner?L"Controller":state.first_player_only?L"Waiting for Player 1":L"Waiting for controller");
    RECT owner_rect={330,25,r.right-36,48};text(dc,font_small,owner_label,owner_rect,RGB(176,184,197),DT_RIGHT|DT_SINGLELINE);
    RECT sub = {38, 57, r.right - 36, 84};
    text(dc,font_small,conflict?L"Game Bar is using your controller.":state.page==PAGE_BACKUPS?L"Save in UFC 6 first; wait for autosave to finish.":state.page==PAGE_GAME_ICONS?L"Replace in-game button icons with Xbox or PlayStation.":state.page==PAGE_GAME_ICONS_CONFIRM?L"Your game will close and restart through your launcher.":state.page==PAGE_CAREER_ADD?L"Career changes are included in normal game saves.":state.page==PAGE_CHEATS?L"Stamina and health apply to Player 1.":state.page==PAGE_SCREENSHOT?L"Clean game frame saved. The overlay is excluded.":state.page==PAGE_RESET?L"Restore overlay and launcher defaults?":state.page==PAGE_SETTINGS?L"Customize your overlay":state.page==PAGE_RESTART?L"Close this session and use the saved launcher settings.":menu_power_page(&state)?(state.shutdown_countdown?L"You can still cancel before the countdown ends.":(state.page==PAGE_REBOOT?(menu_auto_launch(&state)?L"Game auto-launch: On. Change in Settings.":L"Game auto-launch: Off. Change in Settings."):L"Close the game and turn off this PC.")):L"What would you like to do?",sub,conflict?RGB(255,196,94):RGB(176,184,197),DT_LEFT);
    const wchar_t *main_labels[]={L"Resume",L"Take screenshot",L"Restart game",L"Cheats",L"Settings",L"Quit game",L"Restart PC",L"Shut down PC"};
    const wchar_t *setting_labels[]={L"Overlay button icons",L"Opening shortcut",L"UI scale",L"Launch UFC 6 after PC restart",L"Xbox / Guide button",L"Windows Game Bar shortcut",L"Launcher setup...",L"Controller access",L"Screenshots folder",L"Reset overlay settings...",L"Patch in-game controller icons...",L"Check for updates...",L"Export support report...",L"Back"};
    const wchar_t *screenshot_labels[]={L"Open screenshots folder",L"Back"};
    const wchar_t *cheat_labels[]={L"Infinite stamina",L"No damage",L"Add money",L"Add skill points",L"Restore fitness to 100%",L"Refill training days",L"Never get old",L"Training point rewards",L"All sparring grades: A...",L"Savegame backups...",L"Back"};
    wchar_t backup_message[512];int backup_state=BACKUP_READY;
#ifdef QUIT_MENU_TEST
    if(test_mode)wcscpy(backup_message,L"Test backup page; save files are never touched.");else
#endif
    backup_state=save_backups_state(backup_message,512);
    const wchar_t *backup_labels[]={backup_state==BACKUP_BUSY?L"Creating backup...":L"Create savegame backup",L"Open backups folder",L"Back"};
    const wchar_t *game_icons_labels[]={L"Xbox game icons",L"PlayStation game icons",L"Back"};
    const wchar_t *game_icons_confirm_labels[]={L"Cancel",L"Apply and restart game"};
    const wchar_t *career_labels[]={L"Cancel",state.career_kind==CAREER_GRADES?L"Set all grades to A":state.career_kind==CAREER_DAYS?L"Refill now":state.career_kind==CAREER_FITNESS?L"Restore now":L"Add now"};
    const wchar_t *reset_labels[]={L"Cancel",L"Reset now"};
    const wchar_t *restart_labels[]={L"Cancel",L"Restart now"};
    const wchar_t *shutdown_labels[]={state.shutdown_countdown?L"Cancel":L"Cancel",state.page==PAGE_REBOOT?L"Hold to restart PC":L"Hold to shut down"};
    int first=menu_first_row(&state);
    for(int i=0;i<menu_rows(&state);++i) {
        if((state.page==PAGE_SETTINGS||state.page==PAGE_CHEATS)&&(i<first||i>=first+6))continue;
        int top,height;row_geometry(state.page,i,first,&top,&height);
        RECT row={36,top,r.right-36,top+height};
        int focused=i==selection;
        int danger=(state.page==PAGE_MAIN&&(i==MAIN_QUIT||i==MAIN_REBOOT||i==MAIN_SHUTDOWN))||(menu_power_page(&state)&&i==1);
        RECT surface=row;
        rounded_box(dc,surface,10,focused?(danger?RGB(54,35,41):RGB(31,54,43)):RGB(29,37,49),focused?(danger?RGB(173,98,106):RGB(89,169,114)):RGB(42,53,68));
        if(focused){RECT accent={37,top+6,40,top+height-6};HBRUSH mark=CreateSolidBrush(danger?RGB(245,143,150):RGB(118,224,148));FillRect(dc,&accent,mark);DeleteObject(mark);}
        COLORREF color=focused?RGB(244,250,246):danger?RGB(227,159,165):RGB(218,228,240);
        RECT control=row;
        row.left+=18;
        RECT label_row=row;
        if(state.page==PAGE_MAIN&&i==MAIN_REBOOT)label_row.right=330;
        if(state.page==PAGE_SETTINGS&&i==SETTING_SHORTCUT)label_row.right=290;
        else if(state.page==PAGE_SETTINGS&&(i==SETTING_ICONS||i==SETTING_SCALE||i==SETTING_ACCESS))label_row.right=345;
        else if((state.page==PAGE_SETTINGS&&(i==SETTING_AUTO_LAUNCH||i==SETTING_GUIDE||i==SETTING_GAMEBAR))||(state.page==PAGE_CHEATS&&(i<2||i==6)))label_row.right=410;
        else if(state.page==PAGE_CHEATS&&(i==2||i==3||i==7))label_row.right=300;
        text(dc,(state.page==PAGE_MAIN||state.page==PAGE_CHEATS)?font_main:state.page==PAGE_SETTINGS?font_settings:font_body,state.page==PAGE_BACKUPS?backup_labels[i]:state.page==PAGE_GAME_ICONS?game_icons_labels[i]:state.page==PAGE_GAME_ICONS_CONFIRM?game_icons_confirm_labels[i]:state.page==PAGE_CHEATS?cheat_labels[i]:state.page==PAGE_MAIN?main_labels[i]:state.page==PAGE_SETTINGS?setting_labels[i]:state.page==PAGE_SCREENSHOT?screenshot_labels[i]:state.page==PAGE_RESET?reset_labels[i]:state.page==PAGE_CAREER_ADD?career_labels[i]:menu_power_page(&state)?shutdown_labels[i]:restart_labels[i],label_row,color,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
        if(state.page==PAGE_CHEATS&&i==7){wchar_t factor[32];int multiplier=menu_training_multiplier(&state);swprintf(factor,32,multiplier==1?L"1x (Off)":L"%dx",multiplier);value_chip(dc,control,factor,focused,1);}
        if(state.page==PAGE_CHEATS&&i==6)toggle_switch(dc,control,career_state.longevity_requested,focused);
        if(state.page==PAGE_CHEATS&&i<2)toggle_switch(dc,control,cheat_enabled[i],focused);
        if(state.page==PAGE_CHEATS&&(i==2||i==3)){
            wchar_t amount[32],value_text[64];format_number(menu_career_amount(&state,i-2),amount);
            int pending=career_state.status==CAREER_PENDING&&career_state.kind==i-2;
            swprintf(value_text,64,pending?L"Pending...":i==2?L"$%ls":L"%ls",amount);
            value_chip(dc,control,value_text,focused,!pending);
        }
        if(menu_power_page(&state)&&i==1&&state.shutdown_holding){
            wchar_t progress[40];swprintf(progress,40,L"%.1f / 3 s",state.shutdown_progress/1000.0);
            RECT value={330,row.top+5,r.right-55,row.bottom};text(dc,font_small,progress,value,color,DT_RIGHT|DT_SINGLELINE);
            RECT bar={36,top+height-4,36+(r.right-72)*state.shutdown_progress/3000,top+height};
            HBRUSH fill_bar=CreateSolidBrush(RGB(221,246,197));FillRect(dc,&bar,fill_bar);DeleteObject(fill_bar);
        }
        if(state.page==PAGE_MAIN&&i==MAIN_REBOOT){
            int on=menu_auto_launch(&state),middle=(control.top+control.bottom)/2;
            RECT dot={control.right-145,middle-3,control.right-139,middle+3};
            rounded_box(dc,dot,6,on?RGB(113,211,141):RGB(128,141,157),on?RGB(113,211,141):RGB(128,141,157));
            RECT value={control.right-134,control.top,control.right-14,control.bottom};
            text(dc,font_small,on?L"Auto-launch: On":L"Auto-launch: Off",value,focused?RGB(229,234,242):RGB(168,184,202),DT_RIGHT|DT_VCENTER|DT_SINGLELINE);
        }
        if(state.page==PAGE_SETTINGS&&i==SETTING_AUTO_LAUNCH)toggle_switch(dc,control,menu_auto_launch(&state),focused);
        if(state.page==PAGE_SETTINGS&&(i==SETTING_GUIDE||i==SETTING_GAMEBAR)){
            if(i==SETTING_GAMEBAR&&state.gamebar_shortcut<0)value_chip(dc,control,L"Unavailable",focused,0);
            else toggle_switch(dc,control,i==SETTING_GUIDE?menu_guide_enabled(&state):state.gamebar_shortcut,focused);
        }
        if(state.page==PAGE_SETTINGS&&i==SETTING_ACCESS)value_chip(dc,control,state.first_player_only?L"Player 1 only":L"Either",focused,1);
        if(state.page==PAGE_SETTINGS&&i==SETTING_ICONS)value_chip(dc,control,ps?L"PlayStation":L"Xbox",focused,1);
        if(state.page==PAGE_SETTINGS&&i==SETTING_SHORTCUT){wchar_t chord[100];shortcut_label(menu_shortcut(&state),ps,chord,100);value_chip(dc,control,state.capturing?L"Recording...":chord,focused,0);}
        if(state.page==PAGE_SETTINGS&&i==SETTING_SCALE){wchar_t scale[40];swprintf(scale,40,L"%d%%",menu_scale(&state));value_chip(dc,control,scale,focused,1);}
        if(state.page==PAGE_GAME_ICONS&&((i==0&&icon_state==GAME_ICONS_XBOX)||(i==1&&icon_state==GAME_ICONS_PS)))value_chip(dc,control,L"Active",focused,0);
        int simple=(state.page==PAGE_MAIN&&i!=MAIN_REBOOT)||(state.page==PAGE_SETTINGS&&(i==SETTING_LAUNCHER||i==SETTING_SCREENSHOTS||i==SETTING_RESET||i==SETTING_GAME_ICONS||i==SETTING_UPDATE||i==SETTING_DIAGNOSTICS||i==SETTING_BACK))||(state.page==PAGE_CHEATS&&i>=4&&i!=6&&i!=7)||state.page==PAGE_BACKUPS;
        if(simple)arrow_mark(dc,control.right-21,(control.top+control.bottom)/2,1,focused?RGB(178,223,191):RGB(112,132,155));
    }
    if(state.page==PAGE_CHEATS&&first<=2){RECT heading={38,148-32*first,r.right-36,172-32*first};text(dc,font_small,L"Career cheats",heading,RGB(176,184,197),DT_LEFT|DT_SINGLELINE);}
    if(state.page==PAGE_GAME_ICONS_CONFIRM){
        RECT scope={36,98,r.right-36,126},backup={36,133,r.right-36,159};
        text(dc,font_small,state.game_icons_xbox?L"Use Xbox prompts throughout the game.":L"Restore the original PlayStation game prompts.",scope,RGB(234,238,244),DT_CENTER|DT_SINGLELINE);
        text(dc,font_small,state.game_icons_xbox?L"Original artwork is backed up before any changes.":L"Uses your verified local restore backup.",backup,RGB(176,184,197),DT_CENTER|DT_SINGLELINE);
    }
    if(state.page==PAGE_CAREER_ADD){
        wchar_t amount[32],balance[32],line[100];int kind=state.career_kind;
        format_number(kind==CAREER_GRADES?0:kind==CAREER_DAYS?7:kind==CAREER_FITNESS?100:menu_career_amount(&state,kind),amount);format_number(kind==CAREER_GRADES?0:kind==CAREER_DAYS?career_state.training_days:kind==CAREER_FITNESS?career_state.fitness/100:career_state.balance[kind],balance);
        swprintf(line,100,kind==CAREER_GRADES?L"Set every sparring challenge pack to Grade A":kind==CAREER_DAYS?L"Restore 7 days in this training week":kind==CAREER_FITNESS?L"Set this career fighter to 100%% fitness":kind==0?L"Add $%ls to this career":L"Add %ls skill points to this career",amount);
        RECT label={36,94,r.right-36,128},current={36,133,r.right-36,159};text(dc,font_body,line,label,RGB(234,238,244),DT_CENTER|DT_SINGLELINE);
        swprintf(line,100,(kind==CAREER_DAYS?career_state.training_ready:kind==CAREER_FITNESS?career_state.fitness_ready:career_state.ready)?(kind==CAREER_FITNESS?L"Current fitness: %ls%%. Applies once; save normally.":kind==0?L"Current balance: $%ls":L"Current balance: %ls skill points"):L"Open the career hub to load your values.",balance);
        if(kind==CAREER_DAYS)swprintf(line,100,career_state.training_ready?L"Days remaining: %u. Week %u. Applies once.":L"Open an active training camp to refill days.",career_state.training_days,career_state.training_week);
        if(kind==CAREER_GRADES)swprintf(line,100,L"Includes unattempted packs. Save normally.");
        text(dc,font_small,line,current,RGB(176,184,197),DT_CENTER|DT_SINGLELINE);
    }else if(state.page==PAGE_SCREENSHOT){
        RECT label={36,94,r.right-36,117},name={36,121,r.right-36,161};
        text(dc,font_small,L"Saved file",label,RGB(176,184,197),DT_CENTER|DT_SINGLELINE);
        text(dc,font_small,filename,name,RGB(234,238,244),DT_CENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
    }else if(state.page==PAGE_RESET){
        RECT scope={36,97,r.right-36,123},keep={36,130,r.right-36,159};
        text(dc,font_small,L"Restores overlay preferences and launcher settings.",scope,RGB(234,238,244),DT_CENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
        text(dc,font_small,L"Windows Game Bar preference and screenshots are kept.",keep,RGB(176,184,197),DT_CENTER|DT_SINGLELINE);
    }
    if(state.page==PAGE_SETTINGS||state.page==PAGE_CHEATS){
        COLORREF color=RGB(176,184,197);HBRUSH fill=CreateSolidBrush(color);HPEN pen=CreatePen(PS_SOLID,1,color);
        HGDIOBJ old_brush=SelectObject(dc,fill),old_pen=SelectObject(dc,pen);
        POINT above[]={{543,103},{555,103},{549,94}},below[]={{543,252},{555,252},{549,261}};
        if(first>0)Polygon(dc,above,3);
        if(first+6<menu_rows(&state))Polygon(dc,below,3);
        SelectObject(dc,old_brush);SelectObject(dc,old_pen);DeleteObject(fill);DeleteObject(pen);
    }
    if(menu_power_page(&state)&&state.shutdown_countdown){
        wchar_t countdown[40];swprintf(countdown,40,L"%d seconds",state.shutdown_seconds);
        RECT number={36,187,r.right-36,239};text(dc,font_title,countdown,number,RGB(246,247,250),DT_CENTER|DT_SINGLELINE);
    }
    int y=state.page==PAGE_MAIN?314:state.page==PAGE_CHEATS?310:279;
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
        const wchar_t *confirm=state.page==PAGE_CHEATS?((selection<2||selection==6)?L"Change":selection==7?L"Change":selection<4?L"Add...":L"Confirm"):state.page==PAGE_SETTINGS?(selection==SETTING_SHORTCUT?L"Record":(selection==SETTING_ICONS||selection==SETTING_AUTO_LAUNCH||selection==SETTING_GUIDE||selection==SETTING_GAMEBAR||selection==SETTING_ACCESS)?L"Change":selection==SETTING_SCALE?L"Reset":L"Confirm"):menu_power_page(&state)?(state.shutdown_countdown?L"Cancel":selection==1?L"Hold 3s":L"Confirm"):L"Confirm";
        RECT yes={250,y+5,367,y+28};text(dc,font_small,confirm,yes,RGB(187,196,209),DT_LEFT);
        button_icon(dc,386,y,1,ps);
        RECT back={426,y+5,530,y+28};text(dc,font_small,state.page==PAGE_MAIN?L"Resume":L"Back",back,RGB(187,196,209),DT_LEFT);
        if(state.page==PAGE_SETTINGS) {
            wchar_t chord[100],line[130];shortcut_label(menu_shortcut(&state),ps,chord,100);
            swprintf(line,130,L"Open: %ls  |  F10",chord);
            if(selection==SETTING_GAME_ICONS)wcscpy(line,L"Patch the game controller icons. Restart required.");
            if(selection==SETTING_ICONS)wcscpy(line,L"Changes this overlay only. Game icons have a separate setting.");
            if(selection==SETTING_SCALE)wcscpy(line,L"Left/right: resize   |   Confirm: reset to 100%");
            if(selection==SETTING_AUTO_LAUNCH)wcscpy(line,L"Reopen UFC 6 after PC restart and Windows sign-in.");
            if(selection==SETTING_GUIDE)wcscpy(line,L"Turn Game Bar shortcut Off if Guide navigation stalls.");
            if(selection==SETTING_GAMEBAR)wcscpy(line,L"Windows setting for this user. Win+G still opens Game Bar.");
            if(selection==SETTING_DIAGNOSTICS)wcscpy(line,L"Create a local ZIP with logs and component checks.");
            if(selection==SETTING_UPDATE)wcscpy(line,L"Opens the desktop updater. Mouse/keyboard required.");
            if(selection==SETTING_LAUNCHER)wcscpy(line,L"Opens desktop launcher settings. Mouse/keyboard required.");
            if(selection==SETTING_ACCESS)wcscpy(line,L"Player 1 only, or either controller. F10 always works.");
            if(selection==SETTING_SCREENSHOTS)wcscpy(line,L"Open the Windows screenshots folder on the desktop.");
            if(selection==SETTING_RESET)wcscpy(line,L"Restore overlay defaults. Windows Game Bar is kept.");
            RECT shortcut={30,314,r.right-30,338};text(dc,font_small,line,shortcut,RGB(158,170,188),DT_CENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
        }
    }
    if(state.page==PAGE_CHEATS&&!conflict){
        wchar_t balances[120],money[32],points[32];format_number(career_state.balance[0],money);format_number(career_state.balance[1],points);
        swprintf(balances,120,career_state.ready?L"Money: $%ls  |  Skill points: %ls":L"Open a career screen to load money and skill points.",money,points);
        const wchar_t *help=selection>=2&&selection<4?balances:selection<2&&cheat_states[selection]==LIVE_LAUNCHER?L"Disable in PORTHEON, then restart once to use live control.":selection<2&&(cheat_states[selection]==LIVE_UNSUPPORTED||cheat_states[selection]==LIVE_ERROR)?L"This game version could not be verified for live cheats.":L"Player 1 follows the controller, even when fighters move.";
        if(!message[0])lstrcpynW(message,selection==4?L"Set career fitness to 100% once. Save normally afterward.":selection==5?L"Restore 7 training days once, without advancing the week.":selection==6?L"Saved choice. Activates when a valid career loads.":selection==7?L"Multiplies sparring challenge evolution-point rewards.":selection==8?L"Upgrade completed sparring records to A for simulation.":selection==9?L"Back up your saved progress before using career cheats.":help,120);
    }
    if(state.page==PAGE_BACKUPS&&!conflict){
        RECT info={38,226,r.right-38,274};
        text(dc,font_small,backup_message,info,backup_state==BACKUP_ERROR?RGB(244,157,160):RGB(176,184,197),DT_CENTER|DT_WORDBREAK);
        RECT hint={20,314,r.right-20,338};text(dc,font_small,L"Unsaved progress is not included.",hint,RGB(158,170,188),DT_CENTER|DT_SINGLELINE);
        if(!message[0])wcscpy(message,L"Documents / UFC6 Actions Overlay / SaveBackups");
    }
    if(state.page==PAGE_GAME_ICONS&&!conflict){
        const wchar_t *label=icon_state==GAME_ICONS_PS?L"Currently installed: PlayStation":icon_state==GAME_ICONS_XBOX?L"Currently installed: Xbox":icon_state==GAME_ICONS_MIXED?L"Mixed icons: choose a theme to repair":icon_state==GAME_ICONS_CHECKING?L"Checking installed game icons...":L"Game icons unavailable; see the message below.";
        RECT hint={20,314,r.right-20,338};text(dc,font_small,label,hint,RGB(176,184,197),DT_CENTER|DT_SINGLELINE);
    }
    RECT note = {20, 345, r.right - 20, 369};
    text(dc,font_small,conflict?L"In Settings, turn Windows Game Bar shortcut Off.":menu_power_page(&state)?(state.shutdown_countdown?L"Back or F10 cancels. Switching apps also cancels.":L"Unsaved game progress may be lost."):message[0]?message:state.page==PAGE_GAME_ICONS?icon_reason:save_failed?L"Settings could not be saved.":state.page==PAGE_RESTART?L"Unsaved progress may be lost.":state.page==PAGE_CHEATS&&selection>=2&&selection<4?(career_state.status==CAREER_PENDING?L"Pending: resume and reopen a career screen.":career_state.status==CAREER_ADDED?L"Added once. The game saves changes normally.":L"Left/right: amount. Confirm: review addition."):L"The game keeps running while this menu is open.",note,
         RGB(137,148,164), DT_CENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
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
__declspec(dllexport) void Ufc6ScreenshotResultPath(BOOL saved,const wchar_t *path){
    wchar_t filename[128]=L"",feedback[256];
    if(saved&&path&&wcslen(path)<32768){
        const wchar_t *base=wcsrchr(path,L'\\');base=base?base+1:path;
        lstrcpynW(filename,base,128);
        size_t bytes=(wcslen(path)+1)*sizeof(wchar_t);PWSTR copy=HeapAlloc(GetProcessHeap(),0,bytes);
        if(copy)memcpy(copy,path,bytes);
        EnterCriticalSection(&lock);
        if(last_screenshot_path)HeapFree(GetProcessHeap(),0,last_screenshot_path);
        last_screenshot_path=copy;lstrcpynW(last_screenshot_name,filename,128);
        LeaveCriticalSection(&lock);
    }
    InterlockedExchange(&screenshot_busy,0);
    if(saved&&filename[0]){swprintf(feedback,256,L"Saved: %ls",filename);status(feedback);}
    else status(saved?L"Saved to Windows Pictures / Screenshots.":L"Screenshot could not be saved.");
    log_line(saved?"Clean game screenshot saved.":"Screenshot capture failed.");
    if(saved&&filename[0])PostThreadMessageW(ui_thread,WM_SCREENSHOT_READY,0,0);
}
__declspec(dllexport) void Ufc6ScreenshotResult(BOOL saved){Ufc6ScreenshotResultPath(saved,NULL);}
static void open_screenshots_folder(void){
#ifdef QUIT_MENU_TEST
    if(test_mode){++test_folder_requests;EnterCriticalSection(&lock);action(menu_close(&menu,MENU_RESUME));LeaveCriticalSection(&lock);return;}
#endif
    if(!foreground())return;
    HRESULT initialized=CoInitializeEx(NULL,COINIT_APARTMENTTHREADED);
    if(FAILED(initialized)&&initialized!=RPC_E_CHANGED_MODE){status(L"Could not open the screenshots folder.");return;}
    PWSTR directory=NULL;
    EnterCriticalSection(&lock);
    if(last_screenshot_path){size_t bytes=(wcslen(last_screenshot_path)+1)*sizeof(wchar_t);directory=CoTaskMemAlloc(bytes);if(directory)memcpy(directory,last_screenshot_path,bytes);}
    LeaveCriticalSection(&lock);
    if(directory){wchar_t *slash=wcsrchr(directory,L'\\');if(slash)*slash=0;else{CoTaskMemFree(directory);directory=NULL;}}
    if(directory){DWORD attributes=GetFileAttributesW(directory);if(attributes==INVALID_FILE_ATTRIBUTES||!(attributes&FILE_ATTRIBUTE_DIRECTORY)){CoTaskMemFree(directory);directory=NULL;}}
    if(!directory)directory=menu_screenshot_directory();
    if(directory){
        ShowWindow(game_window,SW_MINIMIZE);
        INT_PTR result=(INT_PTR)ShellExecuteW(NULL,L"open",directory,NULL,NULL,SW_SHOWNORMAL);
        if(result>32){EnterCriticalSection(&lock);action(menu_close(&menu,MENU_RESUME));LeaveCriticalSection(&lock);}
        else{ShowWindow(game_window,SW_RESTORE);SetForegroundWindow(game_window);status(L"Could not open the screenshots folder.");}
        CoTaskMemFree(directory);
    }else status(L"The Windows screenshots folder is unavailable.");
    if(SUCCEEDED(initialized))CoUninitialize();
}
static void reset_overlay_settings(void){
    if(!test_mode&&!foreground())return;
    if(!overlay_settings_reset(settings_path)){status(L"Could not reset settings; previous settings kept.");return;}
    stop_live_cheats();
    EnterCriticalSection(&lock);int gamebar=menu.gamebar_shortcut;
    menu=(MenuState){.visible=1,.page=PAGE_SETTINGS,.selection=SETTING_RESET,.gamebar_shortcut=gamebar};
    reset_menu_owner();settings_save_failed=0;
    LeaveCriticalSection(&lock);
    status(L"Overlay defaults restored. Windows Game Bar setting kept.");
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
static uint64_t raster_hash(uint64_t hash,const void *data,size_t length){
    const unsigned char *bytes=data;for(size_t i=0;i<length;++i){hash^=bytes[i];hash*=1099511628211ULL;}return hash;
}
static uint64_t menu_raster_signature(void){
    uint64_t hash=14695981039346656037ULL;
    EnterCriticalSection(&lock);
    hash=raster_hash(hash,&menu,sizeof(menu));hash=raster_hash(hash,&career,sizeof(career));
    hash=raster_hash(hash,live_status,sizeof(live_status));hash=raster_hash(hash,live_enabled,sizeof(live_enabled));
    int owner[]={raw_navigation,raw_owner,menu_device,gamebar_conflict,settings_save_failed,GetTickCount64()<status_until,
        menu_device>=0&&devices[menu_device].used?native_player(&devices[menu_device]):0};
    hash=raster_hash(hash,owner,sizeof(owner));hash=raster_hash(hash,status_text,(wcslen(status_text)+1)*sizeof(wchar_t));
    hash=raster_hash(hash,last_screenshot_name,(wcslen(last_screenshot_name)+1)*sizeof(wchar_t));
    LeaveCriticalSection(&lock);
    wchar_t message[512];int state;
#ifdef QUIT_MENU_TEST
    if(test_mode){state=test_game_icons;message[0]=0;}else
#endif
    state=game_icons_status(message,512);
    hash=raster_hash(hash,&state,sizeof(state));hash=raster_hash(hash,message,(wcslen(message)+1)*sizeof(wchar_t));
    state=save_backups_state(message,512);
    hash=raster_hash(hash,&state,sizeof(state));hash=raster_hash(hash,message,(wcslen(message)+1)*sizeof(wchar_t));
    return hash;
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
    /* Render vector geometry and TrueType glyphs at 2x the FINAL pixel size.
       Downsampling smooths outlines without magnifying a fixed bitmap. */
    static HDC dc,target;
    static HBITMAP bitmap,output;
    static HGDIOBJ original_bitmap,original_output;
    static void *pixels,*output_pixels;
    static uint32_t bitmap_width,bitmap_height;
    static uint64_t cached_signature;static int have_frame;
    uint64_t signature=menu_raster_signature();
    if(!dc||!target||bitmap_width!=width||bitmap_height!=height){
        have_frame=0;
        if(bitmap){SelectObject(dc,original_bitmap);DeleteObject(bitmap);bitmap=NULL;}
        if(output){SelectObject(target,original_output);DeleteObject(output);output=NULL;}
        if(!dc)dc=CreateCompatibleDC(NULL);
        if(!target)target=CreateCompatibleDC(NULL);
        BITMAPINFO info={0};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth=(LONG)(width*2);info.bmiHeader.biHeight=-(LONG)(height*2);
        info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
        if(dc)bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&pixels,NULL,0);
        info.bmiHeader.biWidth=(LONG)width;info.bmiHeader.biHeight=-(LONG)height;
        if(target)output=CreateDIBSection(target,&info,DIB_RGB_COLORS,&output_pixels,NULL,0);
        if(!dc||!target||!bitmap||!output){
            if(bitmap)DeleteObject(bitmap);if(output)DeleteObject(output);
            if(dc)DeleteDC(dc);if(target)DeleteDC(target);
            dc=NULL;target=NULL;bitmap=NULL;output=NULL;return FALSE;
        }
        original_bitmap=SelectObject(dc,bitmap);original_output=SelectObject(target,output);
        bitmap_width=width;bitmap_height=height;
    }
    if(have_frame&&cached_signature==signature){memcpy(destination,output_pixels,bytes);return TRUE;}
    SetMapMode(dc,MM_ANISOTROPIC);SetWindowExtEx(dc,MENU_BASE_WIDTH,MENU_BASE_HEIGHT,NULL);
    SetViewportExtEx(dc,(int)width*2,(int)height*2,NULL);
    RECT r={0,0,MENU_BASE_WIDTH,MENU_BASE_HEIGHT};draw_menu(dc,r);GdiFlush();
    SetMapMode(dc,MM_TEXT);SetMapMode(target,MM_TEXT);
    SetStretchBltMode(target,HALFTONE);SetBrushOrgEx(target,0,0,NULL);
    if(!StretchBlt(target,0,0,(int)width,(int)height,dc,0,0,(int)width*2,(int)height*2,SRCCOPY))return FALSE;
    GdiFlush();
    unsigned char *p=output_pixels;
    for(size_t i=3;i<bytes;i+=4)p[i]=255;
    memcpy(destination,output_pixels,bytes);cached_signature=signature;have_frame=1;
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
        int first=menu_first_row(&menu);
        for(int i=0;i<menu_rows(&menu);++i){
            if((menu.page==PAGE_SETTINGS||menu.page==PAGE_CHEATS)&&(i<first||i>=first+6))continue;
            int top,height;row_geometry(menu.page,i,first,&top,&height);if(y>=top&&y<top+height)selected=i;
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
    log_line("Overlay build " UFC6_OVERLAY_VERSION ".");
    if (!test_mode && !install_hooks()) return 0;
    int longevity_setting=GetPrivateProfileIntW(L"Menu",L"LongevityProtection",0,settings_path)!=0;
    int training_setting=GetPrivateProfileIntW(L"Menu",L"TrainingRewardMultiplier",1,settings_path);
    menu.training_multiplier_index=training_setting==2?1:training_setting==5?2:training_setting==10?3:0;
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
    font_title = CreateFontW(-32,0,0,0,FW_BOLD,0,0,0,DEFAULT_CHARSET,0,0,ANTIALIASED_QUALITY,0,L"Segoe UI");
    font_body = CreateFontW(-22,0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,ANTIALIASED_QUALITY,0,L"Segoe UI");
    font_main = CreateFontW(-18,0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,ANTIALIASED_QUALITY,0,L"Segoe UI");
    font_settings = CreateFontW(-20,0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,ANTIALIASED_QUALITY,0,L"Segoe UI");
    font_small = CreateFontW(-16,0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,ANTIALIASED_QUALITY,0,L"Segoe UI");
    HMODULE xi = LoadLibraryExW(L"xinput1_4.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (xi) xstate = (XStateFn)GetProcAddress(xi, (LPCSTR)100);
    log_line(xstate ? "Ready. LB+RB+Menu (800ms), Guide (if available), or F10." : "Ready. LB+RB+Menu (800ms) or F10. Guide unavailable.");
    if(!test_mode){game_icons_init(instance);save_backups_init(instance);}
    refresh_gamebar();
    live_cheats_init(&live_cheats,0x900000000ULL);career_cheats_init(0x900000000ULL);
    career_cheats_protect_longevity(longevity_setting);career_cheats_training_multiplier(menu_training_multiplier(&menu));
#ifdef QUIT_MENU_TEST
    if(test_mode)career=(CareerSnapshot){.ready=1,.status=CAREER_READY,.balance={12109126,16},.maximum={2147483647,16383},.fitness_ready=1,.fitness=7900,.training_ready=1,.training_days=0,.training_week=1,.longevity_ready=1,.longevity=9895,.longevity_enabled=longevity_setting,.longevity_requested=longevity_setting,.training_multiplier=(uint32_t)menu_training_multiplier(&menu),.training_rewards_ready=1,.sparring_grades_ready=1,.sparring_grades_completed=2};
#endif
    refresh_live_cheats();
    if(!test_mode)gamebar_monitor_start();
    SetTimer(NULL, 1, 16, NULL);
    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        if (msg.message == WM_TIMER) {
            if (!IsWindow(game_window)) break;
            if(GetTickCount64()>=next_live_check){next_live_check=GetTickCount64()+250;refresh_live_cheats();}
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
            else if(msg.wParam==MENU_GAME_ICONS_OPEN){if(!test_mode)game_icons_check();}
            else if(msg.wParam==MENU_GAME_ICONS_APPLY){
                EnterCriticalSection(&lock);int xbox=menu.game_icons_xbox;LeaveCriticalSection(&lock);
#ifdef QUIT_MENU_TEST
                if(test_mode){++test_game_icon_applies;test_game_icons=xbox?GAME_ICONS_XBOX:GAME_ICONS_PS;status(L"Test icons applied; no game files changed.");}else
#endif
                if(game_icons_status(NULL,0)==(xbox?GAME_ICONS_XBOX:GAME_ICONS_PS))status(L"These game button icons are already selected.");
                else if(game_icons_apply(game_window,xbox)){EnterCriticalSection(&lock);menu_close(&menu,MENU_RESUME);LeaveCriticalSection(&lock);ShowWindow(overlay,SW_HIDE);}
                else status(L"Icons could not start. Check the game folder, backup and launcher.");
            }
            else if(msg.wParam==MENU_CHECK_UPDATES||msg.wParam==MENU_EXPORT_DIAGNOSTICS){
#ifdef QUIT_MENU_TEST
                status(msg.wParam==MENU_EXPORT_DIAGNOSTICS?L"Test support report; no files or desktop app opened.":L"Test update check; no network or desktop app opened.");
#else
                wchar_t path[1200],bin[1024],arguments[1200];DWORD n=GetModuleFileNameW(instance,bin,1024);wchar_t *slash=n&&n<1024?wcsrchr(bin,L'\\'):NULL;
                BOOL opened=FALSE;
                if(slash){*slash=0;swprintf(path,1200,L"%ls\\ufc6_menu_update.exe",bin);swprintf(arguments,1200,L"--folder \"%ls\"",bin);if(msg.wParam==MENU_EXPORT_DIAGNOSTICS)swprintf(arguments,1200,L"--diagnostics \"%ls\" %ls %lu",bin,InterlockedCompareExchange(&vulkan_active,0,0)?L"active":L"inactive",(unsigned long)GetCurrentProcessId());
                    SHELLEXECUTEINFOW launch={.cbSize=sizeof(launch),.lpVerb=L"open",.lpFile=path,.lpParameters=arguments,.lpDirectory=bin,.nShow=SW_SHOWNORMAL};opened=ShellExecuteExW(&launch);}
                if(opened){EnterCriticalSection(&lock);menu_close(&menu,MENU_RESUME);LeaveCriticalSection(&lock);ShowWindow(overlay,SW_HIDE);ShowWindow(game_window,SW_MINIMIZE);}
                else status(L"Support/update helper could not start. Run the latest overlay setup.");
#endif
            }
            else if(msg.wParam==MENU_BACKUP_OPEN)status(L"");
            else if(msg.wParam==MENU_BACKUP_CREATE){
#ifdef QUIT_MENU_TEST
                status(L"Test backup requested; save files were not accessed.");
#else
                save_backups_create();
#endif
            }
            else if(msg.wParam==MENU_BACKUP_FOLDER){
#ifdef QUIT_MENU_TEST
                status(L"Test backups folder action; Explorer was not opened.");
#else
                if(save_backups_open()){ShowWindow(game_window,SW_MINIMIZE);}else status(L"Create a backup first to open its folder.");
#endif
            }
            else if(msg.wParam==MENU_OPEN_SCREENSHOTS)open_screenshots_folder();
            else if(msg.wParam==MENU_RESET_SETTINGS)reset_overlay_settings();
            else if(msg.wParam==MENU_CHEAT_STAMINA||msg.wParam==MENU_CHEAT_HEALTH)change_live_cheat(msg.wParam==MENU_CHEAT_STAMINA?LIVE_STAMINA:LIVE_HEALTH);
            else if(msg.wParam==MENU_TRAINING_MULTIPLIER){
                EnterCriticalSection(&lock);int factor=menu_training_multiplier(&menu);
#ifdef QUIT_MENU_TEST
                if(test_mode)career.training_multiplier=(uint32_t)factor;else
#endif
                {career_cheats_training_multiplier(factor);career_cheats_snapshot(&career);}
                LeaveCriticalSection(&lock);save_settings();status(L"Training reward multiplier saved.");
            }
            else if(msg.wParam==MENU_CAREER_LONGEVITY){
                EnterCriticalSection(&lock);int enabled=!career.longevity_requested,ok=0;
#ifdef QUIT_MENU_TEST
                if(test_mode){career.longevity_enabled=enabled;career.longevity_requested=enabled;ok=1;}else
#endif
                {ok=career_cheats_protect_longevity(enabled);career_cheats_snapshot(&career);}
                LeaveCriticalSection(&lock);
                if(ok)save_settings();
                status(ok?(enabled?L"Longevity protection On. Choice saved.":L"Longevity protection Off. Choice saved."):L"Open a verified career hub to enable protection.");
            }
            else if(msg.wParam==MENU_CAREER_ADD)add_career_value();
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
        } else if(msg.message==WM_SCREENSHOT_READY){
            EnterCriticalSection(&lock);
            if(menu.visible&&menu.page==PAGE_MAIN&&menu.selection==MAIN_SCREENSHOT){menu.page=PAGE_SCREENSHOT;menu.selection=1;menu.armed=0;}
            LeaveCriticalSection(&lock);InvalidateRect(overlay,NULL,FALSE);
        } else if (msg.message == WM_MENU_TOGGLE && (test_mode||foreground())) toggle_menu();
        else { TranslateMessage(&msg); DispatchMessageW(&msg); }
    }
    stop_live_cheats();
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
__declspec(dllexport) int MenuTestTextClips(int reset){int value=test_text_clips;if(reset)test_text_clips=0;return value;}
__declspec(dllexport) int MenuTestGameIcons(void){return test_game_icons|(test_game_icon_applies<<8);}
__declspec(dllexport) int MenuTestLongevity(void){return career.longevity_requested;}
__declspec(dllexport) int MenuTestCheatEnabled(int which){return which>=0&&which<LIVE_COUNT?live_enabled[which]:-1;}
__declspec(dllexport) int MenuTestCheatStatus(int which,int value){if(which<0||which>=LIVE_COUNT)return -1;EnterCriticalSection(&lock);if(value>=0&&value<=LIVE_GAME){test_live_status[which]=value;if(value==LIVE_OFF)test_live_requested[which]=0;else if(value==LIVE_ON)test_live_requested[which]=1;}LeaveCriticalSection(&lock);refresh_live_cheats();return live_status[which];}
__declspec(dllexport) int MenuTestFolderRequests(void){return test_folder_requests;}
__declspec(dllexport) void MenuTestScreenshotName(wchar_t *out,int count){EnterCriticalSection(&lock);if(count>0)lstrcpynW(out,last_screenshot_name,count);LeaveCriticalSection(&lock);}
__declspec(dllexport) int MenuTestBackendPreferences(void){CareerSnapshot value;career_cheats_snapshot(&value);return value.longevity_requested|((int)value.training_multiplier<<8);}
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
