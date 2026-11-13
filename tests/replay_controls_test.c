#define WIN32_LEAN_AND_MEAN
#include <windows.h>
static ULONGLONG fixture_now=1000;
#define REPLAY_NOW() fixture_now
#define REPLAY_NATIVE_TEST
#include "../src/replay_controls.c"
#undef NDEBUG
#include <assert.h>
#include <stdio.h>
static unsigned char object[0x128],other[0x128];static unsigned seeks,loops;static float seen;
static void set_double(unsigned char *p,int off,double v){memcpy(p+off,&v,8);}
static double get_double(unsigned char *p,int off){double v;memcpy(&v,p+off,8);return v;}
static void __attribute__((sysv_abi)) fake_original(uintptr_t p,float dt){unsigned char *o=(void*)p;seen=dt;double t=get_double(o,0xe0)+dt;if(t>=129){t=127;++loops;}set_double(o,0xe0,t);set_double(o,0xe8,t);}
static void __attribute__((sysv_abi)) fake_seek(uintptr_t p,double t){assert(p==(uintptr_t)object);assert(t>=127&&t<129);set_double(object,0xe0,t);++seeks;}
static void seed(void){memset(object,0,sizeof(object));uintptr_t vt=0x909c247c0ULL;memcpy(object,&vt,8);unsigned mode=3;memcpy(object+0xb0,&mode,4);memcpy(object+0xb4,&mode,4);set_double(object,0xd0,.017);set_double(object,0xd8,131);set_double(object,0xe0,128);set_double(object,0xe8,128);set_double(object,0xf8,127);set_double(object,0x100,129);paused=valid=enabled=0;pending=0;fixture_now=1000;original=fake_original;seek_native=fake_seek;loops=seeks=0;}
int main(void){ReplaySnapshot r;seed();update((uintptr_t)object,.02f);replay_snapshot(&r);assert(r.available&&!r.paused&&r.position_ms==1020&&r.duration_ms==2000);assert(!replay_toggle_pause());
 replay_poll(1,1);assert(replay_toggle_pause());double held=get_double(object,0xe0);
 /* A normal pause persists across many natural loop lengths while UI heartbeat lives. */
 for(unsigned i=0;i<3000;++i){fixture_now+=20;replay_poll(1,1);update((uintptr_t)object,.02f);assert(seen==0&&get_double(object,0xe0)==held&&!loops);}
 assert(replay_step(-1));update((uintptr_t)object,.02f);assert(fabs(get_double(object,0xe0)-(held-.1))<1e-8&&seeks==1);assert(replay_step(1));update((uintptr_t)object,.02f);assert(fabs(get_double(object,0xe0)-held)<1e-8&&seeks==2);
 assert(replay_toggle_pause());update((uintptr_t)object,.02f);assert(seen==.02f&&fabs(get_double(object,0xe0)-(held+.02))<1e-8);
 assert(replay_step(-1));update((uintptr_t)object,.02f);assert(paused&&seen==0);replay_poll(1,0);update((uintptr_t)object,.02f);assert(!paused&&seen==.02f);
 for(int failure=0;failure<6;++failure){seed();update((uintptr_t)object,.02f);replay_poll(1,1);assert(replay_toggle_pause());uintptr_t owner=(uintptr_t)object;float dt=.02f;
  if(failure==0)replay_poll(0,1);if(failure==1)fixture_now+=1000;if(failure==2)set_double(object,0x100,128.5);if(failure==3){unsigned mode=2;memcpy(object+0xb0,&mode,4);}if(failure==4){memcpy(other,object,sizeof(other));owner=(uintptr_t)other;}if(failure==5)dt=.5f;
  update(owner,dt);assert(!paused&&seen==dt);
 }
 seed();update((uintptr_t)object,.02f);fixture_now+=250;replay_snapshot(&r);assert(!r.available);replay_poll(1,1);assert(!replay_toggle_pause()&&!replay_step(1));
 seed();update((uintptr_t)object,.02f);replay_poll(1,1);for(unsigned i=0;i<40;++i){replay_step(1);update((uintptr_t)object,.02f);}assert(get_double(object,0xe0)<129&&!loops);
 seed();update((uintptr_t)object,.02f);replay_poll(1,1);double precise=get_double(object,0xe0);assert(!replay_step_ms(1,0)&&!replay_step_ms(1,1001));assert(replay_step_ms(-1,10));update((uintptr_t)object,.02f);assert(fabs(get_double(object,0xe0)-(precise-.01))<1e-8);assert(replay_step_ms(1,25));update((uintptr_t)object,.02f);assert(fabs(get_double(object,0xe0)-(precise+.015))<1e-8);
 puts("PASS: native replay backend, sixty-second pause without loops, bidirectional native seek, resume from selected time, automatic step pause, focus/photo-exit/heartbeat/owner/clip/mode/delta guards and end exclusion.");}
