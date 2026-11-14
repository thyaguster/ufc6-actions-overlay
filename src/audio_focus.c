/* Silence verified game PCM submissions, without changing persistent mixer state. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "audio_focus.h"
#ifndef AUDIO_FOCUS_TEST
#include "engine_fingerprint.h"
#endif
typedef bool (*AudioSubmit)(void*,const void*,int);
static AudioSubmit submit_original;
static uintptr_t expected_caller;
static volatile LONG silence_requested;
static int installed,engine_checked;
static void **dispatch_slot;
static SRWLOCK setup_guard=SRWLOCK_INIT;
static const unsigned char silence[262144]={0};
#ifdef AUDIO_FOCUS_TRACE
static volatile LONG submitted_buffers,silenced_buffers;
#endif
#ifndef AUDIO_CALLER
#define AUDIO_CALLER() ((uintptr_t)__builtin_return_address(0))
#endif
static bool submit_audio(void *stream,const void *data,int length,uintptr_t caller){
#ifdef AUDIO_FOCUS_TRACE
 if(caller==expected_caller)InterlockedIncrement(&submitted_buffers);
#endif
 if(!InterlockedCompareExchange(&silence_requested,0,0)||caller!=expected_caller||!data||length<=0||length>16*1024*1024)return submit_original(stream,data,length);

#ifdef AUDIO_FOCUS_TRACE
 InterlockedIncrement(&silenced_buffers);
#endif
 const void *quiet=silence;void *heap=NULL;
 if((size_t)length>sizeof(silence)){heap=HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,(size_t)length);if(!heap)return submit_original(stream,data,length);quiet=heap;}
 bool result=submit_original(stream,quiet,length);if(heap)HeapFree(GetProcessHeap(),0,heap);return result;
}
static bool __attribute__((noinline)) audio_hook(void *stream,const void *data,int length){return submit_audio(stream,data,length,AUDIO_CALLER());}
static int install_slot(void **slot,void *expected){
 DWORD protection=0,ignored;if(!VirtualProtect(slot,sizeof(void*),PAGE_READWRITE,&protection))return 0;
 submit_original=(AudioSubmit)expected;
 int ok=InterlockedCompareExchangePointer((void*volatile*)slot,(void*)audio_hook,expected)==expected;
 VirtualProtect(slot,sizeof(void*),protection,&ignored);if(ok)dispatch_slot=slot;return ok;
}
int audio_focus_prepare(void){
 AcquireSRWLockExclusive(&setup_guard);
 if(installed){int ready=installed>0&&dispatch_slot&&*(void*volatile*)dispatch_slot==(void*)audio_hook;ReleaseSRWLockExclusive(&setup_guard);return ready;}
#ifdef AUDIO_FOCUS_TEST
 (void)install_slot;(void)engine_checked;ReleaseSRWLockExclusive(&setup_guard);return 0;
#else
 if(!engine_checked){int profile=engine_fingerprint();engine_checked=profile==1||profile==2?1:-1;}if(engine_checked<0){installed=-1;ReleaseSRWLockExclusive(&setup_guard);return 0;}
 uintptr_t base=(uintptr_t)GetModuleHandleW(NULL);unsigned char call[6],head[32];uintptr_t target=0;SIZE_T got;
 static const unsigned char call_head[]={0xff,0x15,0xfe,0xd9,0xa8,0};
 static const unsigned char target_head[]={0x41,0x57,0x41,0x56,0x41,0x54,0x56,0x57,0x53,0x48,0x83,0xec,0x28,0x48,0x85,0xc9,0x74,0x3c,0x48,0x85,0xd2,0x74,0x47,0x45,0x85,0xc0,0x78,0x52,0x40,0xb6,0x01,0x0f};
 HANDLE process=GetCurrentProcess();int ready=ReadProcessMemory(process,(void*)(base+0xf74d04),call,6,&got)&&got==6&&!memcmp(call,call_head,6)&&ReadProcessMemory(process,(void*)(base+0x1a02708),&target,8,&got)&&got==8&&target==base+0xb7240&&ReadProcessMemory(process,(void*)target,head,32,&got)&&got==32&&!memcmp(head,target_head,32);
 if(ready){expected_caller=base+0xf74d0a;installed=install_slot((void**)(base+0x1a02708),(void*)target);}
 ReleaseSRWLockExclusive(&setup_guard);return installed>0;
#endif
}
void audio_focus_poll(int enabled,int focused){
 int ready=enabled&&audio_focus_prepare();InterlockedExchange(&silence_requested,ready&&!focused);
}
