#ifndef UFC6_XINPUT_MENU_H
#define UFC6_XINPUT_MENU_H
#include <stdint.h>
/* XInput button bits and the engine's pad bits are different namespaces. */
static uint32_t xinput_menu_buttons(uint16_t b){
    uint32_t out=0;
    const uint16_t raw[]={1,2,4,8,0x10,0x40,0x80,0x100,0x200,0x1000,0x2000,0x4000,0x8000};
    const uint32_t pad[]={0x10,0x40,0x80,0x20,8,2,4,0x400,0x800,0x4000,0x2000,0x8000,0x1000};
    for(unsigned i=0;i<sizeof(raw)/sizeof(raw[0]);++i)if(b&raw[i])out|=pad[i];return out;
}
static inline uint32_t native_menu_stick(int x,int y){
    return (x<64?0x80u:x>192?0x20u:0)|(y<64?0x10u:y>192?0x40u:0);
}
static inline uint32_t xinput_menu_stick(int x,int y){
    return (x < -16000?0x80u:x>16000?0x20u:0)|(y>16000?0x10u:y < -16000?0x40u:0);
}
#endif
