#undef NDEBUG
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/startup_prompts.h"
int main(int argc,char **argv){
 StartupPromptContext context={.enabled=1,.focused=1,.controller=1,.now=100,.deadline=200};
 assert(startup_prompt_can_confirm(1,&context)&&startup_prompt_can_confirm(2,&context)&&startup_prompt_can_confirm(3,&context));
 assert(!startup_prompt_can_confirm(0,&context)&&!startup_prompt_can_confirm(4,&context));
 context.seen=1;assert(!startup_prompt_can_confirm(1,&context)&&startup_prompt_can_confirm(2,&context));context.seen=3;assert(!startup_prompt_can_confirm(2,&context)&&!startup_prompt_can_confirm(3,&context));context.seen=4;assert(!startup_prompt_can_confirm(3,&context)&&startup_prompt_can_confirm(1,&context));context.seen=0;
 context.enabled=0;assert(!startup_prompt_can_confirm(1,&context));context.enabled=1;
 context.focused=0;assert(!startup_prompt_can_confirm(1,&context));context.focused=1;
 context.visible=1;assert(!startup_prompt_can_confirm(1,&context));context.visible=0;
 context.held=1;assert(!startup_prompt_can_confirm(1,&context));context.held=0;
 context.pulsing=1;assert(!startup_prompt_can_confirm(1,&context));context.pulsing=0;
 context.controller=0;assert(!startup_prompt_can_confirm(1,&context)&&startup_prompt_can_confirm(3,&context));context.controller=1;
 context.now=200;assert(!startup_prompt_can_confirm(1,&context));
 assert(startup_prompt_identify(NULL,960,540)==0);
 unsigned char *pixels=calloc(960*540,4);assert(pixels);assert(startup_prompt_identify(pixels,960,540)==0);
 memset(pixels,255,960*540*4);assert(startup_prompt_identify(pixels,960,540)==0);assert(startup_prompt_identify(pixels,960,100)==0);free(pixels);
 if(argc==5){unsigned w=(unsigned)atoi(argv[2]),h=(unsigned)atoi(argv[3]);size_t n=(size_t)w*h*4;assert(n<300000000);pixels=malloc(n);assert(pixels);FILE *file=fopen(argv[1],"rb");assert(file);assert(fread(pixels,1,n,file)==n);fclose(file);int actual=startup_prompt_identify(pixels,w,h);printf("Detected %d (expected %s)\n",actual,argv[4]);assert(actual==atoi(argv[4]));free(pixels);}
 puts("PASS: startup text classifier rejects absent/blank/white/unsupported frames.");return 0;
}
