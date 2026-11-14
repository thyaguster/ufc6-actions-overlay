/* Verified PORTHEON guest replay clock. Mutations run only on its update thread. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include "replay_controls.h"
#include "replay_seek.h"
#if !defined(REPLAY_NATIVE_TEST) && !defined(QUIT_MENU_TEST)
#include "engine_fingerprint.h"
#include "exact_hooks.h"
#endif
#ifndef REPLAY_NOW
#define REPLAY_NOW GetTickCount64
#endif
typedef void (__attribute__((sysv_abi)) *Update)(uintptr_t,float);
typedef void (__attribute__((sysv_abi)) *Seek)(uintptr_t,double);
static Update original;
static Seek seek_native;
static SRWLOCK guard=SRWLOCK_INIT;
static int installed,enabled,paused,valid,speed_index;
#if !defined(REPLAY_NATIVE_TEST) && !defined(QUIT_MENU_TEST)
static int engine_verified;
#endif
static uintptr_t last_owner,pause_owner;
static ULONGLONG poll_tick,last_tick;
static double clip_low,clip_high,pause_low,pause_high,current,pending;
static uintptr_t coverage_owner;
static int coverage_requested,coverage_attempted,coverage_known,coverage_ms;
static double coverage_clip_low,coverage_clip_high;
static ULONGLONG coverage_next_tick;
static int read_memory(uintptr_t p,void *out,size_t n){SIZE_T got=0;return p>=0x10000&&ReadProcessMemory(GetCurrentProcess(),(void*)p,out,n,&got)&&got==n;}
static int capture(uintptr_t owner,double *time,double *range,double *clip){
 unsigned char b[0x128];uintptr_t vt;unsigned requested_mode,mode;
 if(!read_memory(owner,b,sizeof(b)))return 0;
 memcpy(&vt,b,8);memcpy(&requested_mode,b+0xb0,4);memcpy(&mode,b+0xb4,4);memcpy(time,b+0xe0,8);memcpy(range,b+0xd0,16);memcpy(clip,b+0xf8,16);
 return vt==0x909c247c0ULL&&mode==3&&requested_mode==3&&isfinite(*time)&&*time>=.05&&*time<=14400&&
 isfinite(range[0])&&isfinite(range[1])&&range[0]<=*time&&*time<=range[1]&&
 isfinite(clip[0])&&isfinite(clip[1])&&clip[0]<clip[1]&&*time>=clip[0]&&*time<clip[1];
}
#include "replay_recording.h"
#include "replay_budget.h"
static int extended,range_request,range_failed;
static uintptr_t range_owner;
static double saved_low,saved_high,saved_time,extended_low,extended_high;
static int (*coverage_query)(double,const double*,ReplayRange*)=recording_coverage;
static void set_bounds(uintptr_t owner,double low,double high){memcpy((void*)(owner+0xf8),&low,8);memcpy((void*)(owner+0x100),&high,8);}
static void restore_range(uintptr_t owner,int ready,const double clip[2]){
 if(ready&&owner==range_owner&&clip[0]==extended_low&&clip[1]==extended_high){set_bounds(owner,saved_low,saved_high);seek_native(owner,saved_time);}
 extended=0;range_owner=0;paused=0;pending=0;
}
#include "replay_cinematic.h"
#include "replay_postfight.h"
static void __attribute__((sysv_abi)) update(uintptr_t owner,float delta){
 double before=0,range[2]={0},clip[2]={0},target=0;int ready=capture(owner,&before,range,clip);ULONGLONG now=REPLAY_NOW();
 AcquireSRWLockExclusive(&guard);
 if(ready&&(owner!=coverage_owner||clip[0]!=coverage_clip_low||clip[1]!=coverage_clip_high)){
  coverage_owner=owner;coverage_clip_low=clip[0];coverage_clip_high=clip[1];
  coverage_requested=coverage_attempted=coverage_known=coverage_ms=0;coverage_next_tick=0;
 }
 int fresh=enabled&&now-poll_tick<1000&&isfinite(delta)&&delta>=0&&delta<=.25f;
 int check_coverage=ready&&fresh&&coverage_requested&&!coverage_known&&coverage_attempted<5&&now>=coverage_next_tick;
 if(check_coverage){coverage_requested=0;++coverage_attempted;coverage_next_tick=now+1000;}
 if(extended&&(!fresh||!ready||owner!=range_owner||clip[0]!=extended_low||clip[1]!=extended_high||range[0]>extended_low||range[1]<extended_high)){
  restore_range(owner,ready,clip);ready=capture(owner,&before,range,clip);
 }
 if(range_request){
  range_request=0;range_failed=0;
  if(extended){restore_range(owner,ready,clip);ready=capture(owner,&before,range,clip);}
  else if(fresh&&ready){ReplayRange available;
   if(coverage_query(before,range,&available)&&available.low<=clip[0]&&available.high>=clip[1]&&available.high-available.low>clip[1]-clip[0]+.01){
    saved_low=clip[0];saved_high=clip[1];saved_time=before;extended_low=available.low;extended_high=available.high;range_owner=owner;extended=1;
    set_bounds(owner,extended_low,extended_high);seek_native(owner,extended_low);paused=1;pending=0;pause_owner=owner;pause_low=extended_low;pause_high=extended_high;
    ready=capture(owner,&before,range,clip);
   }else range_failed=1;
  }else range_failed=1;
 }
 /* The held finish cinematic can deliver zero delta even with nonzero replay
    speed. Advance only our active expanded playback from consecutive update
    ticks in that case; never catch up a stale, unfocused or paused interval. */
 float playback_delta=delta;
 if(extended&&!paused&&ready&&fresh&&delta==0){
  playback_delta=(owner==last_owner&&now>=last_tick&&now-last_tick<=250)?(float)(now-last_tick)/1000.f:0;
 }
 /* Numbered rates compensate the native cinematic's automatic slow motion.
    Do not mutate its speed or hold counters; native pause (speed zero) wins. */
 if(speed_index&&fresh&&ready&&!paused){
  static const float rates[]={0,.1f,.25f,.5f,1.f,2.f};float native_speed=0;
  read_memory(owner+0x10c,&native_speed,4);
  float elapsed=(owner==last_owner&&now>=last_tick&&now-last_tick<=250)?(float)(now-last_tick)/1000.f:0;
  playback_delta=isfinite(native_speed)&&native_speed>=.001f?elapsed*rates[speed_index]/native_speed:0;
 }
 if(extended&&!paused&&ready){float speed=0;read_memory(owner+0x10c,&speed,4);
  if(!isfinite(speed)||speed<0){paused=1;}
  else if(before+(double)playback_delta*speed>=extended_high-.001){seek_native(owner,extended_high-.001);before=extended_high-.001;paused=1;}
 }
 if(paused&&(!enabled||now-poll_tick>=1000||!ready||owner!=pause_owner||fabs(clip[0]-pause_low)>.000001||fabs(clip[1]-pause_high)>.000001||!isfinite(delta)||delta<0||delta>.25f))paused=0;
 float passed=paused?0:playback_delta;
 int apply=paused&&pending!=0&&seek_target(before,pending,range[0],range[1],clip[0],clip[1],&target);pending=0;
 ReleaseSRWLockExclusive(&guard);
 if(apply)seek_native(owner,target);
 original(owner,passed);
 post_update(owner);
 ReplayRange coverage={0};int has_coverage=check_coverage&&coverage_query(before,range,&coverage);
 double after=before;read_memory(owner+0xe0,&after,8);
 AcquireSRWLockExclusive(&guard);last_owner=owner;last_tick=now;valid=ready;clip_low=clip[0];clip_high=clip[1];current=after;
 if(check_coverage&&owner==coverage_owner&&has_coverage&&coverage.high>coverage.low){coverage_known=1;coverage_ms=(int)((coverage.high-coverage.low)*1000+.5);}
 ReleaseSRWLockExclusive(&guard);
}
int replay_prepare(void){
 if(installed)return installed>0;
#if defined(REPLAY_NATIVE_TEST) || defined(QUIT_MENU_TEST)
 (void)post_create;(void)update;(void)cinematic_update;(void)recording_init;installed=-1;return 0;
#else
 if(!engine_verified){if(!engine_fingerprint()){installed=-1;return 0;}engine_verified=1;}
 static const unsigned char head[]={0x55,0x48,0x89,0xe5,0x41,0x57,0x41,0x56,0x41,0x55,0x41,0x54,0x53,0x48,0x83,0xec,0x48};
 static const unsigned char seek_head[]={0x83,0xbf,0xb4,0,0,0,3,0x0f,0x85,0xbc,0,0,0,0x55,0x48,0x89,0xe5,0x53,0x50,0x48,0x8b,7,0x48,0x89,0xfb,0xc5,0xfb,0x11,0x45,0xf0,0xff,0x90};
 unsigned char bytes[32];if(!read_memory(0x905ac5ed0ULL,bytes,sizeof(head))||memcmp(bytes,head,sizeof(head))||!read_memory(0x905ac4610ULL,bytes,sizeof(seek_head))||memcmp(bytes,seek_head,sizeof(seek_head)))return 0;
 static const unsigned char pause_head[]={0x55,0x48,0x89,0xe5,0x41,0x57,0x41,0x56,0x41,0x55,0x41,0x54,0x53,0x48,0x81,0xec,0x28,1,0,0};
 static const unsigned char resume_head[]={0x55,0x48,0x89,0xe5,0x41,0x57,0x41,0x56,0x41,0x55,0x41,0x54,0x53,0x48,0x83,0xec,0x78};
 if(!read_memory(0x9013565d0ULL,bytes,sizeof(head))||memcmp(bytes,head,sizeof(head))||!read_memory(0x901354490ULL,bytes,sizeof(pause_head))||memcmp(bytes,pause_head,sizeof(pause_head))||!read_memory(0x90134f560ULL,bytes,sizeof(resume_head))||memcmp(bytes,resume_head,sizeof(resume_head)))return 0;
 cinematic_pause_native=(CinematicNative)0x901354490ULL;cinematic_resume_native=(CinematicNative)0x90134f560ULL;
 static const unsigned char init_head[]={0x55,0x48,0x89,0xe5,0x41,0x57,0x41,0x56,0x41,0x55,0x41,0x54,0x53,0x48,0x81,0xec,0xb8,0,0,0};
 static const unsigned char lookup_head[]={0x55,0x48,0x89,0xe5,0x41,0x57,0x41,0x56,0x53,0x50,0x48,0x8d,0x9f,0x80,0,0};
 static const unsigned char convert_head[]={0x55,0x48,0x89,0xe5,0x41,0x57,0x41,0x56,0x41,0x54,0x53,0x48,0x83,0xec,0x10};
 if(!read_memory(0x90108ae50ULL,bytes,sizeof(init_head))||memcmp(bytes,init_head,sizeof(init_head))||!read_memory(0x9014e0df0ULL,bytes,sizeof(lookup_head))||memcmp(bytes,lookup_head,sizeof(lookup_head))||!read_memory(0x903f122c0ULL,bytes,sizeof(convert_head))||memcmp(bytes,convert_head,sizeof(convert_head)))return 0;
 seek_native=(Seek)0x905ac4610ULL;
 static const unsigned char create_head[]={0x55,0x48,0x89,0xe5,0x41,0x57,0x41,0x56,0x41,0x55,0x41,0x54,0x53,0x48,0x81,0xec,0x98,0,0,0};
 ExactHook hooks[]={{(void*)0x905ac5ed0ULL,(void*)update,(void**)&original,head,sizeof(head)},{(void*)0x9013565d0ULL,(void*)cinematic_update,(void**)&cinematic_original,head,sizeof(head)},{(void*)0x90108ae50ULL,(void*)recording_init,(void**)&recording_init_original,init_head,sizeof(init_head)},{(void*)0x905619650ULL,(void*)post_create,(void**)&post_create_original,create_head,sizeof(create_head)}};const char *error=NULL;
 if(!install_exact_hooks(hooks,4,&error))return 0;
 installed=1;return 1;
#endif
}
void replay_poll(int focus,int photo_active){AcquireSRWLockExclusive(&guard);enabled=focus&&photo_active;poll_tick=REPLAY_NOW();if(!enabled){paused=0;pending=0;range_request=0;}ReleaseSRWLockExclusive(&guard);}
static int recent(void){return valid&&REPLAY_NOW()-last_tick<250;}
void replay_snapshot(ReplaySnapshot *out){
 memset(out,0,sizeof(*out));AcquireSRWLockShared(&guard);
 if(recent()&&isfinite(current)&&clip_high>clip_low&&clip_high-clip_low<=14400){out->available=1;out->paused=paused;out->extended=extended;out->range_failed=range_failed;out->position_ms=(int)(fmax(0,fmin(current-clip_low,clip_high-clip_low))*1000+.5);out->duration_ms=(int)((clip_high-clip_low)*1000+.5);out->coverage_known=coverage_known;out->coverage_ms=coverage_ms;out->coverage_pending=!coverage_known&&coverage_attempted<5;}
 ReleaseSRWLockShared(&guard);
}
void replay_request_coverage(void){AcquireSRWLockExclusive(&guard);if(enabled&&!coverage_known&&coverage_attempted<5)coverage_requested=1;ReleaseSRWLockExclusive(&guard);}
int replay_toggle_pause(void){AcquireSRWLockExclusive(&guard);int ok=enabled&&recent();if(ok){paused=!paused;pending=0;pause_owner=last_owner;pause_low=clip_low;pause_high=clip_high;}ReleaseSRWLockExclusive(&guard);return ok;}
int replay_step_ms(int direction,int milliseconds){AcquireSRWLockExclusive(&guard);int ok=enabled&&recent()&&(direction==-1||direction==1)&&milliseconds>=1&&milliseconds<=1000;if(ok){if(!paused){paused=1;pause_owner=last_owner;pause_low=clip_low;pause_high=clip_high;}pending=fmax(-1,fmin(1,pending+(milliseconds/1000.)*direction));}ReleaseSRWLockExclusive(&guard);return ok;}

int replay_step(int direction){return replay_step_ms(direction,100);}

int replay_toggle_range(void){AcquireSRWLockExclusive(&guard);int ok=enabled&&recent();if(ok){range_request=1;pending=0;}ReleaseSRWLockExclusive(&guard);return ok;}

int replay_set_speed(int index){if(index<0||index>5)return 0;AcquireSRWLockExclusive(&guard);speed_index=index;ReleaseSRWLockExclusive(&guard);return 1;}

void replay_offer_decisions(int on){InterlockedExchange(&post_requested,on!=0);}

unsigned replay_decision_events(void){return (unsigned)InterlockedCompareExchange(&post_events,0,0);}
