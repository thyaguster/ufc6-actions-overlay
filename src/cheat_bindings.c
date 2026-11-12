/* Per-fighter updates on the game's own synchronization thread. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string.h>
#include "cheat_bindings.h"
static const char *binding_names[]={"Fighter_Disable_StaminaLoss","Fighter_Disable_HealthLoss","Fighter_ControllerInputMask"};
static int binding_read(uintptr_t address,void *out,size_t size){SIZE_T n=0;return address>=0x10000&&ReadProcessMemory(GetCurrentProcess(),(void*)address,out,size,&n)&&n==size;}
static int binding_write(uintptr_t address,unsigned char value){
    MEMORY_BASIC_INFORMATION m;SIZE_T n=0;
    if(!VirtualQuery((void*)address,&m,sizeof(m))||m.State!=MEM_COMMIT||(m.Protect&PAGE_GUARD)||!(m.Protect&(PAGE_READWRITE|PAGE_WRITECOPY)))return 0;
    return WriteProcessMemory(GetCurrentProcess(),(void*)address,&value,1,&n)&&n==1;
}
static int binding_kind(uintptr_t record){
    uintptr_t object=0,name=0;int32_t first=0;uint64_t stride=0;uint32_t kind=0;char text[32]={0};
    if(!binding_read(record+0x10,&object,8)||!object||!binding_read(object+8,&name,8)||!binding_read(name,text,32)||
       !binding_read(record+0x20,&first,4)||first!=-1||!binding_read(record+0x28,&stride,8)||stride!=4||
       !binding_read(record+0x34,&kind,4))return -1;
    for(int i=0;i<3;++i)if(kind==(i==2?6u:1u)&&!strcmp(text,binding_names[i]))return i;
    return -1;
}
void cheat_bindings_apply(CheatBindingCache *cache,uintptr_t binder,unsigned first,unsigned count,const unsigned command[2],CheatBindingFrame *frame){
    uintptr_t begin=0,end=0;memset(frame,0,sizeof(*frame));frame->player_one=-1;
    if(!binding_read(binder+0x88,&begin,8)||!binding_read(binder+0x90,&end,8)||!begin||end<begin||
       (end-begin)%0x68||(end-begin)/0x68>16384)return;
    size_t length=(end-begin)/0x68;
    if(first>length||count>length-first)return;
    if(cache->owner!=binder||cache->begin!=begin||cache->end!=end){
        if(cache->owner!=binder)cache->physical_owned[0]=cache->physical_owned[1]=0;
        cache->owner=binder;cache->begin=begin;cache->end=end;for(int i=0;i<3;++i)cache->index[i]=SIZE_MAX;
        for(size_t i=0;i<length;++i){int kind=binding_kind(begin+i*0x68);if(kind>=0)cache->index[kind]=i;}
    }
    /* The game's fighter indices need not match controller order. Resolve
       Player 1 from the controller-input mask, never from camera position. */
    if(cache->index[2]>=length)return;
    uintptr_t mapping=begin+cache->index[2]*0x68,map_begin=0,map_end=0;int32_t input[2]={-1,-1};
    if(binding_kind(mapping)!=2||!binding_read(mapping+0x38,&map_begin,8)||!binding_read(mapping+0x40,&map_end,8)||map_end<map_begin||(map_end-map_begin)%16||map_end-map_begin>256)return;
    if(map_begin==map_end)return; /* Inactive/menu worlds must not replace live-fight observations. */
    for(uintptr_t node=map_begin;node<map_end;node+=16){uintptr_t field=0;int32_t player=-1,value=-1;
        if(!binding_read(node,&field,8)||!binding_read(node+8,&player,4)||player<0||player>1||!binding_read(field,&value,4)||value<0||value>0xffff)return;
        if(input[player]!=-1&&input[player]!=value)return;input[player]=value;
    }
    frame->fighters_ready=input[0]>=0&&input[1]>=0;
    frame->player_one=0;
    if(input[0]<0||input[1]<0||!!(input[0]&1)==!!(input[1]&1))return;
    frame->player_one=(input[0]&1)?1:2;
    for(int i=0;i<2;++i){
        size_t index=cache->index[i];if(index<first||index>=first+count)continue;
        uintptr_t record=begin+index*0x68,nodes=0,last=0;
        if(binding_kind(record)!=i){cache->end=0;continue;}
        if(!binding_read(record+0x38,&nodes,8)||!binding_read(record+0x40,&last,8)||last<nodes||(last-nodes)%16||(last-nodes)/16>16)continue;
        /* Validate every node before writing any: non-fighter/default indices
           and malformed boolean fields make the whole record unavailable. */
        unsigned length_nodes=(unsigned)((last-nodes)/16);int valid=1;
        for(unsigned j=0;j<length_nodes;++j){
            uintptr_t field=0;int32_t player=-1;unsigned char value=0;
            if(!binding_read(nodes+j*16,&field,8)||!binding_read(nodes+j*16+8,&player,4)||player<0||player>1||
               !binding_read(field,&value,1)||value>1){valid=0;break;}
            frame->field[i][j]=(CheatBindingField){field,(unsigned)player};
        }
        if(!valid)continue;
        frame->count[i]=length_nodes;
        unsigned logical_controlled=(command[i]>>2)&1,logical_on=(command[i]>>4)&1;
        if(logical_controlled)cache->physical_owned[i]|=(unsigned)frame->player_one;
        unsigned controlled=cache->physical_owned[i],on=logical_on?(unsigned)frame->player_one:0;
        for(unsigned j=0;j<length_nodes;++j){
            CheatBindingField field=frame->field[i][j];unsigned bit=1u<<field.player;
            if(controlled&bit){unsigned char value=0;unsigned char desired=(on&bit)!=0;
                if(!binding_read(field.address,&value,1)||(value!=desired&&!binding_write(field.address,desired)))frame->failed[i]|=bit;
            }
        }
    }
}
void cheat_bindings_observe(const CheatBindingFrame *frame,unsigned known[2],unsigned enabled[2]){
    known[0]=known[1]=enabled[0]=enabled[1]=0;
    for(int i=0;i<2;++i){unsigned failed=frame->failed[i];
        for(unsigned j=0;j<frame->count[i];++j){CheatBindingField field=frame->field[i][j];unsigned bit=1u<<field.player;unsigned char value=0;
            if(!binding_read(field.address,&value,1)||value>1){failed|=bit;continue;}
            known[i]|=bit;if(value)enabled[i]|=bit;
        }
        known[i]&=~failed;
        if(frame->player_one==2){known[i]=((known[i]&1)<<1)|((known[i]&2)>>1);enabled[i]=((enabled[i]&1)<<1)|((enabled[i]&2)>>1);}
        if(failed)known[i]=0;
    }
}
