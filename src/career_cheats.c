/* Checked career changes and remembered longevity protection on the game's own selected-user lookup thread.
   No save-file edits, remote services or heap-value scans. */
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
static SetFn game_set,original_set;
static CareerField longevity;static uint32_t longevity_identity;
static volatile LONG longevity_on,longevity_blocked,longevity_wanted;static volatile LONG64 longevity_store;
static DirtyFn game_dirty;
static SRWLOCK guard=SRWLOCK_INIT;
static CareerSnapshot snapshot;
static uint64_t queued;
static int reward_multiplier=1,reward_active_multiplier=1;static uintptr_t reward_store,reward_packed;static uint32_t reward_rows,reward_original[2048],reward_applied[2048];
typedef struct {CareerField first;uint32_t rows,stride;} GradePack;
static uint32_t expected_grade_ids[5];
static volatile LONG64 next_probe;
static volatile LONG diagnostic_reason,diagnostic_hooks,diagnostic_probes;
static void diagnose(int reason){InterlockedExchange(&diagnostic_reason,reason);}
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
static int describe_checked(uintptr_t store,int kind,CareerField *out,int *failure){
    int fighter=kind==CAREER_MONEY||kind==CAREER_FITNESS||kind==CAREER_LONGEVITY;
    uintptr_t b=0;uint32_t index=kind==CAREER_MONEY?48:kind==CAREER_FITNESS?50:kind==CAREER_LONGEVITY?24:10;
    if(!schema(store,&b,fighter?64:16))return 0;
    if(fighter){
        if(!name_is(b,"IsUserControlled")||!name_is(b+38*80,"RatingCashAvailable")||!name_is(b+48*80,"Cash")||!name_is(b+61*80,"RatingProgress"))return 0;
    }else if(!name_is(b+2*80,"LastSkillPointsEarned")||!name_is(b+10*80,"SkillPoints")||!name_is(b+11*80,"TotalXp"))return 0;
    if(kind==CAREER_FITNESS&&!name_is(b+50*80,"Fitness"))return 0;
    if(kind==CAREER_LONGEVITY&&!name_is(b+24*80,"Longevity"))return 0;
    if(failure)*failure=1; /* The requested schema matched; distinguish layout/protection failures. */
    uintptr_t f=b+index*80,p=pointer(store+0x30);uint16_t bits=0;uint32_t type=0,bias=1,rows=0,stride=0,start=0,bit=0,word=0;
    if(!rd(f+0x30,&bits,2)||!u32(f+0x34,&bias)||!u32(f+0x38,&type)||type!=2||bias||!bits||bits>31||
       !u32(p+0x10,&rows)||rows!=1||!u32(p+0xc,&stride)||!stride||stride>4096||
       !u32(p+0x18,&start)||start<8||start>16384||!u32(p+0x20+index*4,&bit)||bit/32>=stride||bit%32+bits>32)return 0;
    uintptr_t a=p+((uintptr_t)start+bit/32)*4;
    if((a&3)||!u32(a,&word))return 0;
    if(!writable(a,4)||!writable(pointer(store+0x10)+0xe8,4)){if(failure)*failure=2;return 0;}
    *out=(CareerField){store,p,a,index,bits,bit%32,(word>>(bit%32))&((1u<<bits)-1),(1u<<bits)-1,0};return 1;
}
static int describe(uintptr_t store,int kind,CareerField *out){return describe_checked(store,kind,out,NULL);}
static int same(const CareerField *a,const CareerField *b){return a->store==b->store&&a->packed==b->packed&&a->address==b->address&&a->index==b->index&&a->bits==b->bits&&a->shift==b->shift&&a->row==b->row;}
static void longevity_disable(void){InterlockedExchange(&longevity_on,0);InterlockedExchange64(&longevity_store,0);snapshot.longevity_enabled=0;}
static int fighter_identity(uintptr_t store,uint32_t *identity){
    uintptr_t b=0;if(!schema(store,&b,64)||!name_is(b+22*80,"ID")||!name_is(b+4*80,"AppearanceId"))return 0;
    uintptr_t p=pointer(store+0x30);uint32_t start=0,stride=0,bit=0,type=0,bias=0;uint16_t bits=0;
    if(!rd(b+22*80+0x30,&bits,2)||bits!=16||!u32(b+22*80+0x34,&bias)||bias!=(uint32_t)-3||!u32(b+22*80+0x38,&type)||type!=2||
       !u32(p+0x18,&start)||start<8||start>16384||!u32(p+0xc,&stride)||!stride||stride>4096||!u32(p+0x20+22*4,&bit)||bit%32||bit/32>=stride)return 0;
    return u32(p+((uintptr_t)start+bit/32)*4,identity);
}
static void __attribute__((sysv_abi)) hooked_set(uintptr_t store,uint32_t row,uint32_t index,uint32_t value){
    /* Most career/game writes take only this constant-time forwarding path. */
    if(index==24&&row==0&&InterlockedCompareExchange(&longevity_on,0,0)&&store==(uintptr_t)InterlockedCompareExchange64(&longevity_store,0,0)){
        AcquireSRWLockShared(&guard);CareerField current;uint32_t identity=0;
        int valid=InterlockedCompareExchange(&longevity_on,0,0)&&snapshot.ready&&store==longevity.store&&
            pointer(active_root)==guest+0x9c38410&&pointer(pointer(active_root+0x10)+0x10)==collection&&pointer(store+0x10)==collection&&
            describe(store,CAREER_LONGEVITY,&current)&&current.bits==17&&current.value<=10000&&same(&longevity,&current)&&fighter_identity(store,&identity)&&identity==longevity_identity;
        if(valid&&value<current.value){value=current.value;InterlockedIncrement(&longevity_blocked);}
        if(!valid){InterlockedExchange(&longevity_on,0);InterlockedExchange64(&longevity_store,0);}
        original_set(store,row,index,value);ReleaseSRWLockShared(&guard);return;
    }
    original_set(store,row,index,value);
}
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
static uintptr_t owned_store(uintptr_t owner,size_t slot){
    uintptr_t begin=pointer(owner+0x108),end=pointer(owner+0x110);
    if(!begin||end<begin||(end-begin)%8||(end-begin)/8>16384||slot>=(end-begin)/8)return 0;
    uintptr_t store=pointer(begin+slot*8);return pointer(store+0x10)==owner?store:0;
}
static int packed_value_access(uintptr_t store,uint32_t row,uint32_t index,uint32_t type,uint16_t bits,int32_t bias,CareerField *out,int require_write){
    uintptr_t schema_ptr=pointer(store+8),begin=pointer(schema_ptr+0x30),end=pointer(schema_ptr+0x38);uint32_t t=0,b=0,rows=0,stride=0,start=0,bit=0,word=0;uint16_t n=0;
    if(pointer(store)!=guest+0x9bf7be8||!begin||end<begin||(end-begin)%80||index>=(end-begin)/80||bits>31)return 0;
    uintptr_t f=begin+index*80,p=pointer(store+0x30);
    if(!rd(f+0x30,&n,2)||n!=bits||!u32(f+0x34,&b)||(int32_t)b!=bias||!u32(f+0x38,&t)||t!=type||
       !u32(p+0x10,&rows)||!rows||rows>4096||row>=rows||!u32(p+0xc,&stride)||!stride||stride>4096||
       !u32(p+0x18,&start)||start<8||start>16384||!u32(p+0x20+index*4,&bit)||bit/32>=stride||bit%32+bits>32)return 0;
    uintptr_t address=p+((uintptr_t)start+(uintptr_t)row*stride+bit/32)*4;
    if(!u32(address,&word)||(require_write&&!writable(address,4)))return 0;
    *out=(CareerField){store,p,address,index,bits,bit%32,(word>>(bit%32))&((1u<<bits)-1),(1u<<bits)-1,row};return 1;
}
static int packed_value(uintptr_t store,uint32_t row,uint32_t index,uint32_t type,uint16_t bits,int32_t bias,CareerField *out){
    return packed_value_access(store,row,index,type,bits,bias,out,1);
}
static int grade_write_access(uintptr_t address){
    MEMORY_BASIC_INFORMATION m;
    return VirtualQuery((void*)address,&m,sizeof(m))&&m.State==MEM_COMMIT&&!(m.Protect&(PAGE_GUARD|PAGE_NOACCESS))&&
        (m.Type==MEM_PRIVATE||m.Type==MEM_MAPPED)&&address+4<=((uintptr_t)m.BaseAddress+m.RegionSize)&&
        (m.Protect==PAGE_READONLY||m.Protect==PAGE_READWRITE||m.Protect==PAGE_WRITECOPY);
}
static int grade_set(const CareerField *f){
    if(!grade_write_access(f->address)||game_get(f->store,f->row,f->index)!=f->value-1)return 0;
    MEMORY_BASIC_INFORMATION m;DWORD old=0,ignored=0;int changed=0;
    if(!VirtualQuery((void*)f->address,&m,sizeof(m)))return 0;
    if(m.Protect==PAGE_READONLY){
        DWORD protection=m.Type==MEM_MAPPED?PAGE_WRITECOPY:PAGE_READWRITE;
        if(!VirtualProtect((void*)f->address,4,protection,&old))return 0;changed=1;
    }
    /* Mapped save snapshots may be read-only. WRITECOPY keeps the backing file
       untouched; restore protection after the game's enum setter and dirty flag. */
    game_set(f->store,f->row,f->index,4);game_dirty(f->store);
    int restored=!changed||VirtualProtect((void*)f->address,4,old,&ignored);
    return restored&&game_get(f->store,f->row,f->index)==4;
}
static int replace_field(const CareerField *f,uint32_t before,uint32_t after){
    uint32_t mask=f->maximum<<f->shift;
    for(int tries=0;tries<8;++tries){uint32_t word=0;if(!u32(f->address,&word)||((word>>f->shift)&f->maximum)!=before)return 0;
        uint32_t desired=(word&~mask)|((after<<f->shift)&mask);
        if((uint32_t)InterlockedCompareExchange((volatile LONG*)f->address,(LONG)desired,(LONG)word)==word)return 1;
    }
    return 0;
}
static int reward_schema(uintptr_t store,uint32_t *rows){
    uintptr_t b=0;if(!schema(store,&b,20)||!name_is(b+10*80,"SkillPointsReward")||!name_is(b+13*80,"TargetPositive")||!name_is(b+19*80,"TrainingStationId"))return 0;
    return u32(pointer(store+0x30)+0x10,rows)&&*rows&&*rows<=2048;
}
static void reward_restore(void){
    uint32_t rows=0;
    if(reward_store&&pointer(reward_store+0x30)==reward_packed&&reward_schema(reward_store,&rows)&&rows==reward_rows){
        for(uint32_t i=0;i<rows;++i){CareerField f;if(packed_value(reward_store,i,10,2,9,0,&f)&&f.value==reward_applied[i])replace_field(&f,reward_applied[i],reward_original[i]);}
    }
    reward_store=reward_packed=0;reward_rows=0;reward_active_multiplier=1;snapshot.training_rewards_ready=0;
}
static void training_rewards_refresh(uintptr_t owner){
    snapshot.training_multiplier=(uint32_t)reward_multiplier;
    uintptr_t master=pointer(owner+0xf0),begin=pointer(master+0x148),end=pointer(master+0x150),store=0;uint32_t rows=0;
    if(pointer(master)==guest+0x9bf6ae0&&begin&&end>=begin&&(end-begin)%8==0&&(end-begin)/8>211&&(end-begin)/8<=16384){
        store=pointer(begin+211*8);if(pointer(store+0x10)!=master)store=0;
    }
    if(!master||!store||!reward_schema(store,&rows)){snapshot.training_rewards_ready=0;return;}
    uintptr_t packed=pointer(store+0x30);
    if(store!=reward_store||packed!=reward_packed||rows!=reward_rows){
        reward_restore();
        for(uint32_t i=0;i<rows;++i){CareerField f;if(!packed_value(store,i,10,2,9,0,&f))return;reward_original[i]=reward_applied[i]=f.value;}
        reward_store=store;reward_packed=packed;reward_rows=rows;
    }
    if(reward_active_multiplier==reward_multiplier){snapshot.training_rewards_ready=1;return;}
    for(uint32_t i=0;i<rows;++i){
        CareerField f;if(!packed_value(store,i,10,2,9,0,&f)||f.value!=reward_applied[i]){reward_restore();return;}
        uint64_t scaled=(uint64_t)reward_original[i]*(uint32_t)reward_multiplier;uint32_t desired=scaled>f.maximum?f.maximum:(uint32_t)scaled;
        if(desired!=f.value&&!replace_field(&f,f.value,desired)){reward_restore();return;}
        reward_applied[i]=desired;
    }
    reward_active_multiplier=reward_multiplier;snapshot.training_rewards_ready=1;
}
static int grade_row(const GradePack *pack,uint32_t row,CareerField *out){
    *out=pack->first;out->row=row;out->address+=(uintptr_t)row*pack->stride*4;
    uint32_t word=0;if(!u32(out->address,&word))return 0;
    out->value=(word>>out->shift)&out->maximum;return out->value<=5;
}
static int grade_fields(uintptr_t owner,GradePack packs_out[5],uint32_t refs[5]){
    uintptr_t station=owned_store(owner,326),b=0;
    if(!schema(station,&b,23)||!name_is(b+7*80,"CurrentChallengePack")||!name_is(b+14*80,"Id"))return 0;
    uintptr_t p=pointer(station+0x30);uint32_t rows=0,stride=0,start=0,bit=0;
    if(!u32(p+0x10,&rows)||rows!=5||!u32(p+0xc,&stride)||!stride||stride>4096||!u32(p+0x18,&start)||start<8||start>16384||
       !u32(p+0x20+7*4,&bit)||bit%32||bit/32>=stride)return 0;
    int count=0;memset(packs_out,0,sizeof(GradePack)*5);
    for(uint32_t i=0;i<5;++i){uint32_t ref=0;if(!u32(p+((uintptr_t)start+(uintptr_t)i*stride+bit/32)*4,&ref))return 0;
        refs[i]=ref;if(ref==UINT32_MAX)continue;
        uint32_t table=ref>>17;if((table&0x7000)!=0x1000)return 0;
        uintptr_t store=owned_store(owner,table&0xfff);int duplicate=0;
        for(uint32_t j=0;j<i;++j)if(packs_out[j].first.store==store)duplicate=1;
        if(duplicate)continue;
        if(!schema(store,&b,4)||!name_is(b,"Challenges")||!name_is(b+2*80,"Grade")||!name_is(b+3*80,"Name"))return 0;
        uintptr_t enumeration=pointer(b+2*80+0x48),eb=pointer(enumeration+0x30),ee=pointer(enumeration+0x38);uint32_t a=0;
        if(!eb||ee<eb||ee-eb!=6*80||!name_is(eb,"Invalid")||!name_is(eb+5*80,"A")||!u32(eb+5*80+0x40,&a)||a!=4)return 0;
        GradePack *pack=&packs_out[i];uintptr_t packed=pointer(store+0x30);
        if(!u32(packed+0x10,&pack->rows)||!pack->rows||pack->rows>256||(ref&0x1ffff)>=pack->rows||
           !u32(packed+0xc,&pack->stride)||!packed_value_access(store,0,2,3,3,-1,&pack->first,0))return 0;
        for(uint32_t row=0;row<pack->rows;++row){CareerField f;if(!grade_row(pack,row,&f)||!grade_write_access(f.address))return 0;if(f.value<5)++count;}
    }
    return count+1;
}
static int resolve(uintptr_t root,CareerField result[2],uintptr_t *owner){
    /* This is the same default-user registry queried by the career service.
       Inactive save-slot metadata and online currencies do not have this chain. */
    uintptr_t registry=pointer(root+0x10),b=0;
    if(pointer(root)!=guest+0x9c38410){diagnose(CAREER_DIAG_ROOT);return 0;}
    if(!schema(registry,&b,5)||!name_is(b,"RegisterEvent")||!name_is(b+4*80,"UserList")){diagnose(CAREER_DIAG_REGISTRY);return 0;}
    uintptr_t c=pointer(registry+0x10),begin=pointer(c+0x108),end=pointer(c+0x110);
    if(!begin||end<begin||(end-begin)%8||(end-begin)/8>16384){diagnose(CAREER_DIAG_COLLECTION);return 0;}
    size_t n=(end-begin)/8;int found[2]={0,0},failures[2]={0,0};
    /* Verified schema slots first, bounded schema lookup if the layout moves. */
    const size_t hints[]={525,109};size_t slot[2]={SIZE_MAX,SIZE_MAX};
    for(int k=0;k<2;++k){
        size_t i=c==cache_collection&&begin==cache_begin?cached_slot[k]:hints[k];
        if(i<n){uintptr_t s=pointer(begin+i*8);if(pointer(s+0x10)==c&&describe_checked(s,k,&result[k],&failures[k])){found[k]=1;slot[k]=i;}}
    }
    if(!found[0]||!found[1])for(size_t i=0;i<n;++i){
        uintptr_t s=pointer(begin+i*8);if(pointer(s+0x10)!=c)continue;
        for(int k=0;k<2;++k)if(!found[k]){int failure=0;if(describe_checked(s,k,&result[k],&failure)){found[k]=1;slot[k]=i;}else if(failure>failures[k])failures[k]=failure;}
        if(found[0]&&found[1])break;
    }
    if(!found[0]||!found[1]){int k=!found[0]?0:1;diagnose((failures[k]==2?CAREER_DIAG_MONEY_READONLY:failures[k]==1?CAREER_DIAG_MONEY_LAYOUT:CAREER_DIAG_MONEY)+k);return 0;}
    cache_collection=c;cache_begin=begin;memcpy(cached_slot,slot,sizeof(slot));*owner=c;diagnose(CAREER_DIAG_READY);return 1;
}
static void process_user(uintptr_t root,int64_t user,int has_user){
    if(user!=-1||!TryAcquireSRWLockExclusive(&guard))return;
    CareerField fresh[2];uintptr_t c=0;
    if(!has_user||!resolve(root,fresh,&c)){
        if(!has_user)diagnose(CAREER_DIAG_NO_USER);
        snapshot.ready=0;snapshot.fitness_ready=0;snapshot.training_ready=0;snapshot.longevity_ready=0;longevity_disable();if(snapshot.status==CAREER_PENDING)snapshot.status=CAREER_CHANGED;active_root=collection=0;ReleaseSRWLockExclusive(&guard);return;
    }
    CareerField long_field={0};uint32_t identity=0;int long_ready=describe(fresh[0].store,CAREER_LONGEVITY,&long_field)&&long_field.bits==17&&long_field.value<=10000&&fighter_identity(fresh[0].store,&identity);
    if(InterlockedCompareExchange(&longevity_on,0,0)&&(!long_ready||root!=active_root||c!=collection||!same(&longevity,&long_field)||identity!=longevity_identity))longevity_disable();
    longevity=long_field;longevity_identity=identity;snapshot.longevity_ready=long_ready;snapshot.longevity=long_ready?long_field.value:0;snapshot.longevity_enabled=InterlockedCompareExchange(&longevity_on,0,0)!=0;
    CareerField fit={0};int fit_ready=describe(fresh[0].store,CAREER_FITNESS,&fit)&&fit.bits==15&&fit.value<=20000;
    CareerField camp={0};uint32_t ref=0,week=0;int camp_ready=describe_training(fresh[0].store,c,&camp,&ref,&week);
    GradePack grades[5];uint32_t grade_refs[5]={0};int grades_ready=grade_fields(c,grades,grade_refs);
    snapshot.sparring_grades_ready=grades_ready>0;snapshot.sparring_grades_completed=grades_ready>0?(uint32_t)(grades_ready-1):0;
    if(snapshot.status==CAREER_PENDING&&snapshot.kind==CAREER_GRADES){
        if(root!=active_root||c!=collection||!grades_ready||memcmp(grade_refs,expected_grade_ids,sizeof(grade_refs))||GetTickCount64()-queued>30000)snapshot.status=CAREER_CHANGED;
        else{
            snapshot.status=CAREER_ADDED;
            for(int i=0;i<5&&snapshot.status==CAREER_ADDED;++i)if(grades[i].first.store){
                for(uint32_t row=0;row<grades[i].rows;++row){CareerField f;
                    if(!grade_row(&grades[i],row,&f)){snapshot.status=CAREER_ERROR;break;}
                    if(f.value<5&&!grade_set(&f)){snapshot.status=CAREER_ERROR;break;}
                }
            }
        }
    }else if(snapshot.status==CAREER_PENDING){
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
    if(long_ready&&original_set&&InterlockedCompareExchange(&longevity_wanted,0,0)){
        InterlockedExchange64(&longevity_store,(LONG64)long_field.store);InterlockedExchange(&longevity_on,1);snapshot.longevity_enabled=1;
    }
    snapshot.longevity_requested=InterlockedCompareExchange(&longevity_wanted,0,0)!=0;
    training_rewards_refresh(c);
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
    InterlockedIncrement(&diagnostic_probes);
#ifdef CAREER_CHEATS_TEST
    ++probe_count;
#endif
    uintptr_t selected=0;rd((uintptr_t)out,&selected,8);process_user(root,user,selected!=0);return value;
}
void career_cheats_init(uintptr_t base){guest=base;memset(&snapshot,0,sizeof(snapshot));InterlockedExchange64(&next_probe,0);diagnose(CAREER_DIAG_CODE_WAIT);InterlockedExchange(&diagnostic_hooks,0);InterlockedExchange(&diagnostic_probes,0);longevity_disable();InterlockedExchange(&longevity_wanted,0);cache_collection=cache_begin=0;cached_slot[0]=cached_slot[1]=SIZE_MAX;}
void career_cheats_tick(void){
    if(!original_user){
        unsigned char bytes[sizeof(user_signature)];
        /* Career screens can load before fight-only NOA flags are registered.
           The checked career function signatures below establish readiness;
           a stamina binding is unrelated to the selected career database. */
        static const unsigned char get_sig[]={0x48,0x8b,0x47,0x08,0x89,0xd1,0x4c,0x8d,0x04,0x89,0x49,0xc1,0xe0,0x04};
        static const unsigned char set_sig[]={0x48,0x8b,0x47,0x08,0x41,0x89,0xd0,0x48,0x8b,0x7f,0x30,0x4f,0x8d,0x04,0x80};
        static const unsigned char dirty_sig[]={0x48,0x8b,0x47,0x10,0xff,0x80,0xe8,0x00,0x00,0x00,0xc3};
        if(!rd(guest+0x5c015d0,bytes,sizeof(bytes))){diagnose(CAREER_DIAG_CODE_WAIT);return;}
        if(memcmp(bytes,user_signature,sizeof(bytes))||!rd(guest+0x56dcf80,bytes,sizeof(get_sig))||memcmp(bytes,get_sig,sizeof(get_sig))||
           !rd(guest+0x56dd300,bytes,sizeof(set_sig))||memcmp(bytes,set_sig,sizeof(set_sig))||!rd(guest+0x56ccef0,bytes,sizeof(dirty_sig))||memcmp(bytes,dirty_sig,sizeof(dirty_sig))){snapshot.status=CAREER_UNSUPPORTED;diagnose(CAREER_DIAG_CODE_MISMATCH);return;}
        game_get=(GetFn)(guest+0x56dcf80);game_set=(SetFn)(guest+0x56dd300);game_dirty=(DirtyFn)(guest+0x56ccef0);
        ExactHook hooks[]={
            {(unsigned char*)(guest+0x5c015d0),(void*)hooked_user,(void**)&original_user,user_signature,17},
            {(unsigned char*)(guest+0x56dd300),(void*)hooked_set,(void**)&original_set,set_sig,sizeof(set_sig)}
        };const char *error=NULL;
        if(!install_exact_hooks(hooks,2,&error)){diagnose(CAREER_DIAG_HOOK_FAILED);return;}
        InterlockedExchange(&diagnostic_hooks,1);diagnose(CAREER_DIAG_USER_WAIT);
    }
    AcquireSRWLockExclusive(&guard);
    if(snapshot.status==CAREER_UNSUPPORTED)snapshot.status=CAREER_WAIT; /* Checked hooks are now installed. */
    if(snapshot.ready){CareerField fresh[2];
        if(!describe(fields[0].store,0,&fresh[0])||!describe(fields[1].store,1,&fresh[1])||!same(&fields[0],&fresh[0])||!same(&fields[1],&fresh[1])){snapshot.ready=0;snapshot.fitness_ready=0;snapshot.training_ready=0;snapshot.longevity_ready=0;longevity_disable();diagnose(CAREER_DIAG_CHANGED);if(snapshot.status==CAREER_PENDING)snapshot.status=CAREER_CHANGED;}
        else for(int k=0;k<2;++k)snapshot.balance[k]=fresh[k].value;
    }
    if(snapshot.status==CAREER_PENDING&&GetTickCount64()-queued>30000)snapshot.status=CAREER_CHANGED;
    ReleaseSRWLockExclusive(&guard);
}
void career_cheats_cancel(void){AcquireSRWLockExclusive(&guard);InterlockedExchange(&longevity_wanted,0);snapshot.longevity_requested=0;longevity_disable();reward_multiplier=1;reward_restore();snapshot.training_multiplier=1;if(snapshot.status==CAREER_PENDING)snapshot.status=CAREER_CHANGED;ReleaseSRWLockExclusive(&guard);}
void career_cheats_snapshot(CareerSnapshot *out){AcquireSRWLockShared(&guard);*out=snapshot;out->longevity_blocked=(uint32_t)InterlockedCompareExchange(&longevity_blocked,0,0);ReleaseSRWLockShared(&guard);}
void career_cheats_diagnostics(CareerDiagnostics *out){out->reason=InterlockedCompareExchange(&diagnostic_reason,0,0);out->hooks=InterlockedCompareExchange(&diagnostic_hooks,0,0);out->probes=(uint32_t)InterlockedCompareExchange(&diagnostic_probes,0,0);}
const char *career_cheats_diagnostic_name(int reason){
    static const char *names[]={"waiting_for_guest_code","guest_signature_mismatch","hook_install_failed","waiting_for_default_user_lookup","no_selected_user","user_root_mismatch","user_registry_mismatch","collection_unavailable","money_schema_not_found","points_schema_not_found","money_layout_rejected","points_layout_rejected","money_record_read_only","points_record_read_only","verified","binding_changed"};
    return reason>=0&&reason<(int)(sizeof(names)/sizeof(names[0]))?names[reason]:"unknown";
}
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

int career_cheats_protect_longevity(int enabled){
    AcquireSRWLockExclusive(&guard);CareerField current;uint32_t identity=0;
    int was=InterlockedExchange(&longevity_wanted,enabled!=0);snapshot.longevity_requested=enabled!=0;
    if(!enabled)longevity_disable();
    else{
        if(!was)InterlockedExchange(&longevity_blocked,0);
        if(original_set&&snapshot.ready&&snapshot.longevity_ready&&describe(longevity.store,CAREER_LONGEVITY,&current)&&current.bits==17&&current.value<=10000&&same(&longevity,&current)&&fighter_identity(current.store,&identity)&&identity==longevity_identity){
            InterlockedExchange64(&longevity_store,(LONG64)current.store);InterlockedExchange(&longevity_on,1);snapshot.longevity_enabled=1;
        }else longevity_disable(); /* Keep the preference; bind only after a checked career lookup. */
    }
    ReleaseSRWLockExclusive(&guard);return 1;
}

int career_cheats_training_multiplier(int value){
    if(value!=1&&value!=2&&value!=5&&value!=10)return 0;
    AcquireSRWLockExclusive(&guard);reward_multiplier=value;snapshot.training_multiplier=(uint32_t)value;
    if(snapshot.ready)training_rewards_refresh(collection);else if(value==1)reward_restore();
    ReleaseSRWLockExclusive(&guard);return 1;
}
int career_cheats_grade_a(void){
    AcquireSRWLockExclusive(&guard);GradePack fields_now[5];uint32_t refs[5]={0};int ok=0;
    if(snapshot.ready&&snapshot.status!=CAREER_PENDING&&grade_fields(collection,fields_now,refs)>1){
        memcpy(expected_grade_ids,refs,sizeof(refs));snapshot.kind=CAREER_GRADES;snapshot.amount=0;queued=GetTickCount64();snapshot.status=CAREER_PENDING;InterlockedExchange64(&next_probe,0);ok=1;
    }
    ReleaseSRWLockExclusive(&guard);return ok;
}
