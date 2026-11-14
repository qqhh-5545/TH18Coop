#ifndef TH18_ROLLBACK_JOURNAL_H
#define TH18_ROLLBACK_JOURNAL_H
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
/* Bounded undo storage, independent of D3D/threads/network. The title must
   register all world owners and retain their allocations before using it. */
typedef struct {void *address;size_t size,offset;uint32_t left,right,height;} RbBlock;
typedef struct {uint64_t frame;size_t used,count;int sealed;unsigned char *bytes;RbBlock *blocks;uint32_t root;} RbFrame;
typedef struct {RbFrame *frames;size_t capacity,byte_limit,block_limit,start,count;int open,failed;} RbJournal;
static void rb_destroy(RbJournal *j){if(j->frames){for(size_t i=0;i<j->capacity;++i){free(j->frames[i].bytes);free(j->frames[i].blocks);}free(j->frames);}memset(j,0,sizeof(*j));}
static int rb_init(RbJournal *j,size_t frames,size_t bytes,size_t blocks){
    memset(j,0,sizeof(*j));if(!frames || frames>256 || !bytes || bytes>64u*1024u*1024u || !blocks || blocks>65536)return 0;
    j->capacity=frames;j->byte_limit=bytes;j->block_limit=blocks;j->frames=calloc(frames,sizeof(RbFrame));if(!j->frames)return 0;
    for(size_t i=0;i<frames;++i){j->frames[i].bytes=malloc(bytes);j->frames[i].blocks=calloc(blocks,sizeof(RbBlock));if(!j->frames[i].bytes || !j->frames[i].blocks){rb_destroy(j);return 0;}}
    return 1;
}
static RbFrame *rb_last(RbJournal *j){return j->count?&j->frames[(j->start+j->count-1)%j->capacity]:NULL;}
static int rb_begin(RbJournal *j,uint64_t frame){
    RbFrame *last=rb_last(j);if(!j->frames || j->open || j->failed || j->count==j->capacity || (last && frame!=last->frame+1))return 0;
    RbFrame *f=&j->frames[(j->start+j->count)%j->capacity];f->frame=frame;f->count=f->used=0;f->sealed=0;f->root=UINT32_MAX;++j->count;j->open=1;return 1;
}
static unsigned rb_index_height(RbFrame *f,uint32_t n){return n==UINT32_MAX?0:f->blocks[n].height;}
static void rb_index_update(RbFrame *f,uint32_t n){unsigned a=rb_index_height(f,f->blocks[n].left),b=rb_index_height(f,f->blocks[n].right);f->blocks[n].height=1+(a>b?a:b);}
static uint32_t rb_index_left(RbFrame *f,uint32_t n){uint32_t r=f->blocks[n].right;f->blocks[n].right=f->blocks[r].left;f->blocks[r].left=n;rb_index_update(f,n);rb_index_update(f,r);return r;}
static uint32_t rb_index_right(RbFrame *f,uint32_t n){uint32_t l=f->blocks[n].left;f->blocks[n].left=f->blocks[l].right;f->blocks[l].right=n;rb_index_update(f,n);rb_index_update(f,l);return l;}
static uint32_t rb_index_insert(RbFrame *f,uint32_t root,uint32_t n){
    if(root==UINT32_MAX)return n;RbBlock *b=&f->blocks[root];
    if((uintptr_t)f->blocks[n].address<(uintptr_t)b->address)b->left=rb_index_insert(f,b->left,n);else b->right=rb_index_insert(f,b->right,n);
    rb_index_update(f,root);int balance=(int)rb_index_height(f,b->left)-(int)rb_index_height(f,b->right);
    if(balance>1){if((uintptr_t)f->blocks[n].address>(uintptr_t)f->blocks[b->left].address)b->left=rb_index_left(f,b->left);return rb_index_right(f,root);}
    if(balance< -1){if((uintptr_t)f->blocks[n].address<(uintptr_t)f->blocks[b->right].address)b->right=rb_index_right(f,b->right);return rb_index_left(f,root);}return root;
}
/* saved must be an immutable copy taken before this frame changed address.
   This supports dirty-page discovery after a pre-step read-only capture. */
static int rb_store_before(RbJournal *j,void *address,const void *saved,size_t size){
    if(!j->open || j->failed)return 0;RbFrame *f=rb_last(j);uintptr_t a=(uintptr_t)address;
    if(!address || !saved || !size || a>UINTPTR_MAX-size){j->failed=1;return 0;}
    /* AVL address index bounds overlap/duplicate checks to logarithmic work.
       Blocks and undo bytes retain their original insertion/reverse order. */
    for(uint32_t i=f->root;i!=UINT32_MAX;){RbBlock *b=&f->blocks[i];uintptr_t v=(uintptr_t)b->address;if(v==a && b->size==size)return 1;if(a<v+b->size && v<a+size){j->failed=1;return 0;}i=a<v?b->left:b->right;}
    if(f->count==j->block_limit || size>j->byte_limit-f->used){j->failed=1;return 0;}
    uint32_t index=(uint32_t)f->count++;f->blocks[index]=(RbBlock){address,size,f->used,UINT32_MAX,UINT32_MAX,1};f->root=rb_index_insert(f,f->root,index);
    memcpy(f->bytes+f->used,saved,size);f->used+=size;return 1;
}
static int rb_touch(RbJournal *j,void *address,size_t size){return rb_store_before(j,address,address,size);}
static int rb_end(RbJournal *j){if(!j->open || j->failed)return 0;rb_last(j)->sealed=1;j->open=0;return 1;}
static int rb_undo_to(RbJournal *j,uint64_t target){
    if(j->open || j->failed)return 0;int found=0;for(size_t i=0;i<j->count;++i)if(j->frames[(j->start+i)%j->capacity].frame==target)found=1;
    if(!found)return 0;while(j->count){RbFrame *f=rb_last(j);if(f->frame<target)break;for(size_t i=f->count;i;--i){RbBlock *b=&f->blocks[i-1];memcpy(b->address,f->bytes+b->offset,b->size);}--j->count;}return 1;
}
static int rb_discard_before(RbJournal *j,uint64_t confirmed_next){
    if(j->open || j->failed)return 0;while(j->count && j->frames[j->start].frame<confirmed_next){j->start=(j->start+1)%j->capacity;--j->count;}return 1;
}
#endif
