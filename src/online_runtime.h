#ifndef COOP_ONLINE_RUNTIME_H

#define COOP_ONLINE_RUNTIME_H

#include "online_ipc.h"

static HANDLE online_mapping,online_request,online_reply;

static int online_started,online_sampling,online_pending,online_phase,online_boot_pending;

static uint32_t online_keys[3],online_previous,online_previous_p2,online_previous_p3,online_raw,online_step,online_result;

static int online_inside_scheduler;

static DWORD online_deadline;

static DWORD online_load_started;

static int online_loading;

static void *online_tick_original;

static int online_poll(void *input) {

    if(!online_map || input!=(void *)0x4ca210)return 0;

    if(online_sampling){online_raw=kb_online(FIELD(input,0),GetAsyncKeyState);return 2;}

    uint32_t p1_keys=online_started?online_keys[0]:0,p2_keys=online_started?online_keys[1]:0,p3_keys=online_started?online_keys[2]:0;

    uint32_t held=p1_keys,previous=online_previous;

    /* The pause object exists throughout the stage, including normal play.

       Native shop/draw code checks +1ec to distinguish an active pause. */

    void *pause=PTR(0x4cf40c);

    if(PTR(0x4cf2cc) || (pause && FIELD(pause,0x1ec))) {

        uint32_t pressed=(p1_keys&~online_previous)|(p2_keys&~online_previous_p2)|(p3_keys&~online_previous_p3);

        held|=p2_keys|p3_keys;previous=held&~pressed;

    }

    FIELD(input,0)=held;FIELD(input,4)=previous;

    online_previous=p1_keys;online_previous_p2=p2_keys;online_previous_p3=p3_keys;

    coop_input_capture(&input_latch,p1_keys,p2_keys);input_latch.p3=p3_keys;

    return 1;

}

static uint32_t online_hash_word(uint32_t h,uint32_t word) {

    for(unsigned shift=0;shift<32;shift+=8)h=(h^(unsigned char)(word>>shift))*16777619u;

    return h;

}

#include "native_world_digest.h"

static uint32_t online_digest_detail[13];
static uint32_t online_digest(void) {
    memset(online_digest_detail,0,sizeof(online_digest_detail));online_digest_detail[8]=U32(0x4cccdc);memcpy(online_digest_detail+9,(void *)0x4cf280,16);

    uint32_t h=2166136261u;

    if(online_map) {

        online_map->reserved[3]=U32(0x4cccdc);online_map->reserved[4]=U32(0x4cd5e4);online_map->reserved[5]=U32(0x4cd5e8);

        online_map->reserved[10]=U32(0x4cd894);online_map->reserved[11]=U32(0x4cd890);

    }

    const uintptr_t globals[]={0x4cccdc,0x4cccc8,0x4cccf4,0x4cccf8,0x4ccd00,0x4cd5e4,0x4cd5e8,0x4ccd34,0x4cf288,0x4cf28c,0x4cccd0};

    for(unsigned i=0;i<sizeof(globals)/sizeof(globals[0]);++i)h=online_hash_word(h,U32(globals[i]));

    h=online_hash_word(h,party_count());

    for(unsigned id=1;id<party_count();++id){h=online_hash_word(h,guests[id-1].character);h=online_hash_word(h,guests[id-1].funds);}

    h=online_hash_word(h,draft_owner);h=online_hash_word(h,shop_owner);

    online_digest_detail[0]=h;
    if(online_map)online_map->reserved[6]=h;

    void *menu=PTR(0x4cf43c);

    if(valid(menu,0x390)) {

        const unsigned offsets[]={0x18,0x20,0x24,0xb0,0x388};

        for(unsigned i=0;i<5;++i)h=online_hash_word(h,FIELD(menu,offsets[i]));

    } else h=online_hash_word(h,UINT32_MAX);

    online_digest_detail[1]=h;
    if(online_map)online_map->reserved[7]=h;

    for(unsigned id=0;id<party_count();++id) {

        void *player=ship(id);

        h=online_hash_word(h,player?1:0);

        if(valid(player,0x479d0)) {

            const unsigned offsets[]={0x620,0x624,0x476ac,0x476cc,0x47778};

            for(unsigned i=0;i<5;++i)h=online_hash_word(h,FIELD(player,offsets[i]));

        }

        for(unsigned offset=0;offset<0x30;offset+=4)

            h=online_hash_word(h,(uint32_t)*resource(id,offset));

        void *cards=ability(id);unsigned count=0;

        if(valid(cards,0x40)) {

            h=online_hash_word(h,FIELD(cards,0x28));

            void *node=(void *)(uintptr_t)FIELD(cards,0x1c);

            while(valid(node,8) && count++<128) {

                void *card=(void *)(uintptr_t)FIELD(node,0);

                if(!valid(card,0x4c))break;

                h=online_hash_word(h,FIELD(card,4));h=online_hash_word(h,FIELD(card,0x24));h=online_hash_word(h,FIELD(card,0x38));h=online_hash_word(h,FIELD(card,0x48));

                h=online_hash_word(h,FIELD(cards,0x38)==(uint32_t)(uintptr_t)card);

                node=(void *)(uintptr_t)FIELD(node,4);

            }

        }

        h=online_hash_word(h,partner[id].ghost);h=online_hash_word(h,partner[id].life_taps);h=online_hash_word(h,partner[id].power_taps);

        online_digest_detail[2+id]=h;
        if(online_map)online_map->reserved[8+(id!=0)]=h;

    }

    online_digest_detail[4]=h;
    if(rb_runtime_enabled || (online_map && (online_map->reserved[1]&32u))){h=online_hash_bullets(h,PTR(0x4cf2bc));online_digest_detail[5]=h;h=online_hash_enemies(h,PTR(0x4cf2d0));online_digest_detail[6]=h;h=online_hash_items(h,PTR(0x4cf2ec));online_digest_detail[7]=h;}

    return h;

}

