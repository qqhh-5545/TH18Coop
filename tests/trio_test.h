/* Included by the real native fixture after the existing two-player cases. */
static unsigned focus_deleted;
static void *immunity_enemy;static unsigned immunity_interrupts;
static void TC fake_immunity_interrupt(void *vm,int interrupt){
    CHECK(vm==OFF(immunity_enemy,0x124));CHECK(interrupt==11 || interrupt==22);++immunity_interrupts;
}
/* Optional private qualification uses a 203-byte prefix extracted from the
   hash-verified local executable. No proprietary bytes enter the package. */
static void test_native_bomb_prefix(const char *path){
    FILE *file=fopen(path,"rb");CHECK(file);unsigned char code[203];CHECK(fread(code,1,sizeof(code),file)==sizeof(code));CHECK(fgetc(file)==EOF);fclose(file);
    CHECK(code[0]==0x55 && code[6]==0xa1 && code[11]==0x83);
    reset_context();CoopOnlineMap map={0};map.players=3;online_map=&map;
    unsigned char bombs[3][0x40]={{0}};PTR(0x4cf2b8)=bombs[0];guests[0].bomb=bombs[1];guests[1].bomb=bombs[2];
    immunity_enemy=calloc(1,0x558c);CHECK(immunity_enemy);FIELD(immunity_enemy,0x5138)=11;FIELD(immunity_enemy,0x513c)=22;
    memcpy((void *)0x42ed40,code,sizeof(code));
    const unsigned char epilogue[]={0x5f,0x5e,0x5b,0x8b,0xe5,0x5d,0xc3};memcpy((void *)0x42ee0b,epilogue,sizeof(epilogue));
    stub(0x488fd0,fake_immunity_interrupt);const unsigned char bomb_mov[]={0xa1,0xb8,0xf2,0x4c,0};
    CHECK(patch_branch(0x42ed46,bomb_immunity_bridge,5,0xe9,bomb_mov));immunity_interrupts=0;
    FIELD(immunity_enemy,0x5130)=0x10000000;FIELD(bombs[1],0x30)=1;((Tick)0x42ed40)(immunity_enemy);
    CHECK((FIELD(immunity_enemy,0x5130)&0x20000001)==0x20000001 && immunity_interrupts==1);
    CHECK(PTR(0x4cf2b8)==bombs[0]);
    FIELD(bombs[1],0x30)=0;FIELD(bombs[2],0x30)=1;((Tick)0x42ed40)(immunity_enemy);CHECK(immunity_interrupts==1 && (FIELD(immunity_enemy,0x5130)&1));
    FIELD(bombs[2],0x30)=2;((Tick)0x42ed40)(immunity_enemy);CHECK(immunity_interrupts==2 && !(FIELD(immunity_enemy,0x5130)&0x20000001));
    FIELD(bombs[0],0x30)=1;((Tick)0x42ed40)(immunity_enemy);CHECK(immunity_interrupts==3 && (FIELD(immunity_enemy,0x5130)&1));
    FIELD(immunity_enemy,0x5130)=0;((Tick)0x42ed40)(immunity_enemy);CHECK(!FIELD(immunity_enemy,0x5130) && immunity_interrupts==3);
    free(immunity_enemy);immunity_enemy=NULL;guests[0].bomb=guests[1].bomb=NULL;PTR(0x4cf2b8)=NULL;online_map=NULL;
    puts("PASS actual native EX immunity prefix: guest Bomb enables invulnerability, overlapping P3 maintains it, ending effects release it, host Bomb works, ordinary enemies remain native");
}
static void __stdcall fake_focus_delete(uint32_t id){CHECK(id==901);++focus_deleted;}
static unsigned char portrait_root[0x624],portrait_body[0x624],portrait_dialogue[0x1d8],portrait_gui[0x2c4];
static int TC fake_native_portrait(void *manager,void *vm) {
    CHECK(manager==(void *)0x1234 && vm==portrait_body);
    CHECK(FLT(vm,0x5c)==1 && FLT(vm,0x60)==1);
    CHECK(FLT(vm,0x30)==64 && FLT(vm,0x34)==120 && FLT(vm,0x5f0)==5 && FLT(vm,0x5f4)==10);
    CHECK(FIELD(vm,0x524)==0xa0123456 && FIELD(vm,0x528)==0x80123456);
    return 41;
}
static int __cdecl fake_portrait_scene(void) {return anm_draw((void *)0x1234,portrait_body);}
static void test_online_gameplay_fixes(void){
    reset_context();CoopOnlineMap map={0};map.players=3;online_map=&map;
    guests[1].player=calloc(1,0x479d0);CHECK(ship(2));FIELD(ship(2),0x476ac)=1;
    set_position(p1,-80,100);set_position(ship(1),80,100);set_position(ship(2),0,200);
    uint32_t regs[8]={0};float origin[2]={70,90};coop_nearest_aim(regs,origin,7);CHECK(regs[7]==(uint32_t)(uintptr_t)ship(1));
    FIELD(ship(1),0x476ac)=2;coop_nearest_aim(regs,origin,7);CHECK(regs[7]==(uint32_t)(uintptr_t)ship(2));
    partner[2].ghost=1;coop_nearest_aim(regs,origin,7);CHECK(regs[7]==(uint32_t)(uintptr_t)p1);
    FIELD(ship(1),0x476ac)=1;partner[2].ghost=0;origin[0]=0;origin[1]=100;coop_nearest_aim(regs,origin,7);CHECK(regs[7]==(uint32_t)(uintptr_t)p1);
    FIELD(p1,0x476ac)=2;coop_nearest_aim(regs,origin,7);CHECK(regs[7]==(uint32_t)(uintptr_t)ship(1));
    unsigned char *bullet=calloc(1,0xe40);CHECK(bullet);FLT(bullet,0xe38)=75;FLT(bullet,0xe3c)=100;
    *(unsigned char *)0x425fe0=0xc3;uint32_t result,fp;
    __asm__ volatile("pushl %%ebx\n\tmovl %2,%%ebx\n\tmovl $0x3fc00000,%%eax\n\tmovd %%eax,%%xmm0\n\tcall _aim_spawn_bridge\n\tmovd %%xmm0,%%edx\n\tpopl %%ebx"
        :"=a"(result),"=d"(fp):"r"(bullet):"ecx","xmm0","memory","cc");
    CHECK(result==(uint32_t)(uintptr_t)ship(1) && fp==0x3fc00000);free(bullet);
    *(unsigned char *)0x449ada=0xc3;
    __asm__ volatile("movl $0x42960000,%%eax\n\tmovd %%eax,%%xmm2\n\tmovl $0x42c80000,%%eax\n\tmovd %%eax,%%xmm3\n\tcall _aim_laser_emit_bridge\n\tmovd %%xmm2,%%edx"
        :"=a"(result),"=d"(fp)::"ecx","xmm2","xmm3","memory","cc");
    CHECK(result==(uint32_t)(uintptr_t)ship(1) && fp==0x42960000);
    unsigned char *ecl=calloc(1,0x1280);CHECK(ecl);FLT(ecl,0x1270)=0;FLT(ecl,0x1274)=200;
    *(unsigned char *)0x435eb3=0xc3;
    __asm__ volatile("pushl %%edi\n\tmovl %1,%%edi\n\tcall _aim_ecl_angle1_bridge\n\tmovl %%ecx,%%eax\n\tpopl %%edi"
        :"=a"(result):"r"(ecl):"ecx","edx","memory","cc");
    CHECK(result==(uint32_t)(uintptr_t)ship(2));free(ecl);
    bullet=calloc(1,0x700);CHECK(bullet);FLT(bullet,0x638)=75;FLT(bullet,0x63c)=100;FIELD(bullet,0x690)=17;
    *(unsigned char *)0x438e03=0xc3;
    __asm__ volatile("pushl %%esi\n\tpushl %%ebx\n\tmovl %2,%%esi\n\tcall _aim_homing_bridge\n\tmovl %%ebx,%%edx\n\tpopl %%ebx\n\tpopl %%esi"
        :"=a"(result),"=d"(fp):"r"(bullet):"ecx","memory","cc");
    CHECK(result==17 && fp==(uint32_t)(uintptr_t)ship(1));free(bullet);
    unsigned char bombs[3][0x40]={{0}};PTR(0x4cf2b8)=bombs[0];guests[0].bomb=bombs[1];guests[1].bomb=bombs[2];
    FIELD(bombs[1],0x30)=1;coop_any_bomb(regs);CHECK(regs[7]==(uint32_t)(uintptr_t)bombs[1]);
    *(unsigned char *)0x42ed4b=0xc3;__asm__ volatile("call _bomb_immunity_bridge":"=a"(result)::"ecx","edx","memory","cc");CHECK(result==(uint32_t)(uintptr_t)bombs[1]);
    FIELD(bombs[2],0x30)=1;FIELD(bombs[1],0x30)=0;coop_any_bomb(regs);CHECK(regs[7]==(uint32_t)(uintptr_t)bombs[2]);
    FIELD(bombs[2],0x30)=2;coop_any_bomb(regs);CHECK(regs[7]==(uint32_t)(uintptr_t)bombs[0]);
    FIELD(bombs[1],0x30)=1;FIELD(p1,0x476ac)=1;FIELD(ship(2),0x47778)=180;
    FIELD(p1,0x47778)=0;share_bomb_protection(bombs[1]);CHECK(FIELD(p1,0x47778)==60 && FIELD(ship(2),0x47778)==180);
    FIELD(p1,0x47778)=12;share_bomb_protection(bombs[1]);CHECK(FIELD(p1,0x47778)==12);
    FIELD(bombs[1],0x30)=0;share_bomb_protection(bombs[1]);FIELD(bombs[1],0x30)=1;share_bomb_protection(bombs[1]);CHECK(FIELD(p1,0x47778)==60);
    guest_slot=1;enter_p2(0);coop_any_bomb(regs);CHECK(regs[7]==(uint32_t)(uintptr_t)bombs[1]);leave_p2(0);guest_slot=0;CHECK(PTR(0x4cf2b8)==bombs[0]);
    focus_deleted=0;stub(0x488cf0,fake_focus_delete);FIELD(p1,0x476ac)=2;FIELD(p1,0x476b0)=901;FIELD(p1,0x476b4)=902;
    prepare_player_tick(0);prepare_player_tick(0);CHECK(focus_deleted==1 && !FIELD(p1,0x476b0) && FIELD(p1,0x476b4)==902);
    FIELD(p1,0x476b4)=0;FIELD(p1,0x476ac)=1;
    unsigned char cards[4][0x54]={{0}};unsigned flags[]={0x41,9,1,9};
    for(unsigned n=0;n<4;++n){FIELD(cards[n],4)=10+n;FIELD(cards[n],0x50)=flags[n];FIELD(cards[n],0xc)=(uint32_t)(uintptr_t)cards[n];
        FIELD(cards[n],0x10)=n==3?0:(uint32_t)(uintptr_t)OFF(cards[n+1],0xc);FIELD(guest_cards,0x58+n*4)=100+n;}
    FIELD(guest_cards,0x1c)=(uint32_t)(uintptr_t)OFF(cards[0],0xc);FIELD(guest_cards,0x2c)=2;FIELD(guest_cards,0x30)=1;
    FIELD(guest_cards,0x38)=(uint32_t)(uintptr_t)cards[3];HudStats stats={0};read_cards(&stats,1,guest_cards);
    CHECK(stats.cards[1][0].category==1 && stats.cards[1][1].category==0 && stats.cards[1][2].category==2 && stats.cards[1][3].category==0);
    for(unsigned n=0;n<4;++n)CHECK(stats.cards[1][n].native_ui==100+n && stats.cards[1][n].selected==(n==3));
    FIELD(guest_cards,0x38)=(uint32_t)(uintptr_t)cards[1];read_cards(&stats,1,guest_cards);CHECK(stats.cards[1][1].selected && !stats.cards[1][3].selected && stats.cards[1][1].native_ui==101);
    memset(portrait_root,0,sizeof(portrait_root));memset(portrait_body,0,sizeof(portrait_body));memset(portrait_dialogue,0,sizeof(portrait_dialogue));memset(portrait_gui,0,sizeof(portrait_gui));
    PTR(0x4cf2e0)=portrait_gui;FIELD(portrait_gui,0x1b0)=(uint32_t)(uintptr_t)portrait_dialogue;FIELD(portrait_dialogue,0x50)=777;
    FIELD(portrait_root,0x544)=777;FIELD(portrait_root,0x1c)=9;FIELD(portrait_body,0x5bc)=(uint32_t)(uintptr_t)portrait_root;FIELD(portrait_body,0x1c)=9;
    CHECK(is_dialogue_portrait(portrait_root) && is_dialogue_portrait(portrait_body));
    FIELD(portrait_body,0x524)=FIELD(portrait_body,0x528)=0xff123456;anm_draw_original=fake_vm_draw;vm_alpha=255;draw_alpha[0]=51;
    CHECK(anm_draw((void *)0x1234,portrait_body)==19 && FIELD(portrait_body,0x524)==0xff123456);
    FLT(portrait_body,0x30)=64;FLT(portrait_body,0x34)=120;
    FLT(portrait_body,0x5f0)=5;FLT(portrait_body,0x5f4)=10;
    FLT(portrait_body,0x5c)=FLT(portrait_body,0x60)=1;
    FIELD(portrait_body,0x524)=0xa0123456;FIELD(portrait_body,0x528)=0x80123456;
    anm_draw_original=fake_native_portrait;scene_draw_original=fake_portrait_scene;
    CHECK(scene_draw()==41);
    hud_draws=hud_captures=0;
    CHECK(draw_dialogue_hud(p1)==1 && hud_draws==1 && hud_captures==0);
    CHECK(draw_late_hud(p1)==1 && hud_draws==1 && hud_captures==1);
    FIELD(portrait_gui,0x1b0)=0;
    CHECK(draw_dialogue_hud(p1)==1 && hud_draws==1 && hud_captures==1);
    CHECK(draw_late_hud(p1)==1 && hud_draws==2 && hud_captures==2);
    PTR(0x4cf2e0)=PTR(0x51f65c)=NULL;guests[0].bomb=guests[1].bomb=NULL;PTR(0x4cf2b8)=NULL;
    free(guests[1].player);memset(&guests[1],0,sizeof(guests[1]));online_map=NULL;reset_context();
    puts("PASS gameplay fixes: mixed card mapping/switching, nearest living 3-seat aim and SSE bridge, native all-seat Bomb reads, short protection, focus cleanup, native portrait and dialogue HUD ordering");
}
static void test_three_player_rules(void) {
    reset_context();CoopOnlineMap map={0};map.players=3;map.role=2;online_map=&map;
    GuestNative *third=&guests[1];memset(third,0,sizeof(*third));
    third->player=calloc(1,0x479d0);third->cards=calloc(1,0xd70);CHECK(third->player && third->cards);
    FIELD(ship(2),0x476ac)=1;third->funds=777;*resource(2,0)=175;*resource(2,4)=400;
    *resource(2,0x10)=1;*resource(2,0x1c)=7;*resource(2,0x28)=2;
    input_latch.valid=1;input_latch.p2=0x10;input_latch.p3=0x80;
    guest_slot=1;CHECK(p2_input()==0x80);enter_p2(1);
    CHECK(PTR(0x4cf410)==ship(2) && PTR(0x4cf298)==third->cards && U32(0x4ccd34)==777 && U32(0x4ca428)==0x80);
    U32(0x4ccd34)-=17;U32(0x4ccd38)+=25;leave_p2(1);guest_slot=0;
    CHECK(third->funds==760 && *resource(2,0)==200 && p2_funds==400 && U32(0x4ccd34)==300 && *resource(1,0)==200);
    CHECK(coop_view_player()==2 && !coop_shared_keyboard());
    set_position(p1,0,320);set_position(ship(1),8,320);set_position(ship(2),-8,320);
    *resource(1,0x10)=0;*resource(2,0x10)=3;
    CHECK(transfer_recipient(0,0)==1);
    partner[2].ghost=1;*resource(2,0x10)=-1;FIELD(ship(2),0x476ac)=2;
    CHECK(transfer_recipient(0,0)==2); /* Ghost outranks a living lower-life ally. */
    partner[2].ghost=0;FIELD(ship(2),0x476ac)=1;*resource(2,0x10)=0;
    CHECK(transfer_recipient(0,0)==1); /* Seat tie. */
    *resource(1,0)=250;*resource(2,0)=100;CHECK(transfer_recipient(0,1)==2);
    *resource(1,0)=100;CHECK(transfer_recipient(0,1)==1);
    uint32_t input[3]={8,0,0};partner_tick(input);CHECK(partner[0].life_taps==1 && partner[0].life_target==2);
    input[0]=0;partner_tick(input);set_position(ship(1),100,320);
    input[0]=8;partner_tick(input);CHECK(partner[0].life_taps==1 && partner[0].life_target==3);
    partner[0].ghost=partner[1].ghost=1;partner[2].ghost=0;input[0]=0;wipe_frames=team_over=gameovers=0;
    for(unsigned n=0;n<200;++n)partner_tick(input);CHECK(!team_over && !wipe_frames && !gameovers);
    partner[2].ghost=1;for(unsigned n=0;n<179;++n)partner_tick(input);CHECK(!team_over);
    partner_tick(input);CHECK(team_over && gameovers==1);
    memset(partner,0,sizeof(partner));wipe_frames=team_over=0;
    void *enemy=calloc(1,0x558c),*script=calloc(1,0x6834),*enemy_manager=calloc(1,0x88);CHECK(enemy && script && enemy_manager);
    FIELD(enemy,0x5588)=(uint32_t)(uintptr_t)script;FIELD(script,0x6830)=41;FIELD(enemy_manager,0x48)=41;PTR(0x4cf2d0)=enemy_manager;
    uint32_t regs[8]={0};regs[4]=(uint32_t)(uintptr_t)enemy;regs[1]=120;coop_scale_boss_damage(regs);CHECK(regs[1]==80);
    partner[2].ghost=1;regs[1]=120;coop_scale_boss_damage(regs);CHECK(regs[1]==90);
    partner[1].ghost=1;regs[1]=120;coop_scale_boss_damage(regs);CHECK(regs[1]==120);
    free(enemy);free(script);free(enemy_manager);PTR(0x4cf2d0)=NULL;
    void *items=calloc(1,0x14+16*0xc94);CHECK(items!=NULL);spawn_original=fake_spawn_transfer;spawn_fail=0;spawn_fail_after=-1;spawn_count=0;
    float at[3]={0,300,0};spawn_item(items,1,at,0,0,0,0,0,0);CHECK(spawn_count==3);
    spawn_count=0;spawn_item(items,5,at,0,0,0,0,0,0);CHECK(spawn_count==1);
    spawn_count=0;spawn_item(items,7,at,0,0,0,0,0,0);CHECK(spawn_count==1);
    spawn_count=0;spawn_item(items,8,at,0,0,0,0,0,0);CHECK(spawn_count==1);free(items);
    free(third->player);free(third->cards);memset(third,0,sizeof(*third));third->selected_character=-1;third->loadout_count=2;
    online_map=NULL;guest_slot=0;memset(partner,0,sizeof(partner));reset_context();
    puts("PASS three seats: independent P3 input/economy, rescue target priority/reset, 3->2->1 Boss scaling, all-seat wipe and reference P multiplier and single shared-supply spawns");
}

