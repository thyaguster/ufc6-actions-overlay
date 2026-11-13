#define WIN32_LEAN_AND_MEAN
#include <windows.h>
static ULONGLONG fixture_now=1000;
#define REPLAY_NOW() fixture_now
#define REPLAY_NATIVE_TEST
#include "../src/replay_controls.c"
#undef NDEBUG
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
_Alignas(8) static unsigned char clock_object[0x128],entry[0x2c0],descriptor[0x90],tree[0x30],entity[0x40],entity_node[0x20],entity_data[0x30];
_Alignas(8) static unsigned char script_object[0x70],script_buffer[48],schema[48];
static unsigned char *manager;static unsigned native_pauses,native_resumes,updates,seeks;static double cinematic_time;
static char script_name[]="Replay_IsPaused";
static void q(void *p,unsigned off,uintptr_t v){memcpy((char*)p+off,&v,8);}
static void d(void *p,unsigned off,unsigned v){memcpy((char*)p+off,&v,4);}
static void db(void *p,unsigned off,double v){memcpy((char*)p+off,&v,8);}
static double get_time(void){double v;memcpy(&v,clock_object+0xe0,8);return v;}
static void __attribute__((sysv_abi)) clock_update(uintptr_t p,float dt){assert(p==(uintptr_t)clock_object);db(clock_object,0xe0,get_time()+dt);}
static void __attribute__((sysv_abi)) seek(uintptr_t p,double t){assert(p==(uintptr_t)clock_object);assert(manager[0x1834]==1);db(clock_object,0xe0,t);++seeks;}
static void __attribute__((sysv_abi)) full_pause(uintptr_t p,unsigned force){assert(p==(uintptr_t)manager&&force==1);assert(!manager[0x1834]&&!entry[0x2a0]);assert(*(LONG*)(script_buffer+32)==1);manager[0x1834]=entry[0x2a0]=1;++native_pauses;}
static void __attribute__((sysv_abi)) full_resume(uintptr_t p,unsigned force){assert(p==(uintptr_t)manager&&force==1);assert(manager[0x1834]&&entry[0x2a0]);manager[0x1834]=entry[0x2a0]=0;++native_resumes;}
static void __attribute__((sysv_abi)) cinematic_tick(uintptr_t p,float dt){assert(p==(uintptr_t)manager);if(!manager[0x1834])cinematic_time+=dt;++updates;ReplaySnapshot out;replay_snapshot(&out);}
static void seed(void){
 memset(manager,0,0x62400);memset(&cinematic,0,sizeof(cinematic));memset(clock_object,0,sizeof(clock_object));memset(script_object,0,sizeof(script_object));memset(script_buffer,0,sizeof(script_buffer));memset(schema,0,sizeof(schema));memset(entry,0,sizeof(entry));
 q(manager,0,0x9098ec188ULL);d(manager,0x1868,1234);q(manager,0x623b8,(uintptr_t)tree);q(manager,0x3d0,(uintptr_t)entity_node);q(entity_node,0,(uintptr_t)manager+0x3d0);q(entity_node,0x10,(uintptr_t)entity);
 q(tree,0,0);q(tree,8,0);d(tree,0x20,1234);q(tree,0x28,(uintptr_t)entry);q(entry,0,(uintptr_t)descriptor);d(descriptor,0x84,1234);q(entity,0,0x9098ec550ULL);q(entity,0x30,0x12345678);q(entity,0x38,(uintptr_t)entity_data);d(entity_data,0x28,1234);*(uintptr_t*)0x90a1366a8ULL=(uintptr_t)manager;
 q(script_object,0,0x909c227f0ULL);q(script_object,8,(uintptr_t)script_name);q(script_object,0x20,(uintptr_t)schema);q(script_object,0x28,0x12345000);q(script_object,0x50,0xabcdef00);d(script_object,0x58,4);d(script_object,0x5c,4);q(script_object,0x60,(uintptr_t)script_buffer);q(script_buffer,0,0x909927520ULL);q(script_buffer,8,0x12345000);q(script_buffer,16,0xabcdef00);unsigned short off=32;memcpy(script_buffer+28,&off,2);*(uintptr_t*)0x90b46beb0ULL=(uintptr_t)script_object;
 q(clock_object,0,0x909c247c0ULL);d(clock_object,0xb0,3);d(clock_object,0xb4,3);db(clock_object,0xd0,.017);db(clock_object,0xd8,131);db(clock_object,0xe0,128);db(clock_object,0xf8,127);db(clock_object,0x100,129);
 fixture_now=1000;extended=range_request=range_failed=0;paused=valid=enabled=0;pending=0;original=clock_update;seek_native=seek;cinematic_original=cinematic_tick;cinematic_pause_native=full_pause;cinematic_resume_native=full_resume;native_pauses=native_resumes=updates=seeks=0;cinematic_time=0;
 update((uintptr_t)clock_object,.02f);replay_poll(1,1);assert(replay_toggle_pause());
}
static void tick(void){fixture_now+=20;replay_poll(1,1);update((uintptr_t)clock_object,.02f);cinematic_update((uintptr_t)manager,.02f);}
int main(void){
 void *a=VirtualAlloc((void*)0x90a130000ULL,65536,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE),*b=VirtualAlloc((void*)0x90b460000ULL,65536,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);assert(a==(void*)0x90a130000ULL&&b==(void*)0x90b460000ULL);manager=calloc(1,0x62400);assert(manager);
 seed();double held=get_time();for(unsigned i=0;i<3000;++i)tick();assert(native_pauses==1&&!native_resumes&&cinematic_time==0&&get_time()==held&&updates==3000);
 assert(replay_step(-1));tick();assert(seeks==1&&fabs(get_time()-(held-.1))<1e-8&&native_pauses==1);assert(replay_step(1));tick();assert(seeks==2&&fabs(get_time()-held)<1e-8);
 assert(replay_toggle_pause());tick();assert(native_resumes==1&&!manager[0x1834]&&!entry[0x2a0]&&*(LONG*)(script_buffer+32)==0&&cinematic_time>0&&get_time()>held);
 for(int failure=0;failure<4;++failure){seed();tick();assert(native_pauses==1);if(failure==0)replay_poll(0,1);if(failure==1)replay_poll(1,0);if(failure==2)fixture_now+=1000;if(failure==3)db(clock_object,0x100,128.5);cinematic_update((uintptr_t)manager,.02f);assert(native_resumes==1&&*(LONG*)(script_buffer+32)==0);}
 seed();manager[0x1834]=entry[0x2a0]=1;tick();replay_poll(0,0);cinematic_update((uintptr_t)manager,.02f);assert(!native_pauses&&!native_resumes&&manager[0x1834]&&entry[0x2a0]);
 seed();*(LONG*)(script_buffer+32)=1;tick();replay_poll(0,0);cinematic_update((uintptr_t)manager,.02f);assert(!native_pauses&&!native_resumes&&*(LONG*)(script_buffer+32)==1);
 seed();tick();d(manager,0x1868,5678);cinematic_update((uintptr_t)manager,.02f);assert(!native_resumes&&!cinematic.entity&&*(LONG*)(script_buffer+32)==0);
 seed();tick();q(entity,0x30,0x87654321);replay_poll(0,0);cinematic_update((uintptr_t)manager,.02f);assert(!native_resumes&&!cinematic.entity);
 seed();tick();*(LONG*)(script_buffer+32)=2;replay_poll(0,0);cinematic_update((uintptr_t)manager,.02f);assert(native_resumes==1&&*(LONG*)(script_buffer+32)==2);
 seed();tick();extended=1;range_owner=(uintptr_t)clock_object;saved_low=127;saved_high=129;saved_time=get_time();extended_low=127;extended_high=129;
 assert(replay_toggle_pause());tick();assert(!paused&&manager[0x1834]&&native_pauses==1&&native_resumes==0);replay_poll(0,0);update((uintptr_t)clock_object,.02f);cinematic_update((uintptr_t)manager,.02f);assert(!extended&&!manager[0x1834]&&native_resumes==1);
 free(manager);VirtualFree(a,0,MEM_RELEASE);VirtualFree(b,0,MEM_RELEASE);
 puts("PASS: coupled replay/cinematic sixty-second hold, stepping while cinematic paused, selected-time resume, focus/photo-exit/heartbeat/clip release, original pause preservation, changed-context protection and foreign script values.");
}
