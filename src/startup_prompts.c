#include "startup_prompts.h"
#include "startup_prompt_masks.h"
#include <string.h>
static int bit(const unsigned char *p,int x,int y){if(x<0||y<0||x>=PROMPT_GRID_W||y>=PROMPT_GRID_H)return 0;int n=y*PROMPT_GRID_W+x;return (p[n>>3]>>(n&7))&1;}
static int near(const unsigned char *p,int x,int y){for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx)if(bit(p,x+dx,y+dy))return 1;return 0;}
static int light(const unsigned char *p,uint32_t w,uint32_t h,int nx,int ny){
 uint32_t x=((uint64_t)nx*w+w/2)/960,y=((uint64_t)ny*h+h/2)/540;int result=0;
 for(unsigned dy=0;dy<2;++dy)for(unsigned dx=0;dx<2;++dx){uint32_t sx=x+dx,sy=y+dy;if(sx>=w||sy>=h)continue;const unsigned char *q=p+((uint64_t)sy*w+sx)*4;int v=q[0]<q[1]?q[0]:q[1];if(q[2]<v)v=q[2];if(v>result)result=v;}
 return result;
}
int startup_prompt_identify(const unsigned char *p,uint32_t w,uint32_t h){
 if(!p||w<1280||h<720||w>8192||h>8192||((uint64_t)w*9>(uint64_t)h*16+16)||((uint64_t)h*16>(uint64_t)w*9+16))return 0;
 int title=light(p,w,h,280,240)>=200&&light(p,w,h,420,270)>=200&&light(p,w,h,630,285)>=200;
 int warning=1;
 for(int x=240;x<=720;x+=240)if(light(p,w,h,x,135)>16)warning=0;
 if(light(p,w,h,380,445)<190||light(p,w,h,580,445)<190||light(p,w,h,480,520)>24)warning=0;
 if(!title&&!warning)return 0;
 unsigned char sample[7200]={0};int count=0;
 for(int y=0;y<PROMPT_GRID_H;++y)for(int x=0;x<PROMPT_GRID_W;++x)if(light(p,w,h,x+160,y+325)>=160){int n=y*PROMPT_GRID_W+x;sample[n>>3]|=1u<<(n&7);++count;}
 if(count<400)return 0;
 for(int which=0;which<3;++which){
  if((which==2&&!title)||(which<2&&!warning))continue;
  const unsigned char *reference=startup_prompt_masks[which];int total=0,found=0,extras=0;
  for(int y=0;y<PROMPT_GRID_H;++y)for(int x=0;x<PROMPT_GRID_W;++x){if(bit(reference,x,y)){++total;found+=near(sample,x,y);}if(bit(sample,x,y)&&!near(reference,x,y))++extras;}
  if(count*100>=total*65&&count*100<=total*155&&found*100>=total*94&&extras*100<=count*6)return which+1;
 }
 return 0;
}

int startup_prompt_can_confirm(int id,const StartupPromptContext *c){
 return c&&id>=1&&id<=3&&c->enabled&&c->focused&&!c->visible&&!c->held&&!c->pulsing&&(c->controller||id==3)&&c->now<c->deadline&&!(c->seen&(1u<<(id-1)))&&(id!=3||!(c->seen&3));
}
