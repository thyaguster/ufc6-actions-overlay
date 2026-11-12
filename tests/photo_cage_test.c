#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#define NATIVE_CAMERA_TEST
#include "../src/photo_mode.c"
#undef NDEBUG
#include <assert.h>
static unsigned constructed,updated,published;
static uintptr_t object,seen_a,seen_b;
static void __attribute__((sysv_abi)) fake_construct(uintptr_t o,uintptr_t a,uintptr_t b){assert(o==object);constructed++;seen_a=a;seen_b=b;}
static void __attribute__((sysv_abi)) fake_update(uintptr_t o,uintptr_t context){assert(o==object);updated++;seen_a=context;}
static void __attribute__((sysv_abi)) fake_set(uintptr_t root,const float *v,unsigned n){assert(n==1);*(float*)*(uintptr_t*)*(uintptr_t*)root=*v;published++;}
int main(void){
 (void)hud_draw_a;(void)hud_draw_b;(void)update;
 unsigned char *mem=VirtualAlloc(NULL,4096,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);assert(mem);memset(mem,0,4096);
 object=(uintptr_t)mem;uintptr_t data=object+256,root=object+512,target=object+768;
 *(uintptr_t*)object=0x909c9f8c0ULL;*(uintptr_t*)(object+0x40)=0x909ca0100ULL;*(uintptr_t*)(object+0x38)=data;*(uintptr_t*)(object+0x30)=123;
 *(uintptr_t*)data=0x909ca6788ULL;*(uint32_t*)(data+0x18)=0x6b85cec3u;*(uintptr_t*)(object+0x60)=root;*(uintptr_t*)root=target;
 *(float*)(object+0x5c)=1;*(float*)target=1;*(unsigned*)(object+0x50)=2;
 cage_constructor=fake_construct;cage_original_update=fake_update;cage_set_output=fake_set;
 cage_construct(object,11,22);assert(constructed==1&&seen_a==11&&seen_b==22&&cage_panels[0].object==object);
 cage_update(object,33);assert(updated==1&&seen_a==33&&*(float*)target==1&&!published);
 state=2;focused=1;hud_poll_tick=hud_camera_tick=(LONG64)PHOTO_NOW();
 cage_apply(cage_active());assert(*(float*)target==0&&*(float*)(object+0x5c)==1&&*(unsigned*)(object+0x50)==2&&cage_panels[0].owned);
 /* Freeze does not disable visibility. Native source changes still win on exit. */
 frozen=1;cage_apply(cage_active());assert(*(float*)target==0);
 *(float*)(object+0x5c)=.4f;focused=0;cage_apply(cage_active());assert(*(float*)target==.4f&&!cage_panels[0].owned);
 focused=1;cage_apply(cage_active());assert(*(float*)target==0);cage_requested=0;cage_apply(cage_active());assert(*(float*)target==.4f);
 cage_requested=1;cage_apply(cage_active());*(float*)target=.7f;cage_apply(0);assert(*(float*)target==.7f); /* foreign output retained */
 cage_apply(1);*(uint32_t*)(data+0x18)=0;cage_apply(0);assert(*(float*)target==0&&!cage_panels[0].object);
 cage_construct(object,44,55);assert(!cage_panels[0].object); /* wrong asset never cached */
 *(uint32_t*)(data+0x18)=0x6b85cec3u;*(float*)target=1;cage_update(object,66);assert(cage_panels[0].object==object&&*(float*)target==0);
 /* Object recreation/rebinding clears ownership and discovers fresh target. */
 *(float*)target=1;cage_construct(object,77,88);assert(!cage_panels[0].owned);cage_apply(0);assert(*(float*)target==1);
 cage_apply(1);uintptr_t new_target=target+8;*(float*)new_target=.8f;*(uintptr_t*)root=new_target;cage_apply(0);assert(*(float*)new_target==.8f&&!cage_panels[0].object);
 cage_observe(object,0);assert(cage_panels[0].target==new_target);cage_apply(1);assert(*(float*)new_target==0);
 commands=PHOTO_STOP;cage_apply(cage_active());assert(*(float*)new_target==.4f);
 commands=0;hud_camera_tick=(LONG64)PHOTO_NOW()-1001;assert(!cage_active());hud_camera_tick=(LONG64)PHOTO_NOW();hud_poll_tick=(LONG64)PHOTO_NOW()-1001;assert(!cage_active());
 photo_mode_prepare();
 puts("PASS: native cage discovery and ABI forwarding; exact asset/world/binding identities; output-only visibility while frozen; current-source restoration on toggle/focus/stop; foreign writes, recreated/rebound objects and stale-input guards.");
 return 0;
}
