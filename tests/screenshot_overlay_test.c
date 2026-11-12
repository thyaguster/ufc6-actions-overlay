#include <stdio.h>
#include <string.h>
#include "../src/screenshot_overlay.h"
#undef NDEBUG
#include <assert.h>
int main(void){
 unsigned char game[4*4*4],before[sizeof(game)];memset(game,30,sizeof(game));memcpy(before,game,sizeof(game));
 unsigned char menu[]={1,2,3,255,4,5,6,255};
 assert(screenshot_overlay_compose(game,4,4,menu,2,1,1,2,0,0,0));assert(!memcmp(game+(2*4+1)*4,menu,8));assert(!memcmp(game,before,(2*4+1)*4));assert(game[(3*4)*4]==30);
 memcpy(game,before,sizeof(game));assert(!screenshot_overlay_compose(game,4,4,menu,2,1,3,0,0,0,0));assert(!memcmp(game,before,sizeof(game)));assert(!screenshot_overlay_compose(game,4,4,menu,1,1,5,0,0,0,0));
 const float panel[]={1,0,0,.5f};memset(game,0,sizeof(game));assert(screenshot_overlay_compose(game,4,4,panel,1,1,0,0,1,0,0));assert(game[0]==0&&game[1]==0&&game[2]==128&&game[3]==255);
 memset(game,0,sizeof(game));assert(screenshot_overlay_compose(game,4,4,panel,1,1,0,0,1,1,0));assert(game[0]==128&&game[1]==0&&game[2]==0);
 memset(game,0,sizeof(game));assert(screenshot_overlay_compose(game,4,4,panel,1,1,0,0,1,1,1));assert(game[0]>=187&&game[0]<=188);
 const float clear[]={1,1,1,0};memcpy(game,before,sizeof(game));assert(screenshot_overlay_compose(game,4,4,clear,1,1,0,0,1,0,1));assert(game[0]==30&&game[1]==30&&game[2]==30);
 puts("PASS: same-frame opaque/alpha overlay composition, exact region placement, RGBA/BGRA, linear/sRGB, transparent pixels and bounds rejection.");return 0;
}
