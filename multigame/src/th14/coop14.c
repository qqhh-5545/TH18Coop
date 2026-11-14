#define WIN32_LEAN_AND_MEAN
#define COBJMACROS
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#define TC __attribute__((thiscall))
#define U32(a) (*(volatile uint32_t *)(uintptr_t)(a))
#define PTR(a) (*(void **)(uintptr_t)(a))
#define OFF(p,o) ((unsigned char *)(p)+(o))
#define FIELD(p,o) (*(uint32_t *)OFF(p,o))
#define FLT(p,o) (*(float *)OFF(p,o))
#include "../key_bindings.h"
#include "progress14.h"

enum { PLAYER=0x4db67c,BOMB=0x4db52c,ANM=0x4f56cc,GAME_INPUT=0x4d6a90,
       RES=0x4f5830,CHARA=0x4f5828,STYLE=0x4f582c,STAGE=0x4f58a4,GAME_FLAGS=0x4f58b8 };
typedef int (TC *Callback)(void *);
typedef struct {
    void *player,*bomb,*anm;
    Callback tick,draw,bomb_tick,bomb_draw;
    uint32_t character,style,previous,stage;
    unsigned char resources[0x70];
    int has_resources,ghost,life_taps,power_taps,window,release;
    float drift_x,drift_y;
} Seat;
static Seat seats[3];
static unsigned hud_started14;
static unsigned players=2,scope_owner,scope_depth,frame,creating,ending;
static unsigned char saved_resources[0x70],saved_input[0x230];
static uint32_t saved_character,saved_style,sampled[3];
static void *saved_player,*saved_bomb;
static void *create_original,*destroy_original,*input_original,*root_original,*item_original;
static void *dying_original;
static void *bomb_create_original,*bomb_destroy_original;
static void *reset_original,*stage_clear_original;
static void *menu_tick_original,*menu_state_original,*menu_sub_original;
static unsigned draft_owner,draft_pending,draft_ready,draft_release;
static uint32_t draft_char[3],draft_style[3],draft_previous;
extern void menu14_fade_bridge(void);
static unsigned char draft_menu_base[0xd8];static unsigned draft_base_ready;
static int8_t item_owners[0x1258];
static int8_t gift_owners[0x1258];
static int gift_power[0x1258],item_power_before;
static uint8_t fixture_item14[0x1258];
static unsigned item_index_current,gift_spawning;
static void *spawn_original;
static unsigned item_owner,item_kind;
static void *current_item;
static uint32_t item_saved_input;
volatile uint32_t item14_pending;
uint32_t coop14_battle;
void *damage_original,*circle_original,*laser_original,*laser2_original,*rect_original;
void *scale_original;
extern void native_scale14(float),scale14_bridge(void);
static wchar_t log_path[MAX_PATH];
static HMODULE module;
static HANDLE test_mapping;
static HANDLE ready_event;
/* Explicit no-commit test process only: inputs, observation and normal quit.
   This is local IPC, never a remote command or an OS keyboard injection. */