static int online_assets_ready(void) {

    void *manager=PTR(0x51f65c);if(!manager)return 1;

    for(unsigned i=0;i<33;++i) {

        void *file=(void *)(uintptr_t)FIELD(manager,0x312072c+i*4);if(!file)continue;

        if(FIELD(file,0x12c)) {

            ((void (TC *)(void *,unsigned))0x486fc0)(manager,i);

            file=(void *)(uintptr_t)FIELD(manager,0x312072c+i*4);

            if(file)FIELD(file,0x12c)=0;return 0;

        }

        if(FIELD(file,0x128)) {((int (TC *)(void *,void *))0x486920)(manager,file);return 0;}

    }

    return 1;

}

static void online_fail(unsigned code) {

    online_map->error=code;InterlockedExchange(&online_map->status,4);

    log_line("ONLINE stopped at step=%lu phase=%d error=%u; gameplay remains paused",

        (unsigned long)online_step,online_phase,code);

}

static int online_load_ready(void) {

    /* 454860 starts the scene worker, stores its handle at 4cd888 and sets

       4cd894. The worker clears that flag before its final instructions;

       require the handle to be signalled as well. Never hash partial actors

       or overwrite RNG while that worker is still constructing the scene. */

    HANDLE worker=(HANDLE)PTR(0x4cd888);

    DWORD state=worker?WaitForSingleObject(worker,0):WAIT_OBJECT_0;

    /* 464cb0 starts a separate menu-owned job on return to title. Unlike

       the scene worker, 464b80 leaves the job control flag +5d8c set on

       normal return: completion is the thread handle +5d80, not that flag. */

    void *menu=PTR(0x4cf43c);

    HANDLE menu_worker=valid(menu,0x5d90)?(HANDLE)(uintptr_t)FIELD(menu,0x5d80):NULL;

    DWORD menu_state=menu_worker?WaitForSingleObject(menu_worker,0):WAIT_OBJECT_0;

    if(state==WAIT_FAILED || menu_state==WAIT_FAILED){online_fail(17);return 0;}

    if(!U32(0x4cd894) && state==WAIT_OBJECT_0 && menu_state==WAIT_OBJECT_0) {

        if(online_loading)log_line("ONLINE scene load complete; step=%lu wait_ms=%lu",

            (unsigned long)online_step,(unsigned long)(GetTickCount()-online_load_started));

        online_loading=0;return 1;

    }

    if(!online_loading) {

        online_loading=1;online_load_started=GetTickCount();

        log_line("ONLINE scene load barrier; step=%lu stage=%lu menu_pending=%d",

            (unsigned long)online_step,(unsigned long)U32(0x4cccdc),menu_state!=WAIT_OBJECT_0);

    }

    /* The loader itself sleeps waiting for main-thread ANM uploads and for

       the audio queue (443153). Keep those services alive without running

       scene callbacks, sampling gameplay, checking state, or reseeding RNG. */

    U32(0x4cd624)&=~0x180u;

    online_assets_ready();

    ((int (TC *)(void *))0x476d20)((void *)0x56ad80);

    if(GetTickCount()-online_load_started>120000)online_fail(18);

    return 0;

}

