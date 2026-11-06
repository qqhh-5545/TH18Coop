#ifndef TH18_NATIVE_WRITE_WATCH_H
#define TH18_NATIVE_WRITE_WATCH_H
#include <windows.h>
#include "rollback_journal.h"
/* Isolated-test pool storage. Only exact, previously identified fixed native
   pool sizes may use this allocator. Never pass these pointers to Heap APIs.
   The matching EXE imports must intercept size/free/resize as well as alloc.
   API contract: https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-getwritewatch */
typedef struct {
    unsigned char *base,*shadow;SIZE_T size,pages;PVOID *dirty;
    int seeded;SIZE_T copied,scanned;
} RbWatchRegion;
static RbWatchRegion rb_watch_regions[16];
static int rb_watch_enabled;
static RbWatchRegion *rb_watch_find(const void *base) {
    for(unsigned i=0;i<16;++i)if(rb_watch_regions[i].base==base && base)return &rb_watch_regions[i];return NULL;
}
static void rb_watch_destroy(RbWatchRegion *r) {
    if(r->base)VirtualFree(r->base,0,MEM_RELEASE);
    if(r->shadow)VirtualFree(r->shadow,0,MEM_RELEASE);
    if(r->dirty)VirtualFree(r->dirty,0,MEM_RELEASE);memset(r,0,sizeof(*r));
}
static void *rb_watch_allocate(SIZE_T size) {
    SYSTEM_INFO system;GetSystemInfo(&system);if(system.dwPageSize!=4096 || !size || size>64u*1024u*1024u)return NULL;
    RbWatchRegion *r=NULL;for(unsigned i=0;i<16;++i)if(!rb_watch_regions[i].base){r=&rb_watch_regions[i];break;}if(!r)return NULL;
    r->size=size;r->pages=(size+4095)/4096;
    r->base=VirtualAlloc(NULL,size,MEM_RESERVE|MEM_COMMIT|MEM_WRITE_WATCH,PAGE_READWRITE);
    r->shadow=VirtualAlloc(NULL,size,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
    r->dirty=VirtualAlloc(NULL,r->pages*sizeof(PVOID),MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
    if(!r->base || !r->shadow || !r->dirty){rb_watch_destroy(r);return NULL;}return r->base;
}
static int rb_watch_query(RbWatchRegion *r,DWORD flags,ULONG_PTR *count) {
    DWORD granularity=0;*count=r->pages;
    if(GetWriteWatch(flags,r->base,r->size,r->dirty,count,&granularity) || granularity!=4096 || *count>r->pages)return 0;
    for(ULONG_PTR i=0;i<*count;++i){uintptr_t offset=(uintptr_t)r->dirty[i]-(uintptr_t)r->base;if(offset>=r->size || (offset&4095))return 0;}return 1;
}
/* Single owning game thread, scene worker stopped. Atomic query+reset keeps
   writes between separate Get/Reset calls from disappearing. */
static int rb_watch_begin(RbWatchRegion *r) {
    ULONG_PTR count;if(!rb_watch_query(r,WRITE_WATCH_FLAG_RESET,&count))return 0;r->copied=0;
    if(!r->seeded){memcpy(r->shadow,r->base,r->size);r->copied=r->size;r->seeded=1;return 1;}
    for(ULONG_PTR i=0;i<count;++i){SIZE_T offset=(unsigned char *)r->dirty[i]-r->base,size=r->size-offset;if(size>4096)size=4096;
        memcpy(r->shadow+offset,r->base+offset,size);r->copied+=size;}
    return 1;
}
static int rb_watch_page_compare(const void *a,const void *b){uintptr_t x=(uintptr_t)*(PVOID const *)a,y=(uintptr_t)*(PVOID const *)b;return x<y?-1:x>y?1:0;}
static int rb_watch_store_undo(RbWatchRegion *r,RbJournal *journal) {
    ULONG_PTR count;if(!r->seeded || !rb_watch_query(r,0,&count))return 0;r->scanned=0;
    int sorted=1;for(ULONG_PTR i=1;i<count;++i)if((uintptr_t)r->dirty[i]<(uintptr_t)r->dirty[i-1])sorted=0;
    if(!sorted)qsort(r->dirty,count,sizeof(PVOID),rb_watch_page_compare);
    SIZE_T begin=0,end=0;
    for(ULONG_PTR i=0;i<count;++i){SIZE_T offset=(unsigned char *)r->dirty[i]-r->base,size=r->size-offset;if(size>4096)size=4096;r->scanned+=size;
        int changed=memcmp(r->shadow+offset,r->base+offset,size)!=0;
        if(end && (!changed || offset!=end)){if(!rb_store_before(journal,r->base+begin,r->shadow+begin,end-begin))return 0;begin=end=0;}
        if(changed){if(!end)begin=offset;end=offset+size;}}
    if(end && !rb_store_before(journal,r->base+begin,r->shadow+begin,end-begin))return 0;
    return 1;
}
#endif
