#ifndef UFC6_CHEAT_BINDINGS_H
#define UFC6_CHEAT_BINDINGS_H
#include <stdint.h>
#include <stddef.h>
typedef struct {uintptr_t owner,begin,end;size_t index[3];unsigned physical_owned[2];} CheatBindingCache;
typedef struct {uintptr_t address;unsigned player;} CheatBindingField;
typedef struct {CheatBindingField field[2][16];unsigned count[2],failed[2];int player_one;} CheatBindingFrame;
void cheat_bindings_apply(CheatBindingCache *cache,uintptr_t binder,unsigned first,unsigned count,const unsigned command[2],CheatBindingFrame *frame);
void cheat_bindings_observe(const CheatBindingFrame *frame,unsigned known[2],unsigned enabled[2]);
#endif
