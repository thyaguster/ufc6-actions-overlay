#ifndef UFC6_PHOTO_PRESETS_H
#define UFC6_PHOTO_PRESETS_H
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include "photo_effects.h"

#define PHOTO_PRESET_COUNT 3
typedef struct {PhotoEffects effects;int fov,hud,cage,filled;} PhotoPreset;

static inline void photo_preset_values(const PhotoPreset *p,int v[19]){
 const PhotoEffects *e=&p->effects;
 int values[19]={e->preset,e->strength,e->exposure,e->contrast,e->saturation,e->warmth,e->vignette,e->grain,e->bars,e->watermark,e->corner,e->opacity,e->size,e->mark_style,e->mark_font,e->mark_color,p->fov,p->hud,p->cage};
 for(int i=0;i<19;++i)v[i]=values[i];
}
static inline int photo_preset_from_values(PhotoPreset *p,const int v[19]){
 static const int low[19]={0,0,-4,0,0,-4,0,0,0,0,0,1,0,0,0,0,20,0,0};
 static const int high[19]={6,4,4,8,8,4,4,4,2,1,3,4,2,3,3,7,120,1,1};
 for(int i=0;i<19;++i)if(v[i]<low[i]||v[i]>high[i])return 0;
 *p=(PhotoPreset){0};PhotoEffects *e=&p->effects;
 e->preset=v[0];e->strength=v[1];e->exposure=v[2];e->contrast=v[3];e->saturation=v[4];e->warmth=v[5];e->vignette=v[6];e->grain=v[7];e->bars=v[8];e->watermark=v[9];e->corner=v[10];e->opacity=v[11];e->size=v[12];e->mark_style=v[13];e->mark_font=v[14];e->mark_color=v[15];p->fov=v[16];p->hud=v[17];p->cage=v[18];p->filled=1;return 1;
}
static inline void photo_preset_capture(PhotoPreset *p,const PhotoEffects *effects,int fov,int hud,int cage){
 *p=(PhotoPreset){.effects=*effects,.fov=fov,.hud=!!hud,.cage=!!cage,.filled=1};
 for(int i=0;i<64;++i)p->effects.name[i]=0;
 if(p->fov<20)p->fov=20;if(p->fov>120)p->fov=120;
}
static inline int photo_preset_apply(const PhotoPreset *p,PhotoEffects *effects,int *fov,int *hud,int *cage){
 if(!p->filled)return 0;
 wchar_t name[64];for(int i=0;i<64;++i)name[i]=effects->name[i];
 *effects=p->effects;for(int i=0;i<64;++i)effects->name[i]=name[i];
 *fov=p->fov;*hud=p->hud;*cage=p->cage;return 1;
}
static inline void photo_presets_load(PhotoPreset presets[PHOTO_PRESET_COUNT],const wchar_t *path){
 for(int slot=0;slot<PHOTO_PRESET_COUNT;++slot){
  presets[slot]=(PhotoPreset){0};wchar_t key[16],line[256];swprintf(key,16,L"Slot%d",slot+1);
  if(!GetPrivateProfileStringW(L"PhotoPresets",key,L"",line,256,path))continue;
  int values[19];wchar_t *at=line,*end;int valid=1;
  for(int i=0;i<19;++i){long value=wcstol(at,&end,10);if(end==at||value<-1000||value>1000||(i<18?*end!=L',':*end!=0)){valid=0;break;}values[i]=(int)value;at=end+(*end==L',');}
  if(valid)photo_preset_from_values(&presets[slot],values);
 }
}
static inline int photo_preset_save(const PhotoPreset *p,int slot,const wchar_t *path){
 if(slot<0||slot>=PHOTO_PRESET_COUNT||!p->filled)return 0;
 int values[19];photo_preset_values(p,values);PhotoPreset check;if(!photo_preset_from_values(&check,values))return 0;
 wchar_t key[16],line[256];swprintf(key,16,L"Slot%d",slot+1);size_t used=0;
 for(int i=0;i<19;++i){int n=swprintf(line+used,256-used,i?L",%d":L"%d",values[i]);if(n<0||(size_t)n>=256-used)return 0;used+=(size_t)n;}
 return WritePrivateProfileStringW(L"PhotoPresets",key,line,path)!=0;
}
#endif
