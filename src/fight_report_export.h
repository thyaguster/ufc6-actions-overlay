#ifndef UFC6_FIGHT_REPORT_EXPORT_H
#define UFC6_FIGHT_REPORT_EXPORT_H
#include <stddef.h>
#include <stdint.h>
#include <wchar.h>

enum {
    REPORT_STATE, REPORT_RESULT_TYPE, REPORT_ROUND, REPORT_ROUND_LENGTH,
    REPORT_ONLINE_RESULT, REPORT_ONLINE_RESULT_TYPE, REPORT_MATCH_TIME,
    REPORT_KNOCKDOWNS, REPORT_SIG_ATTEMPTED, REPORT_SIG_LANDED,
    REPORT_TOTAL_ATTEMPTED, REPORT_TOTAL_LANDED, REPORT_TD_ATTEMPTED,
    REPORT_TD_LANDED, REPORT_STUNS, REPORT_CONTROL, REPORT_FIELD_COUNT
};
typedef struct { uint32_t value[REPORT_FIELD_COUNT][2]; } FightReportRaw;

int fight_report_format(const FightReportRaw *raw,const char *version,const char *engine,
                        const char *id,unsigned long long captured_at,
                        char *json,size_t capacity);
int fight_report_export(const wchar_t *game_version,int new_engine,
                        wchar_t *path,size_t path_count,wchar_t *error,size_t error_count);
#endif
