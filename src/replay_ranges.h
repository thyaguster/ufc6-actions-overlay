#ifndef UFC6_REPLAY_RANGES_H
#define UFC6_REPLAY_RANGES_H
#include <math.h>
#include <stdlib.h>
typedef struct {double low,high;} ReplayRange;
static int range_order(const void*a,const void*b){double x=((const ReplayRange*)a)->low,y=((const ReplayRange*)b)->low;return (x>y)-(x<y);}
static int range_merge(ReplayRange*a,int n){if(n<=0)return 0;qsort(a,n,sizeof(*a),range_order);int k=0;for(int i=0;i<n;++i){if(!isfinite(a[i].low)||!isfinite(a[i].high)||a[i].high<=a[i].low)return 0;if(k&&a[i].low<=a[k-1].high+.00001){a[k-1].high=fmax(a[k-1].high,a[i].high);}else a[k++]=a[i];}return k;}
static int range_containing(const ReplayRange*a,int n,double t,ReplayRange*out){for(int i=0;i<n;++i)if(t>=a[i].low&&t<a[i].high){*out=a[i];return 1;}return 0;}
#endif
