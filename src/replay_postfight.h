/* Post-fight fallback: reuse native clips, never fabricate a result or handle.
   Included by replay_controls.c; callbacks run on the native recorder thread. */
#ifndef UFC6_REPLAY_POSTFIGHT_H
#define UFC6_REPLAY_POSTFIGHT_H
#include "replay_postfight_policy.h"
typedef int (__attribute__((sysv_abi)) *ReplayCreate)(uintptr_t,uintptr_t,uintptr_t,uintptr_t,float,float,float);
static ReplayCreate post_create_original;
static PVOID volatile post_manager;
static volatile LONG post_requested,post_events;
static int __attribute__((sysv_abi)) post_create(uintptr_t manager,uintptr_t context,uintptr_t begin,uintptr_t end,float before,float after,float score){
 int result=post_create_original(manager,context,begin,end,before,after,score);
 InterlockedExchangePointer(&post_manager,(void*)manager);return result;
}
typedef struct {uintptr_t slot;const char *name;} PostSpec;
static const PostSpec post_specs[]={
 {0x90b4455c0ULL,"Replay_Handle1"},{0x90b4455e8ULL,"Replay_Handle2"},{0x90b445610ULL,"Replay_Handle3"},{0x90b445638ULL,"Replay_Handle4"},{0x90b445660ULL,"Replay_Handle5"},
 {0x90b46bf28ULL,"Replay_replays_to_play"},{0x90b4490d0ULL,"Pres_ViewReplayAvailable"},
 {0x90b444c60ULL,"Global_MaxRounds"},{0x90b444f08ULL,"Global_RoundNum"},{0x90b444c10ULL,"Global_MatchState"},
 {0x90b444be8ULL,"Global_MatchResultType"},{0x90b447fc8ULL,"Global_Fight_FightType"},{0x90b4449e0ULL,"Global_Fight_GameMode"},
 {0x90b46b5f0ULL,"Replay_Enable_KOReplay"}
};
typedef struct {uintptr_t address,world;int value;} PostField;
static int post_field(unsigned index,PostField *out){
 uintptr_t object=0,vt=0,name=0,buffer=0;unsigned short offset=0;unsigned size=0;char text[64]={0};
 const PostSpec *s=&post_specs[index];size_t len=strlen(s->name)+1;
 if(!read_memory(s->slot,&object,8)||!read_memory(object,&vt,8)||vt!=0x909c227f0ULL||
 !read_memory(object+8,&name,8)||!read_memory(name,text,len)||memcmp(text,s->name,len)||
 !read_memory(object+0x28,&out->world,8)||!out->world||!read_memory(object+0x58,&size,4)||size!=4||
 !read_memory(object+0x60,&buffer,8)||!read_memory(buffer+28,&offset,2)||offset!=32)return 0;
 out->address=buffer+offset;return read_memory(out->address,&out->value,4);
}
static int post_fields(PostField out[14]){
 for(unsigned i=0;i<14;++i)if(!post_field(i,&out[i])||(i&&out[i].world!=out[0].world))return 0;
 return 1;
}
static int post_native_clips(uintptr_t manager,uintptr_t recorder,int ids[48]){
 uintptr_t bound=0,begin=0,end=0,buckets=0;unsigned capacity=0;unsigned char active=0;double range[2];
 if(!manager||!read_memory(manager+0x30,&bound,8)||bound!=recorder||
 !read_memory(manager+0x23c,&active,1)||active!=1||!read_memory(manager+0xf8,&begin,8)||!read_memory(manager+0x100,&end,8)||
 end<begin||end-begin>48*16||(end-begin)%16||!read_memory(manager+0x90,&buckets,8)||
 !read_memory(manager+0x98,&capacity,4)||!capacity||capacity>4096||!read_memory(recorder+0xd0,range,16)||
 !isfinite(range[0])||!isfinite(range[1])||range[0]>=range[1])return 0;
 int n=0;
 for(uintptr_t p=begin;p<end;p+=16){
  uintptr_t clip=0,node=0;int id=0;double bounds[2];
  if(!read_memory(p,&clip,8)||!read_memory(clip,&id,4)||id<=0||!read_memory(clip+0x60,bounds,16)||
  !isfinite(bounds[0])||!isfinite(bounds[1])||bounds[0]<range[0]||bounds[0]>=bounds[1]||bounds[1]>range[1]||
  !read_memory(buckets+((unsigned)id%capacity)*8,&node,8))continue;
  for(unsigned hops=0;node&&hops<64;++hops){
   int key=0;uintptr_t value=0,next=0;
   if(!read_memory(node,&key,4)||!read_memory(node+8,&value,8)||!read_memory(node+0x18,&next,8))break;
   if(key==id){if(value==clip)ids[n++]=id;break;}node=next;
  }
 }
 uintptr_t check[2];if(!read_memory(manager+0xf8,check,16)||check[0]!=begin||check[1]!=end)return 0;
 return n;
}
static struct {uintptr_t world,recorder,addresses[7];int owned,seen_fight,original[7],applied[7],result,rounds;ULONGLONG tick;} post_state;
static int post_candidate[14],post_stable;
static void post_update(uintptr_t recorder){
 ULONGLONG now=REPLAY_NOW();if(now-post_state.tick<20)return;post_state.tick=now;
 if(!InterlockedCompareExchange(&post_requested,0,0)&&!post_state.owned){post_state.seen_fight=0;return;}
 uintptr_t vt=0;int modes[2];PostField f[14];
 if(!read_memory(recorder,&vt,8)||vt!=0x909c247c0ULL||!read_memory(recorder+0xb0,modes,8)||!post_fields(f))return;
 int rounds=f[7].value,round=f[8].value,state=f[9].value,result=f[10].value;
 int offline=f[11].value==0&&f[12].value==22;
 if(post_state.owned){
  int same=f[0].world==post_state.world&&recorder==post_state.recorder&&round==post_state.rounds-1&&result==post_state.result;
  /* Keep slots through repeated View Replay entry/exit. Never restore old values
     over a new fight's reset; restoration requires the same finished fight. */
  if(!same||!offline||state!=3){
   if(same&&state!=1&&modes[0]!=3&&modes[1]!=3){
    for(int i=0;i<7;++i)if(f[i].address==post_state.addresses[i]&&f[i].value==post_state.applied[i])memcpy((void*)f[i].address,&post_state.original[i],4);
   }else if(same&&(modes[0]==3||modes[1]==3))return;
   post_state.owned=0;post_state.seen_fight=0;
  }
  if(post_state.owned){
   int intact=1;for(int i=0;i<7;++i)if(f[i].address!=post_state.addresses[i]||f[i].value!=post_state.applied[i])intact=0;
   if(intact||modes[0]!=2||modes[1]!=2)return;
   /* The native collector can finish after our first application. Reconcile
      only a stable completed selection in the same finished fight. */
  }else {post_stable=0;return;}
 }
 if(offline&&state==1&&round==0){post_state.seen_fight=1;post_state.world=f[0].world;post_state.recorder=recorder;}
 if(!post_state.seen_fight||f[0].world!=post_state.world||recorder!=post_state.recorder||!offline||state!=3||
 round!=rounds-1||modes[0]!=2||modes[1]!=2||f[13].value){post_stable=0;return;}
 int positives=0;for(int i=0;i<5;++i){if(f[i].value==0||f[i].value< -1){post_stable=0;return;}positives+=f[i].value>0;}
 if(f[5].value!=positives){post_stable=0;return;}
 int stable=1;for(int i=0;i<14;++i){if(post_candidate[i]!=f[i].value)stable=0;post_candidate[i]=f[i].value;}
 post_stable=stable?(post_stable<3?post_stable+1:3):0;if(post_stable<3)return;
 int handles[5],ids[48],target[5],count=0;
 for(int i=0;i<5;++i)handles[i]=f[i].value;
 uintptr_t manager=(uintptr_t)InterlockedCompareExchangePointer(&post_manager,NULL,NULL);
 int n=post_native_clips(manager,recorder,ids);
 if(!post_plan(handles,rounds,result,ids,n,target,&count))return;
 /* Validate again immediately before scalar writes; no native allocations,
    manager changes, winner changes or recorder-mode changes are involved. */
 PostField check[14];if(!post_fields(check))return;
 for(int i=0;i<14;++i)if(check[i].address!=f[i].address||check[i].value!=f[i].value||check[i].world!=f[i].world)return;
 int changed=f[5].value!=count||f[6].value!=1;
 for(int i=0;i<5;++i)changed|=handles[i]!=target[i];
 if(!changed)return;
 for(int i=0;i<7;++i){if(!post_state.owned||post_state.addresses[i]!=f[i].address||f[i].value!=post_state.applied[i])post_state.original[i]=f[i].value;post_state.addresses[i]=f[i].address;post_state.applied[i]=i<5?target[i]:i==5?count:1;}
 post_state.result=result;post_state.rounds=rounds;post_state.owned=1;
 for(int i=0;i<7;++i)memcpy((void*)f[i].address,&post_state.applied[i],4);
 InterlockedIncrement(&post_events);
}
#endif