static int online_ready_instance(void) {

    if(online_map->reserved[0])return 1;

    if(online_map->reserved[1]&1) {

        /* The verified WinMain stores its own "th18 App" mutex here. Run

           on the owning game thread, after startup's final handle check. */

        HANDLE own=(HANDLE)PTR(0x570c98);

        if(!own || !ReleaseMutex(own)){online_fail(14);return 0;}

        if(!CloseHandle(own)){online_fail(15);return 0;}

        PTR(0x570c98)=NULL;

        HWND window=(HWND)(uintptr_t)U32(0x568c30);

        if(window) {

            wchar_t title[80];swprintf(title,80,L"东方虹龙洞 P%lu - 本机%ls端测试",(unsigned long)online_map->role+1,party_count()==3?L"三":L"双");SetWindowTextW(window,title);

            RECT bounds,work;

            if(GetWindowRect(window,&bounds) && SystemParametersInfoW(SPI_GETWORKAREA,0,&work,0)) {

                int width=bounds.right-bounds.left,height=bounds.bottom-bounds.top;

                SetWindowPos(window,NULL,online_map->role?work.right-width:work.left,

                    work.top+((work.bottom-work.top)>height?((work.bottom-work.top)-height)/2:0),0,0,

                    SWP_NOACTIVATE|SWP_NOSIZE|SWP_NOZORDER);

            }

        }

        online_map->reserved[0]=3;

        log_line("ONLINE local dual-instance: released only this process's startup mutex");

    } else online_map->reserved[0]=1;

    MemoryBarrier();return 1;

}

static uint32_t online_duo_keys(void) {

    DWORD foreground=0;GetWindowThreadProcessId(GetForegroundWindow(),&foreground);

    if(foreground!=online_map->pid && (!online_map->reserved[2] || foreground!=online_map->reserved[2]))return 0;

    if(online_map->role)return read_p2_keys(GetAsyncKeyState);

    const struct {int key;uint32_t mask;} keys[]={

        {VK_UP,0x10},{VK_DOWN,0x20},{VK_LEFT,0x40},{VK_RIGHT,0x80},

        {'Z',1},{'X',2},{VK_SHIFT,8},{'C',0x400},{'D',0x800},{VK_ESCAPE,0x100},{VK_RETURN,0x80000}};

    uint32_t held=0;

    for(unsigned i=0;i<sizeof(keys)/sizeof(keys[0]);++i)if(GetAsyncKeyState(keys[i].key)&0x8000)held|=keys[i].mask;

    return held;

}

static void online_submit(unsigned kind) {

    online_map->step=online_step;online_map->kind=kind;

    /* Both profiles were verified during transfer. Before the first admitted

       frame, do not hash/reset transient title ages or RNG from warmup. */

    online_map->digest=online_boot_pending?COOP_ONLINE_MAGIC:online_digest();online_map->result=online_result;

    if(online_boot_pending) {

        online_map->rng[0]=online_map->initial_seed&0xffff;online_map->rng[1]=0;

        online_map->rng[2]=(online_map->initial_seed>>16)&0xffff;online_map->rng[3]=0;

    } else memcpy(online_map->rng,(void *)0x4cf280,16);

    online_map->options=U32(0x4cd014)&0x200;

    online_pending=1;online_deadline=GetTickCount();

    InterlockedExchange(&online_map->status,2);SetEvent(online_request);

}

