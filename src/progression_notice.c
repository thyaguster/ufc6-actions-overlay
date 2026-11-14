#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <string.h>
#include "progression_notice.h"
#include "exact_hooks.h"
#include "engine_fingerprint.h"

/* UFC 6 01.006 global-progression post-fight status response.  The response
   contains showPopup, sourced from this service object's +0xc8 preference.
   The game's own "Don't show again" handler clears precisely this byte. */
#define GUEST_BASE 0x900000000ULL
#define PROGRESSION_STATUS_RVA 0x59c2970ULL
#define PROGRESSION_SERVICE_VTABLE_RVA 0x9c195a8ULL
#define PROGRESSION_POPUP_OFFSET 0xc8ULL

typedef void (__attribute__((sysv_abi)) *ProgressionStatus)(uintptr_t,uintptr_t);
static ProgressionStatus progression_status_original;
static volatile LONG progression_enabled;
static volatile LONG progression_state;

static void __attribute__((sysv_abi)) progression_status(uintptr_t service,uintptr_t response){
    uintptr_t vtable=0;
    if(InterlockedCompareExchange(&progression_enabled,0,0)&&
       ReadProcessMemory(GetCurrentProcess(),(const void*)service,&vtable,sizeof(vtable),NULL)&&
       vtable==GUEST_BASE+PROGRESSION_SERVICE_VTABLE_RVA){
        /* This callback is the producer of showPopup; a single-byte store is
           sufficient here and matches the game's own native handler. */
        *(volatile unsigned char*)(service+PROGRESSION_POPUP_OFFSET)=0;
        InterlockedExchange(&progression_state,1);
    }
    progression_status_original(service,response);
}

int progression_notice_prepare(void){
    if(InterlockedCompareExchange(&progression_state,0,0)<0)return 0;
    if(InterlockedCompareExchange(&progression_state,0,0)>0)return 1;
    int profile=engine_fingerprint();if(profile!=1&&profile!=2&&profile!=3){InterlockedExchange(&progression_state,-1);return 0;}
    static const unsigned char head[]={0x55,0x48,0x89,0xe5,0x41,0x57,0x41,0x56,0x41,0x54,0x53,0x48,0x83,0xec,0x10};
    MEMORY_BASIC_INFORMATION memory;
    void *target=(void*)(GUEST_BASE+PROGRESSION_STATUS_RVA);
    /* This service is mapped after the controller hooks during boot. ExactHook
       validates by reading its target, so never call it before the page exists. */
    if(!VirtualQuery(target,&memory,sizeof(memory))||memory.State!=MEM_COMMIT||
       memory.Protect&PAGE_GUARD||!(memory.Protect&(PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY)))return 0;
    ExactHook hook={(unsigned char*)target,(void*)progression_status,(void**)&progression_status_original,head,sizeof(head)};
    const char *error=NULL;
    if(!install_exact_hooks(&hook,1,&error)){InterlockedExchange(&progression_state,-1);return 0;}
    InterlockedExchange(&progression_state,1);
    return 1;
}

void progression_notice_set(int enabled){InterlockedExchange(&progression_enabled,enabled!=0);}
int progression_notice_state(void){return InterlockedCompareExchange(&progression_state,0,0);}
