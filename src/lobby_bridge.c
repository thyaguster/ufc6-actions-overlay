#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winhttp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include "lobby_bridge.h"

typedef struct {int start,end,next;char type;} JsonToken;
typedef struct {const char *text;size_t length,pos;int count;JsonToken token[4096];} Json;
static SRWLOCK lobby_guard=SRWLOCK_INIT;
static LobbySnapshot lobby_current;
static wchar_t lobby_settings[MAX_PATH];
static HANDLE lobby_stop,lobby_wake,lobby_thread;
static volatile LONG lobby_enabled;

static void json_space(Json *j){while(j->pos<j->length&&(j->text[j->pos]==' '||j->text[j->pos]=='\n'||j->text[j->pos]=='\r'||j->text[j->pos]=='\t'))++j->pos;}
static int json_value(Json *j,int depth){
    if(depth>30||j->count>=4096)return -1;
    json_space(j);if(j->pos>=j->length)return -1;
    int index=j->count++;JsonToken *t=&j->token[index];char c=j->text[j->pos];t->type=c;
    if(c=='{'||c=='['){
        ++j->pos;json_space(j);char end=c=='{'?'}':']';
        while(j->pos<j->length&&j->text[j->pos]!=end){
            if(c=='{'){if(j->text[j->pos]!='"'||json_value(j,depth+1)<0)return -1;json_space(j);if(j->pos>=j->length||j->text[j->pos++]!=':')return -1;}
            if(json_value(j,depth+1)<0)return -1;
            json_space(j);if(j->pos<j->length&&j->text[j->pos]==','){++j->pos;json_space(j);continue;}
            if(j->pos>=j->length||j->text[j->pos]!=end)return -1;
        }
        if(j->pos>=j->length)return -1;++j->pos;
    }else if(c=='"'){
        t->start=(int)++j->pos;
        while(j->pos<j->length){
            unsigned char ch=(unsigned char)j->text[j->pos++];
            if(ch=='"'){t->end=(int)j->pos-1;t->next=j->count;return index;}
            if(ch<32)return -1;
            if(ch=='\\'){if(j->pos>=j->length)return -1;char escaped=j->text[j->pos++];if(escaped=='u'){for(int k=0;k<4;++k){if(j->pos>=j->length||!((j->text[j->pos]>='0'&&j->text[j->pos]<='9')||(j->text[j->pos]>='a'&&j->text[j->pos]<='f')||(j->text[j->pos]>='A'&&j->text[j->pos]<='F')))return -1;++j->pos;}}else if(!strchr("\"\\/bfnrt",escaped))return -1;}
        }
        return -1;
    }else{
        t->start=(int)j->pos;
        while(j->pos<j->length&&!strchr(",]} \n\r\t",j->text[j->pos]))++j->pos;
        if(j->pos==(size_t)t->start)return -1;
        t->end=(int)j->pos;
    }
    t->next=j->count;return index;
}
static int json_key(const Json *j,int token,const char *key){const JsonToken *t=&j->token[token];size_t n=strlen(key);return t->type=='"'&&t->end-t->start==(int)n&&!memcmp(j->text+t->start,key,n);}
static int json_member(const Json *j,int object,const char *key){
    if(object<0||j->token[object].type!='{')return -1;
    for(int at=object+1;at<j->token[object].next;){int value=at+1;if(value>=j->count)return -1;if(json_key(j,at,key))return value;at=j->token[value].next;}
    return -1;
}
static int json_int(const Json *j,int token,int *out){
    if(token<0)return 0;const JsonToken *t=&j->token[token];int n=t->end-t->start;if(n<1||n>12)return 0;
    char text[16];memcpy(text,j->text+t->start,(size_t)n);text[n]=0;char *end;long value=strtol(text,&end,10);if(*end||value<0||value>100000000)return 0;*out=(int)value;return 1;
}
static int json_bool(const Json *j,int token,int *out){
    if(token<0)return 0;const JsonToken *t=&j->token[token];int n=t->end-t->start;const char *p=j->text+t->start;
    if(n==4&&!memcmp(p,"true",4)){*out=1;return 1;}if(n==5&&!memcmp(p,"false",5)){*out=0;return 1;}return 0;
}
static int json_string(const Json *j,int token,wchar_t *out,size_t count){
    if(token<0||j->token[token].type!='"'||count<2)return 0;
    const JsonToken *t=&j->token[token];size_t n=(size_t)(t->end-t->start);if(n>4096)return 0;
    char utf8[12289];size_t written=0;
    for(int i=t->start;i<t->end;){unsigned char c=(unsigned char)j->text[i++];
        if(c!='\\'){utf8[written++]=(char)c;continue;}
        if(i>=t->end)return 0;c=(unsigned char)j->text[i++];
        if(c=='u'){
            if(i+4>t->end)return 0;unsigned value=0;for(int k=0;k<4;++k){char h=j->text[i++];value=(value<<4)+(h>='0'&&h<='9'?h-'0':h>='a'&&h<='f'?h-'a'+10:h-'A'+10);}
            if(value<0x80)utf8[written++]=(char)value;
            else if(value<0x800){utf8[written++]=(char)(0xc0|(value>>6));utf8[written++]=(char)(0x80|(value&63));}
            else {utf8[written++]=(char)(0xe0|(value>>12));utf8[written++]=(char)(0x80|((value>>6)&63));utf8[written++]=(char)(0x80|(value&63));}
        }else utf8[written++]=c=='n'?'\n':c=='r'?'\r':c=='t'?'\t':c=='b'?'\b':c=='f'?'\f':(char)c;
    }
    int needed=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,utf8,(int)written,NULL,0);
    if(needed<0||needed>=(int)count)return 0;
    if(needed&&!MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,utf8,(int)written,out,needed))return 0;
    out[needed]=0;return 1;
}
static int room_id_valid(const wchar_t *id){size_t n=wcslen(id);if(n<1||n>64)return 0;for(size_t i=0;i<n;++i)if(!((id[i]>='a'&&id[i]<='z')||(id[i]>='A'&&id[i]<='Z')||(id[i]>='0'&&id[i]<='9')||id[i]=='-'||id[i]=='_'))return 0;return 1;}
int lobby_parse_snapshot(const char *text,size_t length,LobbySnapshot *out){
    if(!text||!out||length<2||length>262144)return 0;
    Json j={.text=text,.length=length};int root=json_value(&j,0);json_space(&j);if(root!=0||j.pos!=length||j.token[0].type!='{')return 0;
    int version,online,looking,open_rooms;int presence=json_member(&j,0,"presence"),rooms=json_member(&j,0,"rooms");
    if(!json_int(&j,json_member(&j,0,"version"),&version)||version!=1||presence<0||rooms<0||j.token[rooms].type!='['||
       !json_int(&j,json_member(&j,presence,"online"),&online)||!json_int(&j,json_member(&j,presence,"looking"),&looking)||!json_int(&j,json_member(&j,presence,"openRooms"),&open_rooms))return 0;
    LobbySnapshot result={0};result.connected=1;result.online=online;result.looking=looking;result.open_rooms=open_rooms;
    for(int at=rooms+1;at<j.token[rooms].next;at=j.token[at].next){
        if(j.token[at].type!='{')return 0;
        int demo=0,active=0,players=0;
        int d=json_member(&j,at,"demo");if(d>=0&&!json_bool(&j,d,&demo))return 0;
        int preview=json_member(&j,at,"preview"),sample=0;if(preview>=0&&!json_bool(&j,preview,&sample))return 0;
        if(demo)continue;
        if(!json_bool(&j,json_member(&j,at,"hostActive"),&active)||!json_int(&j,json_member(&j,at,"players"),&players))return 0;
        if(!sample&&(!active||players!=1))continue;
        LobbyRoom room={0};room.host_active=active;room.players=players;room.preview=sample;
        if(!json_string(&j,json_member(&j,at,"id"),room.id,80)||!room_id_valid(room.id)||
           !json_string(&j,json_member(&j,at,"title"),room.title,96)||
           !json_string(&j,json_member(&j,at,"host"),room.host,64)||
           !json_string(&j,json_member(&j,at,"region"),room.region,48)||
           !json_string(&j,json_member(&j,at,"hostRank"),room.rank,48))return 0;
        int mode=json_member(&j,at,"mode");if(mode>=0&&!json_string(&j,mode,room.mode,24))return 0;
        int notes=json_member(&j,at,"notes");if(notes>=0&&!json_string(&j,notes,room.notes,160))return 0;
        int host_avatar=json_member(&j,at,"hostAvatar");if(host_avatar>=0&&!json_string(&j,host_avatar,room.host_avatar,24))return 0;
        int guest=json_member(&j,at,"guestFighter");
        if(guest>=0&&j.token[guest].type!='n'){
            if(j.token[guest].type!='{'||
               !json_string(&j,json_member(&j,guest,"name"),room.guest_name,64))return 0;
            room.guest_present=1;
            int avatar=json_member(&j,guest,"avatar");if(avatar>=0&&!json_string(&j,avatar,room.guest_avatar,24))return 0;
            int guest_rating=json_member(&j,guest,"rating");if(guest_rating>=0&&j.token[guest_rating].type!='n'&&!json_int(&j,guest_rating,&room.guest_rating))return 0;
            int guest_qualified=json_member(&j,guest,"qualified");if(guest_qualified>=0&&!json_bool(&j,guest_qualified,&room.guest_qualified))return 0;
            int guest_wins=json_member(&j,guest,"wins");if(guest_wins>=0&&!json_int(&j,guest_wins,&room.guest_wins))return 0;
            int guest_losses=json_member(&j,guest,"losses");if(guest_losses>=0&&!json_int(&j,guest_losses,&room.guest_losses))return 0;
        }
        int rating=json_member(&j,at,"hostRating");if(rating>=0&&j.token[rating].type!='n'&&!json_int(&j,rating,&room.rating))return 0;
        int qualified=json_member(&j,at,"hostQualified");if(qualified>=0&&!json_bool(&j,qualified,&room.qualified))return 0;
        int wins=json_member(&j,at,"hostWins"),losses=json_member(&j,at,"hostLosses"),draws=json_member(&j,at,"hostDraws");
        if(wins>=0&&!json_int(&j,wins,&room.wins))return 0;if(losses>=0&&!json_int(&j,losses,&room.losses))return 0;
        if(draws>=0){if(!json_int(&j,draws,&room.draws))return 0;room.draws_known=1;}
        if(result.room_count<LOBBY_MAX_ROOMS){
            if(room.preview){
                result.preview_room_count++;
                if(room.host_active&&room.players<2)result.open_rooms++;
                int new_host=1;for(int i=0;i<result.room_count;++i)if(result.rooms[i].preview&&!_wcsicmp(result.rooms[i].host,room.host)){new_host=0;break;}
                if(new_host)result.online++;
            }
            result.rooms[result.room_count++]=room;
        }
    }
    *out=result;return 1;
}
static int lobby_url_valid(const wchar_t *url){
    size_t n=wcslen(url);if(n<10||n>=LOBBY_URL_LENGTH)return 0;
    if(_wcsnicmp(url,L"http://",7)&&_wcsnicmp(url,L"https://",8))return 0;
    for(const wchar_t *p=url;*p;++p)if(*p=='@'||*p=='?'||*p=='#'||*p==' '||*p=='\r'||*p=='\n')return 0;
    URL_COMPONENTS parts={.dwStructSize=sizeof(parts)};parts.dwHostNameLength=(DWORD)-1;parts.dwUrlPathLength=(DWORD)-1;
    if(!WinHttpCrackUrl(url,0,0,&parts)||!parts.dwHostNameLength)return 0;
    return parts.dwUrlPathLength==0||(parts.dwUrlPathLength==1&&parts.lpszUrlPath[0]=='/');
}
int lobby_room_url(const wchar_t *base,const wchar_t *room_id,wchar_t *out,size_t count){
    if(!base||!room_id||!out||!lobby_url_valid(base)||!room_id_valid(room_id))return 0;
    size_t n=wcslen(base);while(n&&base[n-1]=='/')--n;
    if(n+wcslen(room_id)+9>=count)return 0;
    wmemcpy(out,base,n);out[n]=0;wcscat(out,L"/?room=");wcscat(out,room_id);return 1;
}
static void lobby_publish(const LobbySnapshot *state){AcquireSRWLockExclusive(&lobby_guard);lobby_current=*state;ReleaseSRWLockExclusive(&lobby_guard);}
static void lobby_error(const wchar_t *url,const wchar_t *message){LobbySnapshot state={0};lstrcpynW(state.url,url,LOBBY_URL_LENGTH);lstrcpynW(state.error,message,160);state.updated_at=GetTickCount64();lobby_publish(&state);}
static void lobby_poll(void){
    wchar_t url[LOBBY_URL_LENGTH],request_path[128];GetPrivateProfileStringW(L"Menu",L"LobbyServerURL",L"http://127.0.0.1:3000",url,LOBBY_URL_LENGTH,lobby_settings);
    if(!lobby_url_valid(url)){lobby_error(url,L"Invalid server URL. Edit LobbyServerURL in overlay settings.");return;}
    URL_COMPONENTS parts={.dwStructSize=sizeof(parts)};parts.dwHostNameLength=(DWORD)-1;parts.dwUrlPathLength=(DWORD)-1;
    if(!WinHttpCrackUrl(url,0,0,&parts)){lobby_error(url,L"Invalid server URL.");return;}
    wchar_t host[256];if(parts.dwHostNameLength>=256){lobby_error(url,L"Server name is too long.");return;}
    wmemcpy(host,parts.lpszHostName,parts.dwHostNameLength);host[parts.dwHostNameLength]=0;
    wcscpy(request_path,L"/api/bridge/snapshot");
    HINTERNET session=WinHttpOpen(L"UFC6ActionsOverlay/1",WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,NULL,NULL,0);
    if(!session){lobby_error(url,L"Could not start HTTP client.");return;}
    WinHttpSetTimeouts(session,3000,3000,3000,3000);
    HINTERNET connection=WinHttpConnect(session,host,parts.nPort,0),request=NULL;
    if(connection)request=WinHttpOpenRequest(connection,L"POST",request_path,NULL,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,parts.nScheme==INTERNET_SCHEME_HTTPS?WINHTTP_FLAG_SECURE:0);
    DWORD status=0,status_size=sizeof(status);char *body=NULL;size_t used=0,capacity=0;int ok=0;
    if(request&&WinHttpSendRequest(request,L"Content-Type: application/json\r\n",(DWORD)-1,"{}",2,2,0)&&WinHttpReceiveResponse(request,NULL)&&
       WinHttpQueryHeaders(request,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,NULL,&status,&status_size,NULL)&&status==200){
        for(;;){char chunk[4096];DWORD got=0;if(!WinHttpReadData(request,chunk,sizeof(chunk),&got))break;if(!got){ok=1;break;}
            if(used+got>262144)break;if(used+got+1>capacity){size_t next=capacity?capacity*2:8192;while(next<used+got+1)next*=2;char *memory=realloc(body,next);if(!memory)break;body=memory;capacity=next;}
            memcpy(body+used,chunk,got);used+=got;
        }
    }
    if(ok){LobbySnapshot state={0};if(lobby_parse_snapshot(body,used,&state)){lstrcpynW(state.url,url,LOBBY_URL_LENGTH);state.updated_at=GetTickCount64();lobby_publish(&state);}else lobby_error(url,L"Lobby sent an unsupported snapshot.");}
    else if(status&&status!=200){wchar_t message[160];swprintf(message,160,L"Lobby server returned HTTP %lu.",status);lobby_error(url,message);}
    else lobby_error(url,L"Cannot reach the lobby server. Check its URL and connection.");
    free(body);if(request)WinHttpCloseHandle(request);if(connection)WinHttpCloseHandle(connection);WinHttpCloseHandle(session);
}
static DWORD WINAPI lobby_worker(void *unused){(void)unused;HANDLE events[]={lobby_stop,lobby_wake};
    for(;;){DWORD result=WaitForMultipleObjects(2,events,FALSE,InterlockedCompareExchange(&lobby_enabled,0,0)?5000:INFINITE);if(result==WAIT_OBJECT_0)break;if(InterlockedCompareExchange(&lobby_enabled,0,0))lobby_poll();}
    return 0;
}
void lobby_bridge_start(const wchar_t *settings_file){
    lstrcpynW(lobby_settings,settings_file,MAX_PATH);lobby_stop=CreateEventW(NULL,TRUE,FALSE,NULL);lobby_wake=CreateEventW(NULL,FALSE,FALSE,NULL);
    if(lobby_stop&&lobby_wake)lobby_thread=CreateThread(NULL,0,lobby_worker,NULL,0,NULL);
    if(!lobby_thread){lobby_error(L"",L"Lobby polling could not start.");if(lobby_stop)CloseHandle(lobby_stop);if(lobby_wake)CloseHandle(lobby_wake);lobby_stop=lobby_wake=NULL;}
}
void lobby_bridge_stop(void){if(lobby_stop)SetEvent(lobby_stop);if(lobby_thread){WaitForSingleObject(lobby_thread,15000);CloseHandle(lobby_thread);lobby_thread=NULL;}if(lobby_stop)CloseHandle(lobby_stop);if(lobby_wake)CloseHandle(lobby_wake);lobby_stop=lobby_wake=NULL;}
void lobby_bridge_active(int active){InterlockedExchange(&lobby_enabled,!!active);if(active&&lobby_wake)SetEvent(lobby_wake);}
void lobby_bridge_refresh(void){if(lobby_wake)SetEvent(lobby_wake);}
void lobby_bridge_snapshot(LobbySnapshot *out){AcquireSRWLockShared(&lobby_guard);*out=lobby_current;ReleaseSRWLockShared(&lobby_guard);}
