#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <string.h>
#include <math.h>
#include "photo_math.h"
#include "photo_mode.h"
#ifndef NATIVE_CAMERA_TEST
#ifndef QUIT_MENU_TEST
#include "engine_fingerprint.h"
#endif
#include "exact_hooks.h"
#endif
typedef void (__attribute__((sysv_abi)) *Update)(uintptr_t,float);
typedef void (__attribute__((sysv_abi)) *Mode)(uintptr_t,unsigned);
typedef void (__attribute__((sysv_abi)) *Pose)(uintptr_t,const float*);
static Update original;
static Mode set_mode;
static Pose set_pose;
static volatile LONG requested,focused,state,ticks;
static uintptr_t owner,camera;
static unsigned previous_mode;
static unsigned char saved_flags[3];
static float saved_pose[16],test_pose[16];
#ifndef PHOTO_NOW
#define PHOTO_NOW GetTickCount64
#endif
static ULONGLONG deadline;
static SRWLOCK photo_guard=SRWLOCK_INIT;
static PhotoInput published_input;
static volatile LONG commands,installed;
static ULONGLONG input_tick;
static float initial_pose[16],saved_fov,current_fov;

static int readable(uintptr_t p,size_t n){
 MEMORY_BASIC_INFORMATION b;if(!p||!VirtualQuery((void*)p,&b,sizeof(b)))return 0;
 return b.State==MEM_COMMIT&&!(b.Protect&(PAGE_NOACCESS|PAGE_GUARD))&&p+n>=p&&p+n<=(uintptr_t)b.BaseAddress+b.RegionSize;
}
/* Confirmed Scaleform geometry draws; no game HUD preference is modified. */
typedef void (__attribute__((sysv_abi)) *HudDraw)(uintptr_t,uintptr_t,uintptr_t,uintptr_t,uintptr_t,uintptr_t);
static HudDraw hud_originals[2];
#include "photo_hud_commands.h"
static volatile LONG hud_trimmed[2],hud_trim_rejected[2];
static volatile LONG hud_requested,hud_menu_open;
static volatile LONG64 hud_poll_tick,hud_camera_tick;
static void hud_draw(int which,uintptr_t o,uintptr_t a,uintptr_t b,uintptr_t c,uintptr_t d,uintptr_t e){
 int hide=InterlockedCompareExchange(&hud_requested,0,0)&&InterlockedCompareExchange(&state,0,0)==2&&
    InterlockedCompareExchange(&focused,0,0)&&!InterlockedCompareExchange(&hud_menu_open,0,0)&&
    !(InterlockedCompareExchange(&commands,0,0)&PHOTO_STOP)&&
    PHOTO_NOW()-(ULONGLONG)InterlockedCompareExchange64(&hud_poll_tick,0,0)<1000&&
    PHOTO_NOW()-(ULONGLONG)InterlockedCompareExchange64(&hud_camera_tick,0,0)<1000&&
    photo_hud_range(o,0x15b0,0)&&*(uintptr_t*)o==0x909ba4518ULL;
 uintptr_t stats=hide?*(uintptr_t*)(o+0x1580):0;
 int tracked=hide&&photo_hud_range(stats,0x80,0);
 uint32_t before=tracked?*(uint32_t*)(stats+0x7c):0;
 /* Preserve real counts and all native renderer housekeeping. Suppress only the
    verified final geometry packet before this CPU command list is submitted. */
 hud_originals[which](o,a,b,c,d,e);
 if(tracked){
  if(photo_hud_trim_draw(o,which,stats,before))InterlockedIncrement(&hud_trimmed[which]);
  else InterlockedIncrement(&hud_trim_rejected[which]);
 }
}
static void __attribute__((sysv_abi)) hud_draw_a(uintptr_t o,uintptr_t a,uintptr_t b,uintptr_t c,uintptr_t d,uintptr_t e){hud_draw(0,o,a,b,c,d,e);}
static void __attribute__((sysv_abi)) hud_draw_b(uintptr_t o,uintptr_t a,uintptr_t b,uintptr_t c,uintptr_t d,uintptr_t e){hud_draw(1,o,a,b,c,d,e);}
#include "photo_cage.h"
static uintptr_t time_object;
static LONG saved_scale_bits;
static volatile LONG frozen;
static int time_identity(uintptr_t p){return readable(p,0x40)&&*(uintptr_t*)p==0x9098fe7d8ULL&&*(uintptr_t*)(p+8)==0x90a1d9c00ULL;}
#ifndef NATIVE_CAMERA_TEST
static uintptr_t resolve_time_object(void){return readable(0x909e41a10ULL,8)?*(uintptr_t*)0x909e41a10ULL:0;}
#else
static uintptr_t fixture_time_object;
static uintptr_t resolve_time_object(void){return fixture_time_object;}
#endif
static int freeze_capture(void){
 time_object=0;uintptr_t p=resolve_time_object();if(!time_identity(p))return 0;
 float value=*(float*)(p+0x28);if(!isfinite(value)||value<.1f||value>4)return 0;
 time_object=p;memcpy(&saved_scale_bits,&value,4);return 1;
}
static void freeze_restore(void){
 if(frozen&&time_identity(time_object))InterlockedCompareExchange((volatile LONG*)(time_object+0x28),saved_scale_bits,0);
 InterlockedExchange(&frozen,0);
}
static void freeze_toggle(void){
 if(!time_identity(time_object))return;
 if(frozen){freeze_restore();return;}
 if(InterlockedCompareExchange((volatile LONG*)(time_object+0x28),0,saved_scale_bits)==saved_scale_bits)InterlockedExchange(&frozen,1);
}

