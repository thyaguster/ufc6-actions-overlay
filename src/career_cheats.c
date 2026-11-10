/* One-time career additions on the game's own selected-user lookup thread.
   No save-file edits, balance freezes, remote services or heap-value scans. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string.h>
#include "career_cheats.h"
#include "exact_hooks.h"
typedef void *(__attribute__((sysv_abi)) *UserFn)(uintptr_t*,uintptr_t,int64_t);
typedef uint32_t (__attribute__((sysv_abi)) *GetFn)(uintptr_t,uint32_t,uint32_t);
typedef void (__attribute__((sysv_abi)) *SetFn)(uintptr_t,uint32_t,uint32_t,uint32_t);
typedef void (__attribute__((sysv_abi)) *DirtyFn)(uintptr_t);
typedef struct {uintptr_t store,packed,address;uint32_t index,bits,shift,value,maximum,row;} CareerField;
static uintptr_t guest,active_root,collection;
static CareerField fields[2],expected[2],fitness,expected_fitness,training,expected_training;
static uint32_t training_reference,training_week,expected_training_reference,expected_training_week;
static UserFn original_user;
static GetFn game_get;
static SetFn game_set;
static DirtyFn game_dirty;
static SRWLOCK guard=SRWLOCK_INIT;
static CareerSnapshot snapshot;
static uint64_t queued;
static volatile LONG64 next_probe;
static uintptr_t cache_collection,cache_begin;
static size_t cached_slot[2]={SIZE_MAX,SIZE_MAX};
#ifdef CAREER_CHEATS_TEST
static uint64_t probe_count;
#endif
static const unsigned char user_signature[]={0x55,0x48,0x89,0xe5,0x41,0x57,0x41,0x56,0x41,0x55,0x41,0x54,0x53,0x48,0x83,0xec,0x28,0x48,0x8b,0x05,0xc8,0x6c,0x13,0x04};
static int rd(uintptr_t a,void *out,size_t n){SIZE_T got=0;return a>=0x10000&&ReadProcessMemory(GetCurrentProcess(),(void*)a,out,n,&got)&&got==n;}
static uintptr_t pointer(uintptr_t a){uintptr_t p=0;rd(a,&p,8);return p;}
static int u32(uintptr_t a,uint32_t *out){return rd(a,out,4);}
static int writable(uintptr_t a,size_t n){MEMORY_BASIC_INFORMATION m;return VirtualQuery((void*)a,&m,sizeof(m))&&m.State==MEM_COMMIT&&!(m.Protect&PAGE_GUARD)&&(m.Protect&(PAGE_READWRITE|PAGE_WRITECOPY))&&a+n>=a&&a+n<=(uintptr_t)m.BaseAddress+m.RegionSize;}
static int name_is(uintptr_t a,const char *name){
    unsigned char text[16];char full[80];size_t n=strlen(name)+1;
    if(n>sizeof(full)||!rd(a,text,16))return 0;
    if(text[15]&128){uintptr_t p=0;memcpy(&p,text,8);return rd(p,full,n)&&!memcmp(full,name,n);}
    return n<=16&&!memcmp(text,name,n);
}
static int schema(uintptr_t store,uintptr_t *begin,size_t count){
    if(pointer(store)!=guest+0x9bf7be8)return 0;
    uintptr_t s=pointer(store+8),b=pointer(s+0x30),e=pointer(s+0x38);
    if(!b||e<b||e-b!=count*80)return 0;*begin=b;return 1;
}
static int describe(uintptr_t store,int kind,CareerField *out){
    int fighter=kind==CAREER_MONEY||kind==CAREER_FITNESS;
    uintptr_t b=0;uint32_t index=kind==CAREER_MONEY?48:kind==CAREER_FITNESS?50:10;
    if(!schema(store,&b,fighter?64:16))return 0;
    if(fighter){
        if(!name_is(b,"IsUserControlled")||!name_is(b+38*80,"RatingCashAvailable")||!name_is(b+48*80,"Cash")||!name_is(b+61*80,"RatingProgress"))return 0;
    }else if(!name_is(b+2*80,"LastSkillPointsEarned")||!name_is(b+10*80,"SkillPoints")||!name_is(b+11*80,"TotalXp"))return 0;
    if(kind==CAREER_FITNESS&&!name_is(b+50*80,"Fitness"))return 0;
    uintptr_t f=b+index*80,p=pointer(store+0x30);uint16_t bits=0;uint32_t type=0,bias=1,rows=0,stride=0,start=0,bit=0,word=0;
    if(!rd(f+0x30,&bits,2)||!u32(f+0x34,&bias)||!u32(f+0x38,&type)||type!=2||bias||!bits||bits>31||
       !u32(p+0x10,&rows)||rows!=1||!u32(p+0xc,&stride)||!stride||stride>4096||
       !u32(p+0x18,&start)||start<8||start>16384||!u32(p+0x20+index*4,&bit)||bit/32>=stride||bit%32+bits>32)return 0;
    uintptr_t a=p+((uintptr_t)start+bit/32)*4;
    if((a&3)||!u32(a,&word)||!writable(a,4)||!writable(pointer(store+0x10)+0xe8,4))return 0;
    *out=(CareerField){store,p,a,index,bits,bit%32,(word>>(bit%32))&((1u<<bits)-1),(1u<<bits)-1,0};return 1;
}
static int same(const CareerField *a,const CareerField *b){return a->store==b->store&&a->packed==b->packed&&a->address==b->address&&a->index==b->index&&a->bits==b->bits&&a->shift==b->shift&&a->row==b->row;}
/* Decode only the active fighter's checked record reference. A reference stores
   the table ID in its upper 15 bits and row in its lower 17 bits; never assume row 0. */
