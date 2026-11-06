#ifndef TH18_NATIVE_STEP_HISTORY_H
#define TH18_NATIVE_STEP_HISTORY_H
/* Explicit serial115/119 only: qualify real native per-step history, including a
   partial rewind retaining the older four checkpoints. No Draw occurs in
   this logical window; this is not production cross-presentation rollback. */
static struct {
    RbJournal journal;RbProbeBlock *blocks;unsigned char *before,*live;
    unsigned count,events,owners,frees;SIZE_T used,total;
} rb_history;
static int rb_history_enabled(void){return rb_probe.serial==115 || rb_probe.serial==119;}
static int rb_history_delta(RbJournal *journal,void *pointer,SIZE_T size,const unsigned char *saved) {
    const unsigned char *now=pointer;
    if(pointer==(void *)0x4c3000){
        for(unsigned i=0;i<sizeof(rb_global_spans)/sizeof(rb_global_spans[0]);++i){const RbGlobalSpan *span=&rb_global_spans[i];if(!rb_global_span_active(span))continue;
            uintptr_t begin=span->begin>(uintptr_t)pointer?span->begin:(uintptr_t)pointer,end=span->end<(uintptr_t)pointer+size?span->end:(uintptr_t)pointer+size;
            uintptr_t run=0;
            for(uintptr_t at=begin;at+4<=end;at+=4){SIZE_T n=at-(uintptr_t)pointer;if(memcmp(saved+n,now+n,4)){if(!run)run=at;}
                else if(run){if(!rb_store_before(journal,(void *)run,saved+(run-(uintptr_t)pointer),at-run))return 0;run=0;}}
            if(run && !rb_store_before(journal,(void *)run,saved+(run-(uintptr_t)pointer),end-run))return 0;}
    }else{SIZE_T begin=0,end=0;
        for(SIZE_T n=0;n<size;n+=4096){SIZE_T length=size-n;if(length>4096)length=4096;
            if(memcmp(saved+n,now+n,length)){if(!end)begin=n;end=n+length;}
            else if(end){if(!rb_store_before(journal,(unsigned char *)pointer+begin,saved+begin,end-begin))return 0;begin=end=0;}}
        if(end && !rb_store_before(journal,(unsigned char *)pointer+begin,saved+begin,end-begin))return 0;}
    return 1;
}
static int rb_history_begin(void) {
    if(!rb_history_enabled())return 1;
    rb_destroy(&rb_history.journal);rb_history.total=0;
    if(!rb_init(&rb_history.journal,8,4u*1024u*1024u,8192))return 0;
    if(!rb_history.before)rb_history.before=VirtualAlloc(NULL,128u*1024u*1024u,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
    if(!rb_history.blocks)rb_history.blocks=VirtualAlloc(NULL,(65536+4096)*sizeof(RbProbeBlock),MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
    if(!rb_history.live)rb_history.live=VirtualAlloc(NULL,65536,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
    if(!rb_history.before || !rb_history.blocks || !rb_history.live || !rb_begin(&rb_history.journal,0))return 0;
    for(unsigned i=0;i<rb_probe.count;++i){RbProbeBlock *b=&rb_probe.blocks[i];if(!rb_history_delta(&rb_history.journal,b->pointer,b->size,rb_probe_saved(b)))return 0;}
    if(!rb_end(&rb_history.journal))return 0;rb_history.total+=rb_last(&rb_history.journal)->used;return 1;
}
static int rb_history_add(void *pointer,SIZE_T size) {
    if(rb_history.count==65536+4096 || size>128u*1024u*1024u-rb_history.used || !rb_audit_readable(pointer,size))return 0;
    RbProbeBlock *b=&rb_history.blocks[rb_history.count++];*b=(RbProbeBlock){pointer,size,rb_history.used,0,0};
    memcpy(rb_history.before+rb_history.used,pointer,size);rb_history.used+=size;return 1;
}
static int rb_history_before(unsigned step) {
    if(!rb_history_enabled())return 1;rb_history.count=0;rb_history.used=0;
    if(step==4 && rb_tx.mode==1){rb_history.events=rb_tx.count;rb_history.owners=rb_audit.count;rb_history.frees=rb_audit.frees;
        for(unsigned i=0;i<rb_audit.count;++i)rb_history.live[i]=(unsigned char)rb_audit.entries[i].live;}
    for(unsigned i=0;i<rb_probe.count;++i)if(!rb_history_add(rb_probe.blocks[i].pointer,rb_probe.blocks[i].size))return 0;
    /* Births after the initial snapshot become owners of later checkpoints,
       including objects subsequently destroyed but still quarantined. */
    for(unsigned i=0;i<rb_tx.count;++i)if(rb_tx.events[i].kind==1 && !rb_history_add(rb_tx.events[i].pointer,rb_tx.events[i].size))return 0;
    return rb_begin(&rb_history.journal,step);
}
static int rb_history_after(void) {
    if(!rb_history_enabled())return 1;
    for(unsigned i=0;i<rb_history.count;++i){RbProbeBlock *b=&rb_history.blocks[i];if(!rb_history_delta(&rb_history.journal,b->pointer,b->size,rb_history.before+b->offset))return 0;}
    if(!rb_end(&rb_history.journal))return 0;rb_history.total+=rb_last(&rb_history.journal)->used;return 1;
}
static int rb_history_replay(void *scheduler,Tick tick) {
    if(!rb_history_enabled())return 1;
    if(rb_history.journal.count!=8 || !rb_undo_to(&rb_history.journal,4) || rb_history.journal.count!=4)return 0;
    for(unsigned i=0;i<rb_audit.count;++i){rb_audit.entries[i].live=i<rb_history.owners?rb_history.live[i]:0;if(i>=rb_history.owners)rb_audit.entries[i].held=1;}
    rb_audit.frees=rb_history.frees;rb_tx.mode=2;rb_tx.cursor=rb_history.events;rb_replaying=1;
    int pass=1;
    for(unsigned step=4;step<8;++step){
        if(!rb_history_before(step)){pass=0;break;}
        int result=tick(scheduler);
        if(result!=rb_step_result[step] || !rb_probe_boundary_ok() || !rb_history_after()){pass=0;break;}
    }
    rb_replaying=0;rb_tx.mode=0;
    if(rb_tx.failed || rb_tx.cursor!=rb_tx.count || rb_audit.count!=rb_tx.after_count || rb_audit.frees!=rb_tx.after_frees)pass=0;
    unsigned different=0;
    for(unsigned i=0;i<rb_probe.count;++i){RbProbeBlock *b=&rb_probe.blocks[i];if(memcmp(rb_probe.after+b->offset,b->pointer,b->size))++different;}
    for(unsigned i=0;i<rb_tx.count;++i){RbHeapEvent *e=&rb_tx.events[i];if(e->kind==1 && memcmp(rb_probe.after+e->after,e->pointer,e->size))++different;}
    if(different || rb_history.journal.count!=8 || !rb_discard_before(&rb_history.journal,4) || rb_history.journal.count!=4 || rb_undo_to(&rb_history.journal,0))pass=0;
    log_line("RB_HISTORY %s serial=%lu native_steps=8 rewind_to=4 retained=4 rebuilt=8 retired_before=4 different=%u undo_bytes=%lu events_at_target=%u events_at_end=%u",pass?"PASS":"FAIL",(unsigned long)rb_probe.serial,different,(unsigned long)rb_history.total,rb_history.events,rb_tx.count);
    rb_destroy(&rb_history.journal);return pass;
}
#endif
