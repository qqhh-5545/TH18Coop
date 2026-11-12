#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "coop_api.h"
#include "data_profile.h"
#include "frame_input.h"
#include "online_ipc.h"

#define TC __attribute__((thiscall))
#define U32(a) (*(volatile uint32_t *)(uintptr_t)(a))
#define PTR(a) (*(void **)(uintptr_t)(a))
#define OFF(p,o) ((unsigned char *)(p)+(o))
#define FIELD(p,o) (*(uint32_t *)OFF(p,o))
#define FLT(p,o) (*(float *)OFF(p,o))

typedef int (TC *Tick)(void *);
static CoopOnlineMap *online_map;
/* A scene worker can read the same native globals used by guest Draw scopes.
   Its busy flag clears before the thread finishes, so check both signals. */
static int scene_construction_pending(void) {
    if(U32(0x4cd894))return 1;
    HANDLE worker=(HANDLE)PTR(0x4cd888);
    return worker && WaitForSingleObject(worker,0)!=WAIT_OBJECT_0;
}

static int rb_replaying;
static int rb_control_requested,rb_control_active,rb_control_drawing;
static int rb_runtime_enabled,rb_runtime_speculating,rb_runtime_virtual,rb_runtime_barrier;
static volatile DWORD rb_runtime_thread;
static int rb_runtime_audio(void *manager);
static int rb_runtime_reset_boundary(void);
static void *trampoline(uintptr_t address,unsigned length,const unsigned char *expected);
static int patch_branch(uintptr_t address,void *target,unsigned length,unsigned char opcode,const unsigned char *expected);
extern void rb_rect_zero_z_bridge(void);
static int rb_request_precise(void){
    /* External streaming threads are not replayed. Never turn their I/O
       into an artificial failure because the game thread is predicting. */
    if(rb_runtime_thread && rb_runtime_thread!=GetCurrentThreadId())return 0;
    if(!rb_runtime_speculating)return 0;rb_runtime_barrier=1;return 1;
}
static int online_poll(void *input);
static unsigned rp_players;
static unsigned rp_test_start_stage;
static int rp_viewer;
static int rp_guest_due(unsigned id),rp_menu(void *menu),rp_root_wait(void *scheduler),rp_install(void);
static void rp_guest_created(unsigned id);
static void rp_test_keys(void *input);
static int rp_native_root(void *scheduler);
static int rp_playing(void);
typedef void *( __cdecl *CreatePlayer)(void);
static void *p1;
static Tick p1_tick,p1_draw,item_tick;
/* Each guest owns its complete native singleton context. The active bank is
   selected on the game thread only and restored at every callback boundary. */
typedef struct {
    void *player,*bomb,*anm,*cards,*mukade;
    Tick tick,draw;
    uint32_t character,funds,stage_seen,previous_input;
    int selected_character,cards_mode,loadout[3],have_resources,cards_cloned;
    unsigned loadout_count;
    DWORD asset_started;
    unsigned char resources[0x30];
} GuestNative;
static GuestNative guests[2]={
    {.selected_character=-1,.loadout={-1,-1,-1},.loadout_count=2},
    {.selected_character=-1,.loadout={-1,-1,-1},.loadout_count=2}};
static unsigned guest_slot;
#define current_guest (guests[guest_slot].player)
#define p2_tick (guests[guest_slot].tick)
#define p2_draw (guests[guest_slot].draw)
#define guest_bomb (guests[guest_slot].bomb)
#define guest_anm (guests[guest_slot].anm)
#define guest_cards (guests[guest_slot].cards)
#define guest_mukade (guests[guest_slot].mukade)
#define p2_character (guests[guest_slot].character)
#define p2_funds (guests[guest_slot].funds)
#define p2_resources (guests[guest_slot].resources)
#define chosen_character (guests[guest_slot].selected_character)
#define custom_cards (guests[guest_slot].cards_mode)
#define chosen_cards (guests[guest_slot].loadout)
#define chosen_count (guests[guest_slot].loadout_count)
#define guest_asset_started (guests[guest_slot].asset_started)
#define have_guest_resources (guests[guest_slot].have_resources)
#define initial_cards_cloned (guests[guest_slot].cards_cloned)
#define last_stage (guests[guest_slot].stage_seen)
#define previous_p2 (guests[guest_slot].previous_input)
#define guest_seat (guest_slot+1)
#define guest_file_id (18+guest_slot)
static unsigned party_count(void) {return online_map && online_map->players==3?3:rp_viewer && rp_players==3?3:2;}
static void *ship(unsigned id) {return id?guests[id-1].player:p1;}
static void *host_cards;
static void *ability(unsigned id) {return id?guests[id-1].cards:host_cards;}
static int32_t *resource(unsigned id,unsigned offset) {return (int32_t *)(id?guests[id-1].resources+offset:(unsigned char *)0x4ccd38+offset);}
static unsigned bind_guest(void *object,unsigned kind) {
    unsigned old=guest_slot;
    for(unsigned i=0;i<party_count()-1;++i)if(object==(kind==0?guests[i].player:kind==1?guests[i].cards:guests[i].bomb)) {guest_slot=i;break;}
    return old;
}
static uint32_t observed_stage,departed_stage,departed_ghosts;
static Tick bomb_tick,bomb_draw;
static void *saved_bomb,*saved_anm9;
static uint32_t saved_character[2];
static unsigned char run_base_resources[0x30];
static uint32_t run_base_funds;
static int have_run_base;
static wchar_t settings_path[MAX_PATH];
static int bomb_owner,creating_guest;
static unsigned protected_bombs;
static void *hud_node,*dialogue_hud_node;
static unsigned char saved_resources[0x30];
static uint32_t saved_funds;
static void *saved_cards,*saved_mukade;
static int scope_depth,scope_has_input,shop_owner;
static unsigned shop_pending_owner;
static float shop_pending_position[3];
static uint32_t menu_previous;
static unsigned menu_age[4];
static void *menu_tick_original,*menu_draw_original,*menu_input_original;
static int native_draft=1,draft_owner,draft_ready;
static void *draft_menu,*draft_host,*draft_score,*draft_mukade;
static unsigned char draft_templates[0x8c],draft_resources[0x30],draft_history[0x90];
static uint32_t draft_character,draft_style,draft_funds,draft_input;
static uint32_t draft_host_flags[2];
static int draft_release;
typedef struct {
    int ghost,life_taps,life_window,power_taps,power_window,wait_release,deathbomb_extended,life_target;
    int sprite_file,sprite;
    uint32_t keys;
    uint32_t deathbomb_base;
    float dx,dy;
} PartnerState;
static PartnerState partner[3];
static unsigned wipe_frames;
static int8_t transfer_owner[0x1258];
static int transfer_power[0x1258];
static uint32_t item_power_before;
/* Written only at the native successful-pickup branch, never offscreen cleanup. */
volatile uint32_t coop_pickup_confirmed;
static void *cards_destroy_original,*cards_ui_original,*shop_init_original,*shop_close_original;
static void *shop_tick_original,*shop_draw_original;
static void *stamp_draw_original;
static void *scene_draw_original;
static int __cdecl scene_draw(void);
static void *anm_draw_original;
static void *pause_actions_original;
static unsigned draw_alpha[3]={255,255,255};
static int stamp_cache_guard=0;
static unsigned ghost_alpha=80;
static uint32_t saved_input[4];
static float spirit_draw_alpha[3];static int spirit_was_drawn[3];
static volatile LONG frames, requested, attached;
static CoopInputLatch input_latch;
static HANDLE input_trace=INVALID_HANDLE_VALUE;
static uint32_t input_trace_bytes;
static HANDLE activation;
static HMODULE module;
static wchar_t log_path[MAX_PATH];
static int wanted, team_over, item_scope;
static void *item_manager;
static int8_t item_owner[0x1258];
static uint32_t last_age[0x1258];
static unsigned long collected[3], guest_ticks;
static unsigned long damage_done[3], hits[3];
void *damage_original, *rect_original, *circle_original, *laser_original;
static void *player_create_original,*item_create_original,*spawn_original;
extern int __cdecl native_damage(const uint32_t *,float);
extern int __cdecl native_circle(const uint32_t *,float);
extern int __cdecl native_laser(const uint32_t *,float,float);
extern void damage_bridge(void), circle_bridge(void),laser_bridge(void);
extern void boss_damage_bridge(void);
extern void bomb_immunity_bridge(void),bomb_modifier_bridge(void);
extern void aim_spawn_bridge(void),aim_redirect1_bridge(void),aim_redirect2_bridge(void);
extern void aim_redirect3_bridge(void),aim_redirect4_bridge(void),aim_redirect5_bridge(void);
extern void aim_pattern_bridge(void),aim_tick_bridge(void),aim_laser_bridge(void);
extern void aim_ecl_x_bridge(void),aim_ecl_y_bridge(void),aim_ecl_fx_bridge(void),aim_ecl_fy_bridge(void);
extern void aim_ecl_distance_bridge(void),aim_ecl_fdistance_bridge(void),aim_ecl_side_bridge(void);
extern void aim_ecl_angle1_bridge(void),aim_ecl_angle2_bridge(void),aim_homing_bridge(void);
extern void aim_emit_distance_bridge(void),aim_laser_radial_bridge(void),aim_laser_curve_bridge(void);
extern void aim_laser_emit_bridge(void),aim_laser_spawn_bridge(void);
static void *scoped_item;
static uint32_t item_original_input;
static unsigned item_saved_slot;
extern void item_loop_bridge(void);
extern void item_pickup_bridge(void);
extern int TC coop_hud(void *);
static void prepare_guest_cards(void);
static void clone_initial_cards(void);
static void release_guest_cards(void);
static void read_cards(HudStats *stats,unsigned id,void *manager);
static void partner_tick(const uint32_t *keys);
static void prepare_player_tick(unsigned id);
static void revive(unsigned id);
static void revive_at_clear(void);
static void restore_draft_host(void);
extern void coop_menu_hud(int owner);
static void share_bomb_protection(void *bomb) {
    unsigned owner=0;
    for(unsigned id=1;id<party_count();++id)if(bomb==guests[id-1].bomb)owner=id;
    unsigned bit=1u<<owner;
    if(!FIELD(bomb,0x30)){protected_bombs&=~bit;return;}
    if(!ship(1) || (protected_bombs&bit))return;
    protected_bombs|=bit;
    /* One second on activation; do not renew for the entire native Bomb. */
    uint32_t frames=60;
    for(unsigned id=0;id<party_count();++id) {
        void *other=ship(id);if(id==owner || !other)continue;
        if(FIELD(other,0x476ac)==1 && FIELD(other,0x47778)<frames) {
            FIELD(other,0x47778)=frames;FLT(other,0x4777c)=(float)frames;FIELD(other,0x47774)=frames?frames-1:0;
        }
    }
}

int coop_view_player(void) {return online_map?(int)online_map->role:0;}
int coop_shared_keyboard(void) {return !online_map || (online_map->reserved[1]&3)==1;}
void coop_hud_stats(HudStats *stats) {
    memset(stats,0,sizeof(*stats));
    stats->local_player=coop_view_player();stats->shared_keyboard=coop_shared_keyboard();stats->player_count=(int)party_count();
    stats->shop_owner=PTR(0x4cf2a4)?shop_owner+1:0;
    for(unsigned id=0;id<party_count();++id) {
        stats->lives[id]=*resource(id,0x10);stats->bombs[id]=*resource(id,0x20);
        stats->power[id]=*resource(id,0);stats->funds[id]=id?(int)guests[id-1].funds:(int)U32(0x4ccd34);
        read_cards(stats,id,ability(id));
        void *player=ship(id);if(player){stats->x[id]=FLT(player,0x620);stats->y[id]=FLT(player,0x624);stats->state[id]=(int)FIELD(player,0x476ac);}
        stats->life_frag[id]=*resource(id,0x14);stats->bomb_frag[id]=*resource(id,0x24);
        stats->ghost[id]=partner[id].ghost;stats->life_taps[id]=partner[id].life_taps;stats->power_taps[id]=partner[id].power_taps;
        stats->sprite_file[id]=partner[id].sprite_file;stats->sprite[id]=partner[id].sprite;
    }
}

