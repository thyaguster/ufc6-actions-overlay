/* SPDX-License-Identifier: GPL-2.0-only
 * Forward the user's verified original runtime without changing its bytes.
 * This non-system import still loads when Windows prefers system DLLs.
 * The Vulkan layer loads the overlay core outside the Windows loader lock.
 */
#define WIN32_LEAN_AND_MEAN
#define UNICODE
#define _UNICODE
#include <windows.h>
#include "overlay_bootstrap.h"
BOOL WINAPI DllMain(HINSTANCE module,DWORD reason,LPVOID reserved) {
    (void)reserved;
    if(reason==DLL_PROCESS_ATTACH){DisableThreadLibraryCalls(module);overlay_bootstrap(module);}
    return TRUE;
}
