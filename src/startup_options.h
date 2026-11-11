#ifndef UFC6_STARTUP_OPTIONS_H
#define UFC6_STARTUP_OPTIONS_H
#include <stdint.h>
typedef struct {int owned[3],state;} StartupOptions;
/* Session-only changes to checked startup boolean callbacks. */
int startup_options_tick(StartupOptions *options,uintptr_t guest,int short_movie,int skip_splashes);
#endif
