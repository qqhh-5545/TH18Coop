#ifndef TH14_ONLINE_H
#define TH14_ONLINE_H
#include "../online_ipc.h"
#include "../eagler_core.h"
static CoopOnlineMap *online_map;
static unsigned party_count(void){return players;}
#include "../rollback_ipc.h"
static HANDLE online_mapping,online_request,online_reply;
static int online_boot,online_phase,online_pending,online_sampling,online_executing;
static uint32_t online_previous[3],online_raw;
static struct {void *core;unsigned frame,sequence,delay;DWORD progress,sent;int pending,result;
    struct {uint32_t frame,digest;} hashes[3][256];} net14;
static uint32_t hash14_word(uint32_t h,uint32_t value){for(unsigned j=0;j<4;++j)h=(h^(uint8_t)(value>>(j*8)))*16777619u;return h;}
static uint32_t hash14(void) {
    uint32_t h=2166136261u;const unsigned globals[]={STAGE,CHARA,STYLE,0x4f5834,0x4d9648,0x4d964c,0x4db510,0x4db514};
    for(unsigned i=0;i<sizeof(globals)/sizeof(globals[0]);++i)h=hash14_word(h,U32(globals[i]));
    h=hash14_word(h,draft_owner);
    for(unsigned id=0;id<players;++id) {
        Seat *s=&seats[id];h=hash14_word(h,s->player!=NULL);
        if(s->player){const unsigned offsets[]={0x5e0,0x5e4,0x684,0x690,0x182c4};for(unsigned i=0;i<5;++i)h=hash14_word(h,FIELD(s->player,offsets[i]));}
        const unsigned resources[]={0x4f5830,0x4f5858,0x4f5864,0x4f5868,0x4f5870,0x4f5874};
        if(s->player)for(unsigned i=0;i<6;++i)h=hash14_word(h,*resource(id,resources[i]));
        h=hash14_word(h,s->ghost);h=hash14_word(h,s->character);h=hash14_word(h,s->style);
        h=hash14_word(h,s->bomb?FIELD(s->bomb,0x40):0);
        uint32_t drift[2];memcpy(drift,&s->drift_x,sizeof(drift));h=hash14_word(h,drift[0]);h=hash14_word(h,drift[1]);
        RuleState14 *r=&rules14[id];h=hash14_word(h,r->previous);h=hash14_word(h,r->life);h=hash14_word(h,r->power);h=hash14_word(h,r->life_window);h=hash14_word(h,r->power_window);h=hash14_word(h,r->target);h=hash14_word(h,r->power_target);h=hash14_word(h,r->release);
    }
    h=hash14_word(h,wipe14);
    // Hash simulation values, never addresses, animation VMs or graphics RNG.
    void *items=PTR(0x4db660);h=hash14_word(h,items!=NULL);
    if(items)for(unsigned i=0;i<0x1258;++i){void *item=OFF(items,0x14+i*0xc18);unsigned state=FIELD(item,0xbf0);if(!state)continue;
        h=hash14_word(h,i);h=hash14_word(h,state);h=hash14_word(h,FIELD(item,0xbf4));
        const unsigned offsets[]={0xbac,0xbb0,0xbb8,0xbbc,0xbfc};for(unsigned j=0;j<5;++j)h=hash14_word(h,FIELD(item,offsets[j]));
        h=hash14_word(h,(int)item_owners[i]);h=hash14_word(h,(int)gift_owners[i]);h=hash14_word(h,gift_power[i]);}
    void *bullets=PTR(0x4db530);h=hash14_word(h,bullets!=NULL);
    if(bullets){void *head=OFF(bullets,0x7c),*node=(void *)(uintptr_t)FIELD(head,4);unsigned count=0;
        while(node && node!=head && count++<2001){void *bullet=(void *)(uintptr_t)FIELD(node,0);if(bullet){
            const unsigned offsets[]={0x20,0xbc0,0xbc4,0xbcc,0xbd0,0xbd8,0xbdc,0xc04,0xc20,0x13bc,0x13ec};
            for(unsigned j=0;j<11;++j)h=hash14_word(h,FIELD(bullet,offsets[j]));}node=(void *)(uintptr_t)FIELD(node,4);}
        h=hash14_word(h,count);}
    void *enemies=PTR(0x4db544);h=hash14_word(h,enemies!=NULL);
    if(enemies){h=hash14_word(h,FIELD(enemies,0xd8));h=hash14_word(h,FIELD(enemies,0xdc));
        for(unsigned i=0;i<4;++i){void *boss=(void *)(uintptr_t)FIELD(enemies,0x5c+i*4);h=hash14_word(h,boss!=NULL);if(boss){
            const unsigned offsets[]={0x11e4,0x1234,0x1238,0x5164,0x516c,0x5244,0x5248};for(unsigned j=0;j<7;++j)h=hash14_word(h,FIELD(boss,offsets[j]));}}}
    return h;
}
static int assets14_ready(void) {
    void *manager=PTR(ANM);if(manager)for(unsigned i=0;i<26;++i){void *file=(void *)(uintptr_t)FIELD(manager,0xbc7b0c+i*4);
        if(file && FIELD(file,0x12c)){((void (TC *)(void *))0x47d830)(file);((void (__cdecl *)(void *))0x4854fe)(file);FIELD(manager,0xbc7b0c+i*4)=0;return 0;}
        if(file && FIELD(file,0x128)){((int (__stdcall *)(void *))0x47d230)(file);return 0;}}
    HANDLE worker=(HANDLE)PTR(0x4d98f0);
    if(worker && WaitForSingleObject(worker,0)!=WAIT_OBJECT_0){((int (__cdecl *)(void))0x46e5e0)();return 0;}
    // Returning to title also starts a MainMenu-owned zThread. Its handle is
    // at +0x5d88 (TH14), separate from the global scene-loader handle above.
    // Finish its ANM/audio work before hashing or advancing the logical tick:
    // the worker resets draft_owner via the native menu-state hook.
    void *menu=PTR(0x4db6a4);
    if(menu && valid(menu,0x5da0)) {
        HANDLE menu_worker=(HANDLE)(uintptr_t)FIELD(menu,0x5d88);
        if(menu_worker && WaitForSingleObject(menu_worker,0)!=WAIT_OBJECT_0){((int (__cdecl *)(void))0x46e5e0)();return 0;}
    }
    return 1;
}
static void online14_fail(unsigned code) {
    if(online_map && online_map->status==4)return;
    if(online_map){online_map->error=code;InterlockedExchange(&online_map->status,4);}
    if(rb_pipe){rb_pipe->error=code;InterlockedExchange(&rb_pipe->status,4);}
    log_line("ONLINE stopped error=%u frame=%u",code,net14.frame);
}
static int online14_open(void) {
    wchar_t token[40],name[100];DWORD n=GetEnvironmentVariableW(L"TH14_COOP_ONLINE",token,40);if(!n)return 1;
    if(n!=32)return 0;for(unsigned i=0;i<32;++i)if(!((token[i]>=L'0' && token[i]<=L'9')||(token[i]>=L'a' && token[i]<=L'f')))return 0;
    swprintf(name,100,L"Local\\TH18CoopOnline-%ls",token);online_mapping=OpenFileMappingW(FILE_MAP_ALL_ACCESS,FALSE,name);if(!online_mapping)return 0;
    online_map=MapViewOfFile(online_mapping,FILE_MAP_ALL_ACCESS,0,0,sizeof(*online_map));
    if(!online_map || online_map->magic!=COOP_ONLINE_MAGIC || online_map->schema!=2 || online_map->players<2 || online_map->players>3 || online_map->role>=online_map->players)return 0;
    players=online_map->players;
    if(!(online_map->reserved[1]&32) || !rb_pipe_open(token))return 0;
    swprintf(name,100,L"Local\\TH18CoopOnline-%ls-request",token);online_request=OpenEventW(EVENT_MODIFY_STATE,FALSE,name);
    swprintf(name,100,L"Local\\TH18CoopOnline-%ls-reply",token);online_reply=OpenEventW(SYNCHRONIZE,FALSE,name);
    if(!online_request || !online_reply)return 0;
    online_map->pid=GetCurrentProcessId();InterlockedExchange(&online_map->status,1);return 1;
}
static uint32_t online14_sample(void) {
    uint32_t keys=0;
    if(test && test->magic==0x54483134)return test->keys[online_map->role];
    if((online_map->reserved[1]&1) && online_map->reserved_tail[1]==0x54455354u)return online_map->reserved_tail[0]&0x80dfbu;
    if(GetForegroundWindow()!=(HWND)(uintptr_t)U32(0x4f5a18))return 0;
    unsigned char saved[0x234];memcpy(saved,(void *)0x4d6878,sizeof(saved));
    online_sampling=1;keys=((uint32_t (__cdecl *)(void))0x401cd0)();online_sampling=0;
    memcpy((void *)0x4d6878,saved,sizeof(saved));return kb_online(keys,GetAsyncKeyState)&0x80dfbu;
}
static void online14_keys(const uint32_t keys[3]) {
    for(unsigned i=0;i<players;++i)sampled[i]=keys[i];
    void *menu=PTR(0x4db6a4);unsigned owner=menu && draft_owner && (FIELD(menu,0x1c)==6 || FIELD(menu,0x1c)==7)?draft_owner:0;
    uint32_t held=keys[owner],previous=online_previous[owner];
    if(owner && draft_release){if(!held)draft_release=0;held=0;previous=0;}
    U32(0x4d6878)=held;U32(0x4d687c)=previous;((Callback)input_original)((void *)0x4d6878);
    U32(GAME_INPUT)=keys[0];U32(GAME_INPUT+4)=online_previous[0];U32(GAME_INPUT+12)=keys[0]&~online_previous[0];
    for(unsigned i=0;i<players;++i)online_previous[i]=keys[i];
}
static void online14_submit(unsigned kind) {
    online_map->step=0;online_map->kind=kind;online_map->digest=COOP_ONLINE_MAGIC;online_map->result=1;online_map->local_input=0;
    online_map->rng[0]=online_map->initial_seed&0xffff;online_map->rng[1]=0;online_map->rng[2]=online_map->initial_seed>>16;online_map->rng[3]=0;
    online_map->options=0;online_pending=1;InterlockedExchange(&online_map->status,2);SetEvent(online_request);
}
static int online14_take(void) {
    if(online_map->status==4 || online_map->status==5)return -1;
    if(online_map->status!=3)return 0;
    MemoryBarrier();online_pending=0;InterlockedExchange(&online_map->status,1);SetEvent(online_request);return 1;
}
static int net14_receive(void) {
    if(rb_pipe->status==4)return 0;
    unsigned read=rb_pipe->in_read,write=rb_pipe->in_write;if(write-read>RB_PIPE_SLOTS)return 0;
    while(read!=write){
        MemoryBarrier();RbPipePacket *packet=&rb_pipe->incoming[read%RB_PIPE_SLOTS];
        if(packet->peer>=players || packet->peer==online_map->role || !packet->size || packet->size>RB_PIPE_PAYLOAD)return 0;
        if(packet->data[0]==0){if(!ec_receive(net14.core,packet->peer,packet->data+1,packet->size-1))return 0;}
        else if(packet->data[0]==1 && packet->size==9) {
            uint32_t at,digest;memcpy(&at,packet->data+1,4);memcpy(&digest,packet->data+5,4);unsigned slot=at%256,own=online_map->role;
            if(at>net14.frame+64 || net14.frame>at+256)return 0;
            net14.hashes[packet->peer][slot].frame=at;net14.hashes[packet->peer][slot].digest=digest;
            if(net14.hashes[own][slot].frame==at && net14.hashes[own][slot].digest!=digest){log_line("DESYNC frame=%u own=%08x peer%u=%08x",at,net14.hashes[own][slot].digest,packet->peer+1,digest);return 0;}
        }else return 0;
        InterlockedExchange(&rb_pipe->in_read,++read);net14.progress=GetTickCount();
    }
    return ec_rollback(net14.core)==UINT32_MAX;
}
static int net14_send(void) {
    for(unsigned id=0;id<players;++id)if(id!=online_map->role){uint8_t packet[513];size_t n=0;packet[0]=0;
        if(!ec_encode(net14.core,id,net14.frame+net14.delay,++net14.sequence,packet+1,512,&n) || !rb_pipe_send(id,packet,(unsigned)n+1))return 0;
    }
    net14.sent=GetTickCount();return 1;
}
static int net14_finish(int result) {
    uint32_t h=hash14(),at=net14.frame;unsigned slot=at%256,own=online_map->role;
    if(!ec_mark(net14.core,at)){log_line("ONLINE mark rejected frame=%u",at);return 0;}
    if(test && !seats[0].player && U32(STAGE))log_line("ONLINE boundary frame=%u hash=%08x state=%u/%u rng=%08x/%08x draft=%u ghost=%u/%u bombs=%u/%u",at,h,U32(0x4d9648),U32(0x4d964c),U32(0x4db510),U32(0x4db514),draft_owner,seats[0].ghost,seats[1].ghost,seats[0].bomb?FIELD(seats[0].bomb,0x40):0,seats[1].bomb?FIELD(seats[1].bomb,0x40):0);
    net14.hashes[own][slot].frame=at;net14.hashes[own][slot].digest=h;
    for(unsigned id=0;id<players;++id)if(id!=own && net14.hashes[id][slot].frame==at && net14.hashes[id][slot].digest!=h){log_line("DESYNC finish frame=%u own=%08x peer%u=%08x",at,h,id+1,net14.hashes[id][slot].digest);return 0;}
    uint8_t packet[9];packet[0]=1;memcpy(packet+1,&at,4);memcpy(packet+5,&h,4);
    for(unsigned id=0;id<players;++id)if(id!=own && !rb_pipe_send(id,packet,9))return 0;
    ++net14.frame;rb_pipe->frame=net14.frame;rb_pipe->confirmed=net14.frame-1;online_map->completed=net14.frame+1;
    net14.progress=GetTickCount();net14.pending=0;
    if(at%120==0)log_line("ONLINE step=%u digest=%08x stage=%u ready=%u",at,h,U32(STAGE),seats[1].player!=NULL);
    if(result<=0)InterlockedExchange(&rb_pipe->status,result?4:2);return 1;
}
static int online14_root(void *registry) {
    if(rb_pipe->end_request || (U32(0x4d9684)&0x80u)){if(rb_pipe->end_request)PostMessageW((HWND)(uintptr_t)U32(0x4f5a18),WM_CLOSE,0,0);if(!assets14_ready())return 1;InterlockedExchange(&rb_pipe->status,2);return tick_native14(registry);}
    if(!online_boot) {
        void *menu=PTR(0x4db6a4);if(!menu || FIELD(menu,0x1c)!=1 || !assets14_ready())return tick_native14(registry);
        if(online_map->reserved[1]&1) {
            HANDLE own=(HANDLE)PTR(0x4fd84c);if(!own || !ReleaseMutex(own) || !CloseHandle(own)){online14_fail(14);return 1;}PTR(0x4fd84c)=NULL;
        }
        online_map->reserved[0]=3;online_boot=1;log_line("ONLINE title barrier P%u players=%u",online_map->role+1,players);
    }
    if(online_phase<2) {
        if(!online_pending)online14_submit(online_phase+1);
        int ready=online14_take();if(ready<=0)return 1;
        if(online_phase==0) {
            memcpy((void *)0x4db508,online_map->rng,16);memset((void *)0x4d6878,0,0x234);U32(GAME_INPUT)=0;
            void *menu=PTR(0x4db6a4);((void (TC *)(void *,int))menu_state_original)(menu,1);FIELD(menu,0x28)=FIELD(menu,0x2c)=0;
            U32(0x4f58bc)=0;
            online_executing=1;tick_native14(registry);online_executing=0;online_phase=1;return 1;
        }
        online_phase=2;online_map->completed=1;
        net14.delay=rb_pipe->padding[0];if(net14.delay<1 || net14.delay>12){online14_fail(51);return 1;}
        net14.core=ec_create_delay(players,online_map->role,net14.delay,(uint64_t)rb_pipe->session[0]|((uint64_t)rb_pipe->session[1]<<32));
        if(!net14.core){online14_fail(51);return 1;}
        for(unsigned id=0;id<3;++id)for(unsigned j=0;j<256;++j)net14.hashes[id][j].frame=UINT32_MAX;
        InterlockedExchange(&rb_pipe->status,1);net14.progress=GetTickCount();log_line("ONLINE exact input driver delay=%u",net14.delay);
    }
    if(!net14_receive()){online14_fail(52);return 1;}
    if(GetTickCount()-net14.progress>90000){online14_fail(53);return 1;}
    if((unsigned)rb_pipe->out_write-(unsigned)rb_pipe->out_read>RB_PIPE_SLOTS-8)return 1;
    if(net14.pending){if(!assets14_ready())return 1;if(!net14_finish(net14.result)){online14_fail(54);return 1;}}
    if(!ec_has_local(net14.core,net14.frame)) {
        if(!ec_local(net14.core,net14.frame,online14_sample()) || !net14_send()){online14_fail(55);return 1;}
    }else if(GetTickCount()-net14.sent>=25 && !net14_send()){online14_fail(55);return 1;}
    uint32_t keys[3];unsigned predicted;
    if(!ec_prepare(net14.core,net14.frame,keys,&predicted)){++rb_pipe->waits;return 1;}
    if(predicted){online14_fail(56);return 1;}
    if(!assets14_ready())return 1;
    online14_keys(keys);online_executing=1;
    int result=tick_native14(registry);online_executing=0;++frame;
    if(!assets14_ready()){net14.pending=1;net14.result=result;return 1;}
    if(!net14_finish(result)){online14_fail(54);return 1;}return result;
}
#endif
