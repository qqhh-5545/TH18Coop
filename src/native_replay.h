#ifndef TH18_NATIVE_REPLAY_H
#define TH18_NATIVE_REPLAY_H
/* Extend the native t18r stream, not a parallel game simulation. All callbacks
   run on the owning game thread. Only fixed-width values cross the file boundary. */
#define RP_STAGES 8
#define RP_MAX_FRAMES 216000u
#define RP_MAX_BYTES (128u*1024u*1024u)
typedef struct {uint32_t id,data,timer[8];} RpCard;
typedef struct {
    uint32_t character,funds,previous,base_frame,rng_after[4],shot_hash;
    unsigned char resources[0x30];
    uint32_t position[6],state,focus,invincible[3],death_timer[3];
    PartnerState partner;
    uint32_t card_count,selected;
    RpCard cards[256];
} RpSeat;
typedef struct {uint32_t keys[3],rng[4],digest,world[3];} RpFrame;
typedef struct {uint32_t count,birth[3],initial_rng[4];RpSeat seats[3];} RpStageHeader;
typedef struct {RpStageHeader header;RpFrame *frames;uint32_t capacity;} RpStage;
typedef struct {char magic[8];uint32_t format,players,stage_size,frame_size;char identity[224];uint32_t bytes[8];} RpHeader;
typedef struct {char magic[8];uint32_t size,checksum;} RpFooter;
static RpStage rp_stages[RP_STAGES];
static void *rp_manager,*rp_init_original,*rp_destroy_original,*rp_save_original,*rp_record_original,*rp_play_original;
static void *rp_stage_original;
static int rp_recording,rp_active,rp_invalid,rp_stage=-1,rp_menu_seen,rp_stop;
static int rp_playing(void){return rp_active;}
static wchar_t rp_archive[MAX_PATH];
static char rp_identity[224];
static uint32_t rp_root_rng[4];

