#ifndef TH18_NATIVE_ROLLBACK_ORACLE_H
#define TH18_NATIVE_ROLLBACK_ORACLE_H
/* Expensive isolated-test oracle, not a normal frame cost. Independently
   restore/re-execute a sampled actual native Tick+Draw and compare every
   candidate owner and newborn byte, including enemy/ECL/bullet/item pools.
   Exact allocator reuse makes even pointer fields comparable in-process. */
static int rb_live_oracle_enabled(uint32_t frame) {
    static int initialised,enabled;
    if(!initialised){wchar_t value[4];enabled=(online_map->reserved[1]&1) && GetEnvironmentVariableW(L"TH18_COOP_RB_ORACLE",value,4)==1 && value[0]==L'1';initialised=1;}
    return enabled && frame%(U32(0x4cccdc)>=2?15:60)==0;
}
static int rb_live_oracle(void *scheduler,RbNativeFrame *f,int expected_result) {
    if(!rb_live_oracle_enabled(f->frame))return 1;
    RbSoundFrame expected_sound=f->sound;
    if(!rb_probe.after)rb_probe.after=VirtualAlloc(NULL,128u*1024u*1024u,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
    if(!rb_probe.after)return 0;SIZE_T used=rb_probe.used;
    for(unsigned i=0;i<rb_probe.count;++i){RbProbeBlock *b=&rb_probe.blocks[i];memcpy(rb_probe.after+b->offset,b->pointer,b->size);
        if(b->pointer==(void *)0x4c3000)for(SIZE_T n=0;n+4<=b->size;n+=4)if(!rb_probe_global_owned((uintptr_t)b->pointer+n) && memcmp(rb_probe_saved(b)+n,(unsigned char *)b->pointer+n,4)){
            log_line("ROLLBACK oracle unowned global=%08lx before=%08lx after=%08lx",(unsigned long)((uintptr_t)b->pointer+n),(unsigned long)*(uint32_t *)(rb_probe_saved(b)+n),(unsigned long)*(uint32_t *)((unsigned char *)b->pointer+n));return 0;}}
    for(unsigned i=0;i<rb_tx.count;++i){RbHeapEvent *e=&rb_tx.events[i];if(e->kind!=1)continue;if(e->size>128u*1024u*1024u-used)return 0;e->after=used;memcpy(rb_probe.after+used,e->pointer,e->size);used+=e->size;}
    if(!rb_live_undo(f->frame,0)){log_line("ROLLBACK oracle undo failed frame=%lu",(unsigned long)f->frame);return 0;}
    rb_tx.mode=2;rb_tx.cursor=0;rb_runtime_barrier=0;memset(&f->sound,0,sizeof(f->sound));rb_live.sound=&f->sound;
    rb_runtime_speculating=rb_runtime_virtual=1;rb_replaying=1;rb_live_keys(f->keys);
    int result=((Tick)online_tick_original)(scheduler);
    int rendered=!rb_runtime_barrier && !rb_tx.failed && rb_live_render();
    rb_replaying=rb_runtime_speculating=rb_runtime_virtual=0;rb_live.sound=NULL;rb_tx.mode=0;
    if(!rendered || result!=expected_result || rb_tx.cursor!=rb_tx.count || rb_audit.frees!=rb_tx.after_frees){log_line("ROLLBACK oracle execution failed frame=%lu rendered=%d result=%d/%d events=%u/%u frees=%u/%u failed=%d boundary=%d",(unsigned long)f->frame,rendered,result,expected_result,rb_tx.cursor,rb_tx.count,rb_audit.frees,rb_tx.after_frees,rb_tx.failed,rb_runtime_barrier);return 0;}
    unsigned different=0;
    for(unsigned i=0;i<rb_probe.count;++i){RbProbeBlock *b=&rb_probe.blocks[i];const unsigned char *expected=rb_probe.after+b->offset,*now=b->pointer;SIZE_T n=0;
        if(b->pointer==(void *)0x4c3000){for(;n+4<=b->size;n+=4)if(rb_probe_global_owned((uintptr_t)b->pointer+n) && memcmp(expected+n,now+n,4))break;}
        else while(n<b->size && expected[n]==now[n])++n;
        if(n<b->size){++different;if(different<5)log_line("ROLLBACK oracle difference owner=%p size=%lu caller=%lx offset=%lu expected=%02x actual=%02x",b->pointer,(unsigned long)b->size,(unsigned long)b->caller,(unsigned long)n,expected[n],now[n]);}}
    for(unsigned i=0;i<rb_tx.count;++i){RbHeapEvent *e=&rb_tx.events[i];if(e->kind==1 && memcmp(rb_probe.after+e->after,e->pointer,e->size))++different;}
    int sound_equal=!memcmp(&expected_sound,&f->sound,sizeof(expected_sound));
    log_line("ROLLBACK oracle %s frame=%lu owners=%u bytes=%lu events=%u different=%u sound_equal=%d",different || !sound_equal?"FAIL":"PASS",(unsigned long)f->frame,rb_probe.count,(unsigned long)used,rb_tx.count,different,sound_equal);
    return !different && sound_equal && rb_live_seal(f->frame);
}
#endif
