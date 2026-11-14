#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#undef NDEBUG
#include <assert.h>
#include <stdio.h>
#include "photo_effects_settings.h"
#pragma clang diagnostic ignored "-Wunused-function"
#include "menu_state.h"
#include "photo_panel_controls.h"
int main(void){
 PhotoEffects p;photo_effects_default(&p);assert(!photo_effects_needed(&p)&&sizeof(PhotoEffectConstants)==80);
 for(int row=0;row<FX_RESET;++row){photo_effects_default(&p);photo_effects_change(&p,row,1);assert(photo_effects_needed(&p)||row==FX_STRENGTH);photo_effects_change(&p,row,-1);assert(!photo_effects_needed(&p));}
 photo_effects_default(&p);p.watermark=1;wcscpy(p.name,L"Player test");assert(photo_effects_needed(&p));p.preset=4;photo_effects_change(&p,FX_RESET,1);assert(p.preset==0&&p.watermark&&p.name[0]);
 wchar_t base[MAX_PATH],path[MAX_PATH];GetTempPathW(MAX_PATH,base);GetTempFileNameW(base,L"pfx",0,path);DeleteFileW(path);
 assert(WritePrivateProfileStringW(L"Photo",L"WatermarkText",L"Player test",path));p.exposure=-3;p.corner=1;p.grain=4;p.mark_style=3;p.mark_font=2;p.mark_color=6;assert(photo_effects_save(&p,path));PhotoEffects q;photo_effects_load(&q,path);assert(q.exposure==-3&&q.corner==1&&q.grain==4&&q.mark_style==3&&q.mark_font==2&&q.mark_color==6&&q.watermark&&wcscmp(q.name,L"Player test")==0);
 assert(WritePrivateProfileStringW(L"Photo",L"Filter",L"999",path));assert(WritePrivateProfileStringW(L"Photo",L"WatermarkOpacity",L"0",path));photo_effects_load(&q,path);assert(q.preset==0&&q.opacity==4);DeleteFileW(path);
 PhotoPreset presets[PHOTO_PRESET_COUNT]={0};photo_preset_capture(&presets[1],&p,62,1,0);assert(photo_preset_save(&presets[1],1,path));
 PhotoPreset loaded[PHOTO_PRESET_COUNT];photo_presets_load(loaded,path);assert(!loaded[0].filled&&loaded[1].filled&&loaded[1].fov==62&&loaded[1].effects.mark_color==6);
 PhotoEffects target;photo_effects_default(&target);wcscpy(target.name,L"Different player");int fov,hud,cage;
 assert(photo_preset_apply(&loaded[1],&target,&fov,&hud,&cage)&&!wcscmp(target.name,L"Different player")&&target.exposure==-3&&fov==62&&hud==1&&cage==0);
 assert(WritePrivateProfileStringW(L"PhotoPresets",L"Slot3",L"bad data",path));photo_presets_load(loaded,path);assert(!loaded[2].filled);DeleteFileW(path);
 /* Resolve the launcher display name from the game root, preserving Unicode. */
 wchar_t game[MAX_PATH],bin[MAX_PATH],ini[MAX_PATH],launcher[MAX_PATH];GetTempFileNameW(base,L"pfn",0,game);DeleteFileW(game);assert(CreateDirectoryW(game,NULL));swprintf(bin,MAX_PATH,L"%ls\\bin",game);assert(CreateDirectoryW(bin,NULL));swprintf(ini,MAX_PATH,L"%ls\\ufc6-quit-menu.ini",bin);swprintf(launcher,MAX_PATH,L"%ls\\UFC6.ini",game);
 HANDLE f=CreateFileW(launcher,GENERIC_WRITE,0,NULL,CREATE_NEW,0,NULL);assert(f!=INVALID_HANDLE_VALUE);wchar_t bom=0xfeff;DWORD written;assert(WriteFile(f,&bom,2,&written,NULL));CloseHandle(f);
 assert(WritePrivateProfileStringW(L"UFC6",L"user",L"Photo \x03a9 test",launcher));photo_effects_load(&q,ini);assert(wcscmp(q.name,L"Photo \x03a9 test")==0&&!q.watermark);assert(DeleteFileW(launcher));photo_effects_load(&q,ini);assert(!q.name[0]&&!q.watermark);assert(RemoveDirectoryW(bin));assert(RemoveDirectoryW(game));
 MenuState s={0};photo_effects_default(&s.effects);menu_show(&s);s.page=PAGE_PHOTO;s.selection=9;s.armed=1;assert(menu_button(&s,PAD_A,1)==MENU_CHANGED&&s.page==PAGE_PHOTO_EFFECTS);menu_tick(&s,0,1,1);assert(menu_button(&s,PAD_RIGHT,1)==MENU_PHOTO_EFFECTS_CHANGED&&s.effects.preset==1);menu_tick(&s,0,2,1);assert(menu_button(&s,PAD_B,1)==MENU_CHANGED&&s.page==PAGE_PHOTO&&s.selection==9);
 s.selection=10;menu_tick(&s,0,3,1);assert(menu_button(&s,PAD_A,1)==MENU_CHANGED&&s.page==PAGE_PHOTO_WATERMARK);menu_tick(&s,0,4,1);menu_button(&s,PAD_A,1);assert(!s.effects.watermark);wcscpy(s.effects.name,L"Player test");menu_tick(&s,0,5,1);menu_button(&s,PAD_A,1);assert(s.effects.watermark);

 s.photo_panel_page=0;s.photo_help_hidden=0;uint32_t pad=PAD_MENU;assert(!photo_panel_controls(&s,&pad,0)&&s.photo_panel_page==1&&!pad);s.photo_effect_row=0;pad=PAD_RIGHT;int before=s.effects.preset;assert(photo_panel_controls(&s,&pad,0)&&s.effects.preset==(before+1)%7);
 pad=PAD_DOWN;assert(!photo_panel_controls(&s,&pad,0)&&s.photo_effect_row==1);pad=0;assert(photo_panel_controls(&s,&pad,PHOTO_PANEL_LEFT_KEY)&&s.effects.strength==3);
 pad=PAD_MENU;assert(!photo_panel_controls(&s,&pad,0)&&s.photo_panel_page==2);s.photo_mark_row=MARK_STYLE;pad=PAD_RIGHT;before=s.effects.mark_style;assert(photo_panel_controls(&s,&pad,0)&&s.effects.mark_style==(before+1)%4);
 pad=PAD_B;assert(!photo_panel_controls(&s,&pad,0)&&s.photo_panel_page==0&&!(pad&PAD_B));pad=PAD_B;assert(!photo_panel_controls(&s,&pad,0)&&(pad&PAD_B));
 s.photo_help_hidden=1;s.photo_panel_page=2;pad=PAD_RIGHT;before=s.effects.preset;assert(!photo_panel_controls(&s,&pad,0)&&s.effects.preset==before);pad=0;assert(!photo_panel_controls(&s,&pad,PHOTO_PANEL_PAGE_KEY)&&!s.photo_help_hidden&&s.photo_panel_page==1);
 s.photo_mark_row=MARK_COLOR;pad=0;s.photo_panel_page=2;assert(photo_panel_controls(&s,&pad,PHOTO_PANEL_RIGHT_KEY)&&s.effects.mark_color==1);assert(photo_mark_rgb(&s.effects)==0x141820);
 s=(MenuState){.visible=1,.armed=1,.page=PAGE_TOOLS,.selection=1};assert(!s.screenshot_include_ui&&menu_rows(&s)==6);assert(menu_button(&s,PAD_A,1)==MENU_SCREENSHOT_UI_CHANGED&&s.screenshot_include_ui);menu_tick(&s,0,20,1);assert(menu_button(&s,PAD_LEFT,1)==MENU_SCREENSHOT_UI_CHANGED&&!s.screenshot_include_ui);
 s=(MenuState){.replay_ready=1,.photo_panel_page=2};pad=PAD_MENU;assert(!photo_panel_controls(&s,&pad,0)&&s.photo_panel_page==3);
 pad=PAD_A;assert(photo_panel_controls(&s,&pad,0)==PHOTO_PANEL_REPLAY_TOGGLE&&!pad);
 pad=PAD_DOWN;assert(!photo_panel_controls(&s,&pad,0)&&s.photo_replay_row==1);pad=PAD_LEFT;assert(photo_panel_controls(&s,&pad,0)==PHOTO_PANEL_REPLAY_BACK&&!pad);
 pad=0;assert(photo_panel_controls(&s,&pad,PHOTO_PANEL_RIGHT_KEY)==PHOTO_PANEL_REPLAY_FORWARD);pad=PAD_DOWN;assert(!photo_panel_controls(&s,&pad,0)&&s.photo_replay_row==2);pad=PAD_RIGHT;assert(photo_panel_controls(&s,&pad,0)==PHOTO_PANEL_REPLAY_STEP_CHANGED&&menu_replay_step_ms(&s)==25);pad=PAD_UP;assert(!photo_panel_controls(&s,&pad,0)&&s.photo_replay_row==1);pad=PAD_DOWN;assert(!photo_panel_controls(&s,&pad,0)&&s.photo_replay_row==2);pad=PAD_DOWN;assert(!photo_panel_controls(&s,&pad,0)&&s.photo_replay_row==3);pad=PAD_RIGHT;assert(photo_panel_controls(&s,&pad,0)==PHOTO_PANEL_REPLAY_RANGE&&!pad);pad=PAD_DOWN;assert(!photo_panel_controls(&s,&pad,0)&&s.photo_replay_row==4);pad=PAD_RIGHT;assert(photo_panel_controls(&s,&pad,0)==PHOTO_PANEL_REPLAY_SPEED&&s.replay_speed_index==1);pad=0x1000;assert(photo_panel_controls(&s,&pad,0)==PHOTO_PANEL_REPLAY_TOGGLE&&!pad);pad=PAD_LB;assert(photo_panel_controls(&s,&pad,0)==PHOTO_PANEL_REPLAY_BACK&&!pad);pad=PAD_RB;assert(photo_panel_controls(&s,&pad,0)==PHOTO_PANEL_REPLAY_FORWARD&&!pad);pad=PAD_DOWN;assert(!photo_panel_controls(&s,&pad,0)&&s.photo_replay_row==0);s.replay_ready=0;pad=PAD_A|PAD_LB|0x1000;assert(!photo_panel_controls(&s,&pad,0)&&s.photo_panel_page==3&&!pad);
 s.replay_ready=1;pad=PAD_RB;assert(photo_panel_controls(&s,&pad,0)==PHOTO_PANEL_REPLAY_FORWARD&&s.photo_panel_page==3);
 s.replay_ready=0;pad=PAD_B;assert(!photo_panel_controls(&s,&pad,0)&&s.photo_panel_page==0&&!pad);
 s=(MenuState){.photo_panel_page=2,.replay_ready=0};pad=PAD_MENU;assert(!photo_panel_controls(&s,&pad,0)&&s.photo_panel_page==4);pad=PAD_RIGHT;assert(!photo_panel_controls(&s,&pad,0)&&s.photo_preset_slot==1);
 pad=PAD_DOWN;assert(!photo_panel_controls(&s,&pad,0)&&s.photo_preset_row==1);pad=PAD_A;assert(photo_panel_controls(&s,&pad,0)==PHOTO_PANEL_PRESET_LOAD);pad=PAD_DOWN;assert(!photo_panel_controls(&s,&pad,0)&&s.photo_preset_row==2);pad=PAD_A;assert(photo_panel_controls(&s,&pad,0)==PHOTO_PANEL_PRESET_SAVE);
 pad=PAD_MENU;assert(!photo_panel_controls(&s,&pad,0)&&s.photo_panel_page==0);
 puts("PASS: neutral bypass, reversible effect adjustment, reset preserves watermark, settings bounds/persistence, Photo Mode submenu navigation and missing-name guard.");return 0;
}
