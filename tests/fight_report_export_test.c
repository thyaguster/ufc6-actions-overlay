#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "fight_report_export.h"

int main(void){
    FightReportRaw raw={0};char json[2048];
    raw.value[REPORT_STATE][0]=3;
    raw.value[REPORT_RESULT_TYPE][0]=2;
    raw.value[REPORT_ONLINE_RESULT_TYPE][0]=2;
    raw.value[REPORT_ROUND][0]=1;
    raw.value[REPORT_ROUND_LENGTH][0]=1133903872u; /* 300.0f */
    raw.value[REPORT_MATCH_TIME][0]=397;
    raw.value[REPORT_ONLINE_RESULT][0]=2;
    raw.value[REPORT_ONLINE_RESULT][1]=1;
    raw.value[REPORT_KNOCKDOWNS][1]=5;
    raw.value[REPORT_SIG_ATTEMPTED][0]=13;
    raw.value[REPORT_SIG_ATTEMPTED][1]=25;
    raw.value[REPORT_SIG_LANDED][0]=2;
    raw.value[REPORT_SIG_LANDED][1]=9;
    raw.value[REPORT_TOTAL_ATTEMPTED][0]=24;
    raw.value[REPORT_TOTAL_ATTEMPTED][1]=34;
    raw.value[REPORT_TOTAL_LANDED][0]=5;
    raw.value[REPORT_TOTAL_LANDED][1]=12;
    raw.value[REPORT_TD_ATTEMPTED][1]=3;
    raw.value[REPORT_TD_LANDED][1]=1;
    raw.value[REPORT_STUNS][1]=2;
    raw.value[REPORT_CONTROL][1]=1124402339u; /* 133.018f */
    assert(fight_report_format(&raw,"01.006.000","ps5-pro",
        "550e8400-e29b-41d4-a716-446655440000",1794734106070ULL,json,sizeof(json)));
    assert(strstr(json,"\"winnerCorner\":\"red\""));
    assert(strstr(json,"\"method\":\"Submission\""));
    assert(strstr(json,"\"round\":2,\"clockSeconds\":203"));
    assert(strstr(json,"\"corner\":\"red\",\"name\":null,\"stats\":{\"knockdowns\":5"));
    assert(strstr(json,"\"totalStrikesAttempted\":34,\"totalStrikesLanded\":12"));
    assert(strstr(json,"\"controlTimeSeconds\":133"));
    assert(strstr(json,"\"corner\":\"blue\",\"name\":null,\"stats\":{\"knockdowns\":0"));
    assert(fight_report_format_result(&raw,"01.006.000","ps5-pro","result-id",1794734106070ULL,json,sizeof(json)));
    assert(strstr(json,"\"reportStage\":\"result\""));
    assert(strstr(json,"\"fightStatisticsComplete\":false"));
    assert(strstr(json,"\"winnerCorner\":\"red\""));
    assert(!strstr(json,"\"stats\""));
    raw.value[REPORT_ROUND][0]=0;
    raw.value[REPORT_MATCH_TIME][0]=194;
    assert(fight_report_format(&raw,"01.006.000","ps5-pro",
        "550e8400-e29b-41d4-a716-446655440000",1794734106070ULL,json,sizeof(json)));
    assert(strstr(json,"\"round\":1,\"clockSeconds\":106"));
    raw.value[REPORT_STATE][0]=2;
    assert(!fight_report_format(&raw,"01.006.000","ps5-pro","id",1,json,sizeof(json)));
    raw.value[REPORT_STATE][0]=3;
    raw.value[REPORT_SIG_LANDED][1]=26;
    assert(!fight_report_format(&raw,"01.006.000","ps5-pro","id",1,json,sizeof(json)));
    assert(fight_report_format_result(&raw,"01.006.000","ps5-pro","id",1,json,sizeof(json)));
    puts("Native fight report mapping: passed");
    return 0;
}
