#ifndef UFC6_AVATAR_READBACK_H
#define UFC6_AVATAR_READBACK_H
#include <windows.h>
#include <stddef.h>
enum { AVATAR_READBACK_OFF, AVATAR_READBACK_REPAIR, AVATAR_READBACK_ALWAYS };
int avatar_readback_mode(const wchar_t *engine_dir);
BOOL avatar_readback_set(const wchar_t *engine_dir,int mode,wchar_t *error,size_t count);
#endif
