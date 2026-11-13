#ifndef UFC6_REPLAY_POSTFIGHT_POLICY_H
#define UFC6_REPLAY_POSTFIGHT_POLICY_H
static int post_contains(const int *ids,int n,int value){for(int i=0;i<n;++i)if(ids[i]==value&&value>0)return 1;return 0;}
static int post_plan(const int handles[5],int rounds,int result,const int *ids,int n,int target[5],int *count){
 if((rounds!=3&&rounds!=5)||n<1||n>48||!((result>=3&&result<=7)||result==9||result==10))return 0;
 int seeds[5],used=0;
 for(int i=0;i<rounds;++i)if(post_contains(ids,n,handles[i])&&!post_contains(seeds,used,handles[i]))seeds[used++]=handles[i];
 if(!used)for(int i=0;i<n&&used<5;++i)if(ids[i]>0&&!post_contains(seeds,used,ids[i]))seeds[used++]=ids[i];
 if(!used)return 0;
 for(int i=0;i<5;++i)target[i]=-1;
 if(result==9||result==10){for(int i=0;i<used;++i)target[i]=seeds[i];*count=used;}
 else {int next=0;for(int i=0;i<rounds;++i)target[i]=post_contains(ids,n,handles[i])?handles[i]:seeds[(next++)%used];*count=rounds;}
 return 1;
}
#endif
