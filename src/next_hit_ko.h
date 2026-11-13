/* One-shot, opponent-only health-event control. All access is serialized by
   live_cheats' binding_guard. Writes use checked, aligned compare/exchange. */
#ifndef UFC6_NEXT_HIT_KO_H
#define UFC6_NEXT_HIT_KO_H
#include <math.h>
typedef struct {uintptr_t owner,begin,end,record[4],field[4][2];int player;uint64_t seen;int valid;} KoBinding;
typedef struct {KoBinding candidate,owned;int requested,armed,result,cancel;uint64_t deadline,last_probe;} KoControl;
static KoControl ko;
static const char *ko_names[]={"Fighter_ForceHealthEventOnTrigger","Fighter_CurrentHealthEvent","Fighter_ControllerInputMask","Fighter_CurrentHealth_Head"};
static int ko_read(uintptr_t a,void *v,size_t n){SIZE_T got=0;return a>=0x10000&&ReadProcessMemory(GetCurrentProcess(),(void*)a,v,n,&got)&&got==n;}
static int ko_record(uintptr_t r,int kind,uintptr_t fields[2]){
 uintptr_t obj=0,name=0,start=0,end=0;int32_t first=0,type=0;uint64_t stride=0;char label[64]={0};
 if(!ko_read(r+16,&obj,8)||!ko_read(obj+8,&name,8)||!ko_read(name,label,strlen(ko_names[kind])+1)||strcmp(label,ko_names[kind])||
 !ko_read(r+32,&first,4)||first!=-1||!ko_read(r+40,&stride,8)||stride!=4||!ko_read(r+52,&type,4)||type!=(kind==3?10:6)||
 !ko_read(r+56,&start,8)||!ko_read(r+64,&end,8)||end<start||end-start!=32)return 0;
 unsigned mask=0;for(int n=0;n<2;++n){uintptr_t a=0;int32_t idx=-1;
 if(!ko_read(start+n*16,&a,8)||!ko_read(start+n*16+8,&idx,4)||idx<0||idx>1||(mask&(1u<<idx))||!a||(a&3))return 0;
 fields[idx]=a;mask|=1u<<idx;}
 return mask==3;
}
static int ko_identity(const KoBinding *b){
 uintptr_t begin=0,end=0;if(!b->valid||!ko_read(b->owner+0x88,&begin,8)||!ko_read(b->owner+0x90,&end,8)||begin!=b->begin||end!=b->end)return 0;
 for(int n=0;n<4;++n){uintptr_t f[2];if(b->record[n]<begin||b->record[n]>=end||(b->record[n]-begin)%104||!ko_record(b->record[n],n,f)||memcmp(f,b->field[n],sizeof(f)))return 0;}
 return 1;
}
static int ko_cas(uintptr_t a,LONG expected,LONG value){MEMORY_BASIC_INFORMATION m;
 if((a&3)||!VirtualQuery((void*)a,&m,sizeof(m))||m.State!=MEM_COMMIT||(m.Protect&PAGE_GUARD)||!(m.Protect&(PAGE_READWRITE|PAGE_WRITECOPY)))return 0;
 return InterlockedCompareExchange((volatile LONG*)a,value,expected)==expected;
}
static int ko_values(const KoBinding *b){int32_t masks[2],events[2];float health[2];
 for(int n=0;n<2;++n)if(!ko_read(b->field[2][n],&masks[n],4)||masks[n]<0||masks[n]>65535||!ko_read(b->field[1][n],&events[n],4)||events[n]<0||events[n]>=7||!ko_read(b->field[3][n],&health[n],4)||!isfinite(health[n])||health[n]<=0)return 0;
 return !!(masks[0]&1)!=!!(masks[1]&1)&&((masks[0]&1)?0:1)==b->player;
}
static void ko_release(int result){
 if(ko.armed&&ko_identity(&ko.owned))ko_cas(ko.owned.field[0][1-ko.owned.player],7,0);
 ko.armed=ko.requested=0;ko.result=result;memset(&ko.owned,0,sizeof(ko.owned));
}
static void ko_observe(uintptr_t owner,int player,uint64_t now){
 if(player<1||player>2)return;
 if(now-ko.last_probe<(uint64_t)((ko.armed||ko.requested)?16:250)&&ko.candidate.valid)return;ko.last_probe=now;
 if(ko.armed&&owner!=ko.owned.owner){ko_release(3);}
 KoBinding b=ko.candidate;
 if(!b.valid||b.owner!=owner||!ko_identity(&b)){
  memset(&b,0,sizeof(b));b.owner=owner;
  if(!ko_read(owner+0x88,&b.begin,8)||!ko_read(owner+0x90,&b.end,8)||!b.begin||b.end<b.begin||(b.end-b.begin)%104||(b.end-b.begin)/104>16384)return;
  unsigned mask=0;for(uintptr_t r=b.begin;r<b.end;r+=104){uintptr_t obj=0,name=0;char label[64]={0};
   if(!ko_read(r+16,&obj,8)||!ko_read(obj+8,&name,8)||!ko_read(name,label,63))continue;
   for(int n=0;n<4;++n)if(!strcmp(label,ko_names[n])&&ko_record(r,n,b.field[n])){b.record[n]=r;mask|=1u<<n;}
  }b.valid=mask==15;if(!b.valid)return;
 }
 b.player=player==1?0:1;b.seen=now;ko.candidate=b;
 if(ko.armed){int32_t event=0,current=0;int target=1-ko.owned.player;
  if(!ko_identity(&ko.owned)||b.player!=ko.owned.player){ko_release(3);return;}
  if(!ko_read(ko.owned.field[1][target],&event,4)||!ko_read(ko.owned.field[0][target],&current,4)){ko_release(3);return;}
  if(event==7){ko_release(2);return;}if(current!=7||now>=ko.deadline||!ko_values(&b)){ko_release(3);return;}
 }
 if(ko.requested&&!ko.armed){
  if(now>=ko.deadline||!ko_values(&b)||!ko_cas(b.field[0][1-b.player],0,7)){ko_release(3);return;}
  ko.owned=b;ko.armed=1;ko.result=1;
 }
}
#endif
