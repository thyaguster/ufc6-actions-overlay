#ifndef PHOTO_HUD_COMMANDS_H
#define PHOTO_HUD_COMMANDS_H
/* Exact-build Scaleform command-list layout, confirmed in both draw epilogues.
   A mismatch leaves the original draw intact. No zero-count GPU packets. */
#ifndef PSAPI_VERSION
#define PSAPI_VERSION 2
#endif
#include <psapi.h>
/* Query only the accessed pages. VirtualQuery coalesces enormous guest regions
   and measured milliseconds per call in the actual engine process.
   https://learn.microsoft.com/windows/win32/api/psapi/nf-psapi-queryworkingsetex */
static int photo_hud_range(uintptr_t p,size_t n,int writing){
 if(!p||!n||p+n<p)return 0;
 uintptr_t first=p&~(uintptr_t)4095,last=(p+n-1)&~(uintptr_t)4095;
 size_t count=(last-first)/4096+1;
 PSAPI_WORKING_SET_EX_INFORMATION pages[8];
 if(count>8)return 0;
 memset(pages,0,count*sizeof(*pages));
 for(size_t i=0;i<count;++i)pages[i].VirtualAddress=(void*)(first+i*4096);
 if(!QueryWorkingSetEx(GetCurrentProcess(),pages,(DWORD)(count*sizeof(*pages))))return 0;
 for(size_t i=0;i<count;++i){
  if(!pages[i].VirtualAttributes.Valid)return 0;
  DWORD protection=(DWORD)pages[i].VirtualAttributes.Win32Protection;
  if(protection&(PAGE_GUARD|PAGE_NOACCESS))return 0;
  DWORD access=protection&0xff;
  if(access!=PAGE_READWRITE&&access!=PAGE_WRITECOPY&&access!=PAGE_EXECUTE_READWRITE&&access!=PAGE_EXECUTE_WRITECOPY&&
     (writing||(access!=PAGE_READONLY&&access!=PAGE_EXECUTE_READ)))return 0;
 }
 return 1;
}
static int photo_hud_writable(uintptr_t p,size_t n){return photo_hud_range(p,n,1);}
static int photo_hud_trim_draw(uintptr_t hal,int which,uintptr_t stats,uint32_t before){
 if(which<0||which>1||!photo_hud_range(hal,0x15b0,0)||*(uintptr_t*)hal!=0x909ba4518ULL||
    *(uintptr_t*)(hal+0x1580)!=stats||!photo_hud_range(stats,0x80,0)||*(uint32_t*)(stats+0x7c)!=(uint32_t)(before+1))return 0;
 uintptr_t list=*(uintptr_t*)(hal+0x1598),total=*(uintptr_t*)(hal+0x15a8);
 if(!photo_hud_writable(list,16)||!photo_hud_writable(total,12)||list==total)return 0;
 uint32_t size=which?32:24,used=*(uint32_t*)(list+8),capacity=*(uint32_t*)(list+12),all=*(uint32_t*)(total+8);
 uintptr_t data=*(uintptr_t*)list;
 if(used<size||used>capacity||all<size||data+used<data)return 0;
 uintptr_t packet=data+used-size;
 if(!photo_hud_range(packet,size,0)||*(uint64_t*)packet!=(which?0x0000000400200006ULL:0x0000000400180004ULL))return 0;
 /* Validate everything before changing either count; retain draw statistics. */
 if((list<total+12&&total<list+16)||(list<packet+size&&packet<list+16)||(total<packet+size&&packet<total+12))return 0;
 *(uint32_t*)(list+8)=used-size;
 *(uint32_t*)(total+8)=all-size;
 return 1;
}
#endif
