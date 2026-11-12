#ifndef UFC6_SCREENSHOT_OVERLAY_H
#define UFC6_SCREENSHOT_OVERLAY_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <math.h>
/* Compose the exact raster used for THIS presented frame, after the startup
   recognizer reads the clean capture. No extra GPU readback or persistent work. */
static int screenshot_overlay_compose(unsigned char *game,uint32_t width,uint32_t height,const void *overlay,uint32_t ow,uint32_t oh,uint32_t x,uint32_t y,int blend,int rgba,int srgb){
 if(!game||!overlay||!width||!height||!ow||!oh||x>width||y>height||ow>width-x||oh>height-y||(uint64_t)width*height>SIZE_MAX/4)return 0;
 if(!blend){for(uint32_t row=0;row<oh;++row)memcpy(game+((size_t)(y+row)*width+x)*4,(const unsigned char*)overlay+(size_t)row*ow*4,(size_t)ow*4);return 1;}
 float linear[256];if(srgb)for(int i=0;i<256;++i){float v=i/255.f;linear[i]=v<=.04045f?v/12.92f:powf((v+.055f)/1.055f,2.4f);}
 const float *source=overlay;
 for(uint32_t row=0;row<oh;++row)for(uint32_t col=0;col<ow;++col){
  const float *s=source+((size_t)row*ow+col)*4;unsigned char *d=game+((size_t)(y+row)*width+x+col)*4;float a=fmaxf(0,fminf(1,s[3]));
  for(int c=0;c<3;++c){int channel=rgba?c:2-c;float bg=srgb?linear[d[channel]]:d[channel]/255.f;float value=s[c]*a+bg*(1-a);
   if(srgb)value=value<=.0031308f?value*12.92f:1.055f*powf(value,1.f/2.4f)-.055f;
   d[channel]=(unsigned char)(fmaxf(0,fminf(1,value))*255.f+.5f);
  }d[3]=255;
 }
 return 1;
}
#endif
