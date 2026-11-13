#ifndef UFC6_CAREER_CHEATS_H
#define UFC6_CAREER_CHEATS_H
#include <stdint.h>
enum { CAREER_MONEY, CAREER_POINTS, CAREER_FITNESS, CAREER_DAYS, CAREER_LONGEVITY, CAREER_GRADES, CAREER_COUNT };
enum { CAREER_WAIT, CAREER_READY, CAREER_PENDING, CAREER_ADDED, CAREER_LIMIT, CAREER_CHANGED, CAREER_ERROR, CAREER_UNSUPPORTED };
enum { CAREER_DIAG_CODE_WAIT,CAREER_DIAG_CODE_MISMATCH,CAREER_DIAG_HOOK_FAILED,CAREER_DIAG_USER_WAIT,CAREER_DIAG_NO_USER,CAREER_DIAG_ROOT,CAREER_DIAG_REGISTRY,CAREER_DIAG_COLLECTION,CAREER_DIAG_MONEY,CAREER_DIAG_POINTS,CAREER_DIAG_MONEY_LAYOUT,CAREER_DIAG_POINTS_LAYOUT,CAREER_DIAG_MONEY_READONLY,CAREER_DIAG_POINTS_READONLY,CAREER_DIAG_READY,CAREER_DIAG_CHANGED };
typedef struct {int reason,hooks;uint32_t probes;} CareerDiagnostics;
typedef struct {int ready,status,kind;uint32_t balance[2],maximum[2],amount;int fitness_ready;uint32_t fitness;int training_ready;uint32_t training_days,training_week;int longevity_ready,longevity_enabled;uint32_t longevity,longevity_blocked;int longevity_requested;uint32_t training_multiplier;int training_rewards_ready,sparring_grades_ready;uint32_t sparring_grades_completed;} CareerSnapshot;
void career_cheats_init(uintptr_t base);
void career_cheats_tick(void);
void career_cheats_cancel(void);
void career_cheats_snapshot(CareerSnapshot *out);
void career_cheats_diagnostics(CareerDiagnostics *out);
const char *career_cheats_diagnostic_name(int reason);
int career_cheats_add(int kind,uint32_t amount);
int career_cheats_restore_fitness(void);
int career_cheats_restore_days(void);
int career_cheats_protect_longevity(int enabled);
int career_cheats_training_multiplier(int value);
int career_cheats_grade_a(void);
#endif