typedef struct {uint32_t magic,keys[3],quit,frame,stage,menu_state,menu_sub,character,style;
    uint32_t present,states[3],lives[3],bombs[3],power[3];float pos[3][2];
    uint32_t command,args[4],ack,ghosts,life_frag[3],bomb_frag[3],iframes[3],characters[3],styles[3],pause_state,pause_choice,disabled_continue,spawn_count;
    uint32_t focused_markers[3],active_bombs[3];
    uint32_t damage_events[3],damage_raw[3],damage_scaled[3],boss_hp[4];
    float player_scale[3];
} TestControl;
static TestControl *test;
static unsigned test_ticks14=1;
static void fixture14_before(void);
static void rules14_reset(void);
static void log_line(const char *format,...) {
    FILE *f=_wfopen(log_path,L"ab");if(!f)return;
    fprintf(f,"[TH14 PID=%lu frame=%u] ",GetCurrentProcessId(),frame);
    va_list args;va_start(args,format);vfprintf(f,format,args);va_end(args);fputc('\n',f);fclose(f);
}
static int valid(void *p,size_t n);
static LONG CALLBACK exception_trace(EXCEPTION_POINTERS *e) {
    if(e->ExceptionRecord->ExceptionCode==EXCEPTION_ACCESS_VIOLATION) {
        CONTEXT *c=e->ContextRecord;
        log_line("AV eip=%08lx eax=%08lx ecx=%08lx edx=%08lx ebx=%08lx esi=%08lx edi=%08lx esp=%08lx scope=%u creating=%u access=%lu address=%08lx",c->Eip,c->Eax,c->Ecx,c->Edx,c->Ebx,c->Esi,c->Edi,c->Esp,scope_owner,creating,e->ExceptionRecord->ExceptionInformation[0],e->ExceptionRecord->ExceptionInformation[1]);
        if(valid((void *)(uintptr_t)c->Esp,64)){uint32_t *s=(void *)(uintptr_t)c->Esp;log_line("stack %08x %08x %08x %08x %08x %08x %08x %08x",s[0],s[1],s[2],s[3],s[4],s[5],s[6],s[7]);}
    }
    return EXCEPTION_CONTINUE_SEARCH;
}
static int valid(void *p,size_t n) {
    if(!p)return 0;MEMORY_BASIC_INFORMATION m;
    return VirtualQuery(p,&m,sizeof(m)) && m.State==MEM_COMMIT && !(m.Protect&(PAGE_GUARD|PAGE_NOACCESS)) && (uintptr_t)p+n<=(uintptr_t)m.BaseAddress+m.RegionSize;
}
static int32_t *resource(unsigned id,unsigned addr) {
    return (int32_t *)(id?seats[id].resources+(addr-RES):(unsigned char *)(uintptr_t)addr);
}
static int living(unsigned id){return seats[id].player && !seats[id].ghost && FIELD(seats[id].player,0x684)<=1;}
static void enter(unsigned id,int input) {
    if(!id)return;
    if(scope_depth++){if(scope_owner!=id)log_line("ERROR nested owner conflict %u/%u",scope_owner,id);return;}
    scope_owner=id;saved_player=PTR(PLAYER);saved_bomb=PTR(BOMB);
    saved_character=U32(CHARA);saved_style=U32(STYLE);memcpy(saved_resources,(void *)RES,sizeof(saved_resources));
    PTR(PLAYER)=seats[id].player;PTR(BOMB)=seats[id].bomb;U32(CHARA)=seats[id].character;U32(STYLE)=seats[id].style;
    memcpy((void *)RES,seats[id].resources,sizeof(saved_resources));
    if(input){memcpy(saved_input,(void *)GAME_INPUT,sizeof(saved_input));U32(GAME_INPUT)=sampled[id];U32(GAME_INPUT+4)=seats[id].previous;
        U32(GAME_INPUT+8)=0;U32(GAME_INPUT+12)=sampled[id]&~seats[id].previous;U32(GAME_INPUT+16)=seats[id].previous&~sampled[id];}
}
static void leave(unsigned id,int input) {
    if(!id || !scope_depth)return;if(--scope_depth)return;
    memcpy(seats[id].resources,(void *)RES,sizeof(saved_resources));
    memcpy((void *)RES,saved_resources,sizeof(saved_resources));PTR(PLAYER)=saved_player;PTR(BOMB)=saved_bomb;
    U32(CHARA)=saved_character;U32(STYLE)=saved_style;
    if(input)memcpy((void *)GAME_INPUT,saved_input,sizeof(saved_input));scope_owner=0;
}
static unsigned owner_of(void *player){for(unsigned i=0;i<players;++i)if(seats[i].player==player)return i;return 0;}
void coop_scale14(float scale) {
    if(!coop14_battle || scope_depth){native_scale14(scale);return;}
    for(unsigned id=0;id<players;++id)if(seats[id].player){enter(id,0);native_scale14(scale);leave(id,0);}
    if(test)log_line("Native world scale=%g applied to all seats",(double)scale);
}
static void set_position(void *p,float x,float y) {
    FLT(p,0x5e0)=x;FLT(p,0x5e4)=y;FIELD(p,0x5ec)=(int)(x*128);FIELD(p,0x5f0)=(int)(y*128);
}
static void timer(void *p,unsigned offset,int value){FIELD(p,offset)=(uint32_t)(value-1);FIELD(p,offset+4)=value;FLT(p,offset+8)=(float)value;}
static void clear_focus(void *p) {
    unsigned id=FIELD(p,0xd6d8);if(!id)return;
    void *vm=((void *(TC *)(void *,int))0x47f0a0)(PTR(ANM),id);
    if(vm)FIELD(vm,0x1c)|=0x20;FIELD(p,0xd6d8)=0;
}
static void revive(unsigned id) {
    Seat *s=&seats[id];if(!s->player)return;s->ghost=0;s->life_taps=s->power_taps=0;
    float x=FLT(s->player,0x5e0),y=FLT(s->player,0x5e4);enter(id,0);
    U32(0x4f5864)=0;U32(0x4f5870)=2;U32(0x4f5874)=0;U32(0x4f5858)=U32(0x4f585c)/2;
    ((void (TC *)(void *))0x44cc10)(s->player);FIELD(s->player,0x684)=1;
    timer(s->player,0x68c,0);timer(s->player,0x182c0,240);clear_focus(s->player);set_position(s->player,x,y);leave(id,0);
    log_line("P%u revived",id+1);
}
static void ghost_tick(unsigned id) {
    Seat *s=&seats[id];void *p=s->player;if(!p)return;
    float x=FLT(p,0x5e0),y=FLT(p,0x5e4);
    x+=s->drift_x;y+=s->drift_y;
    if(x < -174){x=-174;s->drift_x=fabsf(s->drift_x);}if(x>174){x=174;s->drift_x=-fabsf(s->drift_x);}
    if(y<300){y=300;s->drift_y=fabsf(s->drift_y);}if(y>416){y=416;s->drift_y=-fabsf(s->drift_y);}
    set_position(p,x,y);clear_focus(p);((int (TC *)(void *))0x46fe50)(OFF(p,0x14));
}
static void detect_ghost(unsigned id) {
    Seat *s=&seats[id];if(!s->player || s->ghost)return;
    if(*resource(id,0x4f5864)<0 && FIELD(s->player,0x684)==2) {
        s->ghost=1;s->drift_x=(id&1)?-.28f:.28f;s->drift_y=-.16f;clear_focus(s->player);
        FIELD(s->player,0x684)=1;log_line("P%u became ghost",id+1);
    }
}
static void TC begin_dying14(void *p) {
    ((void (TC *)(void *))dying_original)(p);
    for(unsigned id=0;id<players;++id)if(seats[id].player==p && FIELD(p,0x684)==4){
        // Native state 4 commits death at timer 8. Start eight ticks earlier
        // to double that window while retaining the native deathbomb path.
        timer(p,0x68c,-8);timer(p,0x182c0,12);break;
    }
}
static int __cdecl stage_clear14(void) {
    if(!coop14_battle)return ((int (__cdecl *)(void))stage_clear_original)();
    void *score=PTR(0x4db68c);unsigned difficulty=U32(0x4f5834),stage=U32(STAGE),characters[3],styles[3],stages[3];
    int credit=score && !scope_depth && draft_ready && (stage==6 || stage==7) && difficulty<=4;
    uint32_t before[2]={0};
    if(credit){for(unsigned id=0;id<players;++id){characters[id]=seats[id].character;styles[id]=seats[id].style;stages[id]=seats[id].stage;}
        unsigned loadout=characters[0]*2+styles[0];if(loadout>=6)credit=0;
        else {before[0]=*progress14_counter(score,loadout,difficulty,0x7c8);before[1]=*progress14_counter(score,loadout,difficulty,0x7e4);}}
    if(!scope_depth && seats[0].player) {
        for(unsigned id=0;id<players;++id)if(seats[id].ghost)revive(id);
        for(unsigned id=1;id<players;++id)if(seats[id].player){enter(id,0);
            ((void (TC *)(void *))0x4358e0)(seats[id].player);
            void *b=seats[id].bomb;if(b && FIELD(b,0x40)){void **table=*(void ***)b;((void (TC *)(void *))table[5])(b);}
            leave(id,0);}
    }
    int result=((int (__cdecl *)(void))stage_clear_original)();
    if(credit && PTR(0x4db68c)==score){
        progress14_counts(score,characters,styles,stages,players,difficulty,stage,0x7c8,before[0]);
        progress14_counts(score,characters,styles,stages,players,difficulty,stage,0x7e4,before[1]);
        log_line("Native stage %u completion counters applied to participating loadouts",stage);
    }
    return result;
}
static void __cdecl reset14(void) {
    unsigned stage=U32(STAGE);int transition=!scope_depth && seats[0].player==PTR(PLAYER) && seats[0].stage>0 && stage==seats[0].stage+1 && stage<=6;
    if(transition)for(unsigned id=0;id<players;++id){seats[id].stage=stage;if(seats[id].ghost)revive(id);}
    ((void (__cdecl *)(void))reset_original)();
    if(transition) {
        for(unsigned id=1;id<players;++id)if(seats[id].player){enter(id,0);((void (__cdecl *)(void))reset_original)();clear_focus(seats[id].player);set_position(seats[id].player,id==1?48.f:-48.f,400.f);leave(id,0);}
        rules14_reset();log_line("All seats reset for native stage %u",stage);
    }
}
static int TC tick_player(void *p);
static int TC draw_player(void *p) {
    unsigned id=owner_of(p);Seat *s=&seats[id];if(!s->draw)return 1;
    enter(id,0);
    int state=FIELD(p,0x684);if(s->ghost)FIELD(p,0x684)=1;
    int result=s->draw(p);FIELD(p,0x684)=state;
    leave(id,0);return result;
}
static int TC tick_bomb(void *b) {
    unsigned id=0;for(unsigned i=1;i<players;++i)if(seats[i].bomb==b)id=i;
    enter(id,0);uint32_t before=FIELD(b,0x40);int result=seats[id].bomb_tick(b);
    if(!before && FIELD(b,0x40))for(unsigned i=0;i<players;++i)if(i!=id && living(i) && (int32_t)FIELD(seats[i].player,0x182c4)<60)timer(seats[i].player,0x182c0,60);
    leave(id,0);return result;
}
static int TC draw_bomb(void *b) {
    unsigned id=0;for(unsigned i=1;i<players;++i)if(seats[i].bomb==b)id=i;
    enter(id,0);int result=seats[id].bomb_draw(b);leave(id,0);return result;
}
static void wrap_node(void *object,unsigned offset,void *callback,Callback *original) {
    void *node=(void *)(uintptr_t)FIELD(object,offset);if(!valid(node,0x28))return;
    if(FIELD(node,8)!=(uint32_t)(uintptr_t)callback)*original=(Callback)(uintptr_t)FIELD(node,8);
    FIELD(node,8)=(uint32_t)(uintptr_t)callback;
}
static void *__cdecl create_bomb14(void) {
    void *bomb=((void *(__cdecl *)(void))bomb_create_original)();
    if(!coop14_battle)return bomb;
    unsigned id=scope_depth?scope_owner:0;
    if(bomb && id<players){Seat *s=&seats[id];s->bomb=bomb;
        wrap_node(bomb,12,tick_bomb,&s->bomb_tick);wrap_node(bomb,16,draw_bomb,&s->bomb_draw);
        if(test)log_line("P%u native Bomb created %p",id+1,bomb);
    }
    return bomb;
}
static void TC destroy_bomb14(void *bomb) {
    for(unsigned id=0;id<players;++id)if(seats[id].bomb==bomb){seats[id].bomb=NULL;if(test)log_line("P%u native Bomb retired %p",id+1,bomb);}
    ((void (TC *)(void *))bomb_destroy_original)(bomb);
}
static void create_guest(unsigned id) {
    Seat *s=&seats[id];if(s->player || !seats[0].player)return;
    void *manager=PTR(ANM);if(!manager)return;unsigned file=23+id;
    if(!s->anm) {
        if(FIELD(manager,0xbc7b0c+file*4)){log_line("Reserved animation slot %u is occupied; postponing P%u",file,id+1);return;}
        unsigned character=s->character*2+s->style;
        const char *name=*(const char **)(uintptr_t)(0x4d9f38+character*4);
        s->anm=((void *(TC *)(void *,int,const char *))0x47cff0)(manager,file,name);
        if(s->anm){FIELD(s->anm,0x128)=1;log_line("P%u scheduled ANM %s",id+1,name);}return;
    }
    if(FIELD(s->anm,0x128))return;
    void *host_anm=(void *)(uintptr_t)FIELD(manager,0xbc7b30),*host_cache=PTR(0x4db680);
    if(!s->has_resources){memcpy(s->resources,(void *)RES,sizeof(s->resources));s->has_resources=1;}
    enter(id,0);FIELD(manager,0xbc7b30)=(uint32_t)(uintptr_t)s->anm;PTR(0x4db680)=NULL;
    creating=id;s->player=((void *(__cdecl *)(void))create_original)();creating=0;
    if(s->player) {
        s->bomb=((void *(__cdecl *)(void))0x411bc0)();
        wrap_node(s->player,8,tick_player,&s->tick);wrap_node(s->player,12,draw_player,&s->draw);
        FIELD((void *)(uintptr_t)FIELD(s->player,8),4)|=2;
        FIELD((void *)(uintptr_t)FIELD(s->player,12),4)|=2;
        if(s->bomb){wrap_node(s->bomb,12,tick_bomb,&s->bomb_tick);wrap_node(s->bomb,16,draw_bomb,&s->bomb_draw);}
        set_position(s->player,id==1?48.f:-48.f,400.f);s->stage=U32(STAGE);
        log_line("P%u active character=%u style=%u player=%p bomb=%p",id+1,s->character,s->style,s->player,s->bomb);
    }else log_line("P%u constructor failed",id+1);
    FIELD(manager,0xbc7b30)=(uint32_t)(uintptr_t)host_anm;PTR(0x4db680)=host_cache;leave(id,0);
}
static int TC tick_player(void *p) {
    unsigned id=owner_of(p);Seat *s=&seats[id];
    if(!id && !ending) {
        if(!seats[0].bomb && PTR(BOMB)){seats[0].bomb=PTR(BOMB);wrap_node(seats[0].bomb,12,tick_bomb,&seats[0].bomb_tick);wrap_node(seats[0].bomb,16,draw_bomb,&seats[0].bomb_draw);}
        for(unsigned i=1;i<players;++i)create_guest(i);
    }
    detect_ghost(id);
    if(s->ghost){ghost_tick(id);s->previous=sampled[id];return 1;}
    enter(id,1);uint32_t before=s->bomb?FIELD(s->bomb,0x40):0;
    unsigned old_state=FIELD(p,0x684);int result=s->tick(p);
    if(old_state==2 && FIELD(p,0x684)==0 && U32(0x4f5870)==3)U32(0x4f5870)=2;
    if(s->bomb && !before && FIELD(s->bomb,0x40))for(unsigned i=0;i<players;++i)if(i!=id && living(i) && (int32_t)FIELD(seats[i].player,0x182c4)<60)timer(seats[i].player,0x182c0,60);
    leave(id,1);s->previous=sampled[id];detect_ghost(id);
    return result;
}
static void *__cdecl create_host(void) {
    // The title's attract mode sets 0x40 before loading its native replay.
    // Only a completed co-op character draft authorizes multiplayer play.
    if(!draft_ready || (U32(GAME_FLAGS)&0x40))
        return ((void *(__cdecl *)(void))create_original)();
    unsigned next_stage=U32(STAGE);int next_carry=seats[0].stage>0 && next_stage==seats[0].stage+1 && next_stage<=6;
    if(next_carry && seats[0].ghost){U32(0x4f5864)=0;U32(0x4f5870)=2;U32(0x4f5874)=0;U32(0x4f5858)=U32(0x4f585c)/2;}
    void *p=((void *(__cdecl *)(void))create_original)();if(!p || creating)return p;
    uint32_t stage=U32(STAGE);int carry=seats[0].stage>0 && stage==seats[0].stage+1 && stage<=6;
    if(!carry && U32(0x4f5870)==3)U32(0x4f5870)=2;
    coop14_battle=1;
    hud_started14=0;seats[0].player=p;seats[0].anm=(void *)(uintptr_t)FIELD(p,0x10);seats[0].character=U32(CHARA);seats[0].style=U32(STYLE);seats[0].stage=stage;
    rules14_reset();
    seats[0].ghost=0;ending=0;wrap_node(p,8,tick_player,&seats[0].tick);wrap_node(p,12,draw_player,&seats[0].draw);
    for(unsigned i=1;i<players;++i) {
        if(!carry){seats[i].has_resources=0;seats[i].character=draft_ready?draft_char[i]:U32(CHARA);seats[i].style=draft_ready?draft_style[i]:U32(STYLE);seats[i].ghost=0;}
        if(carry && seats[i].ghost){seats[i].ghost=0;*resource(i,0x4f5864)=0;*resource(i,0x4f5870)=2;*resource(i,0x4f5874)=0;*resource(i,0x4f5858)=*resource(i,0x4f585c)/2;}
    }
    log_line("Host created stage=%u char=%u style=%u",stage,U32(CHARA),U32(STYLE));return p;
}
static void TC destroy_host(void *p) {
    if(p==seats[0].player && !ending) {
        ending=1;
        for(unsigned id=1;id<players;++id) {
            Seat *s=&seats[id];if(!s->player)continue;
            enter(id,0);void *manager=PTR(ANM),*host_anm=manager?(void *)(uintptr_t)FIELD(manager,0xbc7b30):NULL,*cache=PTR(0x4db680);
            uint32_t flags=U32(0x4f58b8);U32(0x4f58b8)=flags&~1u;
            if(manager)FIELD(manager,0xbc7b30)=(uint32_t)(uintptr_t)s->anm;
            clear_focus(s->player);
            if(s->bomb){void *bomb=s->bomb;((void (TC *)(void *))0x411980)(bomb);((void (__cdecl *)(void *))0x4854fe)(bomb);}
            ((void (TC *)(void *))destroy_original)(s->player);((void (__cdecl *)(void *))0x4854fe)(s->player);
            if(manager){FIELD(manager,0xbc7b30)=(uint32_t)(uintptr_t)host_anm;FIELD(manager,0xbc7b0c+(23+id)*4)=0;}
            PTR(0x4db680)=cache;U32(0x4f58b8)=flags;leave(id,0);s->player=s->bomb=s->anm=NULL;
        }
        coop14_battle=0;seats[0].player=NULL;seats[0].bomb=NULL;log_line("Host teardown; all guest callbacks removed");
    }
    ((void (TC *)(void *))destroy_original)(p);
}
static int merge(int a,int b){return a==1 || b==1?1:a==2 || b==2?2:0;}
#include "items14.h"
static ItemSeat14 item_seats[3];
static int item_bomb_trigger,item_line_trigger;

