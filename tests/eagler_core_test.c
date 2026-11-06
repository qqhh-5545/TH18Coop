#include "../src/eagler_core.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#undef assert
#define assert(x) do {if(!(x)){fprintf(stderr,"FAIL upstream adapter line%d: %s\n",__LINE__,#x);exit(1);}}while(0)
typedef struct{unsigned to,from,at;size_t size;unsigned char data[512];} Packet;
static Packet pending[1024];static unsigned count;
static uint32_t input(unsigned seat,unsigned frame){return ((frame/3+seat)&1?0x80:0x40)|((frame+seat)%17==0?2:1)|((frame+seat)%11==0?0x400:0)|((frame+seat)%23==0?0x80000:0);}
static uint32_t step(uint32_t world,const uint32_t keys[3]){for(unsigned s=0;s<3;++s)world=world*31+keys[s];return world;}
static void delay_test(unsigned players,unsigned delay){
 void *core[3]={0};uint32_t next[3]={0},world[3]={17,17,17};unsigned captures[3]={0},waits=0;count=0;
 for(unsigned s=0;s<players;++s){core[s]=ec_create_delay(players,s,delay,456789);assert(core[s]);}
 for(unsigned wall=0;wall<900;++wall){
  for(unsigned s=0;s<players;++s)if(next[s]<300){
   if(!ec_has_local(core[s],next[s])){assert(ec_local(core[s],next[s],input(s,next[s])));++captures[s];}
   for(unsigned to=0;to<players;++to)if(to!=s){assert(count<1024);Packet *p=&pending[count++];p->from=s;p->to=to;
    p->at=wall+1+(wall+s+to)%5;if(s==players-1 && wall>=110 && wall<140)p->at+=30;
    assert(ec_encode(core[s],to,next[s]+delay,wall+1,p->data,sizeof(p->data),&p->size));}
  }
  for(unsigned i=0;i<count;){Packet p=pending[i];if(p.at>wall){++i;continue;}assert(ec_receive(core[p.to],p.from,p.data,p.size));pending[i]=pending[--count];}
  for(unsigned s=0;s<players;++s)if(next[s]<300){uint32_t keys[3];unsigned predicted=99;
   if(!ec_prepare(core[s],next[s],keys,&predicted)){++waits;continue;}assert(predicted==0);
   for(unsigned id=0;id<3;++id)assert(keys[id]==(id>=players || next[s]<delay?0:input(id,next[s]-delay)));
   world[s]=step(world[s],keys);assert(ec_mark(core[s],next[s]));assert(ec_rollback(core[s])==UINT32_MAX);++next[s];
  }
 }
 uint32_t expected=17;for(unsigned f=0;f<300;++f){uint32_t keys[3]={0};for(unsigned s=0;s<players;++s)keys[s]=f<delay?0:input(s,f-delay);expected=step(expected,keys);}
 assert(waits>0);for(unsigned s=0;s<players;++s){assert(next[s]==300 && captures[s]==300 && world[s]==expected);ec_destroy(core[s]);}
 printf("PASS exact input delay:%u seats,delay%u,300 frames,late/shuffled packets+30-tick interruption,no prediction/replay,once-only sampling\n",players,delay);
}
int main(void){
 void *core[3];uint32_t world[3]={17,17,17},before[3][128]={{0}},next[3]={0};unsigned corrections=0,captures[3]={0};
 for(unsigned s=0;s<3;++s){core[s]=ec_create(3,s,0,8,123456);assert(core[s]);}
 for(unsigned wall=0;wall<126;++wall){
  for(unsigned s=0;s<3;++s)if(wall<120){assert(ec_local(core[s],wall,input(s,wall)));++captures[s];assert(ec_has_local(core[s],wall));
   for(unsigned to=0;to<3;++to)if(to!=s){assert(count<1024);Packet *p=&pending[count++];p->from=s;p->to=to;p->at=wall+(s==0?1:s==1?3:5);assert(ec_encode(core[s],to,wall,wall+1,p->data,sizeof(p->data),&p->size));}}
  for(unsigned i=0;i<count;){Packet p=pending[i];if(p.at>wall){++i;continue;}assert(ec_receive(core[p.to],p.from,p.data,p.size));pending[i]=pending[--count];}
  for(unsigned s=0;s<3;++s){uint32_t rewind=ec_rollback(core[s]);if(rewind!=UINT32_MAX){assert(rewind<next[s]);world[s]=before[s][rewind];assert(ec_rewind(core[s],rewind));next[s]=rewind;++corrections;}
   while(next[s]<120 && next[s]<=wall){uint32_t keys[3];unsigned predicted;if(!ec_prepare(core[s],next[s],keys,&predicted))break;before[s][next[s]]=world[s];world[s]=step(world[s],keys);assert(ec_mark(core[s],next[s]));++next[s];}}
 }
 uint32_t expected=17;for(unsigned f=0;f<120;++f){uint32_t keys[3];for(unsigned s=0;s<3;++s)keys[s]=input(s,f);expected=step(expected,keys);}
 assert(!count && corrections>20);for(unsigned s=0;s<3;++s){assert(next[s]==120 && world[s]==expected && ec_confirmed(core[s])==119 && captures[s]==120);uint32_t keys[3];assert(ec_inputs(core[s],119,keys));for(unsigned p=0;p<3;++p)assert(keys[p]==input(p,119));assert(!ec_local(core[s],120,0x1000));ec_destroy(core[s]);}
 printf("PASS actual upstream RollbackCore/codec via C ABI:3 seats120 frames,%u corrections,1/3/5-frame late shuffled packets,exact world,Enter/card/Bomb edges,once-only sampling\n",corrections);
 assert(!ec_create_delay(3,0,0,1) && !ec_create_delay(3,0,13,1));
 for(unsigned p=2;p<=3;++p)for(unsigned d=2;d<=12;d+=d==2?4:6)delay_test(p,d);
 return 0;
}
