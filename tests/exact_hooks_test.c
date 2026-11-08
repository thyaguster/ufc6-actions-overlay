#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#undef NDEBUG
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "../src/exact_hooks.h"

typedef int (*Fn)(void *,int,int,int);
static Fn originals[3], functions[3];
static volatile LONG stop, failures;
static int detour0(void *c,int id,int a,int b) {return originals[0](c,id,a,b)+100;}
static int detour1(void *c,int id,int a,int b) {return originals[1](c,id,a,b)+100;}
static int detour2(void *c,int id,int a,int b) {return originals[2](c,id,a,b)+100;}
static DWORD WINAPI stress(void *p) {
    (void)p;
    while (!InterlockedCompareExchange(&stop,0,0))
        for (int i=0;i<3;++i) {
            int result=functions[i](NULL,7,11,1), expected=i==2 ? 7 : 11;
            if (result != expected && result != expected+100) InterlockedIncrement(&failures);
        }
    return 0;
}
int main(void) {
    static const unsigned char p0[]={0x55,0x41,0x56,0x56,0x57,0x53,0x48,0x81,0xec,0xd0,0,0,0,0x48,0x8d,0xac,0x24,0x80,0,0,0};
    static const unsigned char p1[]={0x55,0x41,0x57,0x41,0x56,0x41,0x54,0x56,0x57,0x53,0x48,0x81,0xec,0xd0,0,0,0,0x48,0x8d,0xac,0x24,0x80,0,0,0};
    static const unsigned char p2[]={0x55,0x56,0x57,0x48,0x83,0xec,0x30,0x48,0x8d,0x6c,0x24,0x30,0x48,0xc7,0x45,0xf8,0xfe,0xff,0xff,0xff};
    static const unsigned char s0[]={0x44,0x89,0xc0,0x48,0x81,0xc4,0xd0,0,0,0,0x5b,0x5f,0x5e,0x41,0x5e,0x5d,0xc3};
    static const unsigned char s1[]={0x44,0x89,0xc0,0x48,0x81,0xc4,0xd0,0,0,0,0x5b,0x5f,0x5e,0x41,0x5c,0x41,0x5e,0x41,0x5f,0x5d,0xc3};
    static const unsigned char s2[]={0x89,0xd0,0x48,0x83,0xc4,0x30,0x5f,0x5e,0x5d,0xc3};
    const unsigned char *prefix[]={p0,p1,p2}, *suffix[]={s0,s1,s2};
    size_t lengths[]={sizeof(p0),sizeof(p1),sizeof(p2)}, tail[]={sizeof(s0),sizeof(s1),sizeof(s2)};
    void *detours[]={detour0,detour1,detour2};
    unsigned char *code=VirtualAlloc(NULL,4096,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
    assert(code);
    ExactHook hooks[3];
    for (int i=0;i<3;++i) {
        memcpy(code+i*128,prefix[i],lengths[i]);
        memcpy(code+i*128+lengths[i],suffix[i],tail[i]);
        functions[i]=(Fn)(code+i*128);
        hooks[i]=(ExactHook){code+i*128,detours[i],(void **)&originals[i],prefix[i],lengths[i]};
    }
    DWORD old; assert(VirtualProtect(code,4096,PAGE_EXECUTE_READ,&old));
    for (int i=0;i<3;++i) assert(functions[i](NULL,7,11,1)==(i==2?7:11));
    const char *error;
    ExactHook wrong=hooks[0];wrong.expected=p1;
    assert(!install_exact_hooks(&wrong,1,&error));
    assert(!memcmp(code,p0,sizeof(p0)));
    HANDLE threads[4];
    for(int i=0;i<4;++i) {threads[i]=CreateThread(NULL,0,stress,NULL,0,NULL);assert(threads[i]);}
    Sleep(20);
    assert(install_exact_hooks(hooks,3,&error));
    for(int i=0;i<3;++i) {
        assert(functions[i](NULL,7,11,1)==(i==2?107:111));
        assert(originals[i](NULL,7,11,1)==(i==2?7:11));
    }
    Sleep(100);
    InterlockedExchange(&stop,1);
    assert(WaitForMultipleObjects(4,threads,TRUE,5000)==WAIT_OBJECT_0);
    for(int i=0;i<4;++i) CloseHandle(threads[i]);
    assert(!failures);
    puts("Absolute hook tests passed: all three exact prologues, trampolines, signature rejection, concurrent calls.");
    return 0;
}
