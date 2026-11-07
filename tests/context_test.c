/* Tests the real context and hook wrappers in a private process. No game,
   desktop window, synthetic keyboard input, or external process is used. */
#define COOP_NATIVE_FIXTURE 1
#include "../src/coop.c"
#include <assert.h>

static unsigned checks,flushes,draws,removals,deletions;
static void *expected_manager;
static uint32_t fake_card_vtable[21];
static int pressed_vk,reset_calls,gameovers,spawn_count,spawn_fail,spawn_fail_after=-1;
static uint32_t spawned_type;
static int expected_revived=1,expected_add_mode=3;
static uint32_t reset_mask;
static LONG WINAPI report_crash(EXCEPTION_POINTERS *error) {
    fprintf(stderr,"TEST EXCEPTION %lx ip=%lx read=%lx ecx=%lx\n",
        (unsigned long)error->ExceptionRecord->ExceptionCode,(unsigned long)error->ContextRecord->Eip,
        (unsigned long)error->ExceptionRecord->ExceptionInformation[1],(unsigned long)error->ContextRecord->Ecx);
    return EXCEPTION_EXECUTE_HANDLER;
}
int TC coop_hud(void *unused) {(void)unused;return 1;}
static int menu_banner;
void coop_menu_hud(int owner) {menu_banner=owner;}

