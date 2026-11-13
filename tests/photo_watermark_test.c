#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#undef NDEBUG
#include <assert.h>
#include <stdio.h>
#include "photo_effects.h"
#include "photo_watermark_raster.h"
static unsigned char raster[PHOTO_MARK_BYTES+16];
static uint64_t hash(void){uint64_t h=14695981039346656037ull;for(unsigned i=0;i<PHOTO_MARK_BYTES;++i){h^=raster[i];h*=1099511628211ull;}return h;}
int main(void){PhotoEffects p;photo_effects_default(&p);assert(!photo_watermark_raster(&p,raster,sizeof(raster)));wcscpy(p.name,L"Photo \x03a9 Creator");assert(!photo_watermark_raster(&p,raster,16));uint64_t styles[4];
 for(int style=0;style<4;++style){p.mark_style=style;
  uint64_t fonts[4];for(int font=0;font<4;++font){p.mark_font=font;memset(raster+PHOTO_MARK_BYTES,0x5a,16);uint32_t width=photo_watermark_raster(&p,raster,PHOTO_MARK_BYTES);assert(width>20&&width<=PHOTO_MARK_W);for(int i=0;i<16;++i)assert(raster[PHOTO_MARK_BYTES+i]==0x5a);fonts[font]=hash();assert(fonts[font]);uint32_t again=photo_watermark_raster(&p,raster,PHOTO_MARK_BYTES);assert(again==width&&fonts[font]==hash());}assert(fonts[0]!=fonts[2]&&fonts[2]!=fonts[3]);styles[style]=fonts[0];}
 assert(styles[0]!=styles[1]&&styles[1]!=styles[2]&&styles[2]!=styles[3]);for(int i=0;i<63;++i)p.name[i]=L'W';p.name[63]=0;assert(photo_watermark_raster(&p,raster,PHOTO_MARK_BYTES)<=PHOTO_MARK_W);
 puts("PASS: four watermark layouts and font families, Unicode/long-name fitting, cache invalidation, missing-name/capacity guards and exact output bounds.");return 0;}
