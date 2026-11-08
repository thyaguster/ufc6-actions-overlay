#ifndef UFC6_SCREENSHOT_PNG_H
#define UFC6_SCREENSHOT_PNG_H
#include <windows.h>
#include <stdint.h>
BOOL menu_save_png(HMODULE module,uint32_t width,uint32_t height,unsigned char *pixels,int rgba);
#endif
