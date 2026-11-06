#ifndef TH18_NATIVE_HEAP_AUDIT_H
#define TH18_NATIVE_HEAP_AUDIT_H
#include "rollback_journal.h"
#include "native_write_watch.h"
/* Ownership audit and exact-input restoration qualification for explicit
   isolated tests only. Commands 50..59 observe; 60..69 temporarily retain
   allocations and replay a restored step. Production sessions cannot opt in. */
typedef struct {void *pointer;SIZE_T size;DWORD caller,thread,born,freed;LONG phase;int live,held,observed,physical;} RbHeapEntry;
static int rb_probe_external_owner(const RbHeapEntry *entry);
static int rb_audit_world_entry(const RbHeapEntry *entry){return entry->observed && entry->caller!=0x402110 && !rb_probe_external_owner(entry);}
static struct {
    int enabled,tracking,require_p2;uint32_t required_p2;CRITICAL_SECTION lock;HANDLE heap;
    RbHeapEntry *entries;unsigned count,dropped,frees,writes;DWORD serial,epoch;
    volatile LONG phase;DWORD main_thread;
    LPVOID (WINAPI *allocate)(HANDLE,DWORD,SIZE_T);
    BOOL (WINAPI *release)(HANDLE,DWORD,LPVOID);
    LPVOID (WINAPI *resize)(HANDLE,DWORD,LPVOID,SIZE_T);
    SIZE_T (WINAPI *size)(HANDLE,DWORD,LPCVOID);
    BOOL (WINAPI *write)(HANDLE,LPCVOID,DWORD,LPDWORD,LPOVERLAPPED);
} rb_audit;
typedef struct {void *pointer;SIZE_T size,offset;unsigned index;DWORD caller;} RbProbeBlock;
static struct {DWORD serial,stage;void *player;unsigned count,start_count,start_frees,start_writes,remaining,steps;SIZE_T used;RbProbeBlock *blocks;unsigned char *bytes,*after;int active,overflow,qualify,seeking;} rb_probe;
static RbJournal rb_native_undo;
#include "native_graphics_replay.h"
static uint64_t rb_first_hash[65536+4096];
static int rb_step_result[8];
typedef struct {void *pointer;SIZE_T size;uint64_t hash;unsigned index;} RbAssetCheck;
static RbAssetCheck rb_assets[4096];static unsigned rb_asset_count;
static uint64_t rb_asset_hash(const void *pointer,SIZE_T size){const unsigned char *p=pointer;uint64_t h=UINT64_C(14695981039346656037);for(SIZE_T i=0;i<size;++i)h=(h^p[i])*UINT64_C(1099511628211);return h;}
#ifdef COOP_NATIVE_FIXTURE
static DWORD rb_fixture_origin;
#endif
static __attribute__((noinline)) DWORD rb_audit_origin(void) {
#ifdef COOP_NATIVE_FIXTURE
    return rb_fixture_origin;
#else
    /* Native x86 callers do not provide unwind metadata. Verify allocator
       return sites on the bounded current stack instead of trusting an
       incomplete CaptureStackBackTrace chain through optimized CRT frames. */
    NT_TIB *tib=(NT_TIB *)NtCurrentTeb();const uintptr_t *scan=(const uintptr_t *)__builtin_frame_address(0)+1;
    for(unsigned i=0;i<256 && (uintptr_t)(scan+i+1)<=(uintptr_t)tib->StackBase;++i){uintptr_t ret=scan[i];
        if(ret<0x401005 || ret>=0x48dc00 || *(unsigned char *)(ret-5)!=0xe8)continue;
        uintptr_t target=ret+*(int32_t *)(ret-4);
        if(target==0x48dc71 || target==0x491a34 || target==0x49d020)return (DWORD)ret;
    }
    void *stack[16];USHORT n=CaptureStackBackTrace(1,16,stack,NULL);
    for(USHORT i=0;i<n;++i){uintptr_t p=(uintptr_t)stack[i];if(p>=0x401000 && p<0x48dc00)return (DWORD)p;}
    return 0;
#endif
}
static void rb_audit_add(void *pointer,SIZE_T size,DWORD caller,LONG phase) {
    if(!pointer)return;
    /* HeapWalk initially includes foreign DLL allocations. A later verified
       EXE allocation at an identical address proves that old foreign owner
       is gone; its stale record must not alias the new native object. */
    for(unsigned i=0;i<rb_audit.count;++i){RbHeapEntry *e=&rb_audit.entries[i];if(e->pointer==pointer && !e->observed && !e->held){e->live=e->physical=0;}}
    if(rb_audit.count==65536){++rb_audit.dropped;return;}
    rb_audit.entries[rb_audit.count++]=(RbHeapEntry){pointer,size,caller,GetCurrentThreadId(),rb_audit.epoch,0,phase,1,0,rb_audit.tracking,1};
}
static void rb_audit_remove(void *pointer) {
    for(unsigned i=rb_audit.count;i;--i)if(rb_audit.entries[i-1].pointer==pointer && rb_audit.entries[i-1].live){rb_audit.entries[i-1].live=0;rb_audit.entries[i-1].freed=rb_audit.epoch;return;}
}
typedef struct {int kind;void *pointer;SIZE_T size,initial,after;DWORD flags,error;unsigned index;} RbHeapEvent;
typedef struct {int mode,failed;DWORD thread;unsigned count,cursor,before_count,before_frees,before_writes,after_count,after_frees,after_writes;SIZE_T used;RbHeapEvent events[4096];unsigned char *initial,*live;} RbHeapTransaction;
static RbHeapTransaction rb_tx,rb_baseline_tx,rb_trial_tx;
static int rb_heap_check;
static int rb_tx_here(void){return rb_tx.mode && GetCurrentThreadId()==rb_tx.thread;}
static BOOL rb_audit_physical_release(void *pointer,DWORD flags) {
    RbWatchRegion *r=rb_watch_find(pointer);BOOL result;
    if(rb_heap_check && pointer && !r && !HeapValidate(rb_audit.heap,0,pointer)){
        for(unsigned i=rb_audit.count;i;--i)if(rb_audit.entries[i-1].pointer==pointer){RbHeapEntry *e=&rb_audit.entries[i-1];
            log_line("ROLLBACK invalid physical release pointer=%p index=%u size=%lu caller=%lx born=%lu freed=%lu live=%d physical=%d held=%d observed=%d thread=%lu epoch=%lu",pointer,i-1,(unsigned long)e->size,(unsigned long)e->caller,(unsigned long)e->born,(unsigned long)e->freed,e->live,e->physical,e->held,e->observed,(unsigned long)e->thread,(unsigned long)rb_audit.epoch);break;}
        rb_tx.failed=1;SetLastError(ERROR_INVALID_ADDRESS);return FALSE;}
    if(r){rb_watch_destroy(r);result=TRUE;}else result=rb_audit.release(rb_audit.heap,flags,pointer);
    if(result)for(unsigned i=rb_audit.count;i;--i){RbHeapEntry *e=&rb_audit.entries[i-1];if(e->pointer==pointer && e->physical){e->physical=0;e->held=0;break;}}
    return result;
}
static RbHeapEvent *rb_tx_event(int kind,void *pointer,SIZE_T size,DWORD flags,unsigned index,DWORD error) {
    if(rb_tx.count==4096){rb_tx.failed=1;return NULL;}
    RbHeapEvent *e=&rb_tx.events[rb_tx.count++];*e=(RbHeapEvent){kind,pointer,size,0,0,flags,error,index};return e;
}
static void rb_tx_begin(void) {
    if(!rb_probe.qualify)return;
    if(!rb_tx.initial)rb_tx.initial=VirtualAlloc(NULL,16u*1024u*1024u,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
    if(!rb_tx.live)rb_tx.live=VirtualAlloc(NULL,65536,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
    if(!rb_tx.initial || !rb_tx.live){rb_probe.qualify=0;return;}
    rb_tx.mode=1;rb_tx.thread=GetCurrentThreadId();rb_tx.failed=0;rb_tx.count=rb_tx.cursor=0;rb_tx.used=0;
    rb_tx.before_count=rb_audit.count;rb_tx.before_frees=rb_audit.frees;rb_tx.before_writes=rb_audit.writes;
    for(unsigned i=0;i<rb_audit.count;++i)rb_tx.live[i]=(unsigned char)rb_audit.entries[i].live;
}
static void rb_tx_commit(void) {
    DWORD error=GetLastError();rb_tx.mode=0;
    EnterCriticalSection(&rb_audit.lock);
    for(unsigned i=0;i<rb_audit.count;++i){RbHeapEntry *e=&rb_audit.entries[i];if(!e->held)continue;e->held=0;if(e->physical && !e->live && !rb_audit_physical_release(e->pointer,0))rb_tx.failed=1;}
    LeaveCriticalSection(&rb_audit.lock);SetLastError(error);
}
static void rb_tx_seal(void){rb_tx.mode=0;rb_tx.after_count=rb_audit.count;rb_tx.after_frees=rb_audit.frees;rb_tx.after_writes=rb_audit.writes;}
static LPVOID WINAPI rb_audit_allocate(HANDLE heap,DWORD flags,SIZE_T size) {
    if(heap==rb_audit.heap && rb_tx_here() && rb_tx.mode==2){
        EnterCriticalSection(&rb_audit.lock);
        RbHeapEvent *e=rb_tx.cursor<rb_tx.count?&rb_tx.events[rb_tx.cursor]:NULL;
        if(e && e->kind==1 && e->size==size && e->flags==flags && rb_audit.entries[e->index].physical){
            ++rb_tx.cursor;memcpy(e->pointer,rb_tx.initial+e->initial,e->size);rb_audit.entries[e->index].live=1;
            LeaveCriticalSection(&rb_audit.lock);SetLastError(e->error);return e->pointer;
        }
        rb_tx.failed=1;LeaveCriticalSection(&rb_audit.lock);
    }
    /* Exact fixed pools only; native EXE HeapSize/Free/ReAlloc imports are
       intercepted too. No foreign DLL/system allocator is redirected. */
    void *p=NULL;
    if(rb_watch_enabled && heap==rb_audit.heap && !(flags&~(HEAP_ZERO_MEMORY|HEAP_NO_SERIALIZE)) &&
       (size==60236984 || size==8012280 || size==15121192))p=rb_watch_allocate(size);
    if(!p)p=rb_audit.allocate(heap,flags,size);DWORD error=GetLastError();
    if(heap==rb_audit.heap && p){DWORD origin=rb_audit_origin();EnterCriticalSection(&rb_audit.lock);unsigned index=rb_audit.count;rb_audit_add(p,size,origin,rb_audit.phase);
        int world=!rb_runtime_enabled || (index<rb_audit.count && rb_audit_world_entry(&rb_audit.entries[index]));
        if(world && rb_tx.mode && !rb_tx_here())rb_tx.failed=1;
        if(world && rb_tx_here() && rb_tx.mode==1){RbHeapEvent *e=rb_tx_event(1,p,size,flags,index,error);
            if(!e || size>16u*1024u*1024u-rb_tx.used || index>=rb_audit.count)rb_tx.failed=1;
            else{e->initial=rb_tx.used;memcpy(rb_tx.initial+rb_tx.used,p,size);rb_tx.used+=size;}}
        LeaveCriticalSection(&rb_audit.lock);}
    SetLastError(error);return p;
}
static BOOL WINAPI rb_audit_release(HANDLE heap,DWORD flags,LPVOID pointer) {
    if(heap!=rb_audit.heap)return rb_audit.release(heap,flags,pointer);
    if(pointer && rb_tx_here()){
        DWORD error=GetLastError();EnterCriticalSection(&rb_audit.lock);unsigned index=UINT32_MAX;
        for(unsigned i=rb_audit.count;i;--i)if(rb_audit.entries[i-1].live && rb_audit.entries[i-1].pointer==pointer){index=i-1;break;}
        if(index!=UINT32_MAX && rb_runtime_enabled && !rb_audit_world_entry(&rb_audit.entries[index])){
            /* File/stream providers are external resources, not speculative
               births/deaths. A main-thread attempted disposal is precise. */
            int new_here=index>=rb_tx.before_count && rb_audit.entries[index].thread==rb_tx.thread && rb_audit.entries[index].born==rb_audit.epoch;
            if(!new_here && rb_request_precise()){LeaveCriticalSection(&rb_audit.lock);SetLastError(error);return TRUE;}
            BOOL result=rb_audit_physical_release(pointer,flags);DWORD released_error=GetLastError();if(result)rb_audit_remove(pointer);
            LeaveCriticalSection(&rb_audit.lock);SetLastError(released_error);return result;
        }
        if(index!=UINT32_MAX){
            if(rb_tx.mode==1){if(!rb_tx_event(2,pointer,0,flags,index,error))rb_tx.failed=1;}
            else {RbHeapEvent *e=rb_tx.cursor<rb_tx.count?&rb_tx.events[rb_tx.cursor]:NULL;
                if(!e || e->kind!=2 || e->pointer!=pointer || e->flags!=flags)rb_tx.failed=1;else{++rb_tx.cursor;error=e->error;}}
            ++rb_audit.frees;rb_audit_remove(pointer);rb_audit.entries[index].held=1;
            LeaveCriticalSection(&rb_audit.lock);SetLastError(error);return TRUE;
        }
        for(unsigned i=rb_audit.count;i;--i)if(rb_audit.entries[i-1].pointer==pointer && rb_audit.entries[i-1].held){rb_tx.failed=1;LeaveCriticalSection(&rb_audit.lock);SetLastError(ERROR_INVALID_ADDRESS);return FALSE;}
        rb_tx.failed=1;LeaveCriticalSection(&rb_audit.lock);
    }
    EnterCriticalSection(&rb_audit.lock);BOOL result=rb_audit_physical_release(pointer,flags);DWORD error=GetLastError();
    int world=1;for(unsigned i=rb_audit.count;i;--i)if(rb_audit.entries[i-1].pointer==pointer){world=!rb_runtime_enabled || rb_audit_world_entry(&rb_audit.entries[i-1]);break;}
    if(world && rb_tx.mode && !rb_tx_here())rb_tx.failed=1;
    if(result){if(world)++rb_audit.frees;rb_audit_remove(pointer);}LeaveCriticalSection(&rb_audit.lock);SetLastError(error);return result;
}
static BOOL WINAPI rb_audit_write(HANDLE file,LPCVOID bytes,DWORD size,LPDWORD written,LPOVERLAPPED overlapped) {
    if(rb_request_precise()){if(written)*written=0;SetLastError(ERROR_RETRY);return FALSE;}
    if(rb_replaying && rb_tx_here()){rb_tx.failed=1;if(written)*written=0;SetLastError(ERROR_RETRY);return FALSE;}
    InterlockedIncrement((LONG *)&rb_audit.writes);return rb_audit.write(file,bytes,size,written,overlapped);
}
static LPVOID WINAPI rb_audit_resize(HANDLE heap,DWORD flags,LPVOID pointer,SIZE_T size) {
    if(heap!=rb_audit.heap)return rb_audit.resize(heap,flags,pointer,size);
    if(rb_request_precise()){SetLastError(ERROR_RETRY);return NULL;}
    DWORD origin=rb_audit_origin();EnterCriticalSection(&rb_audit.lock);
    if(rb_tx.mode)rb_tx.failed=1; /* Resize is a precise-input boundary for now. */
    if(rb_watch_find(pointer)){LeaveCriticalSection(&rb_audit.lock);SetLastError(ERROR_NOT_SUPPORTED);log_line("RB_WATCH fixed pool resize rejected");return NULL;}
    void *p=rb_audit.resize(heap,flags,pointer,size);DWORD error=GetLastError();
    if(p){for(unsigned i=rb_audit.count;i;--i)if(rb_audit.entries[i-1].pointer==pointer && rb_audit.entries[i-1].physical){rb_audit.entries[i-1].physical=0;break;}rb_audit_remove(pointer);rb_audit_add(p,size,origin,rb_audit.phase);}
    LeaveCriticalSection(&rb_audit.lock);SetLastError(error);return p;
}
static SIZE_T WINAPI rb_audit_size(HANDLE heap,DWORD flags,LPCVOID pointer) {
    RbWatchRegion *r=heap==rb_audit.heap?rb_watch_find(pointer):NULL;
    return r?r->size:rb_audit.size(heap,flags,pointer);
}
#include "native_audio_audit.h"
static int rb_audit_swap(uintptr_t address,void *expected,void *replacement) {
    if(PTR(address)!=expected)return 0;DWORD old;
    if(!VirtualProtect((void *)address,sizeof(void *),PAGE_READWRITE,&old))return 0;
    PTR(address)=replacement;VirtualProtect((void *)address,sizeof(void *),old,&old);return 1;
}
#include "native_file_fence.h"
#include "native_asset_fence.h"
static int rb_probe_root(void *pointer);
static int rb_audit_install(void) {
    wchar_t flag[4];
    /* Delay sessions never install rollback heap, audio or file hooks. */
    if(online_map && (online_map->reserved[1]&32u))return 1;
    rb_runtime_enabled=online_map && (online_map->reserved[1]&16u);
    rb_heap_check=online_map && (online_map->reserved[1]&1) && GetEnvironmentVariableW(L"TH18_COOP_RB_HEAP_CHECK",flag,4)==1 && flag[0]==L'1';
    if(!rb_runtime_enabled && (!online_map || !(online_map->reserved[1]&1) || GetEnvironmentVariableW(L"TH18_COOP_RB_AUDIT",flag,4)!=1 || flag[0]!=L'1'))return 1;
    HMODULE kernel=GetModuleHandleW(L"kernel32.dll");
    rb_audit.allocate=(void *)GetProcAddress(kernel,"HeapAlloc");rb_audit.release=(void *)GetProcAddress(kernel,"HeapFree");rb_audit.resize=(void *)GetProcAddress(kernel,"HeapReAlloc");
    rb_audit.size=(void *)GetProcAddress(kernel,"HeapSize");
    rb_audit.write=(void *)GetProcAddress(kernel,"WriteFile");
    if(PTR(0x4ad104)!=(void *)rb_audit.allocate || PTR(0x4ad100)!=(void *)rb_audit.release || PTR(0x4ad188)!=(void *)rb_audit.resize || PTR(0x4ad184)!=(void *)rb_audit.size || PTR(0x4ad0a0)!=(void *)rb_audit.write){log_line("RB_AUDIT import guard rejected");return 0;}
    rb_audit.heap=PTR(0x4cf240);if(!rb_audit.heap)return 0;
    rb_audit.entries=VirtualAlloc(NULL,65536*sizeof(RbHeapEntry),MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);if(!rb_audit.entries)return 0;
    InitializeCriticalSection(&rb_audit.lock);
    if(!HeapLock(rb_audit.heap))return 0;
    PROCESS_HEAP_ENTRY item={0};while(HeapWalk(rb_audit.heap,&item))if(item.wFlags&PROCESS_HEAP_ENTRY_BUSY){rb_audit_add(item.lpData,item.cbData,0,0);if(rb_probe_root(item.lpData))rb_audit.entries[rb_audit.count-1].observed=2;}
    HeapUnlock(rb_audit.heap);
    rb_audit.tracking=1;
    if(!rb_audit_swap(0x4ad104,(void *)rb_audit.allocate,rb_audit_allocate))return 0;
    if(!rb_audit_swap(0x4ad100,(void *)rb_audit.release,rb_audit_release)){
        rb_audit_swap(0x4ad104,rb_audit_allocate,(void *)rb_audit.allocate);return 0;
    }
    if(!rb_audit_swap(0x4ad188,(void *)rb_audit.resize,rb_audit_resize)){
        rb_audit_swap(0x4ad100,rb_audit_release,(void *)rb_audit.release);rb_audit_swap(0x4ad104,rb_audit_allocate,(void *)rb_audit.allocate);return 0;
    }
    if(!rb_audit_swap(0x4ad0a0,(void *)rb_audit.write,rb_audit_write)){
        rb_audit_swap(0x4ad188,rb_audit_resize,(void *)rb_audit.resize);rb_audit_swap(0x4ad100,rb_audit_release,(void *)rb_audit.release);rb_audit_swap(0x4ad104,rb_audit_allocate,(void *)rb_audit.allocate);return 0;
    }
    if(!rb_audit_swap(0x4ad184,(void *)rb_audit.size,rb_audit_size)){
        rb_audit_swap(0x4ad0a0,rb_audit_write,(void *)rb_audit.write);rb_audit_swap(0x4ad188,rb_audit_resize,(void *)rb_audit.resize);
        rb_audit_swap(0x4ad100,rb_audit_release,(void *)rb_audit.release);rb_audit_swap(0x4ad104,rb_audit_allocate,(void *)rb_audit.allocate);return 0;
    }
    rb_watch_enabled=rb_runtime_enabled || (GetEnvironmentVariableW(L"TH18_COOP_RB_WATCH",flag,4)==1 && flag[0]==L'1');
    rb_control_requested=rb_runtime_enabled || (GetEnvironmentVariableW(L"TH18_COOP_CONTROLLED",flag,4)==1 && flag[0]==L'1');
    if(rb_control_requested && !rb_control_install())return 0;
    wchar_t required[16],*end=NULL;DWORD required_length=GetEnvironmentVariableW(L"TH18_COOP_RB_REQUIRE_P2",required,16);
    if(required_length){if(required_length>=16)return 0;unsigned long value=wcstoul(required,&end,0);if(!end || *end || value>0xcfbu)return 0;rb_audit.require_p2=1;rb_audit.required_p2=(uint32_t)value;}
    if(!rb_audio_install()){log_line("RB_AUDIT audio call-site guard rejected");return 0;}
    if(!rb_files_install()){log_line("Rollback file boundary import guard rejected");return 0;}
    if(rb_watch_enabled)log_line("RB_WATCH fixed native pools enabled in isolated verifier only");
    rb_audit.enabled=1;log_line("RB_AUDIT enabled in explicit isolated test; initial_blocks=%u",rb_audit.count);return 1;
}
static int rb_probe_root(void *pointer) {
    if(pointer==PTR(0x51f65c))return 1;
    for(uintptr_t at=0x4cf290;at<=0x4cf454;at+=4)if(PTR(at)==pointer)return 1;
    for(unsigned id=0;id<party_count();++id)if(pointer==ship(id) || pointer==ability(id) || (ship(id) && pointer==(void *)(uintptr_t)FIELD(ship(id),0x47940)))return 1;
    return 0;
}
static int rb_probe_external_owner(const RbHeapEntry *entry) {
    /* Native IStream file source/adapter: 489632 constructs the160-byte
       provider (CreateFileA at48abe8); 48983c constructs the168-byte stream,
       vtable4b9030 and live file cursor at+88. Audio decoding advances these
       asynchronously. They are external output resources, not world state. */
    return entry->caller==0x489632 || entry->caller==0x48983c;
}
static int rb_audit_readable(void *pointer,SIZE_T size) {
    uintptr_t at=(uintptr_t)pointer,end=at+size;if(!at || end<at)return 0;
    while(at<end){MEMORY_BASIC_INFORMATION m;if(!VirtualQuery((void *)at,&m,sizeof(m)) || m.State!=MEM_COMMIT || (m.Protect&(PAGE_NOACCESS|PAGE_GUARD)))return 0;uintptr_t next=(uintptr_t)m.BaseAddress+m.RegionSize;if(next<=at)return 0;at=next;}return 1;
}
static int rb_audit_adopt_pool(void *pointer,SIZE_T size) {
    /* Injection can observe the constructor's allocation before it publishes
       the singleton. Admit only a verified, exact fixed-pool root, never an
       arbitrary pointer from the singleton table or a stale foreign alias. */
    if(!pointer)return 0;
    for(unsigned i=rb_audit.count;i;--i){RbHeapEntry *e=&rb_audit.entries[i-1];
        if(e->pointer!=pointer || !e->physical || !e->live)continue;
        if(e->observed)return e->size==size;
        if(e->held || e->size!=size || rb_audit.size(rb_audit.heap,0,pointer)<size)return 0;
        e->observed=2;log_line("ROLLBACK adopted late-published initial pool pointer=%p bytes=%lu",pointer,(unsigned long)size);return 1;
    }return 0;
}
static const unsigned char *rb_probe_saved(const RbProbeBlock *block) {
    RbWatchRegion *r=rb_watch_find(block->pointer);return r?r->shadow:rb_probe.bytes+block->offset;
}
static void rb_probe_add(void *pointer,SIZE_T size,unsigned index,DWORD caller) {
    if(!size)return;if(rb_probe.count==65536 || size>128u*1024u*1024u-rb_probe.used){rb_probe.overflow=1;return;}
    for(unsigned i=0;i<rb_probe.count;++i)if(rb_probe.blocks[i].pointer==pointer){if(size>rb_probe.blocks[i].size)rb_probe.overflow=1;return;}
    RbProbeBlock *b=&rb_probe.blocks[rb_probe.count++];*b=(RbProbeBlock){pointer,size,rb_probe.used,index,caller};
    RbWatchRegion *r=rb_watch_find(pointer);
    if(r){if(r->size!=size || !rb_watch_begin(r))rb_probe.overflow=1;}
    else memcpy(rb_probe.bytes+rb_probe.used,pointer,size);rb_probe.used+=size;
}
static int rb_capture_world(int qualifying) {
    if(!rb_probe.bytes)rb_probe.bytes=VirtualAlloc(NULL,128u*1024u*1024u,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
    if(!rb_probe.blocks)rb_probe.blocks=VirtualAlloc(NULL,65536*sizeof(RbProbeBlock),MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
    if(!rb_probe.bytes || !rb_probe.blocks)return 0;
    rb_probe.count=0;rb_probe.used=0;rb_probe.overflow=0;rb_asset_count=0;
    EnterCriticalSection(&rb_audit.lock);
    if(rb_runtime_enabled){
        if(!rb_audit_adopt_pool(PTR(0x51f65c),60236984) ||
           (PTR(0x4cf2c8) && !rb_audit_adopt_pool(PTR(0x4cf2c8),8256))){LeaveCriticalSection(&rb_audit.lock);log_line("ROLLBACK native animation/text pool ownership missing");return 0;}
        /* Every observed allocation belongs to EXE HeapAlloc/Free imports.
           Initial process-heap objects are admitted only by explicit roots;
           foreign Windows/DLL heap objects are never traversed here. */
        for(unsigned i=0;i<rb_audit.count;++i){RbHeapEntry *e=&rb_audit.entries[i];
            if(e->physical && e->live && !rb_watch_find(e->pointer) && !rb_probe_external_owner(e) &&
               e->observed && e->caller!=0x402110)rb_probe_add(e->pointer,e->size,i,e->caller);}
    }else{
    if(!HeapLock(rb_audit.heap)){LeaveCriticalSection(&rb_audit.lock);return 0;}
    PROCESS_HEAP_ENTRY item={0};while(HeapWalk(rb_audit.heap,&item))if(item.wFlags&PROCESS_HEAP_ENTRY_BUSY){
        for(unsigned i=rb_audit.count;i;--i){RbHeapEntry *e=&rb_audit.entries[i-1];if(!e->live || e->pointer!=item.lpData)continue;
            SIZE_T size=e->size<item.cbData?e->size:item.cbData;
            if(!rb_probe_external_owner(e) && ((e->observed && e->caller!=0x402110) || rb_probe_root(e->pointer)))rb_probe_add(e->pointer,size,i-1,e->caller);
            else if(qualifying && e->observed && e->caller==0x402110){
                if(rb_asset_count==4096)rb_probe.overflow=1;
                else rb_assets[rb_asset_count++]=(RbAssetCheck){e->pointer,size,rb_asset_hash(e->pointer,size),i-1};
            }break;
        }
    }
    HeapUnlock(rb_audit.heap);}
    LeaveCriticalSection(&rb_audit.lock);
    /* Virtual-backed pools are intentionally absent from HeapWalk. Their
       explicit registry entries, not a fabricated heap membership, own them. */
    for(unsigned i=0;i<rb_audit.count;++i){RbHeapEntry *e=&rb_audit.entries[i];if(e->live && rb_watch_find(e->pointer))rb_probe_add(e->pointer,e->size,i,e->caller);}
    /* Observe native static data without restoring it. CRT locks, audio and
       render/service words must later be split out by explicit owner rules. */
    rb_probe_add((void *)0x4c3000,0xae4c4,UINT32_MAX,0);
    rb_probe_add(guests,sizeof(guests),UINT32_MAX,0);
    rb_probe_add(partner,sizeof(partner),UINT32_MAX,0);
    for(unsigned id=1;id<party_count();++id)if(ability(id))rb_probe_add(ability(id),0xd70,UINT32_MAX,0);
#define RB_CAPTURE_WORLD(value) rb_probe_add((void *)&(value),sizeof(value),UINT32_MAX,0)
    RB_CAPTURE_WORLD(input_latch);RB_CAPTURE_WORLD(guest_slot);RB_CAPTURE_WORLD(frames);RB_CAPTURE_WORLD(guest_ticks);
    RB_CAPTURE_WORLD(observed_stage);RB_CAPTURE_WORLD(departed_stage);RB_CAPTURE_WORLD(departed_ghosts);
    RB_CAPTURE_WORLD(item_owner);RB_CAPTURE_WORLD(transfer_owner);RB_CAPTURE_WORLD(transfer_power);RB_CAPTURE_WORLD(last_age);
    RB_CAPTURE_WORLD(collected);RB_CAPTURE_WORLD(damage_done);RB_CAPTURE_WORLD(hits);RB_CAPTURE_WORLD(wipe_frames);RB_CAPTURE_WORLD(team_over);
    RB_CAPTURE_WORLD(bomb_owner);RB_CAPTURE_WORLD(menu_previous);RB_CAPTURE_WORLD(menu_age);RB_CAPTURE_WORLD(requested);RB_CAPTURE_WORLD(wanted);RB_CAPTURE_WORLD(attached);
    RB_CAPTURE_WORLD(rb_audio.status);
    RB_CAPTURE_WORLD(rb_audio.cursor);
    RB_CAPTURE_WORLD(spirit_draw_alpha);RB_CAPTURE_WORLD(spirit_was_drawn);
#undef RB_CAPTURE_WORLD
    rb_probe.start_count=rb_audit.count;rb_probe.start_frees=rb_audit.frees;rb_probe.start_writes=rb_audit.writes;
    if(rb_probe.overflow){log_line("RB_AUDIT frame probe rejected: capture budget exceeded");return 0;}
    rb_probe.active=1;rb_probe.qualify=qualifying;return 1;
}
static void rb_probe_begin(int drawing,uint32_t p2_keys) {
    if(!rb_audit.enabled || !p1 || !ship(1) || scope_depth || U32(0x4cd894) || PTR(0x4cf2a4))return;
    DWORD serial=online_map->reserved_tail[2];if(drawing?(serial<100 || serial>109):(serial<50 || serial>139 || (serial>=100 && serial<110) || (serial>=120 && serial<130)))return;
    if(!drawing && rb_audit.require_p2 && p2_keys!=rb_audit.required_p2)return;
    int seeking=(serial>=70 && serial<90) || (serial>=95 && serial<100) || (serial>=112 && serial<120) || serial>=132;void *pause=PTR(0x4cf40c);if(!drawing && serial>=70 && pause && FIELD(pause,0x1ec))return;
    if(serial==rb_probe.serial && (!seeking || !rb_probe.remaining))return;
    if(serial!=rb_probe.serial && seeking)rb_probe.remaining=300;
    rb_probe.serial=serial;
    rb_probe.seeking=seeking;if(seeking)--rb_probe.remaining;
    int qualifying=!drawing && serial>=60;
    if(qualifying && !rb_audio_checkpoint()){log_line("RB_VERIFY skipped serial=%lu: music/status boundary command=%lu",(unsigned long)serial,(unsigned long)U32(0x56d204));return;}
    rb_probe.steps=serial>=110?(serial&1?8:4):1;
    rb_probe.qualify=0;rb_probe.stage=U32(0x4cccdc);rb_probe.player=p1;
    if(qualifying && !rb_graphics_begin())return;
    rb_capture_world(qualifying);
}

typedef struct {uintptr_t begin,end;int mode;} RbGlobalSpan;
static const RbGlobalSpan rb_global_spans[]={
    {0x4ca210,0x4ca8ac,0},{0x4cccc0,0x4ccd98,0},{0x4cd2ac,0x4cd2b4,0},
    {0x4cd400,0x4cd5e4,0},{0x4cd5e4,0x4cd5f4,1},{0x4cf280,0x4cf290,0},
    {0x4cf290,0x4cf458,1},{0x4cf2d4,0x4cf2d8,-1},{0x56ad9c,0x56c5fc,0},
    {0x56c804,0x56d104,0},{0x56d204,0x56f384,1},{0x570520,0x570590,0}
};
static int rb_global_span_active(const RbGlobalSpan *span){return !span->mode || (span->mode>0?rb_runtime_enabled:!rb_runtime_enabled);}
static int rb_probe_global_owned(uintptr_t at) {
    /* Four native TL vertices (position/color/UV, stride0x1c), filled by
       47e8f0/47ed50 and adjusted by47dce0 before copying to VM+4f0. */
    if(at>=0x570520 && at<0x570590)return 1;
    if(rb_runtime_enabled && ((at>=0x4cd5e4 && at<0x4cd5f4) || (at>=0x4cf290 && at<0x4cf458) || (at>=0x56d204 && at<0x56f384)))return 1;
    /* 443ae0 updates current high-score value/flags at cccc0/cccc4 when
       the shared score first exceeds the profile record. */
    return (at>=0x4ca210 && at<0x4ca8ac) || (at>=0x4cccc0 && at<0x4ccd98) ||
        /* Native stage Tick41c240 copies89 words from its camera+230 to
           4cd478: the complete camera ends5dc, including fog ARGB at5d8. */
        (at>=0x4cd400 && at<0x4cd5e4) || (at>=0x4cf280 && at<0x4cf290) ||
        at==0x4cd2ac || at==0x4cd2b0 || /* Scaled shake X/Y: native writes475d62/475de0. */
        at==0x4cf2d4 || /* Native per-player kill streak, written at42f295. */
        (at>=0x56ad9c && at<0x56c5fc) || (at>=0x56c804 && at<0x56d104); /* SFX only; music is an external boundary. */
}
static int rb_probe_build_undo(void) {
    if(rb_native_undo.failed || rb_native_undo.open || rb_native_undo.count)rb_destroy(&rb_native_undo);
    if(!rb_native_undo.frames && !rb_init(&rb_native_undo,1,16u*1024u*1024u,8192))return 0;
    if(!rb_begin(&rb_native_undo,rb_audit.epoch))return 0;
    for(unsigned i=0;i<rb_probe.count;++i){RbProbeBlock *b=&rb_probe.blocks[i];const unsigned char *saved=rb_probe_saved(b),*live=b->pointer;
        RbWatchRegion *watch=rb_watch_find(b->pointer);if(watch){if(!rb_watch_store_undo(watch,&rb_native_undo))return 0;continue;}
        if(b->pointer==(void *)0x4c3000){
            for(size_t n=0;n+4<=b->size;n+=4)if(rb_probe_global_owned((uintptr_t)b->pointer+n) && memcmp(saved+n,live+n,4))
                if(!rb_store_before(&rb_native_undo,(unsigned char *)b->pointer+n,saved+n,4))return 0;
        }else for(size_t n=0;n<b->size;n+=4096){SIZE_T size=b->size-n;if(size>4096)size=4096;
            if(memcmp(saved+n,live+n,size) && !rb_store_before(&rb_native_undo,(unsigned char *)b->pointer+n,saved+n,size))return 0;
        }
    }
    return rb_end(&rb_native_undo);
}
static int rb_probe_wrong_input(void *scheduler,Tick tick,uint32_t inputs[3]) {
    if(rb_probe.serial<90)return 1;
    if(!ship(1) || FIELD(ship(1),0x476ac)!=1){log_line("RB_CORRECTION skipped inactive P2");return 0;}
    rb_baseline_tx=rb_tx;
    rb_tx=rb_trial_tx;rb_probe.qualify=1;rb_tx_begin();rb_probe.qualify=0;
    if(rb_tx.mode!=1){rb_tx=rb_baseline_tx;return 0;}
    uint32_t exact=inputs[1];inputs[1]=(exact&~0xf0u)|((exact&0x80)?0x40u:0x80u);
    uint32_t predicted=inputs[1];rb_replaying=1;
    for(unsigned step=0;step<rb_probe.steps;++step){if(step && !rb_graphics_draw()){rb_tx.failed=1;break;}tick(scheduler);if(rb_tx.failed || U32(0x4cd894) || p1!=rb_probe.player || U32(0x4cccdc)!=rb_probe.stage){rb_tx.failed=1;break;}}
    rb_replaying=0;inputs[1]=exact;rb_tx_seal();
    unsigned different=0;int moved=0;
    for(unsigned i=0;i<rb_probe.count;++i){RbProbeBlock *b=&rb_probe.blocks[i];
        if(!rb_audit_readable(b->pointer,b->size)){rb_tx.failed=1;continue;}
        if(memcmp(rb_probe.after+b->offset,b->pointer,b->size))++different;
        if(b->pointer==ship(1) && b->size>0x628)moved=memcmp(rb_probe.after+b->offset+0x620,OFF(b->pointer,0x620),8)!=0;
        if(b->pointer==(void *)0x4c3000)for(size_t n=0;n+4<=b->size;n+=4)
            if(memcmp(rb_probe_saved(b)+n,(unsigned char *)b->pointer+n,4) && !rb_probe_global_owned((uintptr_t)b->pointer+n))rb_tx.failed=1;
    }
    if(rb_tx.failed || U32(0x4cd894) || p1!=rb_probe.player || U32(0x4cccdc)!=rb_probe.stage || rb_audit.writes!=rb_tx.before_writes){
        log_line("RB_CORRECTION failed at speculative boundary");return -1;
    }
    for(unsigned i=0;i<rb_tx.count;++i){DWORD origin=rb_audit.entries[rb_tx.events[i].index].caller;
        if(origin==0x402110 || (origin>=0x486000 && origin<0x487000)){log_line("RB_CORRECTION failed speculative asset boundary");return -1;}}
    for(unsigned i=0;i<rb_asset_count;++i){RbAssetCheck *a=&rb_assets[i];if(!rb_audit.entries[a->index].live || !rb_audit_readable(a->pointer,a->size) || rb_asset_hash(a->pointer,a->size)!=a->hash){log_line("RB_CORRECTION failed resource ownership guard");return -1;}}
    if(!rb_probe_build_undo() || !rb_undo_to(&rb_native_undo,rb_audit.epoch)){log_line("RB_CORRECTION failed speculative undo");return -1;}
    unsigned abandoned=rb_audit.count-rb_tx.before_count;DWORD error=GetLastError();
    EnterCriticalSection(&rb_audit.lock);
    for(unsigned i=rb_tx.before_count;i<rb_audit.count;++i){RbHeapEntry *e=&rb_audit.entries[i];if(!rb_audit_physical_release(e->pointer,0))rb_tx.failed=1;}
    rb_audit.count=rb_tx.before_count;LeaveCriticalSection(&rb_audit.lock);SetLastError(error);
    int failed=rb_tx.failed;rb_trial_tx=rb_tx;rb_tx=rb_baseline_tx;
    log_line("RB_CORRECTION trial serial=%lu exact=%lx predicted=%lx changed_owners=%u player_moved=%d discarded_allocs=%u",(unsigned long)rb_probe.serial,(unsigned long)exact,(unsigned long)predicted,different,moved,abandoned);
    return failed?-1:1;
}
static int rb_probe_boundary_ok(void) {
    if(rb_tx.failed || scope_depth || U32(0x4cd894) || U32(0x4cccdc)!=rb_probe.stage || p1!=rb_probe.player || rb_audit.writes!=rb_probe.start_writes)return 0;
    for(unsigned i=0;i<rb_tx.count;++i){DWORD origin=rb_audit.entries[rb_tx.events[i].index].caller;
        if(origin==0x402110 || (origin>=0x486000 && origin<0x487000))return 0;
    }
    for(unsigned i=0;i<rb_asset_count;++i){RbAssetCheck *a=&rb_assets[i];
        if(!rb_audit.entries[a->index].live || !rb_audit_readable(a->pointer,a->size) || rb_asset_hash(a->pointer,a->size)!=a->hash)return 0;
    }
    for(unsigned i=0;i<rb_probe.count;++i){RbProbeBlock *b=&rb_probe.blocks[i];if(!rb_audit_readable(b->pointer,b->size))return 0;
        if(b->pointer==(void *)0x4c3000)for(size_t n=0;n+4<=b->size;n+=4)
            if(memcmp(rb_probe_saved(b)+n,(unsigned char *)b->pointer+n,4) && !rb_probe_global_owned((uintptr_t)b->pointer+n))return 0;
    }
    return 1;
}
static void rb_probe_reset_lifetimes(void) {
    for(unsigned i=0;i<rb_audit.count;++i){rb_audit.entries[i].live=i<rb_tx.before_count?rb_tx.live[i]:0;if(i>=rb_tx.before_count)rb_audit.entries[i].held=1;}
    rb_audit.frees=rb_tx.before_frees;
}
#include "native_step_history.h"
/* Test-only logical-window qualification. Extra lookahead/correction steps
   never Draw/Present or publish network/input output. Finally restore the
   accurate FIRST step so normal lockstep still admits just one game step.
   This deliberately does not claim restoration across presentation gaps. */
static int rb_probe_replay(void *scheduler,Tick tick,int first_result,uint32_t inputs[3]) {
    if(!rb_probe.qualify || rb_probe.overflow)return 0;rb_probe.qualify=0;
    if(rb_tx.failed || rb_audit.count!=rb_tx.after_count || rb_audit.frees!=rb_tx.after_frees || rb_audit.writes!=rb_probe.start_writes ||
       scope_depth || U32(0x4cd894) || U32(0x4cccdc)!=rb_probe.stage || p1!=rb_probe.player){
        log_line("RB_VERIFY skipped serial=%lu: lifetime/output/scene boundary alloc=%u free=%u write=%u",(unsigned long)rb_probe.serial,rb_audit.count-rb_probe.start_count,rb_audit.frees-rb_probe.start_frees,rb_audit.writes-rb_probe.start_writes);rb_tx_commit();return 0;
    }
    for(unsigned i=0;i<rb_tx.count;++i){RbHeapEvent *event=&rb_tx.events[i];DWORD origin=rb_audit.entries[event->index].caller;
        if(origin==0x402110 || (origin>=0x486000 && origin<0x487000)){log_line("RB_VERIFY skipped asset lifetime boundary at %08lx",(unsigned long)origin);rb_tx_commit();return 0;}}
    for(unsigned i=0;i<rb_asset_count;++i){RbAssetCheck *asset=&rb_assets[i];
        if(!rb_audit.entries[asset->index].live || !rb_audit_readable(asset->pointer,asset->size) || rb_asset_hash(asset->pointer,asset->size)!=asset->hash){
            log_line("RB_VERIFY skipped mutable resource data at %p size=%lu",asset->pointer,(unsigned long)asset->size);rb_tx_commit();return 0;
        }
    }
    for(unsigned i=0;i<rb_probe.count;++i){RbProbeBlock *b=&rb_probe.blocks[i];
        if(!rb_audit_readable(b->pointer,b->size)){rb_tx_commit();return 0;}
        if(b->pointer==(void *)0x4c3000)for(size_t n=0;n+4<=b->size;n+=4)
            if(*(const uint32_t *)(rb_probe_saved(b)+n)!=*(uint32_t *)((unsigned char *)b->pointer+n) && !rb_probe_global_owned((uintptr_t)b->pointer+n)){
                log_line("RB_VERIFY skipped serial=%lu: unclassified global=%08lx",(unsigned long)rb_probe.serial,(unsigned long)((uintptr_t)b->pointer+n));rb_tx_commit();return 0;
            }
    }
    unsigned first_events=rb_tx.count,first_count=rb_audit.count,first_frees=rb_audit.frees;
    rb_step_result[0]=first_result;
    if(rb_probe.steps>1){
        for(unsigned i=0;i<rb_probe.count;++i)rb_first_hash[i]=rb_asset_hash(rb_probe.blocks[i].pointer,rb_probe.blocks[i].size);
        for(unsigned i=0;i<first_events;++i)if(rb_tx.events[i].kind==1)rb_first_hash[rb_probe.count+i]=rb_asset_hash(rb_tx.events[i].pointer,rb_tx.events[i].size);
        if(!rb_history_begin()){log_line("RB_HISTORY FAIL initial checkpoint");return -1;}
        rb_replaying=1;rb_tx.mode=1;
        for(unsigned step=1;step<rb_probe.steps;++step){
            if(!rb_graphics_draw()){rb_replaying=0;rb_tx_seal();return -1;}
            if(!rb_history_before(step)){rb_replaying=0;rb_tx_seal();log_line("RB_HISTORY FAIL pre-step capture");return -1;}
            rb_step_result[step]=tick(scheduler);
            if(!rb_probe_boundary_ok()){rb_replaying=0;rb_tx_seal();log_line("RB_WINDOW FAIL speculative boundary serial=%lu step=%u",(unsigned long)rb_probe.serial,step);return -1;}
            if(!rb_history_after()){rb_replaying=0;rb_tx_seal();log_line("RB_HISTORY FAIL post-step journal");return -1;}
        }
        rb_replaying=0;rb_tx_seal();
    }
    if(!rb_probe.after)rb_probe.after=VirtualAlloc(NULL,128u*1024u*1024u,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
    if(!rb_probe.after){rb_tx_commit();return 0;}
    SIZE_T total=rb_probe.used;
    for(unsigned i=0;i<rb_tx.count;++i){RbHeapEvent *e=&rb_tx.events[i];if(e->kind!=1)continue;
        if(e->size>128u*1024u*1024u-total){log_line("RB_VERIFY skipped post-state budget");rb_tx_commit();return 0;}
        e->after=total;memcpy(rb_probe.after+total,e->pointer,e->size);total+=e->size;
    }
    for(unsigned i=0;i<rb_probe.count;++i){RbProbeBlock *b=&rb_probe.blocks[i];memcpy(rb_probe.after+b->offset,b->pointer,b->size);}
    if(!rb_history_replay(scheduler,tick))return -1;
    if(!rb_probe_build_undo()){log_line("RB_VERIFY skipped sparse undo budget/overlap");rb_tx_commit();return 0;}
    SIZE_T undo_bytes=rb_last(&rb_native_undo)->used,undo_blocks=rb_last(&rb_native_undo)->count;
    if(rb_watch_enabled){SIZE_T watched=0,copied=0,scanned=0;unsigned regions=0;
        for(unsigned i=0;i<16;++i){RbWatchRegion *r=&rb_watch_regions[i];if(r->base && r->seeded){++regions;watched+=r->size;copied+=r->copied;scanned+=r->scanned;}}
        log_line("RB_WATCH snapshot serial=%lu regions=%u watched_bytes=%lu mirror_copied=%lu undo_scanned=%lu",(unsigned long)rb_probe.serial,regions,(unsigned long)watched,(unsigned long)copied,(unsigned long)scanned);
    }
    if(!rb_undo_to(&rb_native_undo,rb_audit.epoch)){rb_tx_commit();return -1;}
    if(!rb_graphics_rewind())return -1;
    rb_probe_reset_lifetimes();
    int trial=rb_probe_wrong_input(scheduler,tick,inputs);if(trial<0)return -1;
    if(!rb_graphics_rewind())return -1;
    rb_probe_reset_lifetimes();rb_tx.mode=2;rb_tx.cursor=0;
    rb_replaying=1;int result=0,results_match=1;
    for(unsigned step=0;step<rb_probe.steps;++step){if(step && !rb_graphics_draw()){rb_tx.failed=1;break;}result=tick(scheduler);if(result!=rb_step_result[step])results_match=0;if(rb_tx.failed || U32(0x4cd894)){rb_tx.failed=1;break;}}
    rb_replaying=0;
    rb_tx.mode=0;
    unsigned different=0;size_t first=0;void *first_owner=NULL;
    for(unsigned i=0;i<rb_probe.count;++i){RbProbeBlock *b=&rb_probe.blocks[i];const unsigned char *expected=rb_probe.after+b->offset,*actual=b->pointer;
        if(!rb_audit_readable(b->pointer,b->size)){++different;continue;}
        if(memcmp(expected,actual,b->size)){++different;size_t offset=0;while(offset<b->size && expected[offset]==actual[offset])++offset;
            if(different<=8)log_line("RB_VERIFY difference owner=%p size=%lu caller=%08lx offset=%lu expected=%02x actual=%02x",b->pointer,(unsigned long)b->size,(unsigned long)b->caller,(unsigned long)offset,expected[offset],actual[offset]);
            if(!first_owner){first_owner=b->pointer;first=offset;}}
    }
    for(unsigned i=0;i<rb_tx.count;++i){RbHeapEvent *e=&rb_tx.events[i];if(e->kind==1 && (!rb_audit_readable(e->pointer,e->size) || memcmp(rb_probe.after+e->after,e->pointer,e->size))){++different;if(!first_owner)first_owner=e->pointer;}}
    int pass=!different && !rb_tx.failed && results_match && rb_audit.count==rb_tx.after_count && rb_audit.frees==rb_tx.after_frees && rb_audit.writes==rb_probe.start_writes && rb_tx.cursor==rb_tx.count;
    unsigned replayed=rb_tx.cursor;
    if(pass && rb_probe.steps>1){
        if(!rb_probe_boundary_ok() || !rb_probe_build_undo() || !rb_undo_to(&rb_native_undo,rb_audit.epoch))return -1;
        if(!rb_graphics_rewind())return -1;
        rb_probe_reset_lifetimes();rb_tx.mode=2;rb_tx.cursor=0;rb_replaying=1;
        int admitted_result=tick(scheduler);rb_replaying=0;rb_tx.mode=0;
        pass=!rb_tx.failed && admitted_result==first_result && rb_tx.cursor==first_events && rb_audit.frees==first_frees && rb_probe_boundary_ok();
        for(unsigned i=0;i<rb_probe.count && pass;++i)if(rb_asset_hash(rb_probe.blocks[i].pointer,rb_probe.blocks[i].size)!=rb_first_hash[i]){log_line("RB_WINDOW first-step mismatch owner=%p size=%lu caller=%lx",rb_probe.blocks[i].pointer,(unsigned long)rb_probe.blocks[i].size,(unsigned long)rb_probe.blocks[i].caller);pass=0;}
        for(unsigned i=0;i<first_events && pass;++i)if(rb_tx.events[i].kind==1 && rb_asset_hash(rb_tx.events[i].pointer,rb_tx.events[i].size)!=rb_first_hash[rb_probe.count+i])pass=0;
        log_line("RB_WINDOW %s serial=%lu steps=%u corrected=%u admitted=1 first_events=%u total_events=%u",pass?"PASS":"FAIL",(unsigned long)rb_probe.serial,rb_probe.steps,trial>0,first_events,rb_tx.count);
    }
    rb_tx_commit();if(rb_tx.failed)pass=0;
    if(pass && rb_probe.steps>1)rb_audit.count=first_count;
    unsigned allocations=0,releases=0;for(unsigned i=0;i<rb_tx.count;++i){allocations+=rb_tx.events[i].kind==1;releases+=rb_tx.events[i].kind==2;}
    log_line("RB_VERIFY %s serial=%lu owners=%u bytes=%lu different=%u first_owner=%p offset=%lu first_result=%d final_result=%d step_results_match=%d heap_events=%u replayed=%u allocs=%u frees=%u undo_bytes=%lu undo_blocks=%lu",pass?"PASS":"FAIL",(unsigned long)rb_probe.serial,rb_probe.count,(unsigned long)total,different,first_owner,(unsigned long)first,first_result,result,results_match,rb_tx.count,replayed,allocations,releases,(unsigned long)undo_bytes,(unsigned long)undo_blocks);
    log_line("RB_AUDIO serial=%lu real_services=%u virtual_services=%u blocked_music=%u",(unsigned long)rb_probe.serial,rb_audio.real,rb_audio.virtualised,rb_audio.blocked);
    if(rb_graphics.active){log_line("RB_GRAPHICS %s serial=%lu intermediate_draws=%u presents=0 restored_first=1",pass?"PASS":"FAIL",(unsigned long)rb_probe.serial,rb_graphics.draws);rb_graphics_release();}
    return pass?1:-1;
}
static void rb_probe_end(void) {
    if(!rb_probe.active)return;rb_probe.active=0;
    if(U32(0x4cd894)){log_line("RB_AUDIT frame probe retired at scene worker boundary");return;}
    wchar_t path[MAX_PATH];GetModuleFileNameW(module,path,MAX_PATH);wchar_t *base=wcsrchr(path,L'\\');if(!base)return;
    swprintf(base+1,MAX_PATH-(base+1-path),L"rb-mutations-P%lu-%lu.csv",(unsigned long)online_map->role+1,(unsigned long)rb_probe.serial);
    FILE *file=_wfopen(path,L"wb");if(!file)return;
    fprintf(file,"pointer,size,caller,alive,changed_bytes,dirty_pages,first,last\n");
    swprintf(base+1,MAX_PATH-(base+1-path),L"rb-globals-P%lu-%lu.csv",(unsigned long)online_map->role+1,(unsigned long)rb_probe.serial);
    FILE *global_words=_wfopen(path,L"wb");if(global_words)fprintf(global_words,"address,before,after\n");
    unsigned mutated=0,freed=0;size_t dirty=0;
    EnterCriticalSection(&rb_audit.lock);
    for(unsigned i=0;i<rb_probe.count;++i){RbProbeBlock *b=&rb_probe.blocks[i];int alive=b->index==UINT32_MAX || (b->index<rb_audit.count && rb_audit.entries[b->index].live && rb_audit.entries[b->index].pointer==b->pointer);
        if(alive && !rb_audit_readable(b->pointer,b->size))alive=0;
        SIZE_T changed=0,first=0,last=0,pages=0;const unsigned char *before=rb_probe_saved(b),*after=b->pointer;
        if(alive)for(size_t page=0;page<b->size;page+=4096){SIZE_T length=b->size-page;if(length>4096)length=4096;if(!memcmp(before+page,after+page,length))continue;++pages;
            for(size_t n=page;n<page+length;++n)if(before[n]!=after[n]){if(!changed)first=n;last=n;++changed;}}
        else ++freed;
        if(changed){++mutated;dirty+=pages*4096;}
        if(global_words && b->pointer==(void *)0x4c3000 && alive)for(size_t n=0;n+4<=b->size;n+=4){uint32_t a=*(const uint32_t *)(before+n),z=*(const uint32_t *)(after+n);if(a!=z)fprintf(global_words,"%08lx,%08lx,%08lx\n",(unsigned long)((uintptr_t)b->pointer+n),(unsigned long)a,(unsigned long)z);}
        fprintf(file,"%08lx,%lu,%08lx,%d,%lu,%lu,%lu,%lu\n",(unsigned long)(uintptr_t)b->pointer,(unsigned long)b->size,(unsigned long)b->caller,alive,(unsigned long)changed,(unsigned long)pages,(unsigned long)first,(unsigned long)last);
    }
    LeaveCriticalSection(&rb_audit.lock);fclose(file);if(global_words)fclose(global_words);
    log_line("RB_AUDIT frame_probe serial=%lu phase=%ld owners=%u captured=%lu mutated=%u freed=%u dirty_page_bytes=%lu stage=%lu",(unsigned long)rb_probe.serial,(long)rb_audit.phase,rb_probe.count,(unsigned long)rb_probe.used,mutated,freed,(unsigned long)dirty,(unsigned long)U32(0x4cccdc));
}
static void rb_probe_finish_step(void) {
    if(rb_probe.active && rb_probe.seeking){
        int matching=rb_tx.count!=0;
        if((rb_probe.serial>=80 && rb_probe.serial<90) || (rb_probe.serial>=98 && rb_probe.serial<100) || (rb_probe.serial>=116 && rb_probe.serial<120) || rb_probe.serial>=136){matching=0;for(unsigned i=0;i<rb_tx.count;++i)if(rb_tx.events[i].kind==2)matching=1;}
        if(!matching && !rb_tx.failed){rb_probe.active=rb_probe.qualify=0;rb_tx_commit();if(!rb_probe.remaining)log_line("RB_VERIFY lifetime search expired serial=%lu",(unsigned long)rb_probe.serial);return;}
        rb_probe.remaining=0;
    }
    rb_probe_end();
}
static void rb_audit_dump(void) {
    if(!rb_audit.enabled)return;
    /* Reuse the bounded capture serial only as a test command. All seats write
       their own metadata; ordinary online messages cannot set this serial. */
    DWORD serial=online_map->reserved_tail[2];if(serial<40 || serial>99 || serial==rb_audit.serial)return;
    rb_audit.serial=serial;wchar_t path[MAX_PATH];GetModuleFileNameW(module,path,MAX_PATH);wchar_t *base=wcsrchr(path,L'\\');if(!base)return;
    swprintf(base+1,MAX_PATH-(base+1-path),L"rb-heap-P%lu-%lu.csv",(unsigned long)online_map->role+1,(unsigned long)serial);
    typedef struct {RbHeapEntry e;uint32_t hash;} Row;
    Row *rows=VirtualAlloc(NULL,65536*sizeof(Row),MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);if(!rows)return;
    unsigned live=0,walk=0,unknown=0,stale=0,count=0;size_t bytes=0;EnterCriticalSection(&rb_audit.lock);
    if(!HeapLock(rb_audit.heap)){LeaveCriticalSection(&rb_audit.lock);VirtualFree(rows,0,MEM_RELEASE);return;}
    count=rb_audit.count;
    for(unsigned i=0;i<count;++i){rows[i].e=rb_audit.entries[i];if(rows[i].e.live)rows[i].e.live=-1;}
    /* Only dereference addresses independently enumerated as busy by HeapWalk;
       foreign DLLs can free process-heap blocks without the EXE import hooks. */
    PROCESS_HEAP_ENTRY item={0};while(HeapWalk(rb_audit.heap,&item))if(item.wFlags&PROCESS_HEAP_ENTRY_BUSY){++walk;int found=0;
        for(unsigned i=count;i;--i){RbHeapEntry *e=&rb_audit.entries[i-1];if(!e->live || e->pointer!=item.lpData)continue;
            found=1;rows[i-1].e.live=1;++live;SIZE_T size=e->size<item.cbData?e->size:item.cbData;bytes+=size;uint32_t hash=2166136261u;
            for(size_t n=0;n<size;++n)hash=(hash^((unsigned char *)item.lpData)[n])*16777619u;rows[i-1].hash=hash;break;
        }if(!found)++unknown;
    }
    for(unsigned i=0;i<count;++i)if(rows[i].e.live==-1){RbWatchRegion *r=rb_watch_find(rows[i].e.pointer);
        if(r && rb_audit_readable(r->base,r->size)){rows[i].e.live=1;++live;bytes+=r->size;uint32_t hash=2166136261u;for(SIZE_T n=0;n<r->size;++n)hash=(hash^r->base[n])*16777619u;rows[i].hash=hash;}
        else ++stale;
    }
    HeapUnlock(rb_audit.heap);LeaveCriticalSection(&rb_audit.lock);
    FILE *file=_wfopen(path,L"wb");if(file){fprintf(file,"pointer,size,caller,thread,phase,born,freed,live,hash\n");
        for(unsigned i=0;i<count;++i){RbHeapEntry *e=&rows[i].e;fprintf(file,"%08lx,%lu,%08lx,%lu,%ld,%lu,%lu,%d,%08lx\n",(unsigned long)(uintptr_t)e->pointer,(unsigned long)e->size,(unsigned long)e->caller,(unsigned long)e->thread,(long)e->phase,(unsigned long)e->born,(unsigned long)e->freed,e->live,(unsigned long)rows[i].hash);}fclose(file);}
    VirtualFree(rows,0,MEM_RELEASE);
    log_line("RB_AUDIT inventory serial=%lu tracked=%u live=%u walk=%u unknown=%u stale=%u bytes=%lu dropped=%u stage=%lu",(unsigned long)serial,count,live,walk,unknown,stale,(unsigned long)bytes,rb_audit.dropped,(unsigned long)U32(0x4cccdc));
}
#endif
