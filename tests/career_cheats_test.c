#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#undef NDEBUG
#include <assert.h>
#include <stdio.h>
#define CAREER_CHEATS_TEST
#include "../src/career_cheats.c"
static unsigned writes,dirty_calls;
static unsigned char root[64],registry[80],registry_schema[80],registry_fields[5*80],owner[0x130];
static uintptr_t stores[526];
static unsigned char store[2][80],schemas[2][80],descriptors[2][64*80],buffers[2][2048];
static void putptr(void *where,const void *value){uintptr_t p=(uintptr_t)value;memcpy(where,&p,8);}
static void put32(void *where,uint32_t value){memcpy(where,&value,4);}
static void label(unsigned char *where,const char *value){
    if(strlen(value)<16){memcpy(where,value,strlen(value)+1);where[15]=0;}
    else{putptr(where,value);where[15]=128;}
}
static uint32_t __attribute__((sysv_abi)) mock_get(uintptr_t s,uint32_t row,uint32_t index){
    assert(row==0);int k=s==(uintptr_t)store[0]?0:1;assert(index==(k==0?48u:10u));CareerField f;assert(describe(s,k,&f));return f.value;
}
static void __attribute__((sysv_abi)) mock_set(uintptr_t s,uint32_t row,uint32_t index,uint32_t value){
    assert(row==0);int k=s==(uintptr_t)store[0]?0:1;CareerField f;assert(describe(s,k,&f)&&f.index==index);uint32_t word=0;assert(u32(f.address,&word));
    uint32_t mask=f.maximum<<f.shift;word=(word&~mask)|((value<<f.shift)&mask);memcpy((void*)f.address,&word,4);++writes;
}
static void __attribute__((sysv_abi)) mock_dirty(uintptr_t s){assert(s==(uintptr_t)store[0]||s==(uintptr_t)store[1]);++dirty_calls;}
static uint32_t value(int k){CareerField f;assert(describe((uintptr_t)store[k],k,&f));return f.value;}
static void __attribute__((sysv_abi)) mock_set_packed(uintptr_t p,uint32_t row,uint32_t index,uint32_t amount){
    assert(p==(uintptr_t)buffers[0]||p==(uintptr_t)buffers[1]);mock_set((uintptr_t)store[p==(uintptr_t)buffers[0]?0:1],row,index,amount);
}
static unsigned char *page(unsigned char *base,uintptr_t offset){unsigned char *p=base+(offset&~(uintptr_t)4095);assert(VirtualAlloc(p,4096,MEM_COMMIT,PAGE_EXECUTE_READWRITE)==p);return base+offset;}
static void jump(unsigned char *p,void *fn){p[0]=0x48;p[1]=0xb8;putptr(p+2,fn);p[10]=0xff;p[11]=0xe0;}
static void process(void){process_user((uintptr_t)root,-1,1);}
int main(void){
    career_cheats_init(0x900000000ULL);game_get=mock_get;game_set=mock_set;game_dirty=mock_dirty;
    putptr(root,(void*)(guest+0x9c38410));putptr(root+0x10,registry);
    putptr(registry,(void*)(guest+0x9bf7be8));putptr(registry+8,registry_schema);putptr(registry+0x10,owner);
    putptr(registry_schema+0x30,registry_fields);putptr(registry_schema+0x38,registry_fields+sizeof(registry_fields));
    label(registry_fields,"RegisterEvent");label(registry_fields+4*80,"UserList");
    putptr(owner+0x108,stores);putptr(owner+0x110,stores+526);stores[525]=(uintptr_t)store[0];stores[109]=(uintptr_t)store[1];
    for(int k=0;k<2;++k){
        putptr(store[k],(void*)(guest+0x9bf7be8));putptr(store[k]+8,schemas[k]);putptr(store[k]+0x10,owner);putptr(store[k]+0x30,buffers[k]);
        putptr(schemas[k]+0x30,descriptors[k]);putptr(schemas[k]+0x38,descriptors[k]+(k==0?64:16)*80);
        unsigned i=k==0?48:10;unsigned char *d=descriptors[k]+i*80;uint16_t bits=k==0?31:14;memcpy(d+0x30,&bits,2);put32(d+0x38,2);
        put32(buffers[k]+0xc,4);put32(buffers[k]+0x10,1);put32(buffers[k]+0x18,80);put32(buffers[k]+0x20+i*4,k==0?0:5);
        put32(buffers[k]+320,k==0?(0x80000000u|12109126):(0xa000001bu|(16<<5)));
    }
    label(descriptors[0],"IsUserControlled");label(descriptors[0]+38*80,"RatingCashAvailable");label(descriptors[0]+48*80,"Cash");label(descriptors[0]+61*80,"RatingProgress");
    label(descriptors[1]+2*80,"LastSkillPointsEarned");label(descriptors[1]+10*80,"SkillPoints");label(descriptors[1]+11*80,"TotalXp");
    assert(!career_cheats_add(0,1000));process();CareerSnapshot s;career_cheats_snapshot(&s);
    assert(s.ready&&s.balance[0]==12109126&&s.balance[1]==16&&s.maximum[1]==16383);
    assert(career_cheats_add(0,1000));assert(!career_cheats_add(0,1000)&&!career_cheats_add(1,5));
    assert(writes==0&&value(0)==12109126);process_user((uintptr_t)root,99,1);assert(!writes);
    process();assert(value(0)==12110126&&value(1)==16&&writes==1&&dirty_calls==1);
    for(int i=0;i<10;++i)process();assert(writes==1); /* One shot, no balance freeze. */
    assert(career_cheats_add(1,10));process();assert(value(1)==26&&writes==2&&dirty_calls==2);
    uint32_t word=0;memcpy(&word,buffers[0]+320,4);assert(word&0x80000000u);memcpy(&word,buffers[1]+320,4);assert((word&~(16383u<<5))==(0xa000001bu&~(16383u<<5)));
    assert(!career_cheats_add(1,16383)&&snapshot.status==CAREER_LIMIT&&writes==2);
    assert(!career_cheats_add(-1,1)&&!career_cheats_add(2,1)&&!career_cheats_add(0,0));
    assert(career_cheats_add(0,1000));queued=GetTickCount64()-30001;process();assert(snapshot.status==CAREER_CHANGED&&writes==2);
    assert(career_cheats_add(1,5));process_user((uintptr_t)root,-1,0);assert(snapshot.status==CAREER_CHANGED&&!snapshot.ready&&writes==2);process();
    assert(career_cheats_add(1,5));unsigned char alternate[64];memcpy(alternate,root,sizeof(root));process_user((uintptr_t)alternate,-1,1);assert(snapshot.status==CAREER_CHANGED&&writes==2);process();
    assert(career_cheats_add(0,1000));unsigned char replacement[2048];memcpy(replacement,buffers[0],sizeof(replacement));putptr(store[0]+0x30,replacement);process();assert(snapshot.status==CAREER_CHANGED&&writes==2);
    putptr(store[0]+0x30,buffers[0]);process();assert(career_cheats_add(0,1000));descriptors[0][48*80]='X';process();assert(!snapshot.ready&&snapshot.status==CAREER_CHANGED&&writes==2);label(descriptors[0]+48*80,"Cash");process();
    /* A larger number of rows must never be guessed as the current player. */
    put32(buffers[0]+0x10,2);process();assert(!snapshot.ready&&!career_cheats_add(0,1)&&writes==2);put32(buffers[0]+0x10,1);process();
    /* Slot layout changes are resolved by checked names, not fixed indices. */
    stores[5]=stores[525];stores[525]=0;stores[7]=stores[109];stores[109]=0;process();assert(snapshot.ready&&value(1)==26);
    /* Exercise the actual checked hook and SysV trampoline with a synthetic
       guest image. This runs the real installer, entry and return conventions. */
    unsigned char *base=VirtualAlloc(NULL,0xc000000,MEM_RESERVE,PAGE_NOACCESS);assert(base);career_cheats_init((uintptr_t)base);
    putptr(root,(void*)(guest+0x9c38410));putptr(registry,(void*)(guest+0x9bf7be8));
    for(int k=0;k<2;++k)putptr(store[k],(void*)(guest+0x9bf7be8));
    unsigned char *user_code=page(base,0x5c015d0);memcpy(user_code,user_signature,sizeof(user_signature));
    uintptr_t cookie=123,canary=(uintptr_t)&cookie;int32_t displacement;memcpy(&displacement,user_code+20,4);putptr(page(base,0x5c015d0+24+displacement),(void*)canary);
    const unsigned char end_code[]={0x48,0x89,0xf8,0x48,0x83,0xc4,0x28,0x5b,0x41,0x5c,0x41,0x5d,0x41,0x5e,0x41,0x5f,0x5d,0xc3};memcpy(user_code+24,end_code,sizeof(end_code));
    const unsigned char get_code[]={0x48,0x8b,0x47,0x08,0x89,0xd1,0x4c,0x8d,0x04,0x89,0x49,0xc1,0xe0,0x04};unsigned char *code=page(base,0x56dcf80);memcpy(code,get_code,sizeof(get_code));jump(code+sizeof(get_code),(void*)mock_get);
    const unsigned char set_code[]={0x48,0x8b,0x47,0x08,0x41,0x89,0xd0,0x48,0x8b,0x7f,0x30};code=page(base,0x56dd300);memcpy(code,set_code,sizeof(set_code));jump(code+sizeof(set_code),(void*)mock_set_packed);
    const unsigned char dirty_code[]={0x48,0x8b,0x47,0x10,0xff,0x80,0xe8,0,0,0,0xc3};memcpy(page(base,0x56ccef0),dirty_code,sizeof(dirty_code));
    unsigned char flag[16]={0};putptr(flag+8,"Fighter_Disable_StaminaLoss");putptr(page(base,0xb452958),flag);FlushInstructionCache(GetCurrentProcess(),base,0xc000000);
    career_cheats_tick();assert(original_user);UserFn run=(UserFn)(void*)user_code;uintptr_t selected=1;assert(run(&selected,(uintptr_t)root,-1)==&selected&&snapshot.ready);
    uint32_t before=value(1),revision=0;assert(u32((uintptr_t)owner+0xe8,&revision));assert(career_cheats_add(1,1));run(&selected,(uintptr_t)root,-1);
    assert(snapshot.status==CAREER_ADDED&&value(1)==before+1);uint32_t after_revision=0;assert(u32((uintptr_t)owner+0xe8,&after_revision)&&after_revision==revision+1);
    run(&selected,(uintptr_t)root,-1);assert(value(1)==before+1);
    uint64_t probes_before=probe_count,started=GetTickCount64();
    for(int i=0;i<100000;++i)run(&selected,(uintptr_t)root,-1);
    uint64_t elapsed=GetTickCount64()-started;assert(probe_count-probes_before<=elapsed/250+2);
    assert(value(1)==before+1&&cached_slot[0]==5&&cached_slot[1]==7);
    printf("PASS: 100,000 career lookups in %llu ms, %llu probes; relocated schemas cached and additions bypass the idle delay.\n",(unsigned long long)elapsed,(unsigned long long)(probe_count-probes_before));
    puts("PASS: real checked SysV career hook, native setter/dirty calls, career identity/schema checks, exact money/skill fields, one-time additions, no auto-repeat, adjacent-bit preservation, amount limits, pending cancellation, changed career/buffers and schema relocation.");return 0;
}