static int third_shop_opened;
static void * __cdecl fake_shop_allocate(unsigned bytes){CHECK(bytes==0xe3c);return calloc(1,bytes);}
static void TC fake_shop_reset(void *player){CHECK(player==ship(2) && PTR(0x4cf410)==ship(2));}
static int TC fake_third_shop_init(void *shop,const float *position){
    CHECK(scope_depth==1 && guest_slot==1 && PTR(0x4cf410)==ship(2));
    CHECK(PTR(0x4cf298)==ability(2) && U32(0x4ccd34)==777 && position[0]==12);
    ++third_shop_opened;return 0;
}
static int TC fake_second_shop_tick(void *shop){
    CHECK(scope_depth==1 && guest_slot==0 && PTR(0x4cf410)==ship(1));
    shop_close(shop);
    CHECK(scope_depth==1 && guest_slot==0 && PTR(0x4cf410)==ship(1));
    CHECK(shop_pending_owner==2 && !third_shop_opened);
    return 1;
}
static void test_three_player_shop_handoff(void){
    reset_context();CoopOnlineMap map={0};map.players=3;online_map=&map;
    guests[1].player=calloc(1,0x479d0);guests[1].cards=calloc(1,0xd70);guests[1].funds=777;
    CHECK(ship(2) && ability(2));PTR(0x4cf2e4)=(void *)1;
    void *shop=calloc(1,0xe3c);CHECK(shop);FLT(shop,0x1d0)=12;shop_owner=1;third_shop_opened=0;
    shop_tick_original=fake_second_shop_tick;shop_close_original=fake_shop_close;shop_init_original=fake_third_shop_init;
    stub(0x48dc71,fake_shop_allocate);stub(0x416cd0,fake_shop_reset);
    CHECK(shop_tick(shop)==1 && third_shop_opened==1 && shop_owner==2 && !shop_pending_owner);
    CHECK(!scope_depth && guest_slot==0 && PTR(0x4cf410)==p1 && PTR(0x4cf298)==host_cards);
    CHECK(U32(0x4ccd34)==300 && guests[0].funds==390 && guests[1].funds==777);
    free(PTR(0x4cf2a4));PTR(0x4cf2a4)=NULL;PTR(0x4cf2e4)=NULL;free(shop);
    free(guests[1].player);free(guests[1].cards);memset(&guests[1],0,sizeof(guests[1]));online_map=NULL;
    puts("PASS nested P2 shop close defers P3 opening until P2 globals restore; all three economies isolated");
}

