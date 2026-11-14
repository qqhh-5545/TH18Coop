#ifndef TH18_NATIVE_ROLLBACK_RUNTIME_H
#define TH18_NATIVE_ROLLBACK_RUNTIME_H
#include "eagler_core.h"
#include "rollback_ipc.h"
typedef struct {int id,count,pan;} RbSoundAction;
typedef struct {unsigned count;RbSoundAction actions[16][12];unsigned lengths[16];} RbSoundFrame;
typedef struct {
    uint32_t frame,keys[3],digest;unsigned before_count,before_frees;
    unsigned char *live;RbSoundFrame sound;CoopInputLatch trace;
} RbNativeFrame;
static struct {
    void *core;uint32_t frame,frontier,latest_capture,sequence,published,precise;
    RbJournal journal;RbNativeFrame frames[8];RbSoundFrame *sound;
    struct {uint32_t frame,digest;} hashes[3][256];
    DWORD last_packet,last_progress;unsigned corrections,replayed,scenario;uint32_t scenario_next;int ready,replaying,pending_exact,pending_result;
} rb_live;
static struct {int init,enabled;LARGE_INTEGER frequency,last;double ms[5];unsigned frames;} rb_perf;
static void rb_perf_begin(void){
    if(!rb_perf.init){wchar_t value[4];rb_perf.init=1;rb_perf.enabled=(online_map->reserved[1]&1) && GetEnvironmentVariableW(L"TH18_COOP_RB_PERF",value,4)==1 && value[0]==L'1';if(rb_perf.enabled)QueryPerformanceFrequency(&rb_perf.frequency);}
    if(rb_perf.enabled)QueryPerformanceCounter(&rb_perf.last);
}
static void rb_perf_phase(unsigned phase){if(rb_perf.enabled){LARGE_INTEGER now;QueryPerformanceCounter(&now);rb_perf.ms[phase]+=(now.QuadPart-rb_perf.last.QuadPart)*1000.0/rb_perf.frequency.QuadPart;rb_perf.last=now;}}
static void rb_perf_report(void){if(rb_perf.enabled && ++rb_perf.frames>=120 && !rb_replaying){
    double n=rb_perf.frames;RbFrame *last=rb_last(&rb_live.journal);unsigned watched=0;for(unsigned i=0;i<16;++i)if(rb_watch_regions[i].base)++watched;
    log_line("ROLLBACK cost samples=%u capture_ms=%.3f tick_ms=%.3f draw_ms=%.3f journal_ms=%.3f digest_ms=%.3f blocks=%lu bytes=%lu watched=%u",rb_perf.frames,rb_perf.ms[0]/n,rb_perf.ms[1]/n,rb_perf.ms[2]/n,rb_perf.ms[3]/n,rb_perf.ms[4]/n,last?(unsigned long)last->count:0,last?(unsigned long)last->used:0,watched);rb_perf.frames=0;memset(rb_perf.ms,0,sizeof(rb_perf.ms));}}
