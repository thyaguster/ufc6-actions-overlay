#define WIN32_LEAN_AND_MEAN
#include <windows.h>
static ULONGLONG fixture_now=5000;
static ULONGLONG fixture_clock(void){return fixture_now;}
#define PHOTO_NOW fixture_clock
#define NATIVE_CAMERA_TEST
#include "../src/photo_mode.c"
#undef NDEBUG
#include <assert.h>
static unsigned char *o,*c,*reg,*node;
static int original_calls,mode_calls,pose_calls;
static float seen_dt;
static float seed[16]={1,0,0,0,0,1,0,0,0,0,1,0,10,2,3,1};
static void __attribute__((sysv_abi)) fake_original(uintptr_t p,float dt){assert(p==(uintptr_t)o);original_calls++;seen_dt=dt;fixture_now+=40;}
static void __attribute__((sysv_abi)) fake_pose(uintptr_t p,const float *v){assert(p==(uintptr_t)c);memcpy(c+0x180,v,64);pose_calls++;}
static void __attribute__((sysv_abi)) fake_mode(uintptr_t p,unsigned mode){assert(p==(uintptr_t)o);*(unsigned*)(o+8)=mode;mode_calls++;if(mode==1){memcpy(c+0x180,seed,64);o[0xd9]=1;}else memcpy(c+0x180,seed,64);}
static void reset(void){memset(o,0,4096);memset(c,0,4096);memset(reg,0,4096);memset(node,0,4096);*(uintptr_t*)o=0x9099aa5b8ULL;*(uintptr_t*)(o+0xc8)=(uintptr_t)c;*(uintptr_t*)(o+0xc0)=(uintptr_t)reg;*(uintptr_t*)c=0x9099ff808ULL;*(uintptr_t*)(reg+0x38)=(uintptr_t)node;*(uintptr_t*)(node+0x28)=(uintptr_t)c;memcpy(c+0x180,seed,64);*(float*)(c+0xac)=55;memset(&published_input,0,sizeof(published_input));commands=0;input_tick=PHOTO_NOW();owner=0;state=0;requested=1;focused=1;mode_calls=pose_calls=0;}
static unsigned char hal[0x2000],hud_stats[0x100],hud_data[4096];
static uintptr_t hud_list[2],hud_all[2],hud_args[6];
static unsigned hud_total,hud_housekeeping;
static void emit_hud(int which,uintptr_t a,uintptr_t b,uintptr_t c,uintptr_t d,uintptr_t e,uintptr_t f){
 uintptr_t v[]={a,b,c,d,e,f};memcpy(hud_args,v,sizeof(v));hud_total++;hud_housekeeping++;
 uint32_t *used=(uint32_t*)((char*)hud_list+8),*all=(uint32_t*)((char*)hud_all+8);
 unsigned size=which?32:24;assert(*used+16+size<=sizeof(hud_data));
 memset(hud_data+*used,0x5a,16);*used+=16;*all+=16;
 *(uint64_t*)(hud_data+*used)=which?0x0000000400200006ULL:0x0000000400180004ULL;
 *(uint32_t*)(hud_data+*used+8)=(uint32_t)b;
 *used+=size;*all+=size;(*(uint32_t*)(hud_stats+0x7c))++;
}
static void __attribute__((sysv_abi)) fake_hud_a(uintptr_t a,uintptr_t b,uintptr_t c,uintptr_t d,uintptr_t e,uintptr_t f){emit_hud(0,a,b,c,d,e,f);}
static void __attribute__((sysv_abi)) fake_hud_b(uintptr_t a,uintptr_t b,uintptr_t c,uintptr_t d,uintptr_t e,uintptr_t f){emit_hud(1,a,b,c,d,e,f);}
static void check_hud(int which,int hidden){
 uint32_t before=*(uint32_t*)((char*)hud_list+8);unsigned calls=hud_total;
 if(which)hud_draw_b((uintptr_t)hal,6,7,8,9,10);else hud_draw_a((uintptr_t)hal,6,7,8,9,10);
 assert(hud_total==calls+1&&hud_housekeeping==hud_total);
 assert(hud_args[0]==(uintptr_t)hal&&hud_args[1]==6&&hud_args[5]==10);
 assert(*(uint32_t*)((char*)hud_list+8)==before+16+(hidden?0:(which?32:24)));
 assert(*(uint32_t*)((char*)hud_all+8)==*(uint32_t*)((char*)hud_list+8));
 assert(hud_data[before]==0x5a); /* state packet survives */
}
static void test_hud(void){
 memset(hal,0,sizeof(hal));*(uintptr_t*)hal=0x909ba4518ULL;
 *(uintptr_t*)(hal+0x1580)=(uintptr_t)hud_stats;*(uintptr_t*)(hal+0x1598)=(uintptr_t)hud_list;*(uintptr_t*)(hal+0x15a8)=(uintptr_t)hud_all;
 hud_list[0]=(uintptr_t)hud_data;*(uint32_t*)((char*)hud_list+12)=sizeof(hud_data);
 hud_originals[0]=fake_hud_a;hud_originals[1]=fake_hud_b;
 reset();update((uintptr_t)o,0);photo_mode_poll(1,0,NULL);
 PhotoSnapshot snap;photo_mode_snapshot(&snap);assert(snap.hud_hidden&&snap.state==2);
 check_hud(0,1);check_hud(1,1);
 photo_mode_poll(1,1,NULL);check_hud(0,1);check_hud(1,1);
 for(int i=0;i<20;++i){photo_mode_poll(1,i&1,NULL);check_hud(i&1,1);photo_mode_snapshot(&snap);assert(snap.hud_hidden&&snap.state==2);}
 photo_mode_poll(1,1,NULL);photo_mode_command(PHOTO_HUD);update((uintptr_t)o,0);check_hud(0,0);
 photo_mode_poll(1,0,NULL);check_hud(1,0);photo_mode_command(PHOTO_HUD);update((uintptr_t)o,0);check_hud(0,1);
 photo_mode_poll(1,0,NULL);photo_mode_command(PHOTO_HUD);update((uintptr_t)o,0);photo_mode_snapshot(&snap);assert(!snap.hud_hidden);check_hud(1,0);
 photo_mode_command(PHOTO_HUD);update((uintptr_t)o,0);check_hud(1,1);
 *(uintptr_t*)hal=0;check_hud(0,0);*(uintptr_t*)hal=0x909ba4518ULL;
 fixture_now+=1001;photo_mode_poll(1,0,NULL);check_hud(1,0);update((uintptr_t)o,0);check_hud(0,1);
 fixture_now+=1001;InterlockedExchange64(&hud_camera_tick,(LONG64)PHOTO_NOW());check_hud(1,0);photo_mode_poll(1,0,NULL);
 photo_mode_command(PHOTO_STOP);check_hud(0,0);update((uintptr_t)o,0);assert(!hud_requested&&state==3);
 reset();update((uintptr_t)o,0);photo_mode_poll(1,0,NULL);photo_mode_poll(0,0,NULL);check_hud(1,0);assert(!hud_requested);update((uintptr_t)o,0);
 /* No draw / nested draws / bad headers / bad sizes / metadata alias must fail open. */
 uintptr_t stats=(uintptr_t)hud_stats;uint32_t count=*(uint32_t*)(hud_stats+0x7c);
 uint32_t used=*(uint32_t*)((char*)hud_list+8),all=*(uint32_t*)((char*)hud_all+8);
 assert(!photo_hud_trim_draw((uintptr_t)hal,0,stats,count));
 assert(!photo_hud_trim_draw((uintptr_t)hal,0,stats,count-2));
 *(uint64_t*)(hud_data+used-24)=0xfeed;
 assert(!photo_hud_trim_draw((uintptr_t)hal,0,stats,count-1));
 *(uint64_t*)(hud_data+used-24)=0x0000000400180004ULL;
 *(uint32_t*)((char*)hud_list+8)=8;assert(!photo_hud_trim_draw((uintptr_t)hal,0,stats,count-1));
 *(uint32_t*)((char*)hud_list+8)=sizeof(hud_data)+1;assert(!photo_hud_trim_draw((uintptr_t)hal,0,stats,count-1));
 *(uint32_t*)((char*)hud_list+8)=used;*(uintptr_t*)(hal+0x15a8)=(uintptr_t)hud_list;assert(!photo_hud_trim_draw((uintptr_t)hal,0,stats,count-1));
 *(uintptr_t*)(hal+0x15a8)=(uintptr_t)hud_all;assert(*(uint32_t*)((char*)hud_all+8)==all);
 DWORD old;assert(VirtualProtect(hud_all,sizeof(hud_all),PAGE_READONLY,&old));
 assert(!photo_hud_trim_draw((uintptr_t)hal,0,stats,count-1));
 assert(VirtualProtect(hud_all,sizeof(hud_all),old,&old));
 assert(*(uint32_t*)((char*)hud_list+8)==used&&*(uint32_t*)((char*)hud_all+8)==all);
}
static void test_hud_pages(void){
 unsigned char *pages=VirtualAlloc(NULL,16384,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);assert(pages);memset(pages,1,16384);
 uintptr_t p=(uintptr_t)pages;assert(photo_hud_range(p+4090,8200,1));
 DWORD old;assert(VirtualProtect(pages+4096,4096,PAGE_READONLY,&old));
 assert(photo_hud_range(p,12288,0)&&!photo_hud_range(p,12288,1));
 assert(VirtualProtect(pages+4096,4096,PAGE_READWRITE|PAGE_GUARD,&old));
 assert(!photo_hud_range(p,12288,0));
 assert(VirtualProtect(pages+4096,4096,PAGE_NOACCESS,&old));assert(!photo_hud_range(p,12288,0));
 assert(VirtualFree(pages+4096,4096,MEM_DECOMMIT));assert(!photo_hud_range(p,12288,0));
 assert(!photo_hud_range(0,1,0)&&!photo_hud_range(p,0,0)&&!photo_hud_range(UINTPTR_MAX-2,8,0)&&!photo_hud_range(p,9*4096,0));
 assert(VirtualFree(pages,0,MEM_RELEASE));
}
int main(void){
 o=VirtualAlloc(NULL,4096,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);c=VirtualAlloc(NULL,4096,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);reg=VirtualAlloc(NULL,4096,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);node=VirtualAlloc(NULL,4096,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);assert(o&&c&&reg&&node);
 installed=1;original=fake_original;set_mode=fake_mode;set_pose=fake_pose;
 reset();ULONGLONG before=PHOTO_NOW();update((uintptr_t)o,.016f);assert(state==2&&*(unsigned*)(o+8)==1&&mode_calls==1&&seen_dt==.016f);assert(deadline>=before+1800000&&deadline<=PHOTO_NOW()+1800000);assert(((float*)(c+0x180))[12]==10.f);
 published_input.right=1;published_input.zoom=1;update((uintptr_t)o,.04f);assert(((float*)(c+0x180))[12]>10.f&&*(float*)(c+0xac)>55);
 published_input.zoom=0;photo_mode_set_fov(70);update((uintptr_t)o,.01f);assert(*(float*)(c+0xac)==70.f);photo_mode_set_fov(200);update((uintptr_t)o,.01f);assert(*(float*)(c+0xac)==70.f);
 memset(&published_input,0,sizeof(published_input));commands=PHOTO_RESET;update((uintptr_t)o,.01f);assert(((float*)(c+0x180))[12]==10.f&&*(float*)(c+0xac)==55);
 deadline=PHOTO_NOW();update((uintptr_t)o,.02f);assert(state==3&&owner==0&&*(unsigned*)(o+8)==0&&mode_calls==2);assert(memcmp(c+0x180,seed,64)==0&&o[0xd9]==0&&*(float*)(c+0xac)==55);
 reset();update((uintptr_t)o,.01f);focused=0;update((uintptr_t)o,.01f);assert(state==3&&owner==0&&*(unsigned*)(o+8)==0);
 reset();*(uintptr_t*)c=0;update((uintptr_t)o,0);assert(state==-1&&!owner&&mode_calls==0);
 reset();o[0xd9]=1;update((uintptr_t)o,0);assert(state==-2&&!owner&&mode_calls==0);
 reset();update((uintptr_t)o,0);*(unsigned*)(o+8)=2;update((uintptr_t)o,0);assert(state==-4&&!owner&&*(unsigned*)(o+8)==2&&mode_calls==1);
 assert(original_calls==12);
 unsigned char *time_mem=VirtualAlloc(NULL,4096,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);assert(time_mem);*(uintptr_t*)time_mem=0x9098fe7d8ULL;*(uintptr_t*)(time_mem+8)=0x90a1d9c00ULL;*(float*)(time_mem+0x28)=1.25f;fixture_time_object=(uintptr_t)time_mem;
 reset();update((uintptr_t)o,.01f);freeze_toggle();assert(frozen&&*(float*)(time_mem+0x28)==0);
 published_input.right=1;float x=((float*)(c+0x180))[12];update((uintptr_t)o,0);assert(((float*)(c+0x180))[12]>x);focused=0;update((uintptr_t)o,0);assert(*(float*)(time_mem+0x28)==1.25f&&!frozen);
 reset();update((uintptr_t)o,.01f);freeze_toggle();assert(frozen);*(float*)(time_mem+0x28)=.5f;freeze_restore();assert(*(float*)(time_mem+0x28)==.5f);*(float*)(time_mem+0x28)=1.25f;

 reset();requested=0;assert(!photo_mode_start(0));assert(!photo_mode_blocks_input());
 reset();requested=0;assert(photo_mode_start(1)&&photo_mode_blocks_input());photo_mode_poll(0,0,NULL);assert(!photo_mode_blocks_input());
 reset();update((uintptr_t)o,0);assert(photo_mode_blocks_input());photo_mode_command(PHOTO_STOP);
 assert(photo_mode_stop_pending()&&!photo_mode_blocks_input()&&state==2&&owner==(uintptr_t)o);
 assert(!photo_mode_start(1));update((uintptr_t)o,0);assert(state==3&&!photo_mode_stop_pending()&&owner==0);
 reset();update((uintptr_t)o,0);freeze_toggle();assert(frozen);photo_mode_poll(0,0,NULL);assert(!frozen&&*(float*)(time_mem+0x28)==1.25f);update((uintptr_t)o,0);assert(state==3);
 reset();update((uintptr_t)o,0);published_input.right=1;photo_mode_poll(1,1,&published_input);assert(published_input.right==0);commands=PHOTO_STOP;update((uintptr_t)o,0);assert(state==3);
 reset();update((uintptr_t)o,0);fixture_now+=1100;update((uintptr_t)o,0);assert(state==3&&!photo_mode_blocks_input());
 test_hud();test_hud_pages();
 puts("PASS: guarded HUD packet suppression preserves native bookkeeping, real counts and state commands; HUD default/toggle, both draw routes, menu visibility stability and explicit/focus/exit/stale-input restoration, foreign-owner forwarding and six arguments; native camera entry, movement/FOV/reset, frozen real-time movement, focus/timeout/exit restoration, external ownership and invalid identities.");return 0;
}
