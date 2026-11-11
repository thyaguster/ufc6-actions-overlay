#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#undef NDEBUG
#include <assert.h>
#include <stdio.h>
#include "../src/controller_artwork.h"
static uint64_t draw(HDC dc,unsigned char *pixels,int size,uint32_t bit,int ps,unsigned *count){
 RECT r={0,0,256,192};HBRUSH bg=CreateSolidBrush(RGB(17,22,30));FillRect(dc,&r,bg);DeleteObject(bg);
 assert(controller_artwork_draw(dc,0,0,size,size*3/4,bit,ps));GdiFlush();
 uint64_t hash=14695981039346656037ULL;*count=0;
 for(int i=0;i<256*192;++i){unsigned char *p=pixels+i*4;if(p[0]!=30||p[1]!=22||p[2]!=17)++*count;for(int c=0;c<3;++c){hash^=p[c];hash*=1099511628211ULL;}}
 return hash;
}
int main(void){
 BITMAPINFO info={0};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=256;info.bmiHeader.biHeight=-192;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
 HDC dc=CreateCompatibleDC(NULL);unsigned char *pixels=NULL;HBITMAP bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,(void**)&pixels,NULL,0);assert(dc&&bitmap&&pixels);HGDIOBJ old=SelectObject(dc,bitmap);
 uint32_t bits[]={2,4,0x400,0x800,8,0x10,0x40,0x80,0x20,0x1000,0x2000,0x4000,0x8000,0};uint64_t hashes[2][14];
 for(int ps=0;ps<2;++ps)for(int i=0;i<14;++i){unsigned normal=0,large=0;hashes[ps][i]=draw(dc,pixels,32,bits[i],ps,&normal);assert(normal>12);draw(dc,pixels,192,bits[i],ps,&large);assert(large>normal*20&&large<normal*55);assert(GetMapMode(dc)==MM_TEXT&&GetGraphicsMode(dc)==GM_COMPATIBLE);}
 assert(hashes[0][2]!=hashes[1][2]&&hashes[0][3]!=hashes[1][3]&&hashes[0][4]!=hashes[1][4]);
 assert(!controller_artwork_draw(dc,0,0,32,24,0x10000000,0));
 SelectObject(dc,old);DeleteObject(bitmap);DeleteDC(dc);puts("PASS: all 28 authored Xbox/PlayStation assets render at small and large sizes, platform-specific shoulders/Options differ, and GDI state is preserved.");return 0;
}