static void rb_live_stop(unsigned code,const char *reason) {
    rb_runtime_virtual=rb_runtime_speculating=0;rb_tx.mode=0;
    if(rb_pipe){rb_pipe->error=code;InterlockedExchange(&rb_pipe->status,4);}
    log_line("ROLLBACK stopped frame=%lu error=%u reason=%s",(unsigned long)rb_live.frame,code,reason);online_fail(code);
}
static int rb_runtime_audio(void *manager) {
    (void)manager;
    if(U32(0x56d204)){rb_runtime_barrier=1;return 0;}
    if(!PTR(0x56ad8c) || !*(unsigned char *)0x4cd00a)return 0;
    RbSoundFrame *frame=rb_live.sound;
    if(!frame || frame->count==16){rb_tx.failed=1;return 0;}
    unsigned event=frame->count++,count=0;
    for(unsigned i=0;i<12;++i){int id=(int32_t)U32(0x56ad9c+i*4);if(id<0)break;
        int n=(int32_t)U32(0x56adcc+i*4);if(id>=96 || n>60){rb_tx.failed=1;return 0;}
        int64_t sum=0;for(int j=0;j<n;++j)sum+=(int32_t)U32(0x56adfc+i*512+j*4);
        if(sum<INT32_MIN || sum>INT32_MAX){rb_tx.failed=1;return 0;}
        frame->actions[event][count++]=(RbSoundAction){id,n<0?-1:n?1:0,n>0?(int)sum/n:0};
    }frame->lengths[event]=count;
    if(!rb_audio_drain((int32_t *)0x56ad9c,(int32_t *)0x56adcc,(int32_t *)0x56adfc,(RbAudioParameter *)0x56c804,rb_audio.status))rb_tx.failed=1;
    return 0;
}
static void rb_live_play_sound(const RbSoundFrame *sound) {
    /* Real output is committed after input confirmation. The game-visible
       queue/parameters stay at the reconciled virtual world, not at the
       later physical DirectSound clock. Music belongs to precise frames. */
    unsigned char queue[0x1860],parameters[96*24],music[0x2180];
    memcpy(queue,(void *)0x56ad9c,sizeof(queue));memcpy(parameters,(void *)0x56c804,sizeof(parameters));memcpy(music,(void *)0x56d204,sizeof(music));
    U32(0x56d204)=0;
    for(unsigned event=0;event<sound->count;++event){
        for(unsigned i=0;i<12;++i){U32(0x56ad9c+i*4)=UINT32_MAX;U32(0x56adcc+i*4)=0;}
        for(unsigned i=0;i<sound->lengths[event];++i){const RbSoundAction *a=&sound->actions[event][i];U32(0x56ad9c+i*4)=a->id;U32(0x56adcc+i*4)=a->count;U32(0x56adfc+i*512)=a->pan;}
        rb_audio.original((void *)0x56ad80);
    }
    memcpy((void *)0x56ad9c,queue,sizeof(queue));memcpy((void *)0x56c804,parameters,sizeof(parameters));memcpy((void *)0x56d204,music,sizeof(music));
}
static int rb_live_hash(unsigned seat,uint32_t frame,uint32_t digest) {
    unsigned slot=frame%256;rb_live.hashes[seat][slot].frame=frame;rb_live.hashes[seat][slot].digest=digest;
    unsigned own=online_map->role;
    if(rb_live.hashes[own][slot].frame!=frame)return 1;
    for(unsigned id=0;id<party_count();++id)if(rb_live.hashes[id][slot].frame==frame && rb_live.hashes[id][slot].digest!=rb_live.hashes[own][slot].digest){
        log_line("ROLLBACK digest mismatch frame=%lu P%u=%08lx P%u=%08lx",(unsigned long)frame,own+1,(unsigned long)rb_live.hashes[own][slot].digest,id+1,(unsigned long)rb_live.hashes[id][slot].digest);return 0;}
    return 1;
}
static int rb_live_publish(uint32_t frame,uint32_t digest) {
    if(!rb_live_hash(online_map->role,frame,digest))return 0;
    unsigned char packet[9];packet[0]=1;memcpy(packet+1,&frame,4);memcpy(packet+5,&digest,4);
    for(unsigned id=0;id<party_count();++id)if(id!=online_map->role && !rb_pipe_send(id,packet,sizeof(packet)))return 0;
    return 1;
}
static int rb_live_receive(void) {
    if(!rb_pipe || rb_pipe->status==4)return 0;
    unsigned read=(unsigned)rb_pipe->in_read,write=(unsigned)rb_pipe->in_write;if(write-read>RB_PIPE_SLOTS)return 0;
    while(read!=write){MemoryBarrier();RbPipePacket *packet=&rb_pipe->incoming[read%RB_PIPE_SLOTS];unsigned peer=packet->peer,size=packet->size;
        if(peer>=party_count() || peer==online_map->role || !size || size>RB_PIPE_PAYLOAD)return 0;
        if(packet->data[0]==0){if(!ec_receive(rb_live.core,peer,packet->data+1,size-1))return 0;}
        else if(packet->data[0]==1 && size==9){uint32_t frame,digest;memcpy(&frame,packet->data+1,4);memcpy(&digest,packet->data+5,4);if(!rb_live_hash(peer,frame,digest))return 0;}
        else return 0;
        InterlockedExchange(&rb_pipe->in_read,(LONG)++read);rb_live.last_progress=GetTickCount();
    }return 1;
}
static void rb_live_gc(uint32_t confirmed,int orphans) {
    DWORD error=GetLastError();EnterCriticalSection(&rb_audit.lock);
    for(unsigned i=0;i<rb_audit.count;++i){RbHeapEntry *e=&rb_audit.entries[i];
        if(e->physical && e->held && !e->live && ((orphans && !e->freed) || (confirmed!=UINT32_MAX && e->freed && e->freed-1<=confirmed))){
            if(!rb_audit_physical_release(e->pointer,0))rb_tx.failed=1;e->held=0;}}
    LeaveCriticalSection(&rb_audit.lock);SetLastError(error);
}
static void rb_live_compact_registry(void) {
    if(rb_live.journal.count || rb_tx.mode || rb_audit.count<4096)return;
    EnterCriticalSection(&rb_audit.lock);unsigned old=rb_audit.count,kept=0;
    for(unsigned i=0;i<old;++i)if(rb_audit.entries[i].physical)rb_audit.entries[kept++]=rb_audit.entries[i];
    rb_audit.count=kept;rb_tx.count=rb_tx.cursor=0;LeaveCriticalSection(&rb_audit.lock);
    if(kept!=old)log_line("ROLLBACK retired heap metadata %u->%u with no retained history",old,kept);
}
static int rb_live_confirm(void) {
    uint32_t confirmed=ec_confirmed(rb_live.core);if(confirmed==UINT32_MAX)return 1;
    if(confirmed>=rb_live.frame)confirmed=rb_live.frame?rb_live.frame-1:UINT32_MAX;
    if(confirmed==UINT32_MAX)return 1;
    while(rb_live.journal.count){RbFrame *journal=&rb_live.journal.frames[rb_live.journal.start];if(journal->frame>confirmed)break;
        RbNativeFrame *f=&rb_live.frames[journal->frame%8];if(f->frame!=journal->frame)return 0;
        rb_live_play_sound(&f->sound);
        CoopInputLatch current=input_latch;input_latch=f->trace;trace_input_record();input_latch=current;
        if(!rb_live_publish(f->frame,f->digest) || !rb_discard_before(&rb_live.journal,f->frame+1))return 0;
    }
    rb_live_gc(confirmed,0);rb_live_compact_registry();rb_pipe->confirmed=confirmed;return !rb_tx.failed;
}
static int rb_live_stable(void) {
    if(!rb_present_eligible() || U32(0x4cd5e4)!=U32(0x4cd5e8))return 0;
    void *music=PTR(0x570484);if(music && FIELD(music,0x20))return 0;
    void *manager=PTR(0x51f65c);if(!manager)return 0;
    for(unsigned i=0;i<33;++i){void *file=(void *)(uintptr_t)FIELD(manager,0x312072c+i*4);if(file && (FIELD(file,0x128) || FIELD(file,0x12c)))return 0;}
    return 1;
}
static void rb_live_keys(const uint32_t keys[3]) {
    memcpy(online_keys,keys,sizeof(online_keys));
    for(unsigned id=1;id<party_count();++id){online_keys[0]|=keys[id]&0x100u;online_keys[id]=(keys[id]&0xcfbu)|((keys[id]&0x80000u)?1u:0u);}
}
static int rb_live_capture(RbNativeFrame *f,uint32_t frame,const uint32_t keys[3]) {
    if(!rb_capture_world(0)){log_line("ROLLBACK capture failed owners=%u bytes=%lu overflow=%d",rb_probe.count,(unsigned long)rb_probe.used,rb_probe.overflow);return 0;}rb_probe.active=0;
    rb_probe_add(&online_previous,sizeof(online_previous),UINT32_MAX,0);rb_probe_add(&online_previous_p2,sizeof(online_previous_p2),UINT32_MAX,0);rb_probe_add(&online_previous_p3,sizeof(online_previous_p3),UINT32_MAX,0);
    if(!f->live)f->live=VirtualAlloc(NULL,65536,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);if(!f->live)return 0;
    f->frame=frame;f->before_count=rb_audit.count;f->before_frees=rb_audit.frees;memcpy(f->keys,keys,sizeof(f->keys));memset(&f->sound,0,sizeof(f->sound));
    for(unsigned i=0;i<rb_audit.count;++i)f->live[i]=(unsigned char)(rb_audit_world_entry(&rb_audit.entries[i]) && rb_audit.entries[i].live);
    rb_probe.qualify=1;rb_tx_begin();rb_probe.qualify=0;return rb_tx.mode==1;
}
static int rb_live_seal(uint32_t frame) {
    if(!rb_begin(&rb_live.journal,frame))return 0;
    for(unsigned i=0;i<rb_probe.count;++i){RbProbeBlock *b=&rb_probe.blocks[i];RbWatchRegion *watch=rb_watch_find(b->pointer);
        if(!(watch?rb_watch_store_undo(watch,&rb_live.journal):rb_history_delta(&rb_live.journal,b->pointer,b->size,rb_probe_saved(b)))){RbFrame *last=rb_last(&rb_live.journal);log_line("ROLLBACK journal failed frame=%lu owner=%p size=%lu watch=%d stored=%lu blocks=%lu failed=%d",(unsigned long)frame,b->pointer,(unsigned long)b->size,!!watch,last?(unsigned long)last->used:0,last?(unsigned long)last->count:0,rb_live.journal.failed);return 0;}}
    return rb_end(&rb_live.journal);
}
static int rb_live_undo(uint32_t target,int core) {
    RbNativeFrame *f=&rb_live.frames[target%8];if(f->frame!=target || !rb_undo_to(&rb_live.journal,target))return 0;
    for(unsigned i=0;i<rb_audit.count;++i){RbHeapEntry *e=&rb_audit.entries[i];
        if(!rb_audit_world_entry(e))continue; /* External resource lifetimes never rewind. */
        if(i<f->before_count){if(f->live[i] && !e->physical)return 0;e->live=f->live[i];}else{e->live=0;e->held=e->physical;e->freed=0;}}
    rb_audit.frees=f->before_frees;rb_live.frame=target;
    return (!core || ec_rewind(rb_live.core,target)) && rb_graphics_rewind();
}
static int rb_runtime_reset_boundary(void) {
    if(!rb_runtime_enabled || !rb_live.ready)return 1;
    if(rb_tx.mode){rb_live_stop(41,"renderer reset inside an open transaction");return 0;}
    if(rb_live.journal.count){uint32_t first=(uint32_t)rb_live.journal.frames[rb_live.journal.start].frame;
        if(!rb_live_undo(first,1)){rb_live_stop(41,"renderer reset checkpoint");return 0;}
        rb_live_gc(UINT32_MAX,1);}
    rb_live.precise=rb_live.frame;return !rb_tx.failed;
}
static int rb_live_render(void) {
    if(rb_control_active && (!p1 || !ship(1) || U32(0x4cd894))){rb_control_active=0;rb_graphics_rewind();rb_graphics_release();}
    if(p1 && ship(1) && (party_count()==2 || ship(2)) && !scope_depth && !U32(0x4cd894)){
        if(!rb_control_active){if(!rb_graphics_begin())return 0;rb_control_active=1;}
        return rb_graphics_draw() && rb_graphics_leave_output();}
    return 1;
}
#include "native_rollback_oracle.h"
#include "native_rollback_scenarios.h"
static int rb_live_finish_frame(uint32_t frame,int result,int exact){
    rb_audit.phase=0;rb_scenario_after(frame,exact);
    if(!ec_mark(rb_live.core,frame))return -1;
    ++rb_live.frame;online_step=rb_live.frame+1;online_map->completed=online_step;rb_pipe->frame=rb_live.frame;
    if(exact && !rb_live_publish(frame,online_digest()))return -1;
    if(rb_live.replaying)++rb_live.replayed;
    return result<0?-1:result==0?2:1;
}
static int rb_live_run_frame(void *scheduler,uint32_t frame,const uint32_t keys[3],unsigned predicted,int exact) {
    if(!exact)rb_perf_begin();
    if(!exact && rb_live.journal.count==8)return 0;
    rb_audit.main_thread=GetCurrentThreadId();rb_audit.phase=1;rb_audit.epoch=frame+1;rb_runtime_barrier=0;
    if(exact && !rb_scenario_before(frame))return -1;
    U32(0x4cd624)&=~0x180u;
    RbNativeFrame *f=&rb_live.frames[frame%8];
    if(!exact && !rb_live_capture(f,frame,keys))return -1;
    if(!exact)rb_perf_phase(0);
    rb_live_keys(keys);U32(0x4cd624)&=~0x180u;
    rb_runtime_speculating=!exact;rb_runtime_virtual=!exact;rb_live.sound=exact?NULL:&f->sound;
    int result=((Tick)online_tick_original)(scheduler);
    if(!exact)rb_perf_phase(1);
    if(!exact){
        /* Commands requesting audio/scene/resource work were stopped at their
           safe request sites. Undo the tentative frame before any Draw. */
        if(U32(0x56d204) || rb_runtime_barrier){
            rb_runtime_speculating=rb_runtime_virtual=0;rb_live.sound=NULL;
            rb_tx_seal();if(!rb_live_seal(frame) || !rb_live_undo(frame,0))return -1;
            rb_live.precise=frame;rb_live_gc(UINT32_MAX,1);return 0;
        }
        if(rb_tx.failed || U32(0x4cd894) || !rb_live_render()){rb_runtime_speculating=rb_runtime_virtual=0;rb_live.sound=NULL;log_line("ROLLBACK execution failed frame=%lu tx=%d events=%u worker=%lu graphics=%d",(unsigned long)frame,rb_tx.failed,rb_tx.count,(unsigned long)U32(0x4cd894),rb_graphics.failed);rb_tx_seal();return -1;}
        rb_runtime_speculating=rb_runtime_virtual=0;rb_live.sound=NULL;
        rb_perf_phase(2);
        if(U32(0x56d204) || rb_runtime_barrier){rb_tx_seal();if(!rb_live_seal(frame) || !rb_live_undo(frame,0))return -1;rb_live.precise=frame;rb_live_gc(UINT32_MAX,1);return 0;}
        rb_tx_seal();if(rb_tx.failed || !rb_live_seal(frame))return -1;
        rb_perf_phase(3);
        if(!rb_live_oracle(scheduler,f,result))return -1;
        f->trace=input_latch;f->digest=online_digest();
        rb_perf_phase(4);rb_perf_report();
    }else{rb_runtime_speculating=rb_runtime_virtual=0;rb_live.sound=NULL;
        /* A native scene worker may start inside this accurate Tick. Do not
           publish a partial world or execute that Tick twice while loading. */
        if(result>0 && (!online_load_ready() || !online_assets_ready())){rb_live.pending_exact=1;rb_live.pending_result=result;return 3;}
        if(!rb_live_render())return -1;}
    (void)predicted;return rb_live_finish_frame(frame,result,exact);
}
static uint32_t rb_live_sample(void) {
    unsigned char saved[0x234];memcpy(saved,(void *)0x4ca210,sizeof(saved));
    online_sampling=1;online_raw=0;((void (__cdecl *)(void))0x401c50)();online_sampling=0;memcpy((void *)0x4ca210,saved,sizeof(saved));
#ifndef COOP_NATIVE_FIXTURE
    if((online_map->reserved[1]&3)==1)online_raw=online_duo_keys();else if(GetForegroundWindow()!=(HWND)(uintptr_t)U32(0x568c30))online_raw=0;
#endif
    if((online_map->reserved[1]&1) && online_map->reserved_tail[1]==0x54455354u)online_raw=online_map->reserved_tail[0]&0x80dfbu;
    return online_raw&0x80dfbu;
}
static int rb_live_send_inputs(void) {
    for(unsigned id=0;id<party_count();++id)if(id!=online_map->role){unsigned char bytes[513];size_t size=0;bytes[0]=0;
        if(!ec_encode(rb_live.core,id,rb_live.latest_capture+2,++rb_live.sequence,bytes+1,512,&size) || !rb_pipe_send(id,bytes,(unsigned)size+1))return 0;}
    rb_live.last_packet=GetTickCount();return 1;
}
static int rb_live_initialise(void) {
    rb_runtime_thread=GetCurrentThreadId();
    /* Bootstrap is complete and no scene worker is active. Install on this
       thread, not concurrently with the startup resource loader. */
    if(!rb_assets_install())return 0;
    uint64_t session=(uint64_t)rb_pipe->session[0]|((uint64_t)rb_pipe->session[1]<<32);
    rb_live.core=ec_create(party_count(),online_map->role,2,8,session);if(!rb_live.core || !rb_init(&rb_live.journal,8,8u*1024u*1024u,8192))return 0;
    rb_live.frontier=rb_live.latest_capture=rb_live.published=rb_live.precise=UINT32_MAX;
    for(unsigned s=0;s<3;++s)for(unsigned i=0;i<256;++i)rb_live.hashes[s][i].frame=UINT32_MAX;
    if(!rb_audio_checkpoint())memset(rb_audio.status,0,sizeof(rb_audio.status));
    rb_live.last_progress=GetTickCount();rb_live.ready=1;InterlockedExchange(&rb_pipe->status,1);
    if((online_map->reserved[1]&1) && rb_pipe->flags==1)rb_live.scenario=1;
    log_line("ROLLBACK native driver ready players=%u delay=2 window=8 upstream_core=8ee5ac9",party_count());return 1;
}
static int rb_live_step(void *scheduler) {
    if(!rb_live.ready && !rb_live_initialise()){rb_live_stop(31,"initialisation");return 1;}
    if(!rb_live_receive()){rb_live_stop(32,"input packet or confirmed digest");return 1;}
    if(GetTickCount()-rb_live.last_progress>90000){rb_live_stop(39,"peer timeout");return 1;}
    /* Reserve enough output slots for retiring the whole8-frame history to
       both peers, the next input bundle and the next confirmed digest. */
    if((unsigned)rb_pipe->out_write-(unsigned)rb_pipe->out_read>RB_PIPE_SLOTS-24){++rb_pipe->waits;return 1;}
    if(rb_live.pending_exact){
        if(!online_load_ready() || !online_assets_ready())return 1;
        if(!rb_live_render()){rb_live_stop(38,"loaded native Draw");return 1;}
        int result=rb_live_finish_frame(rb_live.frame,rb_live.pending_result,1);rb_live.pending_exact=0;
        if(result<0)rb_live_stop(38,"loaded native frame confirmation");return result==2?0:1;
    }
    uint32_t target=ec_rollback(rb_live.core);
    if(target!=UINT32_MAX){uint32_t frontier=rb_live.frame;
        if(!rb_live_undo(target,1)){rb_live_stop(33,"missing native history");return 1;}
        ++rb_live.corrections;rb_live.replaying=1;rb_replaying=1;
        while(rb_live.frame<frontier){uint32_t keys[3];unsigned predicted;
            if(!ec_prepare(rb_live.core,rb_live.frame,keys,&predicted))break;
            if(!rb_live_stable() || ((keys[0]|keys[1]|keys[2])&0x80f02u)){rb_live.precise=rb_live.frame;break;}
            int result=rb_live_run_frame(scheduler,rb_live.frame,keys,predicted,0);
            if(result<0){rb_replaying=rb_live.replaying=0;rb_live_stop(34,"corrected native step");return 1;}
            if(!result)break;
        }
        rb_replaying=rb_live.replaying=0;rb_live_gc(UINT32_MAX,1);
        log_line("ROLLBACK corrected from=%lu frontier=%lu reached=%lu count=%u",(unsigned long)target,(unsigned long)frontier,(unsigned long)rb_live.frame,rb_live.corrections);
    }
    if(!rb_live_confirm()){rb_live_stop(35,"confirmation output");return 1;}
    if(!ec_has_local(rb_live.core,rb_live.frame)){
        if(!ec_local(rb_live.core,rb_live.frame,rb_live_sample())){rb_live_stop(36,"input capture");return 1;}
        rb_live.latest_capture=rb_live.frame;
        if(!rb_live_send_inputs()){rb_live_stop(37,"outbound packet");return 1;}
    }else if(GetTickCount()-rb_live.last_packet>=25 && !rb_live_send_inputs()){rb_live_stop(37,"input retry");return 1;}
    uint32_t keys[3];unsigned predicted;
    if(!ec_prepare(rb_live.core,rb_live.frame,keys,&predicted)){++rb_pipe->waits;return 1;}
    uint32_t confirmed=ec_confirmed(rb_live.core);
    int actual=confirmed!=UINT32_MAX && confirmed>=rb_live.frame;
    int stable=rb_live_stable() && rb_live.precise!=rb_live.frame;
    if(rb_scenario_boundary(rb_live.frame))stable=0;
    if((keys[0]|keys[1]|keys[2])&0x80f02u)stable=0;
    if(!stable && !actual){++rb_pipe->waits;return 1;}
    if(!stable && (!online_load_ready() || !online_assets_ready()))return 1;
    int exact=!rb_live.journal.count && actual;
    if(!stable && !exact){++rb_pipe->waits;return 1;}
    int result=rb_live_run_frame(scheduler,rb_live.frame,keys,predicted,exact);
    if(result<0){rb_live_stop(38,"native frame capture, execution or history");return 1;}
    if(!rb_live_confirm()){rb_live_stop(35,"confirmation output");return 1;}
    rb_pipe->corrections=rb_live.corrections;rb_pipe->replayed=rb_live.replayed;
    if(GetTickCount()-rb_live.last_progress>90000)rb_live_stop(39,"peer timeout");
    return result==2?0:1;
}
static void rb_live_teardown(void *scheduler) {
    if(!rb_runtime_enabled || !rb_live.ready)return;
    uint32_t destroying=FIELD(scheduler,0x54);int clean=1;
    if(rb_live.journal.count){uint32_t first=(uint32_t)rb_live.journal.frames[rb_live.journal.start].frame;
        clean=rb_live_undo(first,1);FIELD(scheduler,0x54)=destroying;
        if(clean)rb_live_gc(ec_confirmed(rb_live.core),1);
        log_line("ROLLBACK shutdown reconciled=%d discarded_from=%lu",clean,(unsigned long)first);}
    rb_runtime_virtual=rb_runtime_speculating=rb_replaying=0;rb_tx.mode=0;
    rb_control_active=0;rb_graphics_rewind();rb_graphics_release();rb_graphics_unhook_reset();
    if(rb_pipe){if(!clean){rb_pipe->error=40;InterlockedExchange(&rb_pipe->status,4);}else if(rb_pipe->status!=4)InterlockedExchange(&rb_pipe->status,2);}
}
#endif
