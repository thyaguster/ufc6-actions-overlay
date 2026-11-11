#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#undef NDEBUG
#include <assert.h>
#include <stdio.h>
#include "../src/overlay_preferences.h"
int wmain(int argc,wchar_t **argv){
 assert(argc==2);OverlayPreferences p={0},loaded;p.actions[BIND_OPEN].pad=0xc08;
 assert(binding_set(&p,BIND_SCREENSHOT,1,KEY_CTRL|VK_F12));
 assert(!binding_set(&p,BIND_CHEATS,1,KEY_CTRL|VK_F12));assert(!binding_set(&p,BIND_CHEATS,1,VK_F10));assert(!binding_set(&p,BIND_CHEATS,1,VK_LWIN));
 assert(!binding_set(&p,BIND_SCREENSHOT,0,0xc00));assert(binding_set(&p,BIND_SCREENSHOT,0,0x806));assert(!binding_set(&p,BIND_CHEATS,0,0x802));
 BindingLatch latch={0};assert(!binding_poll_pad(&latch,0x806,0x806,100,1));assert(!binding_poll_pad(&latch,0x806,0x806,899,1));assert(binding_poll_pad(&latch,0x806,0x806,900,1));assert(!binding_poll_pad(&latch,0x806,0x806,5000,1));
 assert(!binding_poll_pad(&latch,0x806,0,5100,1));assert(!binding_poll_pad(&latch,0x806,0x806,5200,0));assert(!binding_poll_pad(&latch,0x806,0x806,9000,1));assert(!binding_poll_pad(&latch,0x806,0,9100,1));
 unsigned char down[256]={0};BindingLatch key={0};down[VK_F12]=1;assert(!binding_poll_key(&key,KEY_CTRL|VK_F12,down,1));down[VK_CONTROL]=1;assert(binding_poll_key(&key,KEY_CTRL|VK_F12,down,1));assert(!binding_poll_key(&key,KEY_CTRL|VK_F12,down,1));
 down[VK_F12]=0;assert(!binding_poll_key(&key,KEY_CTRL|VK_F12,down,1));down[VK_F12]=1;assert(!binding_poll_key(&key,KEY_CTRL|VK_F12,down,0));assert(!binding_poll_key(&key,KEY_CTRL|VK_F12,down,1));
 wchar_t ini[1024],cfg[1024];swprintf(ini,1024,L"%ls.settings.ini",argv[1]);swprintf(cfg,1024,L"%ls.theme.cfg",argv[1]);p.theme_choice=3;
 assert(WritePrivateProfileStringW(L"Other",L"Keep",L"yes",ini));assert(preferences_save(&p,ini));preferences_load(&loaded,ini,0xc08);assert(loaded.actions[1].key==p.actions[1].key&&loaded.actions[1].pad==p.actions[1].pad&&loaded.theme_choice==3);
 assert(GetPrivateProfileIntW(L"Keybinds",L"ScreenshotPad",0,ini)==0x806);assert(binding_set(&loaded,1,1,0)&&preferences_save(&loaded,ini));preferences_load(&loaded,ini,0xc08);assert(!loaded.actions[1].key);
 theme_create_default(cfg);OverlayTheme green,blue,custom;theme_load(&green,cfg,0);theme_load(&blue,cfg,2);assert(green.accent!=blue.accent);
 assert(WritePrivateProfileStringW(L"Theme",L"Accent",L"#123456",cfg));theme_load(&custom,cfg,0);assert(custom.accent==RGB(0x12,0x34,0x56));theme_create_default(cfg);theme_load(&custom,cfg,0);assert(custom.accent==RGB(0x12,0x34,0x56));
 theme_load(&custom,cfg,2);assert(custom.accent==blue.accent);assert(WritePrivateProfileStringW(L"Theme",L"Accent",L"#GG0011",cfg));theme_load(&custom,cfg,0);assert(custom.accent==green.accent);
 wchar_t first_dir[1024],first_ini[1100],first_cfg[1100];swprintf(first_dir,1024,L"%ls.first-run",argv[1]);assert(CreateDirectoryW(first_dir,NULL));
 swprintf(first_ini,1100,L"%ls\\ufc6-quit-menu.ini",first_dir);swprintf(first_cfg,1100,L"%ls\\ufc6-overlay-theme.cfg",first_dir);
 assert(WritePrivateProfileStringW(L"Appearance",L"FontSize",L"20",first_cfg));assert(WritePrivateProfileStringW(L"Appearance",L"UIScale",L"175",first_cfg));assert(WritePrivateProfileStringW(L"Appearance",L"BoxStyle",L"Octagon",first_cfg));
 assert(WritePrivateProfileStringW(L"Appearance",L"WindowSize",L"Tall",first_cfg));
 preferences_seed_appearance(first_ini);assert(GetPrivateProfileIntW(L"Menu",L"FontSize",0,first_ini)==20&&GetPrivateProfileIntW(L"Menu",L"UIScale",0,first_ini)==175&&GetPrivateProfileIntW(L"Menu",L"BoxStyle",0,first_ini)==4&&GetPrivateProfileIntW(L"Menu",L"WindowSize",0,first_ini)==2);
 assert(WritePrivateProfileStringW(L"Appearance",L"FontSize",L"16",first_cfg));assert(WritePrivateProfileStringW(L"Menu",L"UIScale",L"125",first_ini));preferences_seed_appearance(first_ini);
 assert(GetPrivateProfileIntW(L"Menu",L"FontSize",0,first_ini)==20&&GetPrivateProfileIntW(L"Menu",L"UIScale",0,first_ini)==125);
 DeleteFileW(first_ini);DeleteFileW(first_cfg);assert(RemoveDirectoryW(first_dir));
 DeleteFileW(ini);DeleteFileW(cfg);puts("PASS: key/chord conflicts, modifiers, once-per-hold activation, focus loss, persistence/clear, config theme override, preset independence, invalid colors and launcher-file preservation.");return 0;
}