static ULONGLONG last_camera_tick;
static int valid_pose(const float *p){
 for(int i=0;i<16;++i)if(!isfinite(p[i]))return 0;
 for(int axis=0;axis<3;++axis){float a=0;for(int j=0;j<3;++j)a+=p[axis*4+j]*p[axis*4+j];if(a<.8f||a>1.2f)return 0;}
 return fabsf(p[12])<100000&&fabsf(p[13])<100000&&fabsf(p[14])<100000;
}
static int identity(uintptr_t o){
 if(!readable(o,0x120)||*(uintptr_t*)o!=0x9099aa5b8ULL)return 0;
 uintptr_t c=*(uintptr_t*)(o+0xc8),registry=*(uintptr_t*)(o+0xc0);
 if(!readable(c,0x1e0)||*(uintptr_t*)c!=0x9099ff808ULL||!readable(registry,0x40))return 0;
 uintptr_t n=*(uintptr_t*)(registry+0x38);
 return readable(n,0x30)&&*(unsigned*)(n+0x20)==0&&*(uintptr_t*)(n+0x28)==c;
}
static void restore(void){
 InterlockedExchange(&hud_requested,0);cage_apply(0);freeze_restore();
 if(!owner||!identity(owner)||*(uintptr_t*)(owner+0xc8)!=camera){InterlockedExchange(&state,-3);owner=0;return;}
 if(*(unsigned*)(owner+8)==1){
  set_mode(owner,previous_mode);
  set_pose(camera,saved_pose);
  *(float*)(camera+0xac)=saved_fov;
  *(unsigned char*)(owner+0xd8)=saved_flags[0];
  *(unsigned char*)(owner+0xd9)=saved_flags[1];
  *(unsigned char*)(owner+0xe0)=saved_flags[2];
  InterlockedExchange(&state,3);
 }else InterlockedExchange(&state,-4);
 owner=0;
}
static void camera_update(uintptr_t o,float dt){
 (void)dt;InterlockedIncrement(&ticks);
 if(owner){
  if(o!=owner)return;
  if(!focused||PHOTO_NOW()>=deadline||PHOTO_NOW()-input_tick>1000||(InterlockedCompareExchange(&commands,0,0)&PHOTO_STOP)){restore();return;}
  if(!identity(owner)||*(unsigned*)(owner+8)!=1){restore();return;}
  InterlockedExchange64(&hud_camera_tick,(LONG64)PHOTO_NOW());
  unsigned command=(unsigned)InterlockedExchange(&commands,0);
  if(command&PHOTO_RESET){memcpy(test_pose,initial_pose,64);current_fov=saved_fov;}
  if(command&PHOTO_FREEZE)freeze_toggle();
  if(command&PHOTO_HUD)InterlockedExchange(&hud_requested,!InterlockedCompareExchange(&hud_requested,0,0));
  if(command&PHOTO_CAGE)InterlockedExchange(&cage_requested,!InterlockedCompareExchange(&cage_requested,0,0));
  ULONGLONG now=PHOTO_NOW();float camera_dt=(float)(now-last_camera_tick)*.001f;last_camera_tick=now;
  PhotoInput in=published_input;
  photo_step(test_pose,&current_fov,camera_dt,in.right,in.up,in.forward,in.yaw,in.pitch,in.zoom,in.fast);
  set_pose(camera,test_pose);*(float*)(camera+0xac)=current_fov;return;
 }
 if(InterlockedCompareExchange(&requested,0,1)!=1)return;
 if(!focused||!identity(o)||*(unsigned*)(o+8)!=0){InterlockedExchange(&state,-1);return;}
 owner=o;camera=*(uintptr_t*)(o+0xc8);previous_mode=*(unsigned*)(o+8);
 memcpy(saved_pose,(void*)(camera+0x180),64);saved_fov=*(float*)(camera+0xac);current_fov=saved_fov;
 saved_flags[0]=*(unsigned char*)(o+0xd8);saved_flags[1]=*(unsigned char*)(o+0xd9);saved_flags[2]=*(unsigned char*)(o+0xe0);
 /* A fresh owner must seed from the current render camera using the native API. */
 if(saved_flags[1]||!valid_pose(saved_pose)||!isfinite(saved_fov)||saved_fov<20||saved_fov>120){owner=0;InterlockedExchange(&state,-2);return;}
 set_mode(o,1);
 memcpy(test_pose,(void*)(camera+0x180),64);
 if(*(unsigned*)(o+8)!=1||!valid_pose(test_pose)){restore();return;}
 memcpy(initial_pose,test_pose,64);InterlockedExchange(&commands,0);last_camera_tick=PHOTO_NOW();
 freeze_capture();InterlockedExchange64(&hud_camera_tick,(LONG64)PHOTO_NOW());InterlockedExchange(&hud_requested,1);
 set_pose(camera,test_pose);deadline=PHOTO_NOW()+1800000;InterlockedExchange(&state,2);
}

