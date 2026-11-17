#ifndef UFC6_LOBBY_BRIDGE_H
#define UFC6_LOBBY_BRIDGE_H
#include <windows.h>

#define LOBBY_MAX_ROOMS 24
#define LOBBY_URL_LENGTH 256

typedef struct {
    wchar_t id[80],title[96],host[64],region[48],rank[48],mode[24],notes[160];
    wchar_t host_avatar[24],guest_name[64],guest_avatar[24];
    int rating,wins,losses,draws,draws_known,players,host_active,qualified,preview;
    int guest_present,guest_rating,guest_qualified,guest_wins,guest_losses;
} LobbyRoom;

typedef struct {
    int connected,online,looking,open_rooms,room_count,preview_room_count;
    wchar_t url[LOBBY_URL_LENGTH],error[160];
    int account_status,account_rating; /* 0 unlinked, 1 pending, 2 linked, -1 error */
    wchar_t account_name[64],account_rank[32],account_code[16],account_error[160];
    LobbyRoom rooms[LOBBY_MAX_ROOMS];
    unsigned long long updated_at;
} LobbySnapshot;

int lobby_parse_snapshot(const char *json,size_t length,LobbySnapshot *out);
void lobby_bridge_start(const wchar_t *settings_file);
void lobby_bridge_stop(void);
void lobby_bridge_active(int active);
void lobby_bridge_refresh(void);
void lobby_bridge_snapshot(LobbySnapshot *out);
int lobby_room_url(const wchar_t *base,const wchar_t *room_id,wchar_t *out,size_t count);

#endif
