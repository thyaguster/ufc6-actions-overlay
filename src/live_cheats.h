#ifndef UFC6_LIVE_CHEATS_H
#define UFC6_LIVE_CHEATS_H
#include <stdint.h>
enum { LIVE_STAMINA, LIVE_HEALTH, LIVE_COUNT };
enum { LIVE_WAIT, LIVE_OFF, LIVE_ON, LIVE_LAUNCHER, LIVE_UNSUPPORTED, LIVE_RESTORING, LIVE_ERROR, LIVE_GAME };
enum { LIVE_PLAYER1=1 };
typedef struct {
    int requested,status;
    unsigned owned_mask,clear_mask;
} LiveCheat;
typedef struct { uintptr_t base; unsigned target; LiveCheat flags[LIVE_COUNT]; } LiveCheats;
void live_cheats_init(LiveCheats *state,uintptr_t guest_base);
void live_cheats_tick(LiveCheats *state);
int live_cheats_toggle(LiveCheats *state,int which);
int live_cheats_fight_ready(void);
int live_ko_toggle(void);
int live_ko_poll(int allowed); /* 0 idle, 1 armed/pending, 2 KO, 3 cancelled */
void live_cheats_stop(LiveCheats *state);
#endif
