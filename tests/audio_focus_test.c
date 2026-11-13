#define AUDIO_FOCUS_TEST
#include "../src/audio_focus.c"
#undef NDEBUG
#include <assert.h>
#include <stdio.h>
static unsigned calls;static int expected_length,quiet,failed;static const void *expected_data;static void *expected_stream=(void*)0x12340000;
static bool fake_submit(void *stream,const void *data,int len){assert(stream==expected_stream&&len==expected_length);if(quiet){assert(data!=expected_data);for(int i=0;i<len;++i)assert(((const unsigned char*)data)[i]==0);}else assert(data==expected_data);++calls;return !failed;}
static bool foreign_submit(void *a,const void *b,int n){(void)a;(void)b;(void)n;return false;}
int main(void){
 void **slot=VirtualAlloc(NULL,4096,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);assert(slot);*slot=(void*)foreign_submit;assert(!install_slot(slot,(void*)fake_submit)&&*slot==(void*)foreign_submit);
 *slot=(void*)fake_submit;DWORD prior;assert(VirtualProtect(slot,4096,PAGE_READONLY,&prior));assert(install_slot(slot,(void*)fake_submit)&&*slot==(void*)audio_hook);installed=1;expected_caller=0x11223344;
 unsigned char data[64];memset(data,0x5a,sizeof(data));expected_data=data;expected_length=64;
 audio_focus_poll(0,0);assert(submit_audio(expected_stream,data,64,expected_caller));audio_focus_poll(1,1);assert(submit_audio(expected_stream,data,64,expected_caller));
 audio_focus_poll(1,0);assert(submit_audio(expected_stream,data,64,expected_caller+1));quiet=1;assert(submit_audio(expected_stream,data,64,expected_caller));for(unsigned i=0;i<sizeof(data);++i)assert(data[i]==0x5a);
 failed=1;assert(!submit_audio(expected_stream,data,64,expected_caller));failed=0;expected_length=300000;assert(submit_audio(expected_stream,data,300000,expected_caller));
 quiet=0;expected_length=64;audio_focus_poll(1,1);assert(submit_audio(expected_stream,data,64,expected_caller));audio_focus_poll(0,0);assert(submit_audio(expected_stream,data,64,expected_caller));
 audio_focus_poll(1,0);expected_length=0;expected_data=NULL;assert(submit_audio(expected_stream,NULL,0,expected_caller));
 assert(calls==9);VirtualFree(slot,0,MEM_RELEASE);puts("PASS: verified-slot CAS/foreign rejection, read-only protection restoration, foreground/disabled passthrough, background silence, other callers, unchanged source buffers, large blocks, null/zero input and native failure forwarding; no real audio or mixer changed.");
}
