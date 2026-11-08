#undef NDEBUG
#include <assert.h>
#include <stdio.h>
#include "../src/menu_state.h"

int main(void) {
    MenuState s={0};
    assert(menu_tick(&s,OPEN_CHORD,1000,1)==MENU_NONE);
    assert(menu_tick(&s,OPEN_CHORD,1799,1)==MENU_NONE);
    assert(menu_tick(&s,OPEN_CHORD,1800,1)==MENU_OPEN);
    assert(s.page==PAGE_MAIN&&s.selection==MAIN_RESUME&&!s.armed);
    assert(menu_button(&s,PAD_A,1)==MENU_NONE);
    menu_tick(&s,0,1900,1);
    assert(menu_button(&s,PAD_UP,1)==MENU_CHANGED&&s.selection==MAIN_SHUTDOWN);
    assert(menu_button(&s,PAD_DOWN,1)==MENU_CHANGED&&s.selection==MAIN_RESUME);
    menu_button(&s,PAD_DOWN,1);
    assert(menu_button(&s,PAD_A,1)==MENU_SCREENSHOT&&s.visible&&!s.draining);
    menu_button(&s,PAD_DOWN,1);
    assert(menu_button(&s,PAD_A,1)==MENU_CHANGED&&s.page==PAGE_RESTART&&s.selection==0&&!s.armed);
    assert(menu_button(&s,PAD_A,1)==MENU_NONE); /* No restart from a held confirmation. */
    menu_tick(&s,0,2000,1);
    assert(menu_button(&s,PAD_A,1)==MENU_CHANGED&&s.page==PAGE_MAIN&&s.selection==MAIN_RESTART);
    menu_tick(&s,0,2100,1);menu_button(&s,PAD_A,1);menu_tick(&s,0,2200,1);
    menu_button(&s,PAD_DOWN,1);
    assert(menu_button(&s,PAD_A,1)==MENU_RESTART&&!s.visible&&s.draining);
    menu_tick(&s,PAD_A,2300,1);assert(s.draining);
    menu_tick(&s,0,2400,1);assert(!s.draining);
    menu_show(&s);menu_tick(&s,0,2500,1);
    s.selection=MAIN_SETTINGS;
    assert(menu_button(&s,PAD_A,1)==MENU_CHANGED&&s.page==PAGE_SETTINGS);
    menu_tick(&s,0,2600,1);
    assert(menu_button(&s,PAD_A,1)==MENU_STYLE_CHANGED&&s.playstation_icons&&s.visible);
    assert(menu_button(&s,PAD_LEFT,1)==MENU_STYLE_CHANGED&&!s.playstation_icons);
    assert(menu_button(&s,PAD_RIGHT,1)==MENU_STYLE_CHANGED&&s.playstation_icons);
    menu_button(&s,PAD_DOWN,1);
    assert(menu_button(&s,PAD_A,1)==MENU_CHANGED&&s.capturing);
    menu_tick(&s,PAD_A,2700,1);assert(!s.capture_ready&&!s.capture_mask);
    menu_tick(&s,0,2800,1);assert(s.capture_ready);
    menu_tick(&s,6,3000,1);menu_tick(&s,6,3400,1);
    assert(menu_tick(&s,0,3500,1)==MENU_SHORTCUT_CHANGED&&menu_shortcut(&s)==6&&!s.capturing);
    menu_tick(&s,0,3600,1);
    assert(menu_button(&s,PAD_B,1)==MENU_CHANGED&&s.page==PAGE_MAIN&&s.selection==MAIN_SETTINGS);
    menu_tick(&s,0,3700,1);assert(menu_button(&s,PAD_B,1)==MENU_RESUME);
    menu_tick(&s,0,3800,1);
    menu_tick(&s,OPEN_CHORD,4000,1);
    assert(menu_tick(&s,OPEN_CHORD,5000,1)==MENU_NONE&&!s.visible);
    menu_tick(&s,0,5100,1);menu_tick(&s,6,5200,1);
    assert(menu_tick(&s,6,6000,1)==MENU_OPEN&&s.playstation_icons);
    menu_tick(&s,0,6100,1);s.selection=MAIN_SETTINGS;menu_button(&s,PAD_A,1);menu_tick(&s,0,6200,1);
    s.selection=SETTING_SHORTCUT;menu_button(&s,PAD_A,1);menu_tick(&s,0,6300,1);
    menu_tick(&s,PAD_A,6400,1);
    assert(menu_tick(&s,0,6800,1)==MENU_CHANGED&&s.capturing&&s.capture_error&&menu_shortcut(&s)==6);
    menu_tick(&s,PAD_B,6900,1);
    assert(menu_tick(&s,0,7000,1)==MENU_CHANGED&&!s.capturing&&menu_shortcut(&s)==6);
    assert(!menu_valid_shortcut(PAD_A)&&!menu_valid_shortcut(0)&&!menu_valid_shortcut(0x100006));
    assert(menu_valid_shortcut(OPEN_CHORD)&&menu_valid_shortcut(6));
    menu_close(&s,MENU_RESUME);menu_tick(&s,0,7100,1);
    menu_tick(&s,6,7200,0);assert(menu_tick(&s,6,8500,1)==MENU_NONE);
    menu_tick(&s,0,8600,1);menu_tick(&s,6,8700,1);
    assert(menu_tick(&s,6,9500,1)==MENU_OPEN);
    menu_tick(&s,0,9600,1);s.selection=MAIN_QUIT;
    assert(menu_button(&s,PAD_A,1)==MENU_QUIT&&!s.visible);
    s=(MenuState){0};menu_show(&s);menu_tick(&s,0,1000,1);s.selection=MAIN_SHUTDOWN;
    assert(menu_button(&s,PAD_A,1)==MENU_CHANGED&&s.page==PAGE_SHUTDOWN&&s.selection==0&&!s.armed);
    assert(menu_button(&s,PAD_A,1)==MENU_NONE);
    menu_tick(&s,0,1100,1);menu_button(&s,PAD_DOWN,1);menu_button(&s,PAD_A,1);
    menu_tick(&s,PAD_A,1200,1);
    assert(menu_tick(&s,PAD_A,4199,1)==MENU_CHANGED&&!s.shutdown_countdown);
    menu_tick(&s,0,4200,1);assert(!s.shutdown_holding&&!s.shutdown_countdown);
    menu_button(&s,PAD_A,1);menu_tick(&s,PAD_A,5000,1);menu_tick(&s,PAD_A,8000,1);
    assert(s.shutdown_countdown&&s.shutdown_seconds==10&&!s.armed);
    assert(menu_tick(&s,PAD_A|PAD_B,8001,1)==MENU_CHANGED&&s.page==PAGE_MAIN&&!s.shutdown_countdown);
    menu_tick(&s,0,8100,1);menu_button(&s,PAD_A,1);menu_tick(&s,0,8200,1);
    menu_button(&s,PAD_DOWN,1);menu_button(&s,PAD_A,1);
    menu_tick(&s,PAD_A,10000,1);menu_tick(&s,PAD_A,13000,1);
    menu_tick(&s,0,15000,1);assert(s.shutdown_countdown&&s.shutdown_seconds==8);
    assert(menu_tick(&s,0,16000,0)==MENU_CHANGED&&!s.shutdown_countdown&&s.selection==0);
    menu_tick(&s,0,16100,1);menu_button(&s,PAD_DOWN,1);menu_button(&s,PAD_A,1);
    menu_tick(&s,PAD_A,17000,1);menu_tick(&s,PAD_A,20000,1);
    assert(menu_tick(&s,0,29999,1)==MENU_CHANGED&&s.shutdown_countdown);
    assert(menu_tick(&s,0,30000,1)==MENU_SHUTDOWN&&!s.visible&&!s.shutdown_countdown);
    assert(menu_tick(&s,0,31000,1)==MENU_NONE);
    /* Reboot uses the same release, hold, countdown and cancellation gates. */
    for(int cancel=0;cancel<4;++cancel){
        s=(MenuState){0};menu_show(&s);menu_tick(&s,0,1000,1);s.selection=MAIN_REBOOT;
        assert(menu_button(&s,PAD_A,1)==MENU_CHANGED&&s.page==PAGE_REBOOT&&s.selection==0&&!s.armed);
        assert(menu_button(&s,PAD_A,1)==MENU_NONE);
        menu_tick(&s,0,1100,1);
        if(cancel==3){assert(menu_button(&s,PAD_A,1)==MENU_CHANGED&&s.page==PAGE_MAIN&&s.selection==MAIN_REBOOT);continue;}
        menu_button(&s,PAD_DOWN,1);menu_button(&s,PAD_A,1);
        menu_tick(&s,PAD_A,2000,1);menu_tick(&s,PAD_A,4999,1);assert(!s.shutdown_countdown);
        menu_tick(&s,PAD_A,5000,1);assert(s.shutdown_countdown&&s.shutdown_seconds==10);
        if(cancel==1){menu_tick(&s,PAD_B,5001,1);assert(s.page==PAGE_MAIN&&s.selection==MAIN_REBOOT&&!s.shutdown_countdown);}
        else if(cancel==2){menu_tick(&s,0,5001,0);assert(!s.shutdown_countdown&&s.selection==0);}
        else {assert(menu_tick(&s,0,14999,1)==MENU_CHANGED);assert(menu_tick(&s,0,15000,1)==MENU_REBOOT&&!s.visible);}
    }
    s=(MenuState){.visible=1,.armed=1,.page=PAGE_SETTINGS,.selection=SETTING_SCALE};
    assert(menu_scale(&s)==100);
    assert(menu_auto_launch(&s));
    s.selection=SETTING_AUTO_LAUNCH;
    assert(menu_button(&s,PAD_A,1)==MENU_AUTO_LAUNCH_CHANGED&&!menu_auto_launch(&s)&&s.visible);
    assert(menu_button(&s,PAD_A,0)==MENU_NONE&&!menu_auto_launch(&s));
    menu_close(&s,MENU_RESUME);menu_show(&s);assert(!menu_auto_launch(&s));
    s.page=PAGE_SETTINGS;s.selection=SETTING_AUTO_LAUNCH;s.armed=1;
    assert(menu_button(&s,PAD_RIGHT,1)==MENU_AUTO_LAUNCH_CHANGED&&menu_auto_launch(&s));
    assert(menu_button(&s,PAD_LEFT,1)==MENU_AUTO_LAUNCH_CHANGED&&!menu_auto_launch(&s));
    s.selection=SETTING_SCALE;
    assert(menu_button(&s,PAD_RIGHT,1)==MENU_SCALE_CHANGED&&menu_scale(&s)==125);
    assert(menu_button(&s,PAD_RIGHT,0)==MENU_NONE&&menu_scale(&s)==125);
    assert(menu_button(&s,PAD_A,1)==MENU_SCALE_CHANGED&&menu_scale(&s)==100&&s.visible);
    assert(menu_button(&s,PAD_A,1)==MENU_NONE);
    menu_button(&s,PAD_LEFT,1);menu_button(&s,PAD_LEFT,1);
    assert(menu_scale(&s)==50&&menu_button(&s,PAD_LEFT,1)==MENU_NONE);
    for(int i=0;i<10;++i)assert(menu_button(&s,PAD_RIGHT,1)==MENU_SCALE_CHANGED);
    assert(menu_scale(&s)==300&&menu_button(&s,PAD_RIGHT,1)==MENU_NONE);
    menu_close(&s,MENU_RESUME);menu_show(&s);assert(menu_scale(&s)==300);
    assert(!overlay_valid_scale(0)&&!overlay_valid_scale(101)&&!overlay_valid_scale(325));
    uint32_t width,height;
    overlay_extent(100,2560,1440,&width,&height);assert(width==570&&height==372);
    overlay_extent(200,2560,1440,&width,&height);assert(width==1140&&height==744);
    overlay_extent(300,640,360,&width,&height);assert(width<=608&&height<=328);
    const uint32_t sizes[][2]={{320,240},{640,360},{1280,720},{2560,1440},{3840,2160}};
    for(unsigned i=0;i<sizeof(sizes)/sizeof(sizes[0]);++i)for(int scale=50;scale<=300;scale+=25){
        overlay_extent(scale,sizes[i][0],sizes[i][1],&width,&height);
        assert(width&&height&&width<=sizes[i][0]-32&&height<=sizes[i][1]-32);
        int aspect=(int)(width*372)-(int)(height*570);assert(aspect>=-1140&&aspect<=1140);
    }
    overlay_extent(300,32,32,&width,&height);assert(!width&&!height);
    puts("PASS: menu safeguards, UI scaling bounds/reset/persistence, and fitting from 320x240 to 4K.");
    return 0;
}
