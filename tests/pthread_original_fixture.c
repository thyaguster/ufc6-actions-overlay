/* Test-only stand-in. No threading runtime or game code is distributed. */
#include <windows.h>
__declspec(dllexport) int clock_getres(int clock,void *out){(void)clock;(void)out;return 7331;}
BOOL WINAPI DllMain(HINSTANCE module,DWORD reason,LPVOID reserved){(void)module;(void)reason;(void)reserved;return TRUE;}
