#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#undef NDEBUG
#include <assert.h>
#include <stdio.h>
#include "../src/startup_options.c"
static void put(uintptr_t base,int i){
 DWORD old=0,unused=0;void *p=(void*)(base+callbacks[i]);assert(VirtualProtect(p,16,PAGE_READWRITE,&old));
 memset(p,0xcc,16);memcpy(p,&defaults[i],2);((unsigned char*)p)[2]=0xc3;
 assert(VirtualProtect(p,16,PAGE_EXECUTE_READ,&unused));
}
int main(void){
 const SIZE_T size=0xa000000;uintptr_t base=(uintptr_t)VirtualAlloc(NULL,size,MEM_RESERVE,PAGE_NOACCESS);assert(base);
 for(int i=0;i<3;++i){assert(VirtualAlloc((void*)((base+callbacks[i])&~(uintptr_t)4095),4096,MEM_COMMIT,PAGE_READWRITE));
  assert(VirtualAlloc((void*)((base+names[i])&~(uintptr_t)4095),4096,MEM_COMMIT,PAGE_READWRITE));strcpy((char*)(base+names[i]),labels[i]);put(base,i);}
 StartupOptions s={0};assert(startup_options_tick(&s,base,0,0)==0);
 assert(startup_options_tick(&s,base,0,1)==1&&!s.owned[0]&&!s.owned[1]&&s.owned[2]);assert(checked(base,2,0x01b0));
 assert(startup_options_tick(&s,base,0,0)==0&&checked(base,2,defaults[2]));
 assert(startup_options_tick(&s,base,1,0)==1&&s.owned[0]&&s.owned[1]&&!s.owned[2]);
 assert(startup_options_tick(&s,base,1,1)==1&&s.owned[2]);
 StartupOptions other={0};assert(startup_options_tick(&other,base,1,1)==-1&&!other.owned[0]);
 assert(startup_options_tick(&s,base,1,0)==1&&checked(base,2,defaults[2])&&checked(base,0,0x01b0));
 assert(startup_options_tick(&s,base,0,0)==0);for(int i=0;i<3;++i)assert(checked(base,i,defaults[i]));
 ((char*)(base+names[2]))[0]='X';assert(startup_options_tick(&s,base,1,1)==-1&&!s.owned[0]);strcpy((char*)(base+names[2]),labels[2]);
 DWORD old=0,unused=0;assert(VirtualProtect((void*)(base+callbacks[2]),16,PAGE_READWRITE,&old));((unsigned char*)(base+callbacks[2]))[5]=0x90;assert(VirtualProtect((void*)(base+callbacks[2]),16,old,&unused));
 assert(startup_options_tick(&s,base,1,1)==-1&&!s.owned[0]);put(base,2);
 assert(startup_options_tick(&s,base,1,1)==1);put(base,2);assert(startup_options_tick(&s,base,0,0)==-1);assert(checked(base,0,0x01b0)&&checked(base,1,0x01b0));
 assert(VirtualFree((void*)base,0,MEM_RELEASE));puts("PASS: movie/legal shortening and separate boot-flow bypass, opt-in ownership and restoration, byte/name guards, restore and conflict rejection.");return 0;
}
