#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#undef NDEBUG
#include <assert.h>
#include <stdio.h>
#include "../src/live_cheats.c"
static unsigned calls;
static unsigned char fields[2][4];
static int32_t model[2][2];
static void ptr(unsigned char *p,size_t o,const void *v){uintptr_t a=(uintptr_t)v;memcpy(p+o,&a,8);}
static void integer(unsigned char *p,size_t o,int32_t v){memcpy(p+o,&v,4);}
static unsigned char *page(unsigned char *base,uintptr_t offset){unsigned char *p=base+(offset&~(uintptr_t)4095);assert(VirtualAlloc(p,4096,MEM_COMMIT,PAGE_EXECUTE_READWRITE)==p);return base+offset;}
static void __attribute__((sysv_abi)) model_sync(void *binder,unsigned first,unsigned count){
    assert(binder&&first==0&&count==3);++calls;
    for(int i=0;i<2;++i)for(int j=0;j<2;++j){model[i][j]=fields[i][j*2];fields[i][j*2]=(unsigned char)model[i][j];}
}
static void frame(LiveCheats *s,BindingSyncFn sync,void *binder){sync(binder,0,3);live_cheats_tick(s);}
int main(void){
    unsigned char *base=VirtualAlloc(NULL,0xc000000,MEM_RESERVE,PAGE_NOACCESS);assert(base);const Profile *p=&profiles[1];
    LiveCheats pending;live_cheats_init(&pending,(uintptr_t)base);
    assert(live_cheats_toggle(&pending,0)&&pending.flags[0].requested&&pending.flags[0].status==LIVE_WAIT);
    assert(live_cheats_toggle(&pending,0)&&!pending.flags[0].requested);
    for(int i=0;i<2;++i)memcpy(page(base,p->hook[i]),p->off[i],32);
    unsigned char *code=page(base,0x34cd10);memcpy(code,sync_signature,sizeof(sync_signature));
    uintptr_t cookie=123,canary=(uintptr_t)&cookie;int32_t displacement;memcpy(&displacement,code+20,4);intptr_t rel=0x34cd10+24+displacement;memcpy(page(base,(uintptr_t)rel),&canary,8);
    unsigned char *body=code+sizeof(sync_signature);body[0]=0x48;body[1]=0xb8;uintptr_t fn=(uintptr_t)model_sync;memcpy(body+2,&fn,8);body[10]=0xff;body[11]=0xd0;
    const unsigned char epilogue[]={0x48,0x83,0xc4,0x18,0x5b,0x41,0x5c,0x41,0x5d,0x41,0x5e,0x41,0x5f,0x5d,0xc3};memcpy(body+12,epilogue,sizeof(epilogue));FlushInstructionCache(GetCurrentProcess(),code,128);
    unsigned char binder[0xa0]={0},objects[3][32]={{0}},records[3*0x68]={0},nodes[3][32]={{0}};
    char labels[3][32]={"Fighter_Disable_StaminaLoss","Fighter_Disable_HealthLoss","Fighter_ControllerInputMask"};int32_t input[2]={0,1};
    for(int i=0;i<3;++i){unsigned char *r=records+i*0x68;ptr(objects[i],8,labels[i]);ptr(r,0x10,objects[i]);integer(r,0x20,-1);integer(r,0x28,4);integer(r,0x34,i==2?6:1);ptr(r,0x38,nodes[i]);ptr(r,0x40,nodes[i]+32);
        for(int j=0;j<2;++j){ptr(nodes[i],j*16,i==2?(void*)&input[j]:(void*)&fields[i][j*2]);integer(nodes[i],j*16+8,j);if(i<2)fields[i][j*2+1]=0xa5;}
    }
    for(int i=0;i<2;++i)ptr(page(base,p->flag[i]),0,objects[i]);
    ptr(binder,0x88,records);ptr(binder,0x90,records+sizeof(records));LiveCheats s;live_cheats_init(&s,(uintptr_t)base);
    live_cheats_tick(&s);assert(hooked_base==(uintptr_t)base&&s.target==LIVE_PLAYER1);BindingSyncFn sync=(BindingSyncFn)(void*)code;
    frame(&s,sync,binder);assert(s.flags[0].status==LIVE_OFF&&calls==1);
    fields[0][2]=1;model[0][1]=0;frame(&s,sync,binder);assert(model[0][1]==1&&s.flags[0].status==LIVE_GAME);
    assert(live_cheats_toggle(&s,0));frame(&s,sync,binder);assert(s.flags[0].status==LIVE_OFF&&fields[0][2]==0&&model[0][1]==0);
    assert(live_cheats_toggle(&s,0));assert(live_cheats_toggle(&s,1));frame(&s,sync,binder);
    for(int i=0;i<2;++i)assert(s.flags[i].status==LIVE_ON&&fields[i][0]==0&&fields[i][2]==1); /* P1 is actual fighter index 1. */
    unsigned char inactive_binder[0xa0]={0},inactive_records[sizeof(records)];memcpy(inactive_records,records,sizeof(records));
    for(int i=0;i<3;++i)ptr(inactive_records+i*0x68,0x40,nodes[i]);
    ptr(inactive_binder,0x88,inactive_records);ptr(inactive_binder,0x90,inactive_records+sizeof(inactive_records));
    frame(&s,sync,inactive_binder);for(int i=0;i<2;++i)assert(s.flags[i].status==LIVE_ON);
    input[0]=1;input[1]=2;frame(&s,sync,binder);
    for(int i=0;i<2;++i)assert(s.flags[i].status==LIVE_ON&&fields[i][0]==1&&fields[i][2]==0); /* P1 assignment moves; opponent cleared. */
    input[0]=2;input[1]=1;frame(&s,sync,binder);for(int i=0;i<2;++i)assert(fields[i][0]==0&&fields[i][2]==1);
    live_cheats_stop(&s);for(int n=0;n<10;++n){fields[0][2]=1;frame(&s,sync,binder);assert(s.flags[0].status==LIVE_OFF&&fields[0][2]==0&&model[0][1]==0);}
    input[0]=input[1]=1;assert(live_cheats_toggle(&s,0));frame(&s,sync,binder);assert(s.flags[0].status==LIVE_WAIT&&fields[0][0]==0&&fields[0][2]==0); /* Never guess ambiguous ownership. */
    input[0]=0;input[1]=1;frame(&s,sync,binder);assert(s.flags[0].status==LIVE_ON&&fields[0][0]==0&&fields[0][2]==1);
    unsigned char relocated[sizeof(records)];memcpy(relocated,records,sizeof(records));ptr(binder,0x88,relocated);ptr(binder,0x90,relocated+sizeof(relocated));live_cheats_stop(&s);frame(&s,sync,binder);
    for(int i=0;i<2;++i)assert(s.flags[i].status==LIVE_OFF&&fields[i][0]==0&&fields[i][2]==0);
    CheatBindingCache cache={0};CheatBindingFrame result;unsigned command[2]={1|(1<<2)|(1<<4),1};
    integer(nodes[0],8,-1);cheat_bindings_apply(&cache,(uintptr_t)binder,0,3,command,&result);assert(!result.count[0]&&fields[0][2]==0);integer(nodes[0],8,0);
    unsigned char *readonly=VirtualAlloc(NULL,4096,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);assert(readonly);ptr(nodes[0],16,readonly);DWORD old;assert(VirtualProtect(readonly,4096,PAGE_READONLY,&old));cache=(CheatBindingCache){0};
    cheat_bindings_apply(&cache,(uintptr_t)binder,0,3,command,&result);unsigned known[2],enabled[2];cheat_bindings_observe(&result,known,enabled);assert((result.failed[0]&2)&&!(known[0]&1)&&!*readonly);ptr(nodes[0],16,&fields[0][2]);VirtualFree(readonly,0,MEM_RELEASE);
    memcpy(base+p->hook[0],p->on[0],32);live_cheats_tick(&s);assert(s.flags[0].status==LIVE_LAUNCHER&&!live_cheats_toggle(&s,0));assert(!live_cheats_toggle(&s,2));
    for(int i=0;i<2;++i)for(int j=0;j<2;++j)assert(fields[i][j*2+1]==0xa5);
    puts("PASS: real SysV binding hook; P1 controller mapped to either fighter, two-pad reassignment and opponent clearing; no ambiguous guessing; bidirectional Off regression, relocated tables, invalid nodes and failed writes; only fixture memory changed.");
    return 0;
}
