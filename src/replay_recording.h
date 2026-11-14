/* Recorder coverage is read on the native update thread, only when changing range. */
#include "replay_ranges.h"
static uintptr_t recording_pointer(uintptr_t p){uintptr_t v=0;read_memory(p,&v,8);return v;}
static int recording_intervals(uintptr_t store,ReplayRange*out,int capacity){
 int n=0;if(!store)return 0;
 for(unsigned group=0;group<2;++group){unsigned char b[24],again[24];uintptr_t at=store+0xaa8+(group?0x50:0x30);unsigned head,count,cap;uintptr_t data;
  if(!read_memory(at,b,24))return 0;memcpy(&head,b,4);memcpy(&count,b+4,4);memcpy(&cap,b+8,4);memcpy(&data,b+16,8);
  if(count>head||count>cap||cap>32768||count>(unsigned)(capacity-n)||(!data&&count))return 0;
  for(unsigned j=0;j<count;++j){unsigned index=(head-count+j)%cap;double values[2];if(!read_memory(data+(uintptr_t)index*32,values,16)||!isfinite(values[0])||!isfinite(values[1])||values[0]<0||values[0]>86400||values[1]<=0||values[1]>10)return 0;out[n++]=(ReplayRange){values[0],values[0]+values[1]};}
  if(!read_memory(at,again,24)||memcmp(b,again,24))return 0;
 }
 return range_merge(out,n);
}
typedef unsigned char (__attribute__((sysv_abi)) *RecordingConvert)(double*,double);
static int recording_map_intervals(ReplayRange*items,int n,RecordingConvert convert){
 int mapped=0;
 for(int i=0;i<n;++i){double lo=0,hi=0;
  if(!convert(&lo,items[i].low)||!convert(&hi,items[i].high)||!isfinite(lo)||!isfinite(hi)||hi<=lo)continue;
  items[mapped++]=(ReplayRange){lo,hi};
 }
 return range_merge(items,mapped);
}
static int recording_coverage(double time,const double retained[2],ReplayRange*out){
 enum{MAX_INTERVALS=8192};ReplayRange*items=HeapAlloc(GetProcessHeap(),0,MAX_INTERVALS*sizeof(*items));if(!items)return 0;
 int ok=0;ReplayRange aux={0},main={0};uintptr_t aux_root=recording_pointer(0x90a07af78ULL),aux_store=recording_pointer(recording_pointer(aux_root+0x20)+8);
 int n=recording_intervals(aux_store,items,MAX_INTERVALS);if(!range_containing(items,n,time,&aux))goto done;
 uintptr_t root=recording_pointer(0x90b1a49b8ULL),provider=recording_pointer(recording_pointer(root)+0x2e0),store=recording_pointer(recording_pointer(provider+8)+8);
 n=recording_intervals(store,items,MAX_INTERVALS);if(!n)goto done;
 #if defined(REPLAY_NATIVE_TEST) || defined(QUIT_MENU_TEST)
 (void)main;(void)retained;(void)out;(void)recording_map_intervals;goto done;
 #else
 n=recording_map_intervals(items,n,(RecordingConvert)0x903f122c0ULL);if(!range_containing(items,n,time,&main))goto done;
 out->low=fmax(.05,fmax(retained[0],fmax(aux.low,main.low))+.001);out->high=fmin(retained[1],fmin(aux.high,main.high))-.001;
 ok=isfinite(out->low)&&isfinite(out->high)&&out->high-out->low>.01&&time>=out->low&&time<out->high;
 #endif
 done:HeapFree(GetProcessHeap(),0,items);return ok;
}
