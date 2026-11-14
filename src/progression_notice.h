#ifndef UFC6_PROGRESSION_NOTICE_H
#define UFC6_PROGRESSION_NOTICE_H

/* Suppresses only the verified offline post-fight progression notice. */
int progression_notice_prepare(void);
void progression_notice_set(int enabled);
int progression_notice_state(void);

#endif
