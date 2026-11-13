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
#include "overlay_preferences.h"
#include "controller_artwork.h"
#include "startup_options.h"
#include "startup_prompts.h"
#include "photo_mode.h"
#include "replay_controls.h"
#include "audio_focus.h"
#include "photo_effects_settings.h"
#include "photo_panel_controls.h"
#include <math.h>
static StartupOptions startup_options;
#ifndef QUIT_MENU_TEST
#include "engine_fingerprint.h"
#endif

#define WM_MENU_ACTION (WM_APP + 40)
#define WM_MENU_TOGGLE (WM_APP + 41)
#define WM_SCREENSHOT_READY (WM_APP + 42)
#define WM_BINDING_ACTION (WM_APP + 43)
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
static OverlayPreferences preferences;
static OverlayTheme current_theme;
static __thread OverlayTheme paint_theme;
static wchar_t theme_path[MAX_PATH];
static wchar_t performance_bin[LAUNCH_PATH];
#ifdef QUIT_MENU_TEST
static PerformanceSettings test_performance_saved={.crowd_off=1,.drs_off=1,.render_scale=133};
static int test_performance_saves,test_performance_restarts;
#endif
static BindingLatch pad_bindings[BIND_COUNT][20],key_bindings[BIND_COUNT];
static uint64_t next_theme_read;
static int key_capture_ready;static uint32_t captured_key;
#ifdef QUIT_MENU_TEST
static unsigned char test_keys[256];
#endif

static HINSTANCE instance;
static HWND game_window, overlay;
static DWORD ui_thread;
static HFONT font_title, font_body, font_small, font_main, font_settings;
static HFONT font_sets[3][3];static __thread int frame_font_index;
static uint64_t toast_until;
#ifdef QUIT_MENU_TEST
static int test_photo_help_override,test_require_renderer;
#endif
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
static volatile LONG screenshot_with_overlay;
static volatile LONG screenshot_request,screenshot_busy,screenshot_capable;
static wchar_t status_text[120];
static wchar_t last_screenshot_name[128];
static PWSTR last_screenshot_path;
static uint64_t status_until;
static LiveCheats live_cheats;
static CareerSnapshot career;
static int ko_status;
static int live_status[LIVE_COUNT],live_enabled[LIVE_COUNT];
static uint64_t next_live_check;
#ifdef QUIT_MENU_TEST
static int test_live_status[LIVE_COUNT]={LIVE_OFF,LIVE_OFF};
static int test_live_requested[LIVE_COUNT];
#endif
static uint32_t render_width=MENU_BASE_WIDTH,render_height=MENU_BASE_HEIGHT;

static void status(const wchar_t *message) {
    EnterCriticalSection(&lock);lstrcpynW(status_text,message,120);status_until=GetTickCount64()+5000;toast_until=GetTickCount64()+3000;LeaveCriticalSection(&lock);
}

