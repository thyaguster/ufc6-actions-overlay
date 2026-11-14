#undef NDEBUG
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "../src/game_version.h"

int main(void){
    GameVersion v;
    const char *current="{\"originContentVersion\":\"01.005.000\",\"contentVersion\":\"01.006.000\"}";
    assert(game_version_parse_json(current,strlen(current),&v)&&v.known&&!v.older_than_01006);
    assert(!wcscmp(v.label,L"01.006.000"));
    const char *older="{\"contentVersion\": \"01.005.000\"}";
    assert(game_version_parse_json(older,strlen(older),&v)&&v.older_than_01006);
    const char *unknown="{\"contentVersion\":\"unavailable\"}";
    assert(!game_version_parse_json(unknown,strlen(unknown),&v)&&!v.known);
    puts("PASS: installed content version, older-version warning threshold and unknown metadata.");
    return 0;
}
