#ifndef UFC6_GAME_VERSION_H
#define UFC6_GAME_VERSION_H
#include <stddef.h>
#include <wchar.h>

typedef struct { int known, older_than_01006; wchar_t label[20]; } GameVersion;
int game_version_parse_json(const char *json,size_t length,GameVersion *out);
GameVersion game_version_detect(void);

#endif