static int online_take(void) {

    DWORD wait_started=GetTickCount();

    while(online_map->status==2) {

        DWORD elapsed=GetTickCount()-wait_started;if(elapsed>=20)break;

        /* An already-observed reply can leave its event signalled. Treat

           wakeups as hints and keep waiting within this same budget, rather

           than spending a render interval on a stale notification. */

        WaitForSingleObject(online_reply,20-elapsed);

    }

    if(online_map->status==4 || online_map->status==5)return -1;

    if(online_map->status!=3) {

        if(GetTickCount()-online_deadline>90000)online_fail(1);

        return 0;

    }

    MemoryBarrier();online_pending=0;

    if(online_map->step!=online_step || online_map->kind!=(uint32_t)(online_phase+1)){online_fail(2);return -1;}

    InterlockedExchange(&online_map->status,1);

    /* Wake the bridge's acknowledgement wait immediately. Request and ACK

       share a condition-change event; status/step/kind remain authoritative. */

    SetEvent(online_request);return 1;

}

static int online_has_game_thread(void *scheduler) {

    if(!valid(scheduler,0x58))return 0;

    void *node=(void *)(uintptr_t)FIELD(scheduler,0x18);

    for(unsigned count=0;count<512 && valid(node,8);++count) {

        void *callback=(void *)(uintptr_t)FIELD(node,0);

        if(valid(callback,0x28) && FIELD(callback,8)==0x453460)return 1;

        node=(void *)(uintptr_t)FIELD(node,4);

    }

    return 0;

}

#include "native_present_history.h"

#include "native_rollback_runtime.h"
#include "native_delay_runtime.h"