static int training_field(uintptr_t store,uint32_t row,uint32_t index,uint16_t wanted,CareerField *out){
    uintptr_t b=0;if(!schema(store,&b,32))return 0;
    uintptr_t p=pointer(store+0x30),f=b+index*80;uint16_t bits=0;uint32_t type=0,bias=1,rows=0,stride=0,start=0,bit=0,word=0;
    if(!rd(f+0x30,&bits,2)||bits!=wanted||!u32(f+0x34,&bias)||bias||!u32(f+0x38,&type)||type!=2||
       !u32(p+0x10,&rows)||!rows||rows>131072||row>=rows||!u32(p+0xc,&stride)||!stride||stride>4096||
       !u32(p+0x18,&start)||start<8||start>16384||!u32(p+0x20+index*4,&bit)||bit/32>=stride||bit%32+bits>32)return 0;
    uintptr_t address=p+((uintptr_t)start+(uintptr_t)row*stride+bit/32)*4;
    if((address&3)||!u32(address,&word)||!writable(address,4)||!writable(pointer(store+0x10)+0xe8,4))return 0;
    *out=(CareerField){store,p,address,index,bits,bit%32,(word>>(bit%32))&((1u<<bits)-1),(1u<<bits)-1,row};return 1;
}
static int describe_training(uintptr_t fighter,uintptr_t owner,CareerField *out,uint32_t *reference,uint32_t *week){
    uintptr_t b=0;if(!schema(fighter,&b,64)||!name_is(b+63*80,"TrainingCamp"))return 0;
    uintptr_t f=b+63*80,p=pointer(fighter+0x30);uint32_t type=0,bit=0,start=0,stride=0,ref=0;uint16_t bits=0;
    if(!u32(f+0x38,&type)||type!=7||!rd(f+0x30,&bits,2)||bits!=65535||!u32(p+0x18,&start)||start<8||start>16384||
       !u32(p+0xc,&stride)||!stride||stride>4096||!u32(p+0x20+63*4,&bit)||bit%32||bit/32>=stride||!u32(p+((uintptr_t)start+bit/32)*4,&ref)||ref==UINT32_MAX)return 0;
    uint32_t id=ref>>17,row=ref&0x1ffff;if(id<0x1000)return 0;
    uintptr_t begin=pointer(owner+0x108),end=pointer(owner+0x110);size_t slot=id-0x1000;
    if(!begin||end<begin||(end-begin)%8||(end-begin)/8>16384||slot>=(end-begin)/8)return 0;
    uintptr_t store=pointer(begin+slot*8);uint32_t store_id=0;
    if(pointer(store+0x10)!=owner||!u32(store+0x2c,&store_id)||store_id!=id||pointer(store+8)!=pointer(f+0x48)||!schema(store,&b,32)||
       !name_is(b+2*80,"CampLength")||!name_is(b+10*80,"CurrentPoints")||!name_is(b+11*80,"CurrentWeek")||!name_is(b+25*80,"Sessions"))return 0;
    CareerField length,current;
    if(!training_field(store,row,10,10,out)||out->value>7||!training_field(store,row,2,7,&length)||!length.value||length.value>52||
       !training_field(store,row,11,7,&current)||current.value>=length.value)return 0;
    *reference=ref;*week=current.value;return 1;
}
static int resolve(uintptr_t root,CareerField result[2],uintptr_t *owner){
    /* This is the same default-user registry queried by the career service.
       Inactive save-slot metadata and online currencies do not have this chain. */
    uintptr_t registry=pointer(root+0x10),b=0;
    if(pointer(root)!=guest+0x9c38410||!schema(registry,&b,5)||!name_is(b,"RegisterEvent")||!name_is(b+4*80,"UserList"))return 0;
    uintptr_t c=pointer(registry+0x10),begin=pointer(c+0x108),end=pointer(c+0x110);
    if(!begin||end<begin||(end-begin)%8||(end-begin)/8>16384)return 0;
    size_t n=(end-begin)/8;int found[2]={0,0};
    /* Verified schema slots first, bounded schema lookup if the layout moves. */
    const size_t hints[]={525,109};size_t slot[2]={SIZE_MAX,SIZE_MAX};
    for(int k=0;k<2;++k){
        size_t i=c==cache_collection&&begin==cache_begin?cached_slot[k]:hints[k];
        if(i<n){uintptr_t s=pointer(begin+i*8);if(pointer(s+0x10)==c&&describe(s,k,&result[k])){found[k]=1;slot[k]=i;}}
    }
    if(!found[0]||!found[1])for(size_t i=0;i<n;++i){
        uintptr_t s=pointer(begin+i*8);if(pointer(s+0x10)!=c)continue;
        for(int k=0;k<2;++k)if(!found[k]&&describe(s,k,&result[k])){found[k]=1;slot[k]=i;}
        if(found[0]&&found[1])break;
    }
    if(!found[0]||!found[1])return 0;cache_collection=c;cache_begin=begin;memcpy(cached_slot,slot,sizeof(slot));*owner=c;return 1;
}
static void process_user(uintptr_t root,int64_t user,int has_user){
    if(user!=-1||!TryAcquireSRWLockExclusive(&guard))return;
    CareerField fresh[2];uintptr_t c=0;
    if(!has_user||!resolve(root,fresh,&c)){
        snapshot.ready=0;snapshot.fitness_ready=0;snapshot.training_ready=0;if(snapshot.status==CAREER_PENDING)snapshot.status=CAREER_CHANGED;active_root=collection=0;ReleaseSRWLockExclusive(&guard);return;
    }
    CareerField fit={0};int fit_ready=describe(fresh[0].store,CAREER_FITNESS,&fit)&&fit.bits==15&&fit.value<=20000;
    CareerField camp={0};uint32_t ref=0,week=0;int camp_ready=describe_training(fresh[0].store,c,&camp,&ref,&week);
    if(snapshot.status==CAREER_PENDING){
        int k=snapshot.kind;CareerField *target=k==CAREER_DAYS?&camp:k==CAREER_FITNESS?&fit:&fresh[k];
        if(root!=active_root||c!=collection||!same(&expected[0],&fresh[0])||!same(&expected[1],&fresh[1])||GetTickCount64()-queued>30000)snapshot.status=CAREER_CHANGED;
        else if(k==CAREER_FITNESS&&(!fit_ready||!same(&expected_fitness,&fit)))snapshot.status=CAREER_CHANGED;
        else if(k==CAREER_DAYS&&(!camp_ready||!same(&expected_training,&camp)||ref!=expected_training_reference||week!=expected_training_week))snapshot.status=CAREER_CHANGED;
        else if(k<CAREER_FITNESS&&snapshot.amount>target->maximum-target->value)snapshot.status=CAREER_LIMIT;
        else if(game_get(target->store,target->row,target->index)!=target->value)snapshot.status=CAREER_ERROR;
        else{
            uint32_t desired=k==CAREER_DAYS?7:k==CAREER_FITNESS?10000:target->value+snapshot.amount;
            /* Consume before the one-time setter: never repeat on UI refresh. */
            snapshot.status=CAREER_ERROR;game_set(target->store,target->row,target->index,desired);game_dirty(target->store);
            CareerField after;
            uint32_t after_ref=0,after_week=0;
            int described=k==CAREER_DAYS?describe_training(fresh[0].store,c,&after,&after_ref,&after_week)&&after_ref==ref&&after_week==week:describe(target->store,k,&after);
            if(described&&same(target,&after)&&after.value==desired){*target=after;snapshot.status=CAREER_ADDED;}
        }
    }
    training=camp;training_reference=ref;training_week=week;snapshot.training_ready=camp_ready;snapshot.training_days=camp_ready?camp.value:0;snapshot.training_week=camp_ready?week+1:0;
    fitness=fit;snapshot.fitness_ready=fit_ready;snapshot.fitness=fit_ready?fit.value:0;
    active_root=root;collection=c;memcpy(fields,fresh,sizeof(fields));snapshot.ready=1;
    for(int k=0;k<2;++k){snapshot.balance[k]=fresh[k].value;snapshot.maximum[k]=fresh[k].maximum;}
    if(snapshot.status==CAREER_WAIT)snapshot.status=CAREER_READY;
    ReleaseSRWLockExclusive(&guard);
}
static void *__attribute__((sysv_abi)) hooked_user(uintptr_t *out,uintptr_t root,int64_t user){
    void *value=original_user(out,root,user);
    /* This utility can run many times per frame. Most calls must only forward:
       at most four probes per second, with an immediate probe for a new request. */
    if(user!=-1)return value;
    LONG64 now=(LONG64)GetTickCount64(),due=InterlockedCompareExchange64(&next_probe,0,0);
    if(now<due||InterlockedCompareExchange64(&next_probe,now+250,due)!=due)return value;
#ifdef CAREER_CHEATS_TEST
    ++probe_count;
#endif
    uintptr_t selected=0;rd((uintptr_t)out,&selected,8);process_user(root,user,selected!=0);return value;
}
void career_cheats_init(uintptr_t base){guest=base;memset(&snapshot,0,sizeof(snapshot));InterlockedExchange64(&next_probe,0);cache_collection=cache_begin=0;cached_slot[0]=cached_slot[1]=SIZE_MAX;}
void career_cheats_tick(void){
    if(!original_user){
        unsigned char bytes[sizeof(user_signature)];
        uintptr_t flag=pointer(guest+0xb452958);char flag_name[28];
        if(!rd(pointer(flag+8),flag_name,28)||memcmp(flag_name,"Fighter_Disable_StaminaLoss",26))return;
        static const unsigned char get_sig[]={0x48,0x8b,0x47,0x08,0x89,0xd1,0x4c,0x8d,0x04,0x89,0x49,0xc1,0xe0,0x04};
        static const unsigned char set_sig[]={0x48,0x8b,0x47,0x08,0x41,0x89,0xd0,0x48,0x8b,0x7f,0x30};
        static const unsigned char dirty_sig[]={0x48,0x8b,0x47,0x10,0xff,0x80,0xe8,0x00,0x00,0x00,0xc3};
        if(!rd(guest+0x5c015d0,bytes,sizeof(bytes)))return;
        if(memcmp(bytes,user_signature,sizeof(bytes))||!rd(guest+0x56dcf80,bytes,sizeof(get_sig))||memcmp(bytes,get_sig,sizeof(get_sig))||
           !rd(guest+0x56dd300,bytes,sizeof(set_sig))||memcmp(bytes,set_sig,sizeof(set_sig))||!rd(guest+0x56ccef0,bytes,sizeof(dirty_sig))||memcmp(bytes,dirty_sig,sizeof(dirty_sig))){snapshot.status=CAREER_UNSUPPORTED;return;}
        game_get=(GetFn)(guest+0x56dcf80);game_set=(SetFn)(guest+0x56dd300);game_dirty=(DirtyFn)(guest+0x56ccef0);
        ExactHook hook={(unsigned char*)(guest+0x5c015d0),(void*)hooked_user,(void**)&original_user,user_signature,17};const char *error=NULL;
        if(!install_exact_hooks(&hook,1,&error))return;
    }
    AcquireSRWLockExclusive(&guard);
    if(snapshot.ready){CareerField fresh[2];
        if(!describe(fields[0].store,0,&fresh[0])||!describe(fields[1].store,1,&fresh[1])||!same(&fields[0],&fresh[0])||!same(&fields[1],&fresh[1])){snapshot.ready=0;snapshot.fitness_ready=0;snapshot.training_ready=0;if(snapshot.status==CAREER_PENDING)snapshot.status=CAREER_CHANGED;}
        else for(int k=0;k<2;++k)snapshot.balance[k]=fresh[k].value;
    }
    if(snapshot.status==CAREER_PENDING&&GetTickCount64()-queued>30000)snapshot.status=CAREER_CHANGED;
    ReleaseSRWLockExclusive(&guard);
}
void career_cheats_cancel(void){AcquireSRWLockExclusive(&guard);if(snapshot.status==CAREER_PENDING)snapshot.status=CAREER_CHANGED;ReleaseSRWLockExclusive(&guard);}
void career_cheats_snapshot(CareerSnapshot *out){AcquireSRWLockShared(&guard);*out=snapshot;ReleaseSRWLockShared(&guard);}
int career_cheats_add(int kind,uint32_t amount){
    if(kind<0||kind>=2||!amount)return 0;
    AcquireSRWLockExclusive(&guard);int ok=0;
    if(snapshot.ready&&snapshot.status!=CAREER_PENDING){
        CareerField fresh[2];
        if(describe(fields[0].store,0,&fresh[0])&&describe(fields[1].store,1,&fresh[1])&&same(&fields[0],&fresh[0])&&same(&fields[1],&fresh[1])){
            snapshot.kind=kind;snapshot.amount=amount;
            if(amount<=fresh[kind].maximum-fresh[kind].value){memcpy(expected,fresh,sizeof(expected));queued=GetTickCount64();snapshot.status=CAREER_PENDING;InterlockedExchange64(&next_probe,0);ok=1;}
            else snapshot.status=CAREER_LIMIT;
        }else snapshot.status=CAREER_CHANGED;
    }
    ReleaseSRWLockExclusive(&guard);return ok;
}

