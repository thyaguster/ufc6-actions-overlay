#ifndef UFC6_GAMEBAR_SHORTCUT_H
#define UFC6_GAMEBAR_SHORTCUT_H
#include <windows.h>
/* Read: 1 On (including Windows' absent-value default), 0 Off, -1 unknown. */
int gamebar_shortcut_read(void);
LONG gamebar_shortcut_write(int enabled);
#endif
