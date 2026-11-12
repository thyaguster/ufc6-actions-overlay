#ifndef PRIVATE_PHOTO_MATH_H
#define PRIVATE_PHOTO_MATH_H
#include <math.h>
#include <string.h>
static void photo_rotate(float *v,const float *axis,float angle){
 float cs=cosf(angle),sn=sinf(angle),dot=v[0]*axis[0]+v[1]*axis[1]+v[2]*axis[2];
 float out[3]={v[0]*cs+(axis[1]*v[2]-axis[2]*v[1])*sn+axis[0]*dot*(1-cs),v[1]*cs+(axis[2]*v[0]-axis[0]*v[2])*sn+axis[1]*dot*(1-cs),v[2]*cs+(axis[0]*v[1]-axis[1]*v[0])*sn+axis[2]*dot*(1-cs)};
 memcpy(v,out,sizeof(out));
}
static void photo_step(float *pose,float *fov,float seconds,float right,float up,float forward,float yaw,float pitch,float zoom,int fast){
 float dt=fminf(fmaxf(seconds,0),.05f),speed=fast?6.f:1.5f;
 float world_up[3]={0,1,0};
 for(int axis=0;axis<3;++axis)photo_rotate(pose+axis*4,world_up,yaw*dt);
 float local_right[3]={pose[0],pose[1],pose[2]};
 for(int axis=0;axis<3;++axis)photo_rotate(pose+axis*4,local_right,pitch*dt);
 float norm=sqrtf(right*right+up*up+forward*forward);if(norm>1){right/=norm;up/=norm;forward/=norm;}
 for(int j=0;j<3;++j)pose[12+j]+=speed*dt*(right*pose[j]+up*pose[4+j]-forward*pose[8+j]);
 *fov=fminf(110,fmaxf(25,*fov+zoom*dt*25));
}
#endif
