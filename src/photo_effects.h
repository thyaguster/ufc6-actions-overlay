#ifndef UFC6_PHOTO_EFFECTS_H
#define UFC6_PHOTO_EFFECTS_H
#include <stdint.h>
#include <wchar.h>
#define PHOTO_MARK_W 1536
#define PHOTO_MARK_H 128
#define PHOTO_MARK_BYTES (PHOTO_MARK_W*PHOTO_MARK_H*4)
enum { FX_PRESET,FX_STRENGTH,FX_EXPOSURE,FX_CONTRAST,FX_SATURATION,FX_WARMTH,FX_VIGNETTE,FX_GRAIN,FX_BARS,FX_RESET,FX_BACK,FX_ROWS };
enum { MARK_ENABLED,MARK_NAME,MARK_CORNER,MARK_OPACITY,MARK_SIZE,MARK_STYLE,MARK_FONT,MARK_COLOR,MARK_BACK,MARK_ROWS };
typedef struct {int preset,strength,exposure,contrast,saturation,warmth,vignette,grain,bars,watermark,corner,opacity,size,mark_style,mark_font,mark_color;wchar_t name[64];} PhotoEffects;
/* Explicit shared ABI: five vec4 push-constant groups (80 bytes). */
typedef struct {uint32_t width,height,rgba,srgb;float exposure,contrast,saturation,warmth,vignette,grain,bars,preset,strength,corner,opacity,size;uint32_t mark_width,mark_height,reserved[2];} PhotoEffectConstants;
static inline void photo_effects_default(PhotoEffects *p){*p=(PhotoEffects){.strength=4,.contrast=4,.saturation=4,.corner=3,.opacity=4,.size=1,.mark_style=1};}
static inline int photo_effects_needed(const PhotoEffects *p){return p->preset||p->exposure||p->contrast!=4||p->saturation!=4||p->warmth||p->vignette||p->grain||p->bars||(p->watermark&&p->name[0]);}
static inline int photo_effects_step(int value,int low,int high,int direction){value+=direction;return value<low?high:value>high?low:value;}
static inline void photo_effects_change(PhotoEffects *p,int row,int direction){
 switch(row){case FX_PRESET:p->preset=photo_effects_step(p->preset,0,6,direction);break;case FX_STRENGTH:p->strength=photo_effects_step(p->strength,0,4,direction);break;
 case FX_EXPOSURE:p->exposure=photo_effects_step(p->exposure,-4,4,direction);break;case FX_CONTRAST:p->contrast=photo_effects_step(p->contrast,0,8,direction);break;
 case FX_SATURATION:p->saturation=photo_effects_step(p->saturation,0,8,direction);break;case FX_WARMTH:p->warmth=photo_effects_step(p->warmth,-4,4,direction);break;
 case FX_VIGNETTE:p->vignette=photo_effects_step(p->vignette,0,4,direction);break;case FX_GRAIN:p->grain=photo_effects_step(p->grain,0,4,direction);break;
 case FX_BARS:p->bars=photo_effects_step(p->bars,0,2,direction);break;case FX_RESET:{PhotoEffects before=*p;photo_effects_default(p);p->watermark=before.watermark;p->corner=before.corner;p->opacity=before.opacity;p->size=before.size;p->mark_style=before.mark_style;p->mark_font=before.mark_font;p->mark_color=before.mark_color;for(int i=0;i<64;++i)p->name[i]=before.name[i];break;}}
}
static inline void photo_watermark_change(PhotoEffects *p,int row,int direction){
 switch(row){case MARK_ENABLED:if(p->name[0])p->watermark=!p->watermark;break;case MARK_CORNER:p->corner=photo_effects_step(p->corner,0,3,direction);break;case MARK_OPACITY:p->opacity=photo_effects_step(p->opacity,1,4,direction);break;case MARK_SIZE:p->size=photo_effects_step(p->size,0,2,direction);break;case MARK_STYLE:p->mark_style=photo_effects_step(p->mark_style,0,3,direction);break;case MARK_FONT:p->mark_font=photo_effects_step(p->mark_font,0,3,direction);break;case MARK_COLOR:p->mark_color=photo_effects_step(p->mark_color,0,7,direction);break;}
}
static inline const wchar_t *photo_effect_label(int row){static const wchar_t *names[]={L"Filter",L"Filter strength",L"Exposure",L"Contrast",L"Saturation",L"Warmth",L"Vignette",L"Film grain",L"Cinematic bars",L"Reset effects",L"Back"};return names[row>=0&&row<FX_ROWS?row:FX_BACK];}
static inline const wchar_t *photo_mark_label(int row){static const wchar_t *names[]={L"Show watermark",L"Player name",L"Corner",L"Opacity",L"Size",L"Style",L"Font",L"Text color",L"Back"};return names[row>=0&&row<MARK_ROWS?row:MARK_BACK];}
static inline void photo_effect_value(const PhotoEffects *p,int row,wchar_t *v,size_t count){
 static const wchar_t *filters[]={L"None",L"Vivid",L"Cinematic",L"Warm",L"Cool",L"Monochrome",L"Sepia"};
 switch(row){case FX_PRESET:swprintf(v,count,L"%ls",filters[p->preset]);break;case FX_STRENGTH:swprintf(v,count,L"%d%%",p->strength*25);break;case FX_EXPOSURE:swprintf(v,count,L"%+.1f EV",p->exposure*.5);break;case FX_CONTRAST:swprintf(v,count,L"%d%%",100+(p->contrast-4)*15);break;case FX_SATURATION:swprintf(v,count,L"%d%%",p->saturation*25);break;case FX_WARMTH:swprintf(v,count,L"%+d",p->warmth);break;case FX_VIGNETTE:swprintf(v,count,L"%d%%",p->vignette*25);break;case FX_GRAIN:swprintf(v,count,L"%d%%",p->grain*25);break;case FX_BARS:swprintf(v,count,L"%ls",p->bars==1?L"2.35:1":p->bars==2?L"2.76:1":L"Off");break;default:v[0]=0;break;}
}
static inline const wchar_t *photo_mark_value(const PhotoEffects *p,int row,wchar_t *v,size_t count){
 static const wchar_t *corners[]={L"Top left",L"Top right",L"Bottom left",L"Bottom right"},*styles[]={L"Clean",L"Glass",L"Editorial",L"Signature"},*fonts[]={L"Modern",L"Condensed",L"Serif",L"Mono"};
 switch(row){case MARK_ENABLED:return p->watermark&&p->name[0]?L"On":L"Off";case MARK_NAME:return p->name[0]?p->name:L"Not found";case MARK_CORNER:return corners[p->corner];case MARK_OPACITY:swprintf(v,count,L"%d%%",p->opacity*25);return v;case MARK_SIZE:return p->size==0?L"Small":p->size==2?L"Large":L"Medium";case MARK_STYLE:return styles[p->mark_style];case MARK_FONT:return fonts[p->mark_font];case MARK_COLOR:{static const wchar_t *colors[]={L"White",L"Black",L"Silver",L"Gold",L"Coral",L"Sky",L"Mint",L"Violet"};return colors[p->mark_color];}default:return L"";}
}

static inline uint32_t photo_mark_rgb(const PhotoEffects *p){static const uint32_t colors[]={0xf4f6fa,0x141820,0xb8c4d4,0xf5c56b,0xff7d85,0x87c8ff,0x8ae5c1,0xcfb0ff};return colors[p->mark_color>=0&&p->mark_color<8?p->mark_color:0];}

#endif
