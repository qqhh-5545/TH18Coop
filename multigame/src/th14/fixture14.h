#ifndef TH14_FIXTURE_H
#define TH14_FIXTURE_H
// Opt-in, process-local integration fixtures. Never accepted from a network
// packet and completely disabled in online games, including diagnostic ones.
static void fixture14_before(void) {
    if(!test || test->magic!=0x54483134 || online_map || !test->command || ending)return;
    for(unsigned i=0;i<players;++i)if(!seats[i].player)return;
    unsigned command=test->command,id=test->args[0];test->command=0;
    if(command==1) {
        void *manager=PTR(0x4db660);
        for(unsigned i=0;i<0x1258;++i){void *item=OFF(manager,0x14+i*0xc18);if(FIELD(item,0xbf0))((void (TC *)(void *))0x439c10)(item);}
        memset(item_owners,-1,sizeof(item_owners));memset(gift_owners,-1,sizeof(gift_owners));memset(gift_power,0,sizeof(gift_power));memset(rules14,0,sizeof(rules14));wipe14=0;
        for(unsigned i=0;i<players;++i) {
            seats[i].ghost=0;enter(i,0);U32(0x4f5864)=2;U32(0x4f5868)=U32(0x4f586c)=U32(0x4f5874)=0;U32(0x4f5870)=2;U32(0x4f5858)=i?100:200;
            ((void (TC *)(void *))0x44cc10)(seats[i].player);FIELD(seats[i].player,0x684)=1;timer(seats[i].player,0x182c0,36000);clear_focus(seats[i].player);
            set_position(seats[i].player,id?i*8.f-4.f:(i?100.f:-100.f),380.f);leave(i,0);
        }
    } else if(command==2 && id>=1 && id<=8 && test->args[1]<players) {
        unsigned owner=test->args[1];float pos[3]={FLT(seats[owner].player,0x5e0),FLT(seats[owner].player,0x5e4),0};
        void *item=spawn14(PTR(0x4db660),id,pos,0,0,0,0);test->spawn_count=0;
        void *manager=PTR(0x4db660);for(unsigned i=0;i<0x258;++i)if(FIELD(OFF(manager,0x14+i*0xc18),0xbf0))++test->spawn_count;
        if(item){unsigned index=item14_index(manager,item);item_owners[index]=(int8_t)owner;FIELD(item,0xbf0)=4;FLT(item,0xbfc)=5.f;}
    } else if(command==3 && id<players) {
        enter(id,0);U32(0x4f5864)=0;timer(seats[id].player,0x182c0,0);((void (TC *)(void *))0x44f7a0)(seats[id].player);leave(id,0);
    } else if(command==4) {
        for(unsigned i=0;i<players;++i)set_position(seats[i].player,i*8.f-4.f,380.f);
    } else if(command==5) {
        for(unsigned i=0;i<players;++i){enter(i,0);U32(0x4f5864)=0;timer(seats[i].player,0x182c0,0);((void (TC *)(void *))0x44f7a0)(seats[i].player);leave(i,0);}
    } else if(command==6) {
        uint32_t regs[8]={0};float origin[2]={(float)(int32_t)test->args[0],(float)(int32_t)test->args[1]};aim14_nearest(regs,origin,7);test->spawn_count=0;
        for(unsigned i=0;i<players;++i)if(regs[7]==(uint32_t)(uintptr_t)seats[i].player)test->spawn_count=i+1;
    } else if(command==7 && id<players)*resource(id,0x4f5864)=0;
    else if(command==8){*resource(0,0x4f5864)=8;*resource(0,0x4f5870)=8;*resource(0,0x4f5858)=400;}
    else if(command==9 && id<players) {
        float pos[3]={FLT(seats[id].player,0x5e0),FLT(seats[id].player,0x5e4),0},size[2]={16.f,16.f};
        for(unsigned i=0;i<players;++i)timer(seats[i].player,0x182c0,0);
        if(test->args[1])rect14((uint32_t)(uintptr_t)pos,(uint32_t)(uintptr_t)size,0);
        else {uint32_t values[2]={(uint32_t)(uintptr_t)pos,0};coop_circle14(values,4.f);}
    } else if(command==10)stage_clear14();
    else if(command==11) {
        // Refresh only test protection. Do not reset stage objects, movement,
        // resources, native scale/interpolation, or Boss scripts.
        for(unsigned i=0;i<players;++i)if(living(i))timer(seats[i].player,0x182c0,36000);
    }
    if(command==12 && id<players){
        for(unsigned i=0;i<players;++i)set_position(seats[i].player,i==id?-100.f:100.f,i==id?100.f:380.f);
        if(test->args[2])*resource(id,0x4f5858)=400;
        float pos[3]={100,340,0};spawn14(PTR(0x4db660),test->args[1],pos,0,0,0,0);
    }
    if(command==13){test->spawn_count=0;void *m=PTR(0x4db660);for(unsigned i=0;i<0x258;++i)if(FIELD(OFF(m,0x14+i*0xc18),0xbf0))++test->spawn_count;}
    if(command==14 && id<players){
        // Populate both sides of the playfield while both players are below the line.
        void *manager=PTR(0x4db660);
        for(unsigned i=0;i<players;++i)set_position(seats[i].player,i?132.f:-132.f,380.f);
        memset(fixture_item14,0,sizeof(fixture_item14));
        unsigned count=test->args[1];if(count>240)count=240;
        test->spawn_count=0;
        for(unsigned n=0;n<count;++n){float pos[3]={n%2?96.f:-96.f,100.f+12.f*(n%9),0};
            void *item=spawn14(manager,2,pos,0,0,0,0);
            if(item){fixture_item14[item14_index(manager,item)]=1;++test->spawn_count;}}
    }
    if(command==15){test->spawn_count=0;
        for(unsigned i=0;i<0x1258;++i)test->spawn_count+=fixture_item14[i]==2;}
    if(command==16 && id<players){
        // Advance a normal red P from a confirmed pickup position, without
        // setting its recipient or bypassing the engine's pickup branch.
        void *manager=PTR(0x4db660),*player=seats[id].player;
        for(unsigned i=0;i<players;++i)set_position(seats[i].player,i?100.f:-100.f,380.f);
        float pos[3]={FLT(player,0x5e0),FLT(player,0x5e4),0};
        void *item=((Spawn14)spawn_original)(manager,1,pos,0,0,0,0);
        if(item){unsigned i=item14_index(manager,item);
            item_owners[i]=gift_owners[i]=-1;gift_power[i]=0;
            FIELD(item,0xbf0)=ITEM14_FALLING;FIELD(item,0xc00)=0;}
    }
    MemoryBarrier();test->ack++;
}
#endif
