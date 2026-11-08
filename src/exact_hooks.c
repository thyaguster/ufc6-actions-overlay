/* Exact-build hooks with 14-byte absolute jumps: Kyty reserves all nearby memory. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#include <stdint.h>
#include <string.h>
#include "exact_hooks.h"

#define MAX_THREADS 2048
#define MAX_HOOKS 8
#define SLOT_SIZE 64
typedef struct {
    HANDLE handle;
    CONTEXT context;
    DWORD64 old_rip;
    int suspended, adjusted;
} FrozenThread;

static void jump(unsigned char *dst, const void *address) {
    dst[0] = 0xff; dst[1] = 0x25;
    dst[2] = dst[3] = dst[4] = dst[5] = 0;
    uintptr_t value = (uintptr_t)address;
    memcpy(dst + 6, &value, sizeof(value));
}
int install_exact_hooks(ExactHook *hooks, size_t count, const char **error) {
    unsigned char *stubs = NULL;
    FrozenThread *threads = NULL;
    size_t thread_count = 0, writable = 0;
    DWORD protection[MAX_HOOKS];
    unsigned char patches[MAX_HOOKS][32];
    int success = 0;
    *error = "Invalid hook specification";
    if (!count || count > MAX_HOOKS) return 0;
    for (size_t i = 0; i < count; ++i)
        if (hooks[i].length < 14 || hooks[i].length > 32 ||
            memcmp(hooks[i].target, hooks[i].expected, hooks[i].length)) return 0;
    stubs = VirtualAlloc(NULL, count * SLOT_SIZE, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    threads = VirtualAlloc(NULL, MAX_THREADS * sizeof(*threads), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!stubs || !threads) { *error = "Unable to allocate distant hook storage"; goto done; }
    for (size_t i = 0; i < count; ++i) {
        unsigned char *slot = stubs + i * SLOT_SIZE;
        memcpy(slot, hooks[i].target, hooks[i].length);
        jump(slot + hooks[i].length, hooks[i].target + hooks[i].length);
        memset(patches[i], 0x90, sizeof(patches[i]));
        jump(patches[i], hooks[i].detour);
        *hooks[i].original = slot;
    }
    DWORD old;
    if (!VirtualProtect(stubs, count * SLOT_SIZE, PAGE_EXECUTE_READ, &old)) {
        *error = "Unable to make hook storage executable"; goto done;
    }
    FlushInstructionCache(GetCurrentProcess(), stubs, count * SLOT_SIZE);
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snapshot == INVALID_HANDLE_VALUE) { *error = "Unable to enumerate engine threads"; goto done; }
    THREADENTRY32 entry; entry.dwSize = sizeof(entry);
    BOOL more = Thread32First(snapshot, &entry);
    while (more) {
        if (entry.th32OwnerProcessID == GetCurrentProcessId() && entry.th32ThreadID != GetCurrentThreadId()) {
            if (thread_count == MAX_THREADS) {
                CloseHandle(snapshot); *error = "Too many engine threads"; goto done;
            }
            HANDLE h = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_SET_CONTEXT |
                                   THREAD_QUERY_INFORMATION, FALSE, entry.th32ThreadID);
            if (h) threads[thread_count++].handle = h;
            else if (GetLastError() != ERROR_INVALID_PARAMETER) {
                CloseHandle(snapshot); *error = "Unable to open an engine thread"; goto done;
            }
        }
        more = Thread32Next(snapshot, &entry);
    }
    DWORD enumeration_error = GetLastError();
    CloseHandle(snapshot);
    if (enumeration_error != ERROR_NO_MORE_FILES) { *error = "Thread enumeration failed"; goto done; }
    for (size_t i = 0; i < count; ++i) {
        if (!VirtualProtect(hooks[i].target, hooks[i].length, PAGE_EXECUTE_READWRITE, &protection[i])) {
            *error = "Unable to make a hook site writable"; goto done;
        }
        ++writable;
    }
    /* Everything that might allocate or log is prepared before suspending any thread. */
    for (size_t i = 0; i < thread_count; ++i) {
        FrozenThread *t = &threads[i];
        if (SuspendThread(t->handle) == (DWORD)-1) {
            DWORD exit_code;
            if (GetExitCodeThread(t->handle, &exit_code) && exit_code != STILL_ACTIVE) continue;
            *error = "Unable to suspend an engine thread"; goto done;
        }
        t->suspended = 1;
        t->context.ContextFlags = CONTEXT_CONTROL;
        if (!GetThreadContext(t->handle, &t->context)) {
            *error = "Unable to read an engine thread context"; goto done;
        }
        for (size_t h = 0; h < count; ++h) {
            DWORD64 start = (DWORD64)(uintptr_t)hooks[h].target;
            if (t->context.Rip >= start && t->context.Rip < start + hooks[h].length) {
                t->old_rip = t->context.Rip;
                t->context.Rip = (DWORD64)(uintptr_t)(stubs + h * SLOT_SIZE) + t->old_rip - start;
                if (!SetThreadContext(t->handle, &t->context)) {
                    *error = "Unable to redirect a suspended hook prologue"; goto done;
                }
                t->adjusted = 1;
            }
        }
    }
    for (size_t i = 0; i < count; ++i) {
        memcpy(hooks[i].target, patches[i], hooks[i].length);
        FlushInstructionCache(GetCurrentProcess(), hooks[i].target, hooks[i].length);
    }
    success = 1;
done:
    if (!success && threads)
        for (size_t i = 0; i < thread_count; ++i) if (threads[i].adjusted) {
            threads[i].context.Rip = threads[i].old_rip;
            SetThreadContext(threads[i].handle, &threads[i].context);
        }
    /* Multiple hook sites can share a page; restore protections in reverse order. */
    for (size_t n = writable; n > 0; --n) {
        size_t i = n - 1;
        DWORD ignored;
        VirtualProtect(hooks[i].target, hooks[i].length, protection[i], &ignored);
    }
    if (threads) {
        for (size_t i = 0; i < thread_count; ++i)
            if (threads[i].suspended) ResumeThread(threads[i].handle);
        for (size_t i = 0; i < thread_count; ++i) CloseHandle(threads[i].handle);
        VirtualFree(threads, 0, MEM_RELEASE);
    }
    if (!success && stubs) {
        VirtualFree(stubs, 0, MEM_RELEASE);
        for (size_t i = 0; i < count; ++i) *hooks[i].original = NULL;
    }
    return success;
}