#define CHECK(expr) do { if(!(expr)) {fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#expr);exit(1);} ++checks; } while(0)
static void stub(uintptr_t address,void *function) {
    unsigned char *code=(unsigned char *)address;
    code[0]=0xe9;*(int32_t *)(code+1)=(int32_t)((uintptr_t)function-address-5);
    FlushInstructionCache(GetCurrentProcess(),code,5);
}
static void *TC fake_delete_card(void *card,int release) {
    CHECK(release==1);CHECK(PTR(0x4cf298)==guest_cards);++deletions;free(card);return NULL;
}
static int TC fake_add_card(void *manager,int id,int mode) {
    CHECK(manager==guest_cards);CHECK(mode==expected_add_mode);
    if(mode==1) CHECK(guest_bomb && PTR(0x4cf2b8)==guest_bomb);
    unsigned char *card=calloc(1,0x60);CHECK(card!=NULL);
    FIELD(card,0)=(uint32_t)(uintptr_t)fake_card_vtable;FIELD(card,4)=(uint32_t)id;
    FIELD(card,0xc)=(uint32_t)(uintptr_t)card;
    FIELD(card,0x10)=FIELD(manager,0x1c);FIELD(manager,0x1c)=(uint32_t)(uintptr_t)OFF(card,0xc);
    ++FIELD(manager,0x28);
    /* Initial-copy code must neutralize acquisition rewards already present
       in the copied initial resource snapshot. */
    U32(0x4ccd34)+=100;U32(0x4ccd48)+=1;U32(0x4ccd38)+=100;
    return (int)FIELD(manager,0x28);
}
static void TC fake_remove(void *scheduler,void *node) {
    CHECK(scheduler==expected_manager);++removals;free(node);
}
static void *fake_player_create(void) {
    U32(0x4ccd34)=999;PTR(0x570920)=NULL;PTR(0x4cf2d4)=NULL;return NULL;
}
static void TC fake_flush(void *manager) {
    CHECK(FIELD(manager,0x3120e04)==42);CHECK(FIELD(manager,0x3120e14)==84);
    CHECK(*OFF(manager,0x3120e0a)==7);++flushes;
}
static int TC fake_stamp_draw(void *manager,void *vm) {
    (void)vm;++draws;
    if(flushes) {
        CHECK(FIELD(manager,0x3120e04)==UINT32_MAX);CHECK(FIELD(manager,0x3120e14)==0);
        CHECK(*OFF(manager,0x3120e0a)==0xff);
    } else {
        CHECK(FIELD(manager,0x3120e04)==42);CHECK(FIELD(manager,0x3120e14)==84);
    }
    return 17;
}
static int TC fake_shop_draw(void *shop) {
    (void)shop;CHECK(PTR(0x4cf410)==current_guest);CHECK(PTR(0x4cf298)==guest_cards);return 1;
}
static int TC fake_shop_tick(void *shop) {
    CHECK(PTR(0x4cf410)==current_guest);CHECK(PTR(0x4cf298)==guest_cards);
    CHECK(U32(0x4ccd34)==400);U32(0x4ccd34)-=75;++FIELD(guest_cards,0x28);
    CHECK(shop_draw(shop)==1);CHECK(PTR(0x4cf410)==current_guest);CHECK(scope_depth==1);
    return 9;
}
static void TC fake_shop_close(void *shop) {
    (void)shop;CHECK(PTR(0x4cf410)==current_guest);CHECK(PTR(0x4cf298)==guest_cards);
    U32(0x4ccd34)-=10;PTR(0x4cf2a4)=NULL;
}
static void reset_context(void) {
    if(!p1)p1=calloc(1,0x479d0);if(!current_guest)current_guest=calloc(1,0x479d0);
    if(!host_cards)host_cards=calloc(1,0xd70);if(!guest_cards)guest_cards=calloc(1,0xd70);
    CHECK(p1 && current_guest && host_cards && guest_cards);
    memset(host_cards,0,0xd70);memset(guest_cards,0,0xd70);
    memset(p2_resources,0,sizeof(p2_resources));memset((void *)0x4ccd38,0,0x30);
    memset(partner,0,sizeof(partner));memset(transfer_owner,-1,sizeof(transfer_owner));
    memset(spirit_was_drawn,0,sizeof(spirit_was_drawn));
    memset(transfer_power,0,sizeof(transfer_power));wipe_frames=team_over=0;
    FIELD(p1,0x476ac)=FIELD(current_guest,0x476ac)=1;
    U32(0x4ccd34)=300;p2_funds=400;U32(0x4ccd38)=100;*(uint32_t *)p2_resources=200;
    U32(0x4ccd48)=2;*(int32_t *)(p2_resources+0x10)=5;
    U32(0x4ccd3c)=*(int32_t *)(p2_resources+4)=400;
    U32(0x4ccd54)=*(int32_t *)(p2_resources+0x1c)=7;
    U32(0x4ccd64)=8;*(int32_t *)(p2_resources+0x2c)=6;
    U32(0x4ccd60)=*(int32_t *)(p2_resources+0x28)=2;
    PTR(0x4cf410)=p1;PTR(0x4cf298)=host_cards;
    PTR(0x4cf2d4)=(void *)0x1111;guest_mukade=(void *)0x2222;
    scope_depth=0;scope_has_input=0;shop_owner=0;shop_pending_owner=0;
    coop_pickup_confirmed=0;item_scope=0;scoped_item=NULL;
    guest_anm=guest_bomb=NULL;protected_bombs=0;PTR(0x51f65c)=NULL;PTR(0x4cf2b8)=NULL;
    custom_cards=have_run_base=0;chosen_character=-1;expected_add_mode=3;
    chosen_count=2;chosen_cards[0]=chosen_cards[1]=chosen_cards[2]=-1;
    draft_owner=draft_ready=draft_release=0;draft_menu=NULL;draft_input=0;native_draft=1;
    settings_path[0]=0;departed_stage=departed_ghosts=0;
}
static void test_scope_roundtrip(void) {
    reset_context();enter_p2(0);
    CHECK(PTR(0x4cf410)==current_guest);CHECK(PTR(0x4cf298)==guest_cards);
    CHECK(PTR(0x4cf2d4)==guest_mukade);CHECK(U32(0x4ccd34)==400);
    CHECK(U32(0x4ccd38)==200);CHECK(U32(0x4ccd48)==5);
    CHECK(U32(0x4ccd64)==6);U32(0x4ccd64)=9;
    U32(0x4ccd34)=325;U32(0x4ccd38)=250;PTR(0x4cf2d4)=(void *)0x3333;
    enter_p2(0);CHECK(U32(0x4ccd34)==325);U32(0x4ccd34)=320;
    leave_p2(0);CHECK(scope_depth==1);CHECK(U32(0x4ccd34)==320);
    leave_p2(0);CHECK(scope_depth==0);CHECK(U32(0x4ccd34)==300);
    CHECK(p2_funds==320);CHECK(U32(0x4ccd38)==100);CHECK(*(uint32_t *)p2_resources==250);
    CHECK(PTR(0x4cf410)==p1);CHECK(PTR(0x4cf298)==host_cards);
    CHECK(PTR(0x4cf2d4)==(void *)0x1111);CHECK(guest_mukade==(void *)0x3333);
    CHECK(U32(0x4ccd64)==8 && *(int32_t *)(p2_resources+0x2c)==9);
    puts("PASS independent money, resources, card and special-card context; nested scopes");
}
static void test_item_owner(void) {
    reset_context();item_manager=calloc(1,0x14+0xc94);CHECK(item_manager!=NULL);
    void *item=OFF(item_manager,0x14);FIELD(item,0xc74)=2;FIELD(item,0xc50)=1;FIELD(item,0xc78)=2;
    FLT(p1,0x624)=400;FLT(current_guest,0x624)=50;FLT(p1,0x47998)=FLT(current_guest,0x47998)=128;
    memset(item_owner,-1,sizeof(item_owner));memset(last_age,0,sizeof(last_age));
    collected[0]=collected[1]=0;
    uint32_t regs[8]={0};regs[0]=(uint32_t)(uintptr_t)item;
    coop_select_item(regs);CHECK(regs[1]==(uint32_t)(uintptr_t)current_guest);CHECK(item_scope==2);
    U32(0x4ccd34)+=1;FIELD(item,0xc74)=0;coop_pickup_confirmed=1;item_leave();
    CHECK(U32(0x4ccd34)==300);CHECK(p2_funds==401);CHECK(collected[0]==0 && collected[1]==1);
    CHECK(scope_depth==0);free(item_manager);item_manager=NULL;
    puts("PASS native item-loop ownership commits guest money only");
}
static void test_shop_scopes(void) {
    reset_context();shop_owner=1;void *shop=calloc(1,0xe3c);CHECK(shop!=NULL);
    shop_tick_original=fake_shop_tick;shop_draw_original=fake_shop_draw;
    uint32_t menu[8]={17,23,34,45,56,67,78,89};memcpy((void *)0x4ca210,menu,sizeof(menu));
    CHECK(shop_tick(shop)==9);CHECK(U32(0x4ccd34)==300 && p2_funds==325);
    CHECK(FIELD(host_cards,0x28)==0 && FIELD(guest_cards,0x28)==1);
    CHECK(!memcmp(menu,(void *)0x4ca210,sizeof(menu)));CHECK(scope_depth==0);
    shop_owner=1;shop_close_original=fake_shop_close;shop_close(shop);
    CHECK(U32(0x4ccd34)==300 && p2_funds==315);CHECK(PTR(0x4cf298)==host_cards);
    CHECK(scope_depth==0 && shop_owner==0);free(shop);
    puts("PASS guest purchase, nested draw, menu input restoration and external shop close");
}
static unsigned null_closes;
static void TC fake_shutdown_close(void *shop) {CHECK(shop==NULL);++null_closes;}
static void test_exit_null_close(void) {
    reset_context();shop_close_original=fake_shutdown_close;null_closes=0;
    shop_owner=1;shop_close(NULL);
    CHECK(null_closes==1 && !shop_owner && !scope_depth);
    CHECK(PTR(0x4cf410)==p1 && PTR(0x4cf298)==host_cards && U32(0x4ccd34)==300);
    /* Reproduce the dump: stage and ability cleanup already completed. */
    free(p1);free(current_guest);p1=current_guest=NULL;PTR(0x4cf410)=NULL;
    release_guest_cards();shop_owner=1;shop_close(NULL);shop_close(NULL);
    CHECK(null_closes==3 && !shop_owner && !scope_depth);
    puts("PASS native null shutdown close before and after player/card teardown, repeated calls and untouched host context");
}
static void test_initial_rewards(void) {
    reset_context();stub(0x411460,fake_add_card);
    unsigned char cards[2][0x60]={{0}};
    for(unsigned i=0;i<2;++i) {FIELD(cards[i],4)=26+i;FIELD(cards[i],0xc)=(uint32_t)(uintptr_t)cards[i];}
    FIELD(cards[0],0x10)=(uint32_t)(uintptr_t)OFF(cards[1],0xc);
    FIELD(host_cards,0x1c)=(uint32_t)(uintptr_t)OFF(cards[0],0xc);FIELD(host_cards,0x28)=2;
    initial_cards_cloned=0;enter_p2(0);clone_initial_cards();
    CHECK(FIELD(guest_cards,0x28)==2);CHECK(U32(0x4ccd34)==400);
    CHECK(U32(0x4ccd38)==200 && U32(0x4ccd48)==5);
    clone_initial_cards();CHECK(FIELD(guest_cards,0x28)==2);leave_p2(0);
    CHECK(U32(0x4ccd34)==300 && U32(0x4ccd48)==2);
    stub(0x4015a0,fake_remove);expected_manager=(void *)0x5555;PTR(0x4cf294)=expected_manager;
    FIELD(guest_cards,4)=(uint32_t)(uintptr_t)calloc(1,0x30);
    FIELD(guest_cards,8)=(uint32_t)(uintptr_t)calloc(1,0x30);
    fake_card_vtable[20]=(uint32_t)(uintptr_t)fake_delete_card;
    removals=deletions=0;release_guest_cards();
    CHECK(!guest_cards && !initial_cards_cloned && removals==2 && deletions==2);
    CHECK(PTR(0x4cf298)==host_cards && FIELD(host_cards,0x28)==2);
    FIELD(host_cards,0x1c)=0;
    puts("PASS independent initial inventory, no duplicated rewards and safe guest card release");
}
static void test_failed_guest_create(void) {
    reset_context();free(current_guest);current_guest=NULL;stub(0x45af20,fake_player_create);
    U32(0x4cccc8)=0;U32(0x4cccdc)=2;PTR(0x4cf2a4)=NULL;
    last_stage=1;have_guest_resources=1;PTR(0x570920)=(void *)0x4444;requested=0;
    create_guest();CHECK(!current_guest);CHECK(PTR(0x4cf410)==p1 && PTR(0x4cf298)==host_cards);
    CHECK(U32(0x4ccd34)==300 && U32(0x4ccd38)==100 && U32(0x4ccd48)==2);
    CHECK(PTR(0x570920)==(void *)0x4444 && PTR(0x4cf2d4)==(void *)0x1111);
    CHECK(requested && !have_guest_resources);
    puts("PASS failed guest constructor restores host state and permits retry");
}
static void test_stamp_cache(void) {
    CHECK(!stamp_cache_guard);stamp_cache_guard=1;
    void *manager=calloc(1,0x3121000),*vm=calloc(1,0x624);CHECK(manager && vm);
    stub(0x47e730,fake_flush);stamp_draw_original=fake_stamp_draw;
    FIELD(manager,0x3120e04)=42;FIELD(manager,0x3120e14)=84;*OFF(manager,0x3120e0a)=7;
    FIELD(vm,0x1c)=FIELD(vm,0x20)=16;FIELD(vm,0x24)=27;FIELD(vm,0x534)=8u<<26;
    flushes=draws=0;CHECK(draw_menu_stamp(manager,vm)==17);CHECK(flushes==1 && draws==1);
    FIELD(manager,0x3120e04)=42;FIELD(manager,0x3120e14)=84;*OFF(manager,0x3120e0a)=7;
    FIELD(vm,0x24)=18;flushes=draws=0;
    CHECK(draw_menu_stamp(manager,vm)==17);CHECK(!flushes && draws==1);
    FIELD(vm,0x24)=27;stamp_cache_guard=0;CHECK(draw_menu_stamp(manager,vm)==17);CHECK(!flushes);
    stamp_cache_guard=0;free(vm);free(manager);
    puts("PASS clear-stamp-only cache guard flushes before invalidation; bypass works");
}
static SHORT WINAPI fake_key(int vk) {return vk==pressed_vk?(SHORT)0x8000:0;}
static void test_keyboard_and_menu(void) {
    const int keys[]={'I','K','J','L','G','F','H','U','O'};
    const uint32_t masks[]={0x10,0x20,0x40,0x80,1,8,2,0x400,0x800};
    for(unsigned i=0;i<9;++i) {pressed_vk=keys[i];CHECK(read_p2_keys(fake_key)==masks[i]);}
    for(const char *c="WASDPRTZXCN";*c;++c) {pressed_vk=*c;CHECK(!read_p2_keys(fake_key));}
    unsigned original=kb_keys[1][0];kb_keys[1][0]='A';pressed_vk='A';CHECK(read_p2_keys(fake_key)==0x10);pressed_vk='I';CHECK(!read_p2_keys(fake_key));
    pressed_vk=VK_UP;CHECK(kb_sample(0,fake_key)==0x10 && kb_sample(2,fake_key)==0x10);kb_keys[1][0]=original;
    original=kb_keys[2][4];kb_keys[2][4]=VK_SPACE;pressed_vk=VK_SPACE;CHECK(kb_sample(2,fake_key)==1 && !kb_sample(0,fake_key) && !read_p2_keys(fake_key));kb_keys[2][4]=original;
    CHECK(kb_native[0] && !kb_native[1] && kb_native[2]);
    wchar_t dir[MAX_PATH],config[MAX_PATH],ini[MAX_PATH];GetTempPathW(MAX_PATH,dir);size_t end=wcslen(dir);swprintf(dir+end,MAX_PATH-end,L"TH18-key-test-%lu",(unsigned long)GetCurrentProcessId());CHECK(CreateDirectoryW(dir,NULL));
    swprintf(config,MAX_PATH,L"%ls\\coop.ini",dir);swprintf(ini,MAX_PATH,L"%ls\\keybindings.ini",dir);
    CHECK(WritePrivateProfileStringW(L"LocalP1",L"Up",L"65",ini));CHECK(WritePrivateProfileStringW(L"LocalP1",L"UseNative",L"0",ini));
    CHECK(WritePrivateProfileStringW(L"LocalP2",L"Up",L"87",ini));CHECK(WritePrivateProfileStringW(L"Online",L"Shoot",L"32",ini));CHECK(WritePrivateProfileStringW(L"Online",L"UseNative",L"0",ini));
    unsigned saved_keys[3][9];memcpy(saved_keys,kb_keys,sizeof(saved_keys));kb_load(config);
    CHECK(!kb_native[0] && !kb_native[2]);pressed_vk='A';CHECK(kb_sample(0,fake_key)==0x10);pressed_vk='W';CHECK(read_p2_keys(fake_key)==0x10);
    pressed_vk=VK_SPACE;CHECK(kb_online(0x80001|0x80,fake_key)==0x80001);pressed_vk='Z';CHECK(kb_online(1,fake_key)==0);
    memcpy(kb_keys,saved_keys,sizeof(saved_keys));kb_native[0]=kb_native[2]=1;
    CHECK(WritePrivateProfileStringW(L"LocalP2",L"Up",L"75",ini));kb_load(config);CHECK(kb_keys[1][0]=='I');
    memcpy(kb_keys,saved_keys,sizeof(saved_keys));kb_native[0]=kb_native[2]=1;CHECK(kb_online(0x80dfb,fake_key)==0x80dfb);
    CHECK(DeleteFileW(ini));CHECK(RemoveDirectoryW(dir));

    menu_previous=0;memset(menu_age,0,sizeof(menu_age));unsigned pulses=0;
    for(unsigned frame=0;frame<60;++frame) {
        uint32_t held=0x80,repeat=menu_repeat(held),edge=held&~menu_previous;
        if(frame<24) CHECK(!repeat);
        pulses+=(repeat|edge)!=0;menu_previous=held;
    }
    CHECK(pulses==7);
    menu_repeat(0);menu_previous=0;CHECK(!menu_repeat(0x80));CHECK((0x80&~menu_previous)==0x80);
    puts("PASS IJKL layout avoids native P1 keys; tap moves once, hold repeats after delay");
}
static void fake_revive_reset(void) {
    unsigned id=PTR(0x4cf410)==current_guest;
    ++reset_calls;reset_mask|=1u<<id;
    if(expected_revived>=0) CHECK(id==(unsigned)expected_revived);
    CHECK(PTR(0x4cf298)==(id?guest_cards:host_cards));
    FIELD(ship(id),0x476ac)=1;
}
static void fake_gameover(void) {++gameovers;}
static unsigned pause_calls,pause_rebuilds;static int expected_pause_phase;
static void TC fake_pause_phase(void *menu,int phase) {CHECK(phase==2);FIELD(menu,0x1f4)=phase;++pause_rebuilds;}
static void TC fake_pause_actions(void *menu) {CHECK((int)FIELD(menu,0x1f4)==expected_pause_phase);++pause_calls;}
static void test_no_continue(void) {
    reset_context();void *menu=calloc(1,0x3f8);CHECK(menu!=NULL);
    pause_calls=pause_rebuilds=0;pause_actions_original=fake_pause_actions;stub(0x4577d0,fake_pause_phase);
    U32(0x4cccc8)=0;U32(0x4cccd0)=5;FIELD(menu,0x1ec)=2;FIELD(menu,0x1f4)=6;expected_pause_phase=6;
    coop_pause_actions(menu);CHECK(!U32(0x4cccd0) && !pause_rebuilds);
    partner[0].ghost=partner[1].ghost=1;*resource(0,0x10)=*resource(1,0x10)=-1;
    FIELD(menu,0x1f4)=18;expected_pause_phase=2;coop_pause_actions(menu);
    CHECK(pause_rebuilds==1 && partner[0].ghost && partner[1].ghost && *resource(0,0x10)==-1 && *resource(1,0x10)==-1);
    FIELD(menu,0x34)=1;FIELD(menu,0x1f4)=18;expected_pause_phase=18;coop_pause_actions(menu);CHECK(pause_rebuilds==1); /* Game-over return to title shares state 18. */
    FIELD(menu,0x34)=0;
    FIELD(menu,0x1ec)=1;FIELD(menu,0x1f4)=18;expected_pause_phase=18;coop_pause_actions(menu);CHECK(pause_rebuilds==1); /* Resume ordinary pause remains valid. */
    FIELD(menu,0x1ec)=2;FIELD(menu,0x1f4)=10;expected_pause_phase=10;coop_pause_actions(menu);CHECK(pause_rebuilds==1); /* Full retry remains valid. */
    U32(0x4cccc8)=0x40;U32(0x4cccd0)=5;FIELD(menu,0x1f4)=18;expected_pause_phase=18;coop_pause_actions(menu);CHECK(U32(0x4cccd0)==5); /* Native demo/replay bypass. */
    U32(0x4cccc8)=0;void *guest=current_guest;current_guest=NULL;coop_pause_actions(menu);CHECK(U32(0x4cccd0)==5);current_guest=guest;
    CHECK(pause_calls==7);free(menu);
    puts("PASS native Continue credit disable and stale-confirm gate, unchanged ghosts, pause resume/retry and native-only bypass");
}
static void *TC fake_spawn_transfer(void *manager,uint32_t type,const float *position,
                         uint32_t a,uint32_t b,uint32_t c,uint32_t d,uint32_t e,uint32_t f) {
    (void)a;(void)b;(void)c;(void)d;(void)e;(void)f;
    if(spawn_fail || (spawn_fail_after>=0 && spawn_count>=spawn_fail_after)) return NULL;
    void *item=OFF(manager,0x14+spawn_count++*0xc94);spawned_type=type;
    FIELD(item,0)=(uint32_t)(uintptr_t)OFF(manager,0xe6baf4);
    FIELD(item,0xc74)=1;FIELD(item,0xc78)=type;FIELD(item,0xc50)=0;
    FLT(item,0xc2c)=position[0];FLT(item,0xc30)=position[1];return item;
}
static void rule_frame(uint32_t a,uint32_t b) {uint32_t keys[2]={a,b};partner_tick(keys);}
static void taps(unsigned id,uint32_t button,unsigned count) {
    for(unsigned n=0;n<count;++n) {rule_frame(id?0:button,id?button:0);rule_frame(0,0);}
}
static void test_ghost_and_rescue(void) {
    reset_context();stub(0x45abb0,fake_revive_reset);stub(0x458a30,fake_gameover);
    reset_calls=gameovers=0;set_position(p1,0,300);set_position(current_guest,10,300);
    *(int32_t *)(p2_resources+0x10)=-1;FIELD(current_guest,0x476ac)=2;FIELD(current_guest,0x638)=30;
    enter_p2(0);prepare_player_tick(1);leave_p2(0);
    CHECK(partner[1].ghost && FIELD(current_guest,0x638)==29 && !alive(1));
    CHECK(*resource(1,0)==0 && *resource(1,0x20)==3 && *resource(1,0x24)==0);
    float x=FLT(current_guest,0x620);rule_frame(0,0);CHECK(FLT(current_guest,0x620)!=x);
    for(unsigned n=0;n<120;++n) rule_frame(8,0);
    CHECK(!reset_calls);CHECK(U32(0x4ccd48)==2); /* Holding is not five presses. */
    set_position(current_guest,10,300);
    rule_frame(0,0);taps(0,8,4);CHECK(!reset_calls && partner[1].ghost);
    rule_frame(8,0);CHECK(reset_calls==1 && !partner[1].ghost);
    CHECK(U32(0x4ccd48)==1 && *(int32_t *)(p2_resources+0x10)==0);
    CHECK(*(int32_t *)p2_resources==200 && *(int32_t *)(p2_resources+0x20)==2);
    CHECK(FIELD(current_guest,0x476ac)==1 && FIELD(current_guest,0x47778)==240 && FLT(current_guest,0x4777c)==240);
    CHECK(PTR(0x4cf410)==p1 && PTR(0x4cf298)==host_cards && scope_depth==0);
    CHECK(U32(0x4ccd34)==300 && p2_funds==400);
    reset_context();expected_revived=0;set_position(p1,0,300);set_position(current_guest,0,300);
    *resource(0,0x10)=-1;FIELD(p1,0x476ac)=2;FIELD(p1,0x638)=30;prepare_player_tick(0);
    taps(1,8,5);CHECK(!partner[0].ghost && *resource(0,0x10)==0 && *resource(1,0x10)==4);
    CHECK(*resource(0,0)==200 && *resource(1,0)==200 && PTR(0x4cf410)==p1 && !scope_depth);
    expected_revived=1;
    reset_context();set_position(p1,0,300);set_position(current_guest,0,300);
    partner[1].ghost=1;FIELD(current_guest,0x476ac)=2;*resource(1,0x10)=-1;
    taps(0,8,2);for(int n=0;n<25;++n)rule_frame(0,0);taps(0,8,3);
    CHECK(partner[1].ghost && partner[0].life_taps==3); /* Expired taps don't accumulate. */
    rule_frame(9,0);CHECK(!partner[0].life_taps); /* Shooting cancels life donation. */
    *resource(0,0x10)=0;taps(0,8,5);CHECK(partner[1].ghost && *resource(0,0x10)==0);
    *resource(0,0x10)=2;set_position(p1,100,300);taps(0,8,5);CHECK(partner[1].ghost);
    partner[0].ghost=1;partner[0].dx=partner[1].dx=.2f;partner[0].dy=partner[1].dy=.2f;
    set_position(p1,180,432);rule_frame(0,0);
    CHECK(FLT(p1,0x620)==180 && partner[0].dx<0 && FLT(p1,0x624)==416 && partner[0].dy<0);
    for(int n=1;n<179;++n)rule_frame(0,0);CHECK(!gameovers);
    rule_frame(0,0);CHECK(gameovers==1 && team_over);
    rule_frame(0,0);CHECK(gameovers==1);
    reset_context();FIELD(p1,0x476ac)=4;FIELD(p1,0x47908)=11;
    prepare_player_tick(0);CHECK(FIELD(p1,0x47908)==22);prepare_player_tick(0);CHECK(FIELD(p1,0x47908)==22);
    FIELD(p1,0x476ac)=1;prepare_player_tick(0);CHECK(FIELD(p1,0x47908)==11);
    FIELD(p1,0x476ac)=4;prepare_player_tick(0);CHECK(FIELD(p1,0x47908)==22);
    unsigned char bomb[0x40]={0};FIELD(bomb,0x30)=1;bomb_owner=0;
    FIELD(p1,0x476ac)=1;FIELD(p1,0x47778)=180;FIELD(current_guest,0x47778)=20;
    share_bomb_protection(bomb);CHECK(FIELD(current_guest,0x47778)==60 && FLT(current_guest,0x4777c)==60);
    FIELD(current_guest,0x47778)=15;share_bomb_protection(bomb);CHECK(FIELD(current_guest,0x47778)==15);
    FIELD(current_guest,0x476ac)=2;FIELD(current_guest,0x47778)=0;share_bomb_protection(bomb);CHECK(!FIELD(current_guest,0x47778));
    puts("PASS spirits, drift, five focus presses, rescue cost/own context, last-life guard, timeout and wipe grace");
}
static void test_transfers_and_power_priority(void) {
    reset_context();set_position(p1,0,300);set_position(current_guest,10,300);
    void *manager=calloc(1,0x14+0x1258*0xc94+0x100);CHECK(manager!=NULL);
    PTR(0x4cf2ec)=manager;item_manager=manager;spawn_original=fake_spawn_transfer;
    spawn_count=spawn_fail=0;*resource(1,0x10)=0;*resource(0,0)=300;*resource(1,0)=100;
    taps(0,1,4);CHECK(!spawn_count);taps(0,1,1);
    CHECK(spawn_count==6 && spawned_type==1 && *resource(0,0)==200 && *resource(1,0)==100);
    int total=0;for(int n=0;n<6;++n) {CHECK(transfer_owner[n]==1);total+=transfer_power[n];}
    CHECK(total==100);
    void *item=OFF(manager,0x14);uint32_t regs[8]={0};regs[0]=(uint32_t)(uintptr_t)item;
    for(int n=0;n<6;++n) {
        regs[0]=(uint32_t)(uintptr_t)OFF(manager,0x14+n*0xc94);
        coop_select_item(regs);CHECK(regs[1]==(uint32_t)(uintptr_t)current_guest && scope_depth==1);
        U32(0x4ccd38)+=100;FIELD((void *)(uintptr_t)regs[0],0xc74)=0;coop_pickup_confirmed=1;item_leave();
    }
    CHECK(*resource(1,0)==200 && *resource(0,0)==200 && scope_depth==0);
    CHECK(transfer_owner[0]==-1 && transfer_power[0]==0);
    taps(0,8,5);CHECK(spawn_count==7 && spawned_type==5 && *resource(0,0x10)==1);
    CHECK(transfer_owner[6]==1 && FIELD(OFF(manager,0x14+6*0xc94),0xc74)==4);
    spawn_fail=1;*resource(0,0)=300;taps(0,1,5);CHECK(*resource(0,0)==300);
    taps(0,8,5);CHECK(*resource(0,0x10)==1); /* Allocation failure must cost nothing. */
    spawn_fail=0;spawn_fail_after=spawn_count+2;int partial_start=spawn_count;
    taps(0,1,5);CHECK(*resource(0,0)==300 && spawn_count==partial_start+2);
    for(int n=partial_start;n<spawn_count;++n) {
        CHECK(!FIELD(OFF(manager,0x14+n*0xc94),0xc74) && transfer_owner[n]==-1);
    }
    void *returned=OFF(manager,0x14+(spawn_count-1)*0xc94),*head=OFF(manager,0xe6baf4);
    CHECK(FIELD(head,4)==(uint32_t)(uintptr_t)returned && FIELD(returned,8)==(uint32_t)(uintptr_t)head);
    CHECK(FIELD(returned,4)==(uint32_t)(uintptr_t)OFF(manager,0x14+partial_start*0xc94));
    spawn_fail_after=-1;
    memset(transfer_owner,-1,sizeof(transfer_owner));memset(item_owner,-1,sizeof(item_owner));
    regs[0]=(uint32_t)(uintptr_t)item;
    FIELD(item,0xc74)=2;FIELD(item,0xc78)=1;FIELD(item,0xc50)=0;
    set_position(p1,0,100);set_position(current_guest,150,100);FLT(item,0xc2c)=0;FLT(item,0xc30)=100;
    *resource(0,0)=400;*resource(1,0)=100;coop_select_item(regs);
    CHECK(regs[1]==(uint32_t)(uintptr_t)current_guest);item_leave();
    FIELD(item,0xc78)=2;FIELD(item,0xc74)=2;coop_select_item(regs);
    CHECK(regs[1]==(uint32_t)(uintptr_t)p1);item_leave();
    /* Both eligible and above the line: nearest wins, not item-index rotation. */
    FIELD(item,0xc78)=1;FIELD(item,0xc74)=2;*resource(0,0)=100;*resource(1,0)=100;
    FLT(p1,0x47998)=FLT(current_guest,0x47998)=200;FLT(item,0xc2c)=140;
    coop_select_item(regs);CHECK(regs[1]==(uint32_t)(uintptr_t)current_guest && FIELD(item,0xc74)==4);item_leave();
    /* A full player touching it sends it to a distant non-full teammate below the line. */
    FIELD(item,0xc74)=2;*resource(0,0)=400;set_position(p1,0,300);set_position(current_guest,150,300);
    FLT(item,0xc2c)=0;FLT(item,0xc30)=300;FLT(p1,0x4798c)=16;
    coop_select_item(regs);CHECK(regs[1]==(uint32_t)(uintptr_t)current_guest && FIELD(item,0xc74)==4);item_leave();
    *resource(1,0)=400;*resource(0,0)=100;coop_select_item(regs);
    CHECK(regs[1]==(uint32_t)(uintptr_t)p1 && FIELD(item,0xc74)==4);item_leave();
    spawn_count=20;transfer_owner[20]=1;transfer_power[20]=63;
    float origin[3]={0,300,0};spawn_item(manager,2,origin,0,0,0,0,0,0);
    CHECK(spawn_count==21 && transfer_owner[20]==-1 && transfer_power[20]==0);
    transfer_owner[21]=transfer_owner[22]=1;spawn_item(manager,1,origin,0,0,0,0,0,0);
    CHECK(spawn_count==23 && transfer_owner[21]==-1 && transfer_owner[22]==-1);
    free(manager);item_manager=NULL;PTR(0x4cf2ec)=NULL;
    puts("PASS five-shot power gift, native homing item ownership, normalized amount, life item and no failed-spawn cost");
}
static unsigned supply_card_calls[3],supply_respawns;
static unsigned supply_seat(void){
    for(unsigned id=0;id<party_count();++id)if(PTR(0x4cf410)==ship(id))return id;
    CHECK(0);return 0;
}
static void TC supply_life(void *game){
    CHECK(game==(void *)0x4cccdc);
    if((int32_t)U32(0x4ccd48)>=(int32_t)U32(0x4ccd54))U32(0x4ccd4c)=0;
    if(++U32(0x4ccd48)>U32(0x4ccd54)){U32(0x4ccd48)=U32(0x4ccd54);U32(0x4ccd4c)=0;}
    ++U32(0x4ccd50);
}
static void TC supply_life_piece(void *game,unsigned unused){
    CHECK(!unused && game==(void *)0x4cccdc);
    if((int32_t)U32(0x4ccd48)>=(int32_t)U32(0x4ccd54)){U32(0x4ccd4c)=0;return;}
    unsigned threshold=U32(0x4ccd50)?5:3;
    if(++U32(0x4ccd4c)>=threshold){U32(0x4ccd4c)-=threshold;supply_life(game);}
}
static void TC supply_bomb(void *game,unsigned unused){
    CHECK(!unused && game==(void *)0x4cccdc);
    if(++U32(0x4ccd58)>=U32(0x4ccd64)){U32(0x4ccd5c)=0;if(U32(0x4ccd58)>U32(0x4ccd64))U32(0x4ccd58)=U32(0x4ccd64);}
}
static void TC supply_bomb_piece(void *game,unsigned unused){
    CHECK(!unused && game==(void *)0x4cccdc);
    if(U32(0x4ccd58)>=U32(0x4ccd64)){U32(0x4ccd5c)=0;return;}
    if(++U32(0x4ccd5c)>=3){U32(0x4ccd5c)=0;supply_bomb(game,0);}
}
static int TC supply_power(void *game,unsigned amount){
    CHECK(game==(void *)0x4cccdc);unsigned power=U32(0x4ccd38)+amount;
    U32(0x4ccd38)=power>U32(0x4ccd3c)?U32(0x4ccd3c):power;return 1;
}
static int TC supply_card(void *manager,int id,int mode){
    unsigned seat=supply_seat();CHECK(manager==ability(seat) && mode==0 && id>=3 && id<=6);
    ++supply_card_calls[seat];U32(0x4ccd34)+=(unsigned)id*10;return 1;
}
static void __cdecl supply_reset(void){++supply_respawns;FIELD(PTR(0x4cf410),0x476ac)=1;}
static void supply_original(unsigned type){
    void *game=(void *)0x4cccdc;
    if(type==4)supply_life_piece(game,0);
    else if(type==5){unsigned n=U32(0x4ccd50);supply_life(game);U32(0x4ccd50)=n;}
    else if(type==6)supply_bomb_piece(game,0);else if(type==7)supply_bomb(game,0);
    else if(type==8)supply_power(game,U32(0x4ccd3c));
    else if(type>=16 && type<=19)supply_card(PTR(0x4cf298),(int)U32(0x4b4020+type*8),0);
    else if(type==2)++U32(0x4ccd34);else if(type==1 || type==3)++U32(0x4ccd38);
}
static void supply_pickup(void *item,unsigned collector,unsigned type,int gift,int confirmed){
    CHECK(!scope_depth);FIELD(item,0xc74)=4;FIELD(item,0xc78)=type;FIELD(item,0xc50)++;
    set_position(p1,0,300);set_position(ship(1),100,300);if(party_count()==3)set_position(ship(2),-100,300);
    FLT(item,0xc2c)=FLT(ship(collector),0x620);FLT(item,0xc30)=300;
    item_owner[0]=(int8_t)collector;transfer_owner[0]=gift?-2:-1;transfer_power[0]=0;
    uint32_t regs[8]={0};regs[0]=(uint32_t)(uintptr_t)item;coop_select_item(regs);
    CHECK(item_scope==collector+1 && regs[1]==(uint32_t)(uintptr_t)ship(collector));
    if(confirmed)supply_original(type);
    FIELD(item,0xc74)=0;coop_pickup_confirmed=confirmed;item_leave();item_leave();
    CHECK(!scope_depth && !item_scope && !coop_pickup_confirmed && PTR(0x4cf410)==p1);
}
static void test_shared_supplies(unsigned players){
    reset_context();CoopOnlineMap map={0};map.players=players;online_map=&map;
    if(players==3){guests[1].player=calloc(1,0x479d0);guests[1].cards=calloc(1,0xd70);CHECK(ship(2) && ability(2));FIELD(ship(2),0x476ac)=1;guests[1].funds=600;}
    stub(0x457570,supply_life_piece);stub(0x4575f0,supply_life);stub(0x4576e0,supply_bomb_piece);
    stub(0x457690,supply_bomb);stub(0x4573f0,supply_power);stub(0x411460,supply_card);stub(0x45abb0,supply_reset);
    for(unsigned type=16;type<=19;++type)U32(0x4b4020+type*8)=type-13;
    item_manager=calloc(1,0x14+0xc94);CHECK(item_manager);void *item=OFF(item_manager,0x14);
    for(unsigned id=0;id<players;++id){*resource(id,0x10)=1;*resource(id,0x1c)=7;*resource(id,0x14)=2;*resource(id,0x18)=0;*resource(id,0x20)=1;*resource(id,0x24)=2;*resource(id,0x2c)=8;*resource(id,4)=400;}
    *resource(0,0x10)=7;*resource(0,0x14)=0;
    if(players==3){*resource(2,0x18)=1;*resource(2,0x14)=4;}
    supply_pickup(item,0,4,0,1);CHECK(*resource(0,0x10)==7 && *resource(1,0x10)==2 && *resource(1,0x14)==0 && *resource(1,0x18)==1);
    if(players==3)CHECK(*resource(2,0x10)==2 && *resource(2,0x14)==0 && *resource(2,0x18)==2);
    unsigned threshold=*resource(1,0x18);supply_pickup(item,0,5,0,1);CHECK(*resource(1,0x10)==3 && *resource(1,0x18)==(int)threshold);
    *resource(0,0x20)=8;*resource(0,0x24)=0;supply_pickup(item,0,6,0,1);
    CHECK(*resource(0,0x20)==8 && *resource(1,0x20)==2 && *resource(1,0x24)==0);
    supply_pickup(item,1,7,0,1);CHECK(*resource(0,0x20)==8 && *resource(1,0x20)==3);
    *resource(0,0)=400;*resource(1,0)=50;supply_pickup(item,1,8,0,1);CHECK(*resource(0,0)==400 && *resource(1,0)==400);
    memset(supply_card_calls,0,sizeof(supply_card_calls));
    for(unsigned type=16;type<=19;++type)supply_pickup(item,(type-16)%players,type,0,1);
    for(unsigned id=0;id<players;++id)CHECK(supply_card_calls[id]==4);
    *resource(0,0)=100;*resource(1,0)=100;uint32_t funds=U32(0x4ccd34);
    supply_pickup(item,1,1,0,1);CHECK(*resource(0,0)==100 && *resource(1,0)==101);
    supply_pickup(item,1,3,0,1);CHECK(*resource(0,0)==100 && *resource(1,0)==102);
    uint32_t guest_funds=p2_funds;supply_pickup(item,1,2,0,1);CHECK(U32(0x4ccd34)==funds && p2_funds==guest_funds+1);
    int lives=*resource(1,0x10);supply_pickup(item,0,5,0,0);CHECK(*resource(1,0x10)==lives); /* inactive != picked up */
    int host_lives=*resource(0,0x10);supply_pickup(item,1,5,1,1);CHECK(*resource(0,0x10)==host_lives && *resource(1,0x10)==lives+1);
    *resource(1,0x10)=-1;*resource(1,0x14)=2;*resource(1,0x18)=0;partner[1].ghost=1;FIELD(ship(1),0x476ac)=2;supply_respawns=0;
    supply_pickup(item,0,4,0,1);CHECK(!partner[1].ghost && *resource(1,0x10)==0 && FIELD(ship(1),0x476ac)==1 && supply_respawns==1);
    free(item_manager);item_manager=NULL;
    if(players==3){free(ship(2));free(ability(2));memset(&guests[1],0,sizeof(guests[1]));}
    online_map=NULL;guest_slot=0;reset_context();
    printf("PASS shared supplies:%u seats, native per-seat caps/fragment thresholds/cards, spirit life return, accurate pickup witness; ordinary P/money and gifts stay individual\n",players);
}
static unsigned expected_ghost_alpha=80;
static void *view_root,*view_child;static int view_hidden;static unsigned vm_alpha;
static void *TC fake_view_lookup(void *manager,uint32_t id) {CHECK(manager==(void *)0x1234 && id==42);return view_root;}
static int __cdecl fake_scene_draw(void) {
    CHECK(((FIELD(view_root,0x534)&2)==0)==view_hidden);
    CHECK(((FIELD(view_child,0x534)&2)==0)==view_hidden);
    FIELD(view_root,0x534)|=0x100;return 17;
}
static int TC fake_vm_draw(void *manager,void *vm) {
    CHECK(manager==(void *)0x1234 && (FIELD(vm,0x524)>>24)==vm_alpha);
    CHECK((FIELD(vm,0x528)>>24)==vm_alpha);return 19;
}
static void test_personal_view_and_boss_rules(void) {
    reset_context();CoopOnlineMap map={0};online_map=&map;map.role=1;
    void *card=calloc(1,0x54);CHECK(card!=NULL);
    FIELD(card,4)=9;FIELD(card,0x24)=17;FIELD(card,0x38)=120;FIELD(card,0x48)=300;
    FIELD(card,0xc)=(uint32_t)(uintptr_t)card;FIELD(guest_cards,0x1c)=(uint32_t)(uintptr_t)OFF(card,0xc);
    FIELD(guest_cards,0x38)=(uint32_t)(uintptr_t)card;HudStats stats={0};read_cards(&stats,1,guest_cards);
    CHECK(stats.card_count[1]==1 && stats.cards[1][0].selected);
    CHECK(stats.cards[1][0].remaining==120 && stats.cards[1][0].duration==300);
    FIELD(guest_cards,0x1c)=FIELD(guest_cards,0x38)=0;free(card);
    CHECK(coop_view_player()==1 && !coop_shared_keyboard());map.reserved[1]=1;CHECK(coop_shared_keyboard());map.reserved[1]=0;
    map.reserved[1]=3;CHECK(!coop_shared_keyboard());map.reserved[1]=0;
    online_started=1;online_previous=1;online_previous_p2=0;online_keys[0]=online_keys[1]=1;
    PTR(0x4cf2cc)=(void *)1;CHECK(online_poll((void *)0x4ca210)==1);
    CHECK(U32(0x4ca210)==1 && U32(0x4ca214)==0); /* A held P1 button must not hide a fresh P2 dialogue edge. */
    PTR(0x4cf2cc)=NULL;online_keys[0]=0;CHECK(online_poll((void *)0x4ca210)==1);
    CHECK(!U32(0x4ca210) && input_latch.p1==0 && input_latch.p2==1); /* Battle input stays per player. */
    void *pause=calloc(1,0x1f0);CHECK(pause!=NULL);PTR(0x4cf40c)=pause;
    online_keys[1]=0x80;CHECK(online_poll((void *)0x4ca210)==1);
    CHECK(!U32(0x4ca210)); /* A dormant resident pause object must not merge P2 movement into P1. */
    FIELD(pause,0x1ec)=1;online_keys[1]=1;CHECK(online_poll((void *)0x4ca210)==1);
    CHECK(U32(0x4ca210)==1 && !(U32(0x4ca214)&1));
    PTR(0x4cf40c)=NULL;free(pause);
    online_started=online_previous=online_previous_p2=online_keys[0]=online_keys[1]=0;
    set_position(p1,0,300);set_position(current_guest,0,300);
    CHECK(teammate_alpha(0)==51 && teammate_alpha(1)==255);
    set_position(p1,150,300);CHECK(teammate_alpha(0)==128);
    set_position(p1,50,300);CHECK(teammate_alpha(0)==51);
    void *vm=calloc(1,0x624);CHECK(vm!=NULL);FIELD(vm,0x1c)=9;
    FIELD(vm,0x524)=0xff123456;FIELD(vm,0x528)=0xffabcdef;anm_draw_original=fake_vm_draw;vm_alpha=51;
    draw_alpha[0]=teammate_alpha(0);draw_alpha[1]=teammate_alpha(1);
    CHECK(anm_draw((void *)0x1234,vm)==19);CHECK(FIELD(vm,0x524)==0xff123456 && FIELD(vm,0x528)==0xffabcdef);
    FIELD(vm,0x1c)=18;vm_alpha=255;CHECK(anm_draw((void *)0x1234,vm)==19);
    FIELD(vm,0x1c)=31;CHECK(anm_draw((void *)0x1234,vm)==19);free(vm);
    view_root=calloc(1,0x624);view_child=calloc(1,0x624);void *link=calloc(1,8);CHECK(view_root && view_child && link);
    FIELD(view_root,0x534)=0x12;FIELD(view_child,0x534)=0x42;FIELD(view_root,0x598)=(uint32_t)(uintptr_t)link;
    FIELD(link,0)=(uint32_t)(uintptr_t)view_child;FIELD(view_child,0x598)=(uint32_t)(uintptr_t)link; /* Cyclic child list must terminate. */
    FIELD(host_cards,0x3c)=42;PTR(0x51f65c)=(void *)0x1234;stub(0x488b40,fake_view_lookup);scene_draw_original=fake_scene_draw;
    view_hidden=1;CHECK(scene_draw()==17);CHECK(FIELD(view_root,0x534)==0x112 && FIELD(view_child,0x534)==0x42);
    map.role=0;view_hidden=0;CHECK(scene_draw()==17);
    FIELD(guest_cards,0x3c)=42;view_hidden=1;CHECK(scene_draw()==17);
    online_map=NULL;CHECK(scene_draw()==17); /* Same-window view suppresses the other native root too. */
    FIELD(guest_cards,0x3c)=0;
    free(view_root);free(view_child);free(link);PTR(0x51f65c)=NULL;online_map=NULL;
    void *enemy=calloc(1,0x558c),*script=calloc(1,0x6834),*manager=calloc(1,0x88);CHECK(enemy && script && manager);
    FIELD(enemy,0x5588)=(uint32_t)(uintptr_t)script;FIELD(script,0x6830)=123;FIELD(manager,0x48)=123;PTR(0x4cf2d0)=manager;
    FIELD(enemy,0x4ff4)=9000;uint32_t regs[8]={0};regs[4]=(uint32_t)(uintptr_t)enemy;regs[1]=120;
    coop_scale_boss_damage(regs);CHECK(regs[1]==90 && FIELD(enemy,0x4ff4)==9000);
    *(unsigned char *)(uintptr_t)0x42f2c0=0xc3;uint32_t bridged;
    __asm__ volatile("pushl %%ebx\n\tpushl %%esi\n\tmovl %1,%%ebx\n\tmovl $120,%%esi\n\tcall _boss_damage_bridge\n\tmovl %%esi,%%eax\n\tpopl %%esi\n\tpopl %%ebx"
        :"=a"(bridged):"r"(enemy):"ecx","edx","cc","memory");
    CHECK(bridged==90); /* Exercise the real register/FPU-preserving assembly bridge. */
    partner[1].ghost=1;regs[1]=120;coop_scale_boss_damage(regs);CHECK(regs[1]==120);
    partner[1].ghost=0;FIELD(current_guest,0x476ac)=2;regs[1]=4;coop_scale_boss_damage(regs);CHECK(regs[1]==3); /* A normal respawn is still a participant. */
    FIELD(manager,0x48)=0;regs[1]=120;coop_scale_boss_damage(regs);CHECK(regs[1]==120);
    free(enemy);free(script);free(manager);PTR(0x4cf2d0)=NULL;
    puts("PASS local seat identity, player/option/shot ANM fade, personal HUD suppression/restoration, registered Boss-only damage and ghost roster");
}
static void *active_vm,*active_file;static unsigned active_allocs,active_updates,active_deletes;static int active_registered;
static void *fake_active_allocate(void) {++active_allocs;return active_vm;}
static void *TC fake_active_lookup(void *manager,uint32_t id) {(void)manager;CHECK(id==77);return active_registered?active_vm:NULL;}
static void *TC fake_active_bind(void *file,void *vm,int script) {
    CHECK(file==active_file && vm==active_vm && !script);return vm;
}
static int TC fake_active_tick(void *vm) {
    CHECK(vm==active_vm && (FIELD(vm,0x538)&0x1000));CHECK(!FIELD(vm,0x44) && !FLT(vm,0x5f4));return 1;
}
static void *__stdcall fake_active_register(uint32_t *id,void *vm) {
    CHECK(vm==active_vm && !FIELD(vm,0x520));*id=77;active_registered=1;return id;
}
static void TC fake_active_update(void *manager) {CHECK(manager==guest_cards && FIELD(manager,0x3c)==77);++active_updates;}
static void __stdcall fake_active_delete(uint32_t id) {CHECK(id==77);active_registered=0;++active_deletes;}
static void test_native_active_ui_lifetime(void) {
    reset_context();active_vm=calloc(1,0x624);active_file=calloc(1,0x138);
    void *anm=calloc(1,0x3121000);CHECK(active_vm && active_file && anm);
    PTR(0x51f65c)=anm;FIELD(guest_cards,0x10)=(uint32_t)(uintptr_t)active_file;
    active_allocs=active_updates=active_deletes=active_registered=0;
    stub(0x489320,fake_active_allocate);stub(0x407420,fake_active_bind);stub(0x478580,fake_active_tick);
    stub(0x488350,fake_active_register);stub(0x488b40,fake_active_lookup);stub(0x408c30,fake_active_update);stub(0x488cf0,fake_active_delete);
    FIELD(host_cards,0x3c)=42;ensure_guest_active_ui();ensure_guest_active_ui();
    CHECK(active_allocs==1 && active_updates==1 && FIELD(active_file,0x134)==1);
    CHECK(FIELD(host_cards,0x3c)==42 && FIELD(guest_cards,0x3c)==77);
    FIELD(guest_cards,4)=(uint32_t)(uintptr_t)calloc(1,0x30);FIELD(guest_cards,8)=(uint32_t)(uintptr_t)calloc(1,0x30);
    expected_manager=PTR(0x4cf294);stub(0x4015a0,fake_remove);release_guest_cards();
    CHECK(!guest_cards && active_deletes==1 && FIELD(host_cards,0x3c)==42);
    free(active_vm);free(active_file);free(anm);PTR(0x51f65c)=NULL;
    puts("PASS native active-card script root initialized once, independent ID, native update and own-only cleanup");
}
static int TC fake_native_draw(void *player) {
    CHECK(FIELD(player,0x476ac)==1);
    CHECK((FIELD(player,0x53c)>>24)==expected_ghost_alpha);
    CHECK(FIELD(player,0x538)==FIELD(player,0x53c));
    /* The renderer sees the current native VM frame and original scale. */
    CHECK(FIELD(player,0x38)==23);CHECK(FLT(player,0x64)==1.25f);
    return 31;
}
static unsigned idle_binds;
static void *TC fake_bind_idle(void *file,void *vm,int script) {
    CHECK(file==(void *)0x1234 && vm==OFF(p1,0x14) && script==0);++idle_binds;return vm;
}
static int TC fake_stage_shop_init(void *shop,const float *position) {
    (void)shop;(void)position;CHECK(!partner[1].ghost && FIELD(current_guest,0x476ac)==1);return 1;
}
static void test_native_spirit_and_stage_clear(void) {
    reset_context();stub(0x407420,fake_bind_idle);FIELD(p1,0xc)=0x1234;
    *resource(0,0x10)=-1;FIELD(p1,0x476ac)=2;FIELD(p1,0x638)=30;idle_binds=0;
    prepare_player_tick(0);prepare_player_tick(0);CHECK(idle_binds==1 && partner[0].ghost);
    FIELD(p1,0x538)=0xff123456;FIELD(p1,0x53c)=0xff765432;FIELD(p1,0x38)=23;FLT(p1,0x64)=1.25f;
    CHECK(draw_ship(0,fake_native_draw)==31);
    CHECK(FIELD(p1,0x476ac)==2 && FIELD(p1,0x53c)==0xff765432);
    CHECK(FIELD(p1,0x38)==23 && FLT(p1,0x64)==1.25f);
    CHECK(FIELD(p1,0x538)==0xff123456);
    partner[1].life_taps=1;partner[1].life_target=1;
    for(unsigned n=1;n<=12;++n) {
        spirit_fade_tick();expected_ghost_alpha=(unsigned)(80+14.5f*n+.5f);
        CHECK(draw_ship(0,fake_native_draw)==31);
        float unchanged=spirit_draw_alpha[0];CHECK(draw_ship(0,fake_native_draw)==31 && spirit_draw_alpha[0]==unchanged);
    }
    CHECK((unsigned)(spirit_draw_alpha[0]+.5f)==254);
    CHECK(FIELD(p1,0x538)==0xff123456 && FIELD(p1,0x53c)==0xff765432);
    ghost_alpha=255;spirit_was_drawn[0]=0;expected_ghost_alpha=255;
    CHECK(draw_ship(0,fake_native_draw)==31);
    spirit_was_drawn[0]=1;spirit_draw_alpha[0]=254.1f;spirit_fade_tick();spirit_fade_tick();expected_ghost_alpha=254;
    CHECK(draw_ship(0,fake_native_draw)==31);CHECK(spirit_draw_alpha[0]==254);
    ghost_alpha=80;
    partner[1].life_taps=0;expected_ghost_alpha=80;
    FIELD(p1,0xc)=0;*resource(0,0x10)=2;
    partner[0].ghost=0;partner[1].ghost=1;FIELD(current_guest,0x476ac)=2;
    *resource(1,0x10)=-1;*resource(1,0)=0;reset_calls=0;reset_mask=0;
    expected_revived=-1;stub(0x45abb0,fake_revive_reset);
    shop_init_original=fake_stage_shop_init;float position[3]={0,0,0};
    CHECK(shop_init(NULL,position)==1);CHECK(reset_calls==1 && reset_mask==2);
    CHECK(!partner[1].ghost && *resource(1,0x10)==0 && *resource(1,0)==200);
    CHECK(*resource(0,0x10)==2 && U32(0x4ccd34)==300 && p2_funds==400);
    revive_at_clear();CHECK(reset_calls==1); /* Repeated menus do not grant extra resources. */
    partner[1].ghost=0;*resource(1,0x10)=-1;FIELD(current_guest,0x476ac)=2;FIELD(current_guest,0x638)=3;
    CHECK(shop_init(NULL,position)==1);CHECK(reset_calls==2 && *resource(1,0x10)==0);
    expected_revived=1;
    puts("PASS native animated spirit draw preserves VM size, color/state; stage-clear rescue costs no teammate life");
}
static unsigned char *test_anm,*test_host_file,*test_guest_file;
static void *test_host_bomb;
static int factory_calls;
static unsigned bomb_deletes,asset_unloads,shot_frees,player_deletes;
static uint32_t bomb_vtable[5];
static void *TC fake_delete_bomb(void *bomb,int release) {
    CHECK(bomb==guest_bomb && release==1 && PTR(0x4cf410)==current_guest);
    CHECK(U32(0x4cccf4)==1 && PTR(0x4cf2b8)==guest_bomb);
    free((void *)(uintptr_t)FIELD(bomb,8));free((void *)(uintptr_t)FIELD(bomb,12));free(bomb);
    PTR(0x4cf2b8)=NULL;++bomb_deletes;return NULL;
}
static void TC fake_unload_guest_anm(void *manager,int slot) {
    CHECK(manager==test_anm && slot==18);CHECK(FIELD(manager,0x3120750)==(uint32_t)(uintptr_t)test_host_file);
    FIELD(manager,0x312072c+18*4)=0;++asset_unloads;
}
static void fake_free_shot(void *shot) {CHECK(shot!=NULL);free(shot);++shot_frees;}
static void *TC fake_destroy_player(void *player,int unused) {
    (void)unused;++player_deletes;
    if(player==current_guest) {
        CHECK(bomb_deletes==1 && guest_bomb==NULL && U32(0x4cccf4)==1);
        CHECK(FIELD(test_anm,0x3120750)==(uint32_t)(uintptr_t)test_guest_file);
        PTR(0x570920)=(void *)(uintptr_t)FIELD(player,0x47940);
        free((void *)(uintptr_t)FIELD(player,4));free((void *)(uintptr_t)FIELD(player,8));
    } else {
        CHECK(player==p1 && PTR(0x4cf2b8)==test_host_bomb && U32(0x4cccf4)==0);
        CHECK(FIELD(test_anm,0x3120750)==(uint32_t)(uintptr_t)test_host_file);
    }
    PTR(0x4cf410)=NULL;free(player);return NULL;
}
static void * __stdcall fake_new_node(void *callback) {
    void *node=calloc(1,0x30);CHECK(node!=NULL);FIELD(node,8)=(uint32_t)(uintptr_t)callback;return node;
}
static int __stdcall fake_add_tick(void *node,int priority) {(void)node;CHECK(priority==0x16);return 1;}
static int __stdcall fake_add_draw(void *node,int priority) {(void)node;CHECK(priority==0x61 || priority==0x33);return 1;}
static int TC fake_player_tick(void *player) {(void)player;return 1;}
static int TC fake_bomb_tick(void *bomb) {
    unsigned id=bomb==guest_bomb;
    CHECK(PTR(0x4cf410)==ship(id));CHECK(PTR(0x4cf2b8)==bomb);
    if(id) {CHECK(U32(0x4cccf4)==1);CHECK(PTR(0x4cf298)==guest_cards);}
    return 19;
}
static unsigned asset_starts;
static void *TC fake_load_anm(void *manager,int slot,const char *name) {
    CHECK(manager==test_anm);++asset_starts;
    CHECK(slot==18 && !strcmp(name,"pl01.anm"));
    FIELD(test_anm,0x312072c+18*4)=(uint32_t)(uintptr_t)test_guest_file;
    return test_guest_file;
}
static void *fake_successful_player_create(void) {
    CHECK(U32(0x4cccf4)==1 && U32(0x4cccf8)==0);CHECK(PTR(0x570920)==NULL);
    CHECK(FIELD(test_anm,0x3120750)==(uint32_t)(uintptr_t)test_guest_file);
    void *player=calloc(1,0x479d0),*shot=calloc(1,0x40);CHECK(player && shot);
    FIELD(shot,0x28)=500;FIELD(shot,0x24)=100;FIELD(shot,0x20)=4;
    FIELD(player,0x47940)=(uint32_t)(uintptr_t)shot;
    FIELD(player,4)=(uint32_t)(uintptr_t)fake_new_node(fake_player_tick);
    FIELD(player,8)=(uint32_t)(uintptr_t)fake_new_node(fake_native_draw);
    FIELD(player,0xc)=(uint32_t)(uintptr_t)test_guest_file;
    FIELD(player,0x476ac)=1;PTR(0x4cf410)=player;
    U32(0x4ccd3c)=400;U32(0x4ccd40)=100;return player;
}
static void *fake_bomb_factory(void) {
    ++factory_calls;CHECK(U32(0x4cccf4)==1 && PTR(0x4cf410)==current_guest);
    void *bomb=calloc(1,0xa8);CHECK(bomb!=NULL);
    bomb_vtable[0]=(uint32_t)(uintptr_t)fake_delete_bomb;FIELD(bomb,0)=(uint32_t)(uintptr_t)bomb_vtable;
    FIELD(bomb,8)=(uint32_t)(uintptr_t)fake_new_node(fake_bomb_tick);
    FIELD(bomb,12)=(uint32_t)(uintptr_t)fake_new_node(fake_bomb_tick);
    FIELD((void *)(uintptr_t)FIELD(bomb,8),0x24)=FIELD((void *)(uintptr_t)FIELD(bomb,12),0x24)=(uint32_t)(uintptr_t)bomb;
    PTR(0x4cf2b8)=bomb;return bomb;
}
static void test_independent_loadout_creation(void) {
    reset_context();free(current_guest);current_guest=NULL;
    test_anm=calloc(1,0x3121000);test_host_file=calloc(1,0x13c);test_guest_file=calloc(1,0x13c);
    CHECK(test_anm && test_host_file && test_guest_file);
    FIELD(test_host_file,0)=9;FIELD(test_guest_file,0)=18;
    PTR(0x51f65c)=test_anm;FIELD(test_anm,0x3120750)=(uint32_t)(uintptr_t)test_host_file;
    strcpy((char *)0x4b8000,"pl01.anm");PTR(0x4b709c)=(void *)0x4b8000;
    U32(0x4cccf4)=0;U32(0x4cccf8)=0;U32(0x4cccdc)=1;U32(0x4cccc8)=0;PTR(0x4cf2a4)=NULL;
    PTR(0x570920)=(void *)0x4444;have_guest_resources=0;initial_cards_cloned=0;
    memcpy(run_base_resources,(void *)0x4ccd38,sizeof(run_base_resources));
    *(int32_t *)run_base_resources=0;*(int32_t *)(run_base_resources+0x10)=2;*(int32_t *)(run_base_resources+0x20)=3;
    have_run_base=1;run_base_funds=25;chosen_character=1;custom_cards=1;
    chosen_cards[0]=10;chosen_cards[1]=11;chosen_cards[2]=12;chosen_count=3;expected_add_mode=1;
    test_host_bomb=calloc(1,0xa8);CHECK(test_host_bomb!=NULL);
    FIELD(test_host_bomb,8)=(uint32_t)(uintptr_t)fake_new_node(fake_bomb_tick);
    FIELD(test_host_bomb,12)=(uint32_t)(uintptr_t)fake_new_node(fake_bomb_tick);PTR(0x4cf2b8)=test_host_bomb;
    FIELD((void *)(uintptr_t)FIELD(test_host_bomb,8),0x24)=FIELD((void *)(uintptr_t)FIELD(test_host_bomb,12),0x24)=(uint32_t)(uintptr_t)test_host_bomb;
    stub(0x4867e0,fake_load_anm);stub(0x45af20,fake_successful_player_create);
    stub(0x45abb0,fake_revive_reset);stub(0x411460,fake_add_card);
    stub(0x41fd40,fake_bomb_factory);stub(0x401530,fake_new_node);
    stub(0x401180,fake_add_tick);stub(0x401230,fake_add_draw);
    expected_revived=1;factory_calls=asset_starts=0;wcscpy(log_path,L"tests/bin/context-diagnostic.log");
    void *old_cards=guest_cards;
    create_guest();CHECK(!current_guest && requested && asset_starts==1 && FIELD(test_guest_file,0x128)==1);
    CHECK(guest_cards==old_cards && !have_guest_resources);
    CHECK(PTR(0x4cf410)==p1 && PTR(0x4cf298)==host_cards && U32(0x4cccf4)==0);
    create_guest();CHECK(!current_guest && asset_starts==1 && !factory_calls && guest_cards==old_cards);
    /* The real frame loop completes GPU loading before a later update. */
    FIELD(test_guest_file,0x128)=0;create_guest();
    CHECK(current_guest && p2_character==1 && guest_anm==test_guest_file);
    CHECK(FIELD(current_guest,0xc)==(uint32_t)(uintptr_t)test_guest_file);
    CHECK(FIELD(test_anm,0x3120750)==(uint32_t)(uintptr_t)test_host_file && U32(0x4cccf4)==0);
    CHECK(PTR(0x4cf410)==p1 && PTR(0x4cf298)==host_cards && PTR(0x570920)==(void *)0x4444);
    CHECK(*resource(1,0)==400 && *resource(1,0x10)==5 && p2_funds==325);
    CHECK(*resource(0,0)==100 && *resource(0,0x10)==2 && U32(0x4ccd34)==300);
    CHECK(FIELD(guest_cards,0x28)==3 && FIELD(host_cards,0x28)==0);
    CHECK(factory_calls==1 && guest_bomb && guest_bomb!=test_host_bomb && PTR(0x4cf2b8)==test_host_bomb);
    CHECK(shared_bomb_tick(guest_bomb)==19);CHECK(PTR(0x4cf2b8)==test_host_bomb && U32(0x4cccf4)==0);
    CHECK(shared_bomb_tick(test_host_bomb)==19);
    enter_p2(0);enter_p2(0);CHECK(PTR(0x4cf2b8)==guest_bomb && U32(0x4cccf4)==1);
    leave_p2(0);leave_p2(0);CHECK(PTR(0x4cf2b8)==test_host_bomb && U32(0x4cccf4)==0);
    stub(0x45b020,fake_destroy_player);stub(0x486fc0,fake_unload_guest_anm);stub(0x491a3f,fake_free_shot);
    bomb_deletes=asset_unloads=shot_frees=player_deletes=0;
    U32(0x4cccc8)=1;U32(0x4cccdc)=2;observed_stage=1;partner[1].ghost=1;
    destroy_stage_player(p1,0);
    CHECK(!p1 && !current_guest && !guest_anm && !guest_bomb);
    CHECK(bomb_deletes==1 && asset_unloads==1 && shot_frees==1 && player_deletes==2);
    CHECK(departed_stage==1 && departed_ghosts==2);
    /* Do not leave mock asset pointers available to later tests. */
    PTR(0x51f65c)=NULL;guest_anm=NULL;PTR(0x4cf2b8)=NULL;guest_bomb=NULL;
    puts("PASS different character ANM/SHT, own starting rewards/inventory, original host restoration and native P2 Bomb context");
}
static void *fake_empty_host_create(void) {return NULL;}
static void test_initial_bomb_standard(void) {
    reset_context();player_create_original=fake_empty_host_create;creating_guest=0;
    have_guest_resources=0;U32(0x4cccdc)=1;
    U32(0x4ccd60)=3;U32(0x4ccd58)=4; /* Preserve an extra starting reward. */
    CHECK(!create_host());CHECK(U32(0x4ccd60)==2 && U32(0x4ccd58)==3);
    CHECK(*(uint32_t *)(run_base_resources+0x28)==2 && *(uint32_t *)(run_base_resources+0x20)==3);
    have_guest_resources=1;last_stage=1;U32(0x4cccdc)=2;
    CHECK(!create_host());CHECK(U32(0x4ccd60)==2 && U32(0x4ccd58)==3);
    have_guest_resources=0;U32(0x4cccdc)=1;U32(0x4ccd60)=1;U32(0x4ccd58)=1;
    CHECK(!create_host());CHECK(U32(0x4ccd60)==1 && U32(0x4ccd58)==1);
    puts("PASS initial Bomb base reduced once, starting bonus preserved, stage carry and minimum one");
}
static void test_pending_stage_assets(void) {
    reset_context();free(current_guest);current_guest=NULL;
    test_anm=calloc(1,0x3121000);test_guest_file=calloc(1,0x13c);CHECK(test_anm && test_guest_file);
    PTR(0x51f65c)=test_anm;guest_anm=test_guest_file;FIELD(guest_anm,0x128)=1;
    U32(0x4cccf4)=0;p2_character=1;chosen_character=3;U32(0x4cccdc)=2;U32(0x4cccc8)=0;PTR(0x4cf2a4)=NULL;
    last_stage=1;have_guest_resources=1;FIELD(guest_cards,0x28)=5;void *inventory=guest_cards;
    create_guest();create_guest();
    CHECK(!current_guest && requested && last_stage==1 && have_guest_resources);
    CHECK(guest_cards==inventory && FIELD(guest_cards,0x28)==5 && p2_funds==400 && *(int *)p2_resources==200);
    CHECK(PTR(0x4cf298)==host_cards && U32(0x4ccd34)==300 && U32(0x4cccf4)==0);
    free(test_anm);free(test_guest_file);guest_anm=NULL;PTR(0x51f65c)=NULL;
    puts("PASS pending cross-stage ANM preserves prior stage, inventory and resource carry");
}
static unsigned native_menu_calls,menu_cleanup_calls,menu_commits;
static unsigned menu_action;
static void TC fake_menu_phase(void *menu,int phase) {
    FIELD(menu,0x1c)=FIELD(menu,0x18);FIELD(menu,0x18)=phase;
    FIELD(menu,0x20)=FIELD(menu,0x388)=0;
}
static int TC fake_menu_cleanup(void *menu) {
    CHECK(FIELD(menu,0x20)==4 && FIELD(menu,0x388)==6);
    CHECK(PTR(0x4cf298)==host_cards && !draft_owner);++menu_cleanup_calls;
    PTR(0x4cf2a0)=NULL;FIELD(menu,0x24)=0;FIELD(menu,0xb0)=0;
    fake_menu_phase(menu,5);return 1;
}
static int TC fake_native_menu_tick(void *menu) {
    ++native_menu_calls;
    if(menu_action==1) {
        CHECK(draft_owner==2 && PTR(0x4cf298)==guest_cards);
        U32(0x4cccf4)=FIELD(menu,0x24);U32(0x4cd5e8)=7;
        FIELD(menu,0x18)=2;PTR(0x4cf2a0)=NULL;++menu_commits;
    } else if(menu_action==2) {
        CHECK(PTR(0x4cf298)==guest_cards);fake_menu_phase(menu,5);PTR(0x4cf2a0)=NULL;
    }
    return 29;
}
static int TC fake_native_menu_draw(void *menu) {(void)menu;return 37;}
static void TC fake_native_input(void *input) {
    FIELD(input,0xc)=FIELD(input,0)&~FIELD(input,4);
    FIELD(input,0x10)=FIELD(input,4)&~FIELD(input,0);
}
static int TC fake_draft_cards(void *manager) {
    CHECK(draft_owner==2 && manager==guest_cards && PTR(0x4cf298)==guest_cards);return 43;
}
static void *TC fake_menu_manager_destroy(void *manager,int unused) {
    CHECK(manager==draft_host && !guest_cards && !draft_owner && unused==0);
    CHECK(PTR(0x4cf298)==manager && U32(0x4cccf4)==draft_character);return manager;
}
static void test_native_sequential_selection(void) {
    reset_context();free(p1);free(current_guest);p1=current_guest=NULL;PTR(0x4cf410)=NULL;
    void *menu=calloc(1,0x5d90),*score=calloc(1,0x60000);CHECK(menu && score);
    PTR(0x4cf43c)=menu;PTR(0x4cf41c)=score;U32(0x4cf438)=1;U32(0x4cccc8)=0;
    U32(0x4ccd00)=1;U32(0x4cd5e8)=0;U32(0x4cccf8)=0;
    FIELD(host_cards,4)=(uint32_t)(uintptr_t)fake_new_node(fake_player_tick);
    FIELD(host_cards,8)=(uint32_t)(uintptr_t)fake_new_node(fake_native_draw);
    void *ht=(void *)(uintptr_t)FIELD(host_cards,4),*hd=(void *)(uintptr_t)FIELD(host_cards,8);
    FIELD(ht,4)=FIELD(hd,4)=6;
    FIELD(menu,0x18)=6;FIELD(menu,0x20)=3;FIELD(menu,0x24)=0;FIELD(menu,0x2c)=4;FIELD(menu,0xb0)=2;
    FIELD(score,0x5f678)=2;*OFF(score,0x5f608)=8;*OFF(score,0x5f609)=9;
    *OFF(score,0x5f60a)=55;*OFF(score,0x5f639)=1;
    stub(0x4646e0,fake_menu_phase);stub(0x466740,fake_menu_cleanup);
    stub(0x401530,fake_new_node);stub(0x401180,fake_add_tick);stub(0x401230,fake_add_draw);
    stub(0x4015a0,fake_remove);stub(0x408a90,fake_draft_cards);stub(0x408ab0,fake_draft_cards);
    menu_tick_original=fake_native_menu_tick;menu_draw_original=fake_native_menu_draw;menu_input_original=fake_native_input;
    native_menu_calls=menu_cleanup_calls=menu_commits=menu_action=0;
    CHECK(native_menu_tick(menu)==1);CHECK(menu_cleanup_calls==1 && native_menu_calls==0);
    CHECK(draft_owner==2 && PTR(0x4cf298)==guest_cards && !draft_ready);
    CHECK(FIELD(menu,0x18)==6 && FIELD(menu,0x20)==0 && FIELD(menu,0x24)==1 && FIELD(menu,0xb0)==2);
    CHECK(FIELD(score,0x5f678)==2);CHECK(!(FIELD(ht,4)&2) && !(FIELD(hd,4)&2));
    CHECK(guest_cards_tick(guest_cards)==43 && guest_cards_draw(guest_cards)==43);
    CHECK(native_menu_draw(menu)==37 && menu_banner==2);
    /* A held confirmation cannot cross the handoff. Release is required. */
    draft_menu_keys((void *)0x4ca210,1);CHECK(!U32(0x4ca210) && draft_release);
    draft_menu_keys((void *)0x4ca210,0);CHECK(!draft_release);
    draft_menu_keys((void *)0x4ca210,0x80);fake_native_input((void *)0x4ca210);
    CHECK(U32(0x4ca21c)==0x80);draft_menu_keys((void *)0x4ca210,0x80);fake_native_input((void *)0x4ca210);CHECK(!U32(0x4ca21c));
    /* P2 can change both native cards, with its own temporary resources. */
    *OFF(score,0x5f608)=10;*OFF(score,0x5f609)=11;FIELD(menu,0x24)=3;
    U32(0x4ccd34)=999;U32(0x4ccd48)=99;PTR(0x4cf2d4)=(void *)0x9876;
    menu_action=1;CHECK(native_menu_tick(menu)==29);
    CHECK(menu_commits==1 && !draft_owner && draft_ready && requested && wanted);
    CHECK(chosen_character==3 && chosen_count==2 && chosen_cards[0]==10 && chosen_cards[1]==11 && chosen_cards[2]==-1);
    CHECK(PTR(0x4cf298)==host_cards && U32(0x4cccf4)==0 && U32(0x4cf42c)==0);
    CHECK(U32(0x4ccd34)==300 && U32(0x4ccd48)==2 && PTR(0x4cf2d4)==(void *)0x1111);
    CHECK(*OFF(score,0x5f608)==8 && *OFF(score,0x5f609)==9 && *OFF(score,0x5f60a)==55 && *OFF(score,0x5f639)==1);
    CHECK(FIELD(ht,4)==6 && FIELD(hd,4)==6 && FIELD(menu,0xb0)==3 && FIELD(menu,0x38)==0);
    /* A second selection can be cancelled back to P1 without starting. */
    menu_action=0;U32(0x4cd5e8)=0;FIELD(menu,0x18)=6;FIELD(menu,0x20)=3;FIELD(menu,0x24)=2;FIELD(menu,0xb0)=2;
    CHECK(native_menu_tick(menu)==1 && draft_owner==2);
    *OFF(score,0x5f608)=27;U32(0x4ccd34)=456;
    menu_action=2;CHECK(native_menu_tick(menu)==29);
    CHECK(!draft_owner && !guest_cards && PTR(0x4cf298)==host_cards);
    CHECK(FIELD(menu,0x18)==6 && FIELD(menu,0x20)==0 && FIELD(menu,0x24)==2 && FIELD(menu,0xb0)==2);
    CHECK(*OFF(score,0x5f608)==8 && U32(0x4ccd34)==300 && U32(0x4cd5e8)==0);
    CHECK(native_menu_draw(menu)==37 && menu_banner==1);
    /* A legitimately unlocked third native slot remains usable. */
    FIELD(score,0x5f678)=3;FIELD(menu,0x20)=3;menu_action=0;
    CHECK(native_menu_tick(menu)==1 && draft_owner==2);
    *OFF(score,0x5f608)=10;*OFF(score,0x5f609)=11;*OFF(score,0x5f60a)=12;
    FIELD(menu,0x24)=1;menu_action=1;CHECK(native_menu_tick(menu)==29 && draft_ready);
    CHECK(chosen_count==3 && chosen_cards[2]==12 && FIELD(score,0x5f678)==3 && *OFF(score,0x5f60a)==55);
    /* One native slot must stay one; no unlock or slot promotion for P2. */
    menu_action=0;U32(0x4cd5e8)=0;FIELD(menu,0x18)=6;FIELD(menu,0x20)=3;
    FIELD(score,0x5f678)=1;unsigned char current_templates[0x8c];
    memcpy(current_templates,OFF(score,0x5f608),sizeof(current_templates));
    CHECK(native_menu_tick(menu)==1 && draft_owner==2);
    *OFF(score,0x5f608)=15;*OFF(score,0x5f609)=16;FIELD(menu,0x24)=2;
    menu_action=1;CHECK(native_menu_tick(menu)==29);
    CHECK(chosen_count==1 && chosen_cards[0]==15 && chosen_cards[1]==-1 && chosen_cards[2]==-1);
    CHECK(!memcmp(current_templates,OFF(score,0x5f608),sizeof(current_templates)));
    /* Replay/practice and retry-return origins use their original flow. */
    menu_action=0;FIELD(menu,0x20)=3;U32(0x4cccc8)=0x40;
    CHECK(native_menu_tick(menu)==29 && !draft_owner);
    U32(0x4cccc8)=0;U32(0x4cf438)=4;CHECK(native_menu_tick(menu)==29 && !draft_owner);
    U32(0x4cf438)=1;native_draft=0;CHECK(native_menu_tick(menu)==29 && !draft_owner);native_draft=1;
    FIELD(menu,0x18)=6;FIELD(menu,0x20)=3;menu_action=0;U32(0x4cd5e8)=0;
    CHECK(native_menu_tick(menu)==1 && draft_owner==2);
    void *original_host=host_cards;cards_destroy_original=fake_menu_manager_destroy;
    CHECK(destroy_cards(guest_cards,0)==original_host);host_cards=original_host;
    PTR(0x4cf43c)=PTR(0x4cf41c)=NULL;free(menu);free(score);
    free(ht);free(hd);FIELD(host_cards,4)=FIELD(host_cards,8)=0;
    puts("PASS native P1 -> P2 character/card screens, native slot limits, own keyboard, held-confirm guard, cancellation, host presets/resources and replay bypass");
}
static void test_local_profile_guard(void) {
    CHECK(game_target_allowed(L"C:\\Games\\Steam\\steamapps\\common\\th18\\th18.exe"));
    CHECK(!game_target_allowed(L"C:\\Games\\Steam\\steamapps\\common\\th18\\custom.exe"));
    CHECK(!data_path_matches("C:\\Users\\User\\AppData\\Roaming\\ShanghaiAlice\\th18",L"C:\\Steam\\steamapps\\common\\th18\\th18.exe"));
    char utf8[2048],ansi[2048];const wchar_t *unicode_exe=L"F:\\游戏\\th18.exe",*unicode_profile=L"F:\\游戏\\coop-data\\ShanghaiAlice\\th18";
    WideCharToMultiByte(CP_UTF8,0,unicode_profile,-1,utf8,sizeof(utf8),NULL,NULL);CHECK(data_path_matches(utf8,unicode_exe) && data_profile_encoding==CP_UTF8);
    WideCharToMultiByte(CP_ACP,0,unicode_profile,-1,ansi,sizeof(ansi),NULL,NULL);CHECK(data_path_matches(ansi,unicode_exe));
    strcat(utf8,"-other");CHECK(!data_path_matches(utf8,unicode_exe));
    const wchar_t *exe=L"F:\\Game\\th18.exe";
    CHECK(data_path_matches("F:\\Game\\coop-data\\ShanghaiAlice\\th18\\",exe));
    CHECK(data_path_matches("f:\\game\\coop-data\\ShanghaiAlice\\th18",exe));
    CHECK(!data_path_matches("F:\\Game\\coop-data\\ShanghaiAlice\\th18-other",exe));
    CHECK(!data_path_matches("C:\\Users\\User\\AppData\\Roaming\\ShanghaiAlice\\th18",exe));
    char bad[4096];memset(bad,'A',sizeof(bad));CHECK(!data_path_matches(bad,exe));
    CHECK(data_path_matches("F:\\Game\\coop-data\\sessions\\0123456789abcdef0123456789abcdef\\host\\ShanghaiAlice\\th18",exe));
    CHECK(data_path_matches("F:\\Game\\coop-data\\sessions\\0123456789abcdef0123456789abcdef\\guest\\ShanghaiAlice\\th18\\",exe));
    CHECK(!data_path_matches("F:\\Game\\coop-data\\sessions\\..\\host\\ShanghaiAlice\\th18",exe));
    CHECK(!data_path_matches("F:\\Game\\coop-data\\sessions\\0123456789abcdef0123456789abcdef\\other\\ShanghaiAlice\\th18",exe));
    puts("PASS native loader/DLL private-profile guard rejects ordinary, sibling and unterminated paths");
}
static void test_frame_input(const char *fixture_path) {
    reset_context();memset(&input_latch,0,sizeof(input_latch));
    coop_input_capture(&input_latch,0x80001,0xffffffff);
    CHECK(input_latch.sequence==0 && input_latch.p1==0x80001);
    CHECK(p2_input()==COOP_P2_INPUT_MASK && p2_input()==COOP_P2_INPUT_MASK);
    /* Every consumer sees the same value even if it reads more than once. */
    enter_p2(1);CHECK(U32(0x4ca428)==COOP_P2_INPUT_MASK);leave_p2(1);
    CHECK(p2_input()==COOP_P2_INPUT_MASK);
    coop_input_capture(&input_latch,0,0);CHECK(input_latch.sequence==1 && !p2_input());
    CoopInputRecord r=coop_input_record(&input_latch,2,6,2,0);
    CHECK(r.sequence==1 && r.stage==2 && r.phase==6 && r.owner==2);
    CHECK(r.checksum==coop_input_checksum(&r));r.p2^=1;CHECK(r.checksum!=coop_input_checksum(&r));
    if(fixture_path) {
        FILE *f=fopen(fixture_path,"wb");CHECK(f!=NULL);
        CoopInputHeader h=coop_input_header(COOP_VERSION);CHECK(fwrite(&h,1,sizeof(h),f)==sizeof(h));
        memset(&input_latch,0,sizeof(input_latch));
        for(unsigned i=0;i<240;++i) {
            coop_input_capture(&input_latch,(i*31)&0xfffff,(i*17)&COOP_P2_INPUT_MASK);
            r=coop_input_record(&input_latch,i/80,i<80?6:UINT32_MAX,i<40?1:i<80?2:0,0);
            CHECK(fwrite(&r,1,sizeof(r),f)==sizeof(r));
        }
        CHECK(!fclose(f));
    }
    memset(&input_latch,0,sizeof(input_latch));
    puts("PASS one-poll dual-input latch, shared P2 consumer state, masks, record integrity and native trace fixture");
}
static int mock_online_loads,mock_online_calls,mock_frame_ready=1,mock_native_result;
static volatile LONG mock_scene_active;
static int mock_scene_count,mock_menu_count,mock_audio_pumps;
static uint32_t mock_scene_funds,mock_scene_rng;static float mock_scene_x;
static int TC mock_online_audio(void *queue) {
    CHECK(queue==(void *)0x56ad80);
    if(U32(0x56d204)){U32(0x56d204)=0;++mock_audio_pumps;}return 0;
}
static DWORD WINAPI mock_scene_worker(void *unused) {
    int menu_load=unused!=NULL;DWORD started=GetTickCount();
    void *manager=PTR(0x51f65c),*asset=(void *)(uintptr_t)FIELD(manager,0x312072c);
    while(FIELD(asset,0x128) && GetTickCount()-started<5000)Sleep(1);
    if(FIELD(asset,0x128))return 31; /* Needs main-thread GPU service. */
    U32(0x56d204)=1;
    while(U32(0x56d204) && GetTickCount()-started<5000)Sleep(1);
    if(U32(0x56d204))return 32; /* Needs native audio service, not scene ticks. */
    Sleep(online_map->role?100:15);
    if(!menu_load){U32(0x4cd894)=0;U32(0x4cd890)=1;} /* Scene flags are published before returning. */
    Sleep(online_map->role?50:5); /* A busy flag alone is not a completion barrier. */
    U32(0x4ccd34)=mock_scene_funds+10;FLT(p1,0x620)=mock_scene_x+3;
    U32(0x4cf288)=(mock_scene_rng*25173+13849)&0xffff;
    if(menu_load) {
        void *menu=PTR(0x4cf43c);
        FIELD(menu,0x18)=1;FIELD(menu,0x20)=FIELD(menu,0x24)=FIELD(menu,0xb0)=FIELD(menu,0x388)=0;
        /* Native menu job +5d8c stays 1 even after successful completion. */
    }
    InterlockedExchange(&mock_scene_active,0);return 0;
}
static void mock_start_scene(int menu_load) {
    void *menu=PTR(0x4cf43c);
    HANDLE prior=menu_load?(HANDLE)(uintptr_t)FIELD(menu,0x5d80):(HANDLE)PTR(0x4cd888);DWORD code;
    if(prior){CHECK(GetExitCodeThread(prior,&code) && code==0);CHECK(CloseHandle(prior));}
    if(menu_load)++mock_menu_count;else ++mock_scene_count;
    mock_scene_funds=U32(0x4ccd34);mock_scene_x=FLT(p1,0x620);mock_scene_rng=U32(0x4cf288);
    U32(0x4ccd34)=700+online_map->role;FLT(p1,0x620)=900+(float)online_map->role;
    U32(0x4cf288)=600+online_map->role;
    if(menu_load) {
        FIELD(menu,0x5d8c)=1;FIELD(menu,0x18)=online_map->role;
        FIELD(menu,0x20)=FIELD(menu,0x24)=FIELD(menu,0xb0)=FIELD(menu,0x388)=800+online_map->role;
    } else {U32(0x4cd894)=1;U32(0x4cd890)=0;}
    FIELD((void *)(uintptr_t)FIELD(PTR(0x51f65c),0x312072c),0x128)=1;
    InterlockedExchange(&mock_scene_active,1);
    HANDLE next=CreateThread(NULL,0,mock_scene_worker,menu_load?(void *)1:NULL,0,NULL);CHECK(next!=NULL);
    if(menu_load)FIELD(menu,0x5d80)=(uint32_t)(uintptr_t)next;else PTR(0x4cd888)=next;
}
static uint32_t mock_online_poll(void) {
    uint32_t step=online_map->step,raw=(step*(online_map->role==0?31:online_map->role==1?17:43))&0xcfb;
    U32(0x4ca210)=raw;native_menu_input((void *)0x4ca210);return raw;
}
static int TC mock_online_load(void *manager,void *file) {
    (void)manager;if(--mock_online_loads<=0){if(online_map->role)Sleep(65);FIELD(file,0x128)=0;}return 1;
}
static void TC mock_online_normalize(void *menu,int phase) {fake_menu_phase(menu,phase);mock_frame_ready=0;}
static int TC mock_online_tick(void *game) {
    CHECK(!mock_scene_active); /* No scene callbacks may run during partial construction. */
    (void)game;mock_frame_ready=0;++mock_online_calls;mock_online_poll();
    void *menu=PTR(0x4cf43c);uint32_t step=online_map->step;
    if(step==0)CHECK(FIELD(menu,0x5c4c)==0x101); /* Pending title BGM and unrelated bits survive admission. */
    FIELD(menu,0x18)=step<20?1:step<60?6:step<100?5:1;
    draft_owner=step>=40 && step<60?2:0;
    U32(0x4cccdc)=step/30;
    if(step!=90) {
        FLT(p1,0x620)+=(float)((online_keys[0]&0x80)!=0)-(float)((online_keys[0]&0x40)!=0);
        FLT(current_guest,0x620)+=(float)((online_keys[1]&0x80)!=0)-(float)((online_keys[1]&0x40)!=0);
        U32(0x4ccd34)+=online_keys[0]&1;p2_funds+=online_keys[1]&1;
        if(party_count()==3){FLT(ship(2),0x620)+=(float)((online_keys[2]&0x80)!=0)-(float)((online_keys[2]&0x40)!=0);guests[1].funds+=online_keys[2]&1;}
        U32(0x4cf288)=(U32(0x4cf288)*25173+13849)&0xffff;++U32(0x4cf28c);
    }
    if(step==7 && online_map->role)Sleep(45); /* The peer must render a complete frame while waiting. */
    mock_frame_ready=1;mock_native_result=step%2?37:1;
    if(step==30 || step==80 || step==100)mock_start_scene(step==100);
    return mock_native_result;
}
static int TC mock_online_shutdown(void *scheduler) {(void)scheduler;++mock_online_calls;return mock_native_result;}
static void test_online_idle_contract(void) {
    CoopOnlineMap idle={0},*previous_map=online_map;void *previous_scheduler=PTR(0x4cf294),*previous_tick=online_tick_original;
    unsigned char scheduler[0x58]={0};online_map=&idle;PTR(0x4cf294)=scheduler;
    int calls=mock_online_calls;
    online_inside_scheduler=1;CHECK(online_tick(scheduler)==1);online_inside_scheduler=0;
    idle.status=4;CHECK(online_tick(scheduler)==1);
    idle.status=5;CHECK(online_tick(scheduler)==1);
    CHECK(mock_online_calls==calls);
    /* Shutdown dispatch (+0x54) runs cleanup callbacks even after stop.
       Preserve actual native exit / error returns instead of masking them. */
    online_tick_original=mock_online_shutdown;FIELD(scheduler,0x54)=1;
    mock_native_result=0;CHECK(online_tick(scheduler)==0);
    mock_native_result=-1;CHECK(online_tick(scheduler)==-1);
    mock_native_result=37;CHECK(online_tick(scheduler)==37);
    CHECK(mock_online_calls==calls+3);mock_online_calls=calls;
    online_map=previous_map;PTR(0x4cf294)=previous_scheduler;online_tick_original=previous_tick;
    puts("PASS scheduler waits remain alive; native shutdown bypasses network gating and preserves exit results");
}
static void __cdecl mock_empty_poll(void) { }
static void test_online_post_fallthrough(void) {
    CoopOnlineMap reply={0},*previous_map=online_map;
    void *previous_scheduler=PTR(0x4cf294);
    unsigned char scheduler[0x58]={0},link[8]={0},callback[0x28]={0};
    FIELD(scheduler,0x18)=(uint32_t)(uintptr_t)link;FIELD(link,0)=(uint32_t)(uintptr_t)callback;FIELD(callback,8)=0x453460;
    PTR(0x4cf294)=scheduler;online_map=&reply;reply.reserved[0]=1;
    reply.status=3;reply.kind=2;reply.step=10;online_step=10;online_phase=online_pending=online_started=1;
    online_request=CreateEventW(NULL,FALSE,FALSE,NULL);online_reply=CreateEventW(NULL,FALSE,FALSE,NULL);
    CHECK(online_request && online_reply);stub(0x401c50,mock_empty_poll);
    CHECK(online_tick(scheduler)==1);
    CHECK(reply.completed==11 && reply.step==11 && reply.kind==1 && reply.status==2);
    CHECK(online_phase==0 && online_pending); /* Next pre request was sent in this same presentation. */
    CHECK(WaitForSingleObject(online_request,0)==WAIT_OBJECT_0); /* ACK/new request notification is live. */
    CHECK(CloseHandle(online_request) && CloseHandle(online_reply));online_request=online_reply=NULL;
    PTR(0x4cf294)=previous_scheduler;online_map=previous_map;
    online_step=online_phase=online_pending=online_started=0;
    puts("PASS ready post ACK admits the next pre request without an empty presentation; no native tick without reply");
}
static int run_online_fixture(void) {
    reset_context();PTR(0x4cf2d4)=NULL;guest_mukade=NULL;
    void *menu=calloc(1,0x5d90),*manager=calloc(1,0x3120810),*asset=calloc(1,0x130);
    void *scheduler=calloc(1,0x58),*link=calloc(1,8),*callback=calloc(1,0x28);
    CHECK(menu && manager && asset && scheduler && link && callback);PTR(0x4cf43c)=menu;FIELD(menu,0x18)=1;FIELD(menu,0x5c4c)=0x101;
    PTR(0x4cf294)=scheduler;FIELD(scheduler,0x18)=(uint32_t)(uintptr_t)link;
    FIELD(link,0)=(uint32_t)(uintptr_t)callback;FIELD(callback,8)=0x453460;
    PTR(0x51f65c)=manager;FIELD(manager,0x312072c)=(uint32_t)(uintptr_t)asset;FIELD(asset,0x128)=1;
    stub(0x401c50,mock_online_poll);stub(0x4646e0,mock_online_normalize);stub(0x486920,mock_online_load);stub(0x476d20,mock_online_audio);
    menu_input_original=fake_native_input;online_tick_original=mock_online_tick;
    CHECK(online_initialise()==1);mock_online_loads=online_map->role?7:3;
    if(party_count()==3) {
        guests[1].player=calloc(1,0x479d0);CHECK(guests[1].player!=NULL);
        FIELD(ship(2),0x476ac)=1;guests[1].funds=500;*resource(2,0)=200;*resource(2,4)=400;*resource(2,0x10)=2;
    }
    if(online_map->reserved[1]&1) {
        wchar_t mutex_name[128];CHECK(GetEnvironmentVariableW(L"TH18_COOP_TEST_MUTEX",mutex_name,128)>0);
        HANDLE own=CreateMutexW(NULL,TRUE,mutex_name);CHECK(own && GetLastError()!=ERROR_ALREADY_EXISTS);
        PTR(0x570c98)=own;
    }
    DWORD started=GetTickCount();LARGE_INTEGER steady_start={0},steady_end,frequency;
    CHECK(QueryPerformanceFrequency(&frequency));
    while(online_step<120) {
        if(online_map->status==4 || online_map->status==5 || GetTickCount()-started>30000) {
            fprintf(stderr,"ONLINE_NATIVE_FAIL role=%lu step=%lu status=%ld error=%lu\n",(unsigned long)online_map->role,(unsigned long)online_step,
                online_map->status,(unsigned long)online_map->error);return 7;
        }
        if(online_step>=91 && online_step<96)SetEvent(online_reply); /* Stale notification must not impersonate a reply. */
        int before=mock_online_calls,result=online_tick(scheduler);
        CHECK(mock_online_calls-before<=1); /* Fallthrough must never tick twice per presentation. */
        CHECK(mock_frame_ready); /* Renderer never sees an unfinished callback chain. */
        if(before!=mock_online_calls)CHECK(result==mock_native_result);
        else CHECK(result==1);
        /* Real callers at 0x471b58 / 0x472e9c / 0x472ff5 terminate
           on 0 and -1. A wait must keep the application alive. */
        CHECK(result!=0 && result!=-1);
        if(online_step==25 && online_map->role && GetEnvironmentVariableW(L"TH18_COOP_TEST_DIVERGE",NULL,0))++p2_funds;
        if(online_step==110 && !steady_start.QuadPart)QueryPerformanceCounter(&steady_start);
        Sleep(0); /* No artificial timer-resolution pacing in a transport benchmark. */
    }
    QueryPerformanceCounter(&steady_end);
    CHECK(mock_online_calls==120);
    CHECK(mock_scene_count==2 && mock_menu_count==1 && mock_audio_pumps==3 && !mock_scene_active);
    DWORD worker_code;CHECK(GetExitCodeThread((HANDLE)PTR(0x4cd888),&worker_code) && worker_code==0);
    CHECK(CloseHandle((HANDLE)PTR(0x4cd888)));PTR(0x4cd888)=NULL;
    CHECK(GetExitCodeThread((HANDLE)(uintptr_t)FIELD(menu,0x5d80),&worker_code) && worker_code==0);
    CHECK(CloseHandle((HANDLE)(uintptr_t)FIELD(menu,0x5d80)));FIELD(menu,0x5d80)=0;
    CHECK(FIELD(menu,0x5d8c)==1); /* Do not misread this job flag as busy. */
    if(online_map->reserved[1]&1)CHECK(!PTR(0x570c98) && online_map->reserved[0]==3);
    test_online_idle_contract();
    wchar_t profile[4096];DWORD count=GetEnvironmentVariableW(L"APPDATA",profile,4096);
    if(count && count<3900) {
        wcscat(profile,L"\\ShanghaiAlice\\th18\\scoreth18.dat");
        FILE *file=_wfopen(profile,L"r+b");if(file){fseek(file,23,SEEK_SET);fputc(42,file);fclose(file);}
    }
    printf("ONLINE_NATIVE_PASS role=%lu steps=%lu state=%lu calls=%d atomic=1 async_loads=2 menu_loads=1 steady_frames=10 steady_ms=%.3f\n",(unsigned long)online_map->role,(unsigned long)online_step,
        (unsigned long)online_digest(),mock_online_calls,1000.0*(steady_end.QuadPart-steady_start.QuadPart)/frequency.QuadPart);
    return 0;
}
#include "trio_test.h"
#include "native_heap_test.h"
#include "native_replay_test.h"
static void test_loader_draw_isolation(void) {
    uint32_t flag=U32(0x4cd894);void *old=PTR(0x4cd888);unsigned depth=scope_depth;uint32_t funds=U32(0x4ccd34);void *cards=PTR(0x4cf298);
    HANDLE event=CreateEventW(NULL,TRUE,FALSE,NULL);CHECK(event!=NULL);PTR(0x4cd888)=event;U32(0x4cd894)=1;
    CHECK(scene_construction_pending());CHECK(scene_draw()==1);
    CHECK(guest_cards_draw((void *)1)==1 && guest_draw((void *)1)==1 && shared_bomb_draw((void *)1)==1 && shop_draw((void *)1)==1);
    CHECK(scope_depth==depth && U32(0x4ccd34)==funds && PTR(0x4cf298)==cards);
    U32(0x4cd894)=0;CHECK(scene_construction_pending());CHECK(scene_draw()==1 && guest_draw((void *)1)==1);
    CHECK(SetEvent(event));CHECK(!scene_construction_pending());
    U32(0x4cd894)=1;CHECK(scene_construction_pending());
    PTR(0x4cd888)=old;U32(0x4cd894)=flag;CloseHandle(event);
    puts("PASS scene worker blocks native Draw and guest context swaps until busy flag AND worker completion settle");
}
static void test_delay_mismatch_details(void) {
    CoopOnlineMap *saved_map=online_map,map={0};RbPipe *saved_pipe=rb_pipe;void *saved=malloc(sizeof(dl_live));CHECK(saved!=NULL);memcpy(saved,&dl_live,sizeof(dl_live));
    RbPipe *pipe=calloc(1,sizeof(*pipe));CHECK(pipe!=NULL);online_map=&map;map.players=2;rb_pipe=pipe;
    memset(&dl_live,0,sizeof(dl_live));dl_live.frame=40100;
    for(unsigned id=0;id<3;++id)for(unsigned i=0;i<256;++i)dl_live.hashes[id][i].frame=UINT32_MAX;
    uint32_t a[13]={11,22,33,44,44,55,66,77,4,8,9,10,11},b[13];memcpy(b,a,sizeof(b));b[2]=999;
    CHECK(dl_hash(0,40097,0x176edead,a));a[8]=99; /* Later scene/current state must not replace the historical frame. */
    CHECK(dl_live.hashes[0][40097%256].detail[8]==4);
    CHECK(!dl_hash(1,40097,0xeef8c3dc,b));CHECK(pipe->padding[1]==40097 && pipe->padding2[0]==1);
    CHECK(dl_live.hashes[0][40097%256].detail[2]==33 && dl_live.hashes[1][40097%256].detail[2]==999);
    memset(&dl_live,0,sizeof(dl_live));dl_live.frame=40100;pipe->padding2[0]=0;
    for(unsigned id=0;id<3;++id)for(unsigned i=0;i<256;++i)dl_live.hashes[id][i].frame=UINT32_MAX;
    pipe->status=4;pipe->error=77;CHECK(dl_step(NULL)==1 && pipe->error==77);pipe->status=1;
    void *old_menu=PTR(0x4cf43c),*old_worker=PTR(0x4cd888);uint32_t old_busy=U32(0x4cd894);
    PTR(0x4cf43c)=NULL;PTR(0x4cd888)=NULL;U32(0x4cd894)=0;
    pipe->padding2[1]=1;uint32_t paused_frame=dl_live.frame;
    for(unsigned n=0;n<20;++n)CHECK(dl_step(NULL)==1 && dl_live.frame==paused_frame && !dl_live.ready && pipe->status==1);
    CHECK(!pipe->end_request);pipe->padding2[1]=0;
    PTR(0x4cf43c)=old_menu;PTR(0x4cd888)=old_worker;U32(0x4cd894)=old_busy;
    CHECK(dl_hash(1,40097,123,b));CHECK(dl_hash(0,40097,123,b));CHECK(!pipe->padding2[0]);
    CHECK(dl_hash(0,40098,111,NULL));CHECK(!dl_hash(1,40098,222,NULL)); /* Original strict mismatch check remains. */
    memcpy(&dl_live,saved,sizeof(dl_live));free(saved);free(pipe);online_map=saved_map;rb_pipe=saved_pipe;
    puts("PASS exact delayed mismatch frame and saved groups, equal-digest acceptance and unchanged fail-closed validation");
}
int main(int argc,char **argv) {
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);SetUnhandledExceptionFilter(report_crash);
    setvbuf(stdout,NULL,_IONBF,0);
    void *mock=VirtualAlloc((void *)0x400000,0x180000,MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE);
    if(!mock) {
        MEMORY_BASIC_INFORMATION region={0};VirtualQuery((void *)0x400000,&region,sizeof(region));
        fprintf(stderr,"Mock address range unavailable: error=%lu image=%p allocation=%p state=%lx\n",
            (unsigned long)GetLastError(),GetModuleHandleW(NULL),region.AllocationBase,(unsigned long)region.State);
    }
    CHECK(mock==(void *)0x400000);
    if(argc>1 && !strcmp(argv[1],"--online-fixture"))return run_online_fixture();
    test_loader_draw_isolation();
    test_delay_mismatch_details();
    test_online_idle_contract();
    test_online_post_fallthrough();
    test_scope_roundtrip();test_item_owner();test_shop_scopes();test_initial_rewards();
    test_failed_guest_create();test_stamp_cache();
    test_keyboard_and_menu();test_ghost_and_rescue();test_transfers_and_power_priority();
    test_native_spirit_and_stage_clear();test_independent_loadout_creation();test_pending_stage_assets();test_native_sequential_selection();test_exit_null_close();test_local_profile_guard();
    test_personal_view_and_boss_rules();test_initial_bomb_standard();test_native_active_ui_lifetime();test_no_continue();
    test_frame_input(argc>1?argv[1]:NULL);
    test_three_player_rules();test_three_player_shop_handoff();test_native_card_draw_copy();
    test_shared_supplies(2);test_shared_supplies(3);
    test_online_gameplay_fixes();
    test_native_heap_transaction();
    test_native_replay_format();
    if(argc==3 && !strcmp(argv[1],"--native-bomb-prefix"))test_native_bomb_prefix(argv[2]);
    printf("PASS %u assertions; isolated process, no game/desktop interaction\n",checks);
    return 0;
}
