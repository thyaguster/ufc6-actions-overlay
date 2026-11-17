/* Read-only post-fight report. Guest addresses are validated before reading. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlobj.h>
#include <objbase.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include "fight_report_export.h"

typedef struct { const char *name; uint64_t slot; } ReportField;
static const ReportField fields[REPORT_FIELD_COUNT]={
    {"Global_MatchState",0x90b444c10ULL},
    {"Global_MatchResultType",0x90b444be8ULL},
    {"Global_RoundNum",0x90b444f08ULL},
    {"Global_RoundLength",0x90b444ee0ULL},
    {"Online_MatchResult",0x90b46a820ULL},
    {"Online_MatchResultType",0x90b46a870ULL},
    {"Online_MatchTimeSecs",0x90b46a8c0ULL},
    {"Online_KnockdownsLanded",0x90b46a988ULL},
    {"Online_AdvanceStrikesLanded",0x90b46ac30ULL},
    {"Online_SignificantStrikesThrown",0x90b46a960ULL},
    {"Online_StrikesLanded",0x90b46a9b0ULL},
    {"Online_StrikesThrown",0x90b46a9d8ULL},
    {"Online_TakedownsAttempted",0x90b46aa78ULL},
    {"Online_TakedownsLanded",0x90b46aaa0ULL},
    {"Online_StunsLanded",0x90b46ac80ULL},
    {"Online_Control_Duration",0x90b46aca8ULL}
};
static const GUID documents_id={0xfdd39ad0,0x238f,0x46af,{0xad,0xb4,0x6c,0x85,0x48,0x03,0x69,0xc7}};
static int read_exact(uintptr_t address,void *out,size_t length){
    SIZE_T read=0;
    return address>=0x10000&&ReadProcessMemory(GetCurrentProcess(),(void*)address,out,length,&read)&&read==length;
}
static int report_read_count(FightReportRaw *raw,int count){
    uint64_t world=0;
    for(int i=0;i<count;++i){
        uintptr_t object=0,name_address=0,buffer_address=0;
        unsigned char header[0x68],buffer[40];
        char name[96];
        if(!read_exact((uintptr_t)fields[i].slot,&object,sizeof(object))||
           !read_exact(object,header,sizeof(header)))return 0;
        uint64_t vtable=0,actual_world=0;
        uint32_t element_size=0;
        memcpy(&vtable,header,8);
        memcpy(&name_address,header+8,8);
        memcpy(&actual_world,header+0x28,8);
        memcpy(&element_size,header+0x58,4);
        memcpy(&buffer_address,header+0x60,8);
        if(vtable!=0x909c227f0ULL||element_size!=4||!name_address||
           !read_exact(name_address,name,sizeof(name)))return 0;
        name[sizeof(name)-1]=0;
        if(strcmp(name,fields[i].name))return 0;
        if(world&&world!=actual_world)return 0;
        world=actual_world;
        if(!read_exact(buffer_address,buffer,sizeof(buffer)))return 0;
        uint16_t data_offset=0;memcpy(&data_offset,buffer+0x1c,2);
        if(data_offset!=32)return 0;
        memcpy(raw->value[i],buffer+0x20,8);
    }
    return world!=0;
}
static int report_read(FightReportRaw *raw){return report_read_count(raw,REPORT_FIELD_COUNT);}
int fight_report_match_state(void){
    FightReportRaw raw={0};
    if(!report_read_count(&raw,REPORT_STATE+1))return -1;
    return raw.value[REPORT_STATE][0]<=3?(int)raw.value[REPORT_STATE][0]:-1;
}
static float game_float(uint32_t bits){float value;memcpy(&value,&bits,4);return value;}
static int valid_stat(const FightReportRaw *r,int field,int fighter){
    return r->value[field][fighter]<=9999;
}
static int report_result_valid(const FightReportRaw *r,int *winner,int *round,int *seconds){
    if(r->value[REPORT_STATE][0]!=3||
       r->value[REPORT_RESULT_TYPE][0]!=r->value[REPORT_ONLINE_RESULT_TYPE][0])return 0;
    uint32_t blue=r->value[REPORT_ONLINE_RESULT][0],red=r->value[REPORT_ONLINE_RESULT][1];
    if(blue==2&&red==1)*winner=1;
    else if(blue==1&&red==2)*winner=0;
    else return 0; /* Draw codes require a separate in-game validation. */
    uint32_t round_index=r->value[REPORT_ROUND][0];
    float length=game_float(r->value[REPORT_ROUND_LENGTH][0]);
    if(round_index>=12||!isfinite(length)||length<30||length>1800)return 0;
    float remaining=(float)r->value[REPORT_MATCH_TIME][0]-(float)round_index*length;
    if(!isfinite(remaining)||remaining<0||remaining>length)return 0;
    *round=(int)round_index+1;*seconds=(int)(length-remaining+0.5f);
    return 1;
}
static int report_valid(const FightReportRaw *r,int *winner,int *round,int *seconds){
    if(!report_result_valid(r,winner,round,seconds))return 0;
    const int stats[]={REPORT_KNOCKDOWNS,REPORT_SIG_ATTEMPTED,REPORT_SIG_LANDED,
        REPORT_TOTAL_ATTEMPTED,REPORT_TOTAL_LANDED,REPORT_TD_ATTEMPTED,REPORT_TD_LANDED,REPORT_STUNS};
    for(int fighter=0;fighter<2;++fighter){
        for(size_t i=0;i<sizeof(stats)/sizeof(stats[0]);++i)if(!valid_stat(r,stats[i],fighter))return 0;
        if(r->value[REPORT_SIG_LANDED][fighter]>r->value[REPORT_SIG_ATTEMPTED][fighter]||
           r->value[REPORT_TOTAL_LANDED][fighter]>r->value[REPORT_TOTAL_ATTEMPTED][fighter]||
           r->value[REPORT_TD_LANDED][fighter]>r->value[REPORT_TD_ATTEMPTED][fighter])return 0;
        float control=game_float(r->value[REPORT_CONTROL][fighter]);
        if(!isfinite(control)||control<0||control>9999)return 0;
    }
    return 1;
}
static int format_stats(const FightReportRaw *r,int fighter,char *out,size_t count){
    float control=game_float(r->value[REPORT_CONTROL][fighter]);
    int length=snprintf(out,count,
        "{\"knockdowns\":%u,\"stuns\":%u,\"significantStrikesAttempted\":%u,"
        "\"significantStrikesLanded\":%u,\"totalStrikesAttempted\":%u,"
        "\"totalStrikesLanded\":%u,\"takedownsAttempted\":%u,\"takedownsLanded\":%u,"
        "\"controlTimeSeconds\":%d}",
        r->value[REPORT_KNOCKDOWNS][fighter],r->value[REPORT_STUNS][fighter],
        r->value[REPORT_SIG_ATTEMPTED][fighter],r->value[REPORT_SIG_LANDED][fighter],
        r->value[REPORT_TOTAL_ATTEMPTED][fighter],r->value[REPORT_TOTAL_LANDED][fighter],
        r->value[REPORT_TD_ATTEMPTED][fighter],r->value[REPORT_TD_LANDED][fighter],
        (int)(control+0.5f));
    return length>0&&(size_t)length<count;
}
int fight_report_format_result(const FightReportRaw *raw,const char *version,const char *engine,
                               const char *id,unsigned long long captured_at,
                               char *json,size_t capacity){
    int winner=0,round=0,seconds=0;
    if(!raw||!version||!engine||!id||!json||
       !report_result_valid(raw,&winner,&round,&seconds))return 0;
    uint32_t method=raw->value[REPORT_RESULT_TYPE][0];
    int length=snprintf(json,capacity,
        "{\"schema\":1,\"id\":\"%s\",\"reportStage\":\"result\",\"fightStatisticsComplete\":false,"
        "\"gameVersion\":\"%s\",\"engine\":\"%s\",\"capturedAt\":%llu,"
        "\"result\":{\"winnerCorner\":\"%s\",\"method\":%s,\"gameMethodCode\":%u,"
        "\"round\":%d,\"clockSeconds\":%d},\"fighters\":["
        "{\"corner\":\"red\",\"name\":null},{\"corner\":\"blue\",\"name\":null}]}\n",
        id,version,engine,captured_at,winner?"red":"blue",
        method==0?"\"Knockout\"":method==1?"\"TKO\"":method==2?"\"Submission\"":"null",
        method,round,seconds);
    return length>0&&(size_t)length<capacity;
}
int fight_report_format(const FightReportRaw *raw,const char *version,const char *engine,
                        const char *id,unsigned long long captured_at,
                        char *json,size_t capacity){
    int winner=0,round=0,seconds=0;char red[384],blue[384];
    if(!raw||!version||!engine||!id||!json||
       !report_valid(raw,&winner,&round,&seconds)||
       !format_stats(raw,1,red,sizeof(red))||!format_stats(raw,0,blue,sizeof(blue)))return 0;
    /* Names are left null until the game's fighter-name binding is verified. */
    int length=snprintf(json,capacity,
        "{\"schema\":1,\"id\":\"%s\",\"reportStage\":\"result\",\"fightStatisticsComplete\":false,"
        "\"gameVersion\":\"%s\",\"engine\":\"%s\","
        "\"capturedAt\":%llu,\"result\":{\"winnerCorner\":\"%s\",\"method\":%s,\"gameMethodCode\":%u,"
        "\"round\":%d,\"clockSeconds\":%d},\"fighters\":["
        "{\"corner\":\"red\",\"name\":null,\"stats\":%s},"
        "{\"corner\":\"blue\",\"name\":null,\"stats\":%s}]}\n",
        id,version,engine,captured_at,winner? "red":"blue",
        raw->value[REPORT_RESULT_TYPE][0]==0?"\"Knockout\"":
        raw->value[REPORT_RESULT_TYPE][0]==1?"\"TKO\"":
        raw->value[REPORT_RESULT_TYPE][0]==2?"\"Submission\"":"null",
        raw->value[REPORT_RESULT_TYPE][0],
        round,seconds,red,blue);
    return length>0&&(size_t)length<capacity;
}
static int make_directory(const wchar_t *path){
    return CreateDirectoryW(path,NULL)||GetLastError()==ERROR_ALREADY_EXISTS;
}
static void set_error(wchar_t *out,size_t count,const wchar_t *message){
    if(out&&count)lstrcpynW(out,message,(int)count);
}
int fight_report_export(const wchar_t *game_version,int new_engine,
                        wchar_t *path,size_t path_count,wchar_t *error,size_t error_count){
    FightReportRaw first={0},second={0};
    if(!report_read(&first)){set_error(error,error_count,L"Fight report unavailable for this game version.");return 0;}
    Sleep(120);
    if(!report_read(&second)||memcmp(&first,&second,sizeof(first))){
        set_error(error,error_count,L"Fight data changed during capture. Try again.");return 0;
    }
    char version[24]="unknown";
    if(game_version&&game_version[0]){
        int i=0;for(;i<22&&game_version[i];++i){
            wchar_t c=game_version[i];
            if(!((c>=L'0'&&c<=L'9')||c==L'.'))break;
            version[i]=(char)c;
        }
        if(i&&i<23)version[i]=0;
    }
    GUID guid;if(FAILED(CoCreateGuid(&guid))){set_error(error,error_count,L"Could not create a report ID.");return 0;}
    char id[40];snprintf(id,sizeof(id),"%08lx-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
        (unsigned long)guid.Data1,guid.Data2,guid.Data3,
        guid.Data4[0],guid.Data4[1],guid.Data4[2],guid.Data4[3],guid.Data4[4],
        guid.Data4[5],guid.Data4[6],guid.Data4[7]);
    FILETIME ft;GetSystemTimeAsFileTime(&ft);ULARGE_INTEGER timestamp;
    timestamp.LowPart=ft.dwLowDateTime;timestamp.HighPart=ft.dwHighDateTime;
    unsigned long long milliseconds=(timestamp.QuadPart-116444736000000000ULL)/10000ULL;
    char json[2048];
    if(!fight_report_format(&second,version,new_engine?"ps5-pro":"original-parsec",
                            id,milliseconds,json,sizeof(json))){
        set_error(error,error_count,L"Open a finished fight's Statistics screen, then try again.");return 0;
    }
    PWSTR documents=NULL;
    if(FAILED(SHGetKnownFolderPath(&documents_id,KF_FLAG_CREATE,NULL,&documents))||!documents){
        set_error(error,error_count,L"Windows Documents folder is unavailable.");return 0;
    }
    wchar_t parent[MAX_PATH],folder[MAX_PATH],target[MAX_PATH];
    int ok=swprintf(parent,MAX_PATH,L"%ls\\Parsectagon",documents)>0&&
           swprintf(folder,MAX_PATH,L"%ls\\Fight Reports",parent)>0;
    CoTaskMemFree(documents);
    if(!ok||!make_directory(parent)||!make_directory(folder)){
        set_error(error,error_count,L"Could not create Documents\\Parsectagon\\Fight Reports.");return 0;
    }
    SYSTEMTIME now;GetLocalTime(&now);
    wchar_t short_id[9];
    for(int i=0;i<8;++i)short_id[i]=(wchar_t)(unsigned char)id[i];
    short_id[8]=0;
    if(swprintf(target,MAX_PATH,L"%ls\\fight-%04u%02u%02u-%02u%02u%02u-%ls.json",folder,
                now.wYear,now.wMonth,now.wDay,now.wHour,now.wMinute,now.wSecond,short_id)<=0){
        set_error(error,error_count,L"Could not name the fight report.");return 0;
    }
    HANDLE file=CreateFileW(target,GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
    if(file==INVALID_HANDLE_VALUE){set_error(error,error_count,L"Could not create the fight report file.");return 0;}
    DWORD written=0,size=(DWORD)strlen(json);
    ok=WriteFile(file,json,size,&written,NULL)&&written==size&&FlushFileBuffers(file);
    CloseHandle(file);
    if(!ok){DeleteFileW(target);set_error(error,error_count,L"Could not save the fight report.");return 0;}
    if(path&&path_count)lstrcpynW(path,target,(int)path_count);
    if(error&&error_count)error[0]=0;
    return 1;
}

/* Persist the smallest useful result first. Only seven verified bindings are
   needed, so this stage is independent of the later Fight Statistics table. */
int fight_report_export_result(const wchar_t *game_version,int new_engine,
                               wchar_t *path,size_t path_count,wchar_t *error,size_t error_count){
    FightReportRaw first={0},second={0};
    if(!report_read_count(&first,REPORT_MATCH_TIME+1)){
        set_error(error,error_count,L"Finished-fight result is not ready.");return 0;
    }
    Sleep(10);
    if(!report_read_count(&second,REPORT_MATCH_TIME+1)||memcmp(&first,&second,sizeof(first))){
        set_error(error,error_count,L"Fight result changed during capture.");return 0;
    }
    char version[24]="unknown";
    if(game_version&&game_version[0]){
        int i=0;for(;i<22&&game_version[i];++i){
            wchar_t c=game_version[i];
            if(!((c>=L'0'&&c<=L'9')||c==L'.'))break;
            version[i]=(char)c;
        }
        if(i&&i<23)version[i]=0;
    }
    GUID guid;if(FAILED(CoCreateGuid(&guid))){set_error(error,error_count,L"Could not create a result ID.");return 0;}
    char id[40];snprintf(id,sizeof(id),"%08lx-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
        (unsigned long)guid.Data1,guid.Data2,guid.Data3,
        guid.Data4[0],guid.Data4[1],guid.Data4[2],guid.Data4[3],guid.Data4[4],
        guid.Data4[5],guid.Data4[6],guid.Data4[7]);
    FILETIME ft;GetSystemTimeAsFileTime(&ft);ULARGE_INTEGER timestamp;
    timestamp.LowPart=ft.dwLowDateTime;timestamp.HighPart=ft.dwHighDateTime;
    unsigned long long milliseconds=(timestamp.QuadPart-116444736000000000ULL)/10000ULL;
    char json[1024];
    if(!fight_report_format_result(&second,version,new_engine?"ps5-pro":"original-parsec",
                                   id,milliseconds,json,sizeof(json))){
        set_error(error,error_count,L"Finished-fight result is not ready.");return 0;
    }
    PWSTR documents=NULL;
    if(FAILED(SHGetKnownFolderPath(&documents_id,KF_FLAG_CREATE,NULL,&documents))||!documents){
        set_error(error,error_count,L"Windows Documents folder is unavailable.");return 0;
    }
    wchar_t parent[MAX_PATH],folder[MAX_PATH],target[MAX_PATH];
    int ok=swprintf(parent,MAX_PATH,L"%ls\\Parsectagon",documents)>0&&
           swprintf(folder,MAX_PATH,L"%ls\\Fight Reports",parent)>0;
    CoTaskMemFree(documents);
    if(!ok||!make_directory(parent)||!make_directory(folder)){
        set_error(error,error_count,L"Could not create Documents\\Parsectagon\\Fight Reports.");return 0;
    }
    wchar_t short_id[9];for(int i=0;i<8;++i)short_id[i]=(wchar_t)(unsigned char)id[i];short_id[8]=0;
    if(swprintf(target,MAX_PATH,L"%ls\\fight-result-%ls.json",folder,short_id)<=0){
        set_error(error,error_count,L"Could not name the result file.");return 0;
    }
    HANDLE file=CreateFileW(target,GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
    if(file==INVALID_HANDLE_VALUE){set_error(error,error_count,L"Could not create the result file.");return 0;}
    DWORD written=0,size=(DWORD)strlen(json);
    ok=WriteFile(file,json,size,&written,NULL)&&written==size&&FlushFileBuffers(file);
    CloseHandle(file);
    if(!ok){DeleteFileW(target);set_error(error,error_count,L"Could not save the result.");return 0;}
    if(path&&path_count)lstrcpynW(path,target,(int)path_count);
    if(error&&error_count)error[0]=0;
    return 1;
}
