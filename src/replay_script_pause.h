#ifndef UFC6_REPLAY_SCRIPT_PAUSE_H
#define UFC6_REPLAY_SCRIPT_PAUSE_H
#include <windows.h>
#include <stdint.h>
#include <string.h>
typedef struct {uintptr_t object,buffer,field,world,binding;int owned;} PresentationPause;
static int flag_read(uintptr_t a,void *out,size_t n){SIZE_T got=0;return a>=0x10000&&ReadProcessMemory(GetCurrentProcess(),(void*)a,out,n,&got)&&got==n;}
static int flag_resolve(PresentationPause *out){
 uintptr_t obj=0;unsigned char b[0x70],head[32],meta[48];char name[32]={0};
 if(!flag_read(0x90b46beb0ULL,&obj,8)||!flag_read(obj,b,sizeof(b)))return 0;
 uintptr_t vt,np,world,binding,buf,schema;unsigned size,alignment;
 memcpy(&vt,b,8);memcpy(&np,b+8,8);memcpy(&schema,b+0x20,8);memcpy(&world,b+0x28,8);memcpy(&binding,b+0x50,8);memcpy(&size,b+0x58,4);memcpy(&alignment,b+0x5c,4);memcpy(&buf,b+0x60,8);
 if(vt!=0x909c227f0ULL||size!=4||alignment!=4||!flag_read(np,name,16)||memcmp(name,"Replay_IsPaused",16)||!flag_read(schema,meta,sizeof(meta))||!flag_read(buf,head,32))return 0;
 uintptr_t data_vt,data_world,data_binding;unsigned short offset;int first,last;
 memcpy(&data_vt,head,8);memcpy(&data_world,head+8,8);memcpy(&data_binding,head+16,8);memcpy(&offset,head+28,2);memcpy(&first,meta+0x24,4);memcpy(&last,meta+0x28,4);
 if(data_vt!=0x909927520ULL||data_world!=world||data_binding!=binding||first!=0||last!=0||offset!=32||(buf+offset)%4)return 0;
 LONG value;uintptr_t field=buf+offset;MEMORY_BASIC_INFORMATION info;
 if(!flag_read(field,&value,4)||value<0||value>1||!VirtualQuery((void*)field,&info,sizeof(info))||info.State!=MEM_COMMIT||info.Protect&PAGE_GUARD||!(info.Protect&(PAGE_READWRITE|PAGE_WRITECOPY)))return 0;
 *out=(PresentationPause){obj,buf,field,world,binding,0};return 1;
}
static int flag_identity(const PresentationPause *s){
 uintptr_t object,world,binding,vt,current_buffer;unsigned char b[32];
 if(!flag_read(0x90b46beb0ULL,&object,8)||object!=s->object||!flag_read(object+0x28,&world,8)||world!=s->world||!flag_read(object+0x50,&binding,8)||binding!=s->binding||!flag_read(object+0x60,&current_buffer,8)||current_buffer!=s->buffer||!flag_read(s->buffer,b,32))return 0;
 memcpy(&vt,b,8);memcpy(&world,b+8,8);memcpy(&binding,b+16,8);
 MEMORY_BASIC_INFORMATION info;if(!VirtualQuery((void*)s->field,&info,sizeof(info))||info.State!=MEM_COMMIT||info.Protect&PAGE_GUARD||!(info.Protect&(PAGE_READWRITE|PAGE_WRITECOPY)))return 0;
 return vt==0x909927520ULL&&world==s->world&&binding==s->binding;
}
static int flag_acquire(PresentationPause *s){PresentationPause found;if(!flag_resolve(&found))return 0;
 LONG old=InterlockedCompareExchange((volatile LONG*)found.field,1,0);if(old!=0)return 0;found.owned=1;*s=found;return 1;
}
static void flag_restore(PresentationPause *s){if(s->owned&&flag_identity(s))InterlockedCompareExchange((volatile LONG*)s->field,0,1);memset(s,0,sizeof(*s));}
#endif
