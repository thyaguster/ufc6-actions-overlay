#define WIN32_LEAN_AND_MEAN
#include <windows.h>
typedef struct {volatile LONG *field;LONG previous;int held;} BudgetSwap;
static int budget_acquire(BudgetSwap*s,volatile LONG*p,LONG expected){if(s->held||(expected!=40&&expected!=50))return 0;if(InterlockedCompareExchange(p,256,expected)!=expected)return 0;s->field=p;s->previous=expected;s->held=1;return 1;}
static int budget_restore(BudgetSwap*s){if(!s->held)return 0;int ok=InterlockedCompareExchange(s->field,s->previous,256)==256;s->held=0;return ok;}
