#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include "lobby_bridge.h"
#define assert(condition) do { if(!(condition)){fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#condition);return 1;} } while(0)

int main(void){
    const char *body="{\"version\":1,\"presence\":{\"online\":5,\"looking\":2,\"openRooms\":1},\"rooms\":["
        "{\"id\":\"real-1\",\"title\":\"Fight Night\",\"host\":\"Alex\",\"hostRank\":\"silver\",\"hostQualified\":true,\"hostRating\":1220,\"hostWins\":3,\"hostLosses\":2,\"hostDraws\":1,\"hostActive\":true,\"players\":1,\"region\":\"EU\"},"
        "{\"id\":\"demo-1\",\"demo\":true},"
        "{\"id\":\"preview-1\",\"title\":\"Practice rounds\",\"host\":\"Sample\",\"hostRank\":\"gold\",\"hostRating\":1300,\"hostActive\":true,\"players\":1,\"region\":\"Europe\",\"preview\":true},"
        "{\"id\":\"preview-2\",\"title\":\"Full sample\",\"notes\":\"Five rounds\",\"host\":\"Sample\",\"hostAvatar\":\"champion\",\"hostRank\":\"gold\",\"hostRating\":1300,\"guestFighter\":{\"name\":\"Challenger\",\"avatar\":\"rookie\",\"rating\":1100,\"qualified\":true,\"wins\":2,\"losses\":1},\"hostActive\":true,\"players\":2,\"region\":\"Europe\",\"preview\":true},"
        "{\"id\":\"full-1\",\"title\":\"Full\",\"host\":\"B\",\"hostRank\":\"gold\",\"hostActive\":true,\"players\":2,\"region\":\"EU\"}]}";
    LobbySnapshot snapshot={0};assert(lobby_parse_snapshot(body,strlen(body),&snapshot));
    assert(snapshot.connected&&snapshot.online==6&&snapshot.looking==2&&snapshot.open_rooms==2&&snapshot.room_count==3&&snapshot.preview_room_count==2);
    assert(!wcscmp(snapshot.rooms[0].id,L"real-1")&&snapshot.rooms[0].rating==1220&&snapshot.rooms[0].wins==3&&snapshot.rooms[0].draws==1&&snapshot.rooms[0].draws_known&&snapshot.rooms[0].qualified);
    assert(!snapshot.rooms[0].preview&&snapshot.rooms[1].preview&&snapshot.rooms[2].preview&&snapshot.rooms[2].players==2);
    assert(snapshot.rooms[2].guest_present&&snapshot.rooms[2].guest_wins==2&&snapshot.rooms[2].guest_qualified);
    assert(!wcscmp(snapshot.rooms[2].guest_name,L"Challenger")&&!wcscmp(snapshot.rooms[2].guest_avatar,L"rookie")&&!wcscmp(snapshot.rooms[2].host_avatar,L"champion")&&!wcscmp(snapshot.rooms[2].notes,L"Five rounds"));
    wchar_t url[400];assert(lobby_room_url(L"http://127.0.0.1:3000/",snapshot.rooms[0].id,url,400));
    assert(!wcscmp(url,L"http://127.0.0.1:3000/?room=real-1"));
    assert(!lobby_room_url(L"http://127.0.0.1:3000",L"bad&room=x",url,400));
    assert(!lobby_parse_snapshot("{\"version\":2}",13,&snapshot));

    wchar_t ini[MAX_PATH];GetTempPathW(MAX_PATH,ini);wcscat(ini,L"ufc6-lobby-unavailable-test.ini");
    WritePrivateProfileStringW(L"Menu",L"LobbyServerURL",L"http://127.0.0.1:1",ini);
    lobby_bridge_start(ini);lobby_bridge_active(1);
    int failed=0;for(int i=0;i<60;++i){Sleep(100);lobby_bridge_snapshot(&snapshot);if(snapshot.error[0]){failed=1;break;}}
    lobby_bridge_active(0);lobby_bridge_stop();DeleteFileW(ini);
    assert(failed&&!snapshot.connected&&snapshot.room_count==0&&snapshot.error[0]);
    puts("PASS: public rooms, counts/ranks, demo/full filtering, safe browser URL, and unreachable server error.");
    return 0;
}