int career_cheats_restore_fitness(void){
    AcquireSRWLockExclusive(&guard);int ok=0;CareerField fresh;
    if(snapshot.ready&&snapshot.fitness_ready&&snapshot.status!=CAREER_PENDING&&
       describe(fields[0].store,CAREER_FITNESS,&fresh)&&same(&fitness,&fresh)&&fresh.bits==15&&fresh.value<=20000){
        memcpy(expected,fields,sizeof(expected));expected_fitness=fresh;snapshot.kind=CAREER_FITNESS;snapshot.amount=10000;
        queued=GetTickCount64();snapshot.status=CAREER_PENDING;InterlockedExchange64(&next_probe,0);ok=1;
    }
    ReleaseSRWLockExclusive(&guard);return ok;
}

int career_cheats_restore_days(void){
    AcquireSRWLockExclusive(&guard);int ok=0;CareerField fresh;uint32_t ref=0,week=0;
    if(snapshot.ready&&snapshot.training_ready&&snapshot.status!=CAREER_PENDING&&
       describe_training(fields[0].store,collection,&fresh,&ref,&week)&&same(&training,&fresh)&&ref==training_reference&&week==training_week){
        memcpy(expected,fields,sizeof(expected));expected_training=fresh;expected_training_reference=ref;expected_training_week=week;
        snapshot.kind=CAREER_DAYS;snapshot.amount=7;queued=GetTickCount64();snapshot.status=CAREER_PENDING;InterlockedExchange64(&next_probe,0);ok=1;
    }
    ReleaseSRWLockExclusive(&guard);return ok;
}