static void __attribute__((sysv_abi)) update(uintptr_t o,float dt){
 original(o,dt);
 if(TryAcquireSRWLockExclusive(&photo_guard)){camera_update(o,dt);cage_apply(cage_active());ReleaseSRWLockExclusive(&photo_guard);}
}
int photo_mode_blocks_input(void){return InterlockedCompareExchange(&state,0,0)==2||InterlockedCompareExchange(&requested,0,0)!=0;}
void photo_mode_command(unsigned command){
 InterlockedOr(&commands,(LONG)command);
 if(command&PHOTO_STOP){
  InterlockedExchange(&hud_requested,0);
  AcquireSRWLockExclusive(&photo_guard);
  freeze_restore();if(requested){InterlockedExchange(&requested,0);InterlockedExchange(&state,3);}
  ReleaseSRWLockExclusive(&photo_guard);
 }
}
void photo_mode_snapshot(PhotoSnapshot *out){
 out->cage_count=0;out->cage_writes=InterlockedCompareExchange(&cage_writes,0,0);out->cage_restores=InterlockedCompareExchange(&cage_restores,0,0);
 if(TryAcquireSRWLockShared(&cage_guard)){for(int i=0;i<CAGE_PANEL_CAPACITY;++i)if(cage_panels[i].object)out->cage_count++;ReleaseSRWLockShared(&cage_guard);}
 out->cage_visible=(int)InterlockedCompareExchange(&cage_requested,0,0);out->cage_available=InterlockedCompareExchange(&cage_installed,0,0)==1;
 out->state=(int)InterlockedCompareExchange(&state,0,0);out->frozen=(int)InterlockedCompareExchange(&frozen,0,0);out->fov=0;out->hud_hidden=(int)InterlockedCompareExchange(&hud_requested,0,0);
 if(TryAcquireSRWLockShared(&photo_guard)){out->fov=(int)(current_fov+.5f);ReleaseSRWLockShared(&photo_guard);}
}
void photo_mode_poll(int focus,int menu_open,const PhotoInput *input){
 InterlockedExchange(&focused,focus);InterlockedExchange(&hud_menu_open,menu_open);InterlockedExchange64(&hud_poll_tick,(LONG64)PHOTO_NOW());
 if(!focus)InterlockedExchange(&hud_requested,0);
 if(!focus)InterlockedOr(&commands,PHOTO_STOP);
 if(TryAcquireSRWLockExclusive(&photo_guard)){
  memset(&published_input,0,sizeof(published_input));if(focus&&!menu_open&&input)published_input=*input;
  input_tick=PHOTO_NOW();
  if(!focus){freeze_restore();if(requested){InterlockedExchange(&requested,0);InterlockedExchange(&state,3);}}
  if(requested&&PHOTO_NOW()>=deadline){InterlockedExchange(&requested,0);InterlockedExchange(&state,-5);}
  ReleaseSRWLockExclusive(&photo_guard);
 }
}
int photo_mode_start(int fight_ready){
 if(photo_mode_blocks_input()){photo_mode_command(PHOTO_STOP);return 1;}
 if(!fight_ready)return 0;
#ifndef NATIVE_CAMERA_TEST
 if(!installed){
#ifdef QUIT_MENU_TEST
  (void)update;(void)hud_draw_a;(void)hud_draw_b;return 0;
#else
  if(!engine_fingerprint())return PHOTO_START_BUILD;
  set_mode=(Mode)0x902a84520ULL;set_pose=(Pose)0x902b85580ULL;
  const unsigned char expected[]={0x55,0x48,0x89,0xe5,0x41,0x57,0x41,0x56,0x41,0x55,0x41,0x54,0x53,0x48,0x81,0xec,0x18,0x03,0,0};
  const unsigned char draw_a[]={0x55,0x48,0x89,0xe5,0x41,0x57,0x41,0x56,0x41,0x55,0x41,0x54,0x53,0x48,0x83,0xec,0x68};
  const unsigned char draw_b[]={0x55,0x48,0x89,0xe5,0x41,0x57,0x41,0x56,0x41,0x55,0x41,0x54,0x53,0x48,0x83,0xec,0x78};
  ExactHook hooks[]={
   {(unsigned char*)0x902a83c20ULL,(void*)update,(void**)&original,expected,sizeof(expected)},
   {(unsigned char*)0x905223f20ULL,(void*)hud_draw_a,(void**)&hud_originals[0],draw_a,sizeof(draw_a)},
   {(unsigned char*)0x905224c70ULL,(void*)hud_draw_b,(void**)&hud_originals[1],draw_b,sizeof(draw_b)}};const char *error=NULL;
  if(!install_exact_hooks(hooks,3,&error))return PHOTO_START_HOOKS;
  InterlockedExchange(&installed,1);
#endif
 }
#endif
 AcquireSRWLockExclusive(&photo_guard);
 deadline=PHOTO_NOW()+1800000;input_tick=PHOTO_NOW();InterlockedExchange(&commands,0);InterlockedExchange(&requested,1);
 ReleaseSRWLockExclusive(&photo_guard);return 1;
}
