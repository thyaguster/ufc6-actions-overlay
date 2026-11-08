#ifndef UFC6_OVERLAY_LAYOUT_H
#define UFC6_OVERLAY_LAYOUT_H
#include <stdint.h>

#define MENU_BASE_WIDTH 570u
#define MENU_BASE_HEIGHT 372u
#define MENU_SCALE_MIN 50
#define MENU_SCALE_MAX 300
#define MENU_SCALE_STEP 25
#define MENU_SCALE_DEFAULT 100

static inline int overlay_valid_scale(int value) {
    return value>=MENU_SCALE_MIN&&value<=MENU_SCALE_MAX&&value%MENU_SCALE_STEP==0;
}
static inline void overlay_extent(int percent,uint32_t viewport_width,uint32_t viewport_height,uint32_t *width,uint32_t *height) {
    *width=0;*height=0;
    if(viewport_width<=32||viewport_height<=32)return;
    if(!overlay_valid_scale(percent))percent=MENU_SCALE_DEFAULT;
    uint32_t w=MENU_BASE_WIDTH*(uint32_t)percent/100,h=MENU_BASE_HEIGHT*(uint32_t)percent/100;
    uint32_t max_width=viewport_width-32,max_height=viewport_height-32;
    if(w>max_width){h=(uint32_t)((uint64_t)h*max_width/w);w=max_width;}
    if(h>max_height){w=(uint32_t)((uint64_t)w*max_height/h);h=max_height;}
    *width=w;*height=h;
}
#endif
