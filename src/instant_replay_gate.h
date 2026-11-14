#ifndef UFC6_INSTANT_REPLAY_GATE_H
#define UFC6_INSTANT_REPLAY_GATE_H
#include <stdint.h>

typedef struct { int owned, state; } InstantReplayGate;
/* Verified UFC 6 01.006 callback only; affects pause menus loaded for new fights. */
int instant_replay_gate_tick(InstantReplayGate *gate, uintptr_t guest_base, int enabled);

#endif
