#ifndef UFC6_MENU_STATE_H
#define UFC6_MENU_STATE_H
#include <stdint.h>
#include "overlay_layout.h"
#include "photo_effects.h"
#include "photo_presets.h"
#include "performance_settings.h"

enum {
    PAD_MENU=0x8, PAD_UP=0x10, PAD_RIGHT=0x20, PAD_DOWN=0x40, PAD_LEFT=0x80,
    PAD_LB=0x400, PAD_RB=0x800, PAD_B=0x2000, PAD_A=0x4000,
    OPEN_CHORD=PAD_LB|PAD_RB|PAD_MENU,
    SHORTCUT_BUTTONS=0x0000fcfe
};
enum { PAGE_MAIN, PAGE_SETTINGS, PAGE_RESTART, PAGE_SHUTDOWN, PAGE_REBOOT, PAGE_SCREENSHOT, PAGE_RESET, PAGE_CHEATS, PAGE_CAREER_ADD, PAGE_GAME_ICONS, PAGE_GAME_ICONS_CONFIRM, PAGE_BACKUPS, PAGE_KEYBINDS, PAGE_BINDING, PAGE_THEME, PAGE_QUIT, PAGE_SETTINGS_HOME, PAGE_APPEARANCE, PAGE_CONTROLS, PAGE_GAME_SETTINGS, PAGE_TOOLS, PAGE_PHOTO, PAGE_PHOTO_CONTROLS, PAGE_PHOTO_EFFECTS, PAGE_PHOTO_WATERMARK, PAGE_PERFORMANCE, PAGE_PERFORMANCE_RESTART, PAGE_PERFORMANCE_DISCARD, PAGE_STARTUP, PAGE_STARTUP_CONFIRM, PAGE_EXPERIMENTAL };
enum { MAIN_RESUME, MAIN_SCREENSHOT, MAIN_RESTART, MAIN_CHEATS, MAIN_PHOTO, MAIN_SETTINGS, MAIN_QUIT, MAIN_REBOOT, MAIN_SHUTDOWN, MAIN_COUNT };
enum { SETTING_ICONS, SETTING_SHORTCUT, SETTING_SCALE, SETTING_AUTO_LAUNCH, SETTING_GUIDE, SETTING_GAMEBAR, SETTING_LAUNCHER, SETTING_ACCESS, SETTING_SCREENSHOTS, SETTING_RESET, SETTING_GAME_ICONS, SETTING_UPDATE, SETTING_DIAGNOSTICS, SETTING_KEYBINDS, SETTING_THEME, SETTING_BACK, SETTING_FONT, SETTING_BOX_STYLE, SETTING_WINDOW_SIZE, SETTING_STARTUP_POPUPS, SETTING_SKIP_SPLASHES, SETTING_SUPPRESS_ONLINE_NOTICES, SETTING_SCREENSHOT_UI, SETTING_STARTUP_PATCH, SETTING_AUDIO_FOCUS, SETTING_DECISION_REPLAY, SETTING_EXPERIMENTAL, SETTING_INSTANT_REPLAY, SETTING_COUNT };
enum { MENU_NONE, MENU_OPEN, MENU_RESUME, MENU_QUIT, MENU_CHANGED,
       MENU_STYLE_CHANGED, MENU_SHORTCUT_CHANGED, MENU_SCREENSHOT, MENU_RESTART, MENU_SHUTDOWN, MENU_SCALE_CHANGED, MENU_REBOOT, MENU_AUTO_LAUNCH_CHANGED, MENU_GUIDE_CHANGED, MENU_GAMEBAR_CHANGED, MENU_LAUNCHER_SETTINGS, MENU_ACCESS_CHANGED, MENU_OPEN_SCREENSHOTS, MENU_RESET_SETTINGS, MENU_CHEAT_STAMINA, MENU_CHEAT_HEALTH, MENU_CAREER_ADD, MENU_GAME_ICONS_OPEN, MENU_GAME_ICONS_APPLY, MENU_BACKUP_CREATE, MENU_BACKUP_FOLDER, MENU_BACKUP_OPEN, MENU_CHECK_UPDATES, MENU_EXPORT_DIAGNOSTICS, MENU_CAREER_LONGEVITY, MENU_TRAINING_MULTIPLIER, MENU_BIND_KEY_CAPTURE, MENU_BIND_CLEAR_KEY, MENU_BIND_CLEAR_PAD, MENU_BIND_PAD_CHANGED, MENU_THEME_CHANGED, MENU_FONT_CHANGED, MENU_BOX_STYLE_CHANGED, MENU_WINDOW_SIZE_CHANGED, MENU_STARTUP_CHANGED, MENU_ONLINE_NOTICES_CHANGED, MENU_PHOTO_ENTER, MENU_PHOTO_FREEZE, MENU_PHOTO_RESET, MENU_PHOTO_EXIT, MENU_PHOTO_HUD, MENU_PHOTO_CAGE, MENU_PHOTO_HELP, MENU_SCREENSHOT_OVERLAY, MENU_PHOTO_EFFECTS_CHANGED, MENU_SCREENSHOT_UI_CHANGED, MENU_PERFORMANCE_OPEN, MENU_PERFORMANCE_SAVE, MENU_PERFORMANCE_RESTART, MENU_STARTUP_PATCH_OPEN, MENU_STARTUP_PATCH_APPLY, MENU_AUDIO_FOCUS_CHANGED, MENU_NEXT_HIT_KO, MENU_DECISION_REPLAY_CHANGED, MENU_INSTANT_REPLAY_CHANGED };

