#ifndef UFC6_RESTART_CLIENT_H
#define UFC6_RESTART_CLIENT_H
#include <windows.h>
BOOL menu_restart_game(HWND game,HMODULE module);
BOOL menu_shutdown_pc(HWND game,HMODULE module);
BOOL menu_reboot_pc(HWND game,HMODULE module,BOOL auto_launch);
#endif
