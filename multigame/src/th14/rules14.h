#ifndef TH14_RULES_H
#define TH14_RULES_H
typedef void *(TC *Spawn14)(void *,uint32_t,const float *,uint32_t,uint32_t,uint32_t,uint32_t);
static unsigned item14_index(void *manager,void *item){return (unsigned)(((uintptr_t)item-(uintptr_t)manager-0x14)/0xc18);}
static void *TC spawn14(void *manager,uint32_t kind,const float *position,uint32_t a,uint32_t b,uint32_t c,uint32_t d) {
    void *first=((Spawn14)spawn_original)(manager,kind,position,a,b,c,d);
    unsigned index=first?item14_index(manager,first):0x1258;
    if(index<0x1258){item_owners[index]=gift_owners[index]=-1;gift_power[index]=0;}
    if(coop14_battle && !gift_spawning && (kind==1 || kind==3) && first)for(unsigned n=1;n<players;++n) {
        float pos[3]={position[0]+(n==1?8.f:-8.f),position[1]-4.f,position[2]};
        void *extra=((Spawn14)spawn_original)(manager,kind,pos,a,b,c,d);unsigned i=extra?item14_index(manager,extra):0x1258;
        if(i<0x1258){item_owners[i]=gift_owners[i]=-1;gift_power[i]=0;}
    }
    return first;
}
static int donate14(unsigned from,unsigned to,int power) {
    void *manager=PTR(0x4db660);if(!manager)return 0;unsigned count=power?6:1,free=0;
    for(unsigned i=0;i<0x258 && free<count;++i)free+=FIELD(OFF(manager,0x14+i*0xc18),0xbf0)==0;
    if(free<count)return 0;
    void *spawned[6]={0};gift_spawning=1;
    for(unsigned n=0;n<count;++n) {
        float pos[3]={FLT(seats[from].player,0x5e0)+(int)(n%3)*8.f-8.f,FLT(seats[from].player,0x5e4)+(int)(n/3)*6.f-6.f,0};
        void *item=spawn14(manager,power?(n<2?3:1):5,pos,0,0,0,0);unsigned i=item?item14_index(manager,item):0x1258;
        if(i>=0x1258 || !FIELD(item,0xbf0)) {
            for(unsigned j=0;j<n;++j){unsigned old=item14_index(manager,spawned[j]);((void (TC *)(void *))0x439c10)(spawned[j]);item_owners[old]=gift_owners[old]=-1;gift_power[old]=0;}
            gift_spawning=0;return 0;
        }
        spawned[n]=item;item_owners[i]=gift_owners[i]=(int8_t)to;gift_power[i]=power?(n<2?40:5):0;
        FIELD(item,0xbf0)=4;FLT(item,0xbfc)=5.f;
    }
    gift_spawning=0;log_line("P%u donated %s to P%u",from+1,power?"1.00 power":"a life",to+1);return 1;
}
typedef struct {uint32_t previous;int life,power,life_window,power_window,target,power_target,release;} RuleState14;
static RuleState14 rules14[3];static unsigned wipe14;
static void rules14_reset(void){memset(rules14,0,sizeof(rules14));wipe14=0;memset(item_owners,-1,sizeof(item_owners));memset(gift_owners,-1,sizeof(gift_owners));memset(gift_power,0,sizeof(gift_power));}
static int recipient14(unsigned from,int power) {
    int best=-1;void *p=seats[from].player;
    for(unsigned id=0;id<players;++id) {
        if(id==from || !seats[id].player)continue;
        float dx=FLT(p,0x5e0)-FLT(seats[id].player,0x5e0),dy=FLT(p,0x5e4)-FLT(seats[id].player,0x5e4);
        if(dx*dx+dy*dy>400.f)continue;
        int eligible=power?living(id) && *resource(id,0x4f5858)<*resource(id,0x4f585c):seats[id].ghost || (living(id) && *resource(id,0x4f5864)<8);
        if(!eligible)continue;
        if(best<0 || (!power && seats[id].ghost>seats[best].ghost) || ((power || seats[id].ghost==seats[best].ghost) && *resource(id,power?0x4f5858:0x4f5864)<*resource((unsigned)best,power?0x4f5858:0x4f5864)))best=(int)id;
    }
    return best;
}
static void rules14_tick(void) {
    if(!coop14_battle || ending)return;for(unsigned id=0;id<players;++id)if(!seats[id].player)return;
    void *pause=PTR(0x4db678);if(pause && FIELD(pause,4)){for(unsigned id=0;id<players;++id){rules14[id].previous=sampled[id];rules14[id].life=rules14[id].power=rules14[id].life_window=rules14[id].power_window=0;}return;}
    for(unsigned id=0;id<players;++id) {
        RuleState14 *s=&rules14[id];uint32_t keys=sampled[id],edge=keys&~s->previous;s->previous=keys;
        if(!living(id) || FIELD(seats[id].player,0x684)!=1){s->life=s->power=s->life_window=s->power_window=s->release=s->target=s->power_target=0;continue;}
        int life=recipient14(id,0),power=recipient14(id,1);
        if(s->target!=life+1){s->life=s->life_window=0;s->target=life+1;}
        if(s->power_target!=power+1){s->power=s->power_window=0;s->power_target=power+1;}
        if(s->life && !s->life_window)s->life=0;
        if(s->power && !s->power_window)s->power=0;
        if(!(keys&8))s->release=0;
        if((edge&8) && !(keys&1))s->power=s->power_window=0;
        if(life<0 || *resource(id,0x4f5864)<=0 || (keys&1))s->life=s->life_window=0;
        else if((edge&8) && !s->release) {
            s->life_window=24;if(++s->life==5){
                if(seats[life].ghost){--*resource(id,0x4f5864);revive((unsigned)life);}
                else if(donate14(id,(unsigned)life,0))--*resource(id,0x4f5864);
                s->life=s->life_window=0;s->release=1;
            }
        }
        if(power<0 || *resource(id,0x4f5858)<100)s->power=s->power_window=0;
        else if(edge&1){s->power_window=24;s->life=s->life_window=0;if(++s->power==5){if(donate14(id,(unsigned)power,100))*resource(id,0x4f5858)-=100;s->power=s->power_window=0;}}
        // The 24th frame is still valid. Expire only after testing its edge.
        if(!(edge&8) && s->life_window && --s->life_window==0)s->life=0;
        if(!(edge&1) && s->power_window && --s->power_window==0)s->power=0;
    }
    unsigned ghosts=0;for(unsigned id=0;id<players;++id)ghosts+=seats[id].ghost!=0;
    if(ghosts==players){if(++wipe14==180){log_line("Team wipe; opening native game-over menu");((void (__cdecl *)(void))0x449d30)();}}
    else wipe14=0;
}
static int transfer14_hint(unsigned id,float *x,float *y,char *kind,unsigned *count) {
    if(!living(id) || FIELD(seats[id].player,0x684)!=1)return 0;
    const RuleState14 *s=&rules14[id];
    if(s->life>=3 && s->life<=4){*kind='L';*count=(unsigned)s->life;}
    else if(s->power>=3 && s->power<=4){*kind='P';*count=(unsigned)s->power;}
    else return 0;
    *x=FLT(seats[id].player,0x5e0);*y=FLT(seats[id].player,0x5e4);return 1;
}
static int tick_native14(void *registry){fixture14_before();int result=((Callback)root_original)(registry);rules14_tick();return result;}
#endif
