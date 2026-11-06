#ifndef TH18_NATIVE_PRESENT_HISTORY_H
#define TH18_NATIVE_PRESENT_HISTORY_H
/* Isolated qualification of history that survives actual application Presents.
   Each admitted step owns both native Tick and native Draw. The application
   only copies the completed texture while waiting for the next admission. */
typedef struct {
    RbHeapTransaction tx;uint32_t keys[3],rng[4],audio_status[96];
    unsigned audio_recorded;unsigned char audio_stops[16][96];int result;
} RbPresentedFrame;
static struct {
    RbJournal journal;RbPresentedFrame frame[8];unsigned count,serial;
    unsigned char *end;SIZE_T bytes;int recording;DWORD stage;void *player;
} rb_present;
static int rb_present_eligible(void) {
    void *pause=PTR(0x4cf40c);
    return rb_control_active && p1 && ship(1) && (party_count()==2 || ship(2)) &&
        !scope_depth && !U32(0x4cd894) && !PTR(0x4cf2a4) && !PTR(0x4cf2cc) &&
        !(pause && FIELD(pause,0x1ec)) && !U32(0x56d204);
}
static void rb_present_cancel(const char *reason) {
    rb_present.recording=0;rb_probe.active=rb_probe.qualify=0;rb_tx_commit();
    rb_destroy(&rb_present.journal);rb_present.count=0;
    log_line("RB_PRESENT aborted serial=%u reason=%s",rb_present.serial,reason);
}
static int rb_present_begin(void) {
    DWORD serial=online_map->reserved_tail[2];
    if(!rb_audit.enabled || !rb_control_requested || serial<150 || serial>159)return 0;
    if(!rb_present.count && serial==rb_present.serial)return 0;
    if(!rb_present_eligible()){if(rb_present.count)rb_present_cancel("precise scene boundary");return 0;}
    if(!rb_present.count){
        rb_present.serial=serial;rb_present.stage=U32(0x4cccdc);rb_present.player=p1;rb_present.bytes=0;
        rb_destroy(&rb_present.journal);
        if(!rb_init(&rb_present.journal,8,8u*1024u*1024u,8192)){rb_present_cancel("journal allocation");return -1;}
    }
    if(!rb_audio_checkpoint() || !rb_capture_world(0)){rb_present_cancel("capture boundary");return -1;}
    rb_probe_add(&online_previous,sizeof(online_previous),UINT32_MAX,0);
    rb_probe_add(&online_previous_p2,sizeof(online_previous_p2),UINT32_MAX,0);
    rb_probe_add(&online_previous_p3,sizeof(online_previous_p3),UINT32_MAX,0);
    RbPresentedFrame *f=&rb_present.frame[rb_present.count];
    memcpy(f->keys,online_keys,sizeof(f->keys));memcpy(f->rng,(void *)0x4cf280,16);
    memcpy(f->audio_status,rb_audio.status,sizeof(f->audio_status));
    rb_probe.active=0;rb_probe.qualify=1;rb_tx_begin();rb_probe.qualify=0;
    if(rb_tx.mode!=1){rb_present_cancel("transaction allocation");return -1;}
    rb_present.recording=1;return 1;
}
static int rb_present_replay(void *scheduler,Tick tick) {
    /* The final capture covers all surviving pre-step owners plus quarantined
       deaths; explicitly append births from the final step to the oracle. */
    SIZE_T used=rb_probe.used;
    if(!rb_present.end)rb_present.end=VirtualAlloc(NULL,128u*1024u*1024u,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
    if(!rb_present.end)return 0;
    for(unsigned i=0;i<rb_probe.count;++i){RbProbeBlock *b=&rb_probe.blocks[i];memcpy(rb_present.end+b->offset,b->pointer,b->size);}
    RbPresentedFrame *last=&rb_present.frame[7];
    for(unsigned i=0;i<last->tx.count;++i){RbHeapEvent *e=&last->tx.events[i];if(e->kind!=1)continue;
        if(e->size>128u*1024u*1024u-used)return 0;e->after=used;memcpy(rb_present.end+used,e->pointer,e->size);used+=e->size;}
    uint32_t current_keys[3];memcpy(current_keys,online_keys,sizeof(current_keys));
    if(!rb_undo_to(&rb_present.journal,0) || !rb_graphics_rewind())return 0;
    RbPresentedFrame *first=&rb_present.frame[0];
    for(unsigned i=0;i<rb_audit.count;++i){RbHeapEntry *e=&rb_audit.entries[i];e->live=i<first->tx.before_count?first->tx.live[i]:0;if(i>=first->tx.before_count)e->held=1;}
    rb_audit.frees=first->tx.before_frees;rb_replaying=1;int pass=1;unsigned events=0;
    for(unsigned step=0;step<8;++step){RbPresentedFrame *f=&rb_present.frame[step];rb_tx=f->tx;rb_tx.mode=2;rb_tx.cursor=0;
        memcpy(online_keys,f->keys,sizeof(f->keys));memcpy((void *)0x4cf280,f->rng,16);
        memcpy(rb_audio.status,f->audio_status,sizeof(f->audio_status));rb_audio.cursor=0;rb_audio.recorded=f->audio_recorded;memcpy(rb_audio.stop_status,f->audio_stops,sizeof(f->audio_stops));
        int result=tick(scheduler);
        if(result!=f->result || rb_tx.failed || !rb_graphics_draw() || !rb_graphics_leave_output() || rb_tx.cursor!=rb_tx.count || rb_audit.frees!=f->tx.after_frees){
            log_line("RB_PRESENT step mismatch step=%u result=%d/%d failed=%d events=%u/%u frees=%u/%u",step,result,f->result,rb_tx.failed,rb_tx.cursor,rb_tx.count,rb_audit.frees,f->tx.after_frees);pass=0;break;}
        events+=rb_tx.cursor;
    }
    rb_replaying=0;rb_tx.mode=0;memcpy(online_keys,current_keys,sizeof(current_keys));
    unsigned different=0;
    for(unsigned i=0;i<rb_probe.count;++i){RbProbeBlock *b=&rb_probe.blocks[i];const unsigned char *expected=rb_present.end+b->offset,*now=b->pointer;
        SIZE_T offset=0;
        if(b->pointer==(void *)0x4c3000){for(;offset+4<=b->size;offset+=4)if(rb_probe_global_owned((uintptr_t)b->pointer+offset) && memcmp(expected+offset,now+offset,4))break;}
        else{while(offset<b->size && expected[offset]==now[offset])++offset;}
        if(offset<b->size){++different;if(different<=8)log_line("RB_PRESENT difference owner=%p size=%lu caller=%lx offset=%lu expected=%02x actual=%02x",b->pointer,(unsigned long)b->size,(unsigned long)b->caller,(unsigned long)offset,expected[offset],now[offset]);}}
    for(unsigned i=0;i<last->tx.count;++i){RbHeapEvent *e=&last->tx.events[i];if(e->kind==1 && memcmp(rb_present.end+e->after,e->pointer,e->size))++different;}
    if(different || rb_audit.writes!=first->tx.before_writes)pass=0;
    log_line("RB_PRESENT %s serial=%u actual_frames=8 rewind_to=0 replayed=8 different=%u events=%u undo_bytes=%lu presents_during_replay=0",pass?"PASS":"FAIL",rb_present.serial,different,events,(unsigned long)rb_present.bytes);
    return pass;
}
static int rb_present_end(void *scheduler,Tick tick,int result) {
    if(!rb_present.recording)return 1;rb_present.recording=0;
    if(rb_tx.failed || !rb_present_eligible() || p1!=rb_present.player || U32(0x4cccdc)!=rb_present.stage || rb_tx.after_writes!=rb_tx.before_writes){rb_present_cancel("native lifetime or output boundary");return 1;}
    if(!rb_begin(&rb_present.journal,rb_present.count)){rb_present_cancel("history begin");return 0;}
    for(unsigned i=0;i<rb_probe.count;++i){RbProbeBlock *b=&rb_probe.blocks[i];RbWatchRegion *watch=rb_watch_find(b->pointer);
        if(!(watch?rb_watch_store_undo(watch,&rb_present.journal):rb_history_delta(&rb_present.journal,b->pointer,b->size,rb_probe_saved(b)))){rb_present_cancel("history budget");return 0;}}
    if(!rb_end(&rb_present.journal)){rb_present_cancel("history seal");return 0;}
    rb_present.bytes+=rb_last(&rb_present.journal)->used;
    RbPresentedFrame *f=&rb_present.frame[rb_present.count];
    unsigned char *initial=f->tx.initial,*live=f->tx.live;
    if(!initial)initial=VirtualAlloc(NULL,16u*1024u*1024u,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
    if(!live)live=VirtualAlloc(NULL,65536,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
    if(!initial || !live){rb_present_cancel("frame allocation");return 0;}
    memcpy(initial,rb_tx.initial,rb_tx.used);memcpy(live,rb_tx.live,rb_tx.before_count);
    f->tx=rb_tx;f->tx.initial=initial;f->tx.live=live;f->result=result;
    f->audio_recorded=rb_audio.recorded;memcpy(f->audio_stops,rb_audio.stop_status,sizeof(f->audio_stops));
    ++rb_present.count;
    if(rb_present.count<8)return 1;
    unsigned char *scratch_initial=rb_tx.initial,*scratch_live=rb_tx.live;
    int pass=rb_present_replay(scheduler,tick);
    rb_tx.initial=scratch_initial;rb_tx.live=scratch_live;rb_tx_commit();rb_destroy(&rb_present.journal);rb_present.count=0;
    return pass;
}
#endif
