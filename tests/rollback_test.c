#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../src/rollback_journal.h"
#include <stdio.h>
#include <assert.h>
typedef struct{int x[3],life[3],funds[3],boss,bullets,rng;} World;
static void tick(World *w,int frame,int corrected){for(int i=0;i<3;++i){w->x[i]+=i+frame;w->funds[i]+=(i+frame)%7;w->life[i]-=(frame+i)%13==0;}w->rng=(int)((uint32_t)w->rng*1664525u+1013904223u);w->boss-=1+corrected;w->bullets+=corrected?7:3;}
static unsigned check_index(RbFrame *f,uint32_t n,uintptr_t low,uintptr_t high,unsigned *count){
 if(n==UINT32_MAX)return 0;assert(n<f->count);RbBlock *b=&f->blocks[n];uintptr_t at=(uintptr_t)b->address;assert(at>=low && at+b->size<=high);++*count;
 unsigned l=check_index(f,b->left,low,at,count),r=check_index(f,b->right,at+b->size,high,count);assert(l<=r+1 && r<=l+1 && b->height==1+(l>r?l:r));return b->height;
}
static void large_index(void){
 uint32_t *grid=calloc(8192,sizeof(uint32_t));assert(grid);
 for(unsigned mode=0;mode<3;++mode){RbJournal j;assert(rb_init(&j,2,65536,4096) && rb_begin(&j,0));
  for(unsigned i=0;i<4096;++i)grid[2*i]=i+7;
  for(unsigned i=0;i<4096;++i){unsigned slot=mode==0?i:mode==1?4095-i:(i*4051)%4096;
   assert(rb_touch(&j,grid+2*slot,4));grid[2*slot]=99;assert(rb_touch(&j,grid+2*slot,4));}
  RbFrame *f=rb_last(&j);unsigned count=0;assert(check_index(f,f->root,0,UINTPTR_MAX,&count)<=18 && count==4096 && f->used==16384);
  assert(rb_end(&j) && rb_undo_to(&j,0));for(unsigned i=0;i<4096;++i)assert(grid[2*i]==i+7);
  assert(rb_begin(&j,1) && rb_touch(&j,grid,4));assert(!rb_touch(&j,(unsigned char *)grid+2,4) && j.failed);rb_destroy(&j);
 }free(grid);puts("PASS indexed journal:4096 ascending/descending/shuffled regions,AVL invariants,duplicate first-write preservation,exact undo and overlap rejection");
}
int main(void){
 large_index();
 RbJournal j;assert(rb_init(&j,16,4096,16));World w={.life={2,3,1},.funds={40,8,84},.boss=500,.rng=1234},initial=w;
 for(int f=0;f<10;++f){assert(rb_begin(&j,f));assert(rb_touch(&j,&w,sizeof(w)));World before=w;tick(&w,f,0);assert(rb_touch(&j,&w,sizeof(w)));assert(!memcmp(j.frames[f].bytes,&before,sizeof(w)));assert(rb_end(&j));}
 assert(rb_undo_to(&j,4));World exact=initial;for(int f=0;f<4;++f)tick(&exact,f,0);assert(!memcmp(&w,&exact,sizeof(w)));
 for(int f=4;f<10;++f){assert(rb_begin(&j,f));assert(rb_touch(&j,&w,sizeof(w)));tick(&w,f,1);assert(rb_end(&j));tick(&exact,f,1);}
 assert(!memcmp(&w,&exact,sizeof(w)));assert(!rb_undo_to(&j,99));assert(rb_discard_before(&j,10));assert(!rb_undo_to(&j,0));
 // Multiple full ring reuses with caller-confirmed retirement.
 for(int f=10;f<100;++f){assert(rb_begin(&j,f));assert(rb_touch(&j,&w,sizeof(w)));tick(&w,f,1);assert(rb_end(&j));assert(rb_discard_before(&j,f+1));}rb_destroy(&j);
 assert(rb_init(&j,2,16,2));int a[8]={0};assert(rb_begin(&j,0));assert(rb_touch(&j,a,8));assert(!rb_touch(&j,a+1,8));assert(!rb_end(&j) && !rb_undo_to(&j,0));rb_destroy(&j);
 assert(rb_init(&j,2,4,1));assert(rb_begin(&j,0));assert(!rb_touch(&j,a,8));assert(j.failed);rb_destroy(&j);
 assert(rb_init(&j,2,16,2));int old=7,current=9;assert(rb_begin(&j,0));assert(rb_store_before(&j,&current,&old,sizeof(old)));assert(rb_end(&j));assert(rb_undo_to(&j,0) && current==7);rb_destroy(&j);
 puts("PASS bounded native undo journal: first-write wins, reverse restore, corrected replay, saved preimage import, ring reuse, unknown target and overlap/budget fail-closed");return 0;
}
