/* Session-only per-fighter control through a checked game binding-update hook. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string.h>
#include "live_cheats.h"
#include "cheat_bindings.h"
#include "exact_hooks.h"
typedef struct {uintptr_t flag[2],hook[2]; const char *off[2],*on[2];} Profile;
static const Profile profiles[]={
    {{0xb444d78,0xb440610},{0x5A88787,0x5A887B8},
     {"\x5B\x5D\xC3\x31\xC0\x48\x8B\x0B\x48\x3B\x4D\xF0\x74\xEE\xE8\x76\xE8\x44\x02\x0F\x0B\xCC\xCC\xCC\xCC\x48\x8B\x47\x20\x8B\x4F\x58","\x48\x8B\x77\x60\x0F\xB7\x56\x1C\x48\x01\xD6\x48\x85\xD2\x48\x0F\x44\xF2\x48\x01\xF0\xC3\xCC\xCC\x55\x48\x89\xE5\x41\x56\x53\x49"},
     {"\x5B\x5D\xC3\x31\xC0\x48\x8B\x0B\x48\x3B\x4D\xF0\x74\xEE\xE8\x76\xE8\x44\x02\x0F\x0B\xCC\xCC\xCC\xCC\xE9\x3F\x02\x00\x00\x90\x90","\xE9\xC9\x12\x00\x00\x90\x90\x90\x48\x01\xD6\x48\x85\xD2\x48\x0F\x44\xF2\x48\x01\xF0\xC3\xCC\xCC\x55\x48\x89\xE5\x41\x56\x53\x49"}},
    {{0xb452958,0xb44e1f0},{0x5A98F77,0x5A98FA8},
     {"\x5B\x5D\xC3\x31\xC0\x48\x8B\x0B\x48\x3B\x4D\xF0\x74\xEE\xE8\x36\xF7\x44\x02\x0F\x0B\xCC\xCC\xCC\xCC\x48\x8B\x47\x20\x8B\x4F\x58","\x48\x8B\x77\x60\x0F\xB7\x56\x1C\x48\x01\xD6\x48\x85\xD2\x48\x0F\x44\xF2\x48\x01\xF0\xC3\xCC\xCC\x55\x48\x89\xE5\x41\x56\x53\x49"},
     {"\x5B\x5D\xC3\x31\xC0\x48\x8B\x0B\x48\x3B\x4D\xF0\x74\xEE\xE8\x36\xF7\x44\x02\x0F\x0B\xCC\xCC\xCC\xCC\xE9\x3F\x02\x00\x00\x90\x90","\xE9\xC9\x12\x00\x00\x90\x90\x90\x48\x01\xD6\x48\x85\xD2\x48\x0F\x44\xF2\x48\x01\xF0\xC3\xCC\xCC\x55\x48\x89\xE5\x41\x56\x53\x49"}},
};
typedef void (__attribute__((sysv_abi)) *BindingSyncFn)(void*,unsigned,unsigned);
static BindingSyncFn original_binding_sync;
static uintptr_t hooked_base;
static SRWLOCK binding_guard=SRWLOCK_INIT;
static CheatBindingCache binding_cache[8];
static volatile LONG binding_commands[2];
typedef struct {volatile LONG busy,command,known,enabled;volatile LONG64 tick;} Observation;
static Observation observations[2];
static const unsigned char sync_signature[]={0x55,0x48,0x89,0xe5,0x41,0x57,0x41,0x56,0x41,0x55,0x41,0x54,0x53,0x48,0x83,0xec,0x18,0x48,0x8b,0x05,0x88,0xb5,0x9e,0x09,0x48,0x8b,0x00,0x48,0x89,0x45,0xd0};
static int read_memory(uintptr_t address,void *out,size_t size){SIZE_T got=0;return address>=0x10000&&ReadProcessMemory(GetCurrentProcess(),(void*)address,out,size,&got)&&got==size;}
static void __attribute__((sysv_abi)) hooked_binding_sync(void *binder,unsigned first,unsigned count){
    unsigned commands[2];CheatBindingFrame frame;memset(&frame,0,sizeof(frame));frame.player_one=-1;
    for(int i=0;i<2;++i)commands[i]=(unsigned)InterlockedCompareExchange(&binding_commands[i],0,0);
    if(TryAcquireSRWLockExclusive(&binding_guard)){
        CheatBindingCache *cache=&binding_cache[0];
        for(int i=0;i<8;++i)if(binding_cache[i].owner==(uintptr_t)binder){cache=&binding_cache[i];break;}else if(!binding_cache[i].owner)cache=&binding_cache[i];
        cheat_bindings_apply(cache,(uintptr_t)binder,first,count,commands,&frame);
        ReleaseSRWLockExclusive(&binding_guard);
    }
    original_binding_sync(binder,first,count);
    unsigned known[2],enabled[2];cheat_bindings_observe(&frame,known,enabled);
    for(int i=0;i<2;++i)if((frame.count[i]||frame.player_one==0)&&InterlockedCompareExchange(&observations[i].busy,1,0)==0){
        InterlockedExchange(&observations[i].known,(LONG)known[i]);InterlockedExchange(&observations[i].enabled,(LONG)enabled[i]);
        InterlockedExchange64(&observations[i].tick,(LONG64)GetTickCount64());InterlockedExchange(&observations[i].command,(LONG)commands[i]);InterlockedExchange(&observations[i].busy,0);
    }
}
static int flag_handles_ready(uintptr_t base){
    const char *names[]={"Fighter_Disable_StaminaLoss","Fighter_Disable_HealthLoss"};
    for(int i=0;i<2;++i){uintptr_t object=0,name=0;char text[32]={0};
        if(!read_memory(base+profiles[1].flag[i],&object,8)||!read_memory(object+8,&name,8)||!read_memory(name,text,strlen(names[i])+1)||strcmp(text,names[i]))return 0;
    }
    return 1;
}
static int ensure_binding_hook(uintptr_t base){
    if(hooked_base)return hooked_base==base;
    unsigned char bytes[sizeof(sync_signature)];
    if(!read_memory(base+0x34cd10,bytes,sizeof(bytes))||memcmp(bytes,sync_signature,sizeof(bytes)))return 0;
    ExactHook hook={(unsigned char*)(base+0x34cd10),(void*)hooked_binding_sync,(void**)&original_binding_sync,sync_signature,17};
    const char *error=NULL;
    if(!install_exact_hooks(&hook,1,&error))return 0;
    hooked_base=base;return 1;
}
static unsigned publish(LiveCheats *s,int i,int blocked){
    LiveCheat *f=&s->flags[i];unsigned bits=s->target|((blocked?0:f->owned_mask)<<2)|((!blocked&&f->requested?s->target:0)<<4);
    unsigned old=(unsigned)InterlockedCompareExchange(&binding_commands[i],0,0);
    if((old&63)!=bits){old=((old+64)&~63u)|bits;InterlockedExchange(&binding_commands[i],(LONG)old);}
    return old;
}
void live_cheats_init(LiveCheats *s,uintptr_t guest_base){
    memset(s,0,sizeof(*s));s->base=guest_base;s->target=LIVE_PLAYER1;
    for(int i=0;i<2;++i){InterlockedExchange(&binding_commands[i],LIVE_PLAYER1);InterlockedExchange64(&observations[i].tick,0);}
}
void live_cheats_tick(LiveCheats *s){
    const Profile *p=&profiles[1];unsigned char code[32];int available=0,readable=0;
    /* Binding synchronization is verified for game code 01.005/01.006 only. */
    for(int i=0;i<2;++i)if(read_memory(s->base+p->hook[i],code,sizeof(code))){readable=1;if(!memcmp(code,p->off[i],32)||!memcmp(code,p->on[i],32))available=1;}
    int handles=flag_handles_ready(s->base);
    int ready=available&&handles&&ensure_binding_hook(s->base);
    for(int i=0;i<LIVE_COUNT;++i){
        LiveCheat *f=&s->flags[i];int blocked=-1;
        if(!ready)blocked=!handles?LIVE_WAIT:readable?LIVE_UNSUPPORTED:LIVE_WAIT;
        else if(!read_memory(s->base+p->hook[i],code,sizeof(code)))blocked=LIVE_WAIT;
        else if(memcmp(code,p->off[i],32))blocked=!memcmp(code,p->on[i],32)?LIVE_LAUNCHER:LIVE_UNSUPPORTED;
        if(!ready||blocked>=0){publish(s,i,1);f->status=blocked>=0?blocked:LIVE_WAIT;continue;}
        unsigned command=publish(s,i,0),required=s->target|f->owned_mask;
        if(InterlockedCompareExchange(&observations[i].busy,1,0)!=0)continue;
        unsigned observed=(unsigned)InterlockedCompareExchange(&observations[i].command,0,0);
        unsigned known=(unsigned)InterlockedCompareExchange(&observations[i].known,0,0),enabled=(unsigned)InterlockedCompareExchange(&observations[i].enabled,0,0);
        uint64_t last=(uint64_t)InterlockedCompareExchange64(&observations[i].tick,0,0);InterlockedExchange(&observations[i].busy,0);
        if(!last||GetTickCount64()-last>1000||observed!=command||(known&required)!=required){f->status=f->owned_mask&&!f->requested?LIVE_RESTORING:LIVE_WAIT;continue;}
        if(f->requested)f->status=(enabled&s->target)==s->target?LIVE_ON:LIVE_WAIT;
        else if(f->owned_mask&s->target)f->status=(enabled&s->target)?LIVE_RESTORING:LIVE_OFF;
        else f->status=(enabled&s->target)?LIVE_GAME:LIVE_OFF;
    }
}
int live_cheats_toggle(LiveCheats *s,int which){
    if(which<0||which>=LIVE_COUNT)return 0;
    live_cheats_tick(s);LiveCheat *f=&s->flags[which];
    if((hooked_base&&hooked_base!=s->base)||f->status==LIVE_LAUNCHER||f->status==LIVE_UNSUPPORTED)return 0;
    if(f->requested||f->status==LIVE_GAME){f->requested=0;f->owned_mask|=s->target;}
    else{f->requested=1;f->owned_mask|=s->target;}
    live_cheats_tick(s);return 1;
}
void live_cheats_stop(LiveCheats *s){for(int i=0;i<2;++i)s->flags[i].requested=0;live_cheats_tick(s);}
