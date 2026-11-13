/* Native cinematic ownership is separate from the recorded replay clock.
   Only the cinematic timer's own thread calls the paired engine routines. */
#include "replay_script_pause.h"
typedef void (__attribute__((sysv_abi)) *CinematicNative)(uintptr_t,unsigned);
static Update cinematic_original;
static CinematicNative cinematic_pause_native,cinematic_resume_native;
static struct {uintptr_t manager,entity,world,data;unsigned id;PresentationPause script;ULONGLONG retry;} cinematic;
static uintptr_t cinematic_pointer(uintptr_t p){uintptr_t v=0;read_memory(p,&v,8);return v;}
static unsigned cinematic_uint(uintptr_t p){unsigned v=0;read_memory(p,&v,4);return v;}
static int cinematic_byte(uintptr_t p,unsigned char *v){return read_memory(p,v,1);}
static uintptr_t cinematic_entry(uintptr_t manager,unsigned id){
 uintptr_t node=cinematic_pointer(manager+0x623b8);
 for(unsigned i=0;i<64&&node;++i){unsigned key=cinematic_uint(node+0x20);if(key==id){uintptr_t p=cinematic_pointer(node+0x28),d=cinematic_pointer(p);return d&&cinematic_uint(d+0x84)==id?p:0;}node=cinematic_pointer(node+(key<id?0:8));}return 0;
}
static uintptr_t cinematic_entity(uintptr_t manager,unsigned id,uintptr_t expected){
 uintptr_t head=manager+0x3d0,node=cinematic_pointer(head);
 for(unsigned i=0;i<512&&node&&node!=head;++i){uintptr_t p=cinematic_pointer(node+0x10),data=cinematic_pointer(p+0x38);
  if((!expected||expected==p)&&cinematic_pointer(p)==0x9098ec550ULL&&cinematic_pointer(p+0x30)&&data&&cinematic_uint(data+0x28)==id)return p;
  uintptr_t next=cinematic_pointer(node);if(next==node)break;node=next;
 }return 0;
}
static void cinematic_restore(void){
 if(cinematic.entity){uintptr_t m=cinematic.manager;
  if(cinematic_pointer(m)==0x9098ec188ULL&&cinematic_uint(m+0x1868)==cinematic.id&&cinematic_entity(m,cinematic.id,cinematic.entity)&&cinematic_pointer(cinematic.entity+0x30)==cinematic.world&&cinematic_pointer(cinematic.entity+0x38)==cinematic.data)cinematic_resume_native(m,1);
 }
 flag_restore(&cinematic.script);cinematic.manager=cinematic.entity=cinematic.world=cinematic.data=0;cinematic.id=0;
}
static void __attribute__((sysv_abi)) cinematic_update(uintptr_t manager,float delta){
 ULONGLONG now=REPLAY_NOW();uintptr_t replay_owner=0;double low=0,high=0;
 AcquireSRWLockShared(&guard);int wanted=(paused||extended)&&enabled&&now-poll_tick<1000;replay_owner=pause_owner;low=pause_low;high=pause_high;ReleaseSRWLockShared(&guard);
 if((!wanted&&!cinematic.entity)||cinematic_pointer(0x90a1366a8ULL)!=manager){cinematic_original(manager,delta);return;}
 double time=0,range[2],clip[2];wanted=wanted&&capture(replay_owner,&time,range,clip)&&fabs(clip[0]-low)<.000001&&fabs(clip[1]-high)<.000001;
 unsigned id=cinematic_uint(manager+0x1868);int identity=cinematic_pointer(0x90a1366a8ULL)==manager&&cinematic_pointer(manager)==0x9098ec188ULL&&id;
 if(cinematic.entity&&(!wanted||!identity||manager!=cinematic.manager||id!=cinematic.id||!flag_identity(&cinematic.script)))cinematic_restore();
 if(wanted&&identity&&!cinematic.entity&&now>=cinematic.retry){
  unsigned char held=1,entry_held=1;uintptr_t entry=cinematic_entry(manager,id),entity=cinematic_entity(manager,id,0);
  if(entry&&entity&&cinematic_byte(manager+0x1834,&held)&&!held&&cinematic_byte(entry+0x2a0,&entry_held)&&!entry_held&&flag_acquire(&cinematic.script)){
   cinematic.manager=manager;cinematic.entity=entity;cinematic.id=id;cinematic.world=cinematic_pointer(entity+0x30);cinematic.data=cinematic_pointer(entity+0x38);cinematic_pause_native(manager,1);
  }else cinematic.retry=now+500;
 }
 cinematic_original(manager,delta);
}
