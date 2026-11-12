#ifndef UFC6_PHOTO_CAGE_H
#define UFC6_PHOTO_CAGE_H
/* Exact-build octagon MeshFade group. Discover fresh objects through the native
   constructor/update; never scan the heap or retain per-session addresses. */
typedef void (__attribute__((sysv_abi)) *CageConstruct)(uintptr_t,uintptr_t,uintptr_t);
typedef void (__attribute__((sysv_abi)) *CageUpdate)(uintptr_t,uintptr_t);
typedef void (__attribute__((sysv_abi)) *CageSetOutput)(uintptr_t,const float*,unsigned);
typedef struct {uintptr_t object,data,world,root,target;int owned;} CagePanel;
#define CAGE_PANEL_CAPACITY 32
static CagePanel cage_panels[CAGE_PANEL_CAPACITY];
static SRWLOCK cage_guard=SRWLOCK_INIT;
static CageConstruct cage_constructor;
static CageUpdate cage_original_update;
static CageSetOutput cage_set_output;
static volatile LONG cage_installed,cage_preparing,cage_requested=1;
static volatile LONG cage_writes,cage_restores;
static int cage_read(uintptr_t object,CagePanel *out){
 if(!photo_hud_range(object,0x68,0)||*(uintptr_t*)object!=0x909c9f8c0ULL||*(uintptr_t*)(object+0x40)!=0x909ca0100ULL)return 0;
 uintptr_t data=*(uintptr_t*)(object+0x38),root=*(uintptr_t*)(object+0x60);
 if(!photo_hud_range(data,32,0)||*(uintptr_t*)data!=0x909ca6788ULL||*(uint32_t*)(data+0x18)!=0x6b85cec3u||!photo_hud_range(root,0x30,1))return 0;
 uintptr_t target=*(uintptr_t*)root;float source=*(float*)(object+0x5c);
 if(!photo_hud_range(target,4,1)||!isfinite(source)||source<0||source>1||!isfinite(*(float*)target))return 0;
 *out=(CagePanel){object,data,*(uintptr_t*)(object+0x30),root,target,0};return 1;
}
static int cage_same(const CagePanel *a,const CagePanel *b){return a->object==b->object&&a->data==b->data&&a->world==b->world&&a->root==b->root&&a->target==b->target;}
/* Caller holds cage_guard. Invalid/rebound objects are forgotten without writes. */
static void cage_observe(uintptr_t object,int constructed){
 CagePanel current;if(!cage_read(object,&current))return;
 int empty=-1;
 for(int i=0;i<CAGE_PANEL_CAPACITY;++i){
  CagePanel *p=&cage_panels[i];
  if(p->object==object){if(constructed||!cage_same(p,&current))*p=current;return;}
  if(empty<0){CagePanel check;if(!p->object||!cage_read(p->object,&check)||!cage_same(p,&check))empty=i;}
 }
 if(empty>=0)cage_panels[empty]=current;
}
static int cage_active(void){
 return cage_set_output&&InterlockedCompareExchange(&cage_requested,0,0)&&InterlockedCompareExchange(&state,0,0)==2&&
  InterlockedCompareExchange(&focused,0,0)&&!(InterlockedCompareExchange(&commands,0,0)&PHOTO_STOP)&&
  PHOTO_NOW()-(ULONGLONG)InterlockedCompareExchange64(&hud_poll_tick,0,0)<1000&&
  PHOTO_NOW()-(ULONGLONG)InterlockedCompareExchange64(&hud_camera_tick,0,0)<1000;
}
static void cage_apply_locked(int active){
 if(!cage_set_output)return;
 for(int i=0;i<CAGE_PANEL_CAPACITY;++i){CagePanel *p=&cage_panels[i],current;
  if(!p->object||(!active&&!p->owned))continue;
  if(!cage_read(p->object,&current)||!cage_same(p,&current)){memset(p,0,sizeof(*p));continue;}
  float value=*(float*)p->target;
  if(active){if(value!=0){float visible=0;p->owned=1;cage_set_output(p->object+0x60,&visible,1);InterlockedIncrement(&cage_writes);}}
  else if(p->owned){p->owned=0;if(value==0){float native=*(float*)(p->object+0x5c);cage_set_output(p->object+0x60,&native,1);InterlockedIncrement(&cage_restores);}}
 }
}
static void cage_apply(int active){AcquireSRWLockExclusive(&cage_guard);cage_apply_locked(active);ReleaseSRWLockExclusive(&cage_guard);}
static void __attribute__((sysv_abi)) cage_construct(uintptr_t object,uintptr_t a,uintptr_t b){
 cage_constructor(object,a,b);
 AcquireSRWLockExclusive(&cage_guard);cage_observe(object,1);ReleaseSRWLockExclusive(&cage_guard);
}
static void __attribute__((sysv_abi)) cage_update(uintptr_t object,uintptr_t context){
 cage_original_update(object,context);
 AcquireSRWLockExclusive(&cage_guard);cage_observe(object,0);cage_apply_locked(cage_active());ReleaseSRWLockExclusive(&cage_guard);
}
#ifndef NATIVE_CAMERA_TEST
#ifndef QUIT_MENU_TEST
static DWORD WINAPI cage_prepare_worker(void *unused){
 (void)unused;
 if(!engine_fingerprint()){InterlockedExchange(&cage_installed,-1);return 0;}
 const unsigned char ctor[]={0x55,0x48,0x89,0xe5,0x41,0x57,0x41,0x56,0x41,0x55,0x41,0x54,0x53,0x48,0x83,0xec,0x38};
 const unsigned char tick[]={0x55,0x48,0x89,0xe5,0x41,0x57,0x41,0x56,0x53,0x48,0x81,0xec,0x98,0,0,0};
 const unsigned char setter[]={0x48,0x8b,0x3f,0xe9,0x18,0xb3,0xff,0xff};
 ULONGLONG end=GetTickCount64()+60000;
 while(!readable(0x906bad060ULL,sizeof(ctor))||!readable(0x906bad5d0ULL,sizeof(tick))||!readable(0x904588ce0ULL,sizeof(setter))||
       memcmp((void*)0x906bad060ULL,ctor,sizeof(ctor))||memcmp((void*)0x906bad5d0ULL,tick,sizeof(tick))||memcmp((void*)0x904588ce0ULL,setter,sizeof(setter))){
  if(GetTickCount64()>=end){InterlockedExchange(&cage_installed,-1);return 0;}Sleep(100);
 }
 cage_set_output=(CageSetOutput)0x904588ce0ULL;
 ExactHook hooks[]={
  {(unsigned char*)0x906bad060ULL,(void*)cage_construct,(void**)&cage_constructor,ctor,sizeof(ctor)},
  {(unsigned char*)0x906bad5d0ULL,(void*)cage_update,(void**)&cage_original_update,tick,sizeof(tick)}};
 const char *error=NULL;
 InterlockedExchange(&cage_installed,install_exact_hooks(hooks,2,&error)?1:-1);return 0;
}
#endif
#endif
void photo_mode_prepare(void){
#ifndef NATIVE_CAMERA_TEST
#ifndef QUIT_MENU_TEST
 if(InterlockedCompareExchange(&cage_preparing,1,0))return;
 HANDLE thread=CreateThread(NULL,0,cage_prepare_worker,NULL,0,NULL);
 if(thread)CloseHandle(thread);else InterlockedExchange(&cage_preparing,0);
#else
 (void)cage_construct;(void)cage_update;(void)cage_installed;(void)cage_preparing;
#endif
#else
 (void)cage_construct;(void)cage_update;(void)cage_installed;(void)cage_preparing;
#endif
}
#endif