static unsigned char pose_root[0x624],pose_child[0x624];
static unsigned pose_draws;
static void *TC fake_pose_lookup(void *manager,uint32_t id){CHECK(manager==(void *)0x1234 && id==123);return pose_root;}
static float *TC fake_pose_position(void *vm,float *out){out[0]=FLT(vm,0x30);out[1]=FLT(vm,0x34);out[2]=0;return out;}
static float *TC fake_pose_angle(void *vm){return (float *)OFF(vm,0x3c);}
static void test_native_card_draw_copy(void) {
    memset(pose_root,0,sizeof(pose_root));memset(pose_child,0,sizeof(pose_child));
    FIELD(pose_root,0x1c)=31;FIELD(pose_root,0x28)=5;FIELD(pose_root,0x544)=1;FIELD(pose_child,0x544)=2;
    uint32_t child_node[2]={(uint32_t)(uintptr_t)pose_child,0};FIELD(pose_root,0x598)=(uint32_t)(uintptr_t)child_node;
    for(unsigned n=0;n<2;++n){void *vm=n?pose_child:pose_root;FLT(vm,0x30)=100+n*4;FLT(vm,0x34)=100;FLT(vm,0x54)=FLT(vm,0x58)=FLT(vm,0x5c)=FLT(vm,0x60)=1;FIELD(vm,0x538)=0x800000;FIELD(vm,0x534)=3;FIELD(vm,0x524)=0xdf123456;FLT(vm,0x74)=64;FLT(vm,0x78)=80;}
    FIELD(pose_child,0x5bc)=(uint32_t)(uintptr_t)pose_root;
    unsigned char before_root[0x624],before_child[0x624];memcpy(before_root,pose_root,sizeof(pose_root));memcpy(before_child,pose_child,sizeof(pose_child));
    void *old_manager=PTR(0x51f65c);PTR(0x51f65c)=(void *)0x1234;
    stub(0x488b40,fake_pose_lookup);stub(0x4065a0,fake_pose_position);stub(0x4097d0,fake_pose_angle);
    *(float *)0x56aca0=2;U32(0x56acb4)=640;U32(0x56acb8)=32;pose_draws=0;
    HudCard card={.native_ui=123};HudNativeQuad output[4];CHECK(coop_hud_card_quads(&card,300,100,32,40,output,4)==2);
    CHECK(fabsf(output[0].x[0]-300)<.01f && fabsf(output[1].x[0]-302)<.01f && output[0].color==0xdf123456);
    CHECK(!memcmp(before_root,pose_root,sizeof(pose_root)) && !memcmp(before_child,pose_child,sizeof(pose_child)));
    PTR(0x51f65c)=old_manager;
    puts("PASS native ANM inventory snapshot yields HUD geometry without modifying VM state/timers/parents/colors");
}