static int foreground(void);
static void log_line(const char *line);
static void refresh_live_cheats(void){
    EnterCriticalSection(&lock);
#ifdef QUIT_MENU_TEST
    if(test_mode){memcpy(live_status,test_live_status,sizeof(live_status));for(int i=0;i<LIVE_COUNT;++i)live_enabled[i]=test_live_requested[i]||test_live_status[i]==LIVE_LAUNCHER||test_live_status[i]==LIVE_GAME;LeaveCriticalSection(&lock);return;}
#endif
    live_cheats_tick(&live_cheats);
    ReplaySnapshot ko_replay;replay_snapshot(&ko_replay);
    int previous_ko=ko_status;ko_status=live_ko_poll(foreground()&&!IsIconic(game_window)&&!menu.photo_active&&!ko_replay.available);
    if(previous_ko==1&&ko_status!=1)status(ko_status==2?L"Knockout triggered. Next hit KO disarmed.":L"Next hit KO cancelled or expired.");
    CareerSnapshot before=career;career_cheats_tick();career_cheats_snapshot(&career);
    CareerDiagnostics diagnostic;career_cheats_diagnostics(&diagnostic);
    static int last_career_diagnostic=-1;static uint64_t next_career_diagnostic;
    if(diagnostic.reason!=last_career_diagnostic&&GetTickCount64()>=next_career_diagnostic){
        char line[200];snprintf(line,sizeof(line),"Career binding: reason=%s hooks=%d probes=%u ready=%d status=%d.",career_cheats_diagnostic_name(diagnostic.reason),diagnostic.hooks,diagnostic.probes,career.ready,career.status);log_line(line);
        last_career_diagnostic=diagnostic.reason;next_career_diagnostic=GetTickCount64()+5000;
    }
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
    if(!test_mode){CareerDiagnostics diagnostic;career_cheats_diagnostics(&diagnostic);char line[240];snprintf(line,sizeof(line),"Career action: kind=%d amount=%u queued=%d ready=%d status=%d reason=%s hooks=%d probes=%u.",kind,amount,ok,career.ready,result,career_cheats_diagnostic_name(diagnostic.reason),diagnostic.hooks,diagnostic.probes);log_line(line);}
    if(kind==CAREER_GRADES&&!test_mode){char line[140];snprintf(line,sizeof(line),"Grade A request: queued=%d verified=%d upgradable=%u status=%d.",ok,grades_ready,grade_count,result);log_line(line);}
    status(ok?(test_mode?L"Career test addition applied.":L"Addition queued. Resume and reopen a career screen to apply."):
        result==CAREER_LIMIT?L"This amount exceeds the game's balance limit.":result==CAREER_PENDING?L"An addition is already pending. Resume the game to apply it.":kind==CAREER_GRADES?(grades_ready?L"All sparring grades are already A.":L"Sparring grades unavailable: career data could not be verified."):result==CAREER_UNSUPPORTED?L"Career cheats unavailable: loaded game code did not match.":L"Career data could not be verified. Export a support report from Settings.");
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
/* Bounded lifecycle records preserve the original log lines for support tools. */
static void log_timeline(const char *event){
 SYSTEMTIME utc;GetSystemTime(&utc);
 EnterCriticalSection(&lock);int photo=menu.photo_active,frozen=menu.photo_frozen,hud=menu.photo_hud,cage=menu.photo_cage,page=menu.page,visible=menu.visible,include_ui=menu.screenshot_include_ui;LeaveCriticalSection(&lock);
 char line[360];snprintf(line,sizeof(line),"Timeline UTC=%04u-%02u-%02uT%02u:%02u:%02u.%03uZ tick=%llu event=%s photo=%d frozen=%d hud_hidden=%d cage_visible=%d page=%d menu=%d include_ui=%d",utc.wYear,utc.wMonth,utc.wDay,utc.wHour,utc.wMinute,utc.wSecond,utc.wMilliseconds,(unsigned long long)GetTickCount64(),event,photo,frozen,hud,cage,page,visible,include_ui);log_line(line);
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
static int renderer_input_ready(void){
#ifdef QUIT_MENU_TEST
    if(test_mode&&!test_require_renderer)return 1;
#endif
    return InterlockedCompareExchange(&vulkan_active,0,0)!=0;
}
static void action(int result) {
    if(result==MENU_OPEN&&!renderer_input_ready()){
        menu.visible=0;menu.draining=0;menu.armed=0;reset_menu_owner();
        log_line("Menu opening blocked: Vulkan renderer is inactive; game input remains available.");return;
    }
    /* Log page transitions only; no watermark/player name is recorded. */
    static int diagnostic_page=-1;
    if(result!=MENU_NONE&&diagnostic_page!=menu.page){diagnostic_page=menu.page;
        if(menu.page==PAGE_PHOTO||menu.page==PAGE_PHOTO_CONTROLS||menu.page==PAGE_PHOTO_EFFECTS||menu.page==PAGE_PHOTO_WATERMARK){char line[160];snprintf(line,sizeof(line),"Photo tools: page=%d row=%d active=%d filter=%d strength=%d",menu.page,menu.selection,menu.photo_active,menu.effects.preset,menu.effects.strength);log_line(line);}}
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
    if (!menu.visible && !menu.draining && !photo_mode_blocks_input())
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
    if (!menu.visible && !menu.draining && !photo_mode_blocks_input())
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
static HFONT render_font(HFONT f){int role=f==font_title?0:f==font_small?2:1;HFONT scaled=font_sets[frame_font_index][role];return scaled?scaled:f;}
static void text(HDC dc, HFONT f, const wchar_t *str, RECT r, COLORREF color, UINT flags) {
    flags|=DT_NOPREFIX;
    HFONT previous = SelectObject(dc, render_font(f));
    SetTextColor(dc, color);
    if(!(flags&DT_WORDBREAK))flags|=DT_SINGLELINE;
#ifdef QUIT_MENU_TEST
    if(test_mode&&(flags&DT_SINGLELINE)){TEXTMETRICW metrics;if(GetTextMetricsW(dc,&metrics)&&metrics.tmHeight>r.bottom-r.top){++test_text_clips;char diagnostic[200];snprintf(diagnostic,sizeof(diagnostic),"Text height overflow: %ls height=%ld box=%ld top=%ld",str,metrics.tmHeight,r.bottom-r.top,r.top);log_line(diagnostic);}}
#endif
    DrawTextW(dc, str, -1, &r, flags);
    SelectObject(dc, previous);
}
static void button_icon(HDC dc,int x,int y,int back,int playstation){controller_artwork_draw(dc,x,y,30,30,back?PAD_B:PAD_A,playstation);}
static void dpad_icon(HDC dc,int x,int y,int ps){controller_artwork_draw(dc,x,y,30,30,0,ps);}
static void save_settings(void) {
    EnterCriticalSection(&lock); int training_setting=menu_training_multiplier(&menu),longevity_setting=career.longevity_requested,ps=menu.playstation_icons,scale=menu_scale(&menu),auto_launch=menu_auto_launch(&menu),guide_enabled=menu_guide_enabled(&menu),startup_popups=menu.startup_popups,skip_splashes=menu.skip_splashes,first_only=menu.first_player_only,include_ui=menu.screenshot_include_ui,mute_background=menu.mute_background,decision_replays=menu.decision_replays,replay_step=menu.replay_step_index;uint32_t chord=menu_shortcut(&menu); LeaveCriticalSection(&lock);
    wchar_t value[24];swprintf(value,24,L"0x%08lx",(unsigned long)chord);
    BOOL saved=WritePrivateProfileStringW(L"Menu",L"ButtonIcons",ps?L"PlayStation":L"Xbox",settings_path);
    saved=WritePrivateProfileStringW(L"Menu",L"OpeningShortcut",value,settings_path)&&saved;
    swprintf(value,24,L"%d",scale);
    saved=WritePrivateProfileStringW(L"Menu",L"UIScale",value,settings_path)&&saved;
    saved=WritePrivateProfileStringW(L"Menu",L"AutoLaunchAfterRestart",auto_launch?L"1":L"0",settings_path)&&saved;
    saved=WritePrivateProfileStringW(L"Menu",L"SkipStartupSplashes",skip_splashes?L"1":L"0",settings_path)&&saved;
    saved=WritePrivateProfileStringW(L"Menu",L"StartupSkipPopups",startup_popups?L"1":L"0",settings_path)&&saved;
    saved=WritePrivateProfileStringW(L"Menu",L"OfferDecisionReplays",decision_replays?L"1":L"0",settings_path)&&saved;
    saved=WritePrivateProfileStringW(L"Menu",L"MuteWhenUnfocused",mute_background?L"1":L"0",settings_path)&&saved;
    swprintf(value,24,L"%d",replay_step);saved=WritePrivateProfileStringW(L"Menu",L"ReplayStepIndex",value,settings_path)&&saved;
    saved=WritePrivateProfileStringW(L"Menu",L"IncludeOverlayInScreenshots",include_ui?L"1":L"0",settings_path)&&saved;
    saved=WritePrivateProfileStringW(L"Menu",L"GuideButtonOpensMenu",guide_enabled?L"1":L"0",settings_path)&&saved;
    saved=WritePrivateProfileStringW(L"Menu",L"ControllerAccess",first_only?L"Player1":L"Either",settings_path)&&saved;
    saved=WritePrivateProfileStringW(L"Menu",L"LongevityProtection",longevity_setting?L"1":L"0",settings_path)&&saved;
    swprintf(value,24,L"%d",training_setting);saved=WritePrivateProfileStringW(L"Menu",L"TrainingRewardMultiplier",value,settings_path)&&saved;
    EnterCriticalSection(&lock);saved=photo_effects_save(&menu.effects,settings_path)&&saved;saved=preferences_save(&preferences,settings_path)&&saved; settings_save_failed=!saved; LeaveCriticalSection(&lock);
    log_line(saved ? "Overlay settings saved." : "Could not save overlay settings.");
}
static void open_performance(void){
 PerformanceSettings values={0};int available;
#ifdef QUIT_MENU_TEST
 if(test_mode){values=test_performance_saved;available=1;}else
#endif
 available=performance_load(performance_bin,&values)==PERF_OK;
 EnterCriticalSection(&lock);menu.performance=values;menu.performance_original=values;menu.performance_available=available;menu.performance_dirty=0;menu.performance_feedback=0;LeaveCriticalSection(&lock);
 if(!available)status(L"Performance controls require the PORTHEON launcher and its settings file.");
}
static void save_performance(int restart){
 if(!test_mode&&!foreground())return;
 EnterCriticalSection(&lock);PerformanceSettings desired=menu.performance,expected=menu.performance_original;int available=menu.performance_available;LeaveCriticalSection(&lock);
 if(!available){status(L"PORTHEON performance settings are unavailable.");return;}
 int result;
#ifdef QUIT_MENU_TEST
 if(test_mode){test_performance_saved=desired;++test_performance_saves;result=PERF_OK;}else
#endif
 result=performance_save(performance_bin,&expected,&desired);
 if(result!=PERF_OK){
  status(result==PERF_CHANGED_EXTERNALLY?L"PORTHEON changed these settings. Reopen Performance to reload them.":L"Could not save performance settings. Check folder access and retry.");
  EnterCriticalSection(&lock);menu.page=PAGE_PERFORMANCE;menu.selection=restart?PERF_RESTART:PERF_SAVE;menu.performance_feedback=result==PERF_CHANGED_EXTERNALLY?3:2;menu.armed=0;LeaveCriticalSection(&lock);return;
 }
 EnterCriticalSection(&lock);menu.performance_original=desired;menu.performance_dirty=0;menu.performance_feedback=1;menu.page=PAGE_PERFORMANCE;menu.selection=restart?PERF_RESTART:PERF_SAVE;menu.armed=0;LeaveCriticalSection(&lock);
 log_timeline(restart?"performance_saved_restart_requested":"performance_saved_next_launch");
 if(!restart){status(L"Performance settings saved for the next game launch.");return;}
#ifdef QUIT_MENU_TEST
 if(test_mode){++test_performance_restarts;status(L"Test performance restart; no game was closed.");return;}
#endif
 photo_mode_command(PHOTO_STOP);
 if(menu_restart_game(game_window,instance)){EnterCriticalSection(&lock);menu_close(&menu,MENU_RESUME);LeaveCriticalSection(&lock);ShowWindow(overlay,SW_HIDE);}
 else {EnterCriticalSection(&lock);menu.performance_feedback=4;LeaveCriticalSection(&lock);status(L"Performance settings saved. Restart the game through PORTHEON to apply.");}
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
static void apply_theme(void){OverlayTheme fresh;theme_load(&fresh,theme_path,preferences.theme_choice);EnterCriticalSection(&lock);current_theme=fresh;LeaveCriticalSection(&lock);}
static void save_binding(int id,int keyboard,uint32_t value){
    EnterCriticalSection(&lock);int ok=binding_set(&preferences,id,keyboard,value);
    if(!keyboard&&id==BIND_OPEN)menu.open_chord=preferences.actions[BIND_OPEN].pad;
    memset(pad_bindings,0,sizeof(pad_bindings));memset(key_bindings,0,sizeof(key_bindings));
    LeaveCriticalSection(&lock);
    if(ok){save_settings();status(L"Keybind saved.");}else status(L"Not saved: reserved, overlapping or invalid shortcut.");
}
static void rounded_box(HDC dc,RECT box,int radius,COLORREF fill,COLORREF edge){
    HBRUSH brush=CreateSolidBrush(fill);HPEN pen=CreatePen(PS_SOLID,1,edge);
    HGDIOBJ old_brush=SelectObject(dc,brush),old_pen=SelectObject(dc,pen);
    RoundRect(dc,box.left,box.top,box.right,box.bottom,radius,radius);
    SelectObject(dc,old_pen);SelectObject(dc,old_brush);DeleteObject(pen);DeleteObject(brush);
}
static void row_surface(HDC dc,RECT box,int style,int radius,COLORREF fill,COLORREF edge){
    if(style!=4){rounded_box(dc,box,radius,fill,edge);return;}
    int cut=(box.bottom-box.top)/3;if(cut>9)cut=9;
    POINT points[]={{box.left+cut,box.top},{box.right-cut-1,box.top},{box.right-1,box.top+cut},{box.right-1,box.bottom-cut-1},{box.right-cut-1,box.bottom-1},{box.left+cut,box.bottom-1},{box.left,box.bottom-cut-1},{box.left,box.top+cut}};
    HPEN pen=CreatePen(PS_SOLID,1,edge);HBRUSH brush=CreateSolidBrush(fill);HGDIOBJ old_pen=SelectObject(dc,pen),old_brush=SelectObject(dc,brush);Polygon(dc,points,8);SelectObject(dc,old_pen);SelectObject(dc,old_brush);DeleteObject(pen);DeleteObject(brush);
}
static void arrow_mark(HDC dc,int x,int y,int direction,COLORREF color){
    HPEN pen=CreatePen(PS_SOLID,1,color);HGDIOBJ old=SelectObject(dc,pen);
    MoveToEx(dc,x-direction*3,y-4,NULL);LineTo(dc,x+direction*1,y);LineTo(dc,x-direction*3,y+4);
    SelectObject(dc,old);DeleteObject(pen);
}
static void toggle_switch(HDC dc,RECT row,int on,int selected){
    int y=(row.top+row.bottom)/2;RECT track={row.right-54,y-9,row.right-14,y+10};
    rounded_box(dc,track,20,on?paint_theme.accent:RGB(67,78,94),on?paint_theme.accent:RGB(88,101,119));
    int x=on?track.right-17:track.left+3;RECT thumb={x,y-6,x+14,y+8};
    rounded_box(dc,thumb,14,on?paint_theme.background:RGB(222,229,240),on?paint_theme.background:RGB(222,229,240));
    RECT label={row.right-115,row.top,row.right-64,row.bottom};
    text(dc,font_small,on?L"On":L"Off",label,selected?RGB(235,247,239):on?paint_theme.accent:RGB(157,170,188),DT_RIGHT|DT_VCENTER|DT_SINGLELINE);
}
static void value_chip(HDC dc,RECT row,const wchar_t *label,int selected,int adjustable){
    SIZE extent={0};HFONT old=SelectObject(dc,render_font(font_small));GetTextExtentPoint32W(dc,label,(int)wcslen(label),&extent);SelectObject(dc,old);
    int width=extent.cx+(adjustable?46:24);if(width<64)width=64;if(width>226)width=226;
    int y=(row.top+row.bottom)/2;RECT box={row.right-14-width,y-12,row.right-14,y+12};
    rounded_box(dc,box,10,selected?paint_theme.focus:paint_theme.surface,selected?paint_theme.focus_border:paint_theme.border);
    RECT label_box=box;label_box.left+=adjustable?19:10;label_box.right-=adjustable?19:10;
    text(dc,font_small,label,label_box,selected?paint_theme.text:RGB(192,207,225),DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
    if(adjustable){arrow_mark(dc,box.left+10,y,-1,RGB(144,170,154));arrow_mark(dc,box.right-10,y,1,RGB(144,170,154));}
}
#include "shortcut_badges.h"
static void row_geometry(int page,int index,int first,int *top,int *height) {
    int global_index=index;if(menu_scroll_page(page))index-=first;
    *top=page==PAGE_CHEATS?84+32*index+(first<=3&&global_index>=3?24:0):page==PAGE_MAIN?84+27*index+(index>=MAIN_REBOOT?10:0):(page==PAGE_STARTUP||menu_setting_page(page)||page==PAGE_PHOTO||page==PAGE_PHOTO_CONTROLS||page==PAGE_PHOTO_EFFECTS||page==PAGE_PHOTO_WATERMARK||page==PAGE_PERFORMANCE||page==PAGE_SETTINGS_HOME||page==PAGE_KEYBINDS||page==PAGE_BINDING||page==PAGE_THEME)?86+32*index:page==PAGE_BACKUPS?104+40*index:(page==PAGE_PERFORMANCE_RESTART||page==PAGE_PERFORMANCE_DISCARD||page==PAGE_SCREENSHOT||page==PAGE_RESET||page==PAGE_CAREER_ADD||page==PAGE_STARTUP_CONFIRM||page==PAGE_GAME_ICONS_CONFIRM||page==PAGE_QUIT)?182+48*index:100+56*index;
    *height=page==PAGE_BACKUPS?34:page==PAGE_CHEATS?30:page==PAGE_MAIN?25:(page==PAGE_STARTUP||menu_setting_page(page)||page==PAGE_PHOTO||page==PAGE_PHOTO_CONTROLS||page==PAGE_PHOTO_EFFECTS||page==PAGE_PHOTO_WATERMARK||page==PAGE_PERFORMANCE||page==PAGE_SETTINGS_HOME||page==PAGE_KEYBINDS||page==PAGE_BINDING||page==PAGE_THEME)?30:(page==PAGE_PERFORMANCE_RESTART||page==PAGE_PERFORMANCE_DISCARD||page==PAGE_SCREENSHOT||page==PAGE_RESET||page==PAGE_CAREER_ADD||page==PAGE_STARTUP_CONFIRM||page==PAGE_GAME_ICONS_CONFIRM||page==PAGE_QUIT)?42:50;
}
static void draw_menu(HDC dc, RECT r) {
    EnterCriticalSection(&lock);paint_theme=current_theme;OverlayPreferences bindings=preferences;frame_font_index=(preferences.font_size-16)/2;LeaveCriticalSection(&lock);
    HBRUSH bg = CreateSolidBrush(paint_theme.background);
    FillRect(dc, &r, bg); DeleteObject(bg);
    SetBkMode(dc, TRANSPARENT);
    CareerSnapshot career_state;MenuState state;int save_failed,conflict,owner_player=0,has_owner=0;int ko_state;int cheat_states[LIVE_COUNT],cheat_enabled[LIVE_COUNT];wchar_t message[120],filename[128];
    EnterCriticalSection(&lock);
    ko_state=ko_status;memcpy(cheat_states,live_status,sizeof(cheat_states));memcpy(cheat_enabled,live_enabled,sizeof(cheat_enabled));
    career_state=career;state=menu;save_failed=settings_save_failed;conflict=gamebar_conflict;
    lstrcpynW(filename,last_screenshot_name,128);
    if(raw_navigation&&raw_owner>=0){has_owner=1;owner_player=raw_owner+1;}
    else if(menu_device>=0&&devices[menu_device].used){has_owner=1;owner_player=native_player(&devices[menu_device]);}
    lstrcpynW(message,GetTickCount64()<status_until?status_text:L"",120);
    LeaveCriticalSection(&lock);
    int extra=(int)menu_canvas_height(&state)-(int)MENU_BASE_HEIGHT;
    int selection=state.selection,ps=state.playstation_icons;wchar_t icon_reason[512];int icon_state;
#ifdef QUIT_MENU_TEST
    if(test_mode){icon_state=test_game_icons;wcscpy(icon_reason,L"Test game icons; no game files are changed.");}else
#endif
    icon_state=game_icons_status(icon_reason,512);
    wchar_t startup_reason[512];int startup_state;
#ifdef QUIT_MENU_TEST
    if(test_mode){startup_state=GAME_ICONS_PS;lstrcpyW(startup_reason,L"Test startup page; no game assets changed.");}else
#endif
    startup_state=startup_assets_status(startup_reason,512);
    RECT title = {36, 10, r.right - 36, 57};
    text(dc,font_title,state.page==PAGE_STARTUP?L"Startup screens":state.page==PAGE_STARTUP_CONFIRM?(state.startup_patch_enable?L"Enable startup bypass?":L"Restore startup screens?"):state.page==PAGE_PERFORMANCE?L"Performance":state.page==PAGE_PERFORMANCE_RESTART?L"Save and restart?":state.page==PAGE_PERFORMANCE_DISCARD?L"Discard edits?":state.page==PAGE_PHOTO_EFFECTS?L"Filters & effects":state.page==PAGE_PHOTO_WATERMARK?L"Photo watermark":state.page==PAGE_PHOTO?L"Photo Mode":state.page==PAGE_PHOTO_CONTROLS?L"Photo controls":state.page==PAGE_KEYBINDS?L"Keybinds":state.page==PAGE_BINDING?binding_names[state.binding_action]:state.page==PAGE_THEME?L"Theme":state.page==PAGE_QUIT?L"Quit game?":state.page==PAGE_BACKUPS?L"Savegame backups":state.page==PAGE_GAME_ICONS?L"Controller icon patch":state.page==PAGE_GAME_ICONS_CONFIRM?(state.game_icons_xbox?L"Apply Xbox icons?":L"Restore PS icons?"):state.page==PAGE_CAREER_ADD?(state.career_kind==CAREER_GRADES?L"Set all grades to A?":state.career_kind==CAREER_DAYS?L"Refill training days?":state.career_kind==CAREER_FITNESS?L"Restore fitness?":state.career_kind==0?L"Add money?":L"Add skill points?"):state.page==PAGE_CHEATS?L"Cheats":state.page==PAGE_SETTINGS_HOME?L"Settings":state.page==PAGE_APPEARANCE?L"Appearance":state.page==PAGE_CONTROLS?L"Controls":state.page==PAGE_GAME_SETTINGS?L"Game & launcher":state.page==PAGE_TOOLS?L"Files & support":state.page==PAGE_SETTINGS?L"Settings":state.page==PAGE_SCREENSHOT?L"Screenshot saved":state.page==PAGE_RESET?L"Reset settings?":state.page==PAGE_RESTART?L"Restart game?":menu_power_page(&state)?(state.shutdown_countdown?(state.page==PAGE_REBOOT?L"Restarting PC":L"Shutting down PC"):(state.page==PAGE_REBOOT?L"Restart PC?":L"Shut down PC?")):L"UFC 6",title,RGB(246,247,250),DT_LEFT|DT_SINGLELINE);
    wchar_t owner_label[64];
    if(owner_player>0)swprintf(owner_label,64,L"Player %d",owner_player);
    else wcscpy(owner_label,has_owner?L"Controller":state.first_player_only?L"Waiting for Player 1":L"Waiting for controller");
    RECT owner_rect={330,25,r.right-36,48};if(state.page==PAGE_MAIN||state.page==PAGE_CHEATS||state.page==PAGE_SETTINGS_HOME)text(dc,font_small,owner_label,owner_rect,paint_theme.muted,DT_RIGHT|DT_SINGLELINE);
    RECT sub = {38, 57, r.right - 36, 84};
    text(dc,font_small,conflict?L"Game Bar is using your controller.":state.page==PAGE_STARTUP?L"Skip title input and known offline network warnings.":state.page==PAGE_STARTUP_CONFIRM?L"Save progress first. The game will close and relaunch.":state.page==PAGE_PERFORMANCE?L"PORTHEON settings. Restart the game to apply.":state.page==PAGE_PERFORMANCE_RESTART?L"Save these settings and relaunch through PORTHEON.":state.page==PAGE_PERFORMANCE_DISCARD?L"Your performance edits have not been saved.":state.page==PAGE_PHOTO_EFFECTS?L"Live preview in Photo Mode. Left/right adjusts.":state.page==PAGE_PHOTO_WATERMARK?L"Uses your PORTHEON player name. Saved in photos.":state.page==PAGE_PHOTO?L"Free camera, zoom and freeze during a fight.":state.page==PAGE_PHOTO_CONTROLS?L"Close the overlay to control the camera. Esc exits.":state.page==PAGE_KEYBINDS?L"Assign keyboard keys and controller combinations.":state.page==PAGE_BINDING?L"Choose an input to record or clear.":state.page==PAGE_THEME?L"From config follows the launcher theme file.":state.page==PAGE_QUIT?L"Close UFC 6 normally through your launcher.":state.page==PAGE_BACKUPS?L"Save in UFC 6 first; wait for autosave to finish.":state.page==PAGE_GAME_ICONS?L"Replace in-game button icons with Xbox or PlayStation.":state.page==PAGE_GAME_ICONS_CONFIRM?L"Your game will close and restart through your launcher.":state.page==PAGE_CAREER_ADD?L"Career changes are included in normal game saves.":state.page==PAGE_CHEATS?L"Stamina and health apply to Player 1.":state.page==PAGE_SCREENSHOT?(state.screenshot_overlay?L"Game and overlay saved together.":L"Clean game frame saved. The overlay is excluded."):state.page==PAGE_RESET?L"Restore overlay and launcher defaults?":state.page==PAGE_SETTINGS_HOME?L"Choose a category.":state.page==PAGE_APPEARANCE?L"Theme, size and menu style.":state.page==PAGE_CONTROLS?L"Shortcuts and controller access.":state.page==PAGE_GAME_SETTINGS?L"Launcher, startup screens and game controller icons.":state.page==PAGE_TOOLS?L"Screenshots, updates, support and reset.":state.page==PAGE_SETTINGS?L"Customize your overlay":state.page==PAGE_RESTART?L"Close this session and use the saved launcher settings.":menu_power_page(&state)?(state.shutdown_countdown?L"You can still cancel before the countdown ends.":(state.page==PAGE_REBOOT?(menu_auto_launch(&state)?L"Game auto-launch: On. Change in Settings.":L"Game auto-launch: Off. Change in Settings."):L"Close the game and turn off this PC.")):L"What would you like to do?",sub,conflict?RGB(255,196,94):paint_theme.muted,DT_LEFT);
    const wchar_t *main_labels[]={L"Resume",L"Take screenshot",L"Restart game",L"Cheats",L"Photo Mode",L"Settings",L"Quit game",L"Restart PC",L"Shut down PC"};
    const wchar_t *setting_labels[]={L"Overlay button icons",L"Opening shortcut",L"UI scale",L"Launch UFC 6 after PC restart",L"Xbox / Guide button",L"Windows Game Bar shortcut",L"Launcher setup...",L"Controller access",L"Screenshots folder",L"Reset overlay settings...",L"Patch in-game controller icons...",L"Check for updates...",L"Export support report...",L"Keybinds...",L"Theme...",L"Back",L"Font size",L"Box style",L"Window size",L"Visual auto-confirm fallback",L"Skip intros",L"Include overlay in screenshots",L"Skip title & network warnings...",L"Mute when unfocused",L"Replay after decisions"};
    const wchar_t *photo_labels[]={state.photo_active?L"Continue camera":L"Enter Photo Mode",L"Freeze action",L"Hide HUD",L"Keep cage visible",L"Show controls panel",L"Reset camera",L"Take screenshot",L"Exit Photo Mode",L"Controls...",L"Filters & effects...",L"Watermark...",L"Back"};
    const wchar_t *effect_labels[]={L"Filter",L"Filter strength",L"Exposure",L"Contrast",L"Saturation",L"Warmth",L"Vignette",L"Film grain",L"Cinematic bars",L"Reset effects",L"Back"};
    const wchar_t *mark_labels[]={L"Show watermark",L"Player name",L"Corner",L"Opacity",L"Size",L"Style",L"Font",L"Text color",L"Back"};
    const wchar_t *photo_control_labels[]={L"Move: left stick / I J K L",L"Look: right stick / arrow keys",L"Height: bumpers / U O",L"Zoom: triggers / Page Up, Down",L"Freeze: confirm button / P",L"Screenshot: left face button",L"Reset: top face button / Home",L"Exit: cancel button / Esc",L"Hide HUD: D-pad up / H",L"Keep cage: D-pad right / C",L"Controls panel: D-pad down / Tab",L"Back"};
    const wchar_t *category_labels[]={L"Appearance...",L"Controls...",L"Game & launcher...",L"Files & support...",L"Performance...",L"Back"};
    const wchar_t *performance_labels[]={L"Crowd & animation",L"Particles & sparks",L"Depth of field",L"Dynamic resolution",L"3D render scale",L"Save for next launch",L"Save & restart game...",L"Back"};
    const wchar_t *performance_restart[]={L"Cancel",L"Save and restart game"},*performance_discard[]={L"Keep editing",L"Discard edits"};
    const wchar_t *screenshot_labels[]={L"Open screenshots folder",L"Back"};
    const wchar_t *cheat_labels[]={L"Infinite stamina",L"No damage",L"Next hit KO",L"Add money",L"Add skill points",L"Restore fitness to 100%",L"Refill training days",L"Never get old",L"Training point rewards",L"All sparring grades: A...",L"Savegame backups...",L"Back"};
    wchar_t backup_message[512];int backup_state=BACKUP_READY;
#ifdef QUIT_MENU_TEST
    if(test_mode)wcscpy(backup_message,L"Test backup page; save files are never touched.");else
#endif
    backup_state=save_backups_state(backup_message,512);
    const wchar_t *backup_labels[]={backup_state==BACKUP_BUSY?L"Creating backup...":L"Create savegame backup",L"Open backups folder",L"Back"};
    const wchar_t *startup_labels[]={L"Enable bypass",L"Restore startup screens",L"Back"};
    const wchar_t *game_icons_labels[]={L"Xbox game icons",L"PlayStation game icons",L"Back"};
    const wchar_t *game_icons_confirm_labels[]={L"Cancel",L"Apply and restart game"};
    const wchar_t *career_labels[]={L"Cancel",state.career_kind==CAREER_GRADES?L"Set all grades to A":state.career_kind==CAREER_DAYS?L"Refill now":state.career_kind==CAREER_FITNESS?L"Restore now":L"Add now"};
    const wchar_t *binding_labels[]={L"Keyboard key...",L"Controller combo...",L"Clear keyboard key",state.binding_action==BIND_OPEN?L"Reset controller combo":L"Clear controller combo",L"Back"};
    const wchar_t *quit_labels[]={L"Cancel",L"Quit game"};
    const wchar_t *reset_labels[]={L"Cancel",L"Reset now"};
    const wchar_t *restart_labels[]={L"Cancel",L"Restart now"};
    const wchar_t *shutdown_labels[]={state.shutdown_countdown?L"Cancel":L"Cancel",state.page==PAGE_REBOOT?L"Hold to restart PC":L"Hold to shut down"};
    int first=menu_first_row(&state);
    for(int i=0;i<menu_rows(&state);++i) {
        if(menu_scroll_page(state.page)&&(i<first||i>=first+menu_visible_rows(&state)))continue;
        int gi=menu_setting_id(state.page,i);
        int top,height;row_geometry(state.page,i,first,&top,&height);
        RECT row={36,top,r.right-36,top+height};
        int focused=i==selection;
        int danger=(state.page==PAGE_MAIN&&(i==MAIN_QUIT||i==MAIN_REBOOT||i==MAIN_SHUTDOWN))||(menu_power_page(&state)&&i==1);
        RECT surface=row;
        row_surface(dc,surface,bindings.box_style,bindings.box_style==1?0:bindings.box_style==3?height:10,focused?(danger?RGB(54,35,41):paint_theme.focus):(bindings.box_style==2?paint_theme.background:paint_theme.surface),focused?(danger?RGB(173,98,106):paint_theme.focus_border):paint_theme.border);
        if(focused){RECT accent={37,top+6,40,top+height-6};HBRUSH mark=CreateSolidBrush(danger?RGB(245,143,150):paint_theme.accent);FillRect(dc,&accent,mark);DeleteObject(mark);}
        COLORREF color=focused?RGB(244,250,246):danger?RGB(227,159,165):paint_theme.text;
        RECT control=row;
        row.left+=18;
        RECT label_row=row;
        if(state.page==PAGE_MAIN&&i==MAIN_REBOOT)label_row.right=330;
        if(menu_setting_page(state.page)&&gi==SETTING_SHORTCUT)label_row.right=280;
        else if(menu_setting_page(state.page)&&(gi==SETTING_ICONS||gi==SETTING_SCALE||gi==SETTING_ACCESS||gi==SETTING_FONT||gi==SETTING_BOX_STYLE||gi==SETTING_WINDOW_SIZE))label_row.right=345;
        else if((menu_setting_page(state.page)&&(gi==SETTING_SCREENSHOT_UI||gi==SETTING_STARTUP_POPUPS||gi==SETTING_SKIP_SPLASHES||gi==SETTING_AUTO_LAUNCH||gi==SETTING_GUIDE||gi==SETTING_GAMEBAR))||(state.page==PAGE_CHEATS&&(i<2||i==7)))label_row.right=410;
        else if(state.page==PAGE_CHEATS&&(i==2||i==3||i==4||i==8))label_row.right=300;
        if(state.page==PAGE_KEYBINDS&&i<BIND_COUNT)label_row.right=285;
        if(state.page==PAGE_BINDING&&i<2)label_row.right=280;
        if(state.page==PAGE_PHOTO_EFFECTS||state.page==PAGE_PHOTO_WATERMARK)label_row.right=300;
        static const int main_binding[]={-1,BIND_SCREENSHOT,BIND_RESTART,BIND_CHEATS,BIND_PHOTO,BIND_SETTINGS,BIND_QUIT,BIND_REBOOT,BIND_SHUTDOWN};
        int bound=state.page==PAGE_MAIN?main_binding[i]:state.page==PAGE_CHEATS&&i==10?BIND_BACKUP:-1;
        int has_binding=bound>=0&&(bindings.actions[bound].key||bindings.actions[bound].pad);
        if(state.page==PAGE_PERFORMANCE&&i<PERF_SAVE)label_row.right=i==PERF_SCALE?300:410;
        if(has_binding)label_row.right=300;
        if(state.page==PAGE_MAIN&&bindings.font_size==20){--label_row.top;++label_row.bottom;}
        text(dc,font_main,state.page==PAGE_PERFORMANCE?performance_labels[i]:state.page==PAGE_PERFORMANCE_RESTART?performance_restart[i]:state.page==PAGE_PERFORMANCE_DISCARD?performance_discard[i]:state.page==PAGE_PHOTO_EFFECTS?effect_labels[i]:state.page==PAGE_PHOTO_WATERMARK?mark_labels[i]:state.page==PAGE_PHOTO?photo_labels[i]:state.page==PAGE_PHOTO_CONTROLS?photo_control_labels[i]:state.page==PAGE_SETTINGS_HOME?category_labels[i]:state.page==PAGE_KEYBINDS?(i==BIND_COUNT?L"Back":binding_names[i]):state.page==PAGE_BINDING?binding_labels[i]:state.page==PAGE_THEME?theme_choices[i]:state.page==PAGE_QUIT?quit_labels[i]:state.page==PAGE_BACKUPS?backup_labels[i]:state.page==PAGE_STARTUP?startup_labels[i]:state.page==PAGE_STARTUP_CONFIRM?game_icons_confirm_labels[i]:state.page==PAGE_GAME_ICONS?game_icons_labels[i]:state.page==PAGE_GAME_ICONS_CONFIRM?game_icons_confirm_labels[i]:state.page==PAGE_CHEATS?cheat_labels[i]:state.page==PAGE_MAIN?main_labels[i]:menu_setting_page(state.page)?setting_labels[gi]:state.page==PAGE_SCREENSHOT?screenshot_labels[i]:state.page==PAGE_RESET?reset_labels[i]:state.page==PAGE_CAREER_ADD?career_labels[i]:menu_power_page(&state)?shutdown_labels[i]:restart_labels[i],label_row,color,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
        if(has_binding)binding_chip(dc,control,&bindings.actions[bound],focused,ps,0,bound==BIND_REBOOT?(menu_auto_launch(&state)?L"Auto: On":L"Auto: Off"):NULL);
        if(state.page==PAGE_PERFORMANCE&&i<PERF_SAVE){
            if(!state.performance_available)value_chip(dc,control,L"Unavailable",focused,0);
            else if(i<PERF_SCALE){const int flags[]={state.performance.crowd_off,state.performance.particles_off,state.performance.dof_off,state.performance.drs_off};toggle_switch(dc,control,!flags[i],focused);}
            else{wchar_t scale[64];if(state.performance.render_scale)swprintf(scale,64,L"%d%%",state.performance.render_scale);else wcscpy(scale,L"Launcher default");value_chip(dc,control,scale,focused,1);}
        }
        if(state.page==PAGE_PHOTO_EFFECTS&&i<FX_RESET){wchar_t value[64];photo_effect_value(&state.effects,i,value,64);value_chip(dc,control,value,focused,1);}
        if(state.page==PAGE_PHOTO_WATERMARK&&i<MARK_BACK){wchar_t value[64];PhotoEffects *p=&state.effects;
            if(i==MARK_ENABLED)toggle_switch(dc,control,p->watermark&&p->name[0],focused);
            else value_chip(dc,control,photo_mark_value(p,i,value,64),focused,i!=MARK_NAME);}
        if(state.page==PAGE_PHOTO&&i==2)toggle_switch(dc,control,state.photo_hud,focused);
        if(state.page==PAGE_PHOTO&&i==3)toggle_switch(dc,control,state.photo_cage,focused);
        if(state.page==PAGE_PHOTO&&i==4)toggle_switch(dc,control,!state.photo_help_hidden,focused);
        if(state.page==PAGE_PHOTO&&i==1)toggle_switch(dc,control,state.photo_frozen,focused);
        if(state.page==PAGE_PHOTO&&i==0)value_chip(dc,control,state.photo_active?L"Active":L"Off",focused,0);
        if(state.page==PAGE_PHOTO_CONTROLS&&i==8)controller_artwork_draw(dc,control.right-44,control.top+2,26,26,PAD_UP,ps);
        if(state.page==PAGE_PHOTO_CONTROLS&&i>=4&&i<=7){const uint32_t icons[]={PAD_A,0x8000,0x1000,PAD_B};controller_artwork_draw(dc,control.right-44,control.top+2,26,26,icons[i-4],ps);}
        if(state.page==PAGE_KEYBINDS&&i<BIND_COUNT)binding_chip(dc,control,&bindings.actions[i],focused,ps,i==BIND_OPEN,NULL);
        if(state.page==PAGE_BINDING&&i<2){
            if(state.capturing)value_chip(dc,control,L"Recording...",focused,0);
            else if(i==0){wchar_t label[100];binding_key_label(bindings.actions[state.binding_action].key,label,100);value_chip(dc,control,label,focused,0);}
            else{ActionBinding chord={.pad=bindings.actions[state.binding_action].pad};binding_chip(dc,control,&chord,focused,ps,0,NULL);}
        }
        if(state.page==PAGE_THEME&&i<6&&i==bindings.theme_choice)value_chip(dc,control,i==0?paint_theme.name:L"Selected",focused,0);
        if(state.page==PAGE_CHEATS&&i==8){wchar_t factor[32];int multiplier=menu_training_multiplier(&state);swprintf(factor,32,multiplier==1?L"1x (Off)":L"%dx",multiplier);value_chip(dc,control,factor,focused,1);}
        if(state.page==PAGE_CHEATS&&i==7)toggle_switch(dc,control,career_state.longevity_requested,focused);
        if(state.page==PAGE_CHEATS&&i==2)value_chip(dc,control,ko_state==1?L"Armed / Cancel":L"Arm",focused,0);
        if(state.page==PAGE_CHEATS&&i<2)toggle_switch(dc,control,cheat_enabled[i],focused);
        if(state.page==PAGE_CHEATS&&(i==3||i==4)){
            wchar_t amount[32],value_text[64];format_number(menu_career_amount(&state,i-3),amount);
            int pending=career_state.status==CAREER_PENDING&&career_state.kind==i-3;
            swprintf(value_text,64,pending?L"Pending...":i==3?L"$%ls":L"%ls",amount);
            value_chip(dc,control,value_text,focused,!pending);
        }
        if(menu_power_page(&state)&&i==1&&state.shutdown_holding){
            wchar_t progress[40];swprintf(progress,40,L"%.1f / 3 s",state.shutdown_progress/1000.0);
            RECT value={330,row.top+5,r.right-55,row.bottom};text(dc,font_small,progress,value,color,DT_RIGHT|DT_SINGLELINE);
            RECT bar={36,top+height-4,36+(r.right-72)*state.shutdown_progress/3000,top+height};
            HBRUSH fill_bar=CreateSolidBrush(RGB(221,246,197));FillRect(dc,&bar,fill_bar);DeleteObject(fill_bar);
        }
        if(state.page==PAGE_MAIN&&i==MAIN_REBOOT&&!has_binding){
            int on=menu_auto_launch(&state),middle=(control.top+control.bottom)/2;
            RECT dot={control.right-145,middle-3,control.right-139,middle+3};
            rounded_box(dc,dot,6,on?RGB(113,211,141):RGB(128,141,157),on?RGB(113,211,141):RGB(128,141,157));
            RECT value={control.right-134,control.top,control.right-14,control.bottom};
            text(dc,font_small,on?L"Auto-launch: On":L"Auto-launch: Off",value,focused?RGB(229,234,242):RGB(168,184,202),DT_RIGHT|DT_VCENTER|DT_SINGLELINE);
        }
        if(menu_setting_page(state.page)&&gi==SETTING_DECISION_REPLAY)toggle_switch(dc,control,state.decision_replays,focused);
        if(menu_setting_page(state.page)&&gi==SETTING_AUDIO_FOCUS)toggle_switch(dc,control,state.mute_background,focused);
        if(menu_setting_page(state.page)&&gi==SETTING_SCREENSHOT_UI)toggle_switch(dc,control,state.screenshot_include_ui,focused);
        if(menu_setting_page(state.page)&&(gi==SETTING_STARTUP_POPUPS||gi==SETTING_SKIP_SPLASHES))toggle_switch(dc,control,gi==SETTING_STARTUP_POPUPS?state.startup_popups:state.skip_splashes,focused);
        if(menu_setting_page(state.page)&&gi==SETTING_AUTO_LAUNCH)toggle_switch(dc,control,menu_auto_launch(&state),focused);
        if(menu_setting_page(state.page)&&(gi==SETTING_GUIDE||gi==SETTING_GAMEBAR)){
            if(gi==SETTING_GAMEBAR&&state.gamebar_shortcut<0)value_chip(dc,control,L"Unavailable",focused,0);
            else toggle_switch(dc,control,gi==SETTING_GUIDE?menu_guide_enabled(&state):state.gamebar_shortcut,focused);
        }
        if(menu_setting_page(state.page)&&gi==SETTING_ACCESS)value_chip(dc,control,state.first_player_only?L"Player 1 only":L"Either",focused,1);
        if(menu_setting_page(state.page)&&gi==SETTING_ICONS)value_chip(dc,control,ps?L"PlayStation":L"Xbox",focused,1);
        if(menu_setting_page(state.page)&&gi==SETTING_SHORTCUT){ActionBinding chord={.pad=menu_shortcut(&state)};if(state.capturing)value_chip(dc,control,L"Recording...",focused,0);else binding_chip(dc,control,&chord,focused,ps,0,NULL);}
        if(menu_setting_page(state.page)&&gi==SETTING_FONT){wchar_t size[32];swprintf(size,32,L"%d px",16+state.font_index*2);value_chip(dc,control,size,focused,1);}
        if(menu_setting_page(state.page)&&gi==SETTING_WINDOW_SIZE){const wchar_t *sizes[]={L"Compact (6 rows)",L"Expanded (9 rows)",L"Tall (12 rows)"};value_chip(dc,control,sizes[state.window_size],focused,1);}
        if(menu_setting_page(state.page)&&gi==SETTING_BOX_STYLE){const wchar_t *styles[]={L"Rounded",L"Square",L"Outline",L"Pill",L"Octagon"};value_chip(dc,control,styles[state.box_style],focused,1);}
        if(menu_setting_page(state.page)&&gi==SETTING_SCALE){wchar_t scale[40];swprintf(scale,40,L"%d%%",menu_scale(&state));value_chip(dc,control,scale,focused,1);}
        if(state.page==PAGE_STARTUP&&((i==0&&startup_state==GAME_ICONS_XBOX)||(i==1&&startup_state==GAME_ICONS_PS)))value_chip(dc,control,L"Active",focused,0);
        if(state.page==PAGE_GAME_ICONS&&((i==0&&icon_state==GAME_ICONS_XBOX)||(i==1&&icon_state==GAME_ICONS_PS)))value_chip(dc,control,L"Active",focused,0);
        int simple=(state.page==PAGE_PERFORMANCE&&i>=PERF_SAVE)||(state.page==PAGE_MAIN&&i!=MAIN_REBOOT&&!has_binding)||(menu_setting_page(state.page)&&(gi==SETTING_LAUNCHER||gi==SETTING_SCREENSHOTS||gi==SETTING_RESET||gi==SETTING_GAME_ICONS||gi==SETTING_STARTUP_PATCH||gi==SETTING_UPDATE||gi==SETTING_DIAGNOSTICS||gi==SETTING_KEYBINDS||gi==SETTING_THEME||gi==SETTING_BACK))||(state.page==PAGE_CHEATS&&i>=5&&i!=7&&i!=8)||state.page==PAGE_BACKUPS;
        if(simple&&!has_binding)arrow_mark(dc,control.right-21,(control.top+control.bottom)/2,1,focused?paint_theme.accent:RGB(112,132,155));
    }
    if(state.page==PAGE_CHEATS&&first<=3){RECT heading={38,180-32*first,r.right-36,204-32*first};text(dc,font_small,L"Career cheats",heading,paint_theme.muted,DT_LEFT|DT_SINGLELINE);}
    if(state.page==PAGE_PERFORMANCE_RESTART||state.page==PAGE_PERFORMANCE_DISCARD){
        RECT explain={36,98,r.right-36,156};text(dc,font_small,state.page==PAGE_PERFORMANCE_RESTART?L"Save your game progress first. Settings are applied at startup.":L"Discarding keeps the settings currently saved in PORTHEON.",explain,paint_theme.muted,DT_LEFT|DT_WORDBREAK);}
    if(state.page==PAGE_STARTUP_CONFIRM){
        RECT scope={36,98,r.right-36,132},backup={36,133,r.right-36,171};
        text(dc,font_small,state.startup_patch_enable?L"No image matching or button presses are needed.":L"Restore the original title and offline-warning scripts.",scope,paint_theme.text,DT_LEFT|DT_WORDBREAK);
        text(dc,font_small,L"Original scripts are backed up and verified locally.",backup,paint_theme.muted,DT_LEFT|DT_WORDBREAK);
    }
    if(state.page==PAGE_GAME_ICONS_CONFIRM){
        RECT scope={36,98,r.right-36,126},backup={36,133,r.right-36,159};
        text(dc,font_small,state.game_icons_xbox?L"Use Xbox prompts throughout the game.":L"Restore the original PlayStation game prompts.",scope,RGB(234,238,244),DT_CENTER|DT_SINGLELINE);
        text(dc,font_small,state.game_icons_xbox?L"Original artwork is backed up before any changes.":L"Uses your verified local restore backup.",backup,paint_theme.muted,DT_CENTER|DT_SINGLELINE);
    }
    if(state.page==PAGE_CAREER_ADD){
        wchar_t amount[32],balance[32],line[100];int kind=state.career_kind;
        format_number(kind==CAREER_GRADES?0:kind==CAREER_DAYS?7:kind==CAREER_FITNESS?100:menu_career_amount(&state,kind),amount);format_number(kind==CAREER_GRADES?0:kind==CAREER_DAYS?career_state.training_days:kind==CAREER_FITNESS?career_state.fitness/100:career_state.balance[kind],balance);
        swprintf(line,100,kind==CAREER_GRADES?L"Set every sparring challenge pack to Grade A":kind==CAREER_DAYS?L"Restore 7 days in this training week":kind==CAREER_FITNESS?L"Set this career fighter to 100%% fitness":kind==0?L"Add $%ls to this career":L"Add %ls skill points to this career",amount);
        RECT label={36,94,r.right-36,128},current={36,133,r.right-36,159};text(dc,font_body,line,label,RGB(234,238,244),DT_CENTER|DT_SINGLELINE);
        swprintf(line,100,(kind==CAREER_DAYS?career_state.training_ready:kind==CAREER_FITNESS?career_state.fitness_ready:career_state.ready)?(kind==CAREER_FITNESS?L"Current fitness: %ls%%. Applies once; save normally.":kind==0?L"Current balance: $%ls":L"Current balance: %ls skill points"):L"Open the career hub to load your values.",balance);
        if(kind==CAREER_DAYS)swprintf(line,100,career_state.training_ready?L"Days remaining: %u. Week %u. Applies once.":L"Open an active training camp to refill days.",career_state.training_days,career_state.training_week);
        if(kind==CAREER_GRADES)swprintf(line,100,L"Includes unattempted packs. Save normally.");
        text(dc,font_small,line,current,paint_theme.muted,DT_CENTER|DT_SINGLELINE);
    }else if(state.page==PAGE_SCREENSHOT){
        RECT label={36,94,r.right-36,117},name={36,121,r.right-36,161};
        text(dc,font_small,L"Saved file",label,paint_theme.muted,DT_CENTER|DT_SINGLELINE);
        text(dc,font_small,filename,name,RGB(234,238,244),DT_CENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
    }else if(state.page==PAGE_RESET){
        RECT scope={36,97,r.right-36,123},keep={36,130,r.right-36,159};
        text(dc,font_small,L"Restores overlay preferences and launcher settings.",scope,RGB(234,238,244),DT_CENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
        text(dc,font_small,L"Windows Game Bar preference and screenshots are kept.",keep,paint_theme.muted,DT_CENTER|DT_SINGLELINE);
    }
    if(menu_scroll_page(state.page)){
        COLORREF color=paint_theme.muted;HBRUSH fill=CreateSolidBrush(color);HPEN pen=CreatePen(PS_SOLID,1,color);
        HGDIOBJ old_brush=SelectObject(dc,fill),old_pen=SelectObject(dc,pen);
        POINT above[]={{543,103},{555,103},{549,94}},below[]={{543,252+extra},{555,252+extra},{549,261+extra}};
        if(first>0)Polygon(dc,above,3);
        if(first+menu_visible_rows(&state)<menu_rows(&state))Polygon(dc,below,3);
        SelectObject(dc,old_brush);SelectObject(dc,old_pen);DeleteObject(fill);DeleteObject(pen);
    }
    if(menu_power_page(&state)&&state.shutdown_countdown){
        wchar_t countdown[40];swprintf(countdown,40,L"%d seconds",state.shutdown_seconds);
        RECT number={36,187,r.right-36,239};text(dc,font_title,countdown,number,RGB(246,247,250),DT_CENTER|DT_SINGLELINE);
    }
    int y=(state.page==PAGE_MAIN?314:state.page==PAGE_CHEATS?310:279)+extra;
    if(conflict){
        wchar_t chord[100],fallback[150];shortcut_label(menu_shortcut(&state),ps,chord,100);
        swprintf(fallback,150,L"Fallback: %ls  |  F10",chord);
        RECT help={30,y,r.right-30,y+22},keys={30,y+22,r.right-30,y+44};
        text(dc,font_small,L"Press Guide again or click the game to restore input.",help,RGB(255,196,94),DT_CENTER|DT_SINGLELINE);
        text(dc,font_small,fallback,keys,RGB(255,196,94),DT_CENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
    }else if(state.key_capturing){
        RECT prompt={36,279+extra,r.right-36,307+extra},cancel={36,308+extra,r.right-36,333+extra};
        text(dc,font_small,L"Press a key, with optional Ctrl / Alt / Shift, then release.",prompt,paint_theme.text,DT_CENTER|DT_SINGLELINE);
        text(dc,font_small,L"Esc cancels. F10 remains the recovery shortcut.",cancel,paint_theme.muted,DT_CENTER|DT_SINGLELINE);
    }else if(state.capturing) {
        RECT prompt={36,279+extra,r.right-36,307+extra};
        text(dc,font_small,state.capture_error?L"Use 2 to 4 buttons; hold briefly, then release.":L"Hold 2 to 4 buttons, then release to save.",prompt,RGB(220,231,245),DT_CENTER|DT_SINGLELINE);
        RECT cancel={36,308+extra,r.right-36,333+extra};
        text(dc,font_small,ps?L"Circle alone cancels. F10 always closes the overlay.":L"B alone cancels. F10 always closes the overlay.",cancel,RGB(158,170,188),DT_CENTER|DT_SINGLELINE);
    } else {
        dpad_icon(dc,44,y,ps);
        RECT select={84,y+5,190,y+28};text(dc,font_small,L"Select",select,RGB(187,196,209),DT_LEFT);
        button_icon(dc,210,y,0,ps);
        int setting=menu_setting_id(state.page,selection);
        const wchar_t *confirm=state.page==PAGE_CHEATS?(selection==2?(ko_state==1?L"Cancel":L"Arm"):(selection<2||selection==7||selection==8)?L"Change":selection>=3&&selection<5?L"Add...":L"Confirm"):menu_setting_page(state.page)?(setting==SETTING_SHORTCUT?L"Record":(setting==SETTING_DECISION_REPLAY||setting==SETTING_AUDIO_FOCUS||setting==SETTING_SCREENSHOT_UI||setting==SETTING_ICONS||setting==SETTING_AUTO_LAUNCH||setting==SETTING_GUIDE||setting==SETTING_GAMEBAR||setting==SETTING_ACCESS||setting==SETTING_FONT||setting==SETTING_BOX_STYLE||setting==SETTING_WINDOW_SIZE)?L"Change":setting==SETTING_SCALE?L"Reset":L"Confirm"):menu_power_page(&state)?(state.shutdown_countdown?L"Cancel":selection==1?L"Hold 3s":L"Confirm"):L"Confirm";
        RECT yes={250,y+5,367,y+28};text(dc,font_small,confirm,yes,RGB(187,196,209),DT_LEFT);
        button_icon(dc,386,y,1,ps);
        RECT back={426,y+5,530,y+28};text(dc,font_small,state.page==PAGE_MAIN?L"Resume":L"Back",back,RGB(187,196,209),DT_LEFT);
        if(state.page==PAGE_KEYBINDS||state.page==PAGE_BINDING||state.page==PAGE_THEME){RECT hint={30,314+extra,r.right-30,338+extra};
            text(dc,font_small,state.page==PAGE_THEME?L"ufc6-overlay-theme.cfg | Presets and #RRGGBB colors":L"Hold combos 0.8s. Exit and power actions ask first.",hint,paint_theme.muted,DT_CENTER|DT_SINGLELINE|DT_END_ELLIPSIS);}
        if(menu_setting_page(state.page)) {
            int selection=menu_setting_id(state.page,state.selection);
            wchar_t chord[100],line[130];shortcut_label(menu_shortcut(&state),ps,chord,100);
            swprintf(line,130,L"Open: %ls  |  F10",chord);
            if(selection==SETTING_DECISION_REPLAY)wcscpy(line,L"Enable before a new offline fight. Requires recorded clips.");
            if(selection==SETTING_AUDIO_FOCUS)wcscpy(line,L"Silence only UFC 6 while minimized or in the background.");
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
            if(selection==SETTING_SCREENSHOT_UI)wcscpy(line,L"Optional: use normal screenshot controls to capture the overlay.");
            if(selection==SETTING_KEYBINDS)wcscpy(line,L"Configure action shortcuts for keyboard and controller.");
            if(selection==SETTING_THEME)wcscpy(line,L"Launcher config or a saved theme preset.");
            if(selection==SETTING_WINDOW_SIZE)wcscpy(line,L"Show more rows without increasing text size.");
            if(selection==SETTING_FONT)wcscpy(line,L"Change text size consistently across overlay pages.");
            if(selection==SETTING_BOX_STYLE)wcscpy(line,L"Rounded, square, outline, pill or octagonal menu rows.");
            if(selection==SETTING_RESET)wcscpy(line,L"Restore overlay defaults. Windows Game Bar is kept.");
            RECT shortcut={30,314+extra,r.right-30,338+extra};text(dc,font_small,line,shortcut,RGB(158,170,188),DT_CENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
        }
    }
    if(state.page==PAGE_CHEATS&&!conflict){
        wchar_t balances[120],money[32],points[32];format_number(career_state.balance[0],money);format_number(career_state.balance[1],points);
        swprintf(balances,120,career_state.ready?L"Money: $%ls  |  Skill points: %ls":L"Open a career screen to load money and skill points.",money,points);
        const wchar_t *help=selection>=3&&selection<5?balances:selection<2&&cheat_states[selection]==LIVE_LAUNCHER?L"Disable in PORTHEON, then restart once to use live control.":selection<2&&(cheat_states[selection]==LIVE_UNSUPPORTED||cheat_states[selection]==LIVE_ERROR)?L"This game version could not be verified for live cheats.":L"Player 1 follows the controller, even when fighters move.";
        if(!message[0])lstrcpynW(message,selection==2?L"Arm for 25 seconds. Land a head strike; confirm again to cancel.":selection==5?L"Set career fitness to 100% once. Save normally afterward.":selection==6?L"Restore 7 training days once, without advancing the week.":selection==7?L"Saved choice. Activates when a valid career loads.":selection==8?L"Multiplies sparring challenge evolution-point rewards.":selection==9?L"Upgrade completed sparring records to A for simulation.":selection==10?L"Back up your saved progress before using career cheats.":help,120);
    }
    if(state.page==PAGE_BACKUPS&&!conflict){
        RECT info={38,226,r.right-38,274};
        text(dc,font_small,backup_message,info,backup_state==BACKUP_ERROR?RGB(244,157,160):paint_theme.muted,DT_CENTER|DT_WORDBREAK);
        RECT hint={20,314+extra,r.right-20,338+extra};text(dc,font_small,L"Unsaved progress is not included.",hint,RGB(158,170,188),DT_CENTER|DT_SINGLELINE);
        if(!message[0])wcscpy(message,L"Documents / UFC6 Actions Overlay / SaveBackups");
    }
    if(state.page==PAGE_GAME_ICONS&&!conflict){
        const wchar_t *label=icon_state==GAME_ICONS_PS?L"Currently installed: PlayStation":icon_state==GAME_ICONS_XBOX?L"Currently installed: Xbox":icon_state==GAME_ICONS_MIXED?L"Mixed icons: choose a theme to repair":icon_state==GAME_ICONS_CHECKING?L"Checking installed game icons...":L"Game icons unavailable; see the message below.";
        RECT hint={20,314+extra,r.right-20,338+extra};text(dc,font_small,label,hint,paint_theme.muted,DT_CENTER|DT_SINGLELINE);
    }
    if(state.page==PAGE_PERFORMANCE){
        static const wchar_t *hints[]={L"Turns off both crowd rendering and crowd animation.",L"Controls sparks, sweat spray, smoke and other emitters.",L"Controls the game's depth-of-field blur.",L"On lets the game vary resolution and restores launcher scale.",L"Fixed scale turns dynamic resolution Off. GPU limits still apply.",L"Preserves unrelated launcher settings and game files.",L"Save progress first. The next screen asks before restarting.",L"Unsaved edits ask before being discarded."};
        RECT hint={20,314+extra,r.right-20,338+extra};text(dc,font_small,hints[selection],hint,paint_theme.muted,DT_CENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
        wcscpy(message,!state.performance_available?L"PORTHEON launcher/settings not found.":state.performance_feedback==3?L"Settings changed in PORTHEON. Reopen this page to reload.":state.performance_feedback==2?L"Could not save. Check folder access and retry.":state.performance_feedback==4?L"Saved. Restart the game through PORTHEON to apply.":state.performance_dirty?L"Unsaved changes. Current gameplay is unchanged.":state.performance_feedback==1?L"Saved for next launch. Restart the game to apply.":L"Changes take effect after the game restarts.");}
    RECT note = {20, 345+extra, r.right - 20, 369+extra};
    text(dc,font_small,state.page==PAGE_PERFORMANCE_RESTART?L"Unsaved game progress may be lost.":state.page==PAGE_PERFORMANCE_DISCARD?L"Keep editing preserves your pending choices.":conflict?L"In Settings, turn Windows Game Bar shortcut Off.":menu_power_page(&state)?(state.shutdown_countdown?L"Back or F10 cancels. Switching apps also cancels.":L"Unsaved game progress may be lost."):message[0]?message:state.page==PAGE_STARTUP?startup_reason:state.page==PAGE_GAME_ICONS?icon_reason:save_failed?L"Settings could not be saved.":state.page==PAGE_RESTART?L"Unsaved progress may be lost.":state.page==PAGE_CHEATS&&selection>=3&&selection<5?(career_state.status==CAREER_PENDING?L"Pending: resume and reopen a career screen.":career_state.status==CAREER_ADDED?L"Added once. The game saves changes normally.":L"Left/right: amount. Confirm: review addition."):(state.photo_active||state.page==PAGE_PHOTO||state.page==PAGE_PHOTO_CONTROLS)?L"Esc exits Photo Mode. F10 opens the tools.":L"The game keeps running while this menu is open.",note,
         RGB(137,148,164), DT_CENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
}
static void paint(HWND w) {
    PAINTSTRUCT ps;
    HDC target = BeginPaint(w, &ps);
    RECT r; GetClientRect(w, &r);
    HDC dc = CreateCompatibleDC(target);
    HBITMAP bitmap = CreateCompatibleBitmap(target, r.right, r.bottom);
    HGDIOBJ previous = SelectObject(dc, bitmap);
    SetMapMode(dc,MM_ANISOTROPIC);SetWindowExtEx(dc,MENU_BASE_WIDTH,(int)menu_canvas_height(&menu),NULL);
    SetViewportExtEx(dc,r.right,r.bottom,NULL);RECT logical={0,0,MENU_BASE_WIDTH,(LONG)menu_canvas_height(&menu)};draw_menu(dc,logical);
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
    return (BOOL)InterlockedExchange(&screenshot_request,0);
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
    int included=InterlockedCompareExchange(&screenshot_with_overlay,0,0);
    EnterCriticalSection(&lock);menu.screenshot_overlay=included;LeaveCriticalSection(&lock);
    InterlockedExchange(&screenshot_busy,0);
    if(saved&&filename[0]){swprintf(feedback,256,L"Screenshot saved: %ls",filename);status(feedback);}
    else status(saved?L"Saved to Windows Pictures / Screenshots.":L"Screenshot could not be saved.");
    log_line(saved?(included?"Game screenshot with overlay saved.":"Clean game screenshot saved."):"Screenshot capture failed.");
    log_timeline(saved?"screenshot_completed":"screenshot_failed");
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
    EnterCriticalSection(&lock);int gamebar=menu.gamebar_shortcut,legacy=menu.settings_legacy;
    menu=(MenuState){.visible=1,.page=legacy?PAGE_SETTINGS:PAGE_TOOLS,.selection=legacy?SETTING_RESET:4,.gamebar_shortcut=gamebar,.settings_legacy=legacy};
    menu.decision_replays=GetPrivateProfileIntW(L"Menu",L"OfferDecisionReplays",0,settings_path)!=0;
    menu.mute_background=GetPrivateProfileIntW(L"Menu",L"MuteWhenUnfocused",0,settings_path)!=0;
    menu.replay_step_index=GetPrivateProfileIntW(L"Menu",L"ReplayStepIndex",0,settings_path);if(menu.replay_step_index<0||menu.replay_step_index>6)menu.replay_step_index=0;
    photo_effects_load(&menu.effects,settings_path);
    reset_menu_owner();preferences_load(&preferences,settings_path,menu_shortcut(&menu));menu.theme_selection=preferences.theme_choice;menu.font_index=(preferences.font_size-16)/2;menu.box_style=preferences.box_style;menu.window_size=preferences.window_size;theme_load(&current_theme,theme_path,preferences.theme_choice);settings_save_failed=0;
    LeaveCriticalSection(&lock);
    status(L"Overlay defaults restored. Windows Game Bar setting kept.");
}
static void request_screenshot_mode(int included) {
    if(InterlockedCompareExchange(&screenshot_capable,0,0)!=1){status(L"Screenshot is unavailable for this display mode.");return;}
    if(InterlockedCompareExchange(&screenshot_busy,1,0))return;
    log_timeline(included?"screenshot_request_ui":"screenshot_request_clean");
    InterlockedExchange(&screenshot_with_overlay,included);
    if(!included)status(L"Taking screenshot...");
    InterlockedExchange(&screenshot_request,included?2:1);
}
static void request_screenshot(void){EnterCriticalSection(&lock);int include_ui=menu.screenshot_include_ui;LeaveCriticalSection(&lock);request_screenshot_mode(include_ui);}
#include "photo_help_panel.h"
static uint32_t photo_help_top(uint32_t height){uint32_t top=height*18/100;if(top<24)top=24;if(height>32&&top>height-32)top=height-32;return top;}
static int photo_help_visible(void){return !menu.visible&&menu.photo_active&&!menu.photo_help_hidden;}
static int notification_visible(void){return !menu.visible&&GetTickCount64()<toast_until&&status_text[0];}
#ifdef QUIT_MENU_TEST
__declspec(dllexport) void MenuTestEffects(const PhotoEffects *p,int active){EnterCriticalSection(&lock);if(p)menu.effects=*p;menu.photo_active=active;menu.photo_help_hidden=active==2?0:1;menu.visible=0;menu.draining=0;toast_until=0;test_photo_help_override=active;LeaveCriticalSection(&lock);}
#endif
#ifdef QUIT_MENU_TEST
__declspec(dllexport) int MenuTestPhotoPanel(uint32_t pad,unsigned keys){EnterCriticalSection(&lock);int save=photo_panel_controls(&menu,&pad,keys);int result=menu.photo_panel_page;if(save==PHOTO_PANEL_EFFECTS_CHANGED)action(MENU_PHOTO_EFFECTS_CHANGED);else if(save==PHOTO_PANEL_REPLAY_STEP_CHANGED)save_settings();LeaveCriticalSection(&lock);return result;}
#endif
__declspec(dllexport) BOOL Ufc6PhotoEffects(PhotoEffects *out){if(!out)return FALSE;EnterCriticalSection(&lock);*out=menu.effects;int active=menu.photo_active&&(test_mode||foreground());LeaveCriticalSection(&lock);return active;}
#include "photo_watermark_raster.h"
__declspec(dllexport) uint32_t Ufc6PhotoWatermark(void *out,uint32_t capacity){EnterCriticalSection(&lock);uint32_t result=photo_watermark_raster(&menu.effects,out,capacity);LeaveCriticalSection(&lock);return result;}
__declspec(dllexport) BOOL Ufc6OverlayBlend(void){EnterCriticalSection(&lock);int blend=photo_help_visible();LeaveCriticalSection(&lock);return blend;}
__declspec(dllexport) void Ufc6OverlayPosition(uint32_t viewport_width,uint32_t viewport_height,uint32_t *x,uint32_t *y){
    EnterCriticalSection(&lock);int help=photo_help_visible(),toast=notification_visible()&&!help;uint32_t width=render_width,height=render_height;
    *x=help?(viewport_width>width+24?24:0):viewport_width>width?(toast?(viewport_width-width>24?viewport_width-width-24:0):(viewport_width-width)/2):0;
    *y=help?photo_help_top(viewport_height):toast?24:viewport_height>height?(viewport_height-height)/2:0;LeaveCriticalSection(&lock);
}
static void draw_notification(HDC dc,RECT r){
    wchar_t message[120];EnterCriticalSection(&lock);paint_theme=current_theme;frame_font_index=(preferences.font_size-16)/2;lstrcpynW(message,status_text,120);LeaveCriticalSection(&lock);
    HBRUSH brush=CreateSolidBrush(paint_theme.background);FillRect(dc,&r,brush);DeleteObject(brush);SetBkMode(dc,TRANSPARENT);
    RECT edge={0,0,4,r.bottom};brush=CreateSolidBrush(paint_theme.accent);FillRect(dc,&edge,brush);DeleteObject(brush);
    RECT label={20,12,r.right-16,r.bottom-10};text(dc,font_main,message,label,paint_theme.text,DT_LEFT|DT_VCENTER|DT_WORDBREAK);
}
__declspec(dllexport) void Ufc6MenuExtent(uint32_t viewport_width,uint32_t viewport_height,uint32_t *width,uint32_t *height) {
    EnterCriticalSection(&lock);
    overlay_extent_for_height(menu_scale(&menu),menu_canvas_height(&menu),viewport_width,viewport_height,width,height);
    if((photo_help_visible()||notification_visible())&&viewport_width>32&&viewport_height>32){
        uint32_t w=(photo_help_visible()?PHOTO_HELP_WIDTH:420)*(uint32_t)menu_scale(&menu)/100,h=(photo_help_visible()?PHOTO_HELP_HEIGHT:80)*(uint32_t)menu_scale(&menu)/100;
        if(w>viewport_width-32){h=(uint32_t)((uint64_t)h*(viewport_width-32)/w);w=viewport_width-32;}
        uint32_t max_height=photo_help_visible()?viewport_height-photo_help_top(viewport_height)-16:viewport_height-32;
        if(h>max_height){w=(uint32_t)((uint64_t)w*max_height/h);h=max_height;}
        *width=w;*height=h;
    }
    render_width=*width;render_height=*height;
    LeaveCriticalSection(&lock);
}
static uint64_t raster_hash(uint64_t hash,const void *data,size_t length){
    const unsigned char *bytes=data;for(size_t i=0;i<length;++i){hash^=bytes[i];hash*=1099511628211ULL;}return hash;
}
static uint64_t menu_raster_signature(void){
    uint64_t hash=14695981039346656037ULL;
    EnterCriticalSection(&lock);
    hash=raster_hash(hash,&menu,sizeof(menu));hash=raster_hash(hash,&preferences,sizeof(preferences));hash=raster_hash(hash,&current_theme,sizeof(current_theme));hash=raster_hash(hash,&career,sizeof(career));
    hash=raster_hash(hash,live_status,sizeof(live_status));hash=raster_hash(hash,live_enabled,sizeof(live_enabled));
    int owner[]={raw_navigation,raw_owner,menu_device,gamebar_conflict,settings_save_failed,GetTickCount64()<status_until,GetTickCount64()<toast_until,
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
    if(!test_mode){state=startup_assets_status(message,512);hash=raster_hash(hash,&state,sizeof(state));hash=raster_hash(hash,message,(wcslen(message)+1)*sizeof(wchar_t));}
    state=save_backups_state(message,512);
    hash=raster_hash(hash,&state,sizeof(state));hash=raster_hash(hash,message,(wcslen(message)+1)*sizeof(wchar_t));
    return hash;
}
__declspec(dllexport) BOOL Ufc6RenderMenu(void *destination,uint32_t size) {
    EnterCriticalSection(&lock);
    int help=photo_help_visible(),toast=notification_visible()&&!help;int visible=(menu.visible||toast||help)&&(test_mode||foreground())&&font_body!=NULL;
    uint32_t width=render_width,height=render_height;
    uint32_t canvas_height=menu_canvas_height(&menu);
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
    SetMapMode(dc,MM_ANISOTROPIC);SetWindowExtEx(dc,help?PHOTO_HELP_WIDTH:toast?420:MENU_BASE_WIDTH,help?PHOTO_HELP_HEIGHT:toast?80:(int)canvas_height,NULL);
    SetViewportExtEx(dc,(int)width*2,(int)height*2,NULL);
    RECT r={0,0,help?PHOTO_HELP_WIDTH:toast?420:MENU_BASE_WIDTH,help?PHOTO_HELP_HEIGHT:toast?80:(LONG)canvas_height};if(help)draw_photo_help(dc,r);else if(toast)draw_notification(dc,r);else draw_menu(dc,r);GdiFlush();
    SetMapMode(dc,MM_TEXT);SetMapMode(target,MM_TEXT);
    SetStretchBltMode(target,HALFTONE);SetBrushOrgEx(target,0,0,NULL);
    if(!StretchBlt(target,0,0,(int)width,(int)height,dc,0,0,(int)width*2,(int)height*2,SRCCOPY))return FALSE;
    GdiFlush();
    unsigned char *p=output_pixels;
    for(size_t i=0;i<bytes;i+=4){
        int delta=abs((int)p[i]-(int)GetBValue(paint_theme.background));int v=abs((int)p[i+1]-(int)GetGValue(paint_theme.background));if(v>delta)delta=v;v=abs((int)p[i+2]-(int)GetRValue(paint_theme.background));if(v>delta)delta=v;if(delta>80)delta=80;
        p[i+3]=(unsigned char)(help?184+71*delta/80:255);
    }
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
        x=(int)((int64_t)x*MENU_BASE_WIDTH/r.right);EnterCriticalSection(&lock);uint32_t logical_height=menu_canvas_height(&menu);LeaveCriticalSection(&lock);
        y=(int)((int64_t)y*logical_height/r.bottom);
        EnterCriticalSection(&lock);
        int selected=-1;
        int first=menu_first_row(&menu);
        for(int i=0;i<menu_rows(&menu);++i){
            if(menu_scroll_page(menu.page)&&(i<first||i>=first+menu_visible_rows(&menu)))continue;
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
        menu_show(&menu);if(photo_mode_blocks_input())menu.page=PAGE_PHOTO;menu_device=-1;wait_guide_release=0;
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
static void start_photo(void);
static void poll_photo(int active);
static void run_binding(int id){
    if(id<0||id>=BIND_COUNT||(!test_mode&&!foreground())||input_redirected()||launcher_setup_active)return;
    EnterCriticalSection(&lock);int blocked=menu.capturing||menu.draining||menu.shutdown_holding||menu.shutdown_countdown;LeaveCriticalSection(&lock);if(blocked)return;
    if(id==BIND_OPEN){toggle_menu();return;}
    if(id==BIND_SCREENSHOT){request_screenshot();return;}
    if(id==BIND_PHOTO){start_photo();return;}
    if(photo_mode_blocks_input())photo_mode_command(PHOTO_STOP);
    EnterCriticalSection(&lock);
    if(!menu.visible){menu_show(&menu);reset_menu_owner();neutralize();}
    menu.page=id==BIND_CHEATS?PAGE_CHEATS:id==BIND_SETTINGS?(menu.settings_legacy?PAGE_SETTINGS:PAGE_SETTINGS_HOME):id==BIND_BACKUP?PAGE_BACKUPS:id==BIND_RESTART?PAGE_RESTART:id==BIND_QUIT?PAGE_QUIT:id==BIND_REBOOT?PAGE_REBOOT:PAGE_SHUTDOWN;
    menu.selection=0;menu.armed=0;menu_cancel_shutdown(&menu);action(MENU_OPEN);
    if(id==BIND_BACKUP)action(MENU_BACKUP_CREATE);
    LeaveCriticalSection(&lock);
}
static void poll_action_bindings(uint64_t now,int active,int controller_active){
    unsigned char keys[256]={0};
#ifdef QUIT_MENU_TEST
    if(test_mode)memcpy(keys,test_keys,sizeof(keys));else
#endif
    {
        for(int k=0;k<256;++k){int needed=menu.key_capturing||k==VK_CONTROL||k==VK_MENU||k==VK_SHIFT;
            for(int id=0;id<BIND_COUNT&&!needed;++id)needed=(preferences.actions[id].key&255)==(unsigned)k;
            if(needed)keys[k]=(GetAsyncKeyState(k)&0x8000)!=0;
        }
    }
    if(menu.key_capturing){
        if(!active){key_capture_ready=0;captured_key=0;return;}
        int held=0;for(int k=8;k<256;++k)if(keys[k]&&k!=VK_SHIFT&&k!=VK_CONTROL&&k!=VK_MENU&&!(k>=VK_LSHIFT&&k<=VK_RMENU))held=1;
        if(!key_capture_ready){if(!held)key_capture_ready=1;return;}
        if(keys[VK_ESCAPE]){menu.capturing=menu.key_capturing=0;menu.armed=0;captured_key=0;return;}
        if(!captured_key&&held)for(int k=8;k<256;++k)if(keys[k]&&k!=VK_SHIFT&&k!=VK_CONTROL&&k!=VK_MENU&&!(k>=VK_LSHIFT&&k<=VK_RMENU)){
            captured_key=(uint32_t)k|(keys[VK_CONTROL]?KEY_CTRL:0)|(keys[VK_MENU]?KEY_ALT:0)|(keys[VK_SHIFT]?KEY_SHIFT:0);break;}
        if(captured_key&&!keys[captured_key&255]){uint32_t value=captured_key;captured_key=0;menu.capturing=menu.key_capturing=0;menu.armed=0;save_binding(menu.binding_action,1,value);}
        return;
    }
    int enabled=active&&!menu.capturing&&!menu.draining&&!launcher_setup_active&&!menu.shutdown_holding&&!menu.shutdown_countdown;
    for(int id=0;id<BIND_COUNT;++id){
        int fired=binding_poll_key(&key_bindings[id],preferences.actions[id].key,keys,enabled);
        if(id)for(int slot=0;slot<20;++slot){uint32_t buttons=0;int allowed=0;
            if(slot<16){buttons=devices[slot].used?devices[slot].buttons:0;allowed=devices[slot].used&&native_allowed(&devices[slot]);}
            else{int pad=slot-16;buttons=raw_connected[pad]?xinput_menu_buttons(raw_states[pad].Gamepad.wButtons):0;allowed=raw_connected[pad]&&raw_allowed(pad);}
            if(binding_poll_pad(&pad_bindings[id][slot],preferences.actions[id].pad,buttons,now,enabled&&controller_active&&allowed))fired=1;
        }
        if(fired)PostThreadMessageW(ui_thread,WM_BINDING_ACTION,id,0);
    }
}
/* Photo controls are polled from one controller only; never combine pads. */
static int photo_pad=-1,photo_armed,photo_last_state;
static uint32_t photo_previous;
static unsigned photo_previous_keys;
static float photo_axis(float value){float a=fabsf(value);return a<.18f?0:(value<0?-1:1)*fminf(1,(a-.18f)/.82f);}
static void start_photo(void){
    if(photo_mode_blocks_input()){photo_mode_command(PHOTO_STOP);return;}
    EnterCriticalSection(&lock);
    photo_pad=raw_owner>=0?16+raw_owner:menu_device>=0&&devices[menu_device].used?menu_device:-1;
    if(photo_pad<0){for(int i=0;i<4;++i)if(raw_connected[i]&&raw_allowed(i)){photo_pad=16+i;break;}}
    if(photo_pad<0){for(int i=0;i<16;++i)if(devices[i].used&&native_allowed(&devices[i])){photo_pad=i;break;}}
    ReplaySnapshot playback;replay_snapshot(&playback);
    int ready=!test_mode&&(playback.available||live_cheats_fight_ready());
    int result=photo_mode_start(ready);
    if(result>0){
        neutralize();if(menu.visible)action(menu_close(&menu,MENU_RESUME));
        photo_armed=0;photo_previous=0;photo_previous_keys=0;menu.photo_panel_page=0;
        status(L"Starting Photo Mode. Esc exits; F10 opens controls.");
    }else {
        status(result==PHOTO_START_BUILD?L"Photo Mode is unavailable on this engine build.":result==PHOTO_START_HOOKS?L"Photo Mode hooks could not load. Restart the game and retry.":L"Photo Mode needs a running fight or practice session.");
        log_line(result==PHOTO_START_HOOKS?"Photo Mode start rejected: native hook installation failed.":result==PHOTO_START_BUILD?"Photo Mode start rejected: unsupported engine.":"Photo Mode start rejected: no fresh fighter-presence observation.");
    }
    LeaveCriticalSection(&lock);
}
static void photo_freeze_action(void){
    ReplaySnapshot replay;replay_snapshot(&replay);
    if(replay.available)replay_toggle_pause();else photo_mode_command(PHOTO_FREEZE);
}
static int photo_key(int key){
    /* User shortcuts take priority over built-in photo controls. */
    for(int i=0;i<BIND_COUNT;++i)if(preferences.actions[i].key&&(preferences.actions[i].key&255)==(unsigned)key)return 0;
    return !!(GetAsyncKeyState(key)&0x8000);
}
static void poll_photo(int active){
#ifdef QUIT_MENU_TEST
    if(test_photo_help_override)return;
#endif
    PhotoSnapshot snap;photo_mode_snapshot(&snap);
    replay_offer_decisions(menu.decision_replays);
    static unsigned decision_events;unsigned events=replay_decision_events();if(events!=decision_events){decision_events=events;log_line("Decision replay fallback applied using validated native clips.");}
    menu.photo_active=photo_mode_blocks_input();replay_poll(active,menu.photo_active);
    ReplaySnapshot replay;replay_snapshot(&replay);menu.replay_ready=replay.available;menu.replay_paused=replay.paused;menu.replay_position_ms=replay.position_ms;menu.replay_duration_ms=replay.duration_ms;menu.replay_extended=replay.extended;menu.replay_range_failed=replay.range_failed;
    menu.photo_frozen=replay.available?replay.paused:snap.frozen;menu.photo_fov=snap.fov;menu.photo_hud=snap.hud_hidden;menu.photo_cage=snap.cage_visible;
    if(snap.state!=photo_last_state){
        char cage_log[180];snprintf(cage_log,sizeof(cage_log),"Photo Mode state=%d cage_hooks=%d panels=%d visible=%d writes=%d restored=%d",snap.state,snap.cage_available,snap.cage_count,snap.cage_visible,snap.cage_writes,snap.cage_restores);log_line(cage_log);
        log_timeline(snap.state==2?"photo_entered":snap.state==3?"photo_restored":"photo_state_other");
        if(snap.state==2)status(L"Photo Mode On. F10: tools. Esc: exit.");
        else if(photo_last_state==2&&snap.state==3)status(L"Photo Mode Off. Gameplay restored.");
        else if(snap.state<0)status(L"Photo Mode could not take camera control.");
        photo_last_state=snap.state;
    }
    PhotoInput in={0};uint32_t buttons=0;int connected=1;
    if(menu.photo_active&&photo_pad>=16){int slot=photo_pad-16;connected=raw_connected[slot]&&raw_allowed(slot);
        if(connected){XINPUT_GAMEPAD *p=&raw_states[slot].Gamepad;buttons=xinput_menu_buttons(p->wButtons);
            in.right=photo_axis(p->sThumbLX/32767.f);in.forward=photo_axis(p->sThumbLY/32767.f);
            in.yaw=-photo_axis(p->sThumbRX/32767.f);in.pitch=photo_axis(p->sThumbRY/32767.f);
            in.zoom=photo_axis(p->bLeftTrigger/255.f)-photo_axis(p->bRightTrigger/255.f);}}
    else if(menu.photo_active&&photo_pad>=0){InputDevice *p=&devices[photo_pad];connected=p->used&&native_allowed(p);
        if(connected){buttons=p->buttons;in.right=photo_axis((p->axes[0]-128)/127.f);in.forward=-photo_axis((p->axes[1]-128)/127.f);
            in.yaw=-photo_axis((p->axes[2]-128)/127.f);in.pitch=-photo_axis((p->axes[3]-128)/127.f);
            in.zoom=photo_axis(p->axes[4]/255.f)-photo_axis(p->axes[5]/255.f);}}
    if(menu.photo_active&&!connected)photo_mode_command(PHOTO_STOP);
    unsigned keys=0;
    if(active&&menu.photo_active&&!menu.visible&&!menu.draining){
        if(!buttons)photo_armed=1;
        in.up=(!!(buttons&PAD_RB))-(!!(buttons&PAD_LB));
        in.right+=photo_key('L')-photo_key('J');in.forward+=photo_key('I')-photo_key('K');in.up+=photo_key('U')-photo_key('O');
        in.yaw+=photo_key(VK_LEFT)-photo_key(VK_RIGHT);in.pitch+=photo_key(VK_UP)-photo_key(VK_DOWN);
        in.zoom+=photo_key(VK_NEXT)-photo_key(VK_PRIOR);in.fast=photo_key(VK_SHIFT);
        keys=(photo_key('P')?1u:0)|(photo_key(VK_HOME)?2u:0)|(photo_key(VK_ESCAPE)?4u:0)|(photo_key('H')?8u:0)|(photo_key(VK_TAB)?16u:0)|(photo_key('C')?32u:0)|((photo_key(VK_F8)||photo_key(VK_F4))?PHOTO_PANEL_PAGE_KEY:0)|(photo_key('W')?PHOTO_PANEL_UP_KEY:0)|(photo_key('S')?PHOTO_PANEL_DOWN_KEY:0)|(photo_key('A')?PHOTO_PANEL_LEFT_KEY:0)|(photo_key('D')?PHOTO_PANEL_RIGHT_KEY:0);
        unsigned pressed=keys&~photo_previous_keys;uint32_t pad=photo_armed?buttons&~photo_previous:0;
        for(int id=0;id<BIND_COUNT;++id)if(preferences.actions[id].pad&&(buttons&preferences.actions[id].pad)==preferences.actions[id].pad){pad=0;memset(&in,0,sizeof(in));break;}
        if((pressed&16)||((pad&PAD_DOWN)&&!photo_panel_editing(&menu))){menu.photo_help_hidden=!menu.photo_help_hidden;pad&=~PAD_DOWN;}
        if(photo_panel_editing(&menu)&&menu.photo_panel_page==3)in.up=photo_key('U')-photo_key('O');
        int panel_action=photo_panel_controls(&menu,&pad,pressed);
        if(panel_action==PHOTO_PANEL_EFFECTS_CHANGED)action(MENU_PHOTO_EFFECTS_CHANGED);
        else if(panel_action==PHOTO_PANEL_REPLAY_STEP_CHANGED)save_settings();
        else if(panel_action==PHOTO_PANEL_REPLAY_SPEED)replay_set_speed(menu.replay_speed_index);
        else if(panel_action==PHOTO_PANEL_REPLAY_RANGE)replay_toggle_range();
        else if(panel_action==PHOTO_PANEL_REPLAY_TOGGLE)replay_toggle_pause();
        else if(panel_action==PHOTO_PANEL_REPLAY_BACK)replay_step_ms(-1,menu_replay_step_ms(&menu));
        else if(panel_action==PHOTO_PANEL_REPLAY_FORWARD)replay_step_ms(1,menu_replay_step_ms(&menu));
        int editing=photo_panel_editing(&menu);
        if(((pressed&1)||(!editing&&(pad&PAD_A)))&&!(menu.photo_panel_page==3&&!menu.replay_ready))photo_freeze_action();
        if((pressed&2)||(pad&0x1000))photo_mode_command(PHOTO_RESET);
        if((pressed&4)||(pad&PAD_B))photo_mode_command(PHOTO_STOP);
        if((pressed&8)||(!editing&&(pad&PAD_UP)))photo_mode_command(PHOTO_HUD);
        if((pressed&32)||(!editing&&(pad&PAD_RIGHT))){photo_mode_command(PHOTO_CAGE);if(snap.frozen)status(L"Cage setting changed. Unfreeze briefly to update the fence.");}
        if(pad&0x8000)request_screenshot();
    }else{memset(&in,0,sizeof(in));photo_armed=0;}
    photo_previous=buttons;photo_previous_keys=keys;
    photo_mode_poll(active,menu.visible||menu.draining,&in);
}
static void tick(void) {
    static int loader_seen,replay_ready_logged;
    static ULONGLONG replay_next,replay_deadline;
    if(!test_mode&&!replay_ready_logged){
        ULONGLONG now=GetTickCount64();if(!replay_deadline)replay_deadline=now+90000;
        if(now>=replay_next){
            if(replay_prepare()){log_line("Native replay clock controls ready.");replay_ready_logged=1;}
            else if(now>=replay_deadline){log_line("Native replay controls unavailable: startup verification timed out.");replay_ready_logged=1;}
            replay_next=now+1000;
        }
    }
    if(!test_mode&&!loader_seen){HMODULE loader=GetModuleHandleW(L"vulkan-1.dll");if(loader){
        typedef int (WINAPI *EnumerateVersion)(unsigned*);EnumerateVersion version=(EnumerateVersion)GetProcAddress(loader,"vkEnumerateInstanceVersion");unsigned api=1u<<22;
        int ok=!version||version(&api)==0;char line[120];snprintf(line,sizeof(line),"Vulkan loader observed: API=%u.%u.%u query_ok=%d",api>>22,(api>>12)&1023,api&4095,ok);log_line(line);loader_seen=1;
    }}
    if(GetTickCount64()>=next_theme_read){apply_theme();next_theme_read=GetTickCount64()+1000;}
    int active = test_mode||foreground();
    EnterCriticalSection(&lock);int mute_background=menu.mute_background;LeaveCriticalSection(&lock);
    if(!test_mode)audio_focus_poll(mute_background,active&&!IsIconic(game_window));
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
    poll_action_bindings(now,active,controller_active);
    uint32_t all_buttons = 0;
    for (int i = 0; i < 16; ++i) if (devices[i].used) all_buttons |= devices[i].buttons;
    if(wait_access_release){
        uint32_t held=0;
        for(int i=0;i<16;++i)if(devices[i].used&&native_allowed(&devices[i]))held|=devices[i].buttons|native_menu_stick(devices[i].axes[0],devices[i].axes[1]);
        for(int slot=0;slot<4;++slot)if(raw_connected[slot]&&raw_allowed(slot))held|=raw_menu_buttons(slot);
        if(!held)wait_access_release=0;
    }
    if (!menu.visible && !menu.draining && !photo_mode_blocks_input()) {
        /* A chord must come entirely from one controller. */
        int owner = -1;
        for (int i = 0; i < 16; ++i)
            if (devices[i].used && native_allowed(&devices[i]) && (devices[i].buttons & menu_shortcut(&menu)) == menu_shortcut(&menu)) { owner = i; break; }
        if (owner != menu_device) { menu.chord_since = 0; menu.chord_latched = 0; }
        menu_device = owner;
        int result = menu_tick(&menu, owner >= 0 ? devices[owner].buttons : 0, now, controller_active);
        if (result == MENU_OPEN) {
            if(photo_mode_blocks_input())menu.page=PAGE_PHOTO;
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
        if (was_draining && !menu.draining && !photo_mode_blocks_input()) {
            for (int i = 0; i < 16; ++i) if (devices[i].used)
                for (int a = 0; a < 6; ++a)
                    original_axis(devices[i].controller, devices[i].id, a, devices[i].axes[a]);
        }
    }
    poll_photo(active);
    int visible = menu.visible;
    LeaveCriticalSection(&lock);
    if (visible) {
        if (active && !IsIconic(game_window)) position_overlay();
        else ShowWindow(overlay, SW_HIDE);
    }
}
static uint64_t startup_probe_deadline,startup_probe_next,startup_probe_press_until;
static unsigned startup_probe_seen;
static void *startup_probe_controller;
static int startup_probe_controller_id,startup_probe_keyboard;
#ifdef QUIT_MENU_TEST
static int test_startup_probe,test_startup_frames,test_startup_dirty;
__declspec(dllexport) void MenuTestStartupProbeEnable(void){test_startup_probe=1;startup_probe_deadline=GetTickCount64()+300000;startup_probe_next=0;menu.startup_popups=1;}
__declspec(dllexport) int MenuTestStartupProbeFrames(void){return test_startup_frames;}
__declspec(dllexport) int MenuTestStartupProbeDirty(void){return test_startup_dirty;}
#endif
static void startup_release_confirm(void){
    if(!startup_probe_press_until)return;
    refresh_native_players();
    if(startup_probe_keyboard){PostMessageW(game_window,WM_KEYUP,VK_RETURN,0xc01c0001);startup_probe_keyboard=0;}
    else if(startup_probe_controller==native_manager&&startup_probe_controller_id==native_primary_id)
        original_button(startup_probe_controller,startup_probe_controller_id,PAD_A,false);
    startup_probe_press_until=0;startup_probe_controller=NULL;
}
__declspec(dllexport) BOOL Ufc6StartupProbeRequest(void){
    BOOL ready=FALSE;uint64_t now=GetTickCount64();EnterCriticalSection(&lock);
#ifdef QUIT_MENU_TEST
    if(test_mode){ready=test_startup_probe&&now>=startup_probe_next;if(ready)startup_probe_next=now+100;LeaveCriticalSection(&lock);return ready;}
#endif
    if(menu.startup_popups&&!menu.visible&&!menu.draining&&!startup_probe_press_until&&(startup_probe_seen&3)!=3&&now<startup_probe_deadline&&now>=startup_probe_next&&foreground()&&!IsIconic(game_window)){
        ready=TRUE;startup_probe_next=now+100;
    }
    LeaveCriticalSection(&lock);return ready;
}
__declspec(dllexport) void Ufc6StartupProbeFrame(const void *pixels,uint32_t width,uint32_t height){
#ifdef QUIT_MENU_TEST
    if(test_mode){++test_startup_frames;if(test_startup_probe&&pixels&&width&&height){const unsigned char *p=pixels;int dirty=0;for(unsigned y=0;y<height;y+=height/4?height/4:1)for(unsigned x=0;x<width;x+=width/4?width/4:1){const unsigned char *v=p+((size_t)y*width+x)*4;if(memcmp(v,p,3))dirty=1;}test_startup_dirty+=dirty;}return;}
#endif
    int id=startup_prompt_identify(pixels,width,height);if(!id)return;
    EnterCriticalSection(&lock);uint64_t now=GetTickCount64();int held=0;
    for(int i=0;i<16;++i)held|=devices[i].used&&devices[i].buttons;
    for(int i=0;i<4;++i)held|=raw_connected[i]&&raw_states[i].Gamepad.wButtons;
    refresh_native_players();
    StartupPromptContext context={menu.startup_popups,foreground()&&!IsIconic(game_window),menu.visible||menu.draining,held,startup_probe_press_until!=0,native_manager&&native_primary_id>=0,startup_probe_seen,now,startup_probe_deadline};
    if(startup_prompt_can_confirm(id,&context)){
        startup_probe_controller=native_manager;startup_probe_controller_id=native_primary_id;startup_probe_press_until=now+100;
        if(id==3&&(!native_manager||native_primary_id<0)){
            startup_probe_keyboard=PostMessageW(game_window,WM_KEYDOWN,VK_RETURN,0x001c0001)!=0;
            if(!startup_probe_keyboard){startup_probe_press_until=0;LeaveCriticalSection(&lock);return;}
        }else original_button(startup_probe_controller,startup_probe_controller_id,PAD_A,true);
        startup_probe_seen|=1u<<(id-1);
        log_line(id==3?"Auto-confirmed recognized Press any button title screen.":id==1?"Auto-confirmed recognized PSN/EA startup warning.":"Auto-confirmed recognized roster-download startup warning.");
    }
    LeaveCriticalSection(&lock);
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
static void renderer_startup_status(void){
    HANDLE token=NULL;TOKEN_ELEVATION elevation={0};DWORD length=0;int elevated=-1;
    if(OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&token)){if(GetTokenInformation(token,TokenElevation,&elevation,sizeof(elevation),&length))elevated=elevation.TokenIsElevated!=0;CloseHandle(token);}
    char line[180];snprintf(line,sizeof(line),"Renderer startup: elevated=%d legacy_layer_path=%d additive_layer_path=%d instance_layers=%d",elevated,GetEnvironmentVariableW(L"VK_LAYER_PATH",NULL,0)!=0,GetEnvironmentVariableW(L"VK_ADD_LAYER_PATH",NULL,0)!=0,GetEnvironmentVariableW(L"VK_INSTANCE_LAYERS",NULL,0)!=0);log_line(line);
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
    GetModuleFileNameW(instance,performance_bin,LAUNCH_PATH);wchar_t *performance_slash=wcsrchr(performance_bin,L'\\');if(performance_slash)*performance_slash=0;
    preferences_seed_appearance(settings_path);
    menu.decision_replays=GetPrivateProfileIntW(L"Menu",L"OfferDecisionReplays",0,settings_path)!=0;
    menu.mute_background=GetPrivateProfileIntW(L"Menu",L"MuteWhenUnfocused",0,settings_path)!=0;
    menu.replay_step_index=GetPrivateProfileIntW(L"Menu",L"ReplayStepIndex",0,settings_path);if(menu.replay_step_index<0||menu.replay_step_index>6)menu.replay_step_index=0;
    photo_effects_load(&menu.effects,settings_path);
    wchar_t icons[32];
    GetPrivateProfileStringW(L"Menu",L"ButtonIcons",L"Xbox",icons,32,settings_path);
    EnterCriticalSection(&lock);menu.playstation_icons=lstrcmpiW(icons,L"PlayStation")==0;LeaveCriticalSection(&lock);
    wchar_t shortcut[40];GetPrivateProfileStringW(L"Menu",L"OpeningShortcut",L"",shortcut,40,settings_path);
    wchar_t *end;unsigned long chord=wcstoul(shortcut,&end,0);
    EnterCriticalSection(&lock);if(shortcut[0]&&!*end&&menu_valid_shortcut((uint32_t)chord))menu.open_chord=(uint32_t)chord;LeaveCriticalSection(&lock);
    int scale=(int)GetPrivateProfileIntW(L"Menu",L"UIScale",MENU_SCALE_DEFAULT,settings_path);
    EnterCriticalSection(&lock);menu.ui_scale_percent=overlay_valid_scale(scale)?scale:MENU_SCALE_DEFAULT;LeaveCriticalSection(&lock);
    menu.screenshot_include_ui=GetPrivateProfileIntW(L"Menu",L"IncludeOverlayInScreenshots",0,settings_path)!=0;
    menu.skip_splashes=GetPrivateProfileIntW(L"Menu",L"SkipStartupSplashes",0,settings_path)!=0;
    menu.startup_popups=GetPrivateProfileIntW(L"Menu",L"StartupSkipPopups",0,settings_path)!=0;
    int auto_launch=(int)GetPrivateProfileIntW(L"Menu",L"AutoLaunchAfterRestart",1,settings_path);
    EnterCriticalSection(&lock);menu.disable_auto_launch=auto_launch==0;LeaveCriticalSection(&lock);
    wchar_t access[32];GetPrivateProfileStringW(L"Menu",L"ControllerAccess",L"Either",access,32,settings_path);
    EnterCriticalSection(&lock);menu.first_player_only=!lstrcmpiW(access,L"Player1");LeaveCriticalSection(&lock);
    int guide_enabled=(int)GetPrivateProfileIntW(L"Menu",L"GuideButtonOpensMenu",1,settings_path);
    EnterCriticalSection(&lock);menu.disable_guide=guide_enabled==0;LeaveCriticalSection(&lock);
    log_line("UFC6 quit menu loaded inside engine process.");
    log_line("Overlay build " UFC6_OVERLAY_VERSION ".");
    if(!test_mode)renderer_startup_status();
    if (!test_mode && !install_hooks()) return 0;
    if (!test_mode) photo_mode_prepare();
    /* The route selector can be read only once during boot. Apply it as soon as
       guest code is mapped, before constructing the overlay or starting its timer. */
    if(!test_mode&&menu.skip_splashes){
        uint64_t until=GetTickCount64()+10000;int applied=0;
        do{if(startup_options_tick(&startup_options,0x900000000ULL,1,1)>0&&startup_options.owned[2]){applied=1;break;}Sleep(1);}while(GetTickCount64()<until);
        log_line(applied?"Early experimental splash-bypass callback applied.":"Early splash-bypass callback unavailable; retaining normal game flow.");
    }

    int longevity_setting=GetPrivateProfileIntW(L"Menu",L"LongevityProtection",0,settings_path)!=0;
    preferences_load(&preferences,settings_path,menu_shortcut(&menu));menu.theme_selection=preferences.theme_choice;menu.font_index=(preferences.font_size-16)/2;menu.box_style=preferences.box_style;menu.window_size=preferences.window_size;
    lstrcpynW(theme_path,settings_path,MAX_PATH);wchar_t *theme_slash=wcsrchr(theme_path,L'\\');if(theme_slash)lstrcpyW(theme_slash+1,L"ufc6-overlay-theme.cfg");
    theme_create_default(theme_path);theme_load(&current_theme,theme_path,preferences.theme_choice);
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
    for(int size=0;size<3;++size){int base=16+size*2;int heights[]={32*base/18,base,16*base/18};
        for(int role=0;role<3;++role)font_sets[size][role]=CreateFontW(-heights[role],0,0,0,role==0?FW_BOLD:FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,ANTIALIASED_QUALITY,0,L"Segoe UI");}
    font_title=font_sets[1][0];font_main=font_body=font_settings=font_sets[1][1];font_small=font_sets[1][2];
    HMODULE xi = LoadLibraryExW(L"xinput1_4.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (xi) xstate = (XStateFn)GetProcAddress(xi, (LPCSTR)100);
    log_line(xstate ? "Ready. LB+RB+Menu (800ms), Guide (if available), or F10." : "Ready. LB+RB+Menu (800ms) or F10. Guide unavailable.");
    log_timeline("session_ready");
    if(!test_mode){game_icons_init(instance);save_backups_init(instance);}
    refresh_gamebar();
    live_cheats_init(&live_cheats,0x900000000ULL);career_cheats_init(0x900000000ULL);
    career_cheats_protect_longevity(longevity_setting);career_cheats_training_multiplier(menu_training_multiplier(&menu));
#ifdef QUIT_MENU_TEST
    if(test_mode)career=(CareerSnapshot){.ready=1,.status=CAREER_READY,.balance={12109126,16},.maximum={2147483647,16383},.fitness_ready=1,.fitness=7900,.training_ready=1,.training_days=0,.training_week=1,.longevity_ready=1,.longevity=9895,.longevity_enabled=longevity_setting,.longevity_requested=longevity_setting,.training_multiplier=(uint32_t)menu_training_multiplier(&menu),.training_rewards_ready=1,.sparring_grades_ready=1,.sparring_grades_completed=2};
#endif
    refresh_live_cheats();
    if(!test_mode)gamebar_monitor_start();
    startup_probe_deadline=GetTickCount64()+300000;
    SetTimer(NULL, 1, 16, NULL);
    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        if (msg.message == WM_TIMER) {
            if (!IsWindow(game_window)) break;
            if(GetTickCount64()>=next_live_check){next_live_check=GetTickCount64()+250;refresh_live_cheats();}
            if(!test_mode){EnterCriticalSection(&lock);int skip=menu.skip_splashes;LeaveCriticalSection(&lock);int before=startup_options.state;int result=startup_options_tick(&startup_options,0x900000000ULL,skip,skip);if(result!=before)log_line(result<0?"Startup options waiting for verified game callbacks (or conflict).":result?"Experimental startup options applied in memory.":"Startup options restored to game defaults.");}
            EnterCriticalSection(&lock);if(startup_probe_press_until&&GetTickCount64()>=startup_probe_press_until)startup_release_confirm();LeaveCriticalSection(&lock);
            tick();
        } else if (msg.message == WM_MENU_ACTION) {
            if (msg.wParam == MENU_OPEN) { position_overlay(); log_line("Menu opened.");log_timeline("tools_opened"); }
            else if (msg.wParam == MENU_RESUME) { ShowWindow(overlay, SW_HIDE); log_line("Resume selected."); }
            else if (msg.wParam == MENU_QUIT) {
                photo_mode_command(PHOTO_STOP);ShowWindow(overlay, SW_HIDE); log_line("Quit selected: posting WM_CLOSE to game window.");
                PostMessageW(game_window, WM_CLOSE, 0, 0);
            }
            else if(msg.wParam==MENU_SHORTCUT_CHANGED)save_binding(BIND_OPEN,0,menu_shortcut(&menu));
            else if(msg.wParam==MENU_STYLE_CHANGED||msg.wParam==MENU_SCALE_CHANGED||msg.wParam==MENU_AUTO_LAUNCH_CHANGED||msg.wParam==MENU_GUIDE_CHANGED||msg.wParam==MENU_ACCESS_CHANGED)save_settings();
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
            else if(msg.wParam==MENU_STARTUP_PATCH_OPEN){status(L"");if(!test_mode)startup_assets_check();}
            else if(msg.wParam==MENU_STARTUP_PATCH_APPLY){
                EnterCriticalSection(&lock);int enabled=menu.startup_patch_enable;LeaveCriticalSection(&lock);
#ifdef QUIT_MENU_TEST
                if(test_mode){status(L"Test startup patch; no files changed or game restarted.");}else
#endif
                if(startup_assets_status(NULL,0)==(enabled?GAME_ICONS_XBOX:GAME_ICONS_PS))status(L"These startup screens are already selected.");
                else if(startup_assets_apply(game_window,enabled)){photo_mode_command(PHOTO_STOP);EnterCriticalSection(&lock);menu_close(&menu,MENU_RESUME);LeaveCriticalSection(&lock);ShowWindow(overlay,SW_HIDE);}
                else status(L"Could not prepare startup patch. The game was left running.");
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
            else if(msg.wParam==MENU_DECISION_REPLAY_CHANGED){save_settings();status(menu.decision_replays?L"Decision replays On for new offline fights.":L"Off for new fights; the current replay remains available.");}
            else if(msg.wParam==MENU_AUDIO_FOCUS_CHANGED){save_settings();status(menu.mute_background?L"Background audio muting On.":L"Background audio muting Off.");}
            else if(msg.wParam==MENU_STARTUP_CHANGED){save_settings();EnterCriticalSection(&lock);if(menu.startup_popups){startup_probe_deadline=GetTickCount64()+300000;startup_probe_seen=0;}else startup_release_confirm();LeaveCriticalSection(&lock);status(L"Startup option saved. Restart game to test splashes.");}
            else if(msg.wParam==MENU_OPEN_SCREENSHOTS)open_screenshots_folder();
            else if(msg.wParam==MENU_RESET_SETTINGS)reset_overlay_settings();
            else if(msg.wParam==MENU_NEXT_HIT_KO){
                int ok=0;EnterCriticalSection(&lock);
#ifdef QUIT_MENU_TEST
                if(test_mode){ko_status=ko_status==1?3:1;ok=1;}else
#endif
                {ReplaySnapshot replay;replay_snapshot(&replay);if(foreground()&&!menu.photo_active&&!replay.available)ok=live_ko_toggle();}
                LeaveCriticalSection(&lock);refresh_live_cheats();
                status(!ok?L"Start a fight with Player 1. Exit Photo Mode/replay first.":ko_status==1?L"Next hit KO armed for 25 seconds. Land a head strike.":L"Next hit KO cancelled.");
            }
            else if(msg.wParam==MENU_CHEAT_STAMINA||msg.wParam==MENU_CHEAT_HEALTH)change_live_cheat(msg.wParam==MENU_CHEAT_STAMINA?LIVE_STAMINA:LIVE_HEALTH);
            else if(msg.wParam==MENU_BIND_KEY_CAPTURE){key_capture_ready=0;captured_key=0;}
            else if(msg.wParam==MENU_BIND_CLEAR_KEY)save_binding(menu.binding_action,1,0);
            else if(msg.wParam==MENU_BIND_CLEAR_PAD)save_binding(menu.binding_action,0,menu.binding_action==BIND_OPEN?OPEN_CHORD:0);
            else if(msg.wParam==MENU_BIND_PAD_CHANGED)save_binding(menu.capture_target,0,menu.assigned_chord);
            else if(msg.wParam==MENU_WINDOW_SIZE_CHANGED){EnterCriticalSection(&lock);preferences.window_size=menu.window_size;LeaveCriticalSection(&lock);save_settings();status(L"Window size saved.");}
            else if(msg.wParam==MENU_FONT_CHANGED){EnterCriticalSection(&lock);preferences.font_size=16+menu.font_index*2;LeaveCriticalSection(&lock);save_settings();status(L"Font size saved.");}
            else if(msg.wParam==MENU_BOX_STYLE_CHANGED){EnterCriticalSection(&lock);preferences.box_style=menu.box_style;LeaveCriticalSection(&lock);save_settings();status(L"Box style saved.");}
            else if(msg.wParam==MENU_THEME_CHANGED){EnterCriticalSection(&lock);preferences.theme_choice=menu.theme_selection;LeaveCriticalSection(&lock);apply_theme();save_settings();}
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
            else if(msg.wParam==MENU_PERFORMANCE_OPEN)open_performance();
            else if(msg.wParam==MENU_PERFORMANCE_SAVE)save_performance(0);
            else if(msg.wParam==MENU_PERFORMANCE_RESTART)save_performance(1);
            else if(msg.wParam==MENU_SCREENSHOT_UI_CHANGED){save_settings();log_timeline("screenshot_ui_setting_changed");status(menu.screenshot_include_ui?L"Screenshots include the overlay. Turn Off for clean photos.":L"Screenshots exclude the overlay.");}
            else if(msg.wParam==MENU_PHOTO_EFFECTS_CHANGED){save_settings();EnterCriticalSection(&lock);PhotoEffects settings=menu.effects;LeaveCriticalSection(&lock);char line[200];snprintf(line,sizeof(line),"Photo effects changed: filter=%d strength=%d exposure=%d contrast=%d saturation=%d vignette=%d grain=%d bars=%d watermark=%d",settings.preset,settings.strength,settings.exposure,settings.contrast,settings.saturation,settings.vignette,settings.grain,settings.bars,settings.watermark);log_line(line);}
            else if(msg.wParam==MENU_PHOTO_ENTER){if(!photo_mode_blocks_input())start_photo();}
            else if(msg.wParam==MENU_PHOTO_FREEZE){if(photo_mode_blocks_input())photo_freeze_action();else status(L"Enter Photo Mode first.");}
            else if(msg.wParam==MENU_PHOTO_HUD){if(photo_mode_blocks_input())photo_mode_command(PHOTO_HUD);else status(L"Enter Photo Mode first.");}
            else if(msg.wParam==MENU_PHOTO_CAGE){PhotoSnapshot snap;photo_mode_snapshot(&snap);if(!snap.cage_available)status(L"Cage control is unavailable on this session.");else if(photo_mode_blocks_input())photo_mode_command(PHOTO_CAGE);else status(L"Enter Photo Mode first.");}
            else if(msg.wParam==MENU_PHOTO_HELP){EnterCriticalSection(&lock);menu.photo_help_hidden=!menu.photo_help_hidden;LeaveCriticalSection(&lock);}
            else if(msg.wParam==MENU_PHOTO_RESET){if(photo_mode_blocks_input())photo_mode_command(PHOTO_RESET);else status(L"Enter Photo Mode first.");}
            else if(msg.wParam==MENU_PHOTO_EXIT)photo_mode_command(PHOTO_STOP);
            else if(msg.wParam==MENU_SCREENSHOT)request_screenshot();
            else if(msg.wParam==MENU_SCREENSHOT_OVERLAY)request_screenshot();
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
        } else if(msg.message==WM_BINDING_ACTION){run_binding((int)msg.wParam);
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
__declspec(dllexport) void MenuTestSettingsMode(int legacy){EnterCriticalSection(&lock);menu.settings_legacy=legacy!=0;LeaveCriticalSection(&lock);}
__declspec(dllexport) void MenuTestLegacySettings(void){EnterCriticalSection(&lock);menu.settings_legacy=1;menu.page=PAGE_SETTINGS;menu.selection=0;menu.visible=1;menu.armed=0;LeaveCriticalSection(&lock);}
__declspec(dllexport) void MenuTestRendererGate(int required){test_require_renderer=required;InterlockedExchange(&vulkan_active,0);}
__declspec(dllexport) int MenuTestWindowSize(void){return preferences.window_size;}
__declspec(dllexport) void MenuTestPhotoHelp(int active,int ps,int frozen){EnterCriticalSection(&lock);test_photo_help_override=active;menu.photo_active=active;menu.photo_frozen=frozen;menu.photo_hud=1;menu.photo_cage=1;menu.photo_fov=55;menu.visible=0;menu.draining=0;menu.playstation_icons=ps;toast_until=0;LeaveCriticalSection(&lock);}
__declspec(dllexport) int MenuTestVisibleRows(void){return menu_visible_rows(&menu);}
__declspec(dllexport) int MenuTestFontSize(void){return preferences.font_size;}
__declspec(dllexport) int MenuTestBoxStyle(void){return preferences.box_style;}
__declspec(dllexport) void MenuTestStatus(const wchar_t *message){status(message);}
__declspec(dllexport) int MenuTestThemeChoice(void){return preferences.theme_choice;}
__declspec(dllexport) uint32_t MenuTestThemeAccent(void){return current_theme.accent;}
__declspec(dllexport) void MenuTestKey(int vk,int down){if(vk>=0&&vk<256)test_keys[vk]=(unsigned char)(down!=0);}
__declspec(dllexport) uint32_t MenuTestBinding(int id,int keyboard){return id>=0&&id<BIND_COUNT?(keyboard?preferences.actions[id].key:preferences.actions[id].pad):0;}
__declspec(dllexport) void MenuTestInvokeBinding(int id){PostThreadMessageW(ui_thread,WM_BINDING_ACTION,id,0);}
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
    int result=menu.selection|(menu.playstation_icons<<4)|(menu.visible<<8)|(menu.page<<12)|(menu.capturing<<20);
    LeaveCriticalSection(&lock);return result;
}
__declspec(dllexport) uint32_t MenuTestShortcut(void){EnterCriticalSection(&lock);uint32_t result=menu_shortcut(&menu);LeaveCriticalSection(&lock);return result;}
__declspec(dllexport) void MenuTestScreenshot(void){request_screenshot();}
__declspec(dllexport) void MenuTestScreenshotOverlay(void){request_screenshot_mode(1);}
__declspec(dllexport) int MenuTestPerformanceSaves(void){return test_performance_saves;}
__declspec(dllexport) int MenuTestPerformanceRestarts(void){return test_performance_restarts;}
__declspec(dllexport) int MenuTestScreenshotUI(void){EnterCriticalSection(&lock);int value=menu.screenshot_include_ui;LeaveCriticalSection(&lock);return value;}
__declspec(dllexport) void MenuTestScreenshotPath(wchar_t *out,int size){EnterCriticalSection(&lock);if(size>0)lstrcpynW(out,last_screenshot_path?last_screenshot_path:L"",size);LeaveCriticalSection(&lock);}
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

#ifdef QUIT_MENU_TEST
__declspec(dllexport) void MenuTestReplayHelp(int page,int paused){EnterCriticalSection(&lock);test_photo_help_override=1;menu.photo_active=1;menu.photo_help_hidden=0;menu.visible=0;menu.draining=0;menu.replay_ready=1;menu.replay_paused=paused;menu.photo_frozen=paused;menu.replay_position_ms=1250;menu.replay_duration_ms=4011;menu.photo_panel_page=page;menu.photo_replay_row=1;toast_until=0;LeaveCriticalSection(&lock);}

#endif

#ifdef QUIT_MENU_TEST
__declspec(dllexport) void MenuTestReplayUnavailable(void){EnterCriticalSection(&lock);menu.replay_ready=0;menu.photo_panel_page=3;LeaveCriticalSection(&lock);}
__declspec(dllexport) void MenuTestReplayRange(int extended,int failed){EnterCriticalSection(&lock);menu.replay_extended=extended;menu.replay_range_failed=failed;menu.photo_replay_row=3;LeaveCriticalSection(&lock);}
#endif

#ifdef QUIT_MENU_TEST
__declspec(dllexport) int MenuTestKoState(void){return ko_status;}
#endif
