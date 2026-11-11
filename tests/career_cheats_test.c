#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#undef NDEBUG
#include <assert.h>
#include <stdio.h>
#define CAREER_CHEATS_TEST
#include "../src/career_cheats.c"
static unsigned writes,dirty_calls;
static uintptr_t extra_dirty_store;
static unsigned char other_store[80],other_buffer[2048];
static unsigned char camp_store[80],camp_schema[80],camp_descriptors[32*80],camp_buffer[2048];
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
    if(index==2){uintptr_t packed=pointer(s+0x30);uint32_t stride=0,start=0,bit=0,word=0;
        assert(u32(packed+12,&stride)&&u32(packed+24,&start)&&u32(packed+40,&bit));assert(u32(packed+4*(start+row*stride+bit/32),&word));return ((word>>(bit%32))&7)-1;}

    if(s==(uintptr_t)camp_store){CareerField f;assert(index==10&&training_field(s,row,index,10,&f));return f.value;}
    assert(row==0);int k=s==(uintptr_t)store[1]?1:(index==24?CAREER_LONGEVITY:index==50?CAREER_FITNESS:0);assert(index==(k==0?48u:k==CAREER_FITNESS?50u:k==CAREER_LONGEVITY?24u:10u));CareerField f;assert(describe(s,k,&f));return f.value;
}
static void __attribute__((sysv_abi)) mock_set(uintptr_t s,uint32_t row,uint32_t index,uint32_t value){
    if(s==(uintptr_t)camp_store){CareerField f;uint32_t word=0;assert(index==10&&training_field(s,row,index,10,&f)&&u32(f.address,&word));word=(word&~(f.maximum<<f.shift))|((value<<f.shift)&(f.maximum<<f.shift));memcpy((void*)f.address,&word,4);++writes;return;}
    assert(row==0);int k=s==(uintptr_t)store[1]?1:(index==24?CAREER_LONGEVITY:index==50?CAREER_FITNESS:0);CareerField f;assert(describe(s,k,&f)&&f.index==index);uint32_t word=0;assert(u32(f.address,&word));
    uint32_t mask=f.maximum<<f.shift;word=(word&~mask)|((value<<f.shift)&mask);memcpy((void*)f.address,&word,4);++writes;
}
static void __attribute__((sysv_abi)) mock_dirty(uintptr_t s){assert(s==(uintptr_t)store[0]||s==(uintptr_t)store[1]||s==(uintptr_t)camp_store||s==extra_dirty_store);++dirty_calls;}
static uint32_t value(int k){CareerField f;assert(describe((uintptr_t)store[k==CAREER_FITNESS||k==CAREER_LONGEVITY?0:k],k,&f));return f.value;}
static void __attribute__((sysv_abi)) mock_set_packed(uintptr_t p,uint32_t row,uint32_t index,uint32_t amount){
    if(index==2){uint32_t stride=0,start=0,bit=0,word=0;assert(amount==4);
        assert(u32(p+12,&stride)&&u32(p+24,&start)&&u32(p+40,&bit));uintptr_t address=p+4*(start+row*stride+bit/32);assert(u32(address,&word));
        put32((void*)address,(word&~(7u<<(bit%32)))|((amount+1)<<(bit%32)));++writes;return;}

    if(p==(uintptr_t)camp_buffer){mock_set((uintptr_t)camp_store,row,index,amount);return;}
    if(p==(uintptr_t)other_buffer){mock_set((uintptr_t)other_store,row,index,amount);return;}
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
    label(descriptors[0]+50*80,"Fitness");uint16_t fit_bits=15;memcpy(descriptors[0]+50*80+0x30,&fit_bits,2);put32(descriptors[0]+50*80+0x38,2);put32(buffers[0]+0x20+50*4,49);put32(buffers[0]+324,(7900u<<17)|9895);
    putptr(camp_store,(void*)(guest+0x9bf7be8));putptr(camp_store+8,camp_schema);putptr(camp_store+0x10,owner);putptr(camp_store+0x30,camp_buffer);put32(camp_store+0x2c,0x1016);stores[22]=(uintptr_t)camp_store;
    putptr(camp_schema+0x30,camp_descriptors);putptr(camp_schema+0x38,camp_descriptors+sizeof(camp_descriptors));
    label(camp_descriptors+2*80,"CampLength");label(camp_descriptors+10*80,"CurrentPoints");label(camp_descriptors+11*80,"CurrentWeek");label(camp_descriptors+25*80,"Sessions");
    put32(camp_buffer+0xc,4);put32(camp_buffer+0x10,4);put32(camp_buffer+0x18,64);
    for(int i=0;i<3;++i){unsigned index=i==0?2:i==1?10:11;uint16_t n=index==10?10:7;memcpy(camp_descriptors+index*80+0x30,&n,2);put32(camp_descriptors+index*80+0x38,2);put32(camp_buffer+0x20+index*4,index==2?32:index==10?0:16);}
    for(int row=0;row<4;++row){put32(camp_buffer+(64+row*4)*4,0xa0000000u+(row==2?0:row+1));put32(camp_buffer+(65+row*4)*4,4);}
    label(descriptors[0]+63*80,"TrainingCamp");uint16_t ref_bits=65535;memcpy(descriptors[0]+63*80+0x30,&ref_bits,2);put32(descriptors[0]+63*80+0x38,7);putptr(descriptors[0]+63*80+0x48,camp_schema);put32(buffers[0]+0x20+63*4,64);put32(buffers[0]+328,0x202c0002);
    label(descriptors[0]+24*80,"Longevity");uint16_t long_bits=17;memcpy(descriptors[0]+24*80+0x30,&long_bits,2);put32(descriptors[0]+24*80+0x38,2);put32(buffers[0]+0x20+24*4,32);
    label(descriptors[0]+22*80,"ID");label(descriptors[0]+4*80,"AppearanceId");uint16_t id_bits=16;memcpy(descriptors[0]+22*80+0x30,&id_bits,2);put32(descriptors[0]+22*80+0x34,(uint32_t)-3);put32(descriptors[0]+22*80+0x38,2);put32(buffers[0]+0x20+22*4,96);put32(buffers[0]+332,20003);
    assert(career_cheats_protect_longevity(0)&&career_cheats_protect_longevity(1)&&!snapshot.longevity_enabled&&snapshot.longevity_requested);assert(career_cheats_protect_longevity(0));
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
    assert(snapshot.fitness_ready&&snapshot.fitness==7900);unsigned before_fitness_writes=writes;uint32_t adjacent=0;assert(u32((uintptr_t)buffers[0]+324,&adjacent));
    assert(career_cheats_restore_fitness()&&writes==before_fitness_writes);process();assert(snapshot.status==CAREER_ADDED&&snapshot.fitness==10000&&value(CAREER_FITNESS)==10000&&writes==before_fitness_writes+1);
    uint32_t fitness_word=0;assert(u32((uintptr_t)buffers[0]+324,&fitness_word));assert((fitness_word&~(32767u<<17))==(adjacent&~(32767u<<17)));
    for(int i=0;i<10;++i)process();assert(writes==before_fitness_writes+1);
    assert(career_cheats_restore_fitness());queued=GetTickCount64()-30001;process();assert(snapshot.status==CAREER_CHANGED&&writes==before_fitness_writes+1);
    assert(career_cheats_restore_fitness());descriptors[0][50*80]='X';process();assert(snapshot.status==CAREER_CHANGED&&!snapshot.fitness_ready&&writes==before_fitness_writes+1&&snapshot.ready);
    assert(!career_cheats_restore_fitness());label(descriptors[0]+50*80,"Fitness");process();
    assert(snapshot.training_ready&&snapshot.training_days==0&&snapshot.training_week==1);unsigned before_days=writes;
    assert(career_cheats_restore_days()&&writes==before_days);process();assert(snapshot.status==CAREER_ADDED&&snapshot.training_days==7&&writes==before_days+1);
    uint32_t row0=0,row2=0;assert(u32((uintptr_t)camp_buffer+256,&row0)&&u32((uintptr_t)camp_buffer+288,&row2));assert((row0&1023)==1&&(row2&1023)==7&&(row2&~1023u)==0xa0000000u);
    for(int i=0;i<10;++i)process();assert(writes==before_days+1);
    assert(career_cheats_restore_days());put32(camp_buffer+288,row2|(1u<<16));process();assert(snapshot.status==CAREER_CHANGED&&writes==before_days+1);put32(camp_buffer+288,row2);process();
    assert(career_cheats_restore_days());put32(buffers[0]+328,0x202c0001);process();assert(snapshot.status==CAREER_CHANGED&&writes==before_days+1);put32(buffers[0]+328,0x202c0002);process();
    assert(career_cheats_restore_days());descriptors[0][63*80]='X';process();assert(snapshot.status==CAREER_CHANGED&&!snapshot.training_ready&&!career_cheats_restore_days());label(descriptors[0]+63*80,"TrainingCamp");process();
    put32(camp_buffer+292,0);process();assert(!snapshot.training_ready&&!career_cheats_restore_days());put32(camp_buffer+292,4);process();assert(snapshot.training_ready);
    /* Exercise the actual checked hook and SysV trampoline with a synthetic
       guest image. This runs the real installer, entry and return conventions. */
    unsigned char *base=VirtualAlloc(NULL,0xc000000,MEM_RESERVE,PAGE_NOACCESS);assert(base);career_cheats_init((uintptr_t)base);
    putptr(root,(void*)(guest+0x9c38410));putptr(registry,(void*)(guest+0x9bf7be8));
    for(int k=0;k<2;++k)putptr(store[k],(void*)(guest+0x9bf7be8));putptr(camp_store,(void*)(guest+0x9bf7be8));
    unsigned char *user_code=page(base,0x5c015d0);memcpy(user_code,user_signature,sizeof(user_signature));
    uintptr_t cookie=123,canary=(uintptr_t)&cookie;int32_t displacement;memcpy(&displacement,user_code+20,4);putptr(page(base,0x5c015d0+24+displacement),(void*)canary);
    const unsigned char end_code[]={0x48,0x89,0xf8,0x48,0x83,0xc4,0x28,0x5b,0x41,0x5c,0x41,0x5d,0x41,0x5e,0x41,0x5f,0x5d,0xc3};memcpy(user_code+24,end_code,sizeof(end_code));
    const unsigned char get_code[]={0x48,0x8b,0x47,0x08,0x89,0xd1,0x4c,0x8d,0x04,0x89,0x49,0xc1,0xe0,0x04};unsigned char *code=page(base,0x56dcf80);memcpy(code,get_code,sizeof(get_code));jump(code+sizeof(get_code),(void*)mock_get);
    const unsigned char set_code[]={0x48,0x8b,0x47,0x08,0x41,0x89,0xd0,0x48,0x8b,0x7f,0x30,0x4f,0x8d,0x04,0x80};code=page(base,0x56dd300);memcpy(code,set_code,sizeof(set_code));jump(code+sizeof(set_code),(void*)mock_set_packed);
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
    assert(career_cheats_restore_days());run(&selected,(uintptr_t)root,-1);assert(snapshot.status==CAREER_ADDED&&snapshot.training_days==7);
    assert(snapshot.longevity_ready&&!snapshot.longevity_enabled&&value(CAREER_LONGEVITY)==9895);
    assert(career_cheats_protect_longevity(1)&&value(CAREER_LONGEVITY)==9895);
    game_set((uintptr_t)store[0],0,24,9800);assert(value(CAREER_LONGEVITY)==9895&&value(CAREER_FITNESS)==10000&&InterlockedCompareExchange(&longevity_blocked,0,0)==1);
    game_set((uintptr_t)store[0],0,24,10000);assert(value(CAREER_LONGEVITY)==10000);
    game_set((uintptr_t)store[0],0,50,9000);assert(value(CAREER_LONGEVITY)==10000&&value(CAREER_FITNESS)==9000);
    memcpy(other_store,store[0],80);memcpy(other_buffer,buffers[0],2048);putptr(other_store+0x30,other_buffer);
    game_set((uintptr_t)other_store,0,24,5000);CareerField other;assert(describe((uintptr_t)other_store,CAREER_LONGEVITY,&other)&&other.value==5000&&value(CAREER_LONGEVITY)==10000);
    assert(career_cheats_protect_longevity(0));game_set((uintptr_t)store[0],0,24,9900);assert(value(CAREER_LONGEVITY)==9900);
    assert(career_cheats_protect_longevity(1));game_set((uintptr_t)store[0],0,24,9800);assert(value(CAREER_LONGEVITY)==9900);
    put32(buffers[0]+332,20004);game_set((uintptr_t)store[0],0,24,9700);assert(value(CAREER_LONGEVITY)==9700&&!InterlockedCompareExchange(&longevity_on,0,0));
    process();assert(career_cheats_protect_longevity(1));career_cheats_cancel();game_set((uintptr_t)store[0],0,24,9600);assert(value(CAREER_LONGEVITY)==9600&&!snapshot.longevity_enabled);
    assert(career_cheats_protect_longevity(1));process_user((uintptr_t)root,-1,0);assert(!snapshot.ready&&!snapshot.longevity_enabled&&snapshot.longevity_requested);
    process();assert(snapshot.ready&&snapshot.longevity_enabled&&snapshot.longevity_requested);game_set((uintptr_t)store[0],0,24,9500);assert(value(CAREER_LONGEVITY)==9600);
    career_cheats_cancel();assert(!snapshot.longevity_requested&&!snapshot.longevity_enabled);game_set((uintptr_t)store[0],0,24,9500);assert(value(CAREER_LONGEVITY)==9500);
    unsigned char master[0x160]={0},reward_record[80]={0},reward_schema_data[80]={0},reward_descriptors[20*80]={0},reward_buffer[2048]={0};uintptr_t master_stores[213]={0};
    putptr(master,(void*)(guest+0x9bf6ae0));putptr(owner+0xf0,master);putptr(master+0x148,master_stores);putptr(master+0x150,master_stores+213);master_stores[211]=(uintptr_t)reward_record;
    putptr(reward_record,(void*)(guest+0x9bf7be8));putptr(reward_record+8,reward_schema_data);putptr(reward_record+0x10,master);putptr(reward_record+0x30,reward_buffer);
    putptr(reward_schema_data+0x30,reward_descriptors);putptr(reward_schema_data+0x38,reward_descriptors+sizeof(reward_descriptors));
    label(reward_descriptors+10*80,"SkillPointsReward");label(reward_descriptors+13*80,"TargetPositive");label(reward_descriptors+19*80,"TrainingStationId");uint16_t reward_bits=9;memcpy(reward_descriptors+10*80+0x30,&reward_bits,2);put32(reward_descriptors+10*80+0x38,2);
    put32(reward_buffer+0xc,1);put32(reward_buffer+0x10,3);put32(reward_buffer+0x18,80);put32(reward_buffer+0x20+10*4,0);
    put32(reward_buffer+320,0xab000002);put32(reward_buffer+324,0xab000004);put32(reward_buffer+328,0xab000008);
    uint32_t skills_before=value(1);assert(career_cheats_training_multiplier(2));uint32_t reward_word=0;assert(u32((uintptr_t)reward_buffer+320,&reward_word)&&reward_word==0xab000004);
    assert(career_cheats_training_multiplier(2));assert(u32((uintptr_t)reward_buffer+320,&reward_word)&&reward_word==0xab000004);assert(career_cheats_training_multiplier(5));assert(u32((uintptr_t)reward_buffer+320,&reward_word)&&reward_word==0xab00000a);
    assert(!career_cheats_training_multiplier(7));assert(career_cheats_training_multiplier(1));assert(u32((uintptr_t)reward_buffer+320,&reward_word)&&reward_word==0xab000002);assert(value(1)==skills_before);
    reward_restore();put32(reward_buffer+328,0xab000064);assert(career_cheats_training_multiplier(10));assert(u32((uintptr_t)reward_buffer+328,&reward_word)&&reward_word==0xab0001ff);
    career_cheats_cancel();assert(u32((uintptr_t)reward_buffer+328,&reward_word)&&reward_word==0xab000064);
    unsigned char grade_record[80]={0},grade_schema_data[80]={0},grade_descriptors[4*80]={0},grade_buffer[1024]={0},enum_schema[80]={0},enum_fields[6*80]={0};
    unsigned char station_record[80]={0},station_schema[80]={0},station_descriptors[23*80]={0},station_buffer[1024]={0};
    stores[301]=(uintptr_t)grade_record;stores[326]=(uintptr_t)station_record;extra_dirty_store=(uintptr_t)grade_record;
    putptr(grade_record,(void*)(guest+0x9bf7be8));putptr(grade_record+8,grade_schema_data);putptr(grade_record+0x10,owner);putptr(grade_record+0x30,grade_buffer);putptr(grade_schema_data+0x30,grade_descriptors);putptr(grade_schema_data+0x38,grade_descriptors+sizeof(grade_descriptors));
    label(grade_descriptors,"Challenges");label(grade_descriptors+2*80,"Grade");label(grade_descriptors+3*80,"Name");uint16_t grade_bits=3;memcpy(grade_descriptors+2*80+0x30,&grade_bits,2);put32(grade_descriptors+2*80+0x34,UINT32_MAX);put32(grade_descriptors+2*80+0x38,3);putptr(grade_descriptors+2*80+0x48,enum_schema);
    putptr(enum_schema+0x30,enum_fields);putptr(enum_schema+0x38,enum_fields+sizeof(enum_fields));label(enum_fields,"Invalid");label(enum_fields+5*80,"A");put32(enum_fields+5*80+0x40,4);
    put32(grade_buffer+0xc,1);put32(grade_buffer+0x10,6);put32(grade_buffer+0x18,64);put32(grade_buffer+0x20+2*4,0);
    for(unsigned i=0;i<6;++i)put32(grade_buffer+256+i*4,0xab000000u|(i==2?3:i==4?4:i==5?5:0));
    putptr(station_record,(void*)(guest+0x9bf7be8));putptr(station_record+8,station_schema);putptr(station_record+0x10,owner);putptr(station_record+0x30,station_buffer);putptr(station_schema+0x30,station_descriptors);putptr(station_schema+0x38,station_descriptors+sizeof(station_descriptors));label(station_descriptors+7*80,"CurrentChallengePack");label(station_descriptors+14*80,"Id");put32(station_buffer+0xc,1);put32(station_buffer+0x10,5);put32(station_buffer+0x18,64);put32(station_buffer+0x20+7*4,0);
    unsigned active_rows[]={0,1,3,0,1};for(unsigned i=0;i<5;++i)put32(station_buffer+256+i*4,0x225a0000u|active_rows[i]);
    unsigned char second_grade_record[80]={0};HANDLE grade_mapping=CreateFileMappingW(INVALID_HANDLE_VALUE,NULL,PAGE_READWRITE,0,4096,NULL);assert(grade_mapping);
    unsigned char *second_grade_seed=MapViewOfFile(grade_mapping,FILE_MAP_READ|FILE_MAP_WRITE,0,0,4096),*second_grade_buffer=MapViewOfFile(grade_mapping,FILE_MAP_READ,0,0,4096);assert(second_grade_seed&&second_grade_buffer);
    memcpy(second_grade_record,grade_record,sizeof(grade_record));putptr(second_grade_record+0x30,second_grade_buffer);stores[276]=(uintptr_t)second_grade_record;
    put32(second_grade_seed+0xc,1);put32(second_grade_seed+0x10,2);put32(second_grade_seed+0x18,64);put32(second_grade_seed+0x20+2*4,0);
    put32(second_grade_seed+256,0xcd000000);put32(second_grade_seed+260,0xcd000002);
    put32(station_buffer+256,0x22280000); /* Current pack is unattempted; previous pack has a grade. */
    uint32_t grade_epoch=0;assert(u32((uintptr_t)owner+0xe8,&grade_epoch));
    process();assert(snapshot.sparring_grades_ready&&snapshot.sparring_grades_completed==7);
    putptr(second_grade_record+0x10,NULL);assert(!career_cheats_grade_a());putptr(second_grade_record+0x10,owner);
    assert(career_cheats_grade_a());process();assert(snapshot.status==CAREER_ADDED);
    uint32_t second_word=0;assert(u32((uintptr_t)second_grade_buffer+256,&second_word)&&second_word==0xcd000005);
    assert(u32((uintptr_t)second_grade_buffer+260,&second_word)&&second_word==0xcd000005);
    for(unsigned i=0;i<6;++i){uint32_t word=0;assert(u32((uintptr_t)grade_buffer+256+i*4,&word));assert(word==0xab000005u);}
    assert(value(1)==skills_before&&!career_cheats_grade_a());
    MEMORY_BASIC_INFORMATION grade_memory;assert(VirtualQuery(second_grade_buffer,&grade_memory,sizeof(grade_memory))&&grade_memory.Protect==PAGE_READONLY);
    assert(u32((uintptr_t)second_grade_seed+256,&second_word)&&second_word==0xcd000000);
    assert(u32((uintptr_t)second_grade_seed+260,&second_word)&&second_word==0xcd000002);
    uint32_t updated_epoch=0;assert(u32((uintptr_t)owner+0xe8,&updated_epoch)&&updated_epoch==grade_epoch+7);
    assert(UnmapViewOfFile(second_grade_buffer)&&UnmapViewOfFile(second_grade_seed)&&CloseHandle(grade_mapping));
    puts("PASS: sparring reward definition multipliers, non-compounding changes, Off/reset restoration, packed-field limits and neighbor bits; confirmed all-pack A grades use the normal setter/dirty tracking, support read-only mapped records, restore protection, preserve backing views and point balances, and reject foreign owners.");
    puts("PASS: real checked SysV longevity setter guard; immediate decrease blocking, allowed increases, immediate Off, unrelated fighters/fitness preserved, career identity change and reset disable.");
    printf("PASS: 100,000 career lookups in %llu ms, %llu probes; relocated schemas cached and additions bypass the idle delay.\n",(unsigned long long)elapsed,(unsigned long long)(probe_count-probes_before));
    puts("PASS: real checked SysV career hook, native setter/dirty calls, career identity/schema checks, exact money/skill/fitness fields, one-time fitness/day restoration, nonzero active camp rows, camp/week changes, schema rejection, one-time additions, no auto-repeat, adjacent-bit preservation, amount limits, pending cancellation, changed career/buffers and schema relocation.");return 0;
}
