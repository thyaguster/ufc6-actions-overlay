#ifndef UFC6_EXACT_HOOKS_H
#define UFC6_EXACT_HOOKS_H
#include <windows.h>
#include <stddef.h>

/* Only for checked, position-independent prologues. No automatic relocation. */
typedef struct {
    unsigned char *target;
    void *detour;
    void **original;
    const unsigned char *expected;
    size_t length;
} ExactHook;
int install_exact_hooks(ExactHook *hooks, size_t count, const char **error);
int install_parsec_hooks(ExactHook *hooks,size_t count,unsigned char *padding,const char **error);
#endif
