#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string.h>
#include "startup_options.h"
/* Verified 01.006 registrations: ui_bootflow_short_movie, ui_bootflow_short_legal_timer, ui_short_bootflow.
   Keep the separate tutorial, onboarding, EULA and hospitality skip flags untouched. */
static const uintptr_t callbacks[]={0x5740dc0,0x5740db0,0x5740c80};
static const uintptr_t names[]={0x92e186d,0x926b768,0x922fb9a};
static const char *labels[]={"ui_bootflow_short_movie","ui_bootflow_short_legal_timer","ui_short_bootflow"};
static const unsigned short defaults[]={0xc031,0xc031,0xc031};
static int rd(uintptr_t a,void *out,size_t n){SIZE_T got=0;return ReadProcessMemory(GetCurrentProcess(),(void*)a,out,n,&got)&&got==n;}
static int checked(uintptr_t base,int i,unsigned short prefix){
    unsigned char actual[16],expected[16];char name[40]={0};MEMORY_BASIC_INFORMATION m;
    memset(expected,0xcc,sizeof(expected));memcpy(expected,&prefix,2);expected[2]=0xc3;
    uintptr_t a=base+callbacks[i];
    return !(a&1)&&VirtualQuery((void*)a,&m,sizeof(m))&&m.State==MEM_COMMIT&&!(m.Protect&PAGE_GUARD)&&
      (m.Protect&(PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY))&&
      rd(a,actual,16)&&!memcmp(actual,expected,16)&&rd(base+names[i],name,strlen(labels[i])+1)&&!strcmp(name,labels[i]);
}
int startup_options_tick(StartupOptions *s,uintptr_t base,int movie,int skip){
    const int wanted[]={movie!=0,movie!=0,skip!=0};int active=0;
    for(int i=0;i<3;++i){
        if(!wanted[i]&&!s->owned[i])continue;
        unsigned short current=s->owned[i]?0x01b0:defaults[i];
        if(!checked(base,i,current))return s->state=-1;
    }
    for(int i=0;i<3;++i){
        if(wanted[i]==s->owned[i]){active+=s->owned[i];continue;}
        unsigned short patched=0x01b0;
        unsigned short from=s->owned[i]?patched:defaults[i],to=wanted[i]?patched:defaults[i];
        void *address=(void*)(base+callbacks[i]);DWORD old=0,unused=0;
        if(!VirtualProtect(address,2,PAGE_EXECUTE_READWRITE,&old))return s->state=-1;
        SHORT observed=InterlockedCompareExchange16((volatile SHORT*)address,(SHORT)to,(SHORT)from);
        FlushInstructionCache(GetCurrentProcess(),address,2);
        BOOL restored=VirtualProtect(address,2,old,&unused);
        if((unsigned short)observed!=from)return s->state=-1;
        s->owned[i]=wanted[i];active+=s->owned[i];
        if(!restored)return s->state=-1;
    }
    return s->state=active?1:0;
}
