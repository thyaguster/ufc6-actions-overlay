#ifndef UFC6_CONTROLLER_ARTWORK_H
#define UFC6_CONTROLLER_ARTWORK_H
/* Exact authored vector geometry; embedded so installation needs no loose image files. */
typedef struct {unsigned char op;int xy[6];} ControllerArtCommand;
typedef struct {const ControllerArtCommand *commands;unsigned count;unsigned rgb;} ControllerArtPath;
typedef struct {const ControllerArtPath *paths;unsigned count;int left,top,right,bottom;} ControllerArtAsset;
#include "controller_artwork_data.h"
static int controller_artwork_draw(HDC dc,int x,int y,int width,int height,uint32_t bit,int ps){
    static const uint32_t bits[]={2,4,0x400,0x800,8,0x10,0x40,0x80,0x20,0x1000,0x2000,0x4000,0x8000,0};
    unsigned index=0;for(;index<14&&bits[index]!=bit;++index){}if(index==14)return 0;
    const ControllerArtAsset *asset=controller_art_assets[ps!=0][index];
    FLOAT sx=(FLOAT)(width-2)/(asset->right-asset->left),sy=(FLOAT)(height-2)/(asset->bottom-asset->top),scale=sx<sy?sx:sy;
    XFORM xf={scale,0,0,scale,x+(width-(asset->right-asset->left)*scale)/2-asset->left*scale,y+(height-(asset->bottom-asset->top)*scale)/2-asset->top*scale};
    int saved=SaveDC(dc);if(!saved)return 0;
    if(!SetGraphicsMode(dc,GM_ADVANCED)||!SetWorldTransform(dc,&xf)){RestoreDC(dc,saved);return 0;}
    SetPolyFillMode(dc,WINDING);int ok=1;
    for(unsigned p=0;p<asset->count&&ok;++p){const ControllerArtPath *path=&asset->paths[p];ok=BeginPath(dc)!=0;
        for(unsigned i=0;i<path->count&&ok;++i){const ControllerArtCommand *c=&path->commands[i];
            if(c->op==0)ok=MoveToEx(dc,c->xy[0],c->xy[1],NULL)!=0;
            else if(c->op==1)ok=LineTo(dc,c->xy[0],c->xy[1])!=0;
            else if(c->op==2){POINT points[]={{c->xy[0],c->xy[1]},{c->xy[2],c->xy[3]},{c->xy[4],c->xy[5]}};ok=PolyBezierTo(dc,points,3)!=0;}
            else ok=CloseFigure(dc)!=0;
        }
        if(!ok){AbortPath(dc);break;}ok=EndPath(dc)!=0;
        COLORREF color=RGB((path->rgb>>16)&255,(path->rgb>>8)&255,path->rgb&255);
        if(path->rgb==0xffffffu){
            color=RGB(232,238,247);
            if(bit==0x4000)color=ps?RGB(134,192,255):RGB(139,218,100);
            else if(bit==0x2000)color=ps?RGB(244,135,148):RGB(242,105,103);
            else if(bit==0x8000)color=ps?RGB(225,155,227):RGB(111,180,255);
            else if(bit==0x1000)color=ps?RGB(137,221,180):RGB(242,214,104);
        }
        HBRUSH brush=CreateSolidBrush(color);if(!brush){AbortPath(dc);ok=0;break;}HGDIOBJ previous=SelectObject(dc,brush);if(ok)ok=FillPath(dc)!=0;SelectObject(dc,previous);DeleteObject(brush);
    }
    RestoreDC(dc,saved);return ok;
}
#endif
