#ifndef UFC6_REPLAY_SEEK_H
#define UFC6_REPLAY_SEEK_H
#include <math.h>
/* End is excluded: the native viewer dispatches its finish event there. */
static int seek_target(double time,double delta,double stream_low,double stream_high,double clip_low,double clip_high,double *out){
 if(!isfinite(time)||!isfinite(delta)||!isfinite(stream_low)||!isfinite(stream_high)||!isfinite(clip_low)||!isfinite(clip_high))return 0;
 double low=fmax(stream_low,clip_low),high=fmin(stream_high,clip_high)-.001;
 if(low<.05||high>14400||high<low||time<low-.001||time>high+.001||fabs(delta)>1)return 0;
 double target=time+delta;if(target<low)target=low;if(target>high)target=high;
 if(fabs(target-time)<1e-9)return 0;*out=target;return 1;
}
#endif