static int online_scheduler_step(void *scheduler) {

    rb_audio_diagnostic(online_step);

    if(rb_pipe && rb_pipe->end_request){dl_teardown();rb_live_teardown(scheduler);return 0;}

    /* The application exits on a root scheduler result of 0 (normal quit)

       or -1 (error), at 0x471b58, 0x472e9c and 0x472ff5. An idle network

       pass is a live application iteration: return 1 without ticking. */

    if(online_map->status==4 || online_map->status==5){if(!rb_runtime_enabled)online_load_ready();return 1;}

    if(!online_has_game_thread(scheduler))return rp_native_root(scheduler);

    if(!online_ready_instance())return 1;

    /* A pending post reply belongs to an already completed native frame.

       Waiting here never exposes half-initialized scene callbacks to Draw. */

    if(online_phase) {

        if(!online_pending) {

            if(!online_load_ready() || !online_assets_ready())return 1;

            online_submit(2);

        }

        if(online_take()!=1)return 1;

        online_phase=0;++online_step;online_map->completed=online_step;

        /* This is the previous iteration's completed frame. Admit the next

           frame now instead of spending a whole present interval on an ACK.

           No native callback has run in this scheduler invocation yet. */

    }

    if(dl_enabled() && online_step>=1)return dl_step(scheduler);
    if(rb_runtime_enabled && online_step>=1)return rb_live_step(scheduler);

    if(!online_started) {

        void *menu=PTR(0x4cf43c);

        if(!valid(menu,0x5d90) || FIELD(menu,0x18)!=1) return rp_native_root(scheduler);

        if(!online_assets_ready())return 1;

        online_started=online_boot_pending=1;

        log_line("ONLINE title barrier reached; role=P%lu",(unsigned long)online_map->role+1);

    }

    if(!online_phase) {

        if(!online_pending) {

            if(!online_load_ready() || !online_assets_ready())return 1;

            unsigned char saved[0x234];memcpy(saved,(void *)0x4ca210,sizeof(saved));

            online_sampling=1;online_raw=0;((void (__cdecl *)(void))0x401c50)();online_sampling=0;

            memcpy((void *)0x4ca210,saved,sizeof(saved));

#ifndef COOP_NATIVE_FIXTURE

            if((online_map->reserved[1]&3)==1)online_raw=online_duo_keys();

            else if(GetForegroundWindow()!=(HWND)(uintptr_t)U32(0x568c30))online_raw=0;

#endif

            /* Explicit isolated test only. The normal online path cannot opt

               into this input source through a peer message. */

            if((online_map->reserved[1]&1) && online_map->reserved_tail[1]==0x54455354u)

                online_raw=online_map->reserved_tail[0]&0x80dfbu;

            online_map->local_input=online_raw;

            online_submit(1);

        }

        if(online_take()!=1)return 1;

        if(online_boot_pending) {

            /* Reset the title only after admission, immediately followed by

               the whole native callback chain, never on a wait/render pass. */

            void *menu=PTR(0x4cf43c);

            if(!valid(menu,0x5d90)){online_fail(16);return 1;}

            ((void (TC *)(void *,int))0x4646e0)(menu,1);

            FIELD(menu,0x24)=FIELD(menu,0xb0)=FIELD(menu,0x5c48)=0;

            /* +5c4c bit0 is the native pending title-BGM start. 46507b

               consumes it after30 ticks; clearing it cancels music forever.

               Preserve the native request while resetting its local timer. */

            U32(0x4cccd4)=0;U32(0x4cccc8)=0;U32(0x4cf438)=1;

            memset((void *)0x4ca210,0,0x234);online_previous=online_previous_p2=online_previous_p3=0;

            requested=wanted=1;online_boot_pending=0;

        }

        online_keys[0]=online_map->p1;online_keys[1]=online_map->p2;online_keys[2]=online_map->p3;

        memcpy((void *)0x4cf280,online_map->rng,16);

        U32(0x4cd014)=(U32(0x4cd014)&~0x200u)|(online_map->options&0x200);

        /* Native focus-loss auto-pause must not diverge between machines. */

        U32(0x4cd624)&=~0x180u;

        if(rb_audit.enabled){rb_probe_begin(0,online_keys[1]);rb_audit.phase=1;rb_audit.main_thread=GetCurrentThreadId();++rb_audit.epoch;}

        if(rb_probe.active){rb_probe_add(&online_previous,sizeof(online_previous),UINT32_MAX,0);rb_probe_add(&online_previous_p2,sizeof(online_previous_p2),UINT32_MAX,0);rb_probe_add(&online_previous_p3,sizeof(online_previous_p3),UINT32_MAX,0);}

        int presented_history=rb_present_begin();

        if(presented_history<0){online_fail(20);return 1;}

        rb_tx_begin();

        int native_result=rp_native_root(scheduler);

        if(rb_control_active && (!p1 || !ship(1) || U32(0x4cd894))){rb_control_active=0;rb_graphics_rewind();rb_graphics_release();log_line("RB_CONTROL returned Draw ownership at scene boundary");}

        if(rb_control_requested && p1 && ship(1) && (party_count()==2 || ship(2)) && !scope_depth && !U32(0x4cd894)){

            if(!rb_control_active){if(!rb_graphics_begin()){online_fail(19);return 1;}rb_control_active=1;log_line("RB_CONTROL native tick/draw ownership enabled");}

            if(!rb_graphics_draw() || !rb_graphics_leave_output()){online_fail(19);return 1;}

        }

        rb_tx_seal();

        if(presented_history && !rb_present_end(scheduler,(Tick)online_tick_original,native_result)){online_fail(20);return 1;}

        if(rb_audit.enabled){rb_audit.phase=0;rb_probe_finish_step();rb_audit_dump();}

        if(rb_probe_replay(scheduler,(Tick)online_tick_original,native_result,online_keys)<0){rb_graphics_rewind();rb_graphics_release();online_fail(18);return 1;}

        if(rb_graphics.active && !rb_control_active){rb_graphics_rewind();rb_graphics_release();}

        /* The scheduler returns a callback count, not the scene node's 1/4

           status. Compare only completion category, preserve its exact return. */

        online_result=native_result<0?2u:native_result==0?0u:1u;

        online_phase=1;

        if(online_load_ready() && online_assets_ready()) {

            online_submit(2);

            if(online_take()==1){online_phase=0;++online_step;online_map->completed=online_step;}

        }

        return native_result;

    }

    return 1;

}

