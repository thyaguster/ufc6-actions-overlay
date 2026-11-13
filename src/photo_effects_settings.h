#ifndef UFC6_PHOTO_EFFECTS_SETTINGS_H
#define UFC6_PHOTO_EFFECTS_SETTINGS_H
#include "photo_effects.h"
static int photo_fx_read(const wchar_t *path,const wchar_t *key,int fallback,int low,int high){int v=(int)GetPrivateProfileIntW(L"Photo",key,fallback,path);return v>=low&&v<=high?v:fallback;}
static void photo_effects_load(PhotoEffects *p,const wchar_t *path){
 photo_effects_default(p);
 #define READ(field,key,lo,hi) p->field=photo_fx_read(path,L##key,p->field,lo,hi)
 READ(preset,"Filter",0,6);READ(strength,"FilterStrength",0,4);READ(exposure,"Exposure",-4,4);READ(contrast,"Contrast",0,8);READ(saturation,"Saturation",0,8);READ(warmth,"Warmth",-4,4);READ(vignette,"Vignette",0,4);READ(grain,"FilmGrain",0,4);READ(bars,"CinematicBars",0,2);READ(watermark,"Watermark",0,1);READ(corner,"WatermarkCorner",0,3);READ(opacity,"WatermarkOpacity",1,4);READ(size,"WatermarkSize",0,2);READ(mark_style,"WatermarkStyle",0,3);READ(mark_font,"WatermarkFont",0,3);READ(mark_color,"WatermarkColor",0,7);
 #undef READ
 /* Read only the launcher's chosen display name. Never use a Windows login. */
 GetPrivateProfileStringW(L"Photo",L"WatermarkText",L"",p->name,64,path);
 if(!p->name[0]){wchar_t launcher[MAX_PATH];lstrcpynW(launcher,path,MAX_PATH);wchar_t *slash=wcsrchr(launcher,L'\\');if(slash){*slash=0;slash=wcsrchr(launcher,L'\\');if(slash){lstrcpyW(slash+1,L"UFC6.ini");GetPrivateProfileStringW(L"UFC6",L"user",L"",p->name,64,launcher);}}}
 for(int i=0;p->name[i];++i)if(p->name[i]<32)p->name[i]=L' ';
}
static int photo_effects_save(const PhotoEffects *p,const wchar_t *path){int ok=1;wchar_t value[16];
 #define SAVE(field,key) swprintf(value,16,L"%d",p->field);ok=WritePrivateProfileStringW(L"Photo",L##key,value,path)&&ok
 SAVE(preset,"Filter");SAVE(strength,"FilterStrength");SAVE(exposure,"Exposure");SAVE(contrast,"Contrast");SAVE(saturation,"Saturation");SAVE(warmth,"Warmth");SAVE(vignette,"Vignette");SAVE(grain,"FilmGrain");SAVE(bars,"CinematicBars");SAVE(watermark,"Watermark");SAVE(corner,"WatermarkCorner");SAVE(opacity,"WatermarkOpacity");SAVE(size,"WatermarkSize");SAVE(mark_style,"WatermarkStyle");SAVE(mark_font,"WatermarkFont");SAVE(mark_color,"WatermarkColor");
 #undef SAVE
 return ok;
}
#endif
