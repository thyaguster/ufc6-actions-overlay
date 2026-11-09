#ifndef UFC6_MENU_STATE_H
#define UFC6_MENU_STATE_H
#include <stdint.h>
#include "overlay_layout.h"

enum {
    PAD_MENU=0x8, PAD_UP=0x10, PAD_RIGHT=0x20, PAD_DOWN=0x40, PAD_LEFT=0x80,
    PAD_LB=0x400, PAD_RB=0x800, PAD_B=0x2000, PAD_A=0x4000,
    OPEN_CHORD=PAD_LB|PAD_RB|PAD_MENU,
    SHORTCUT_BUTTONS=0x0000fcfe
};
enum { PAGE_MAIN, PAGE_SETTINGS, PAGE_RESTART, PAGE_SHUTDOWN, PAGE_REBOOT, PAGE_SCREENSHOT, PAGE_RESET };
enum { MAIN_RESUME, MAIN_SCREENSHOT, MAIN_RESTART, MAIN_SETTINGS, MAIN_QUIT, MAIN_REBOOT, MAIN_SHUTDOWN, MAIN_COUNT };
enum { SETTING_ICONS, SETTING_SHORTCUT, SETTING_SCALE, SETTING_AUTO_LAUNCH, SETTING_GUIDE, SETTING_GAMEBAR, SETTING_LAUNCHER, SETTING_ACCESS, SETTING_SCREENSHOTS, SETTING_RESET, SETTING_BACK, SETTING_COUNT };
enum { MENU_NONE, MENU_OPEN, MENU_RESUME, MENU_QUIT, MENU_CHANGED,
       MENU_STYLE_CHANGED, MENU_SHORTCUT_CHANGED, MENU_SCREENSHOT, MENU_RESTART, MENU_SHUTDOWN, MENU_SCALE_CHANGED, MENU_REBOOT, MENU_AUTO_LAUNCH_CHANGED, MENU_GUIDE_CHANGED, MENU_GAMEBAR_CHANGED, MENU_LAUNCHER_SETTINGS, MENU_ACCESS_CHANGED, MENU_OPEN_SCREENSHOTS, MENU_RESET_SETTINGS };

typedef struct {
    int visible,selection,armed,chord_latched,draining;
    int playstation_icons,page;
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
} MenuState;

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
static int menu_rows(const MenuState *s) {return s->page==PAGE_MAIN?MAIN_COUNT:s->page==PAGE_SETTINGS?SETTING_COUNT:s->shutdown_countdown?1:2;}
static void menu_cancel_shutdown(MenuState *s) {
    s->shutdown_holding=0;s->shutdown_countdown=0;s->shutdown_progress=0;
    s->shutdown_hold_since=0;s->shutdown_countdown_since=0;s->shutdown_seconds=0;
}
static void menu_show(MenuState *s) {
    s->visible=1;s->page=PAGE_MAIN;s->selection=MAIN_RESUME;s->armed=0;
    s->capturing=0;s->capture_error=0;
    menu_cancel_shutdown(s);
}
static int menu_close(MenuState *s,int action) {
    s->visible=0;s->draining=1;s->armed=0;s->capturing=0;menu_cancel_shutdown(s);return action;
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
                s->open_chord=mask;s->capturing=0;s->armed=0;s->chord_since=0;s->chord_latched=0;
                return MENU_SHORTCUT_CHANGED;
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
        if(s->page==PAGE_RESET){s->page=PAGE_SETTINGS;s->selection=SETTING_RESET;s->armed=0;return MENU_CHANGED;}
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
    if(s->page==PAGE_SETTINGS) {
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
        s->page=PAGE_SETTINGS;s->selection=SETTING_RESET;return MENU_CHANGED;
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
        case MAIN_SETTINGS:s->page=PAGE_SETTINGS;s->selection=SETTING_ICONS;s->armed=0;return MENU_CHANGED;
        case MAIN_REBOOT:case MAIN_SHUTDOWN:menu_cancel_shutdown(s);s->page=s->selection==MAIN_REBOOT?PAGE_REBOOT:PAGE_SHUTDOWN;s->selection=0;s->armed=0;return MENU_CHANGED;
        case MAIN_QUIT:return menu_close(s,MENU_QUIT);
        default:return MENU_NONE;
    }
}
#endif
