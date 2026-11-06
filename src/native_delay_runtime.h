#ifndef TH18_NATIVE_DELAY_RUNTIME_H
#define TH18_NATIVE_DELAY_RUNTIME_H
/* Fixed input delay, following th06_multi_net's capture/send -> frame-D
   consumption scheme. No speculative native Tick, snapshots or replay. */
static struct {
    void *core;
    uint32_t frame,sequence;
    DWORD last_packet,last_progress;
    unsigned delay;
    int ready,pending,pending_result;
    struct {uint32_t frame,digest;} hashes[3][256];
} dl_live;
static int dl_enabled(void){return online_map && (online_map->reserved[1]&32u);}
static void dl_stop(unsigned code,const char *reason){
    rb_pipe->error=code;InterlockedExchange(&rb_pipe->status,4);
    log_line("DELAY stopped frame=%lu error=%u reason=%s",(unsigned long)dl_live.frame,code,reason);online_fail(code);
}
static int dl_hash(unsigned seat,uint32_t frame,uint32_t digest){
    if(frame>dl_live.frame+64 || (dl_live.frame>256 && frame<dl_live.frame-256))return 0;
    unsigned slot=frame%256,own=online_map->role;
    dl_live.hashes[seat][slot].frame=frame;dl_live.hashes[seat][slot].digest=digest;
    if(dl_live.hashes[own][slot].frame!=frame)return 1;
    for(unsigned id=0;id<party_count();++id)
        if(dl_live.hashes[id][slot].frame==frame && dl_live.hashes[id][slot].digest!=dl_live.hashes[own][slot].digest){
            log_line("DELAY digest mismatch frame=%lu P%u=%08lx P%u=%08lx",(unsigned long)frame,own+1,
                (unsigned long)dl_live.hashes[own][slot].digest,id+1,(unsigned long)dl_live.hashes[id][slot].digest);return 0;
        }
    return 1;
}
static int dl_receive(void){
    if(rb_pipe->status==4)return 0;
    unsigned read=(unsigned)rb_pipe->in_read,write=(unsigned)rb_pipe->in_write;
    if(write-read>RB_PIPE_SLOTS)return 0;
    while(read!=write){
        MemoryBarrier();RbPipePacket *p=&rb_pipe->incoming[read%RB_PIPE_SLOTS];
        if(p->peer>=party_count() || p->peer==online_map->role || !p->size || p->size>RB_PIPE_PAYLOAD)return 0;
        if(p->data[0]==0){if(!ec_receive(dl_live.core,p->peer,p->data+1,p->size-1))return 0;}
        else if(p->data[0]==1 && p->size==9){uint32_t frame,digest;memcpy(&frame,p->data+1,4);memcpy(&digest,p->data+5,4);if(!dl_hash(p->peer,frame,digest))return 0;}
        else return 0;
        InterlockedExchange(&rb_pipe->in_read,(LONG)++read);dl_live.last_progress=GetTickCount();
    }
    return ec_rollback(dl_live.core)==UINT32_MAX;
}
static int dl_send_inputs(void){
    for(unsigned id=0;id<party_count();++id)if(id!=online_map->role){unsigned char bytes[513];size_t size=0;bytes[0]=0;
        if(!ec_encode(dl_live.core,id,dl_live.frame+dl_live.delay,++dl_live.sequence,bytes+1,512,&size) || !rb_pipe_send(id,bytes,(unsigned)size+1))return 0;
    }
    dl_live.last_packet=GetTickCount();return 1;
}
static int dl_initialise(void){
    dl_live.delay=rb_pipe->padding[0];if(dl_live.delay<1 || dl_live.delay>12)return 0;
    uint64_t session=(uint64_t)rb_pipe->session[0]|((uint64_t)rb_pipe->session[1]<<32);
    dl_live.core=ec_create_delay(party_count(),online_map->role,dl_live.delay,session);if(!dl_live.core)return 0;
    for(unsigned id=0;id<3;++id)for(unsigned i=0;i<256;++i)dl_live.hashes[id][i].frame=UINT32_MAX;
    if((online_map->reserved[1]&1) && rb_pipe->flags==1)rb_live.scenario=1;
    dl_live.last_progress=GetTickCount();dl_live.ready=1;InterlockedExchange(&rb_pipe->status,1);
    log_line("DELAY native driver ready players=%u delay=%u prediction=0 history=0",party_count(),dl_live.delay);return 1;
}
static int dl_finish(int result){
    if(!supply_test_after(dl_live.frame))return 0;
    uint32_t frame=dl_live.frame,digest=online_digest();unsigned char packet[9];
    if(!ec_mark(dl_live.core,frame) || !dl_hash(online_map->role,frame,digest))return 0;
    packet[0]=1;memcpy(packet+1,&frame,4);memcpy(packet+5,&digest,4);
    for(unsigned id=0;id<party_count();++id)if(id!=online_map->role && !rb_pipe_send(id,packet,sizeof(packet)))return 0;
    rb_scenario_after(frame,1);
    ++dl_live.frame;online_step=dl_live.frame+1;online_map->completed=online_step;
    rb_pipe->frame=dl_live.frame;rb_pipe->confirmed=dl_live.frame-1;
    dl_live.last_progress=GetTickCount();dl_live.pending=0;
    if(result<=0)InterlockedExchange(&rb_pipe->status,result?4:2);
    return 1;
}
static int dl_step(void *scheduler){
    if(!dl_live.ready && !dl_initialise()){dl_stop(51,"initialisation");return 1;}
    if(!dl_receive()){dl_stop(52,"input or world digest");return 1;}
    if(GetTickCount()-dl_live.last_progress>90000){dl_stop(53,"peer timeout");return 1;}
    /* Bound outstanding packets; reserve both the next input and digest. */
    if((unsigned)rb_pipe->out_write-(unsigned)rb_pipe->out_read>RB_PIPE_SLOTS-8){++rb_pipe->waits;return 1;}
    if(dl_live.pending){
        if(!online_load_ready() || !online_assets_ready())return 1;
        if(!dl_finish(dl_live.pending_result)){dl_stop(54,"loaded frame completion");return 1;}
    }
    if(!ec_has_local(dl_live.core,dl_live.frame)){
        if(!ec_local(dl_live.core,dl_live.frame,rb_live_sample()) || !dl_send_inputs()){dl_stop(55,"input capture or send");return 1;}
    }else if(GetTickCount()-dl_live.last_packet>=25 && !dl_send_inputs()){dl_stop(55,"input retry");return 1;}
    uint32_t keys[3];unsigned predicted=0;
    if(!ec_prepare(dl_live.core,dl_live.frame,keys,&predicted)){++rb_pipe->waits;return 1;}
    if(predicted){dl_stop(56,"prediction forbidden");return 1;}
    if(!online_load_ready() || !online_assets_ready())return 1;
    if(!rb_scenario_before(dl_live.frame)){dl_stop(57,"isolated scenario");return 1;}
    if(!supply_test_before((online_map->reserved[1]&1) && rb_pipe->flags==2,dl_live.frame)){dl_stop(58,"isolated pickup scenario");return 1;}
    gameplay_test_before((online_map->reserved[1]&1) && rb_pipe->flags==3,keys);
    rb_live_keys(keys);U32(0x4cd624)&=~0x180u;
    int result=rp_native_root(scheduler);
    /* Complete asynchronous construction before digest publication. Never
       repeat the already executed Tick, and do not resample this input. */
    if(result>0 && (!online_load_ready() || !online_assets_ready())){
        dl_live.pending=1;dl_live.pending_result=result;return 1;
    }
    if(!dl_finish(result)){dl_stop(54,"native frame completion");return 1;}
    return result;
}
static void dl_teardown(void){
    if(!dl_enabled() || !rb_pipe)return;
    if(rb_pipe->status!=4)InterlockedExchange(&rb_pipe->status,2);
    log_line("DELAY shutdown frame=%lu prediction=0 replay=0",(unsigned long)dl_live.frame);
}
#endif
