#include "replay_budget_guard.h"
typedef uintptr_t (__attribute__((sysv_abi)) *Init)(uintptr_t,uintptr_t);
typedef struct {uintptr_t pointer,epoch;} Pair;
typedef Pair (__attribute__((sysv_abi)) *Lookup)(uintptr_t,uintptr_t);
static Init recording_init_original;
static uintptr_t __attribute__((sysv_abi)) recording_init(uintptr_t owner,uintptr_t allocator){
 uintptr_t context=recording_pointer(0x90a1c6230ULL);LONG before=0,estimate=0;BudgetSwap swap={0};
 if(!recording_pointer(owner+0x20)&&context){
  Pair p=((Lookup)0x9014e0df0ULL)(context,0x90a07b030ULL);
  if(recording_pointer(p.pointer)==0x9098c3928ULL&&read_memory(p.pointer+0x20,&before,4)&&read_memory(p.pointer+0x24,&estimate,4)&&estimate==8192){
   budget_acquire(&swap,(volatile LONG*)(p.pointer+0x20),before);
  }
 }
 uintptr_t result=recording_init_original(owner,allocator);
 if(swap.held)budget_restore(&swap);
 return result;
}
