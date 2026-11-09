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
static int install_hooks(ExactHook *hooks, size_t count, unsigned char *relays, const char **error) {
    unsigned char *stubs = NULL;
    FrozenThread *threads = NULL;
    size_t thread_count = 0, writable = 0;
    DWORD protection[MAX_HOOKS],relay_protection=0;int relay_writable=0;
    unsigned char patches[MAX_HOOKS][32];
    int success = 0;
    *error = "Invalid hook specification";
    if (!count || count > MAX_HOOKS) return 0;
    for (size_t i = 0; i < count; ++i)
        if (hooks[i].length < (relays?5u:14u) || hooks[i].length > 32 ||
            memcmp(hooks[i].target, hooks[i].expected, hooks[i].length)) return 0;
    if(relays)for(size_t i=0;i<count*16;++i)if(relays[i]!=0xcc){*error="Parsec relay padding is not empty";return 0;}
    if(!relays)stubs = VirtualAlloc(NULL, count * SLOT_SIZE, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    threads = VirtualAlloc(NULL, MAX_THREADS * sizeof(*threads), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if ((!relays&&!stubs) || !threads) { *error = "Unable to allocate distant hook storage"; goto done; }
    for (size_t i = 0; i < count; ++i) {
        if(relays){
            int32_t offset;memcpy(&offset,hooks[i].target+1,4);
            if(hooks[i].length!=5||hooks[i].target[0]!=0xe9){*error="Parsec entry is not a checked five-byte jump";goto done;}
            *hooks[i].original=hooks[i].target+5+offset;
            intptr_t distance=(intptr_t)(relays+i*16)-(intptr_t)(hooks[i].target+5);
            if(distance<INT32_MIN||distance>INT32_MAX){*error="Parsec relay is out of range";goto done;}
            int32_t delta=(int32_t)distance;patches[i][0]=0xe9;memcpy(patches[i]+1,&delta,4);continue;
        }
        unsigned char *slot = stubs + i * SLOT_SIZE;
        memcpy(slot, hooks[i].target, hooks[i].length);
        jump(slot + hooks[i].length, hooks[i].target + hooks[i].length);
        memset(patches[i], 0x90, sizeof(patches[i]));
        jump(patches[i], hooks[i].detour);
        *hooks[i].original = slot;
    }
    DWORD old;
    if (!relays&&!VirtualProtect(stubs, count * SLOT_SIZE, PAGE_EXECUTE_READ, &old)) {
        *error = "Unable to make hook storage executable"; goto done;
    }
    if(!relays)FlushInstructionCache(GetCurrentProcess(), stubs, count * SLOT_SIZE);
    if(relays){
        if(!VirtualProtect(relays,count*16,PAGE_EXECUTE_READWRITE,&relay_protection)){*error="Cannot prepare Parsec relay padding";goto done;}
        relay_writable=1;
        for(size_t i=0;i<count;++i)jump(relays+i*16,hooks[i].detour);
        FlushInstructionCache(GetCurrentProcess(),relays,count*16);
    }
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
                if(relays&&t->old_rip!=start){*error="Unexpected thread position in Parsec entry";goto done;}
                t->context.Rip = relays?(DWORD64)(uintptr_t)*hooks[h].original:(DWORD64)(uintptr_t)(stubs + h * SLOT_SIZE) + t->old_rip - start;
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
    if(relay_writable){
        if(!success){memset(relays,0xcc,count*16);FlushInstructionCache(GetCurrentProcess(),relays,count*16);}
        DWORD ignored;VirtualProtect(relays,count*16,relay_protection,&ignored);
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

int install_exact_hooks(ExactHook *hooks,size_t count,const char **error){return install_hooks(hooks,count,NULL,error);}
int install_parsec_hooks(ExactHook *hooks,size_t count,unsigned char *padding,const char **error){return install_hooks(hooks,count,padding,error);}
