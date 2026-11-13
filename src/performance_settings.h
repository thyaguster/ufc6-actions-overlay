#ifndef UFC6_PERFORMANCE_SETTINGS_H
#define UFC6_PERFORMANCE_SETTINGS_H
#include <wchar.h>
typedef struct {int crowd_off,particles_off,dof_off,drs_off,render_scale;} PerformanceSettings;
enum { PERF_CROWD,PERF_PARTICLES,PERF_DOF,PERF_DYNAMIC,PERF_SCALE,PERF_SAVE,PERF_RESTART,PERF_BACK,PERF_ROWS };
enum { PERF_OK,PERF_UNAVAILABLE,PERF_CHANGED_EXTERNALLY,PERF_WRITE_FAILED };
static inline int performance_equal(const PerformanceSettings *a,const PerformanceSettings *b){return a->crowd_off==b->crowd_off&&a->particles_off==b->particles_off&&a->dof_off==b->dof_off&&a->drs_off==b->drs_off&&a->render_scale==b->render_scale;}
static inline void performance_adjust(PerformanceSettings *p,int row,int direction){
 static const int scales[]={0,33,40,44,50,59,67,75,77,100,133,200};
 if(row==PERF_CROWD)p->crowd_off=!p->crowd_off;
 else if(row==PERF_PARTICLES)p->particles_off=!p->particles_off;
 else if(row==PERF_DOF)p->dof_off=!p->dof_off;
 else if(row==PERF_DYNAMIC){p->drs_off=!p->drs_off;if(!p->drs_off)p->render_scale=0;}
 else if(row==PERF_SCALE){int index=-1;for(int i=0;i<12;++i)if(p->render_scale==scales[i])index=i;if(index<0)index=0;index=(index+(direction<0?11:1))%12;p->render_scale=scales[index];if(p->render_scale)p->drs_off=1;}
}
int performance_load(const wchar_t *bin,PerformanceSettings *out);
int performance_save(const wchar_t *bin,const PerformanceSettings *expected,const PerformanceSettings *desired);
#endif