typedef struct {
    int visible,selection,armed,chord_latched,draining;
    int playstation_icons,page;
    int startup_popups,skip_splashes,suppress_online_notices,mute_background,decision_replays,instant_replay;
    PerformanceSettings performance,performance_original;
    int performance_available,performance_dirty,performance_feedback;
    PhotoEffects effects;
    PhotoPreset photo_presets[PHOTO_PRESET_COUNT];
    int photo_panel_page,photo_effect_row,photo_mark_row,photo_replay_row,photo_preset_row,photo_preset_slot,replay_step_index,replay_speed_index;
    int replay_ready,replay_paused,replay_position_ms,replay_duration_ms,replay_extended,replay_range_failed,replay_coverage_known,replay_coverage_ms,replay_coverage_pending;
    int photo_active,photo_frozen,photo_fov,photo_hud,photo_cage,photo_help_hidden,screenshot_overlay,screenshot_include_ui;
    uint32_t open_chord,capture_mask;
    int capturing,capture_ready,capture_error;
    uint64_t chord_since,capture_since;
    int shutdown_holding,shutdown_countdown,shutdown_progress,shutdown_seconds;
    uint64_t shutdown_hold_since,shutdown_countdown_since;
    int ui_scale_percent;
    int disable_auto_launch;
    int disable_guide;
    int gamebar_shortcut;
    int first_player_only;
    int career_amount_index[2],career_kind,game_icons_xbox,training_multiplier_index,startup_patch_enable;
    int binding_action,key_capturing,capture_target,theme_selection,font_index,box_style,window_size,settings_legacy;uint32_t assigned_chord;
} MenuState;

static inline int menu_replay_step_ms(const MenuState *s){static const int steps[]={10,25,50,100,250,500,1000};return steps[s->replay_step_index>=0&&s->replay_step_index<7?s->replay_step_index:0];}
static int menu_power_page(const MenuState *s){return s->page==PAGE_SHUTDOWN||s->page==PAGE_REBOOT;}
static int menu_power_selection(const MenuState *s){return s->page==PAGE_REBOOT?MAIN_REBOOT:MAIN_SHUTDOWN;}
static int menu_auto_launch(const MenuState *s){return !s->disable_auto_launch;}
static int menu_guide_enabled(const MenuState *s){return !s->disable_guide;}
static int menu_controller_allowed(const MenuState *s,int player){return !s->first_player_only||player==1;}
static int menu_hotkey_pressed(const MenuState *s,int guide,int was_guide,int f10,int was_f10){
    return (menu_guide_enabled(s)&&guide&&!was_guide)||(f10&&!was_f10);
}

