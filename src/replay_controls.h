#ifndef UFC6_REPLAY_CONTROLS_H
#define UFC6_REPLAY_CONTROLS_H
typedef struct {int available,paused,position_ms,duration_ms,extended,range_failed;} ReplaySnapshot;
int replay_prepare(void);
void replay_offer_decisions(int enabled);
unsigned replay_decision_events(void);
void replay_poll(int focused,int photo_active);
void replay_snapshot(ReplaySnapshot *out);
int replay_toggle_pause(void);
int replay_toggle_range(void);
int replay_set_speed(int index);
int replay_step(int direction);
int replay_step_ms(int direction,int milliseconds);
#endif
