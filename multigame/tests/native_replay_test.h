static void __cdecl fake_replay_stage_cards(void) { }
static void test_native_replay_format(void) {
    rp_clear();online_map=NULL;rp_viewer=1;rp_players=3;strcpy(rp_identity,"fixture-identity");
    RpStage *s=&rp_stages[2];s->header.count=2;s->capacity=2;s->frames=calloc(2,sizeof(RpFrame));
    for(unsigned id=0;id<3;++id){s->header.birth[id]=id?1:0;s->header.seats[id].character=id;s->header.seats[id].state=1;s->header.seats[id].selected=UINT32_MAX;}
    s->header.seats[2].resources[0]=123;s->header.seats[1].partner.life_taps=3;
    s->frames[0].keys[0]=1;s->frames[0].keys[1]=0x81;s->frames[0].keys[2]=0x42;s->frames[1].rng[2]=0x12345678;
    uint32_t size;unsigned char *blob=rp_encode(&size);CHECK(blob && size==sizeof(RpHeader)+sizeof(RpStageHeader)+2*sizeof(RpFrame));
    uint32_t hash=rp_hash(blob,size);rp_clear();CHECK(rp_decode(blob,size));CHECK(rp_players==3 && rp_stages[2].header.count==2);
    CHECK(rp_stages[2].frames[0].keys[2]==0x42 && rp_stages[2].frames[1].rng[2]==0x12345678);
    CHECK(rp_stages[2].header.seats[1].partner.life_taps==3 && rp_stages[2].header.seats[2].resources[0]==123);
    void *manager=calloc(1,0x31c);rp_manager=manager;rp_active=1;rp_stage_original=fake_replay_stage_cards;
    FIELD(manager,0x214)=2;FIELD(manager,0x20c)=0;guests[0].selected_character=guests[1].selected_character=-1;
    rp_stages[2].header.initial_rng[0]=1234;rp_stages[2].header.initial_rng[2]=5678;rp_stage_cards();
    CHECK(guests[0].selected_character==1 && guests[1].selected_character==2);
    CHECK(U32(0x4cf280)==1234 && U32(0x4cf288)==5678);
    CHECK(!rp_guest_due(1));FIELD(manager,0x20c)=1;CHECK(rp_guest_due(1));
    free(manager);
    rp_clear();CHECK(!rp_decode(blob,size-1));CHECK(!rp_decode(blob,sizeof(RpHeader)-1));
    RpHeader *head=(RpHeader *)blob;head->players=4;CHECK(!rp_decode(blob,size));head->players=3;
    head->bytes[0]=UINT32_MAX;CHECK(!rp_decode(blob,size));head->bytes[0]=0;
    RpStageHeader *stage=(RpStageHeader *)(blob+sizeof(RpHeader));stage->birth[2]=3;CHECK(!rp_decode(blob,size));stage->birth[2]=1;
    stage->seats[1].card_count=257;CHECK(!rp_decode(blob,size));stage->seats[1].card_count=0;
    stage->seats[1].card_count=1;stage->seats[1].cards[0].id=999;CHECK(!rp_decode(blob,size));stage->seats[1].card_count=0;
    strcpy(head->identity,"wrong-build");CHECK(!rp_decode(blob,size));CHECK(rp_hash(blob,size)!=hash);
    free(blob);rp_clear();rp_viewer=0;rp_players=0;rp_identity[0]=0;
    CHECK(data_path_matches("F:\\Game\\coop-data\\replay-sessions\\0123456789abcdef0123456789abcdef\\viewer\\ShanghaiAlice\\th18",L"F:\\Game\\th18.exe"));
    CHECK(!data_path_matches("F:\\Game\\coop-data\\replay-sessions\\0123456789abcdef0123456789abcdef\\host\\ShanghaiAlice\\th18",L"F:\\Game\\th18.exe"));
    puts("PASS native multiplayer Replay extension: 3-seat stage snapshots/input roundtrip, checksum, truncated/oversize/bad-seat/card/build rejection and viewer profile isolation");
}