static unsigned menu_button_count(uint32_t mask) {
    unsigned count=0;for(;mask;mask&=mask-1)++count;return count;
}
static int menu_valid_shortcut(uint32_t mask) {
    unsigned count=menu_button_count(mask);
    return !(mask&~SHORTCUT_BUTTONS)&&count>=2&&count<=4;
}
static uint32_t menu_shortcut(const MenuState *s) {return s->open_chord?s->open_chord:OPEN_CHORD;}
static int menu_scale(const MenuState *s) {return overlay_valid_scale(s->ui_scale_percent)?s->ui_scale_percent:MENU_SCALE_DEFAULT;}
static uint32_t menu_career_amount(const MenuState *s,int kind){
    static const uint32_t money[]={1000,10000,100000,1000000,10000000};
    static const uint32_t points[]={1,5,10,25,50,100,250,500,1000};
    int i=s->career_amount_index[kind];return kind==0?money[i>=0&&i<5?i:0]:points[i>=0&&i<9?i:0];
}
static int menu_training_multiplier(const MenuState *s){static const int values[]={1,2,5,10};return values[s->training_multiplier_index>=0&&s->training_multiplier_index<4?s->training_multiplier_index:0];}
static int menu_career_row(int kind){return kind==5?9:3+kind;}
static int menu_setting_page(int page){return page==PAGE_SETTINGS||page==PAGE_APPEARANCE||page==PAGE_CONTROLS||page==PAGE_GAME_SETTINGS||page==PAGE_TOOLS||page==PAGE_EXPERIMENTAL;}
static int menu_setting_id(int page,int row){
    static const int appearance[]={SETTING_THEME,SETTING_SCALE,SETTING_FONT,SETTING_BOX_STYLE,SETTING_WINDOW_SIZE,SETTING_ICONS,SETTING_BACK};
    static const int controls[]={SETTING_KEYBINDS,SETTING_GUIDE,SETTING_GAMEBAR,SETTING_ACCESS,SETTING_BACK};
    static const int game[]={SETTING_LAUNCHER,SETTING_AUTO_LAUNCH,SETTING_GAME_ICONS,SETTING_SKIP_SPLASHES,SETTING_SUPPRESS_ONLINE_NOTICES,SETTING_STARTUP_PATCH,SETTING_EXPERIMENTAL,SETTING_AUDIO_FOCUS,SETTING_DECISION_REPLAY,SETTING_BACK};
    static const int tools[]={SETTING_SCREENSHOTS,SETTING_SCREENSHOT_UI,SETTING_UPDATE,SETTING_DIAGNOSTICS,SETTING_RESET,SETTING_BACK};
    static const int experimental[]={SETTING_STARTUP_POPUPS,SETTING_INSTANT_REPLAY,SETTING_BACK};
    return page==PAGE_APPEARANCE?appearance[row]:page==PAGE_CONTROLS?controls[row]:page==PAGE_GAME_SETTINGS?game[row]:page==PAGE_TOOLS?tools[row]:page==PAGE_EXPERIMENTAL?experimental[row]:row;
}
static void menu_return_setting(MenuState *s,int id){
    s->page=PAGE_SETTINGS;s->selection=id;s->armed=0;if(s->settings_legacy)return;
    if(id==SETTING_STARTUP_POPUPS||id==SETTING_INSTANT_REPLAY){s->page=PAGE_EXPERIMENTAL;s->selection=id==SETTING_STARTUP_POPUPS?0:1;return;}
    const int pages[]={PAGE_APPEARANCE,PAGE_CONTROLS,PAGE_GAME_SETTINGS,PAGE_TOOLS},counts[]={6,4,9,5};
    for(int p=0;p<4;++p)for(int i=0;i<counts[p];++i)if(menu_setting_id(pages[p],i)==id){s->page=pages[p];s->selection=i;return;}
    s->page=PAGE_SETTINGS_HOME;s->selection=0;
}
static int menu_scroll_page(int page){return page==PAGE_STARTUP||menu_setting_page(page)||page==PAGE_CHEATS||page==PAGE_KEYBINDS||page==PAGE_THEME||page==PAGE_PHOTO||page==PAGE_PHOTO_CONTROLS||page==PAGE_PHOTO_EFFECTS||page==PAGE_PHOTO_WATERMARK||page==PAGE_PERFORMANCE;}
static int menu_rows(const MenuState *s) {return s->page==PAGE_STARTUP?3:s->page==PAGE_STARTUP_CONFIRM?2:s->page==PAGE_PERFORMANCE?PERF_ROWS:s->page==PAGE_PERFORMANCE_RESTART||s->page==PAGE_PERFORMANCE_DISCARD?2:s->page==PAGE_PHOTO_EFFECTS?FX_ROWS:s->page==PAGE_PHOTO_WATERMARK?MARK_ROWS:s->page==PAGE_PHOTO?12:s->page==PAGE_PHOTO_CONTROLS?12:s->page==PAGE_SETTINGS_HOME?6:s->page==PAGE_APPEARANCE?7:s->page==PAGE_CONTROLS?5:s->page==PAGE_GAME_SETTINGS?10:s->page==PAGE_TOOLS?6:s->page==PAGE_EXPERIMENTAL?3:s->page==PAGE_KEYBINDS?11:s->page==PAGE_BINDING?5:s->page==PAGE_THEME?7:s->page==PAGE_MAIN?MAIN_COUNT:s->page==PAGE_SETTINGS?SETTING_COUNT:s->page==PAGE_CHEATS?12:s->page==PAGE_BACKUPS?3:s->page==PAGE_GAME_ICONS?3:s->shutdown_countdown?1:2;}
static int menu_visible_rows(const MenuState *s){int capacity=s->window_size==2?12:s->window_size==1?9:6,count=menu_rows(s);return count<capacity?count:capacity;}
static uint32_t menu_canvas_height(const MenuState *s){int rows=menu_visible_rows(s);return MENU_BASE_HEIGHT+(s->page==PAGE_MAIN?27:0)+(menu_scroll_page(s->page)&&rows>6?(uint32_t)(rows-6)*32:0);}
static int menu_first_row(const MenuState *s){int visible=menu_visible_rows(s);return menu_scroll_page(s->page)&&s->selection>=visible?s->selection-visible+1:0;}
static void menu_cancel_shutdown(MenuState *s) {
    s->shutdown_holding=0;s->shutdown_countdown=0;s->shutdown_progress=0;
    s->shutdown_hold_since=0;s->shutdown_countdown_since=0;s->shutdown_seconds=0;
}
static void menu_show(MenuState *s) {
    s->visible=1;s->page=PAGE_MAIN;s->selection=MAIN_RESUME;s->armed=0;
    s->capturing=0;s->key_capturing=0;s->capture_error=0;
    menu_cancel_shutdown(s);
}
static int menu_close(MenuState *s,int action) {
    s->visible=0;s->draining=1;s->armed=0;s->capturing=0;s->key_capturing=0;menu_cancel_shutdown(s);return action;
}
static int menu_tick(MenuState *s,uint32_t buttons,uint64_t now,int foreground) {
    int chord=(buttons&menu_shortcut(s))==menu_shortcut(s);
    if(!foreground) {
        s->chord_since=0;s->chord_latched=chord;
        if(s->capturing){s->capture_ready=0;s->capture_mask=0;s->capture_since=0;}
        if(s->shutdown_holding||s->shutdown_countdown){menu_cancel_shutdown(s);s->selection=0;s->armed=0;return MENU_CHANGED;}
        return MENU_NONE;
    }
    if(menu_power_page(s)&&(s->shutdown_holding||s->shutdown_countdown)) {
        if(buttons&PAD_B){menu_cancel_shutdown(s);s->selection=menu_power_selection(s);s->page=PAGE_MAIN;s->armed=0;return MENU_CHANGED;}
        if(s->shutdown_countdown) {
            uint64_t elapsed=now-s->shutdown_countdown_since;
            if(elapsed>=10000)return menu_close(s,s->page==PAGE_REBOOT?MENU_REBOOT:MENU_SHUTDOWN);
            s->shutdown_seconds=10-(int)(elapsed/1000);
            if(!buttons)s->armed=1;
            return MENU_CHANGED;
        }
        if(!(buttons&PAD_A)){menu_cancel_shutdown(s);return MENU_CHANGED;}
        if(!s->shutdown_hold_since)s->shutdown_hold_since=now;
        uint64_t elapsed=now-s->shutdown_hold_since;
        s->shutdown_progress=elapsed>=3000?3000:(int)elapsed;
        if(elapsed>=3000){
            s->shutdown_holding=0;s->shutdown_countdown=1;s->shutdown_countdown_since=now;
            s->shutdown_seconds=10;s->selection=0;s->armed=0;
        }
        return MENU_CHANGED;
    }
    if(s->key_capturing)return MENU_NONE;
    if(s->capturing) {
        if(!s->capture_ready){if(!buttons)s->capture_ready=1;return MENU_NONE;}
        if(buttons) {
            if(!s->capture_mask)s->capture_since=now;
            s->capture_mask|=buttons&SHORTCUT_BUTTONS;
            return MENU_NONE;
        }
        if(s->capture_mask) {
            uint32_t mask=s->capture_mask;s->capture_mask=0;
            if(mask==PAD_B){s->capturing=0;s->armed=0;return MENU_CHANGED;}
            if(menu_valid_shortcut(mask)&&now-s->capture_since>=300) {
                if(!s->capture_target)s->open_chord=mask;s->assigned_chord=mask;s->capturing=0;s->armed=0;s->chord_since=0;s->chord_latched=0;
                return s->capture_target?MENU_BIND_PAD_CHANGED:MENU_SHORTCUT_CHANGED;
            }
            s->capture_error=1;return MENU_CHANGED;
        }
        return MENU_NONE;
    }
    if(!chord){s->chord_since=0;s->chord_latched=0;}
    else if(!s->chord_latched) {
        if(!s->chord_since)s->chord_since=now;
        if(now-s->chord_since>=800) {
            s->chord_latched=1;
            if(!s->visible&&!s->draining){menu_show(s);return MENU_OPEN;}
        }
    }
    if(s->visible&&!buttons)s->armed=1;
    if(s->draining&&!buttons)s->draining=0;
    return MENU_NONE;
}
static int menu_button(MenuState *s,uint32_t button,int down) {
    if(!s->visible||!s->armed||!down||s->capturing)return MENU_NONE;
    if(button==PAD_B) {
        if(s->page==PAGE_STARTUP_CONFIRM){s->page=PAGE_STARTUP;s->selection=s->startup_patch_enable?0:1;s->armed=0;return MENU_CHANGED;}
        if(s->page==PAGE_STARTUP){menu_return_setting(s,SETTING_STARTUP_PATCH);return MENU_CHANGED;}
        if(s->page==PAGE_EXPERIMENTAL){s->page=PAGE_GAME_SETTINGS;s->selection=6;s->armed=0;return MENU_CHANGED;}
        if(s->page==PAGE_PERFORMANCE_RESTART||s->page==PAGE_PERFORMANCE_DISCARD){int page=s->page;s->page=PAGE_PERFORMANCE;s->selection=page==PAGE_PERFORMANCE_RESTART?PERF_RESTART:PERF_BACK;s->armed=0;return MENU_CHANGED;}
        if(s->page==PAGE_PERFORMANCE){s->page=s->performance_dirty?PAGE_PERFORMANCE_DISCARD:PAGE_SETTINGS_HOME;s->selection=s->performance_dirty?0:4;s->armed=0;return MENU_CHANGED;}
        if(s->page==PAGE_PHOTO_EFFECTS||s->page==PAGE_PHOTO_WATERMARK){int page=s->page;s->page=PAGE_PHOTO;s->selection=page==PAGE_PHOTO_EFFECTS?9:10;s->armed=0;return MENU_CHANGED;}
        if(s->page==PAGE_PHOTO_CONTROLS){s->page=PAGE_PHOTO;s->selection=8;s->armed=0;return MENU_CHANGED;}
        if(s->page==PAGE_PHOTO){s->page=PAGE_MAIN;s->selection=MAIN_PHOTO;s->armed=0;return MENU_CHANGED;}
        if(menu_setting_page(s->page)&&s->page!=PAGE_SETTINGS){int page=s->page;s->page=PAGE_SETTINGS_HOME;s->selection=page-PAGE_APPEARANCE;s->armed=0;return MENU_CHANGED;}
        if(s->page==PAGE_SETTINGS_HOME){s->page=PAGE_MAIN;s->selection=MAIN_SETTINGS;s->armed=0;return MENU_CHANGED;}
        if(s->page==PAGE_BINDING){s->page=PAGE_KEYBINDS;s->selection=s->binding_action;s->armed=0;return MENU_CHANGED;}
        if(s->page==PAGE_KEYBINDS||s->page==PAGE_THEME){int id=s->page==PAGE_KEYBINDS?SETTING_KEYBINDS:SETTING_THEME;menu_return_setting(s,id);return MENU_CHANGED;}
        if(s->page==PAGE_QUIT){s->page=PAGE_MAIN;s->selection=MAIN_QUIT;s->armed=0;return MENU_CHANGED;}
        if(s->page==PAGE_BACKUPS){s->page=PAGE_CHEATS;s->selection=10;s->armed=0;return MENU_CHANGED;}
        if(s->page==PAGE_GAME_ICONS_CONFIRM){s->page=PAGE_GAME_ICONS;s->selection=s->game_icons_xbox?0:1;s->armed=0;return MENU_CHANGED;}
        if(s->page==PAGE_GAME_ICONS){menu_return_setting(s,SETTING_GAME_ICONS);return MENU_CHANGED;}
        if(s->page==PAGE_CAREER_ADD){s->page=PAGE_CHEATS;s->selection=menu_career_row(s->career_kind);s->armed=0;return MENU_CHANGED;}
        if(s->page==PAGE_CHEATS){s->page=PAGE_MAIN;s->selection=MAIN_CHEATS;s->armed=0;return MENU_CHANGED;}
        if(s->page==PAGE_RESET){menu_return_setting(s,SETTING_RESET);return MENU_CHANGED;}
        if(s->page==PAGE_SCREENSHOT){s->page=PAGE_MAIN;s->selection=MAIN_SCREENSHOT;s->armed=0;return MENU_CHANGED;}
        if(menu_power_page(s)){menu_cancel_shutdown(s);s->selection=menu_power_selection(s);s->page=PAGE_MAIN;s->armed=0;return MENU_CHANGED;}
        if(s->page!=PAGE_MAIN){s->selection=s->page==PAGE_RESTART?MAIN_RESTART:MAIN_SETTINGS;s->page=PAGE_MAIN;s->armed=0;return MENU_CHANGED;}
        return menu_close(s,MENU_RESUME);
    }
    if(button==PAD_UP||button==PAD_DOWN) {
        if(menu_power_page(s))menu_cancel_shutdown(s);
        int count=menu_rows(s);s->selection=(s->selection+(button==PAD_UP?count-1:1))%count;
        return MENU_CHANGED;
    }
    if(s->page==PAGE_PERFORMANCE&&(button==PAD_A||button==PAD_LEFT||button==PAD_RIGHT)){
        s->armed=0;
        if(s->selection==PERF_BACK){if(button!=PAD_A)return MENU_NONE;s->page=s->performance_dirty?PAGE_PERFORMANCE_DISCARD:PAGE_SETTINGS_HOME;s->selection=s->performance_dirty?0:4;return MENU_CHANGED;}
        if(!s->performance_available)return MENU_PERFORMANCE_OPEN;
        if(s->selection<PERF_SAVE){performance_adjust(&s->performance,s->selection,button==PAD_LEFT?-1:1);s->performance_feedback=0;s->performance_dirty=!performance_equal(&s->performance,&s->performance_original);return MENU_CHANGED;}
        if(button!=PAD_A)return MENU_NONE;
        if(s->selection==PERF_SAVE)return MENU_PERFORMANCE_SAVE;
        s->page=PAGE_PERFORMANCE_RESTART;s->selection=0;return MENU_CHANGED;
    }
    if(s->page==PAGE_PERFORMANCE_RESTART&&button==PAD_A){s->armed=0;if(s->selection==1)return MENU_PERFORMANCE_RESTART;s->page=PAGE_PERFORMANCE;s->selection=PERF_RESTART;return MENU_CHANGED;}
    if(s->page==PAGE_PERFORMANCE_DISCARD&&button==PAD_A){s->armed=0;if(s->selection==1){s->performance=s->performance_original;s->performance_dirty=0;s->page=PAGE_SETTINGS_HOME;s->selection=4;}else{s->page=PAGE_PERFORMANCE;s->selection=PERF_BACK;}return MENU_CHANGED;}
    if((s->page==PAGE_PHOTO_EFFECTS||s->page==PAGE_PHOTO_WATERMARK)&&(button==PAD_A||button==PAD_LEFT||button==PAD_RIGHT)){
        int direction=button==PAD_LEFT?-1:1;s->armed=0;
        if(s->selection==menu_rows(s)-1){if(button==PAD_A){int page=s->page;s->page=PAGE_PHOTO;s->selection=page==PAGE_PHOTO_EFFECTS?9:10;}return MENU_CHANGED;}
        if(s->page==PAGE_PHOTO_EFFECTS)photo_effects_change(&s->effects,s->selection,direction);
        else photo_watermark_change(&s->effects,s->selection,direction);
        return MENU_PHOTO_EFFECTS_CHANGED;
    }
    if(s->page==PAGE_PHOTO&&button==PAD_A){
        s->armed=0;
        if(s->selection==0)return menu_close(s,MENU_PHOTO_ENTER);
        if(s->selection==1)return MENU_PHOTO_FREEZE;
        if(s->selection==2)return MENU_PHOTO_HUD;
        if(s->selection==3)return MENU_PHOTO_CAGE;
        if(s->selection==4)return MENU_PHOTO_HELP;
        if(s->selection==5)return MENU_PHOTO_RESET;
        if(s->selection==6)return MENU_SCREENSHOT;
        if(s->selection==7)return menu_close(s,MENU_PHOTO_EXIT);
        if(s->selection==8){s->page=PAGE_PHOTO_CONTROLS;s->selection=0;return MENU_CHANGED;}
        if(s->selection==9||s->selection==10){s->page=s->selection==9?PAGE_PHOTO_EFFECTS:PAGE_PHOTO_WATERMARK;s->selection=0;return MENU_CHANGED;}
        s->page=PAGE_MAIN;s->selection=MAIN_PHOTO;return MENU_CHANGED;
    }
    if(s->page==PAGE_PHOTO_CONTROLS&&button==PAD_A){s->page=PAGE_PHOTO;s->selection=8;s->armed=0;return MENU_CHANGED;}
    if(s->page==PAGE_SETTINGS_HOME&&button==PAD_A){
        if(s->selection==5){s->page=PAGE_MAIN;s->selection=MAIN_SETTINGS;}
        else if(s->selection==4){s->page=PAGE_PERFORMANCE;s->selection=0;s->armed=0;return MENU_PERFORMANCE_OPEN;}
        else {s->page=PAGE_APPEARANCE+s->selection;s->selection=0;}
        s->armed=0;return MENU_CHANGED;
    }
    if(menu_setting_page(s->page)&&s->page!=PAGE_SETTINGS){
        int page=s->page,row=s->selection,id=menu_setting_id(page,row);
        if(id==SETTING_EXPERIMENTAL&&button==PAD_A){s->page=PAGE_EXPERIMENTAL;s->selection=0;s->armed=0;return MENU_CHANGED;}
        if(page==PAGE_EXPERIMENTAL&&id==SETTING_BACK&&button==PAD_A){s->page=PAGE_GAME_SETTINGS;s->selection=6;s->armed=0;return MENU_CHANGED;}
        if(id==SETTING_BACK&&button==PAD_A){s->page=PAGE_SETTINGS_HOME;s->selection=page-PAGE_APPEARANCE;s->armed=0;return MENU_CHANGED;}
        s->page=PAGE_SETTINGS;s->selection=id;int result=menu_button(s,button,down);
        if(s->page==PAGE_SETTINGS){s->page=page;s->selection=row;}return result;
    }
    if(s->page==PAGE_KEYBINDS&&button==PAD_A){
        if(s->selection==10){menu_return_setting(s,SETTING_KEYBINDS);}
        else{s->binding_action=s->selection;s->page=PAGE_BINDING;s->selection=0;}
        s->armed=0;return MENU_CHANGED;
    }
    if(s->page==PAGE_THEME&&button==PAD_A){
        if(s->selection==6){menu_return_setting(s,SETTING_THEME);return MENU_CHANGED;}
        s->theme_selection=s->selection;return MENU_THEME_CHANGED;
    }
    if(s->page==PAGE_BINDING&&button==PAD_A){
        s->armed=0;
        if(s->selection==0){s->capturing=1;s->key_capturing=1;return MENU_BIND_KEY_CAPTURE;}
        if(s->selection==1){s->capture_target=s->binding_action;s->capturing=1;s->capture_ready=0;s->capture_mask=0;s->capture_since=0;s->capture_error=0;return MENU_CHANGED;}
        if(s->selection==2)return MENU_BIND_CLEAR_KEY;
        if(s->selection==3)return MENU_BIND_CLEAR_PAD;
        s->page=PAGE_KEYBINDS;s->selection=s->binding_action;return MENU_CHANGED;
    }
    if(s->page==PAGE_QUIT&&button==PAD_A){
        if(s->selection==1)return menu_close(s,MENU_QUIT);
        s->page=PAGE_MAIN;s->selection=MAIN_QUIT;s->armed=0;return MENU_CHANGED;
    }
    if(s->page==PAGE_BACKUPS){
        if(button!=PAD_A)return MENU_NONE;
        s->armed=0;
        if(s->selection==0)return MENU_BACKUP_CREATE;
        if(s->selection==1)return MENU_BACKUP_FOLDER;
        s->page=PAGE_CHEATS;s->selection=10;return MENU_CHANGED;
    }
    if(s->page==PAGE_STARTUP){
        if(button!=PAD_A)return MENU_NONE;
        if(s->selection==2)menu_return_setting(s,SETTING_STARTUP_PATCH);
        else{s->startup_patch_enable=s->selection==0;s->page=PAGE_STARTUP_CONFIRM;s->selection=0;}
        s->armed=0;return MENU_CHANGED;
    }
    if(s->page==PAGE_STARTUP_CONFIRM){
        if(button!=PAD_A)return MENU_NONE;int apply=s->selection==1;
        s->page=PAGE_STARTUP;s->selection=2;s->armed=0;return apply?MENU_STARTUP_PATCH_APPLY:MENU_CHANGED;
    }
    if(s->page==PAGE_GAME_ICONS){
        if(button!=PAD_A)return MENU_NONE;
        if(s->selection==2){menu_return_setting(s,SETTING_GAME_ICONS);}
        else{s->game_icons_xbox=s->selection==0;s->page=PAGE_GAME_ICONS_CONFIRM;s->selection=0;}
        s->armed=0;return MENU_CHANGED;
    }
    if(s->page==PAGE_GAME_ICONS_CONFIRM){
        if(button!=PAD_A)return MENU_NONE;int apply=s->selection==1;
        s->page=PAGE_GAME_ICONS;s->selection=2;s->armed=0;return apply?MENU_GAME_ICONS_APPLY:MENU_CHANGED;
    }
    if(s->page==PAGE_CAREER_ADD){
        if(button!=PAD_A)return MENU_NONE;
        int add=s->selection==1;s->page=PAGE_CHEATS;s->selection=menu_career_row(s->career_kind);s->armed=0;
        return add?MENU_CAREER_ADD:MENU_CHANGED;
    }
    if(s->page==PAGE_CHEATS){
        if(s->selection==2&&button==PAD_A){s->armed=0;return MENU_NEXT_HIT_KO;}
        if(s->selection>=3&&s->selection<=4){
            int kind=s->selection-3;
            if(button==PAD_LEFT||button==PAD_RIGHT){
                int i=s->career_amount_index[kind]+(button==PAD_LEFT?-1:1),n=kind==0?5:9;
                if(i<0||i>=n)return MENU_NONE;s->career_amount_index[kind]=i;return MENU_CHANGED;
            }
            if(button==PAD_A){s->career_kind=kind;s->page=PAGE_CAREER_ADD;s->selection=0;s->armed=0;return MENU_CHANGED;}
        }
        if(s->selection<2&&(button==PAD_A||button==PAD_LEFT||button==PAD_RIGHT)){s->armed=0;return s->selection==0?MENU_CHEAT_STAMINA:MENU_CHEAT_HEALTH;}
        if(s->selection==5&&button==PAD_A){s->career_kind=2;s->page=PAGE_CAREER_ADD;s->selection=0;s->armed=0;return MENU_CHANGED;}
        if(s->selection==6&&button==PAD_A){s->career_kind=3;s->page=PAGE_CAREER_ADD;s->selection=0;s->armed=0;return MENU_CHANGED;}
        if(s->selection==7&&(button==PAD_A||button==PAD_LEFT||button==PAD_RIGHT)){s->armed=0;return MENU_CAREER_LONGEVITY;}
        if(s->selection==8&&(button==PAD_A||button==PAD_LEFT||button==PAD_RIGHT)){s->training_multiplier_index=(s->training_multiplier_index+(button==PAD_LEFT?3:1))%4;return MENU_TRAINING_MULTIPLIER;}
        if(s->selection==9&&button==PAD_A){s->career_kind=5;s->page=PAGE_CAREER_ADD;s->selection=0;s->armed=0;return MENU_CHANGED;}
        if(s->selection==10&&button==PAD_A){s->page=PAGE_BACKUPS;s->selection=0;s->armed=0;return MENU_BACKUP_OPEN;}
        if(s->selection==11&&button==PAD_A){s->page=PAGE_MAIN;s->selection=MAIN_CHEATS;s->armed=0;return MENU_CHANGED;}
        return MENU_NONE;
    }
    if(s->page==PAGE_SETTINGS) {
        if(s->selection==SETTING_INSTANT_REPLAY&&(button==PAD_A||button==PAD_LEFT||button==PAD_RIGHT)){s->instant_replay=!s->instant_replay;return MENU_INSTANT_REPLAY_CHANGED;}
        if(s->selection==SETTING_DECISION_REPLAY&&(button==PAD_A||button==PAD_LEFT||button==PAD_RIGHT)){s->decision_replays=!s->decision_replays;return MENU_DECISION_REPLAY_CHANGED;}
        if(s->selection==SETTING_AUDIO_FOCUS&&(button==PAD_A||button==PAD_LEFT||button==PAD_RIGHT)){s->mute_background=!s->mute_background;return MENU_AUDIO_FOCUS_CHANGED;}
        if(s->selection==SETTING_SCREENSHOT_UI&&(button==PAD_A||button==PAD_LEFT||button==PAD_RIGHT)){s->screenshot_include_ui=!s->screenshot_include_ui;s->armed=0;return MENU_SCREENSHOT_UI_CHANGED;}
        if((s->selection==SETTING_STARTUP_POPUPS||s->selection==SETTING_SKIP_SPLASHES||s->selection==SETTING_SUPPRESS_ONLINE_NOTICES)&&(button==PAD_LEFT||button==PAD_RIGHT||button==PAD_A)){
            if(s->selection==SETTING_STARTUP_POPUPS)s->startup_popups=!s->startup_popups;
            else if(s->selection==SETTING_SUPPRESS_ONLINE_NOTICES){s->suppress_online_notices=!s->suppress_online_notices;return MENU_ONLINE_NOTICES_CHANGED;}
            else {s->skip_splashes=!s->skip_splashes;s->startup_popups=s->skip_splashes;s->suppress_online_notices=s->skip_splashes;}
            return MENU_STARTUP_CHANGED;
        }
        if(s->selection==SETTING_WINDOW_SIZE&&(button==PAD_LEFT||button==PAD_RIGHT||button==PAD_A)){s->window_size=(s->window_size+(button==PAD_LEFT?2:1))%3;return MENU_WINDOW_SIZE_CHANGED;}
        if(s->selection==SETTING_FONT&&(button==PAD_LEFT||button==PAD_RIGHT||button==PAD_A)){s->font_index=(s->font_index+(button==PAD_LEFT?2:1))%3;return MENU_FONT_CHANGED;}
        if(s->selection==SETTING_BOX_STYLE&&(button==PAD_LEFT||button==PAD_RIGHT||button==PAD_A)){s->box_style=(s->box_style+(button==PAD_LEFT?4:1))%5;return MENU_BOX_STYLE_CHANGED;}
        if(s->selection==SETTING_KEYBINDS&&button==PAD_A){s->page=PAGE_KEYBINDS;s->selection=0;s->armed=0;return MENU_CHANGED;}
        if(s->selection==SETTING_THEME&&button==PAD_A){s->page=PAGE_THEME;s->selection=s->theme_selection;s->armed=0;return MENU_CHANGED;}
        if(s->selection==SETTING_DIAGNOSTICS&&button==PAD_A){s->armed=0;return MENU_EXPORT_DIAGNOSTICS;}
        if(s->selection==SETTING_UPDATE&&button==PAD_A){s->armed=0;return MENU_CHECK_UPDATES;}
        if(s->selection==SETTING_STARTUP_PATCH&&button==PAD_A){s->page=PAGE_STARTUP;s->selection=2;s->armed=0;return MENU_STARTUP_PATCH_OPEN;}
        if(s->selection==SETTING_GAME_ICONS&&button==PAD_A){s->page=PAGE_GAME_ICONS;s->selection=2;s->armed=0;return MENU_GAME_ICONS_OPEN;}
        if(s->selection==SETTING_SCREENSHOTS&&button==PAD_A){s->armed=0;return MENU_OPEN_SCREENSHOTS;}
        if(s->selection==SETTING_RESET&&button==PAD_A){s->page=PAGE_RESET;s->selection=0;s->armed=0;return MENU_CHANGED;}
        if(s->selection==SETTING_ACCESS&&(button==PAD_A||button==PAD_LEFT||button==PAD_RIGHT)){s->first_player_only=!s->first_player_only;s->armed=0;menu_cancel_shutdown(s);return MENU_ACCESS_CHANGED;}
        if(s->selection==SETTING_LAUNCHER&&button==PAD_A){s->armed=0;return MENU_LAUNCHER_SETTINGS;}
        if(s->selection==SETTING_GAMEBAR&&(button==PAD_LEFT||button==PAD_RIGHT||button==PAD_A)){
            s->armed=0;return MENU_GAMEBAR_CHANGED;
        }
        if(s->selection==SETTING_GUIDE&&(button==PAD_LEFT||button==PAD_RIGHT||button==PAD_A)){
            s->disable_guide=!s->disable_guide;return MENU_GUIDE_CHANGED;
        }
        if(s->selection==SETTING_AUTO_LAUNCH&&(button==PAD_LEFT||button==PAD_RIGHT||button==PAD_A)){
            s->disable_auto_launch=!s->disable_auto_launch;return MENU_AUTO_LAUNCH_CHANGED;
        }
        if(s->selection==SETTING_SCALE&&(button==PAD_LEFT||button==PAD_RIGHT||button==PAD_A)){
            int scale=menu_scale(s);
            int value=button==PAD_A?MENU_SCALE_DEFAULT:scale+(button==PAD_LEFT?-MENU_SCALE_STEP:MENU_SCALE_STEP);
            if(!overlay_valid_scale(value)||value==scale)return MENU_NONE;
            s->ui_scale_percent=value;return MENU_SCALE_CHANGED;
        }
        if(s->selection==SETTING_ICONS&&(button==PAD_A||button==PAD_LEFT||button==PAD_RIGHT)) {
            s->playstation_icons=!s->playstation_icons;return MENU_STYLE_CHANGED;
        }
        if(button!=PAD_A)return MENU_NONE;
        if(s->selection==SETTING_SHORTCUT) {
            s->capture_target=0;
            s->capturing=1;s->capture_ready=0;s->capture_mask=0;s->capture_since=0;
            s->capture_error=0;s->armed=0;return MENU_CHANGED;
        }
        s->page=PAGE_MAIN;s->selection=MAIN_SETTINGS;s->armed=0;return MENU_CHANGED;
    }
    if(button!=PAD_A)return MENU_NONE;
    if(s->page==PAGE_SCREENSHOT){
        if(s->selection==0){s->armed=0;return MENU_OPEN_SCREENSHOTS;}
        s->page=PAGE_MAIN;s->selection=MAIN_SCREENSHOT;s->armed=0;return MENU_CHANGED;
    }
    if(s->page==PAGE_RESET){
        s->armed=0;
        if(s->selection==1)return MENU_RESET_SETTINGS;
        menu_return_setting(s,SETTING_RESET);return MENU_CHANGED;
    }
    if(menu_power_page(s)) {
        if(s->shutdown_countdown||s->selection==0){menu_cancel_shutdown(s);s->selection=menu_power_selection(s);s->page=PAGE_MAIN;s->armed=0;return MENU_CHANGED;}
        s->shutdown_holding=1;s->shutdown_hold_since=0;s->shutdown_progress=0;return MENU_CHANGED;
    }
    if(s->page==PAGE_RESTART) {
        if(s->selection==1)return menu_close(s,MENU_RESTART);
        s->page=PAGE_MAIN;s->selection=MAIN_RESTART;s->armed=0;return MENU_CHANGED;
    }
    switch(s->selection) {
        case MAIN_RESUME:return menu_close(s,MENU_RESUME);
        case MAIN_SCREENSHOT:return MENU_SCREENSHOT;
        case MAIN_RESTART:s->page=PAGE_RESTART;s->selection=0;s->armed=0;return MENU_CHANGED;
        case MAIN_PHOTO:s->page=PAGE_PHOTO;s->selection=0;s->armed=0;return MENU_CHANGED;
        case MAIN_CHEATS:s->page=PAGE_CHEATS;s->selection=0;s->armed=0;return MENU_CHANGED;
        case MAIN_SETTINGS:s->page=s->settings_legacy?PAGE_SETTINGS:PAGE_SETTINGS_HOME;s->selection=SETTING_ICONS;s->armed=0;return MENU_CHANGED;
        case MAIN_REBOOT:case MAIN_SHUTDOWN:menu_cancel_shutdown(s);s->page=s->selection==MAIN_REBOOT?PAGE_REBOOT:PAGE_SHUTDOWN;s->selection=0;s->armed=0;return MENU_CHANGED;
        case MAIN_QUIT:return menu_close(s,MENU_QUIT);
        default:return MENU_NONE;
    }
}
#endif