static wchar_t rp_test_file[MAX_PATH];
static void rp_test_keys(void *input) {
    if(!rp_viewer || !rp_test_file[0] || input!=(void *)0x4ca210)return;
    HANDLE f=CreateFileW(rp_test_file,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);char text[24]={0};DWORD got=0;
    if(f!=INVALID_HANDLE_VALUE){ReadFile(f,text,23,&got,NULL);CloseHandle(f);}
    FIELD(input,0)=(uint32_t)strtoul(text,NULL,10)&0x80dfb;
}
static uint32_t rp_hash(const void *bytes,size_t length) {
    uint32_t h=2166136261u;const unsigned char *p=bytes;
    for(size_t i=0;i<length;++i)h=(h^p[i])*16777619u;return h;
}
static void rp_fail(const char *why) {
    if(!rp_invalid)log_line("REPLAY_ERROR %s stage=%d frame=%lu",why,rp_stage,rp_manager?(unsigned long)FIELD(rp_manager,0x20c):0);
    rp_invalid=1;if(rp_active){rp_stop=1;
        wchar_t path[MAX_PATH];if(*(char *)0x568c61 && MultiByteToWideChar(data_profile_encoding,0,(char *)0x568c61,-1,path,MAX_PATH) && wcslen(path)+20<MAX_PATH){wcscat(path,L"\\replay-error.txt");HANDLE f=CreateFileW(path,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);if(f!=INVALID_HANDLE_VALUE){DWORD n;WriteFile(f,why,(DWORD)strlen(why),&n,NULL);CloseHandle(f);}}
    }
}
static void rp_clear(void) {
    for(unsigned i=0;i<RP_STAGES;++i){free(rp_stages[i].frames);memset(&rp_stages[i],0,sizeof(RpStage));for(unsigned j=0;j<3;++j)rp_stages[i].header.birth[j]=UINT32_MAX;}
    rp_recording=rp_active=rp_invalid=0;rp_stage=-1;rp_manager=NULL;
}
static int rp_filename(wchar_t *path,const char *name) {
    size_t n=name?strlen(name):0;if(!n || n>100)return 0;
    for(size_t i=0;i<n;++i){char c=name[i];if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='-'||c=='.'))return 0;}
    if(strstr(name,".."))return 0;
    wchar_t profile[MAX_PATH],file[104];
    if(!MultiByteToWideChar(data_profile_encoding,0,(char *)0x568c61,-1,profile,MAX_PATH) || !MultiByteToWideChar(data_profile_encoding,0,name,-1,file,104))return 0;
    return swprintf(path,MAX_PATH,L"%ls\\replay\\%ls",profile,file)>0 && wcslen(profile)+wcslen(file)+9<MAX_PATH;
}
static void rp_state(uint32_t out[3][22]) {
    memset(out,0,3*22*sizeof(uint32_t));
    for(unsigned id=0;id<party_count();++id){uint32_t *v=out[id];void *s=ship(id),*a=id?ability(id):PTR(0x4cf298);v[0]=s!=NULL;
        if(s){v[1]=FIELD(s,0x62c);v[2]=FIELD(s,0x630);v[3]=FIELD(s,0x476ac);v[4]=FIELD(s,0x47778);}
        memcpy(v+5,resource(id,0),0x30);v[17]=id?guests[id-1].funds:U32(0x4ccd34);v[18]=partner[id].ghost;v[19]=a?FIELD(a,0x28):0;v[20]=UINT32_MAX;
        if(a){void *selected=(void *)(uintptr_t)FIELD(a,0x38);if(selected)v[20]=FIELD(selected,4);uint32_t h=2166136261u;
            for(void *node=(void *)(uintptr_t)FIELD(a,0x1c);node;node=(void *)(uintptr_t)FIELD(node,4)){
                void *c=(void *)(uintptr_t)FIELD(node,0);if(!c)break;h=online_hash_word(h,FIELD(c,4));h=online_hash_word(h,FIELD(c,0x24));h=online_hash_word(h,FIELD(c,0x38));h=online_hash_word(h,FIELD(c,0x48));}
            v[21]=h;
        }
    }
}
static void rp_world(uint32_t out[3]) {
    out[0]=online_hash_enemies(2166136261u,PTR(0x4cf2d0));out[1]=online_hash_bullets(2166136261u,PTR(0x4cf2bc));out[2]=online_hash_items(2166136261u,PTR(0x4cf2ec));
}
static void rp_capture_seat(RpSeat *out,unsigned id) {
    memset(out,0,sizeof(*out));void *s=ship(id),*a=id?ability(id):PTR(0x4cf298);
    out->character=id?guests[id-1].character:U32(0x4cccf4);out->funds=id?guests[id-1].funds:U32(0x4ccd34);
    out->previous=id?guests[id-1].previous_input:U32(0x4ca42c);out->base_frame=(uint32_t)frames;memcpy(out->rng_after,(void *)0x4cf280,16);
    memcpy(out->resources,resource(id,0),0x30);out->partner=partner[id];
    if(s){memcpy(out->position,OFF(s,0x620),24);out->state=FIELD(s,0x476ac);out->focus=FIELD(s,0x476cc);memcpy(out->invincible,OFF(s,0x47774),12);memcpy(out->death_timer,OFF(s,0x634),12);}
    void *shot=s?(void *)(uintptr_t)FIELD(s,0x47940):NULL;out->shot_hash=shot?rp_hash(shot,0x30):0;
    out->selected=UINT32_MAX;
    if(!a)return;void *selected=(void *)(uintptr_t)FIELD(a,0x38);if(selected)out->selected=FIELD(selected,4);
    for(void *node=(void *)(uintptr_t)FIELD(a,0x1c);node && out->card_count<256;node=(void *)(uintptr_t)FIELD(node,4)) {
        void *card=(void *)(uintptr_t)FIELD(node,0);if(!card)continue;
        RpCard *c=&out->cards[out->card_count++];c->id=FIELD(card,4);
        void *v=(void *)(uintptr_t)FIELD(card,0);c->data=((uint32_t (TC *)(void *))(uintptr_t)FIELD(v,0x3c))(card);
        memcpy(c->timer,OFF(card,0x20),12);c->timer[3]=FIELD(card,0x30);memcpy(c->timer+4,OFF(card,0x34),12);c->timer[7]=FIELD(card,0x44);
    }
}
static void rp_restore_seat(const RpSeat *in,unsigned id,int rebuild) {
    unsigned old=guest_slot;if(id){guest_slot=id-1;enter_p2(0);}
    void *s=ship(id),*a=id?ability(id):PTR(0x4cf298);
    if(a && rebuild){((void (TC *)(void *,int))0x407da0)(a,0);for(unsigned i=0;i<in->card_count;++i)((int (TC *)(void *,int,int))0x411460)(a,in->cards[i].id,1);}
    unsigned index=0;
    for(void *node=a?(void *)(uintptr_t)FIELD(a,0x1c):NULL;node && index<in->card_count;node=(void *)(uintptr_t)FIELD(node,4)) {
        void *card=(void *)(uintptr_t)FIELD(node,0);if(!card)continue;const RpCard *c=&in->cards[index++];
        if(FIELD(card,4)!=c->id){rp_fail("inventory order");break;}
        void *v=(void *)(uintptr_t)FIELD(card,0);((void (TC *)(void *,uint32_t))(uintptr_t)FIELD(v,0x38))(card,c->data);
        memcpy(OFF(card,0x20),c->timer,12);FIELD(card,0x30)=c->timer[3];memcpy(OFF(card,0x34),c->timer+4,12);FIELD(card,0x44)=c->timer[7];
    }
    if(a && in->selected!=UINT32_MAX)((void (TC *)(void *,int))0x408b00)(a,(int)in->selected);
    if(s && rebuild){memcpy((void *)0x4ccd38,in->resources,0x30);U32(0x4ccd34)=in->funds;((void (__cdecl *)(void))0x45abb0)();}
    if(s){memcpy(OFF(s,0x620),in->position,24);FIELD(s,0x476ac)=in->state;FIELD(s,0x476cc)=in->focus;memcpy(OFF(s,0x47774),in->invincible,12);memcpy(OFF(s,0x634),in->death_timer,12);}
    memcpy((void *)0x4ccd38,in->resources,0x30);U32(0x4ccd34)=in->funds;
    partner[id]=in->partner;
    if(id){previous_p2=in->previous;p2_character=in->character;leave_p2(0);}else frames=(LONG)in->base_frame;
    guest_slot=old;
}
static RpStage *rp_current(void) {
    if(!rp_manager || rp_invalid)return NULL;
    int stage=(int)FIELD(rp_manager,0x214);if(stage<0 || stage>=RP_STAGES)return NULL;rp_stage=stage;return &rp_stages[stage];
}
static void rp_guest_created(unsigned id) {
    RpStage *stage=rp_current();if(!stage || (!rp_recording && !rp_active))return;
    uint32_t frame=FIELD(rp_manager,0x20c);
    if(rp_recording){stage->header.birth[id]=frame;rp_capture_seat(&stage->header.seats[id],id);}
    else {rp_restore_seat(&stage->header.seats[id],id,1);void *shot=(void *)(uintptr_t)FIELD(ship(id),0x47940);if(!shot || rp_hash(shot,0x30)!=stage->header.seats[id].shot_hash)rp_fail("character shot data");memcpy((void *)0x4cf280,stage->header.seats[id].rng_after,16);}
    log_line("REPLAY_GUEST P%u stage=%d birth=%lu mode=%s",id+1,rp_stage,(unsigned long)frame,rp_active?"play":"record");
}
static int rp_guest_due(unsigned id) {
    if(!rp_active)return 1;RpStage *stage=rp_current();return stage && FIELD(rp_manager,0x20c)>=stage->header.birth[id];
}
static void __cdecl rp_stage_cards(void) {
    if(rp_viewer && rp_active && rp_manager && FIELD(rp_manager,0xc)==1){
        unsigned stage=U32(0x4cccdc);
        if(stage>=RP_STAGES || !FIELD(rp_manager,0xd4+stage*40)){rp_fail("native stage checkpoint missing");return;}
    }
    ((void (__cdecl *)(void))rp_stage_original)();
    RpStage *stage=rp_current();if(!stage)return;
    /* Native builds the ECL and its enemies immediately after this call,
       before frame zero. Preserve the complete generator, not only the
       single 16-bit seed in vanilla's checkpoint. */
    if(rp_recording)memcpy(stage->header.initial_rng,(void *)0x4cf280,16);
    else if(rp_active){memcpy((void *)0x4cf280,stage->header.initial_rng,16);
        for(unsigned id=1;id<rp_players;++id)guests[id-1].selected_character=stage->header.seats[id].character;
    }
}
static int TC rp_record_tick(void *manager) {
    int before=(int)FIELD(manager,0x20c);int result=((Tick)rp_record_original)(manager);
    if(manager!=rp_manager || !rp_recording || rp_invalid || before<0 || FIELD(manager,0x20c)!=(uint32_t)before+1)return result;
    RpStage *stage=rp_current();if(!stage)return result;
    if(!before){stage->header.count=0;for(unsigned i=0;i<3;++i)stage->header.birth[i]=UINT32_MAX;
        for(unsigned id=0;id<party_count();++id)if(ship(id)){stage->header.birth[id]=0;rp_capture_seat(&stage->header.seats[id],id);}}
    if((uint32_t)before!=stage->header.count || stage->header.count>=RP_MAX_FRAMES){rp_fail("record boundary");return result;}
    if(stage->header.count==stage->capacity){uint32_t cap=stage->capacity?stage->capacity*2:2048;if(cap>RP_MAX_FRAMES)cap=RP_MAX_FRAMES;
        void *next=realloc(stage->frames,cap*sizeof(RpFrame));if(!next){rp_fail("record allocation");return result;}stage->frames=next;stage->capacity=cap;}
    RpFrame *f=&stage->frames[stage->header.count++];f->keys[0]=input_latch.p1;f->keys[1]=input_latch.p2;f->keys[2]=input_latch.p3;memcpy(f->rng,rp_root_rng,16);
    uint32_t state[3][22];rp_state(state);f->digest=rp_hash(state,sizeof(state));memset(f->world,0,sizeof(f->world));if(before%60==0)rp_world(f->world);
    return result;
}
static int TC rp_play_tick(void *manager) {
    int before=(int)FIELD(manager,0x20c);int result=((Tick)rp_play_original)(manager);
    if(manager!=rp_manager || !rp_active || rp_invalid || before<0 || FIELD(manager,0x20c)!=(uint32_t)before+1)return result;
    RpStage *stage=rp_current();if(!stage)return result;
    if((uint32_t)before>=stage->header.count){if((int)FIELD(manager,0x214)>=0)rp_fail("input exhausted");return result;}
    if(!before){memset(partner,0,sizeof(partner));wipe_frames=team_over=protected_bombs=0;
        for(unsigned id=0;id<party_count();++id)if(ship(id) && stage->header.birth[id]==0)rp_restore_seat(&stage->header.seats[id],id,id!=0);}
    RpFrame *f=&stage->frames[before];coop_input_capture(&input_latch,f->keys[0],f->keys[1]);input_latch.p3=f->keys[2];
    uint32_t state[3][22];rp_state(state);uint32_t actual=rp_hash(state,sizeof(state));
    if(before>3 && actual!=f->digest){log_line("REPLAY_DIGEST expected=%08lx actual=%08lx",(unsigned long)f->digest,(unsigned long)actual);rp_fail("player state diverged");}
    if(before>3 && before%60==0){uint32_t world[3];rp_world(world);for(unsigned i=0;i<3;++i)if(world[i]!=f->world[i]){log_line("REPLAY_WORLD frame=%d kind=%u expected=%08lx actual=%08lx",before,i,(unsigned long)f->world[i],(unsigned long)world[i]);rp_fail("world state diverged");}}
    if(before%600==0)log_line("REPLAY_PLAY stage=%d frame=%d digest=%08lx",rp_stage,before,(unsigned long)actual);
    return result;
}
static int rp_native_root(void *scheduler) {
    if(scheduler==PTR(0x4cf294) && !FIELD(scheduler,0x54)) {
        if(rp_active && !rp_invalid && rp_manager && !PTR(0x4cf2a4)) {
            RpStage *stage=rp_current();int frame=(int)FIELD(rp_manager,0x20c);
            if(stage && frame>=0 && (uint32_t)frame<stage->header.count)memcpy((void *)0x4cf280,stage->frames[frame].rng,16);
        }
        memcpy(rp_root_rng,(void *)0x4cf280,16);
    }
    return ((Tick)online_tick_original)(scheduler);
}
static unsigned char *rp_encode(uint32_t *size) {
    RpHeader head={{'T','1','8','C','O','O','P','1'},1,party_count(),sizeof(RpStageHeader),sizeof(RpFrame),{0},{0}};
    memcpy(head.identity,rp_identity,sizeof(head.identity));uint32_t total=sizeof(head);
    for(unsigned i=0;i<RP_STAGES;++i)if(rp_stages[i].header.count){for(unsigned id=0;id<party_count();++id)if(rp_stages[i].header.birth[id]>rp_stages[i].header.count)return NULL;head.bytes[i]=sizeof(RpStageHeader)+rp_stages[i].header.count*sizeof(RpFrame);total+=head.bytes[i];}
    if(total>RP_MAX_BYTES || total==sizeof(head))return NULL;unsigned char *blob=malloc(total);if(!blob)return NULL;
    memcpy(blob,&head,sizeof(head));unsigned char *at=blob+sizeof(head);
    for(unsigned i=0;i<RP_STAGES;++i)if(head.bytes[i]){memcpy(at,&rp_stages[i].header,sizeof(RpStageHeader));at+=sizeof(RpStageHeader);memcpy(at,rp_stages[i].frames,rp_stages[i].header.count*sizeof(RpFrame));at+=rp_stages[i].header.count*sizeof(RpFrame);}
    *size=total;return blob;
}
static int rp_decode(const unsigned char *blob,uint32_t size) {
    if(size<sizeof(RpHeader))return 0;const RpHeader *h=(const RpHeader *)blob;
    if(memcmp(h->magic,"T18COOP1",8)||h->format!=1||h->players<2||h->players>3||h->stage_size!=sizeof(RpStageHeader)||h->frame_size!=sizeof(RpFrame)||memcmp(h->identity,rp_identity,sizeof(h->identity)))return 0;
    uint32_t offset=sizeof(*h);unsigned stages=0;
    for(unsigned i=0;i<RP_STAGES;++i)if(h->bytes[i]) {
        if(h->bytes[i]<sizeof(RpStageHeader) || h->bytes[i]>size-offset)return 0;
        const RpStageHeader *s=(const RpStageHeader *)(blob+offset);
        if(!s->count || s->count>RP_MAX_FRAMES || h->bytes[i]!=sizeof(*s)+s->count*sizeof(RpFrame))return 0;
        for(unsigned id=0;id<h->players;++id) {
            const RpSeat *seat=&s->seats[id];
            if(s->birth[id]>s->count || seat->character>3 || seat->card_count>256 || seat->state>4 || seat->partner.ghost<0 || seat->partner.ghost>1)return 0;
            int found=seat->selected==UINT32_MAX;
            for(unsigned card=0;card<seat->card_count;++card){if(seat->cards[card].id>56)return 0;if(seat->cards[card].id==seat->selected)found=1;}
            if(!found)return 0;
        }
        offset+=h->bytes[i];++stages;
    }
    if(offset!=size || !stages)return 0;
    offset=sizeof(*h);
    for(unsigned i=0;i<RP_STAGES;++i)if(h->bytes[i]) {
        RpStage *s=&rp_stages[i];memcpy(&s->header,blob+offset,sizeof(s->header));s->frames=malloc(s->header.count*sizeof(RpFrame));if(!s->frames)return 0;
        s->capacity=s->header.count;memcpy(s->frames,blob+offset+sizeof(s->header),s->header.count*sizeof(RpFrame));offset+=h->bytes[i];
    }
    rp_players=h->players;return 1;
}
static int rp_load(const char *name) {
    wchar_t path[MAX_PATH];if(!rp_filename(path,name))return 0;
    HANDLE f=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);if(f==INVALID_HANDLE_VALUE)return 0;
    DWORD size=GetFileSize(f,NULL),got;RpFooter footer;int ok=0;
    if(size!=INVALID_FILE_SIZE && size<RP_MAX_BYTES && size>sizeof(footer)+sizeof(RpHeader)+36) {
        SetFilePointer(f,-(LONG)sizeof(footer),NULL,FILE_END);
        if(ReadFile(f,&footer,sizeof(footer),&got,NULL) && got==sizeof(footer) && !memcmp(footer.magic,"T18COOP1",8) && footer.size>=sizeof(RpHeader) && footer.size<size-sizeof(footer)-36) {
            unsigned char *blob=malloc(footer.size);
            if(blob){SetFilePointer(f,size-sizeof(footer)-footer.size,NULL,FILE_BEGIN);
                if(ReadFile(f,blob,footer.size,&got,NULL)&&got==footer.size&&rp_hash(blob,footer.size)==footer.checksum)ok=rp_decode(blob,footer.size);free(blob);}
        }
    }
    CloseHandle(f);return ok;
}
static void rp_append(const char *name) {
    if(rp_invalid || !rp_recording || !rp_identity[0])return;
    wchar_t path[MAX_PATH];if(!rp_filename(path,name))return;uint32_t size;unsigned char *blob=rp_encode(&size);if(!blob)return;
    HANDLE f=CreateFileW(path,GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);int ok=0;DWORD got;
    if(f!=INVALID_HANDLE_VALUE){uint32_t header[9];DWORD length=GetFileSize(f,NULL);
        if(ReadFile(f,header,sizeof(header),&got,NULL)&&got==sizeof(header)&&header[0]==0x72383174&&header[3]>=36&&header[3]<=length) {
            SetFilePointer(f,header[3],NULL,FILE_BEGIN);SetEndOfFile(f);RpFooter foot={{'T','1','8','C','O','O','P','1'},size,rp_hash(blob,size)};
            ok=WriteFile(f,blob,size,&got,NULL)&&got==size&&WriteFile(f,&foot,sizeof(foot),&got,NULL)&&got==sizeof(foot);FlushFileBuffers(f);
        }CloseHandle(f);}
    free(blob);
    if(ok && rp_archive[0]){wchar_t target[MAX_PATH];SYSTEMTIME now;GetLocalTime(&now);CreateDirectoryW(rp_archive,NULL);
        int n=swprintf(target,MAX_PATH,L"%ls\\%04u%02u%02u-%02u%02u%02u-%lu-%lu.rpy",rp_archive,now.wYear,now.wMonth,now.wDay,now.wHour,now.wMinute,now.wSecond,(unsigned long)GetCurrentProcessId(),(unsigned long)GetTickCount());
        if(n>0 && n<MAX_PATH && CopyFileW(path,target,TRUE))log_line("REPLAY_SAVED native=%s players=%u bytes=%lu",name,party_count(),(unsigned long)size);
    }
}
static int __stdcall rp_save(const char *name,const char *nickname,uint32_t unused,uint32_t finish) {
    int result=((int (__stdcall *)(const char *,const char *,uint32_t,uint32_t))rp_save_original)(name,nickname,unused,finish);
    if(!result)rp_append(name);return result;
}
static void TC rp_destroy(void *manager) {
    if(manager==rp_manager && rp_recording && !rp_invalid && (!online_map || online_map->role==0)) {
        int any=0;for(unsigned i=0;i<RP_STAGES;++i)any|=rp_stages[i].header.count!=0;
        if(any){char file[48];snprintf(file,sizeof(file),"th18_udc%03lu.rpy",(unsigned long)(GetTickCount()%1000));rp_save(file,"COOP",0,1);}
    }
    ((void (TC *)(void *))rp_destroy_original)(manager);
    if(manager==rp_manager)rp_clear();
}
static int TC rp_initialize(void *manager,int mode,const char *name) {
    if(mode==0 && rp_viewer){rp_fail("normal play disabled in viewer");return -1;}
    if(mode==0){rp_clear();rp_manager=manager;rp_recording=1;rp_players=party_count();}
    if(mode==1 && rp_viewer){rp_clear();if(!rp_load(name)){rp_stop=rp_invalid=1;log_line("REPLAY_ERROR incompatible multiplayer extension");return -1;}rp_manager=manager;rp_active=1;
        for(unsigned id=1;id<rp_players;++id){guests[id-1].selected_character=-1;guests[id-1].cards_mode=1;guests[id-1].loadout_count=0;guests[id-1].have_resources=0;guests[id-1].cards_cloned=0;}
        log_line("REPLAY_OPEN native=%s players=%u",name,rp_players);
    }
    int result=((int (TC *)(void *,int,const char *))rp_init_original)(manager,mode,name);
    if(mode==1 && rp_active){RpStage *s=rp_current();if(s)for(unsigned id=1;id<rp_players;++id)guests[id-1].selected_character=s->header.seats[id].character;requested=wanted=1;}
    return result;
}
static int rp_menu(void *menu) {
    if(!rp_viewer)return 0;
    unsigned phase=FIELD(menu,0x18);
    if(phase==1){if(rp_menu_seen){rp_stop=1;return 1;}((void (TC *)(void *,int))0x4646e0)(menu,12);rp_menu_seen=1;}
    else if(phase!=0 && phase!=2 && phase!=12 && phase!=9 && phase!=13){rp_stop=1;return 1;}
    return 0;
}
static int rp_root_wait(void *scheduler) {
    if(!rp_viewer || FIELD(scheduler,0x54))return 0;
    if(rp_stop)return -1;
    if(rp_active && p1) {
        if(!online_load_ready())return 1;
        RpStage *stage=rp_current();if(stage)for(unsigned id=1;id<party_count();++id)if(!ship(id)) {
            uint32_t frame=FIELD(rp_manager,0x20c);if(frame+1<stage->header.birth[id])continue;
            guests[id-1].selected_character=stage->header.seats[id].character;
            unsigned old=guest_slot;guest_slot=id-1;int ready=guest_asset_ready(PTR(0x51f65c),stage->header.seats[id].character);guest_slot=old;
            if(!ready){online_assets_ready();((int (TC *)(void *))0x476d20)((void *)0x56ad80);return 1;}
            /* A carried teammate exists before frame0 in the recording. Create
               it before the Replay callback, not after frame0 in host_tick. */
            if(!frame && !stage->header.birth[id]){old=guest_slot;guest_slot=id-1;create_guest();guest_slot=old;if(!ship(id))return 1;}
        }
    }
    return 0;
}
static int rp_install(void) {
    char flag[8];rp_viewer=GetEnvironmentVariableA("TH18_COOP_REPLAY_VIEWER",flag,sizeof(flag))==1 && flag[0]=='1';
    GetEnvironmentVariableA("TH18_COOP_REPLAY_IDENTITY",rp_identity,sizeof(rp_identity));
    if(GetEnvironmentVariableA("TH18_COOP_ROLLBACK_TEST",flag,sizeof(flag))==1 && flag[0]=='1') {
        if(rp_viewer)GetEnvironmentVariableW(L"TH18_COOP_REPLAY_TEST_INPUT",rp_test_file,MAX_PATH);
        else if(online_map && (online_map->reserved[1]&1) && GetEnvironmentVariableA("TH18_COOP_REPLAY_START_STAGE",flag,sizeof(flag))==1 && flag[0]>='2' && flag[0]<='7')rp_test_start_stage=(unsigned)(flag[0]-'0');
    }
    wchar_t game[MAX_PATH];GetModuleFileNameW(NULL,game,MAX_PATH);wchar_t *slash=wcsrchr(game,L'\\');if(slash){*slash=0;if(wcslen(game)+18<MAX_PATH)swprintf(rp_archive,MAX_PATH,L"%ls\\coop-data\\replay",game);}
    const unsigned char init[]={0x55,0x8b,0xec,0x51,0x8b,0x45,0x08},destroy[]={0x55,0x8b,0xec,0x6a,0xff,0x68,0xb0,0xc3,0x4a,0x00};
    const unsigned char save[]={0x55,0x8b,0xec,0x81,0xec,0x2c,0x01,0x00,0x00},tick[]={0x83,0x3d,0xe4,0xf2,0x4c,0x00,0x00};
    const unsigned char stage[]={0x53,0x8b,0xdc,0x83,0xec,0x08};
    rp_init_original=trampoline(0x4615e0,sizeof(init),init);rp_destroy_original=trampoline(0x461ad0,sizeof(destroy),destroy);rp_save_original=trampoline(0x461e90,sizeof(save),save);
    rp_record_original=trampoline(0x462940,sizeof(tick),tick);rp_play_original=trampoline(0x462a50,sizeof(tick),tick);
    rp_stage_original=trampoline(0x462d20,sizeof(stage),stage);
    if(!rp_init_original||!rp_destroy_original||!rp_save_original||!rp_record_original||!rp_play_original||!rp_stage_original)return 0;
    return patch_branch(0x4615e0,rp_initialize,sizeof(init),0xe9,init)&&patch_branch(0x461ad0,rp_destroy,sizeof(destroy),0xe9,destroy)&&patch_branch(0x461e90,rp_save,sizeof(save),0xe9,save)&&patch_branch(0x462940,rp_record_tick,sizeof(tick),0xe9,tick)&&patch_branch(0x462a50,rp_play_tick,sizeof(tick),0xe9,tick)&&patch_branch(0x462d20,rp_stage_cards,sizeof(stage),0xe9,stage);
}
#endif
