#define WIN32_LEAN_AND_MEAN
#include <windows.h>
static ULONGLONG fixture_now=1000;
#define REPLAY_NOW() fixture_now
#define REPLAY_NATIVE_TEST
#include "../src/replay_controls.c"
#undef NDEBUG
#include <assert.h>
#include <stdio.h>
static unsigned char obj[0x128];static int coverage_ok=1;static float delivered;
static void put(unsigned off,double d){memcpy(obj+off,&d,8);}
static double get(unsigned off){double d;memcpy(&d,obj+off,8);return d;}
static void __attribute__((sysv_abi)) forward(uintptr_t p,float dt){assert(p==(uintptr_t)obj);delivered=dt;put(0xe0,get(0xe0)+dt);}
static void __attribute__((sysv_abi)) rate_forward(uintptr_t p,float dt){assert(p==(uintptr_t)obj);float speed;memcpy(&speed,obj+0x10c,4);delivered=dt;put(0xe0,get(0xe0)+dt*speed);}
static void __attribute__((sysv_abi)) seek(uintptr_t p,double t){assert(p==(uintptr_t)obj);assert(t>=.05&&t<300);put(0xe0,t);}
static int coverage(double t,const double*r,ReplayRange*out){(void)t;(void)r;*out=(ReplayRange){.05,299};return coverage_ok;}
static void seed(void){memset(obj,0,sizeof(obj));uintptr_t vt=0x909c247c0ULL;unsigned mode=3;float speed=1;memcpy(obj,&vt,8);memcpy(obj+0xb0,&mode,4);memcpy(obj+0xb4,&mode,4);memcpy(obj+0x10c,&speed,4);put(0xd0,.017);put(0xd8,300);put(0xe0,295);put(0xf8,294);put(0x100,296);original=forward;seek_native=seek;coverage_query=coverage;extended=range_request=range_failed=paused=enabled=valid=0;pending=0;coverage_ok=1;fixture_now=1000;update((uintptr_t)obj,.01f);replay_poll(1,1);}
int main(void){ReplayRange items[]={{10,11},{0,1},{1,2},{7,8}},found;int n=range_merge(items,4);assert(n==3&&range_containing(items,n,.5,&found)&&found.low==0&&found.high==2);assert(!range_containing(items,n,5,&found));
 seed();double saved=get(0xe0);assert(replay_toggle_range());update((uintptr_t)obj,.02f);assert(extended&&paused&&get(0xf8)==.05&&get(0xe0)==.05&&get(0x100)==299);ReplaySnapshot snap;replay_snapshot(&snap);assert(snap.extended&&snap.paused&&!snap.range_failed);assert(replay_toggle_pause());update((uintptr_t)obj,.02f);assert(extended&&!paused&&get(0xe0)>.05);assert(replay_toggle_range());update((uintptr_t)obj,.02f);assert(!extended&&!paused&&get(0xf8)==294&&get(0x100)==296&&fabs(get(0xe0)-(saved+.02))<1e-8);
 for(int cancel=0;cancel<3;++cancel){seed();saved=get(0xe0);assert(replay_toggle_range());update((uintptr_t)obj,.02f);if(cancel==0)replay_poll(0,1);if(cancel==1)replay_poll(1,0);if(cancel==2)fixture_now+=1000;update((uintptr_t)obj,.02f);assert(!extended&&get(0xf8)==294&&get(0x100)==296&&fabs(get(0xe0)-(saved+.02))<1e-8);}
 seed();coverage_ok=0;replay_toggle_range();update((uintptr_t)obj,.02f);assert(!extended&&range_failed&&get(0xf8)==294);
 seed();replay_toggle_range();update((uintptr_t)obj,.02f);put(0xf8,290);put(0x100,297);update((uintptr_t)obj,.02f);assert(!extended&&get(0xf8)==290&&get(0x100)==297);
 seed();replay_toggle_range();update((uintptr_t)obj,.02f);replay_toggle_pause();put(0xe0,298.995);update((uintptr_t)obj,.02f);assert(extended&&paused&&delivered==0&&get(0xe0)==298.999);
 seed();replay_toggle_range();update((uintptr_t)obj,0);assert(paused);fixture_now+=20;update((uintptr_t)obj,0);assert(get(0xe0)==.05);
 assert(replay_toggle_pause());fixture_now+=20;update((uintptr_t)obj,0);assert(!paused&&fabs(get(0xe0)-.07)<.000001&&delivered>.019f);
 double held=get(0xe0);update((uintptr_t)obj,0);assert(get(0xe0)==held&&delivered==0);
 fixture_now+=300;replay_poll(1,1);update((uintptr_t)obj,0);assert(get(0xe0)==held&&delivered==0);
 fixture_now+=20;update((uintptr_t)obj,0);assert(get(0xe0)>held);
 assert(replay_toggle_pause());held=get(0xe0);fixture_now+=20;update((uintptr_t)obj,0);assert(get(0xe0)==held);
 seed();fixture_now+=20;update((uintptr_t)obj,0);assert(delivered==0); /* Native finish playback unchanged. */
 seed();original=rate_forward;float slow=.2f;memcpy(obj+0x10c,&slow,4);assert(replay_set_speed(4));held=get(0xe0);fixture_now+=20;update((uintptr_t)obj,0);assert(fabs(get(0xe0)-held-.02)<.000001);
 assert(replay_set_speed(2));held=get(0xe0);fixture_now+=20;update((uintptr_t)obj,.01f);assert(fabs(get(0xe0)-held-.005)<.000001);
 assert(replay_toggle_pause());held=get(0xe0);fixture_now+=20;update((uintptr_t)obj,.02f);assert(get(0xe0)==held);
 assert(replay_toggle_pause());float zero=0;memcpy(obj+0x10c,&zero,4);fixture_now+=20;update((uintptr_t)obj,.02f);assert(get(0xe0)==held);
 assert(!replay_set_speed(-1)&&!replay_set_speed(6));replay_set_speed(0);memcpy(obj+0x10c,&slow,4);held=get(0xe0);fixture_now+=20;update((uintptr_t)obj,.02f);assert(fabs(get(0xe0)-held-.004)<.000001);
 puts("PASS: continuous interval selection, range entry, resume, exact finish restoration, focus/exit/heartbeat cancellation, coverage rejection, foreign bounds and end hold.");return 0;}