static void log_line(const char *format, ...) {
    if(rb_replaying)return;
    /* One append-only kernel write prevents concurrent endpoints from seeking
       to the same old EOF and overwriting one another's diagnostic lines. */
    char line[2048];int prefix=snprintf(line,sizeof(line),"[%lu PID=%lu] ",(unsigned long)GetTickCount(),(unsigned long)GetCurrentProcessId());
    if(prefix<0 || prefix>=(int)sizeof(line)-2)return;
    va_list args;va_start(args,format);int used=vsnprintf(line+prefix,sizeof(line)-prefix-1,format,args);va_end(args);
    if(used<0)return;size_t length=(size_t)prefix+(size_t)used;if(length>sizeof(line)-2)length=sizeof(line)-2;
    line[length++]='\n';HANDLE file=CreateFileW(log_path,FILE_APPEND_DATA,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
    if(file==INVALID_HANDLE_VALUE)return;DWORD written;WriteFile(file,line,(DWORD)length,&written,NULL);CloseHandle(file);
}

static int valid(void *p, SIZE_T n) {
    uintptr_t at=(uintptr_t)p,end=at+n;if(!at || end<at)return 0;
    /* A live heap object can span adjacent committed regions. RegionSize is
       a protection/commit run, not the owning allocation's object length. */
    do{MEMORY_BASIC_INFORMATION m;if(!VirtualQuery((void *)at,&m,sizeof(m)) || m.State!=MEM_COMMIT || (m.Protect&(PAGE_NOACCESS|PAGE_GUARD)))return 0;
        uintptr_t next=(uintptr_t)m.BaseAddress+m.RegionSize;if(next<=at)return 0;at=next;
    }while(at<end);return 1;
}

#include "native_heap_audit.h"

#include "key_bindings.h"

static const struct {int vk;uint32_t mask;} p2_bindings[]={
    {'I',0x10},{'K',0x20},{'J',0x40},{'L',0x80},
    {'G',1},{'F',8},{'H',2},{'U',0x400},{'O',0x800}};
static uint32_t read_p2_keys(SHORT (WINAPI *read_key)(int)) {
    return kb_sample(1,read_key);
}
static uint32_t sample_p2_local(void) {
    if (GetForegroundWindow() != (HWND)(uintptr_t)U32(0x568c30)) return 0;
    return read_p2_keys(GetAsyncKeyState);
}
static uint32_t p2_input(void) {
    return input_latch.valid?(guest_slot?input_latch.p3:input_latch.p2):guest_slot?0:sample_p2_local();
}
static void trace_input_record(void) {
    if(rb_runtime_virtual)return;
    if(rb_replaying)return;
    if(input_trace==INVALID_HANDLE_VALUE)return;
    uint32_t phase=UINT32_MAX,owner=0;void *menu=PTR(0x4cf43c);
    if(valid(menu,0x24)){phase=FIELD(menu,0x18);if(phase==6)owner=draft_owner?(unsigned)draft_owner:1;}
    CoopInputRecord record=coop_input_record(&input_latch,U32(0x4cccdc),phase,owner,U32(0x4cccc8));
    DWORD size;
    if(input_trace_bytes+sizeof(record)>COOP_TRACE_LIMIT ||
       !WriteFile(input_trace,&record,sizeof(record),&size,NULL) || size!=sizeof(record)) {
        CloseHandle(input_trace);input_trace=INVALID_HANDLE_VALUE;
        log_line("Input trace stopped (size cap or write error); local input remains active");return;
    }
    input_trace_bytes+=sizeof(record);
}
static void open_input_trace(void) {
    wchar_t path[MAX_PATH];SYSTEMTIME now;DWORD size;
    if(!GetModuleFileNameW(NULL,path,MAX_PATH))return;
    wchar_t *slash=wcsrchr(path,L'\\');if(!slash)return;
    size_t remaining=MAX_PATH-(size_t)(slash-path);
    if(swprintf(slash,remaining,L"\\coop-data\\logs")<0)return;
    CreateDirectoryW(path,NULL);size_t length=wcslen(path);GetLocalTime(&now);
    if(swprintf(path+length,MAX_PATH-length,L"\\input-%04u%02u%02u-%02u%02u%02u-%lu.th18input",
        now.wYear,now.wMonth,now.wDay,now.wHour,now.wMinute,now.wSecond,(unsigned long)GetCurrentProcessId())<0)return;
    input_trace=CreateFileW(path,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
    if(input_trace==INVALID_HANDLE_VALUE){log_line("Input trace unavailable; local input remains active");return;}
    CoopInputHeader header=coop_input_header(COOP_VERSION);
    if(!WriteFile(input_trace,&header,sizeof(header),&size,NULL) || size!=sizeof(header)) {
        CloseHandle(input_trace);input_trace=INVALID_HANDLE_VALUE;return;
    }
    input_trace_bytes=sizeof(header);log_line("Input trace enabled (raw input polls, not a playable replay), 16 MiB cap");
}

static void enter_p2(int input) {
    if(scope_depth++) return;
    scope_has_input=input;
    saved_character[0]=U32(0x4cccf4);saved_character[1]=U32(0x4cccf8);
    U32(0x4cccf4)=p2_character;U32(0x4cccf8)=0;
    saved_bomb=PTR(0x4cf2b8);PTR(0x4cf2b8)=guest_bomb;
    void *anm=PTR(0x51f65c);saved_anm9=anm?(void *)(uintptr_t)FIELD(anm,0x3120750):NULL;
    if(anm && guest_anm) FIELD(anm,0x3120750)=(uint32_t)(uintptr_t)guest_anm;
    saved_funds=U32(0x4ccd34);U32(0x4ccd34)=p2_funds;
    saved_cards=PTR(0x4cf298);if(guest_cards) PTR(0x4cf298)=guest_cards;
    saved_mukade=PTR(0x4cf2d4);PTR(0x4cf2d4)=guest_mukade;
    memcpy(saved_resources, (void *)0x4ccd38, sizeof(saved_resources));
    memcpy((void *)0x4ccd38, p2_resources, sizeof(p2_resources));
    PTR(0x4cf410)=current_guest;
    if (input) {
        memcpy(saved_input, (void *)0x4ca428, sizeof(saved_input));
        uint32_t keys=p2_input();
        U32(0x4ca428)=keys;
        U32(0x4ca42c)=previous_p2;
        U32(0x4ca430)=keys;
        U32(0x4ca434)=keys & ~previous_p2;
        previous_p2=keys;
    }
}

static void leave_p2(int input) {
    (void)input;
    if(--scope_depth) return;
    p2_funds=U32(0x4ccd34);U32(0x4ccd34)=saved_funds;
    PTR(0x4cf298)=saved_cards;
    guest_mukade=PTR(0x4cf2d4);PTR(0x4cf2d4)=saved_mukade;
    memcpy(p2_resources, (void *)0x4ccd38, sizeof(p2_resources));
    memcpy((void *)0x4ccd38, saved_resources, sizeof(saved_resources));
    if (scope_has_input) memcpy((void *)0x4ca428, saved_input, sizeof(saved_input));
    U32(0x4cccf4)=saved_character[0];U32(0x4cccf8)=saved_character[1];
    PTR(0x4cf2b8)=saved_bomb;
    void *anm=PTR(0x51f65c);if(anm && guest_anm) FIELD(anm,0x3120750)=(uint32_t)(uintptr_t)saved_anm9;
    PTR(0x4cf410)=p1;
}

static void read_cards(HudStats *stats,unsigned id,void *manager) {
    stats->card_count[id]=0;
    if(!manager) return;
    void *node=(void *)(uintptr_t)FIELD(manager,0x1c);
    while(node && stats->card_count[id]<COOP_HUD_CARDS) {
        void *card=(void *)(uintptr_t)FIELD(node,0);
        if(!card) break;
        HudCard *out=&stats->cards[id][stats->card_count[id]++];
        out->id=(int)FIELD(card,4);
        out->selected=FIELD(manager,0x38)==(uint32_t)(uintptr_t)card;
        /* Native 408890 reads _recharge_timer.current at +38, not the
           separate activation timer at +24. */
        out->remaining=(int32_t)FIELD(card,0x38);
        out->duration=(int32_t)FIELD(card,0x48);
        uint32_t flags=FIELD(card,0x50);out->category=(flags&8)?0:(flags&0x40)?1:2;
        /* 408de0 advances this array for EVERY list node, even when its
           category is skipped. It is inventory order, not category order. */
        unsigned slot=(unsigned)stats->card_count[id]-1;
        out->native_ui=slot<256?FIELD(manager,0x58+slot*4):0;
        node=(void *)(uintptr_t)FIELD(node,4);
    }
}

static void ensure_guest_active_ui(void) {
    /* Reproduce only the active HUD root in native 407f10. Do not call its
       inventory reset or the file-wide UI rebuild. Both endpoints create
       the same VM during logic, independently of the local viewing seat. */
    void *anm=PTR(0x51f65c);
    if(!anm || !guest_cards)return;
    uint32_t id=FIELD(guest_cards,0x3c);
    if(cards_ui_original && FIELD(guest_cards,0x28) && (!FIELD(guest_cards,0x58) || !((void *(TC *)(void *,uint32_t))0x488b40)(anm,FIELD(guest_cards,0x58))))
        ((void (TC *)(void *,int))cards_ui_original)(guest_cards,0);
    if(id && ((void *(TC *)(void *,uint32_t))0x488b40)(anm,id))return;
    FIELD(guest_cards,0x3c)=0;
    void *file=(void *)(uintptr_t)FIELD(guest_cards,0x10);
    if(!file)return;
    void *vm=((CreatePlayer)0x489320)();
    if(!vm)return;
    ++FIELD(file,0x134);
    ((void *(TC *)(void *,void *,int))0x407420)(file,vm,0);
    FIELD(vm,0x538)|=0x1000;
    FLT(vm,0x5f0)=FLT(vm,0x5f4)=FLT(vm,0x5f8)=0;
    FIELD(vm,0x44)=0;
    ((Tick)0x478580)(vm);
    FIELD(vm,0x520)=0;
    ((void *(__stdcall *)(uint32_t *,void *))0x488350)(&id,vm);
    FIELD(guest_cards,0x3c)=id;
    ((void (TC *)(void *))0x408c30)(guest_cards);
    log_line("P%u native active-card HUD root=%lu",guest_seat+1,(unsigned long)id);
}

static int TC guest_cards_tick(void *manager) {
    unsigned old=bind_guest(manager,1);
    if(draft_owner==(int)guest_seat+1 && manager==guest_cards) {int r=((Tick)0x408a90)(manager);guest_slot=old;return r;}
    if(!current_guest || manager!=guest_cards || PTR(0x4cf410)!=p1) {guest_slot=old;return 1;}
    void *shot=(void *)(uintptr_t)FIELD(current_guest,0x47940);
    if(shot) FIELD(current_guest,0x47984)=FIELD(shot,0x28);
    enter_p2(0);ensure_guest_active_ui();int result=((Tick)0x408a90)(manager);leave_p2(0);guest_slot=old;return result;
}

static int TC guest_cards_draw(void *manager) {
    if(scene_construction_pending())return 1;
    unsigned old=bind_guest(manager,1);
    if(draft_owner==(int)guest_seat+1 && manager==guest_cards) {int r=((Tick)0x408ab0)(manager);guest_slot=old;return r;}
    if(!current_guest || manager!=guest_cards || PTR(0x4cf410)!=p1) {guest_slot=old;return 1;}
    enter_p2(0);int result=((Tick)0x408ab0)(manager);leave_p2(0);guest_slot=old;return result;
}

typedef struct {void *vm;uint32_t flags;} HiddenUiVm;
static void *stage_dialogue(void) {
    void *gui=PTR(0x4cf2e0);
    return gui?(void *)(uintptr_t)FIELD(gui,0x1b0):NULL;
}
static int TC draw_dialogue_hud(void *player) {
    return stage_dialogue()?coop_hud(player):1;
}
static int TC draw_late_hud(void *player) {
    int result=stage_dialogue()?1:coop_hud(player);
    coop_hud_capture();
    return result;
}
static int is_dialogue_portrait(void *vm) {
    void *dialogue=stage_dialogue();if(!dialogue)return 0;
    for(unsigned depth=0;depth<8 && vm;++depth) {
        uint32_t id=FIELD(vm,0x544);
        if(id)for(unsigned slot=0;slot<8;++slot)
            if(FIELD(dialogue,0x40+slot*4)==id)return 1;
        vm=(void *)(uintptr_t)FIELD(vm,0x5bc);
    }
    return 0;
}
static unsigned teammate_alpha(unsigned id);
static void hide_card_vm(void *vm,HiddenUiVm *saved,unsigned *count) {
    if(*count>=64 || !valid(vm,0x59c))return;
    for(unsigned i=0;i<*count;++i)if(saved[i].vm==vm)return;
    saved[*count]=(HiddenUiVm){vm,FIELD(vm,0x534)};++*count;
    FIELD(vm,0x534)&=~2u; /* Native 4094c0 hides this HUD tree with the same bit. */
    void *node=(void *)(uintptr_t)FIELD(vm,0x598);
    for(unsigned i=0;i<64 && valid(node,8) && *count<64;++i) {
        hide_card_vm((void *)(uintptr_t)FIELD(node,0),saved,count);
        node=(void *)(uintptr_t)FIELD(node,4);
    }
}
static int __cdecl scene_draw(void) {
    if(scene_construction_pending())return 1;
    if(rb_control_active && !rb_control_drawing){if(!rb_graphics_blit()){rb_graphics.failed=1;log_line("RB_CONTROL cached frame blit failed");}return 1;}
    LONG saved_audit_phase=rb_audit.phase;if(rb_audit.enabled)rb_audit.phase=2;
    if(rb_audit.enabled)rb_probe_begin(1,0);
    HiddenUiVm saved[64];unsigned count=0;
    for(unsigned id=0;id<party_count();++id)draw_alpha[id]=teammate_alpha(id);
    if(ship(1) && PTR(0x51f65c))for(unsigned id=0;id<party_count();++id) {
        void *manager=ability(id);if(!manager || id==(unsigned)coop_view_player())continue;
        uint32_t root=FIELD(manager,0x3c);
        if(root)hide_card_vm(((void *(TC *)(void *,uint32_t))0x488b40)(PTR(0x51f65c),root),saved,&count);
    }
    uint32_t draw_rng[4];int audit_rng=online_map && (online_map->reserved[1]&1) && !U32(0x4cd894);memcpy(draw_rng,(void *)0x4cf280,16);
    int result=((int (__cdecl *)(void))scene_draw_original)();
    static unsigned draw_rng_changes;
    if(audit_rng && !U32(0x4cd894) && memcmp(draw_rng,(void *)0x4cf280,16) && draw_rng_changes++<32)log_line("DRAW_RNG_CHANGE stage=%lu step=%lu before=%08lx,%08lx,%08lx,%08lx after=%08lx,%08lx,%08lx,%08lx",(unsigned long)U32(0x4cccdc),(unsigned long)online_map->completed,(unsigned long)draw_rng[0],(unsigned long)draw_rng[1],(unsigned long)draw_rng[2],(unsigned long)draw_rng[3],(unsigned long)U32(0x4cf280),(unsigned long)U32(0x4cf284),(unsigned long)U32(0x4cf288),(unsigned long)U32(0x4cf28c));
    while(count){--count;void *vm=saved[count].vm;FIELD(vm,0x534)=(FIELD(vm,0x534)&~2u)|(saved[count].flags&2u);}
    if(rb_audit.enabled && rb_probe.serial>=100 && rb_probe.active)rb_probe_end();
    rb_audit.phase=saved_audit_phase;return result;
}
static unsigned teammate_alpha(unsigned id) {
    unsigned local=(unsigned)coop_view_player();void *other=ship(id),*own=ship(local);
    if(!p1 || !ship(1) || !other || !own || id==local || partner[id].ghost)return 255;
    if(FIELD(other,0x476ac)==2)return 255;
    if(partner[local].ghost || FIELD(own,0x476ac)==2)return 128;
    float dx=FLT(other,0x620)-FLT(own,0x620),dy=FLT(other,0x624)-FLT(own,0x624),distance=sqrtf(dx*dx+dy*dy);
    if(distance>=100)return 128;if(distance<50)distance=50;
    unsigned alpha=(unsigned)((distance-50)*(255-51)/50)+51;return alpha>128?128:alpha;
}
static float vm_coordinate_unit(void *vm) {
    unsigned mode=FIELD(vm,0x538)&0x1c00000;
    float scale=*(float *)0x56aca0;
    if(mode==(1u<<22) || mode==0xc00000)return scale;
    if(mode==0x800000 || mode==0x1000000)return scale*.5f;
    return 1;
}
static int TC anm_draw(void *manager,void *vm) {
    uint32_t primary=0,secondary=0;int tinted=0;
    /* The native caller already owns a live VM. VirtualQuery here used to
       perform a kernel query for every bullet/effect, including nonplayers. */
    if(current_guest && vm) {
        uint32_t file=FIELD(vm,0x1c);
        if(file==31) {
            void *root=vm;
            for(unsigned depth=0;depth<8 && root;++depth) {
                if(FIELD(root,0x28)==5)return -1;
                root=(void *)(uintptr_t)FIELD(root,0x5bc);
            }
        }
        if((file==9 || file==18 || file==19) && !is_dialogue_portrait(vm)) {
            unsigned alpha=draw_alpha[file==9?0:file-17];
            if(alpha<255) {
                primary=FIELD(vm,0x524);secondary=FIELD(vm,0x528);tinted=1;
                unsigned a=primary>>24,b=secondary>>24;
                if(a>alpha)FIELD(vm,0x524)=(primary&0xffffff)|(alpha<<24);
                if(b>alpha)FIELD(vm,0x528)=(secondary&0xffffff)|(alpha<<24);
            }
        }
    }
    int result=((int (TC *)(void *,void *))anm_draw_original)(manager,vm);
    if(tinted){FIELD(vm,0x524)=primary;FIELD(vm,0x528)=secondary;}
    return result;
}

typedef struct {void *vm;unsigned ordinal;} CardVmRef;
static void collect_card_tree(void *root,CardVmRef *nodes,unsigned *count,unsigned depth) {
    if(!root || *count>=24 || depth>6)return;
    for(unsigned i=0;i<*count;++i)if(nodes[i].vm==root)return;
    nodes[*count]=(CardVmRef){root,*count};++*count;
    void *node=(void *)(uintptr_t)FIELD(root,0x598);
    for(unsigned n=0;node && n<24 && *count<24;++n) {
        collect_card_tree((void *)(uintptr_t)FIELD(node,0),nodes,count,depth+1);
        node=(void *)(uintptr_t)FIELD(node,4);
    }
}
static void card_vm_units(void *vm,float *sx,float *sy) {
    *sx=FLT(vm,0x54)*FLT(vm,0x5c);*sy=FLT(vm,0x58)*FLT(vm,0x60);
    void *parent=(void *)(uintptr_t)FIELD(vm,0x5c0);
    if(parent && !(FIELD(vm,0x538)&0x40000)){*sx*=FLT(parent,0x54)*FLT(parent,0x5c);*sy*=FLT(parent,0x58)*FLT(parent,0x60);}
    float unit=vm_coordinate_unit(vm);
    *sx*=unit;*sy*=unit;
}
unsigned coop_hud_card_quads(const HudCard *card,float x,float y,float width,float height,HudNativeQuad *out,unsigned capacity) {
    void *manager=PTR(0x51f65c);if(!manager || !card->native_ui || !capacity)return 0;
    void *root=((void *(TC *)(void *,uint32_t))0x488b40)(manager,card->native_ui);
    if(!root || FIELD(root,0x1c)!=31 || FIELD(root,0x28)!=5)return 0;
    CardVmRef refs[24];unsigned count=0;collect_card_tree(root,refs,&count,0);
    float origin[3];((float *(TC *)(void *,float *))0x4065a0)(root,origin);
    float original_width=32*(*(float *)0x56aca0);
    for(unsigned i=0;i<count;++i)if(FIELD(refs[i].vm,0x28)==6) {
        float sx,sy;card_vm_units(refs[i].vm,&sx,&sy);float measured=fabsf(FLT(refs[i].vm,0x74)*sx);
        if(measured>.001f)original_width=measured;break;
    }
    if(original_width<=.001f)return 0;
    float factor=width/original_width;unsigned written=0;
    for(unsigned i=1;i<count;++i) {
        CardVmRef value=refs[i];unsigned j=i;
        while(j && (int32_t)(FIELD(refs[j-1].vm,0x544)-FIELD(value.vm,0x544))>0){refs[j]=refs[j-1];--j;}
        refs[j]=value;
    }
    for(unsigned i=0;i<count && written<capacity;++i) {
        void *vm=refs[i].vm;uint32_t flags=FIELD(vm,0x534),extra=FIELD(vm,0x538),color=FIELD(vm,0x524);
        if((flags&3)!=3 || (extra&0x180) || !(color>>24))continue;
        /* Read native ANM-evaluated geometry, UV crop, tint, selection flash
           and rotation. Rendering is batched with the HUD state block so it
           cannot corrupt native renderer texture/transform caches. No tick,
           interrupt, gameplay owner swap or original VM writes occur here. */
        float position[3];((float *(TC *)(void *,float *))0x4065a0)(vm,position);
        float angle=((float *(TC *)(void *))0x4097d0)(vm)[2],sx,sy;card_vm_units(vm,&sx,&sy);
        float w=FLT(vm,0x74)*sx*factor,h=FLT(vm,0x78)*sy*factor;
        if(!isfinite(w) || !isfinite(h) || fabsf(w)<.0001f || fabsf(h)<.0001f)continue;
        float cx=x+width*.5f+(position[0]-origin[0])*factor,cy=y+height*.5f+(position[1]-origin[1])*factor;
        float cosine=cosf(angle),sine=sinf(angle);HudNativeQuad *q=&out[written++];
        q->file=(int)FIELD(vm,0x1c);q->sprite=(int)FIELD(vm,0x24);q->color=color;q->additive=(flags&0x20)!=0;
        for(unsigned corner=0;corner<4;++corner) {
            float ox=(corner&1?w:-w)*.5f,oy=(corner&2?h:-h)*.5f;
            q->x[corner]=cx+ox*cosine-oy*sine;q->y[corner]=cy+ox*sine+oy*cosine;
            float u=FLT(vm,0x3ac+corner*8),v=FLT(vm,0x3b0+corner*8);
            if(flags&0x18){u=FLT(vm,0x3ac)+(u-FLT(vm,0x3ac))*FLT(vm,0x6c);v=FLT(vm,0x3b0)+(v-FLT(vm,0x3b0))*FLT(vm,0x70);}
            q->u[corner]=u+FLT(vm,0x7c);q->v[corner]=v+FLT(vm,0x80);
        }
    }
    return written;
}
void coop_hud_card_flush(void) {
    if(PTR(0x51f65c))((void (TC *)(void *))0x47e730)(PTR(0x51f65c));
}
int coop_hud_capture_path(wchar_t *path,unsigned length) {
    static uint32_t previous;
    if(rb_replaying || !online_map || !(online_map->reserved[1]&1))return 0;
    uint32_t serial=online_map->reserved_tail[2];if(!serial || serial==previous || serial>999)return 0;
    previous=serial;
    if(!GetModuleFileNameW(module,path,length))return 0;
    wchar_t *name=wcsrchr(path,L'\\');if(!name)return 0;
    return swprintf(name+1,length-(unsigned)(name+1-path),L"ui-capture-P%lu-%lu.bmp",(unsigned long)online_map->role+1,(unsigned long)serial)>0;
}

static void prepare_guest_cards(void) {
    host_cards=PTR(0x4cf298);
    if(guest_cards || !host_cards) return;
    /* Independent native card instances; ANM files and card descriptions are
       immutable shared assets, whose ownership stays with the host manager. */
    guest_cards=calloc(1,0xd70);
    if(!guest_cards) return;
    FIELD(guest_cards,0)=2;
    memcpy(OFF(guest_cards,0xc),OFF(host_cards,0xc),12);
    FIELD(guest_cards,0x18)=(uint32_t)(uintptr_t)guest_cards;
    FLT(guest_cards,0xc58)=1.0f;FIELD(guest_cards,0xc60)=1;
    FIELD(guest_cards,0xc5c)=FIELD(host_cards,0xc5c);
    FIELD(guest_cards,0xc68)=FIELD(host_cards,0xc68);
    typedef void *(__stdcall *NewNode)(void *);
    typedef int (__stdcall *AddNode)(void *,int);
    void *tick=((NewNode)0x401530)(guest_cards_tick),*draw=((NewNode)0x401530)(guest_cards_draw);
    FIELD(tick,0x24)=FIELD(draw,0x24)=(uint32_t)(uintptr_t)guest_cards;
    ((AddNode)0x401180)(tick,0x16);((AddNode)0x401230)(draw,0x33);
    FIELD(tick,4)|=2;FIELD(draw,4)|=2;
    FIELD(guest_cards,4)=(uint32_t)(uintptr_t)tick;FIELD(guest_cards,8)=(uint32_t)(uintptr_t)draw;
    log_line("Created independent P%u ability manager %p (host %p)",guest_seat+1,guest_cards,host_cards);
    log_line("Guest diagnostic addresses: funds=%p resources=%p",(void *)&p2_funds,(void *)p2_resources);
}

static void clone_initial_cards(void) {
    if(!guest_cards || initial_cards_cloned) return;
    initial_cards_cloned=1;
    typedef int (TC *AddCard)(void *,int,int);
    if(custom_cards) {
        for(unsigned i=0;i<chosen_count && i<3;++i) if(chosen_cards[i]>=0)
            ((AddCard)0x411460)(guest_cards,chosen_cards[i],1);
        log_line("P2 independent initial cards=%lu",(unsigned long)FIELD(guest_cards,0x28));
        return;
    }
    void *node=(void *)(uintptr_t)FIELD(host_cards,0x1c);
    unsigned char resources[0x30];memcpy(resources,(void *)0x4ccd38,sizeof(resources));
    uint32_t funds=U32(0x4ccd34);
    for(unsigned n=0;node && n<256;++n) {
        void *card=(void *)(uintptr_t)FIELD(node,0);
        if(card) ((AddCard)0x411460)(guest_cards,(int)FIELD(card,4),3);
        node=(void *)(uintptr_t)FIELD(node,4);
    }
    memcpy((void *)0x4ccd38,resources,sizeof(resources));U32(0x4ccd34)=funds;
    log_line("P2 initial cards=%lu; inventory instances are independent",(unsigned long)FIELD(guest_cards,0x28));
}

static void TC cards_ui(void *manager,int arg) {
    /* 408d00 releases only IDs in THIS manager+58 array. File-wide unloads
       are elsewhere; preserve each seat's native inventory animation tree. */
    ((void (TC *)(void *,int))cards_ui_original)(manager,arg);
}

static void release_guest_cards(void) {
    if(!guest_cards) return;
    uint32_t active_ui=FIELD(guest_cards,0x3c);
    if(active_ui && (!host_cards || active_ui!=FIELD(host_cards,0x3c)))
        ((void (__stdcall *)(uint32_t))0x488cf0)(active_ui);
    FIELD(guest_cards,0x3c)=0;
    for(unsigned slot=0;slot<256;++slot) {
        uint32_t id=FIELD(guest_cards,0x58+slot*4);
        if(id)((void (__stdcall *)(uint32_t))0x488cf0)(id);
        FIELD(guest_cards,0x58+slot*4)=0;
    }
    if(FIELD(guest_cards,0xc6c)) ((void (TC *)(void *))0x402b30)(OFF(guest_cards,0xc68));
    typedef void (TC *Remove)(void *,void *);
    for(unsigned off=4;off<=8;off+=4)
        ((Remove)0x4015a0)(PTR(0x4cf294),(void *)(uintptr_t)FIELD(guest_cards,off));
    void *old_manager=PTR(0x4cf298);PTR(0x4cf298)=guest_cards;
    void *node=(void *)(uintptr_t)FIELD(guest_cards,0x1c);
    while(node) {
        void *next=(void *)(uintptr_t)FIELD(node,4),*card=(void *)(uintptr_t)FIELD(node,0);
        if(card) {
            void *vtable=(void *)(uintptr_t)FIELD(card,0);
            ((void *(TC *)(void *,int))(uintptr_t)FIELD(vtable,0x50))(card,1);
        }
        node=next;
    }
    PTR(0x4cf298)=old_manager;
    free(guest_cards);guest_cards=NULL;guest_mukade=NULL;initial_cards_cloned=0;
    have_guest_resources=0;last_stage=0;
    log_line("Independent guest ability manager released");
}

static void *TC destroy_cards(void *manager,int unused) {
    shop_pending_owner=0;
    if(draft_owner) {
        guest_slot=(unsigned)draft_owner-2;
        if(manager==guest_cards)manager=draft_host;
        restore_draft_host();
    }
    for(unsigned id=1;id<party_count();++id){guest_slot=id-1;release_guest_cards();}
    guest_slot=0;host_cards=NULL;
    return ((void *(TC *)(void *,int))cards_destroy_original)(manager,unused);
}

static int TC shop_init(void *shop,const float *position) {
    if(rb_request_precise())return 1;
    if(p1 && ship(1) && !scope_depth)revive_at_clear();
    int result=((int (TC *)(void *,const float *))shop_init_original)(shop,position);
    shop_owner=scope_depth?(int)guest_seat:0;menu_previous=shop_owner?p2_input():input_latch.p1;memset(menu_age,0,sizeof(menu_age));
    log_line("Shop opened for P%d; funds=%lu",shop_owner+1,(unsigned long)U32(0x4ccd34));return result;
}
static void open_pending_shop(void) {
    if(!shop_pending_owner || scope_depth)return;
    unsigned target=shop_pending_owner,old=guest_slot;shop_pending_owner=0;
    if(target>=party_count() || !ship(target) || !ability(target) || !PTR(0x4cf2e4) || team_over)return;
    guest_slot=target-1;enter_p2(0);
    void *object=((void *(__cdecl *)(unsigned))0x48dc71)(0xe3c);
    if(object) {
        memset(object,0,0xe3c);FIELD(object,0)=2;PTR(0x4cf2a4)=object;
        ((void (TC *)(void *))0x416cd0)(current_guest);
        if(shop_init(object,shop_pending_position)){((void *(TC *)(void *,int))0x4176e0)(object,0);shop_owner=0;}
    }
    leave_p2(0);guest_slot=old;
}

static void TC shop_close(void *shop) {
    if(rp_viewer){((void (TC *)(void *))shop_close_original)(shop);shop_owner=0;shop_pending_owner=0;return;}
    if(!shop || !p1 || !ship(1)) {((void (TC *)(void *))shop_close_original)(shop);shop_owner=0;shop_pending_owner=0;return;}
    unsigned old=guest_slot;int owner=shop_owner,next=owner+1;
    int again=next<(int)party_count() && ship((unsigned)next) && ability((unsigned)next) && PTR(0x4cf2e4) && !team_over;
    if(again)memcpy(shop_pending_position,OFF(shop,0x1d0),sizeof(shop_pending_position));
    if(owner){guest_slot=(unsigned)owner-1;enter_p2(0);}
    ((void (TC *)(void *))shop_close_original)(shop);
    if(owner)leave_p2(0);
    shop_owner=0;
    shop_pending_owner=again?(unsigned)next:0;
    guest_slot=old;open_pending_shop();
}

static int TC draw_menu_stamp(void *manager,void *vm) {
    /* Title sprite 27 is the clear stamp, stored in a subrectangle of rank00.
       Its rotated draw path uses a cached D3D texture transform. Flush queued
       geometry before invalidating caches, so prior sprites keep their state.
       The visual effect of this guard still needs a foreground game check. */
    if(stamp_cache_guard && FIELD(vm,0x1c)==16 && FIELD(vm,0x20)==16 && FIELD(vm,0x24)==27 &&
       ((FIELD(vm,0x534)>>26)&31)==8) {
        ((void (TC *)(void *))0x47e730)(manager);
        FIELD(manager,0x3120e04)=UINT32_MAX;
        FIELD(manager,0x3120e14)=0;
        *OFF(manager,0x3120e0a)=0xff;
    }
    return ((int (TC *)(void *,void *))stamp_draw_original)(manager,vm);
}

static uint32_t menu_repeat(uint32_t keys) {
    uint32_t repeat=0;
    for(unsigned i=0;i<4;++i) {
        uint32_t bit=0x10u<<i;
        if(!(keys&bit) || !(menu_previous&bit)) menu_age[i]=0;
        else if(++menu_age[i]>=24 && (menu_age[i]-24)%6==0) repeat|=bit;
    }
    return repeat;
}

static int TC shop_tick(void *shop) {
    if(!ship(1) || !shop_owner)return ((Tick)shop_tick_original)(shop);
    unsigned old=guest_slot;guest_slot=(unsigned)shop_owner-1;
    uint32_t saved_menu[8];memcpy(saved_menu,(void *)0x4ca210,sizeof(saved_menu));
    uint32_t keys=p2_input()&0xf3;
    uint32_t edge=keys&~menu_previous;menu_previous=keys;
    /* The native shop only accepts its own menu confirm (0x80000). No guest
       gameplay key produces that bit, so a guest shop could be opened and then
       never completed: the session sat on the shop until the transport timed
       out. Give the second player a confirm on the shoot key's rising edge and
       mirror it into the held word for this tick only (restored below). */
    if(edge&1){keys|=0x80000u;edge|=0x80000u;U32(0x4ca210)|=0x80000u;}
    U32(0x4ca218)=menu_repeat(keys);U32(0x4ca21c)=edge;
    enter_p2(0);int result=((Tick)shop_tick_original)(shop);leave_p2(0);
    memcpy((void *)0x4ca210,saved_menu,sizeof(saved_menu));guest_slot=old;open_pending_shop();return result;
}

static int TC shop_draw(void *shop) {
    if(scene_construction_pending())return 1;
    if(!ship(1) || !shop_owner)return ((Tick)shop_draw_original)(shop);
    unsigned old=guest_slot;guest_slot=(unsigned)shop_owner-1;
    enter_p2(0);int result=((Tick)shop_draw_original)(shop);leave_p2(0);guest_slot=old;return result;
}

/* Reuse the real character/card selector twice. Never run its P1 launch
   animation: it creates a screen fade before the game-state request appears. */
static int draft_allowed(void *menu) {
    return native_draft && menu==PTR(0x4cf43c) && U32(0x4cf438)==1 &&
        !(U32(0x4cccc8)&0xf0) && !p1 && !current_guest;
}
static void restore_draft_host(void) {
    PTR(0x4cf298)=draft_host;
    guest_mukade=PTR(0x4cf2d4);PTR(0x4cf2d4)=draft_mukade;
    for(unsigned i=0;i<2;++i) {
        void *node=(void *)(uintptr_t)FIELD(draft_host,4+i*4);
        if(node) FIELD(node,4)=draft_host_flags[i];
    }
    if(draft_score) memcpy(OFF(draft_score,0x5f608),draft_templates,sizeof(draft_templates));
    memcpy((void *)0x4ccd38,draft_resources,sizeof(draft_resources));U32(0x4ccd34)=draft_funds;
    U32(0x4cccf4)=draft_character;U32(0x4cccf8)=draft_style;U32(0x4cf42c)=draft_character;
    draft_owner=0;draft_menu=NULL;draft_input=0;
}
static int begin_guest_draft(void *menu) {
    if(!PTR(0x4cf298) || !PTR(0x4cf41c)) return 0;
    if(guest_cards) release_guest_cards();
    prepare_guest_cards();if(!guest_cards) return 0;
    draft_host=PTR(0x4cf298);draft_score=PTR(0x4cf41c);draft_menu=menu;
    draft_mukade=PTR(0x4cf2d4);
    draft_character=FIELD(menu,0x24);draft_style=U32(0x4cccf8);draft_funds=U32(0x4ccd34);
    memcpy(draft_templates,OFF(draft_score,0x5f608),sizeof(draft_templates));
    memcpy(draft_resources,(void *)0x4ccd38,sizeof(draft_resources));
    memcpy(draft_history,OFF(menu,0x24),sizeof(draft_history));
    for(unsigned i=0;i<2;++i) {
        void *node=(void *)(uintptr_t)FIELD(draft_host,4+i*4);
        draft_host_flags[i]=node?FIELD(node,4):0;
        if(node) FIELD(node,4)&=~2u;
    }
    /* Native cancel branch closes both character VMs and its child card
       selector, without a start-game fade or any supervisor transition. */
    FIELD(menu,0x20)=4;FIELD(menu,0x388)=6;
    ((Tick)0x466740)(menu);
    memcpy(OFF(menu,0x24),draft_history,sizeof(draft_history));
    PTR(0x4cf298)=guest_cards;draft_owner=(int)guest_seat+1;draft_ready=0;
    PTR(0x4cf2d4)=NULL;
    U32(0x4cccf4)=chosen_character>=0 && chosen_character<4?(uint32_t)chosen_character:1;
    U32(0x4cccf8)=0;U32(0x4cf42c)=U32(0x4cccf4);
    ((void (TC *)(void *,int))0x4646e0)(menu,6);
    FIELD(menu,0x24)=U32(0x4cccf4);FIELD(menu,0x2c)=4;
    /* Native presets initially seed this screen; subsequent edits are P2's.
       The count is an unlocked-slot limit, NOT a selected-card counter.
       Leave it intact (including legitimately unlocked third slots). */
    draft_input=0;draft_release=1;
    memset((void *)0x4ca210,0,0x234);
    log_line("Native selection: P%u now selecting in game; P1 character=%lu",guest_seat+1,(unsigned long)draft_character);
    return 1;
}
static void finish_guest_draft(void *menu) {
    unsigned group=FIELD(guest_cards,0xc5c);if(group>2) group=0;
    chosen_count=FIELD(draft_score,0x5f678+group*4);if(chosen_count>3) chosen_count=3;
    chosen_character=(int)U32(0x4cccf4);custom_cards=1;
    for(unsigned i=0;i<3;++i) chosen_cards[i]=i<chosen_count?*OFF(draft_score,0x5f608+group*16+i):-1;
    int finished=draft_owner;
    restore_draft_host();
    memcpy(OFF(menu,0x24),draft_history,sizeof(draft_history));
    if(finished<(int)party_count()) {
        guest_slot=(unsigned)finished-1;
        if(!begin_guest_draft(menu)) {guest_slot=0;requested=wanted=0;log_line("Could not initialize next native selector");}
        return;
    }
    unsigned depth=FIELD(menu,0xb0);if(depth>15) depth=15;
    FIELD(menu,0x30+depth*4)=draft_character;FIELD(menu,0x70+depth*4)=4;
    FIELD(menu,0xb0)=depth<15?depth+1:15;
    draft_ready=1;requested=wanted=1;
    log_line("Native selection complete: P1=%lu; party=%u",(unsigned long)draft_character,party_count());
    for(unsigned id=1;id<party_count();++id)log_line("Selected P%u character=%d cards=%d,%d,%d slots=%u",id+1,guests[id-1].selected_character,guests[id-1].loadout[0],guests[id-1].loadout[1],guests[id-1].loadout[2],guests[id-1].loadout_count);
    guest_slot=0;
}
static int TC native_menu_tick(void *menu) {
    if(rp_menu(menu))return 1;
    if(draft_owner)guest_slot=(unsigned)draft_owner-2;
    if(draft_owner && menu!=draft_menu) {restore_draft_host();release_guest_cards();guest_slot=0;}
    if(draft_allowed(menu) && FIELD(menu,0x18)==6 && !draft_owner) {
        if(FIELD(menu,0x20)==0) draft_ready=0;
        if(FIELD(menu,0x20)==3) {guest_slot=0;if(begin_guest_draft(menu))return 1;}
    }
    if(draft_owner && draft_owner<(int)party_count() && FIELD(menu,0x18)==6 && FIELD(menu,0x20)==3) {
        finish_guest_draft(menu);return 1;
    }
    uint32_t before=U32(0x4cd5e8);
    int result=((Tick)menu_tick_original)(menu);
    /* Isolated, no-commit Replay seek qualification. Enter a fresh native
       stage at normal launch, never jump with live bullets/old ECL owners. */
    if(rp_test_start_stage && !p1 && before!=7 && U32(0x4cd5e8)==7) {
        unsigned stage=rp_test_start_stage;rp_test_start_stage=0;
        U32(0x4cccdc)=U32(0x4ccce0)=stage;PTR(0x4cf428)=(void *)(0x4c9410+stage*0xd4);
        log_line("REPLAY_TEST isolated native start stage=%u",stage);
    }
    if(draft_owner && menu==draft_menu) {
        if(U32(0x4cd5e8)==7 && before!=7) finish_guest_draft(menu);
        else if(FIELD(menu,0x18)!=6) {
            /* P2 Back returns to P1's character screen and original loadout. */
            restore_draft_host();release_guest_cards();guest_slot=0;
            memcpy(OFF(menu,0x24),draft_history,sizeof(draft_history));
            ((void (TC *)(void *,int))0x4646e0)(menu,6);
            memset((void *)0x4ca210,0,0x234);
            log_line("Native selection: P2 cancelled; returned to P1");
        }
    }
    return result;
}
static int TC native_menu_draw(void *menu) {
    int result=((Tick)menu_draw_original)(menu);
    if(draft_allowed(menu) && FIELD(menu,0x18)==6) {
        if(PTR(0x51f65c)) ((void (TC *)(void *))0x47e730)(PTR(0x51f65c));
        coop_menu_hud(draft_owner?draft_owner:1);
    }
    return result;
}
static void draft_menu_keys(void *input,uint32_t keys) {
    if(draft_release) {if(!keys) draft_release=0;keys=0;}
    FIELD(input,0)=keys;FIELD(input,4)=draft_input;draft_input=keys;
}
static void TC native_menu_input(void *input) {
    if(input==(void *)0x4ca210 && !online_map && !rp_playing() && !rp_viewer)kb_local(input);
    rp_test_keys(input);
    if(input==(void *)0x4ca210) {
        int mapped=online_poll(input);if(mapped==2)return;
        if(!mapped && !rp_playing())coop_input_capture(&input_latch,FIELD(input,0),sample_p2_local());
        trace_input_record();
    }
    if(draft_owner && input==(void *)0x4ca210) {unsigned old=guest_slot;guest_slot=(unsigned)draft_owner-2;draft_menu_keys(input,p2_input()&0xcfb);guest_slot=old;}
    ((void (TC *)(void *))menu_input_original)(input);
}

static int TC guest_tick(void *player) {
    unsigned old=bind_guest(player,0);
    if (!current_guest || player!=current_guest || PTR(0x4cf410)!=p1) {guest_slot=old;return 1;}
    enter_p2(1);
    prepare_player_tick(guest_seat);
    void *bomb=PTR(0x4cf2b8);
    uint32_t prior_bomb=bomb?FIELD(bomb,0x30):0;
    int result=p2_tick(player);
    if(bomb && !prior_bomb && FIELD(bomb,0x30)) {bomb_owner=(int)guest_seat;log_line("P%u used Bomb; remaining=%lu",guest_seat+1,(unsigned long)U32(0x4ccd58));}
    ++guest_ticks;
    leave_p2(1);guest_slot=old;
    return result;
}

static int TC shared_bomb_tick(void *bomb) {
    unsigned old=bind_guest(bomb,2);int guest=ship(1) && bomb==guest_bomb;
    if(guest)enter_p2(0);
    int result=bomb_tick(bomb);share_bomb_protection(bomb);
    if(guest)leave_p2(0);
    if(!FIELD(bomb,0x30))bomb_owner=0;
    guest_slot=old;return result;
}

static int TC shared_bomb_draw(void *bomb) {
    if(scene_construction_pending())return 1;
    unsigned old=bind_guest(bomb,2);int guest=ship(1) && bomb==guest_bomb;
    if(guest)enter_p2(0);int result=bomb_draw(bomb);if(guest)leave_p2(0);
    guest_slot=old;return result;
}

static void TC coop_pause_actions(void *menu) {
    if(current_guest && !(U32(0x4cccc8)&0xc0)) {
        /* Native credits control both the disabled selector bit and gray
           Continue artwork. Do not fabricate a second game-over UI. */
        U32(0x4cccd0)=0;
        if(FIELD(menu,0x1ec)==2 && FIELD(menu,0x1f4)==18 && FIELD(menu,0x34)==0) {
            /* State 18 dispatches by selection: 0 continues, 1 returns to
               title, 6 retries. Block only the stale Continue selection. */
            ((void (TC *)(void *,int))0x4577d0)(menu,2);
            log_line("Blocked co-op Continue; restored native game-over menu");
        }
    }
    ((void (TC *)(void *))pause_actions_original)(menu);
}

static void spirit_fade_tick(void) {
    for(unsigned id=0;id<party_count();++id){
        if(!partner[id].ghost){spirit_was_drawn[id]=0;continue;}
        unsigned target=ghost_alpha;
        for(unsigned other=0;other<party_count();++other)if(other!=id && partner[other].life_taps && partner[other].life_target==(int)id+1)target=254;
        if(!spirit_was_drawn[id])spirit_draw_alpha[id]=(float)ghost_alpha;
        float change=fabsf(254-(float)ghost_alpha)/12.0f;
        if(spirit_draw_alpha[id]<target){spirit_draw_alpha[id]+=change;if(spirit_draw_alpha[id]>target)spirit_draw_alpha[id]=(float)target;}
        else if(spirit_draw_alpha[id]>target){spirit_draw_alpha[id]-=change;if(spirit_draw_alpha[id]<target)spirit_draw_alpha[id]=(float)target;}
        spirit_was_drawn[id]=1;
    }
}
static int draw_ship(unsigned id,Tick native_draw) {
    void *player=ship(id);
    uint32_t state=FIELD(player,0x476ac),color=FIELD(player,0x53c),primary=FIELD(player,0x538),focus=FIELD(player,0x476cc);
    if(partner[id].ghost) {
        /* Render the live native VM: its own animation, scale and transforms.
           The state override exists only inside this draw callback. */
        FIELD(player,0x476ac)=1;
        FIELD(player,0x476cc)=0;
        /* Fade is advanced by logical frames, never wall time or replay Draw. */
        unsigned alpha=spirit_was_drawn[id]?(unsigned)(spirit_draw_alpha[id]+.5f):ghost_alpha;
        if(alpha>255) alpha=255;
        /* Native ANM selects primary or secondary color by blend flags.
           Tint both; changing only the secondary can leave the sprite opaque. */
        FIELD(player,0x538)=FIELD(player,0x53c)=(alpha<<24)|0x00ffffff;
    }
    int result=native_draw(player);
    FIELD(player,0x476ac)=state;FIELD(player,0x53c)=color;FIELD(player,0x538)=primary;FIELD(player,0x476cc)=focus;
    return result;
}
static int TC host_draw(void *player) {
    if(!current_guest) return p1_draw(player);
    return draw_ship(0,p1_draw);
}
static int TC guest_draw(void *player) {
    if(scene_construction_pending())return 1;
    unsigned old=bind_guest(player,0);
    if (!current_guest || player!=current_guest || PTR(0x4cf410)!=p1) {guest_slot=old;return 1;}
    enter_p2(0);
    int result=draw_ship(guest_seat,p2_draw);
    leave_p2(0);guest_slot=old;
    return result;
}

static int guest_asset_ready(void *anm,uint32_t character) {
    if(!guest_anm) {
        if(FIELD(anm,0x312072c+guest_file_id*4)) return 0;
        const char *name=*(const char **)(uintptr_t)(0x4b7098+character*4);
        /* The blocking 486880 wrapper sleeps until a future render tick.
           Start its CPU phase and let normal frames perform GPU completion. */
        guest_anm=((void *(TC *)(void *,int,const char *))0x4867e0)(anm,(int)guest_file_id,name);
        if(!guest_anm) {FIELD(anm,0x312072c+guest_file_id*4)=0;return 0;}
        FIELD(guest_anm,0x128)=1;guest_asset_started=GetTickCount();
        log_line("Scheduled P2 animation load: %s; waiting across frames",name);
        return 0;
    }
    return FIELD(guest_anm,0x128)==0;
}

static void create_guest(void) {
    if (current_guest || !p1 || PTR(0x4cf2a4) || ((U32(0x4cccc8)&0xc0) && !rp_viewer)) return;
    uint32_t stage=U32(0x4cccdc);
    int carry=have_guest_resources && stage==last_stage+1 && stage<=6;
    uint32_t host_character=U32(0x4cccf4),host_style=U32(0x4cccf8);
    uint32_t next_character=carry?p2_character:chosen_character>=0?(uint32_t)chosen_character:host_character;
    void *anm=PTR(0x51f65c),*host_anm=anm?(void *)(uintptr_t)FIELD(anm,0x3120750):NULL;
    /* Keep the previous stage and inventory intact while awaiting GPU work.
       Updating last_stage early would turn the next poll into a fresh run. */
    /* A private file ID identifies P2's options and shot VMs for presentation,
       including when both players selected the same character. */
    if(anm && !guest_asset_ready(anm,next_character)) {requested=1;return;}
    log_line("Creating native P2; P1=%p stage=%lu state=%lu",p1,
             (unsigned long)U32(0x4cccdc),(unsigned long)FIELD(p1,0x476ac));
    if(!carry && guest_cards) release_guest_cards();
    if(!carry) {
        memcpy(p2_resources,custom_cards && have_run_base?run_base_resources:(void *)0x4ccd38,sizeof(p2_resources));
        p2_funds=custom_cards && have_run_base?run_base_funds:U32(0x4ccd34);
        p2_character=chosen_character>=0?(uint32_t)chosen_character:U32(0x4cccf4);
    }
    if(carry) for(unsigned id=0;id<2;++id) {
        unsigned char *resources=id?p2_resources:(unsigned char *)0x4ccd38;
        if(*(int32_t *)(resources+0x10)<0) {
            *(int32_t *)(resources+0x10)=0;*(int32_t *)(resources+0x20)=*(int32_t *)(resources+0x28)>0?*(int32_t *)(resources+0x28):2;
            *(int32_t *)resources=*(int32_t *)(resources+4)/2;
        }
    }
    last_stage=stage;have_guest_resources=1;
    observed_stage=stage;
    unsigned char saved[0x30]; memcpy(saved,(void *)0x4ccd38,sizeof(saved));
    uint32_t host_funds=U32(0x4ccd34);
    void *cached_shot=PTR(0x570920);
    void *host_bomb=PTR(0x4cf2b8);
    prepare_guest_cards();
    if(!guest_cards) {
        log_line("P2 ability manager allocation failed; guest creation postponed");
        have_guest_resources=0;requested=1;return;
    }
    void *host_manager=PTR(0x4cf298);PTR(0x4cf298)=guest_cards?guest_cards:host_manager;
    void *host_mukade=PTR(0x4cf2d4);PTR(0x4cf2d4)=guest_mukade;
    memcpy((void *)0x4ccd38,p2_resources,sizeof(p2_resources));U32(0x4ccd34)=p2_funds;
    U32(0x4cccf4)=p2_character;U32(0x4cccf8)=0;PTR(0x570920)=NULL;
    if(anm && guest_anm) FIELD(anm,0x3120750)=(uint32_t)(uintptr_t)guest_anm;
    /* Constructor is called on the game update thread, never the loader thread. */
    creating_guest=1;current_guest=((CreatePlayer)0x45af20)();creating_guest=0;
    if (!current_guest) {PTR(0x4cf410)=p1;PTR(0x4cf298)=host_manager;memcpy((void *)0x4ccd38,saved,sizeof(saved));
        U32(0x4cccf4)=host_character;U32(0x4cccf8)=host_style;
        if(anm) FIELD(anm,0x3120750)=(uint32_t)(uintptr_t)host_anm;
        /* The failing native constructor has already destroyed its ANM. */
        if(guest_anm) {
            if(U32(0x4cccc8)&1) ((void (TC *)(void *,int))0x486fc0)(anm,(int)guest_file_id);
            else FIELD(anm,0x312072c+guest_file_id*4)=0;
            guest_anm=NULL;
        }
        void *failed_shot=PTR(0x570920);
        if((U32(0x4cccc8)&1) && failed_shot && failed_shot!=cached_shot)
            ((void (__cdecl *)(void *))0x491a3f)(failed_shot);
        PTR(0x4cf2d4)=host_mukade;PTR(0x570920)=cached_shot;
        U32(0x4ccd34)=host_funds;have_guest_resources=0;requested=1;log_line("P2 constructor failed");return;}
    log_line("Native P%u=%p tickNode=%p drawNode=%p",guest_seat+1,current_guest,(void *)(uintptr_t)FIELD(current_guest,4),(void *)(uintptr_t)FIELD(current_guest,8));
    /* Starting-card callbacks must already see P2's own native Bomb. */
    guest_bomb=((CreatePlayer)0x41fd40)();
    if(!carry && custom_cards) U32(0x4ccd38)=U32(0x4ccd40)*(stage==1 || stage==7?1:4);
    clone_initial_cards();
    if(!rp_playing())((void (__cdecl *)(void))0x45abb0)();
    memcpy(p2_resources,(void *)0x4ccd38,sizeof(p2_resources));p2_funds=U32(0x4ccd34);
    guest_mukade=PTR(0x4cf2d4);PTR(0x4cf2d4)=host_mukade;
    U32(0x4cccf4)=host_character;U32(0x4cccf8)=host_style;
    if(anm) FIELD(anm,0x3120750)=(uint32_t)(uintptr_t)host_anm;
    PTR(0x4cf2b8)=host_bomb;
    U32(0x4ccd34)=host_funds;
    PTR(0x4cf298)=host_manager;
    FLT(current_guest,0x620)=FLT(p1,0x620)+(guest_slot?-40.0f:40.0f);
    if(FLT(current_guest,0x620)>174.0f) FLT(current_guest,0x620)=174.0f;
    FLT(current_guest,0x624)=FLT(p1,0x624);
    FIELD(current_guest,0x62c)=(int)(FLT(current_guest,0x620)*128.0f);
    FIELD(current_guest,0x630)=(int)(FLT(current_guest,0x624)*128.0f);
    /* Each native player owns its animation VM; the HUD adds player labels. */
    FIELD(current_guest,0x47984)=FIELD((void *)(uintptr_t)FIELD(current_guest,0x47940),0x28);
    void *tick=(void *)(uintptr_t)FIELD(current_guest,4), *draw=(void *)(uintptr_t)FIELD(current_guest,8);
    p2_tick=(Tick)(uintptr_t)FIELD(tick,8);
    p2_draw=(Tick)(uintptr_t)FIELD(draw,8);
    InterlockedExchange((LONG *)OFF(tick,8),(LONG)(uintptr_t)guest_tick);
    InterlockedExchange((LONG *)OFF(draw,8),(LONG)(uintptr_t)guest_draw);
    /* Stage startup normally enables registered callbacks in one batch.
       A second player constructed after startup needs that flag explicitly. */
    FIELD(tick,4)|=2;
    FIELD(draw,4)|=2;
    void *bomb=PTR(0x4cf2b8);
    if(bomb && valid(bomb,0x10)) {
        void *bt=(void *)(uintptr_t)FIELD(bomb,8),*bd=(void *)(uintptr_t)FIELD(bomb,12);
        if(valid(bt,0x28) && valid(bd,0x28) && FIELD(bt,0x24)==(uint32_t)(uintptr_t)bomb && FIELD(bd,0x24)==(uint32_t)(uintptr_t)bomb) {
            if(FIELD(bt,8)!=(uint32_t)(uintptr_t)shared_bomb_tick) bomb_tick=(Tick)(uintptr_t)FIELD(bt,8);
            if(FIELD(bd,8)!=(uint32_t)(uintptr_t)shared_bomb_draw) bomb_draw=(Tick)(uintptr_t)FIELD(bd,8);
            FIELD(bt,8)=(uint32_t)(uintptr_t)shared_bomb_tick;
            FIELD(bd,8)=(uint32_t)(uintptr_t)shared_bomb_draw;
        }
    }
    /* All four native Bomb classes share the verified 0xa8-byte base layout.
       Use the native factory so each character gets its own virtual methods,
       effects, timer and scheduler nodes. */
    if(guest_bomb) {
        FIELD((void *)(uintptr_t)FIELD(guest_bomb,8),8)=(uint32_t)(uintptr_t)shared_bomb_tick;
        FIELD((void *)(uintptr_t)FIELD(guest_bomb,12),8)=(uint32_t)(uintptr_t)shared_bomb_draw;
    }
    typedef void *(__stdcall *NewNode)(void *);
    typedef int (__stdcall *AddDraw)(void *,int);
    if(!hud_node) {
        hud_node=((NewNode)0x401530)(draw_late_hud);
        FIELD(hud_node,0x24)=(uint32_t)(uintptr_t)p1;
        ((AddDraw)0x401230)(hud_node,0x61);
        FIELD(hud_node,4)|=2;
    }
    if(!dialogue_hud_node) {
        dialogue_hud_node=((NewNode)0x401530)(draw_dialogue_hud);
        FIELD(dialogue_hud_node,0x24)=(uint32_t)(uintptr_t)p1;
        /* New nodes precede equal priorities. The native ANM pass at 0x42
           draws dialogue portraits, including their inactive layer. */
        ((AddDraw)0x401230)(dialogue_hud_node,0x42);
        FIELD(dialogue_hud_node,4)|=2;
    }
    PTR(0x4cf410)=p1; PTR(0x570920)=cached_shot;
    memcpy((void *)0x4ccd38,saved,sizeof(saved));
    previous_p2=0;
    if(!guest_slot) {
        memset(item_owner,-1,sizeof(item_owner));memset(transfer_owner,-1,sizeof(transfer_owner));memset(transfer_power,0,sizeof(transfer_power));
        memset(partner,0,sizeof(partner));wipe_frames=0;team_over=0;memset(last_age,0,sizeof(last_age));
        memset(collected,0,sizeof(collected));memset(damage_done,0,sizeof(damage_done));memset(hits,0,sizeof(hits));bomb_owner=0;protected_bombs=0;
    }
    U32(0x4cccd0)=0;
    if(stage==departed_stage+1) {
        if(!guest_slot && (departed_ghosts&1))revive(0);
        if(departed_ghosts&(1u<<guest_seat))revive(guest_seat);
    }
    if(guest_seat+1==party_count())departed_ghosts=0;
    rp_guest_created(guest_seat);
    log_line("P%u active, native tick=%p draw=%p",guest_seat+1,(void *)p2_tick,(void *)p2_draw);
}

static int TC host_tick(void *player) {
    if(current_guest && U32(0x4cccdc)!=observed_stage) {
        if(U32(0x4cccdc)>observed_stage) revive_at_clear();
        observed_stage=U32(0x4cccdc);
    }
    if(current_guest) prepare_player_tick(0);
    void *bomb=PTR(0x4cf2b8);uint32_t prior_bomb=bomb?FIELD(bomb,0x30):0;
    int result=p1_tick(player);
    if(bomb && !prior_bomb && FIELD(bomb,0x30)) bomb_owner=0;
    if (WaitForSingleObject(activation,0)==WAIT_OBJECT_0) requested=wanted=1;
    if (!online_map && (GetForegroundWindow()==(HWND)(uintptr_t)U32(0x568c30)) && (GetAsyncKeyState(VK_F7)&1)) requested=wanted=1;
    if(requested && !PTR(0x4cf2a4) && (!(U32(0x4cccc8)&0xc0) || rp_viewer)) {
        unsigned old=guest_slot;requested=0;
        for(unsigned id=1;id<party_count();++id)if(!ship(id)) {if(!rp_guest_due(id)){requested=1;continue;}guest_slot=id-1;create_guest();}
        guest_slot=old;
    }
    int ready=1;for(unsigned id=0;id<party_count();++id)if(!ship(id))ready=0;
    if(ready && !PTR(0x4cf2a4)) {
        uint32_t keys[3]={U32(0x4ca428),input_latch.p2,input_latch.p3};partner_tick(keys);
    }
    if (current_guest && ++frames%120==0) {
        log_line("tick=%ld guestTicks=%lu P1=(%.1f,%.1f) state=%lu power=%lu lives=%ld P2=(%.1f,%.1f) state=%lu power=%lu lives=%ld items=%lu/%lu damage=%lu/%lu hits=%lu/%lu",
            frames,guest_ticks,FLT(p1,0x620),FLT(p1,0x624),(unsigned long)FIELD(p1,0x476ac),
            (unsigned long)U32(0x4ccd38),(long)(int32_t)U32(0x4ccd48),
            FLT(current_guest,0x620),FLT(current_guest,0x624),(unsigned long)FIELD(current_guest,0x476ac),
            (unsigned long)*(uint32_t *)(p2_resources),(long)*(int32_t *)(p2_resources+0x10),collected[0],collected[1],damage_done[0],damage_done[1],hits[0],hits[1]);
        log_line("Economy funds=%lu/%lu cards=%lu/%lu",(unsigned long)U32(0x4ccd34),(unsigned long)p2_funds,
                 host_cards?(unsigned long)FIELD(host_cards,0x28):0,guest_cards?(unsigned long)FIELD(guest_cards,0x28):0);
        if(party_count()==3 && ship(2))log_line("P3 state=%lu power=%ld lives=%ld bombs=%ld funds=%lu cards=%lu items=%lu damage=%lu hits=%lu",
            (unsigned long)FIELD(ship(2),0x476ac),(long)*resource(2,0),(long)*resource(2,0x10),(long)*resource(2,0x20),
            (unsigned long)guests[1].funds,ability(2)?(unsigned long)FIELD(ability(2),0x28):0,collected[2],damage_done[2],hits[2]);
    }
    return result;
}

static int alive(unsigned id) {
    void *player=ship(id);
    return player && !partner[id].ghost && *resource(id,0x10)>=0 && FIELD(player,0x476ac)!=2 && FIELD(player,0x476ac)!=4;
}

/* Rules audited against eagler-touhou 8ee5ac9570, th07 and CooperativeRules.hpp.
   Native TH18 counts spare lives (zero still means an active last ship). */
static void set_position(void *player,float x,float y) {
    FLT(player,0x620)=x;FLT(player,0x624)=y;
    FIELD(player,0x62c)=(int)(x*128);FIELD(player,0x630)=(int)(y*128);
}
static void refresh_player_options(void *player) {
    /* Native power/option rebuild takes the position subobject as this and
       reads the current seat's power, character and ability manager. */
    ((void (TC *)(void *,void *))0x45d5e0)(OFF(player,0x620),player);
}
static void ensure_equipment_options(void *player) {
    void *cards=PTR(0x4cf298),*anm=PTR(0x51f65c);
    if(!cards || !anm || PTR(0x4cf2a4) || FIELD(player,0x476ac)!=1 ||
       (FIELD(player,0x4779c)&0x182))return;
    unsigned missing=0,available=0;
    void *node=(void *)(uintptr_t)FIELD(cards,0x1c);
    for(unsigned n=0;node && n<256;++n) {
        void *card=(void *)(uintptr_t)FIELD(node,0);
        node=(void *)(uintptr_t)FIELD(node,4);
        if(!card)continue;
        uint32_t id=FIELD(card,4);
        if(id!=19 && id!=51)continue; /* Back Door and Ancient Magatama */
        unsigned count=id==51?2:1;
        for(unsigned i=0;i<count;++i) {
            void *option=(void *)(uintptr_t)FIELD(card,0x54+i*4);
            uintptr_t offset=(uintptr_t)option-(uintptr_t)OFF(player,0xa30);
            if(offset>=12*0xf0 || offset%0xf0 || FIELD(option,0)!=2 ||
               FIELD(option,0xd0)!=FIELD(card,8)) {++missing;continue;}
            uint32_t vm=FIELD(option,0xb0);
            if(!vm || !((void *(TC *)(void *,uint32_t))0x488b40)(anm,vm)) {++missing;++available;}
        }
    }
    /* Acquisition can precede the native option rebuild. Restore missing
       attachments on the owning seat's tick, retaining native shot/collision
       callbacks and animations. A full pool is left to native capacity rules. */
    if(!missing)return;
    for(unsigned i=0;i<12;++i)if(!FIELD(player,0xa30+i*0xf0))++available;
    if(missing<=available && !rb_request_precise())refresh_player_options(player);
}
static void prepare_player_tick(unsigned id) {
    void *player=ship(id);
    /* The solo death path eventually cleans this detached focus VM. Spirits
       deliberately hold that path, so release it before it can be orphaned.
       +476b4 is a separate card effect and must remain under native ownership. */
    if(FIELD(player,0x476ac)==2 && FIELD(player,0x476b0)) {
        ((void (__stdcall *)(uint32_t))0x488cf0)(FIELD(player,0x476b0));
        FIELD(player,0x476b0)=0;
    }
    /* Called inside the corresponding resource scope. Keep native bullets and
       effects ticking, but stop the solo retry path at its last safe tick. */
    if((int32_t)U32(0x4ccd48)<0 && FIELD(player,0x476ac)==2 && FIELD(player,0x638)>=29) {
        FIELD(player,0x638)=29;FLT(player,0x63c)=29.0f;FIELD(player,0x634)=28;
        if(!partner[id].ghost) {
            uint32_t drift=((uint32_t)frames*1664525u+U32(0x4cccdc)*1013904223u+id*2246822519u);
            partner[id].ghost=1;partner[id].dx=(drift&0x1000)?.2f:-.2f;partner[id].dy=(drift&0x2000)?.2f:-.2f;
            U32(0x4ccd38)=0;U32(0x4ccd58)=3;U32(0x4ccd5c)=0;
            void *file=(void *)(uintptr_t)FIELD(player,0xc);
            if(file) ((void *(TC *)(void *,void *,int))0x407420)(file,OFF(player,0x14),0);
            log_line("P%u became a spirit",id+1);
        }
    }
    if(FIELD(player,0x476ac)==4 && !partner[id].deathbomb_extended) {
        /* The reference doubles the native deathbomb window. */
        partner[id].deathbomb_base=FIELD(player,0x47908);
        FIELD(player,0x47908)*=2;partner[id].deathbomb_extended=1;
    } else if(FIELD(player,0x476ac)!=4 && partner[id].deathbomb_extended) {
        FIELD(player,0x47908)=partner[id].deathbomb_base;partner[id].deathbomb_extended=0;
    }
    if(!partner[id].ghost)ensure_equipment_options(player);
}
static int item_index(void *manager,void *item) {
    intptr_t offset=(unsigned char *)item-OFF(manager,0x14);
    return offset>=0 && offset%0xc94==0 && offset/0xc94<0x1258?(int)(offset/0xc94):-1;
}
static void recycle_transfer(void *item) {
    FIELD(item,0xc74)=0;
    uint32_t effect=FIELD(item,0xc28);
    if(effect) ((void (__stdcall *)(uint32_t))0x488cf0)(effect);
    FIELD(item,0xc28)=0;
    /* Native pickup/offscreen cleanup returns this detached intrusive node to
       its original free-list sentinel, retained in item->owner at offset 0. */
    void *head=(void *)(uintptr_t)FIELD(item,0);
    if(head) {
        void *next=(void *)(uintptr_t)FIELD(head,4);
        FIELD(item,4)=(uint32_t)(uintptr_t)next;
        if(next) FIELD(next,8)=(uint32_t)(uintptr_t)item;
        if(FIELD(head,0xc)) FIELD(head,0xc)=(uint32_t)(uintptr_t)item;
        FIELD(head,4)=(uint32_t)(uintptr_t)item;FIELD(item,8)=(uint32_t)(uintptr_t)head;
    }
}
static int spawn_transfer(unsigned from,unsigned to,int power) {
    void *manager=PTR(0x4cf2ec);
    if(!manager || !spawn_original) return 0;
    typedef void *(TC *Spawn)(void *,uint32_t,const float *,uint32_t,uint32_t,uint32_t,uint32_t,uint32_t,uint32_t);
    unsigned count=power?6:1,free_slots=0;
    for(unsigned i=0;i<0x1258 && free_slots<count;++i)
        if(!FIELD(OFF(manager,0x14+i*0xc94),0xc74)) ++free_slots;
    if(free_slots<count) return 0;
    void *spawned[6]={0};unsigned weight=0;int assigned=0;
    /* Reference: two large and four small P items, bypassing duplication. */
    for(unsigned i=0;i<count;++i) {
        float origin[3]={FLT(ship(from),0x620)+(int)(i%3)*10-10,
                         FLT(ship(from),0x624)+(int)(i/3)*8-8,0};
        void *item=((Spawn)spawn_original)(manager,power?(i<2?3:1):5,origin,0,0,0,0,0,0);
        int index=item?item_index(manager,item):-1;
        if(index<0 || !FIELD(item,0xc74)) {
            for(unsigned n=0;n<i;++n) {
                int old=item_index(manager,spawned[n]);recycle_transfer(spawned[n]);
                transfer_owner[old]=-1;transfer_power[old]=0;item_owner[old]=-1;
            }
            return 0;
        }
        spawned[i]=item;weight+=i<2?8:1;
        int total=power?(power*(int)weight+19)/20:0;
        transfer_owner[index]=(int8_t)to;transfer_power[index]=total-assigned;assigned=total;
        item_owner[index]=(int8_t)to;last_age[index]=FIELD(item,0xc50);FIELD(item,0xc74)=4;
        float speed=FLT(ship(to),0x47988);FLT(item,0xc80)=speed>0?speed:5.0f;
    }
    return 1;
}
static void revive(unsigned id) {
    unsigned old=guest_slot;if(id)guest_slot=id-1;
    void *player=ship(id);float x=FLT(player,0x620),y=FLT(player,0x624);
    if(id) enter_p2(0);
    U32(0x4ccd48)=0;U32(0x4ccd58)=U32(0x4ccd60)>0?U32(0x4ccd60):2;U32(0x4ccd5c)=0;
    /* The native reset derives options and the position trail immediately.
       Give it the restored power and rescue position before it runs. */
    U32(0x4ccd38)=U32(0x4ccd3c)/2;
    set_position(player,x,y);
    ((void (__cdecl *)(void))0x45abb0)();
    FIELD(player,0x47778)=240;FLT(player,0x4777c)=240;FIELD(player,0x47774)=239;
    set_position(player,x,y);
    if(id) leave_p2(0);guest_slot=old;
    partner[id].ghost=0;partner[id].keys=0;
    log_line("P%u revived with half power and 240 ticks of protection",id+1);
}
static void revive_at_clear(void) {
    for(unsigned id=0;id<party_count();++id)if(ship(id) && (partner[id].ghost || (*resource(id,0x10)<0 && FIELD(ship(id),0x476ac)==2)))revive(id);
    wipe_frames=0;
}
static float player_distance(unsigned from,unsigned to) {
    float dx=FLT(ship(from),0x620)-FLT(ship(to),0x620),dy=FLT(ship(from),0x624)-FLT(ship(to),0x624);
    return dx*dx+dy*dy;
}
static int transfer_recipient(unsigned from,int power) {
    int best=-1;
    for(unsigned id=0;id<party_count();++id) {
        if(id==from || !ship(id) || player_distance(from,id)>400)continue;
        int eligible=power?alive(id) && *resource(id,0)<*resource(id,4):partner[id].ghost || (alive(id) && *resource(id,0x10)<*resource(id,0x1c));
        if(!eligible)continue;
        if(best<0 || (!power && partner[id].ghost>partner[best].ghost) ||
           ((power || partner[id].ghost==partner[best].ghost) && *resource(id,power?0:0x10)<*resource((unsigned)best,power?0:0x10)))best=(int)id;
    }
    return best;
}
static void partner_tick(const uint32_t *keys) {
    unsigned spirits=0;
    for(unsigned id=0;id<party_count();++id) {
        PartnerState *state=&partner[id];void *player=ship(id);
        if(!player)continue;
        if(!state->ghost && FIELD(player,0x476ac)==1) {state->sprite_file=(int)FIELD(player,0x34);state->sprite=(int)FIELD(player,0x38);}
        if(state->ghost) {
            ++spirits;
            float x=FLT(player,0x620)+state->dx,y=FLT(player,0x624)+state->dy;
            if(x < -180){x=-180;state->dx=.2f;}if(x>180){x=180;state->dx=-.2f;}
            if(y<300){y=300;state->dy=.2f;}if(y>416){y=416;state->dy=-.2f;}
            set_position(player,x,y);
        }
    }
    for(unsigned id=0;id<party_count();++id) {
        PartnerState *state=&partner[id];uint32_t edge=keys[id]&~state->keys;state->keys=keys[id];
        if(!alive(id) || FIELD(ship(id),0x476ac)!=1) {state->life_taps=state->life_window=state->power_taps=state->power_window=state->wait_release=state->life_target=0;continue;}
        int life=transfer_recipient(id,0),power=transfer_recipient(id,1);
        if(state->life_target!=life+1){state->life_taps=state->life_window=0;state->life_target=life+1;}
        if(state->life_taps){if(!state->life_window)state->life_taps=0;else --state->life_window;}
        if(state->power_window && --state->power_window==0)state->power_taps=0;
        if(!(keys[id]&8))state->wait_release=0;
        if((edge&8) && !(keys[id]&1))state->power_taps=state->power_window=0;
        if(life<0 || *resource(id,0x10)<=0 || (keys[id]&1))state->life_taps=state->life_window=0;
        else if((edge&8) && !state->wait_release) {
            state->life_window=24;
            if(++state->life_taps==5) {
                if(partner[life].ghost){--*resource(id,0x10);revive((unsigned)life);}
                else if(spawn_transfer(id,(unsigned)life,0))--*resource(id,0x10);
                state->life_taps=state->life_window=0;state->wait_release=1;
            }
        }
        if(power<0 || *resource(id,0)<100)state->power_taps=state->power_window=0;
        else if(edge&1) {
            state->power_window=24;state->life_taps=state->life_window=0;
            if(++state->power_taps==5) {
                if(spawn_transfer(id,(unsigned)power,100))*resource(id,0)-=100;
                state->power_taps=state->power_window=0;
            }
        }
    }
    spirit_fade_tick();
    /* Re-evaluate after rescue; every enabled seat must be a spirit. */
    spirits=0;for(unsigned id=0;id<party_count();++id)spirits+=partner[id].ghost!=0;
    if(spirits==party_count()) {
        if(!team_over && ++wipe_frames>=180 && !rb_request_precise()){team_over=1;U32(0x4cccd0)=0;((void (__cdecl *)(void))0x458a30)();}
    }else wipe_frames=0;
}

static int shared_supply(unsigned type) {
    /* Ordinary small/large P(1/3), money(2) and score-only drops stay under
       the reference rules. Resource-card pickups16..19 are not shop choices. */
    return (type>=4 && type<=8) || (type>=16 && type<=19);
}
static void share_supply(unsigned collector,unsigned type) {
    if(!ship(1) || !shared_supply(type))return;
    unsigned saved_slot=guest_slot;
    for(unsigned id=0;id<party_count();++id)if(id!=collector && ship(id)) {
        if(id){guest_slot=id-1;enter_p2(0);}
        void *game=(void *)0x4cccdc;
        if(type==4)((void (TC *)(void *,unsigned))0x457570)(game,0);
        else if(type==5) {
            /* A complete life pickup does not advance the fragment threshold. */
            uint32_t threshold=U32(0x4ccd50);
            ((void (TC *)(void *))0x4575f0)(game);U32(0x4ccd50)=threshold;
        }else if(type==6)((void (TC *)(void *,unsigned))0x4576e0)(game,0);
        else if(type==7)((void (TC *)(void *,unsigned))0x457690)(game,0);
        else if(type==8)((int (TC *)(void *,unsigned))0x4573f0)(game,U32(0x4ccd3c));
        else {
            void *cards=PTR(0x4cf298);
            if(cards)((int (TC *)(void *,int,int))0x411460)(cards,(int)U32(0x4b4020+type*8),0);
        }
        if(id)leave_p2(0);
        /* A shared life/third fragment can return a spirit to play. Keep the
           awarded spare lives; native respawn supplies the normal protection. */
        if(partner[id].ghost && *resource(id,0x10)>=0) {
            int lives=*resource(id,0x10);revive(id);*resource(id,0x10)=lives;
        }
    }
    guest_slot=saved_slot;
}
#include "native_supply_scenario.h"
/* Explicit no-commit local qualification only. A synchronized test chord is
   consumed here, never sent to the native game. Ordinary sessions cannot
   enable this fixture. It invokes original dialogue/scene constructors. */
static void gameplay_test_before(int enabled,uint32_t *keys) {
    static unsigned command,held;
    if(!enabled)return;
    int trigger=(keys[0]&0xcfb)==0xcfb;
    if(trigger)keys[0]=0;
    if(!p1 || !ship(1) || (party_count()==3 && !ship(2)))return;
    for(unsigned id=0;id<party_count();++id)if(!partner[id].ghost) {
        FIELD(ship(id),0x47778)=240;FLT(ship(id),0x4777c)=240;FIELD(ship(id),0x47774)=239;
    }
    if(!trigger){held=0;return;}if(held)return;held=1;
    void *gui=PTR(0x4cf2e0);if(!gui)return;
    if(!(command&1)) {
        if(!command) {
            unsigned old=guest_slot;guest_slot=0;enter_p2(0);
            const int ids[]={9,42,43};
            for(unsigned n=0;n<3;++n)((int (TC *)(void *,int,int))0x411460)(guest_cards,ids[n],1);
            leave_p2(0);guest_slot=old;
        }
        if(!stage_dialogue())((void (TC *)(void *,int))0x43e360)(gui,0);
        log_line("GAMEPLAY_TEST native dialogue stage=%lu command=%u",(unsigned long)U32(0x4cccdc),command);
    }else {
        void *dialogue=stage_dialogue();
        if(dialogue){((void *(TC *)(void *,void *))0x43b540)(dialogue,dialogue);FIELD(gui,0x1b0)=0;}
        const unsigned stages[]={4,5,6,7};unsigned index=command/2;
        if(index<4){unsigned stage=stages[index];U32(0x4cccdc)=stage;PTR(0x4cf428)=(void *)(0x4c9410+stage*0xd4);U32(0x4cd5e8)=12;
            log_line("GAMEPLAY_TEST injected stage=%u",stage);}
    }
    ++command;
}
static void item_leave(void) {
    unsigned collector=UINT32_MAX,type=0;
    if(scoped_item && item_scope && coop_pickup_confirmed && FIELD(scoped_item,0xc74)==0) {
        unsigned id=(unsigned)(item_scope-1);
        if(supply_test.item==scoped_item)supply_test.collector=id;
        int index=item_index(item_manager,scoped_item);
        if(index>=0 && transfer_owner[index]==-1 && !transfer_power[index]) {
            collector=id;type=FIELD(scoped_item,0xc78);
        }
        if(index>=0 && transfer_power[index]>0) {
            uint32_t power=item_power_before+(uint32_t)transfer_power[index];
            U32(0x4ccd38)=power>U32(0x4ccd3c)?U32(0x4ccd3c):power;
        }
        if(index>=0) {transfer_owner[index]=-1;transfer_power[index]=0;}
        ++collected[id];
        if(collected[id]<=12)
            log_line("P%u collected item type=%lu power=%lu lives=%ld",id+1,
                (unsigned long)FIELD(scoped_item,0xc78),(unsigned long)U32(0x4ccd38),(long)(int32_t)U32(0x4ccd48));
    }
    if(item_scope>=2) {leave_p2(0);U32(0x4ca428)=item_original_input;guest_slot=item_saved_slot;}
    scoped_item=NULL;
    item_scope=0;coop_pickup_confirmed=0;
    /* Always restore the collector's bank before visiting other players. */
    if(collector!=UINT32_MAX)share_supply(collector,type);
}

/* Called from a register/FPU preserving bridge at the original per-item loop.
   esi is the native cached player; ecx and the other registers stay untouched. */
void __cdecl coop_select_item(uint32_t *regs) {
    item_leave();if(!ship(1) || !item_manager)return;
    void *item=(void *)(uintptr_t)regs[0];int index=item_index(item_manager,item);if(index<0)return;
    uint32_t state=FIELD(item,0xc74),age=FIELD(item,0xc50),type=FIELD(item,0xc78);
    if(state==0){item_owner[index]=transfer_owner[index]=-1;transfer_power[index]=0;last_age[index]=0;return;}
    if(age<last_age[index]){item_owner[index]=transfer_owner[index]=-1;transfer_power[index]=0;}
    last_age[index]=age;
    int owner=item_owner[index];
    if(transfer_owner[index]>=0 && alive((unsigned)transfer_owner[index]))owner=transfer_owner[index];
    else {
        if(transfer_owner[index]>=0)transfer_owner[index]=-2; /* orphaned gift, never a natural team reward */
        if(owner<0 || (unsigned)owner>=party_count() || !alive((unsigned)owner) || (state!=3 && state!=4)) {
            int candidates[3],count=0,nearest=-1;float best=0;
            for(unsigned id=0;id<party_count();++id)if(alive(id)) {
                if(FLT(ship(id),0x624)<FLT(ship(id),0x47998))candidates[count++]=(int)id;
                float dx=FLT(item,0xc2c)-FLT(ship(id),0x620),dy=FLT(item,0xc30)-FLT(ship(id),0x624),distance=dx*dx+dy*dy;
                if(nearest<0 || distance<best){nearest=(int)id;best=distance;}
            }
            owner=count?candidates[(unsigned)index%(unsigned)count]:nearest>=0?nearest:0;
        }
    }
    if(transfer_owner[index]<0 && (type==1 || type==3 || type==8)) {
        int needs[3]={0},any=0,selected=-1,collect=0;float best=0;
        for(unsigned id=0;id<party_count();++id) {needs[id]=alive(id) && *resource(id,0)<*resource(id,4);any|=needs[id];}
        int locked=owner>=0 && (state==3 || state==4) && alive((unsigned)owner) && needs[owner];
        for(unsigned id=0;id<party_count();++id)if(alive(id)) {
            float dx=FLT(item,0xc2c)-FLT(ship(id),0x620),dy=FLT(item,0xc30)-FLT(ship(id),0x624),distance=dx*dx+dy*dy;
            if(FLT(ship(id),0x624)<FLT(ship(id),0x47998))collect=1;
            float radius=FLT(ship(id),0x4798c);if(!needs[id] && radius>0 && distance<radius*radius)collect=1;
            if(any && !needs[id])continue;
            if(selected<0 || distance<best){selected=(int)id;best=distance;}
        }
        if(!locked && selected>=0)owner=selected;
        if(owner>=0 && needs[owner] && (collect || state==3 || state==4)) {
            FIELD(item,0xc74)=4;float speed=FLT(ship((unsigned)owner),0x47988);FLT(item,0xc80)=speed>0?speed:5;
        }
    }
    item_owner[index]=(int8_t)owner;scoped_item=item;
    if(owner>0) {
        item_saved_slot=guest_slot;guest_slot=(unsigned)owner-1;enter_p2(0);item_scope=owner+1;
        item_original_input=U32(0x4ca428);U32(0x4ca428)=previous_p2;
    }else item_scope=1;
    item_power_before=U32(0x4ccd38);regs[1]=(uint32_t)(uintptr_t)ship((unsigned)owner);
}

static int TC items_tick(void *manager) {
    item_manager=manager;
    if(!current_guest) return item_tick(manager);
    int result=item_tick(manager);
    item_leave();
    PTR(0x4cf410)=p1;
    return result;
}

static void *TC destroy_stage_player(void *player,int unused) {
    typedef void *(TC *Destroy)(void *,int);
    if(player==p1) {
        shop_pending_owner=0;shop_owner=0;
        departed_stage=observed_stage?observed_stage:U32(0x4cccdc);departed_ghosts=0;
        if(U32(0x4cccc8)&1)for(unsigned id=0;id<party_count();++id)if(ship(id) && (partner[id].ghost || *resource(id,0x10)<0))departed_ghosts|=1u<<id;
        if(hud_node){((void (TC *)(void *,void *))0x4015a0)(PTR(0x4cf294),hud_node);hud_node=NULL;}
        if(dialogue_hud_node){((void (TC *)(void *,void *))0x4015a0)(PTR(0x4cf294),dialogue_hud_node);dialogue_hud_node=NULL;}
        for(unsigned id=1;id<party_count();++id) {
            guest_slot=id-1;
            if(current_guest) {
                enter_p2(0);
                if(guest_bomb) {
                    void *vtable=(void *)(uintptr_t)FIELD(guest_bomb,0);
                    ((void *(TC *)(void *,int))(uintptr_t)FIELD(vtable,0))(guest_bomb,1);guest_bomb=NULL;
                }
                ((Destroy)0x45b020)(current_guest,0);
                void *cached=PTR(0x570920);
                if(cached && cached!=(void *)(uintptr_t)FIELD(p1,0x47940)){((void (__cdecl *)(void *))0x491a3f)(cached);PTR(0x570920)=NULL;}
                leave_p2(0);current_guest=NULL;
                if(guest_anm) {
                    void *anm=PTR(0x51f65c);
                    if(U32(0x4cccc8)&1)((void (TC *)(void *,int))0x486fc0)(anm,(int)guest_file_id);
                    else FIELD(anm,0x312072c+guest_file_id*4)=0;
                    guest_anm=NULL;
                }
            }else if(guest_anm){FIELD(guest_anm,0x12c)=1;guest_anm=NULL;}
        }
        guest_slot=0;team_over=0;frames=0;guest_ticks=0;
    }
    void *result=((Destroy)0x45b020)(player,unused);
    p1=NULL;attached=0;item_manager=NULL;if(wanted)requested=1;return result;
}

static void * __cdecl create_host(void) {
    if(!creating_guest) {
        int carry=have_guest_resources && U32(0x4cccdc)==last_stage+1 && U32(0x4cccdc)<=6;
        if(!carry) {
            if(settings_path[0] && !draft_ready) {
                chosen_character=(int)GetPrivateProfileIntW(L"Player2",L"Character",1,settings_path);
                if(chosen_character<0 || chosen_character>3) chosen_character=-1;
                custom_cards=GetPrivateProfileIntW(L"Player2",L"CustomCards",1,settings_path)!=0;
                chosen_count=2;chosen_cards[2]=-1;
                const wchar_t *keys[2]={L"Card1",L"Card2"};
                for(unsigned i=0;i<2;++i) {
                    chosen_cards[i]=(int)GetPrivateProfileIntW(L"Player2",keys[i],-1,settings_path);
                    int id=chosen_cards[i],valid_card=0;
                    const uint32_t *desc=(const uint32_t *)0x4c53c0;
                    for(unsigned n=0;n<57;++n,desc+=13) if((int)desc[1]==id && id<56 && desc[8]) valid_card=1;
                    for(unsigned n=0;n<i;++n) if(chosen_cards[n]==id) valid_card=0;
                    if(!valid_card) chosen_cards[i]=-1;
                }
            }
            /* Native stage init 442785 establishes the original initial Bomb
               count before this first player constructor. New lives read it
               at 45d1cb; reduce the base once per new run, not per stage. */
            if(!(U32(0x4cccc8)&0xc0) && U32(0x4ccd60)>1){--U32(0x4ccd60);if(U32(0x4ccd58)>0)--U32(0x4ccd58);}
            memcpy(run_base_resources,(void *)0x4ccd38,sizeof(run_base_resources));
            run_base_funds=U32(0x4ccd34);have_run_base=1;
        }
    }
    void *player=((CreatePlayer)player_create_original)();
    if(player && !creating_guest) {
        p1=player;p1_tick=(Tick)0x45caa0;
        FIELD((void *)(uintptr_t)FIELD(player,4),8)=(uint32_t)(uintptr_t)host_tick;
        void *draw=(void *)(uintptr_t)FIELD(player,8);
        p1_draw=(Tick)(uintptr_t)FIELD(draw,8);FIELD(draw,8)=(uint32_t)(uintptr_t)host_draw;
        log_line("Bound host callback P1=%p mode=%08lx",player,(unsigned long)U32(0x4cccc8));
    }
    return player;
}

static void * __cdecl create_items(void) {
    void *manager=((CreatePlayer)item_create_original)();
    if(manager) {
        item_tick=(Tick)0x446ec0;
        FIELD((void *)(uintptr_t)FIELD(manager,4),8)=(uint32_t)(uintptr_t)items_tick;
        log_line("Bound item manager %p",manager);
    }
    return manager;
}

static void *TC spawn_item(void *manager,uint32_t type,const float *position,
                          uint32_t a2,uint32_t angle,uint32_t speed,uint32_t delay,uint32_t a6,uint32_t a7) {
    typedef void *(TC *Spawn)(void *,uint32_t,const float *,uint32_t,uint32_t,uint32_t,uint32_t,uint32_t,uint32_t);
    unsigned count=1;
    if(ship(1)) {
        if(type==1 || type==3)count=party_count();
        /* Supplies are awarded to every seat on pickup, so they no longer
           need extra copies. Ordinary P retains the reference multiplier. */
    }
    void *first=NULL;
    for(unsigned n=0;n<count;++n) {
        float offset=count>1?((float)n-(count-1)*.5f)*18.0f:0;
        float at[3]={position[0]+offset,position[1],position[2]};
        void *item=((Spawn)spawn_original)(manager,type,at,a2,angle,speed,delay,a6,a7);
        if(!n)first=item;
        int index=item?item_index(manager,item):-1;
        if(index>=0){transfer_owner[index]=-1;transfer_power[index]=0;item_owner[index]=-1;last_age[index]=0;}
    }
    return first;
}

static int patch_branch(uintptr_t address,void *target,unsigned length,unsigned char opcode,const unsigned char *expected) {
    if(memcmp((void *)address,expected,length)) return 0;
    DWORD old;
    if(!VirtualProtect((void *)address,length,PAGE_EXECUTE_READWRITE,&old)) return 0;
    unsigned char code[16];memset(code,0x90,length);code[0]=opcode;
    *(int32_t *)(code+1)=(int32_t)((uintptr_t)target-address-5);
    memcpy((void *)address,code,length);FlushInstructionCache(GetCurrentProcess(),(void *)address,length);
    VirtualProtect((void *)address,length,old,&old);
    return 1;
}

static void *trampoline(uintptr_t address,unsigned length,const unsigned char *expected) {
    if(memcmp((void *)address,expected,length)) return NULL;
    unsigned char *code=VirtualAlloc(NULL,length+5,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE);
    if(!code) return NULL;
    memcpy(code,(void *)address,length);code[length]=0xe9;
    *(int32_t *)(code+length+1)=(int32_t)(address+length-(uintptr_t)code-length-5);
    DWORD old;VirtualProtect(code,length+5,PAGE_EXECUTE_READ,&old);
    FlushInstructionCache(GetCurrentProcess(),code,length+5);
    return code;
}

/* These bridges replace ONLY native aim reads. Collision, player update and
   inventory callbacks retain their original scoped singleton. Tie by seat so
   all peers make the same choice, without consuming the native RNG. */
void __cdecl coop_nearest_aim(uint32_t *regs,const float *origin,unsigned output) {
    void *best=PTR(0x4cf410);float distance=INFINITY;
    if(ship(1))for(unsigned id=0;id<party_count();++id) {
        void *player=ship(id);
        if(!player || partner[id].ghost || FIELD(player,0x476ac)!=1)continue;
        float x=FLT(player,0x620)-origin[0],y=FLT(player,0x624)-origin[1],d=x*x+y*y;
        if(d<distance){distance=d;best=player;}
    }
    regs[output]=(uint32_t)(uintptr_t)best;
}
void __cdecl coop_any_bomb(uint32_t *regs) {
    void *bomb=PTR(0x4cf2b8);
    /* Native state 1 is the damaging Bomb interval. Ending effects do not
       keep EX immune. Simultaneous Bombs remain active until the last ends. */
    if(ship(1) && (!bomb || FIELD(bomb,0x30)!=1))
        for(unsigned id=1;id<party_count();++id)
            if(guests[id-1].bomb && FIELD(guests[id-1].bomb,0x30)==1){bomb=guests[id-1].bomb;break;}
    regs[7]=(uint32_t)(uintptr_t)bomb;
}
static int install_party_aim(void) {
    const unsigned char eax[]={0xa1,0x10,0xf4,0x4c,0x00};
    const unsigned char ecx[]={0x8b,0x0d,0x10,0xf4,0x4c,0x00};
    const unsigned char homing[]={0x8b,0x86,0x90,0x06,0x00,0x00};
    const unsigned char bomb[]={0xa1,0xb8,0xf2,0x4c,0x00};
    const struct {uintptr_t address;void *bridge;} sites[]={
        {0x425fdb,aim_spawn_bridge},{0x42615a,aim_redirect1_bridge},
        {0x42679b,aim_redirect2_bridge},{0x426a67,aim_redirect3_bridge},
        {0x427104,aim_redirect4_bridge},{0x427347,aim_redirect5_bridge},
        {0x427819,aim_pattern_bridge},{0x42848e,aim_tick_bridge},
        {0x44963e,aim_laser_bridge},{0x4374ad,aim_ecl_x_bridge},
        {0x4374c0,aim_ecl_y_bridge},{0x4380f7,aim_ecl_fx_bridge},{0x438108,aim_ecl_fy_bridge},
        {0x4375d5,aim_ecl_distance_bridge},{0x43836e,aim_ecl_fdistance_bridge},
        {0x4252fe,aim_emit_distance_bridge},{0x44aad9,aim_laser_radial_bridge},
        {0x44cf93,aim_laser_curve_bridge},{0x449ad5,aim_laser_emit_bridge},{0x451026,aim_laser_spawn_bridge}
    };
    const struct {uintptr_t address;void *bridge;} cx_sites[]={
        {0x43345b,aim_ecl_side_bridge},{0x435ead,aim_ecl_angle1_bridge},{0x435eb8,aim_ecl_angle2_bridge}
    };
    /* Verify the entire set before changing the first instruction. */
    for(unsigned i=0;i<sizeof(sites)/sizeof(sites[0]);++i)
        if(memcmp((void *)sites[i].address,eax,5))return 0;
    for(unsigned i=0;i<sizeof(cx_sites)/sizeof(cx_sites[0]);++i)
        if(memcmp((void *)cx_sites[i].address,ecx,6))return 0;
    if(memcmp((void *)0x438dfd,homing,6))return 0;
    if(memcmp((void *)0x42ed46,bomb,5)||memcmp((void *)0x42f233,bomb,5))return 0;
    for(unsigned i=0;i<sizeof(sites)/sizeof(sites[0]);++i)
        if(!patch_branch(sites[i].address,sites[i].bridge,5,0xe9,eax))return 0;
    for(unsigned i=0;i<sizeof(cx_sites)/sizeof(cx_sites[0]);++i)
        if(!patch_branch(cx_sites[i].address,cx_sites[i].bridge,6,0xe9,ecx))return 0;
    if(!patch_branch(0x438dfd,aim_homing_bridge,6,0xe9,homing))return 0;
    return patch_branch(0x42ed46,bomb_immunity_bridge,5,0xe9,bomb) &&
           patch_branch(0x42f233,bomb_modifier_bridge,5,0xe9,bomb);
}

int __cdecl coop_damage(const uint32_t *args,float angle) {
    int result=native_damage(args,angle);
    if(!ship(1) || PTR(0x4cf410)!=p1)return result;
    if(result>0)damage_done[0]+=(unsigned)result;
    unsigned old=guest_slot;
    uint32_t bomb_hit=args[3]?*(uint32_t *)(uintptr_t)args[3]:0;
    for(unsigned id=1;id<party_count();++id)if(ship(id)) {
        guest_slot=id-1;enter_p2(0);int value=native_damage(args,angle);
        if(value>0)damage_done[id]+=(unsigned)value;
        if(args[3]) {*(uint32_t *)(uintptr_t)args[3]|=bomb_hit;bomb_hit=*(uint32_t *)(uintptr_t)args[3];}
        leave_p2(0);result+=value;
    }
    guest_slot=old;return result;
}
void __cdecl coop_scale_boss_damage(uint32_t *regs) {
    if(!p1 || !ship(1) || (int32_t)regs[1]<=0)return;
    unsigned active=0;for(unsigned seat=0;seat<party_count();++seat)if(ship(seat) && !partner[seat].ghost)++active;
    if(active<2)return;
    void *enemy=(void *)(uintptr_t)regs[4],*manager=PTR(0x4cf2d0);
    if(!valid(enemy,0x558c) || !valid(manager,0x88))return;
    void *script=(void *)(uintptr_t)FIELD(enemy,0x5588);
    if(!valid(script,0x6834))return;
    uint32_t id=FIELD(script,0x6830);if(!id)return;
    /* Native 42d490 stores the registered Boss ECL IDs at +48..+84.
       Apply after native Bomb/ability modifiers, before HP subtraction. */
    for(unsigned i=0;i<16;++i)if(FIELD(manager,0x48+i*4)==id) {
        regs[1]=active==3?(uint32_t)((uint64_t)regs[1]*2/3):(uint32_t)((uint64_t)regs[1]*3/4);return;
    }
}

static int merge_collision(int a,int b) {return a==1||b==1?1:(a==2||b==2?2:0);}

static int __stdcall coop_rect(uint32_t position,uint32_t size,uint32_t graze_only) {
    int result=((int (__stdcall *)(uint32_t,uint32_t,uint32_t))rect_original)(position,size,graze_only);
    if(!ship(1) || PTR(0x4cf410)!=p1)return result;
    unsigned old=guest_slot;
    for(unsigned id=1;id<party_count();++id)if(ship(id)) {
        guest_slot=id-1;uint32_t before=FIELD(current_guest,0x476ac);enter_p2(0);
        int value=((int (__stdcall *)(uint32_t,uint32_t,uint32_t))rect_original)(position,size,graze_only);if(before!=4 && FIELD(current_guest,0x476ac)==4)++hits[id];
        leave_p2(0);result=merge_collision(result,value);
    }
    guest_slot=old;return result;
}

int __cdecl coop_circle(const uint32_t *args,float radius) {
    int result=native_circle(args,radius);
    if(!ship(1) || PTR(0x4cf410)!=p1)return result;
    unsigned old=guest_slot;
    for(unsigned id=1;id<party_count();++id)if(ship(id)) {
        guest_slot=id-1;uint32_t before=FIELD(current_guest,0x476ac);enter_p2(0);
        int value=native_circle(args,radius);if(before!=4 && FIELD(current_guest,0x476ac)==4)++hits[id];
        leave_p2(0);result=merge_collision(result,value);
    }
    guest_slot=old;return result;
}

int __cdecl coop_laser(const uint32_t *args,float angle,float width) {
    int result=native_laser(args,angle,width);
    if(!ship(1) || PTR(0x4cf410)!=p1)return result;
    unsigned old=guest_slot;
    for(unsigned id=1;id<party_count();++id)if(ship(id)) {
        guest_slot=id-1;uint32_t before=FIELD(current_guest,0x476ac);enter_p2(0);
        int value=native_laser(args,angle,width);if(before!=4 && FIELD(current_guest,0x476ac)==4)++hits[id];
        leave_p2(0);result=merge_collision(result,value);
    }
    guest_slot=old;return result;
}

#include "online_runtime.h"
#include "native_replay.h"

static DWORD WINAPI monitor(void *unused) {
    (void)unused;
    GetModuleFileNameW(module,log_path,MAX_PATH);
    wchar_t *slash=wcsrchr(log_path,L'\\');
    if(slash) wcscpy(slash+1,L"coop.log");
    wchar_t config_path[MAX_PATH];wcscpy(config_path,log_path);
    wchar_t *config_name=wcsrchr(config_path,L'\\');
    if(config_name) wcscpy(config_name+1,L"coop.ini");
    wcscpy(settings_path,config_path);kb_load(config_path);
    stamp_cache_guard=GetPrivateProfileIntW(L"Graphics",L"StampCacheGuard",0,config_path)!=0;
    int configured_alpha=(int)GetPrivateProfileIntW(L"Graphics",L"GhostAlpha",80,config_path);
    ghost_alpha=configured_alpha<0?0:configured_alpha>255?255:(unsigned)configured_alpha;
    native_draft=GetPrivateProfileIntW(L"Selection",L"NativeSequential",1,config_path)!=0;
    log_line("TH18 local co-op " COOP_VERSION " experimental loader, PID=%lu",(unsigned long)GetCurrentProcessId());
    log_line("Menu stamp cache guard=%d (visual recheck pending)",stamp_cache_guard);
    const unsigned char signature[]={0x55,0x8b,0xec,0x83,0xec,0x08,0x56,0x57,0x68,0xd0,0x79,0x04,0x00};
    if (!valid((void *)0x45af20,sizeof(signature)) || memcmp((void *)0x45af20,signature,sizeof(signature))) {
        log_line("Unsupported running executable: constructor signature differs. No changes made."); return 1;
    }
    wchar_t game_exe[MAX_PATH];GetModuleFileNameW(NULL,game_exe,MAX_PATH);
    if(!valid((void *)0x568c61,4096) || !data_path_matches((char *)0x568c61,game_exe)) {
        log_line("Private co-op data path missing; no hooks installed. Start via co-op launcher.");return 1;
    }
    log_line("Private data path verified: %s",(char *)0x568c61);
    int online=online_initialise();if(online<0){log_line("Invalid online bridge; no hooks installed");return 1;}
    if(!rb_audit_install()){log_line("Rollback heap audit setup failed; no gameplay hooks installed");return 1;}
    wchar_t name[100]; swprintf(name,100,L"Local\\TH18LocalCoop-%lu-activate",(unsigned long)GetCurrentProcessId());
    /* Only exact instruction signatures of this verified 1.00a profile are patched. */
    const unsigned char stage_call[]={0xe8,0x58,0x7b,0x01,0x00};
    const unsigned char boss_damage_start[]={0xf6,0x83,0x30,0x51,0x00,0x00,0x10};
    const unsigned char loop[]={0x8b,0x87,0x74,0x0c,0x00,0x00};
    const unsigned char picked[]={0xc7,0x87,0x74,0x0c,0x00,0x00,0x00,0x00,0x00,0x00};
    const unsigned char damage_start[]={0x53,0x8b,0xdc,0x83,0xec,0x08};
    const unsigned char rect_start[]={0x55,0x8b,0xec,0x8b,0x15,0x10,0xf4,0x4c,0x00};
    const unsigned char circle_start[]={0x55,0x8b,0xec,0x8b,0x0d,0x10,0xf4,0x4c,0x00};
    const unsigned char create_player_start[]={0x55,0x8b,0xec,0x83,0xec,0x08};
    const unsigned char create_items_start[]={0x55,0x8b,0xec,0x6a,0xff};
    const unsigned char spawn_start[]={0x55,0x8b,0xec,0x51,0x53};
    const unsigned char cards_destroy_start[]={0x55,0x8b,0xec,0x6a,0xff};
    const unsigned char cards_ui_start[]={0x55,0x8b,0xec,0x83,0xec,0x10};
    const unsigned char shop_init_start[]={0x55,0x8b,0xec,0x81,0xec,0xf0,0x00,0x00,0x00};
    const unsigned char shop_tick_start[]={0x55,0x8b,0xec,0x83,0xec,0x48};
    const unsigned char shop_draw_start[]={0x55,0x8b,0xec,0xa1,0x0c,0xf4,0x4c,0x00};
    const unsigned char stamp_draw_start[]={0x55,0x8b,0xec,0x81,0xec,0xe0,0x00,0x00,0x00};
    const unsigned char menu_tick_start[]={0x55,0x8b,0xec,0x83,0xec,0x10};
    const unsigned char menu_draw_start[]={0x55,0x8b,0xec,0x83,0xe4,0xf8};
    const unsigned char menu_input_start[]={0x53,0x56,0x8b,0xf1,0xb9,0x01,0x00,0x00,0x00};
    const unsigned char online_tick_start[]={0x55,0x8b,0xec,0x51,0x80,0x3d,0xbe,0x17,0x52,0x00,0x00};
    const unsigned char scene_draw_start[]={0x55,0x8b,0xec,0x51,0x80,0x3d,0xbe,0x17,0x52,0x00,0x00};
    const unsigned char anm_draw_start[]={0x55,0x8b,0xec,0x83,0xe4,0xf8,0x83,0xec,0x78};
    const unsigned char pause_actions_start[]={0x55,0x8b,0xec,0x83,0xe4,0xf8};
    online_tick_original=trampoline(0x4012e0,11,online_tick_start);
    scene_draw_original=trampoline(0x401420,11,scene_draw_start);
    anm_draw_original=trampoline(0x481210,9,anm_draw_start);
    pause_actions_original=trampoline(0x458e40,6,pause_actions_start);
    menu_tick_original=trampoline(0x464ed0,6,menu_tick_start);
    menu_draw_original=trampoline(0x465870,6,menu_draw_start);
    menu_input_original=trampoline(0x404e90,9,menu_input_start);
    if(stamp_cache_guard) stamp_draw_original=trampoline(0x480160,9,stamp_draw_start);
    cards_destroy_original=trampoline(0x4084c0,5,cards_destroy_start);
    cards_ui_original=trampoline(0x408d00,6,cards_ui_start);
    shop_init_original=trampoline(0x4171b0,9,shop_init_start);
    shop_close_original=trampoline(0x417880,6,damage_start);
    shop_tick_original=trampoline(0x417cc0,6,shop_tick_start);
    shop_draw_original=trampoline(0x418990,8,shop_draw_start);
    damage_original=trampoline(0x45f0f0,6,damage_start);
    rect_original=trampoline(0x45cc20,9,rect_start);
    circle_original=trampoline(0x45cd90,9,circle_start);
    laser_original=trampoline(0x45cea0,6,damage_start);
    player_create_original=trampoline(0x45af20,6,create_player_start);
    item_create_original=trampoline(0x445820,5,create_items_start);
    spawn_original=trampoline(0x446f40,5,spawn_start);
    if(!pause_actions_original || !scene_draw_original || !anm_draw_original || !damage_original || !rect_original || !circle_original || !laser_original || !player_create_original || !item_create_original || !spawn_original ||
       !cards_destroy_original || !cards_ui_original || !shop_init_original || !shop_close_original || !shop_tick_original || !shop_draw_original || (stamp_cache_guard && !stamp_draw_original) ||
       !menu_tick_original || !menu_draw_original || !menu_input_original || (online && (!online_tick_original || !scene_draw_original))) {
        log_line("Damage/collision signatures differ; no patches installed");return 1;
    }
    if(!rp_install()){log_line("Replay signatures differ; no gameplay hooks installed");return 1;}
    if(GetPrivateProfileIntW(L"Diagnostics",L"InputTrace",1,config_path))open_input_trace();
    if(!patch_branch(0x4434c3,destroy_stage_player,5,0xe8,stage_call)) {
        log_line("Stage-cleanup signature mismatch; extension remains inactive");return 1;
    }
    if(!patch_branch(0x445ae0,item_loop_bridge,6,0xe9,loop) ||
       !patch_branch(0x446595,item_pickup_bridge,10,0xe9,picked)) {
        log_line("Item-loop signature mismatch; extension remains inactive");return 1;
    }
    if(!patch_branch(0x42f2b9,boss_damage_bridge,7,0xe9,boss_damage_start)) {
        log_line("Boss damage signature differs; extension remains inactive");return 1;
    }
    if(!install_party_aim()){log_line("Party aim/Bomb signatures differ; extension remains inactive");return 1;}
    if(!patch_branch(0x45f0f0,damage_bridge,6,0xe9,damage_start) ||
       !patch_branch(0x458e40,coop_pause_actions,6,0xe9,pause_actions_start) ||
       !patch_branch(0x481210,anm_draw,9,0xe9,anm_draw_start) ||
       !patch_branch(0x45cc20,coop_rect,9,0xe9,rect_start) ||
       !patch_branch(0x45cd90,circle_bridge,9,0xe9,circle_start) ||
       !patch_branch(0x45cea0,laser_bridge,6,0xe9,damage_start) ||
       !patch_branch(0x45af20,create_host,6,0xe9,create_player_start) ||
       !patch_branch(0x445820,create_items,5,0xe9,create_items_start) ||
       !patch_branch(0x446f40,spawn_item,5,0xe9,spawn_start) ||
       !patch_branch(0x4084c0,destroy_cards,5,0xe9,cards_destroy_start) ||
       !patch_branch(0x408d00,cards_ui,6,0xe9,cards_ui_start) ||
       !patch_branch(0x4171b0,shop_init,9,0xe9,shop_init_start) ||
       !patch_branch(0x417880,shop_close,6,0xe9,damage_start) ||
       !patch_branch(0x417cc0,shop_tick,6,0xe9,shop_tick_start) ||
       !patch_branch(0x418990,shop_draw,8,0xe9,shop_draw_start) ||
       (stamp_cache_guard && !patch_branch(0x480160,draw_menu_stamp,9,0xe9,stamp_draw_start)) ||
       !patch_branch(0x464ed0,native_menu_tick,6,0xe9,menu_tick_start) ||
       !patch_branch(0x465870,native_menu_draw,6,0xe9,menu_draw_start) ||
       !patch_branch(0x404e90,native_menu_input,9,0xe9,menu_input_start) ||
       !patch_branch(0x4012e0,online_tick,11,0xe9,online_tick_start) ||
       !patch_branch(0x401420,scene_draw,11,0xe9,scene_draw_start)) {
        log_line("Unable to install verified damage/collision hooks");return 1;
    }
    activation=CreateEventW(NULL,FALSE,FALSE,name);
    log_line("Verified hooks installed; native constructors will bind callbacks on the game thread");
    return 0;
}

BOOL WINAPI DllMain(HINSTANCE instance,DWORD reason,LPVOID reserved) {
    (void)reserved;
    if(reason==DLL_PROCESS_ATTACH) {
        module=instance; DisableThreadLibraryCalls(instance);
        HANDLE t=CreateThread(NULL,0,monitor,NULL,0,NULL); if(t) CloseHandle(t);
    }
    return TRUE;
}