static void finish_item(void) {
    if(!item14_pending)return;
    unsigned owner=item_owner,kind=item_kind;void *item=current_item;unsigned gift=item_index_current<0x1258 && gift_owners[item_index_current]>=0;
    if(kind && gift && gift_power[item_index_current]>0){int max=U32(0x4f585c),value=item_power_before+gift_power[item_index_current];U32(0x4f5858)=value<max?value:max;((void (TC *)(void *))0x44f970)(seats[owner].player);}
    if(FIELD(item,0xbf0)==0) {
        item_owners[item_index_current]=gift_owners[item_index_current]=-1;
        gift_power[item_index_current]=0;
        if(kind && fixture_item14[item_index_current]==1)fixture_item14[item_index_current]=2;
    }
    if(owner)U32(GAME_INPUT)=item_saved_input;
    leave(owner,0);item14_pending=0;item_kind=0;current_item=NULL;
    if(kind==1 || kind==3 || kind==8)
        item_seats[owner].needs_power=*resource(owner,0x4f5858)<*resource(owner,0x4f585c);
    if(!gift && kind>=4 && kind<=8)for(unsigned id=0;id<players;++id)if(id!=owner && living(id)) {
        enter(id,0);
        if(kind==8)((void (TC *)(void *))0x439220)(item);
        else {
            const uintptr_t rewards[]={0x447bb0,0x447b70,0x447d00,0x447cc0};
            ((int (__stdcall *)(int))rewards[kind-4])(0);
        }
        leave(id,0);
        if(kind==8)item_seats[id].needs_power=*resource(id,0x4f5858)<*resource(id,0x4f585c);
    }
}
void item14_begin(void *item) {
    finish_item();
    unsigned state=FIELD(item,0xbf0);
    if(!state || state==5)return;
    uintptr_t base=(uintptr_t)PTR(0x4db660)+0x14;unsigned index=((uintptr_t)item-base)/0xc18;
    if(index>=0x1258)return;
    int previous=item_owners[index];
    unsigned kind=FIELD(item,0xbf4);int power=kind==1 || kind==3;
    int gift=gift_owners[index];
    if(gift>=0 && (unsigned)gift<players && seats[gift].ghost){gift_owners[index]=-1;gift_power[index]=0;gift=ITEM14_NONE;}
    // Delayed drops retain their native spawn animation. Bonus-only kinds 9–11
    // keep their existing native movement and scoring behavior.
    int trigger=ITEM14_NONE;
    if(kind<=8 && (state==ITEM14_NEARBY || (state==ITEM14_FALLING && (int)FIELD(item,0xc00)<=0) ||
       (state==ITEM14_RISING && item_bomb_trigger!=ITEM14_NONE)))
        trigger=item_bomb_trigger!=ITEM14_NONE?item_bomb_trigger:item_line_trigger;
    ItemRoute14 route=item14_route(FLT(item,0xbac),FLT(item,0xbb0),players,state,previous,gift,trigger,power,item_seats);
    if(route.owner==ITEM14_NONE)return;
    unsigned owner=(unsigned)route.owner;
    item_owners[index]=(int8_t)owner;
    if(route.attract){
        void *movement=(void *)(uintptr_t)FIELD(seats[owner].player,0x688);
        if(movement){
            FIELD(item,0xbf0)=ITEM14_HOMING;
            FIELD(item,0xbfc)=FIELD(movement,8);
            U32(0x4f5884)=8;
        }
    }
    item_owner=owner;current_item=item;item14_pending=1;item_index_current=index;
    enter(owner,0);
    if(owner){item_saved_input=U32(GAME_INPUT);U32(GAME_INPUT)=sampled[owner];}
    if(gift>=0)item_power_before=U32(0x4f5858);
}
int item14_picked(void *item) {
    if(!item14_pending || item!=current_item)return 0;
    unsigned kind=FIELD(item,0xbf4);
    if((kind==1 || kind==3) && gift_owners[item_index_current]<0 &&
       U32(0x4f5858)>=U32(0x4f585c)) {
        int target=item14_nearest(FLT(item,0xbac),FLT(item,0xbb0),players,1,item_seats);
        if(target!=ITEM14_NONE && (unsigned)target!=item_owner) {
            void *movement=(void *)(uintptr_t)FIELD(seats[target].player,0x688);
            if(movement){
                FIELD(item,0xbf0)=ITEM14_HOMING;
                FIELD(item,0xbfc)=FIELD(movement,8);
                item_owners[item_index_current]=(int8_t)target;
                return 1;
            }
        }
    }
    item_kind=kind;
    if(test && item_kind<=8)log_line("P%u picked native item kind=%u shared=%u",item_owner+1,item_kind,item_kind>=4 && item_kind<=8);
    return 0;
}
extern void item14_loop_bridge(void),item14_pickup_bridge(void);
static int TC tick_items(void *manager){
    if(!coop14_battle)return ((Callback)item_original)(manager);
    void *host=seats[0].player;int suppress=host && seats[0].ghost;
    item_bomb_trigger=item_line_trigger=ITEM14_NONE;
    for(unsigned id=0;id<players;++id){
        void *player=seats[id].player,*bomb=seats[id].bomb;
        item_seats[id]=(ItemSeat14){0};
        if(!living(id) || !bomb)continue;
        suppress=0;
        item_seats[id]=(ItemSeat14){1,*resource(id,0x4f5858)<*resource(id,0x4f585c),
            FLT(player,0x5e0),FLT(player,0x5e4),FLT(player,0x1835c),FLT(player,0x18360),
            FLT(player,0x18368),FLT(player,0x1836c)};
        if(item_bomb_trigger==ITEM14_NONE && FIELD(bomb,0x40)==1 && FIELD(bomb,0x1c)<60)item_bomb_trigger=(int)id;
        if(item_line_trigger==ITEM14_NONE && item_seats[id].y<(seats[id].character==1?148.f:128.f))item_line_trigger=(int)id;
    }
    unsigned state=suppress?FIELD(host,0x684):0;
    // With nobody eligible, the native loop still advances each item once.
    // Native state 2 skips its pickup branch; a ghost cannot collect an item
    // through the original singleton fallback and revive a wiped team.
    if(suppress)FIELD(host,0x684)=2;
    int result=((Callback)item_original)(manager);finish_item();if(suppress)FIELD(host,0x684)=state;return result;
}
extern int native_damage14(const uint32_t *,float),native_circle14(const uint32_t *,float),native_laser14(const uint32_t *,float,float),native_laser214(const uint32_t *,float,float);
extern void damage14_bridge(void),circle14_bridge(void),laser14_bridge(void),laser214_bridge(void);
int coop_damage14(const uint32_t *args,float angle) {
    if(!coop14_battle || scope_depth)return native_damage14(args,angle);
    int total=0;unsigned active=0;uint32_t hit=0;
    for(unsigned i=0;i<players;++i)if(seats[i].player && !seats[i].ghost){++active;enter(i,0);total+=native_damage14(args,angle);if(args[3]){hit|=*(uint32_t *)(uintptr_t)args[3];*(uint32_t *)(uintptr_t)args[3]=hit;}leave(i,0);}
    void *manager=PTR(0x4db544);if(manager && active && args[6])for(unsigned i=0;i<4;++i)if(FIELD(manager,0x5c+i*4)==args[6]) {
        int scaled=active==3?total*2/3:active==2?total*3/4:total;
        if(test && total>0){test->damage_events[active-1]++;test->damage_raw[active-1]=total;test->damage_scaled[active-1]=scaled;}
        return scaled;
    }
    return total;
}
int coop_circle14(const uint32_t *args,float radius) {
    if(!coop14_battle || scope_depth)return native_circle14(args,radius);
    const float *position=(const float *)(uintptr_t)args[0];
    // Conservative enclosing square for both native hit and graze circles.
    // Keep the original collision routine for all potentially relevant shots.
    float outer=fabsf(radius)+fmaxf(radius/2.5f,40.f)+1.f;
    int result=0;
    for(unsigned i=0;i<players;++i)if(living(i)){
        void *player=seats[i].player,*parameters=(void *)(uintptr_t)FIELD(player,0x688);
        if(parameters){
            float body=FLT(parameters,4);
            if(FIELD(player,0x182d4)&0x10)body=(FLT(player,0x18308)*3.6f)*body;
            float bound=outer+fabsf(body);
            if(fabsf(FLT(player,0x5e0)-position[0])>bound || fabsf(FLT(player,0x5e4)-position[1])>bound)continue;
        }
        enter(i,0);result=merge(result,native_circle14(args,radius));leave(i,0);
    }
    return result;
}
int coop_laser14(const uint32_t *args,float angle,float width) {
    if(!coop14_battle || scope_depth)return native_laser14(args,angle,width);
    int result=0;for(unsigned i=0;i<players;++i)if(living(i)){enter(i,0);result=merge(result,native_laser14(args,angle,width));leave(i,0);}return result;
}
int coop_laser214(const uint32_t *args,float angle,float width) {
    if(!coop14_battle || scope_depth)return native_laser214(args,angle,width);
    int result=0;for(unsigned i=0;i<players;++i)if(living(i)){enter(i,0);result=merge(result,native_laser214(args,angle,width));leave(i,0);}return result;
}
static int __stdcall rect14(uint32_t pos,uint32_t size,uint32_t graze) {
    if(!coop14_battle || scope_depth)return ((int (__stdcall *)(uint32_t,uint32_t,uint32_t))rect_original)(pos,size,graze);
    int result=0;for(unsigned i=0;i<players;++i)if(living(i)){enter(i,0);result=merge(result,((int (__stdcall *)(uint32_t,uint32_t,uint32_t))rect_original)(pos,size,graze));leave(i,0);}return result;
}
#include "rules14.h"
#include "online14.h"
#include "aim14.h"
#include "visual14.h"
#include "fixture14.h"
static int TC input14(void *input) {
    if(U32(GAME_FLAGS)&0x40)return ((Callback)input_original)(input);
    if(online_map) {
        if(online_sampling)return ((Callback)input_original)(input);
        if(input==(void *)0x4d6878 || input==(void *)GAME_INPUT)return 1;
    }
    static uint32_t test_previous;
    if(input==(void *)0x4d6878) {
        int focused=GetForegroundWindow()==(HWND)(uintptr_t)U32(0x4f5a18);
        sampled[0]=FIELD(input,0);sampled[1]=focused?kb_sample(1,GetAsyncKeyState):0;
        if(!kb_native[0])FIELD(input,0)=(FIELD(input,0)&~0xcfbu)|(focused?kb_sample(0,GetAsyncKeyState):0);
        sampled[0]=FIELD(input,0);
        if(test && test->magic==0x54483134){FIELD(input,0)=test->keys[0];FIELD(input,4)=test_previous;test_previous=test->keys[0];}
        if(test && test->magic==0x54483134)for(unsigned i=0;i<players;++i)sampled[i]=test->keys[i];
        void *menu=PTR(0x4db6a4);
        if(menu && (FIELD(menu,0x1c)==6 || FIELD(menu,0x1c)==7) && draft_owner) {
            uint32_t held=sampled[draft_owner];
            if(draft_release){if(!held)draft_release=0;held=0;}
            FIELD(input,0)=held;FIELD(input,4)=draft_previous;draft_previous=held;
        }
        return ((Callback)input_original)(input);
    }
    if(input!=(void *)GAME_INPUT)return ((Callback)input_original)(input);
    int focused=GetForegroundWindow()==(HWND)(uintptr_t)U32(0x4f5a18);
    if(!kb_native[0])U32(GAME_INPUT)=(U32(GAME_INPUT)&~0xcfbu)|(focused?kb_sample(0,GetAsyncKeyState):0);
    sampled[0]=U32(GAME_INPUT);sampled[1]=focused?kb_sample(1,GetAsyncKeyState):0;sampled[2]=0;
    if(test && test->magic==0x54483134){for(unsigned i=0;i<players;++i)sampled[i]=test->keys[i];U32(GAME_INPUT)=sampled[0];}
    return ((Callback)input_original)(input);
}
int menu14_allow_fade(void) {
    // The native weapon-confirm handler fades music AFTER menu_sub returns.
    // Only the final seat is actually leaving the menu to load a stage.
    if(draft_pending==1){log_line("Draft handoff: preserving selection BGM");return 0;}
    log_line("Final draft confirmation: native BGM fade");return 1;
}
static void TC menu_sub(void *menu,int next) {
    if(menu==PTR(0x4db6a4) && FIELD(menu,0x1c)==7) {
        if(next==3) {
            draft_char[draft_owner]=U32(CHARA);draft_style[draft_owner]=FIELD(menu,0x28);
            log_line("P%u selected native character=%u style=%u",draft_owner+1,draft_char[draft_owner],draft_style[draft_owner]);
            if(draft_owner+1<players){draft_pending=1;next=4;}
            else draft_ready=1;
        }
    }
    ((void (TC *)(void *,int))menu_sub_original)(menu,next);
}
static void TC menu_state(void *menu,int next) {
    int previous_seat=menu==PTR(0x4db6a4) && FIELD(menu,0x1c)==6 && next==5 && draft_owner;
    if(previous_seat){draft_pending=2;next=6;}
    if(menu==PTR(0x4db6a4) && FIELD(menu,0x1c)==7 && next==2 && draft_ready) {
        U32(CHARA)=draft_char[0];U32(STYLE)=draft_style[0];U32(0x4db6a8)=draft_style[0];
        log_line("Draft complete; restoring host selection before native loading");
    }
    if(menu==PTR(0x4db6a4) && next==1){draft_owner=draft_pending=draft_ready=draft_base_ready=0;}
    ((void (TC *)(void *,int))menu_state_original)(menu,next);
}
static int TC menu_tick(void *menu) {
    int result=((Callback)menu_tick_original)(menu);
    if(!draft_base_ready && !draft_owner && FIELD(menu,0x1c)==6 && FIELD(menu,0x24)==0){memcpy(draft_menu_base,OFF(menu,0x28),sizeof(draft_menu_base));draft_base_ready=1;}
    if(draft_pending && FIELD(menu,0x1c)==6) {
        draft_owner=draft_pending==1?draft_owner+1:draft_owner-1;draft_pending=0;draft_ready=0;
        draft_release=1;draft_previous=0;
        U32(CHARA)=draft_char[draft_owner];U32(STYLE)=draft_style[draft_owner];U32(0x4db6a8)=draft_style[draft_owner];
        U32(0x4db6ac)=draft_char[draft_owner];
        if(draft_base_ready)memcpy(OFF(menu,0x28),draft_menu_base,sizeof(draft_menu_base));
        ((void (TC *)(void *,int))menu_state_original)(menu,6);
        ((void (TC *)(void *,int))0x41e820)(OFF(menu,0x28),draft_char[draft_owner]);
        log_line("Native character screen now belongs to P%u",draft_owner+1);
    }
    return result;
}
static void observe(void) {
    if(test){test->frame=frame;test->stage=U32(STAGE);test->character=U32(CHARA);test->style=U32(STYLE);test->present=0;
        void *menu=PTR(0x4db6a4);if(menu){test->menu_state=FIELD(menu,0x1c)|(draft_owner<<16);test->menu_sub=FIELD(menu,0x24);}
        for(unsigned i=0;i<players;++i)if(seats[i].player){test->present|=1u<<i;test->states[i]=FIELD(seats[i].player,0x684);test->lives[i]=*resource(i,0x4f5864);test->bombs[i]=*resource(i,0x4f5870);test->power[i]=*resource(i,0x4f5858);test->pos[i][0]=FLT(seats[i].player,0x5e0);test->pos[i][1]=FLT(seats[i].player,0x5e4);}
        test->ghosts=0;
        for(unsigned i=0;i<players;++i)if(seats[i].player){test->ghosts|=(seats[i].ghost!=0)<<i;test->life_frag[i]=*resource(i,0x4f5868);test->bomb_frag[i]=*resource(i,0x4f5874);test->iframes[i]=FIELD(seats[i].player,0x182c4);test->characters[i]=seats[i].character;test->styles[i]=seats[i].style;}
        for(unsigned i=0;i<players;++i){test->focused_markers[i]=seats[i].player?FIELD(seats[i].player,0xd6d8):0;test->active_bombs[i]=seats[i].bomb?FIELD(seats[i].bomb,0x40):0;}
        for(unsigned i=0;i<players;++i)test->player_scale[i]=seats[i].player?FLT(seats[i].player,0x18308):0;
        void *enemies=PTR(0x4db544);for(unsigned i=0;i<4;++i){void *boss=enemies?(void *)(uintptr_t)FIELD(enemies,0x5c+i*4):NULL;test->boss_hp[i]=boss?FIELD(boss,0x5164):0;}
        void *pause=PTR(0x4db678);test->pause_state=pause?FIELD(pause,4):0;test->pause_choice=pause?FIELD(pause,0x38):0;test->disabled_continue=0;
        if(pause && FIELD(pause,0x10c)<=40)for(unsigned i=0;i<FIELD(pause,0x10c);++i)if(!FIELD(pause,0xc8+i*4))test->disabled_continue=1;
        if(test->quit==1){test->quit=0;PostMessageW((HWND)(uintptr_t)U32(0x4f5a18),WM_CLOSE,0,0);}
    }
}
static int TC root14(void *registry) {
    if(online_map){int result=online14_root(registry);observe();return result;}
    int result=0;
    unsigned count=test && test->magic==0x54483134?test_ticks14:1;
    for(unsigned i=0;i<count;++i){result=tick_native14(registry);++frame;observe();if(!result)break;}
    return result;
}
static int hud_battle_visible14(void) {
    if(PTR(0x4db6a4) || !PTR(0x4db550) || U32(0x4d9648)!=U32(0x4d964c))return 0;
    HANDLE worker=(HANDLE)PTR(0x4d98f0);if(worker && WaitForSingleObject(worker,0)!=WAIT_OBJECT_0)return 0;
    for(unsigned i=0;i<players;++i)if(!seats[i].player || (!hud_started14 && FIELD(seats[i].player,0x684)!=1))return 0;
    hud_started14=1;return 1;
}
#include "hud14.h"
#include "progress14.h"
static void *ending_init_original;
static int TC ending_init14(void *ending_object) {
    int result=((Callback)ending_init_original)(ending_object);
    void *score=PTR(0x4db68c);
    if(result==0 && score && !scope_depth && draft_ready && !(U32(GAME_FLAGS)&0x40) && U32(STAGE)==6 && U32(0x4f5838)==0) {
        unsigned characters[3],styles[3],stages[3];
        for(unsigned id=0;id<players;++id){characters[id]=seats[id].character;styles[id]=seats[id].style;stages[id]=seats[id].stage;}
        progress14_credit(OFF(score,0x24246),characters,styles,stages,players,U32(0x4f5834),FIELD(ending_object,0x1c));
        log_line("Native ending %u; completion flags credited to %u participating loadouts",FIELD(ending_object,0x1c),players);
    }
    return result;
}
static void *trampoline(uintptr_t addr,unsigned n) {
    unsigned char *copy=VirtualAlloc(NULL,n+5,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE);if(!copy)return NULL;
    memcpy(copy,(void *)addr,n);copy[n]=0xe9;*(int32_t *)(copy+n+1)=addr+n-(uintptr_t)(copy+n+5);return copy;
}
static void patch(uintptr_t addr,void *target,unsigned n) {
    DWORD old;VirtualProtect((void *)addr,n,PAGE_EXECUTE_READWRITE,&old);
    memset((void *)addr,0x90,n);*(uint8_t *)addr=0xe9;*(int32_t *)(addr+1)=(uintptr_t)target-addr-5;
    FlushInstructionCache(GetCurrentProcess(),(void *)addr,n);VirtualProtect((void *)addr,n,old,&old);
}
static DWORD WINAPI monitor(void *unused) {
    (void)unused;wchar_t path[MAX_PATH];GetModuleFileNameW(module,path,MAX_PATH);wchar_t *slash=wcsrchr(path,L'\\');wcscpy(slash+1,L"th14-coop.log");wcscpy(log_path,path);
    wcscpy(slash+1,L"coop.ini");kb_load(path);
    wchar_t ready_name[100];swprintf(ready_name,100,L"Local\\TH14CoopReady-%lu",GetCurrentProcessId());ready_event=CreateEventW(NULL,TRUE,FALSE,ready_name);if(!ready_event)return 1;
    wchar_t test_flag[8];if(GetEnvironmentVariableW(L"TH14_COOP_TEST",test_flag,8) && !wcscmp(test_flag,L"1")) {
        AddVectoredExceptionHandler(1,exception_trace);
        wchar_t name[90];swprintf(name,90,L"Local\\TH14CoopTest-%lu",GetCurrentProcessId());
        test_mapping=CreateFileMappingW(INVALID_HANDLE_VALUE,NULL,PAGE_READWRITE,0,sizeof(TestControl),name);
        if(test_mapping)test=MapViewOfFile(test_mapping,FILE_MAP_ALL_ACCESS,0,0,sizeof(TestControl));
        wchar_t ticks[16];
        if(test && GetEnvironmentVariableW(L"TH14_COOP_TEST_TICKS",ticks,16)){
            unsigned value=(unsigned)_wtoi(ticks);if(value>=1 && value<=16)test_ticks14=value;
        }
    }
    log_line("TH14 adapter bootstrap 0.2.8-transfer");
    memset(item_owners,-1,sizeof(item_owners));memset(gift_owners,-1,sizeof(gift_owners));
    if(!online14_open()){log_line("REJECT online bridge identity");return 1;}
    struct Hook {uintptr_t address;unsigned n;const char *expected;void *target;void **original;};
    struct Hook hooks[]={
        {0x44d250,5,"\x55\x8b\xec\x6a\xff",create_host,&create_original},
        {0x44d0a0,5,"\x55\x8b\xec\x6a\xff",destroy_host,&destroy_original},
        {0x44f7a0,6,"\x55\x8b\xec\x83\xec\x08",begin_dying14,&dying_original},
        {0x44cc10,6,"\x55\x8b\xec\x83\xec\x0c",reset14,&reset_original},
        {0x436f90,9,"\x55\x8b\xec\x51\xa1\xb8\x58\x4f\x00",stage_clear14,&stage_clear_original},
        {0x407540,8,"\x56\x8b\xf1\xb9\x01\x00\x00\x00",input14,&input_original},
        {0x401280,11,"\x55\x8b\xec\x51\x80\x3d\x15\x58\x4f\x00\x00",root14,&root_original},
        {0x4013b0,11,"\x55\x8b\xec\x51\x80\x3d\x15\x58\x4f\x00\x00",draw_root14,&draw_root_original},
        {0x451430,6,"\x55\x8b\xec\x83\xec\x0c",damage14_bridge,&damage_original},
        {0x44efa0,9,"\x55\x8b\xec\x8b\x0d\x7c\xb6\x4d\x00",circle14_bridge,&circle_original},
        {0x44ee80,7,"\x55\x8b\xec\x51\x8b\x4d\x0c",rect14,&rect_original},
        {0x44f0b0,6,"\x55\x8b\xec\x83\xe4\xf8",laser14_bridge,&laser_original},
        {0x44f3f0,6,"\x55\x8b\xec\x83\xe4\xf8",laser214_bridge,&laser2_original},
        {0x459940,8,"\x56\x57\x8b\xf9\x83\x7f\x1c\x01",menu_tick,&menu_tick_original},
        {0x458c80,6,"\x55\x8b\xec\x8b\x41\x1c",menu_state,&menu_state_original},
        {0x458d00,6,"\x55\x8b\xec\x8b\x45\x08",menu_sub,&menu_sub_original},
        {0x438550,6,"\x55\x8b\xec\x83\xe4\xf8",tick_items,&item_original},
        {0x4397b0,10,"\x55\x8b\xec\x51\xff\x81\x78\xd8\xdd\x00",spawn14,&spawn_original},
        {0x478f60,9,"\x55\x8b\xec\x83\xe4\xf8\x83\xec\x18",anm_draw14,&anm_draw_original},
        {0x448300,6,"\x56\x8b\xf1\x8b\x46\x04",pause14,&pause_original},
        {0x4485d0,6,"\x55\x8b\xec\x83\xec\x18",pause_draw14,&pause_draw_original},
        {0x41fac0,5,"\x55\x8b\xec\x6a\xff",ending_init14,&ending_init_original},
        {0x4504c0,6,"\x55\x8b\xec\x83\xec\x08",scale14_bridge,&scale_original},
        {0x411bc0,6,"\x8b\x0d\x28\x58\x4f\x00",create_bomb14,&bomb_create_original},
        {0x411980,6,"\x55\x8b\xec\x83\xec\x08",destroy_bomb14,&bomb_destroy_original},
    };
    if(memcmp((void *)0x4385b0,"\x8b\x87\xf0\x0b\x00\x00",6) || memcmp((void *)0x438b3e,"\x8b\x87\xf4\x0b\x00\x00",6)){log_line("REJECT item loop mismatch");return 1;}
    for(unsigned i=0;i<sizeof(aim14_sites)/sizeof(aim14_sites[0]);++i)if(memcmp((void *)aim14_sites[i].address,aim14_sites[i].length==5?"\xa1\x7c\xb6\x4d\x00":"\x8b\x0d\x7c\xb6\x4d\x00",aim14_sites[i].length)){log_line("REJECT aim read mismatch at %x",(unsigned)aim14_sites[i].address);return 1;}
    for(unsigned i=0;i<sizeof(bomb14_sites)/sizeof(bomb14_sites[0]);++i)if(memcmp((void *)bomb14_sites[i].address,"\xa1\x2c\xb5\x4d\x00",5)){log_line("REJECT Bomb read mismatch");return 1;}
    for(unsigned i=0;i<sizeof(hooks)/sizeof(hooks[0]);++i)if(memcmp((void *)hooks[i].address,hooks[i].expected,hooks[i].n)) {log_line("REJECT instruction mismatch at %x",(unsigned)hooks[i].address);return 1;}
    if(memcmp((void *)0x45e613,"\xe8\x28\x72\xfe\xff",5)){log_line("REJECT menu BGM call mismatch");return 1;}
    for(unsigned i=0;i<sizeof(hooks)/sizeof(hooks[0]);++i){*hooks[i].original=trampoline(hooks[i].address,hooks[i].n);if(!*hooks[i].original)return 1;}
    // The title is idle when the loader attaches. All sites are checked and
    // all trampolines allocated before the first production instruction edit.
    for(unsigned i=0;i<sizeof(hooks)/sizeof(hooks[0]);++i)patch(hooks[i].address,hooks[i].target,hooks[i].n);
    patch(0x4385b0,item14_loop_bridge,6);patch(0x438b3e,item14_pickup_bridge,6);
    for(unsigned i=0;i<sizeof(aim14_sites)/sizeof(aim14_sites[0]);++i)patch(aim14_sites[i].address,aim14_sites[i].bridge,aim14_sites[i].length);
    for(unsigned i=0;i<sizeof(bomb14_sites)/sizeof(bomb14_sites[0]);++i)patch(bomb14_sites[i].address,bomb14_sites[i].bridge,5);
    // Keep this a CALL: the bridge either returns without a fade or tail-calls
    // the native XMM1 fade routine with its original return address intact.
    DWORD protection;VirtualProtect((void *)0x45e613,5,PAGE_EXECUTE_READWRITE,&protection);
    *(int32_t *)0x45e614=(uintptr_t)menu14_fade_bridge-0x45e618;
    FlushInstructionCache(GetCurrentProcess(),(void *)0x45e613,5);VirtualProtect((void *)0x45e613,5,protection,&protection);
    SetEvent(ready_event);log_line("Native callbacks installed; waiting for original character selection");return 0;
}
BOOL WINAPI DllMain(HINSTANCE instance,DWORD reason,void *reserved) {
    (void)reserved;if(reason==DLL_PROCESS_ATTACH){module=instance;DisableThreadLibraryCalls(instance);HANDLE t=CreateThread(NULL,0,monitor,NULL,0,NULL);if(t)CloseHandle(t);}return TRUE;
}
