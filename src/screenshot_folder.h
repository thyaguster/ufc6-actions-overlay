#ifndef UFC6_SCREENSHOT_FOLDER_H
#define UFC6_SCREENSHOT_FOLDER_H
#include <windows.h>
/* Caller frees the Windows-known-folder result with CoTaskMemFree. */
PWSTR menu_screenshot_directory(void);
#endif
