#ifndef UFC6_PHOTO_MODE_H
#define UFC6_PHOTO_MODE_H
#include <stdint.h>
enum { PHOTO_START_NO_FIGHT=0,PHOTO_START_OK=1,PHOTO_START_BUILD=-1,PHOTO_START_HOOKS=-2 };
enum { PHOTO_FREEZE=1,PHOTO_RESET=2,PHOTO_STOP=4,PHOTO_HUD=8,PHOTO_CAGE=16 };
typedef struct {float right,up,forward,yaw,pitch,zoom;int fast;} PhotoInput;
typedef struct {int state,frozen,fov,hud_hidden,cage_visible,cage_available,cage_count,cage_writes,cage_restores;} PhotoSnapshot;
/* UI thread publishes input; native mutations run on the camera update thread. */
void photo_mode_prepare(void);
int photo_mode_start(int fight_ready);
void photo_mode_command(unsigned command);
void photo_mode_poll(int focused,int menu_open,const PhotoInput *input);
int photo_mode_blocks_input(void);
void photo_mode_snapshot(PhotoSnapshot *out);
#endif
