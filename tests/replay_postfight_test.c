#define WIN32_LEAN_AND_MEAN
#include <windows.h>
static ULONGLONG fixture_now=1000;
#define REPLAY_NOW() fixture_now
#define REPLAY_NATIVE_TEST
#include "../src/replay_controls.c"
#undef NDEBUG
#include <assert.h>
#include <stdio.h>
#include "../src/exact_hooks.h"

static int __attribute__((sysv_abi)) fake_create(uintptr_t m,uintptr_t c,uintptr_t b,uintptr_t e,float x,float y,float z){assert(m==11&&c==22&&b==33&&e==44&&x==1.25f&&y==2.5f&&z==.75f);return 123;}

static void trampoline(void){
 static const unsigned char head[]={0x55,0x48,0x89,0xe5,0x41,0x57,0x41,0x56,0x41,0x55,0x41,0x54,0x53,0x48,0x81,0xec,0x98,0,0,0};
 static const unsigned char tail[]={0x48,0x81,0xc4,0x98,0,0,0,0x5b,0x41,0x5c,0x41,0x5d,0x41,0x5e,0x41,0x5f,0x5d,0x48,0xb8};
 unsigned char *code=VirtualAlloc(NULL,4096,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);assert(code);
 memcpy(code,head,sizeof(head));memcpy(code+sizeof(head),tail,sizeof(tail));uintptr_t dest=(uintptr_t)fake_create;memcpy(code+sizeof(head)+sizeof(tail),&dest,8);code[sizeof(head)+sizeof(tail)+8]=0xff;code[sizeof(head)+sizeof(tail)+9]=0xe0;
 DWORD old;assert(VirtualProtect(code,4096,PAGE_EXECUTE_READ,&old));FlushInstructionCache(GetCurrentProcess(),code,4096);
 ReplayCreate call=(ReplayCreate)code;assert(call(11,22,33,44,1.25f,2.5f,.75f)==123);
 ExactHook hook={code,(void*)post_create,(void**)&post_create_original,head,sizeof(head)};const char *error=NULL;assert(install_exact_hooks(&hook,1,&error));
 for(int i=0;i<10000;++i)assert(call(11,22,33,44,1.25f,2.5f,.75f)==123);
}
static unsigned char objects[14][0x68],buffers[14][36],recorder[0x128],manager[0x240],clip[0x80],node[32];
static uintptr_t buckets[48],entries[2];
static void putq(void *p,uintptr_t n){memcpy(p,&n,8);}
static void puti(void *p,int n){memcpy(p,&n,4);}
static int val(int i){int n;memcpy(&n,buffers[i]+32,4);return n;}
static void field(int i,int n){puti(buffers[i]+32,n);}
static void tick(void){fixture_now+=21;post_update((uintptr_t)recorder);}
static void settle(void){for(int i=0;i<4;++i)tick();}
static void lifecycle(void){
 void *mapping=VirtualAlloc((void*)0x90b440000ULL,0x30000,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);assert(mapping==(void*)0x90b440000ULL);
 for(int i=0;i<14;++i){putq((void*)post_specs[i].slot,(uintptr_t)objects[i]);putq(objects[i],0x909c227f0ULL);putq(objects[i]+8,(uintptr_t)post_specs[i].name);putq(objects[i]+0x28,0x123456);puti(objects[i]+0x58,4);putq(objects[i]+0x60,(uintptr_t)buffers[i]);buffers[i][28]=32;}
 putq(recorder,0x909c247c0ULL);puti(recorder+0xb0,2);puti(recorder+0xb4,2);
 double bounds[]={10,30};memcpy(recorder+0xd0,bounds,16);
 putq(manager+0x30,(uintptr_t)recorder);manager[0x23c]=1;putq(manager+0xf8,(uintptr_t)entries);putq(manager+0x100,(uintptr_t)(entries+2));putq(manager+0x90,(uintptr_t)buckets);puti(manager+0x98,47);
 entries[0]=(uintptr_t)clip;puti(clip,113);double times[]={15,17};memcpy(clip+0x60,times,16);
 puti(node,113);putq(node+8,(uintptr_t)clip);buckets[113%47]=(uintptr_t)node;post_manager=manager;
 memset(&post_state,0,sizeof(post_state));replay_offer_decisions(1);
 field(7,5);field(8,0);field(9,1);field(10,7);field(11,0);field(12,22);tick();assert(post_state.seen_fight);
 field(8,4);field(9,3);field(10,3);for(int i=0;i<5;++i)field(i,i? -1:113);field(5,0);field(6,0);settle();assert(!post_state.owned&&val(6)==0);field(5,1);tick();assert(!post_state.owned);settle();assert(post_state.owned&&val(5)==5&&val(6)==1);
 for(int i=0;i<5;++i)field(i,-1);field(5,0);tick();assert(val(5)==0);settle();assert(post_state.owned&&val(5)==5&&val(0)==113);
 /* Repeated entry and idle exit retain playable slots even after opting out. */
 for(int j=0;j<3;++j){puti(recorder+0xb0,3);puti(recorder+0xb4,3);tick();assert(post_state.owned&&val(4)==113);puti(recorder+0xb0,1);puti(recorder+0xb4,1);tick();assert(post_state.owned&&val(5)==5);}
 replay_offer_decisions(0);tick();assert(post_state.owned);
 field(12,0);tick();assert(!post_state.owned&&val(1)==-1&&val(5)==0&&val(6)==0);
 /* A new fight's reset wins over restoration from the preceding fight. */
 replay_offer_decisions(1);field(12,22);field(8,0);field(9,1);puti(recorder+0xb0,2);puti(recorder+0xb4,2);tick();
 field(8,4);field(9,3);field(10,3);settle();assert(post_state.owned);
 field(8,0);field(9,1);field(1,999);tick();assert(!post_state.owned&&val(1)==999);
 VirtualFree(mapping,0,MEM_RELEASE);
}
int main(void){
 post_create_original=fake_create;assert(post_create(11,22,33,44,1.25f,2.5f,.75f)==123&&(uintptr_t)post_manager==11);
 int h[5]={113,-1,-1,-1,-1},ids[]={113,114,116,115},t[5],count;
 assert(post_plan(h,5,3,ids,4,t,&count)&&count==5);for(int i=0;i<5;++i)assert(t[i]==113);
 int empty[5]={-1,-1,-1,-1,-1};
 assert(post_plan(empty,5,10,ids,4,t,&count)&&count==4&&t[0]==113&&t[3]==115&&t[4]==-1);
 assert(post_plan(empty,3,9,ids,4,t,&count)&&count==4);
 assert(post_plan(empty,3,3,ids,4,t,&count)&&count==3&&t[3]==-1&&t[4]==-1);
 int stale[5]={999,999,999,999,999};assert(post_plan(stale,5,3,ids,4,t,&count)&&t[0]==113);
 assert(!post_plan(empty,5,3,ids,0,t,&count));assert(!post_plan(h,5,1,ids,4,t,&count));assert(!post_plan(h,2,3,ids,4,t,&count));
 int invalid[]={-1,0};assert(!post_plan(empty,5,10,invalid,2,t,&count));
 replay_offer_decisions(1);assert(post_requested==1);replay_offer_decisions(0);assert(post_requested==0);
 trampoline();lifecycle();
 puts("PASS: lifecycle repeat entry/idle exit, opt-out deferral, cleanup, new-fight ownership; selected clip reuse, zero-selected draws, retained clip fallback, stale IDs, no-clip rejection and opt-in setting.");
}
