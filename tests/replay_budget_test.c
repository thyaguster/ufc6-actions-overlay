#undef NDEBUG
#include <assert.h>
#include <stdio.h>
#include "../src/replay_budget_guard.h"
int main(void){volatile LONG field=50;BudgetSwap s={0};assert(budget_acquire(&s,&field,50)&&field==256);assert(!budget_acquire(&s,&field,50));assert(budget_restore(&s)&&field==50);assert(!budget_restore(&s));field=40;assert(budget_acquire(&s,&field,40)&&budget_restore(&s)&&field==40);field=128;assert(!budget_acquire(&s,&field,128)&&field==128);field=50;assert(budget_acquire(&s,&field,50));field=128;assert(!budget_restore(&s)&&field==128);field=40;assert(!budget_acquire(&s,&field,50)&&field==40);puts("Budget compare/exchange, exact restoration and foreign-value preservation passed");return 0;}