static int TC online_tick(void *scheduler) {
    int replay_wait=rp_root_wait(scheduler);if(replay_wait)return replay_wait<0?0:1;

    if(!online_map || scheduler!=PTR(0x4cf294))return rp_native_root(scheduler);

    /* The application's teardown sets +0x54 before dispatching cleanup at

       0x471fb8. Never freeze or network this local destruction pass. */

    if(FIELD(scheduler,0x54)){dl_teardown();rb_live_teardown(scheduler);return rp_native_root(scheduler);}

    if(online_inside_scheduler)return 1;

    online_inside_scheduler=1;int result=online_scheduler_step(scheduler);online_inside_scheduler=0;return result;

}

static LONG online_exception_logging;

static LONG CALLBACK online_exception(EXCEPTION_POINTERS *error) {

    DWORD code=error->ExceptionRecord->ExceptionCode;

    if(code!=EXCEPTION_ACCESS_VIOLATION && code!=EXCEPTION_ILLEGAL_INSTRUCTION && code!=EXCEPTION_INT_DIVIDE_BY_ZERO)return EXCEPTION_CONTINUE_SEARCH;

    if(InterlockedCompareExchange(&online_exception_logging,1,0))return EXCEPTION_CONTINUE_SEARCH;

    uintptr_t ip=error->ContextRecord->Eip;MEMORY_BASIC_INFORMATION region;wchar_t path[MAX_PATH];char name[4*MAX_PATH]="unknown";

    uintptr_t base=0;

    if(VirtualQuery((void *)ip,&region,sizeof(region))) {

        base=(uintptr_t)region.AllocationBase;

        if(GetModuleFileNameW((HMODULE)base,path,MAX_PATH)) {

            wchar_t *leaf=wcsrchr(path,L'\\');if(!leaf)leaf=path;else ++leaf;

            WideCharToMultiByte(CP_UTF8,0,leaf,-1,name,sizeof(name),NULL,NULL);

        }

    }

    log_line("ONLINE EXCEPTION code=%08lx module=%s ip=%p rva=%lx address=%p step=%lu phase=%d",

        (unsigned long)code,name,(void *)ip,(unsigned long)(ip-base),

        (void *)(error->ExceptionRecord->NumberParameters>=2?error->ExceptionRecord->ExceptionInformation[1]:0),

        (unsigned long)online_step,online_phase);

    log_line("ONLINE registers eax=%08lx ecx=%08lx edx=%08lx esp=%08lx ebp=%08lx",

        (unsigned long)error->ContextRecord->Eax,(unsigned long)error->ContextRecord->Ecx,(unsigned long)error->ContextRecord->Edx,

        (unsigned long)error->ContextRecord->Esp,(unsigned long)error->ContextRecord->Ebp);

    InterlockedExchange(&online_exception_logging,0);return EXCEPTION_CONTINUE_SEARCH;

}

static int online_initialise(void) {

    wchar_t token[40],name[100];DWORD n=GetEnvironmentVariableW(L"TH18_COOP_ONLINE",token,40);

    if(!n)return 0;

    if(n!=32)return -1;

    for(unsigned i=0;i<32;++i)if(!((token[i]>=L'0' && token[i]<=L'9')||(token[i]>=L'a' && token[i]<=L'f')))return -1;

    swprintf(name,100,L"Local\\TH18CoopOnline-%ls",token);

    online_mapping=OpenFileMappingW(FILE_MAP_ALL_ACCESS,FALSE,name);if(!online_mapping)return -1;

    online_map=MapViewOfFile(online_mapping,FILE_MAP_ALL_ACCESS,0,0,sizeof(*online_map));

    if(!online_map || online_map->magic!=COOP_ONLINE_MAGIC || online_map->schema!=2 || online_map->players<2 || online_map->players>3 || online_map->role>=online_map->players)return -1;

    if((online_map->reserved[1]&48u) && !rb_pipe_open(token))return -1;

    swprintf(name,100,L"Local\\TH18CoopOnline-%ls-request",token);online_request=OpenEventW(EVENT_MODIFY_STATE,FALSE,name);

    swprintf(name,100,L"Local\\TH18CoopOnline-%ls-reply",token);online_reply=OpenEventW(SYNCHRONIZE,FALSE,name);

    if(!online_request || !online_reply)return -1;

    online_map->pid=GetCurrentProcessId();InterlockedExchange(&online_map->status,1);

    AddVectoredExceptionHandler(1,online_exception);return 1;

}

#endif

