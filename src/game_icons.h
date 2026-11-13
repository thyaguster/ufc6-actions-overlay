#ifndef UFC6_GAME_ICONS_H
#define UFC6_GAME_ICONS_H
#include <windows.h>
enum { GAME_ICONS_CHECKING, GAME_ICONS_PS, GAME_ICONS_XBOX, GAME_ICONS_MIXED, GAME_ICONS_UNAVAILABLE };
void startup_assets_check(void);
int startup_assets_status(wchar_t *message,size_t count);
BOOL startup_assets_apply(HWND game,int enabled);
void game_icons_init(HMODULE module);
void game_icons_check(void);
int game_icons_status(wchar_t *message,size_t count);
BOOL game_icons_apply(HWND game,int xbox);
#endif
