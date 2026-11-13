#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#undef NDEBUG
#include <assert.h>
#include "../src/live_cheats.c"
static void __attribute__((sysv_abi)) sync_noop(void *b,unsigned f,unsigned n){(void)b;(void)f;(void)n;}
static void ptr(void *base,int off,void *p){uintptr_t v=(uintptr_t)p;memcpy((char*)base+off,&v,8);}
static void num(void *base,int off,int v){memcpy((char*)base+off,&v,4);}
int main(void){
 unsigned char binder[160]={0},rec[416]={0},obj[4][32]={{0}},nodes[4][32]={{0}};char labels[4][64]={{0}};int value[3][2]={{0},{0},{0,1}};float health[2]={100,100};
 ptr(binder,0x88,rec);ptr(binder,0x90,rec+sizeof(rec));
 for(int j=0;j<4;++j){strcpy(labels[j],ko_names[j]);ptr(obj[j],8,labels[j]);ptr(rec+j*104,16,obj[j]);num(rec+j*104,32,-1);num(rec+j*104,40,4);num(rec+j*104,52,j==3?10:6);ptr(rec+j*104,56,nodes[j]);ptr(rec+j*104,64,nodes[j]+32);
 for(int p=0;p<2;++p){ptr(nodes[j],p*16,j==3?(void*)&health[p]:(void*)&value[j][p]);num(nodes[j],p*16+8,p);}}
 uint64_t t=100;ko_observe((uintptr_t)binder,2,t);assert(ko.candidate.valid&&ko.candidate.player==1);
 ko.requested=1;ko.deadline=1000;ko_observe((uintptr_t)binder,2,t+=20);assert(ko.armed&&value[0][0]==7&&value[0][1]==0);
 value[1][0]=7;ko_observe((uintptr_t)binder,2,t+=20);assert(!ko.armed&&ko.result==2&&value[0][0]==0);value[1][0]=0;
 ko.requested=1;ko.deadline=t+30;ko_observe((uintptr_t)binder,2,t+=20);assert(ko.armed);ko_observe((uintptr_t)binder,2,t+=20);assert(!ko.armed&&value[0][0]==0);
 ko.requested=1;ko.deadline=t+1000;ko_observe((uintptr_t)binder,2,t+=20);value[0][0]=4;ko_release(3);assert(value[0][0]==4);value[0][0]=0;
 ko.requested=1;ko.deadline=t+1000;ko_observe((uintptr_t)binder,2,t+=20);value[2][0]=1;value[2][1]=0;ko_observe((uintptr_t)binder,1,t+=20);assert(!ko.armed&&value[0][0]==0);
 ko.requested=1;ko.deadline=t+1000;ko_observe((uintptr_t)binder,1,t+=20);assert(ko.armed&&value[0][1]==7&&value[0][0]==0);ko_release(3);assert(value[0][1]==0);
 value[2][0]=value[2][1]=1;ko.requested=1;ko.deadline=t+1000;ko_observe((uintptr_t)binder,1,t+=20);assert(!ko.armed&&!value[0][0]&&!value[0][1]);value[2][1]=0;
 health[1]=0;ko.requested=1;ko.deadline=t+1000;ko_observe((uintptr_t)binder,1,t+=20);assert(!ko.armed);health[1]=100;
 value[0][1]=4;ko.requested=1;ko.deadline=t+1000;ko_observe((uintptr_t)binder,1,t+=20);assert(!ko.armed&&value[0][1]==4);value[0][1]=0;
 /* Changed identity must never clear a recycled object's value. */
 ko.requested=1;ko.deadline=t+1000;ko_observe((uintptr_t)binder,1,t+=20);assert(ko.armed);labels[0][0]='X';ko_release(3);assert(value[0][1]==7);labels[0][0]='F';value[0][1]=0;
 ko.last_probe=0;ko.requested=1;ko.deadline=GetTickCount64()+25000;ko_observe((uintptr_t)binder,1,GetTickCount64());assert(ko.armed&&value[0][1]==7);
 assert(live_ko_poll(0)==3&&ko.cancel&&value[0][1]==7); /* UI publishes; never writes the fighter. */
 original_binding_sync=sync_noop;hooked_binding_sync(binder,0,4);assert(!ko.armed&&!ko.cancel&&value[0][1]==0);
 puts("PASS: opponent mapping both ways, KO consumption, timeout/cancel, ambiguous/dead fighters rejected, foreign values and changed identities preserved.");return 0;
}
