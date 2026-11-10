#ifndef UFC6_CAREER_CHEATS_H
#define UFC6_CAREER_CHEATS_H
#include <stdint.h>
enum { CAREER_MONEY, CAREER_POINTS, CAREER_COUNT };
enum { CAREER_WAIT, CAREER_READY, CAREER_PENDING, CAREER_ADDED, CAREER_LIMIT, CAREER_CHANGED, CAREER_ERROR, CAREER_UNSUPPORTED };
typedef struct {int ready,status,kind;uint32_t balance[2],maximum[2],amount;} CareerSnapshot;
void career_cheats_init(uintptr_t base);
void career_cheats_tick(void);
void career_cheats_cancel(void);
void career_cheats_snapshot(CareerSnapshot *out);
int career_cheats_add(int kind,uint32_t amount);
#endif
