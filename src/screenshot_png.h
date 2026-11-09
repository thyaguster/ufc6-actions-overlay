#ifndef UFC6_SCREENSHOT_PNG_H
#define UFC6_SCREENSHOT_PNG_H
#include <windows.h>
#include <stdint.h>
BOOL menu_save_png(HMODULE module,uint32_t width,uint32_t height,unsigned char *pixels,int rgba);
BOOL menu_save_png_path(HMODULE module,uint32_t width,uint32_t height,unsigned char *pixels,int rgba,PWSTR *saved_path);
#endif
