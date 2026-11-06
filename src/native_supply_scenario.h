#ifndef TH18_NATIVE_SUPPLY_SCENARIO_H
#define TH18_NATIVE_SUPPLY_SCENARIO_H
/* Synthetic pickups in an explicit isolated, no-commit session only. The
   runtime passes enabled=0 for every ordinary online/local game. */
typedef struct {int power,cap,lives,life_piece,bombs,bomb_piece,funds,cards;} SupplySample;
static struct {
    void *item;unsigned step,collector;uint32_t next,started;
    int active,done;SupplySample before[3];
} supply_test;
static SupplySample supply_sample(unsigned id){
    void *cards=ability(id);
    return (SupplySample){*resource(id,0),*resource(id,4),*resource(id,0x10),*resource(id,0x14),
        *resource(id,0x20),*resource(id,0x24),id?(int)guests[id-1].funds:(int)U32(0x4ccd34),cards?(int)FIELD(cards,0x28):0};
}
static const unsigned supply_types[]={4,5,6,7,8,16,17,18,19,1,2};
static int supply_test_before(int enabled,uint32_t frame){
    if(!enabled || supply_test.done)return 1;
    void *pause=PTR(0x4cf40c);
    if(!p1 || !ship(1) || (party_count()==3 && !ship(2)) || frames<120 ||
       (pause && FIELD(pause,0x1ec)))return 1;
    if(supply_test.active){if(frame-supply_test.started>120){log_line("SUPPLY_TEST FAIL pickup timeout step=%u",supply_test.step);return 0;}return 1;}
    if(frame<supply_test.next)return 1;
    unsigned type=supply_types[supply_test.step],collector=supply_test.step%party_count();
    for(unsigned id=0;id<party_count();++id){
        set_position(ship(id),(float)((int)id*100-100),400);
        FIELD(ship(id),0x47778)=600;FLT(ship(id),0x4777c)=600;FIELD(ship(id),0x47774)=599;
        *resource(id,0)=100+(int)id*50;
        *resource(id,0x10)=2;*resource(id,0x1c)=7;*resource(id,0x18)=0;
        unsigned table=U32(0x4ccd00)==4?0x4b3ff4:0x4b40c8;
        *resource(id,0x14)=(int)U32(table)-1;
        *resource(id,0x20)=1;*resource(id,0x24)=2;*resource(id,0x2c)=8;
        if(id)guests[id-1].funds=100+id*100;else U32(0x4ccd34)=100;
    }
    if(type==4 || type==5){*resource(collector,0x10)=7;*resource(collector,0x14)=0;}
    if(type==6 || type==7){*resource(collector,0x20)=8;*resource(collector,0x24)=0;}
    for(unsigned id=0;id<party_count();++id)supply_test.before[id]=supply_sample(id);
    float at[3]={FLT(ship(collector),0x620),400,0};
    typedef void *(TC *Spawn)(void *,uint32_t,const float *,uint32_t,uint32_t,uint32_t,uint32_t,uint32_t,uint32_t);
    void *manager=PTR(0x4cf2ec);
    void *item=((Spawn)spawn_original)(manager,type,at,0,0,0,0,0,0);int index=item?item_index(manager,item):-1;
    if(index<0)return 0;
    FIELD(item,0xc74)=4;item_owner[index]=(int8_t)collector;transfer_owner[index]=-1;transfer_power[index]=0;last_age[index]=0;
    supply_test.item=item;supply_test.collector=UINT32_MAX;supply_test.active=1;supply_test.started=frame;
    log_line("SUPPLY_TEST spawn step=%u type=%u target=P%u frame=%lu",supply_test.step,type,collector+1,(unsigned long)frame);
    return 1;
}
static int supply_test_after(uint32_t frame){
    if(!supply_test.active || supply_test.collector==UINT32_MAX)return 1;
    unsigned type=supply_types[supply_test.step],collector=supply_test.collector;int pass=1;
    for(unsigned id=0;id<party_count();++id){SupplySample b=supply_test.before[id],a=supply_sample(id);
        if(type==4 || type==5)pass&=a.lives==(b.lives<7?b.lives+1:7);
        else if(type==6 || type==7)pass&=a.bombs==(b.bombs<8?b.bombs+1:8);
        else if(type==8)pass&=a.power==a.cap;
        else if(type>=16 && type<=19)pass&=a.lives>b.lives || a.bombs>b.bombs || a.power>b.power || a.funds>b.funds || a.cards>b.cards || a.life_piece!=b.life_piece || a.bomb_piece!=b.bomb_piece;
        else if(type==1)pass&=id==collector?a.power>b.power:a.power==b.power;
        else if(type==2)pass&=id==collector?a.funds==b.funds+1:a.funds==b.funds;
        log_line("SUPPLY_TEST result step=%u type=%u P%u power=%d->%d funds=%d->%d lives=%d->%d life_frag=%d->%d bombs=%d->%d bomb_frag=%d->%d cards=%d->%d",
            supply_test.step,type,id+1,b.power,a.power,b.funds,a.funds,b.lives,a.lives,b.life_piece,a.life_piece,b.bombs,a.bombs,b.bomb_piece,a.bomb_piece,b.cards,a.cards);
    }
    log_line("SUPPLY_TEST %s step=%u type=%u collector=P%u",pass?"PASS":"FAIL",supply_test.step,type,collector+1);
    if(!pass)return 0;
    supply_test.active=0;supply_test.item=NULL;supply_test.next=frame+12;
    if(++supply_test.step==sizeof(supply_types)/sizeof(supply_types[0])){supply_test.done=1;log_line("SUPPLY_TEST PASS all11 cases players=%u",party_count());}
    return 1;
}
#endif
