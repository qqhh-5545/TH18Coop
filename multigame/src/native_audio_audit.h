#ifndef TH18_NATIVE_AUDIO_AUDIT_H
#define TH18_NATIVE_AUDIO_AUDIT_H
/* Native audio service drains SFX inside the root traversal. An isolated
   replay must retain its deterministic queue/pan changes without calling
   DirectSound twice. Music/stream commands are external-resource barriers.
   These are test-only substitutions, not the production confirmation queue. */
static struct {Tick original;uint32_t status[96];unsigned cursor,recorded,real,virtualised,blocked;unsigned char stop_status[16][96];} rb_audio;
static void rb_audio_diagnostic(uint32_t step) {
#ifdef COOP_NATIVE_FIXTURE
    (void)step;return; /* Mock addresses never own actual DirectSound buffers. */
#else
    static int initialised,enabled;static DWORD last;
    if(!online_map || !(online_map->reserved[1]&1))return;
    if(!initialised){wchar_t value[4];enabled=GetEnvironmentVariableW(L"TH18_COOP_AUDIO_DIAG",value,4)==1 && value[0]==L'1';initialised=1;}
    if(!enabled || GetTickCount()-last<1000)return;last=GetTickCount();
    void *music=PTR(0x570484),*array=NULL,*buffer=NULL;unsigned count=0;
    if(valid(music,0x24)){array=(void *)(uintptr_t)FIELD(music,8);count=FIELD(music,0x14);if(count>0 && count<=8 && valid(array,count*4))buffer=*(void **)array;}
    DWORD status=0,play=0,write=0,caps[5]={20,0,0,0,0};LONG volume=0,hr=-1;
    if(valid(buffer,4)){void **vtable=*(void ***)buffer;
        if(valid(vtable,10*sizeof(void *))){typedef LONG (WINAPI *Out)(void *,void *);typedef LONG (WINAPI *Position)(void *,DWORD *,DWORD *);
            hr=((Out)vtable[9])(buffer,&status);((Position)vtable[4])(buffer,&play,&write);((Out)vtable[6])(buffer,&volume);((Out)vtable[3])(buffer,caps);}}
    log_line("AUDIO_DIAG step=%lu mode=%lu cfg_music=%u cfg_a=%u cfg_b=%u volume=%lu/%lu foreground=%d music=%p buffers=%u buffer=%p hr=%lx status=%lx cursor=%lu/%lu dB=%ld caps=%lx bytes=%lu command=%lu",
        (unsigned long)step,(unsigned long)U32(0x4cccc8),*(unsigned char *)0x4cd00d,*(unsigned char *)0x4cd009,*(unsigned char *)0x4cd00a,
        (unsigned long)U32(0x5704b0),(unsigned long)U32(0x5704b4),GetForegroundWindow()==(HWND)(uintptr_t)U32(0x568c30),music,count,buffer,(unsigned long)hr,(unsigned long)status,(unsigned long)play,(unsigned long)write,(long)volume,(unsigned long)caps[1],(unsigned long)caps[2],(unsigned long)U32(0x56d204));
#endif
}
typedef struct {void *buffer;int volume;void *definition,*unused;int pan;uint32_t was_playing;} RbAudioParameter;
static void rb_audio_mark(RbAudioParameter *parameters,uint32_t *status,unsigned id,int stop) {
    if(stop)status[id]&=~5u;
    else{void *definition=parameters[id].definition;status[id]=(status[id]&~5u)|1u|((definition && (FIELD(definition,0xc)&1u))?4u:0u);}
}
static int rb_audio_drain(int32_t *ids,int32_t *counts,const int32_t *positions,RbAudioParameter *parameters,uint32_t *status) {
    for(unsigned i=0;i<12;++i){int id=ids[i];if(id<0)break;int count=counts[i];if(id>=96 || count>60)return 0;
        int64_t sum=0;if(count>=0)for(int n=0;n<count;++n)sum+=positions[i*128+n];if(sum<INT32_MIN || sum>INT32_MAX)return 0;
        ids[i]=-1;RbAudioParameter *p=&parameters[id];
        if(count<0){p->was_playing=0;if(p->buffer){p->was_playing=status[id]&1u;rb_audio_mark(parameters,status,(unsigned)id,1);}}
        else if(p->buffer){p->pan=count?(int)sum/count:0;rb_audio_mark(parameters,status,(unsigned)id,0);}
        counts[i]=0;
    }
    return 1;
}
static int rb_audio_checkpoint(void) {
    if(U32(0x56d204))return 0;
    rb_audio.cursor=rb_audio.recorded=0;
    memset(rb_audio.status,0,sizeof(rb_audio.status));
    if(!PTR(0x56ad8c))return 1;
    for(unsigned id=0;id<96;++id){void *buffer=PTR(0x56c804+id*24);if(!buffer)continue;
        void **vtable=*(void ***)buffer;typedef LONG (WINAPI *Status)(void *,DWORD *);
        DWORD status=0;if(((Status)vtable[9])(buffer,&status)<0 || (status&2u))return 0;rb_audio.status[id]=status;
    }
    return 1;
}
static int TC rb_audio_service(void *manager) {
    if(rb_runtime_virtual)return rb_runtime_audio(manager);
    if(!rb_tx.mode)return rb_audio.original(manager);
    if(!rb_replaying){
        /* Only the original accurate admitted step reaches real DirectSound.
           Remember its queue actions before the native function drains them. */
        int ids[12],stop[12];unsigned count=0;
        if(PTR(0x56ad8c) && *(unsigned char *)0x4cd00a)for(unsigned i=0;i<12;++i){int id=(int32_t)U32(0x56ad9c+i*4);if(id<0)break;if(id>=96){rb_tx.failed=1;break;}
            ids[count]=id;stop[count++]=(int32_t)U32(0x56adcc+i*4)<0;}
        unsigned event=rb_audio.cursor++;if(event>=16){rb_tx.failed=1;return 0;}
        memset(rb_audio.stop_status[event],2,96);int result=rb_audio.original(manager);++rb_audio.real;rb_audio.recorded=event+1;
        for(unsigned i=0;i<count;++i)if(stop[i] && PTR(0x56c804+ids[i]*24))rb_audio.stop_status[event][ids[i]]=(unsigned char)(U32(0x56c818+ids[i]*24)&1u);
        for(unsigned i=0;i<count;++i)if(PTR(0x56c804+ids[i]*24))rb_audio_mark((RbAudioParameter *)0x56c804,rb_audio.status,(unsigned)ids[i],stop[i]);return result;
    }
    if(U32(0x56d204)){++rb_audio.blocked;rb_tx.failed=1;return 0;}
    ++rb_audio.virtualised;
    unsigned event=rb_audio.cursor++;
    if(!PTR(0x56ad8c) || !*(unsigned char *)0x4cd00a)return 0;
    /* A real DirectSound GetStatus is an external, time-dependent read.
       Replay the first accurate step's recorded answers, not a later poll. */
    if(event<rb_audio.recorded)for(unsigned i=0;i<12;++i){int id=(int32_t)U32(0x56ad9c+i*4);if(id<0)break;if(id<96 && (int32_t)U32(0x56adcc+i*4)<0 && rb_audio.stop_status[event][id]<2)
        rb_audio.status[id]=(rb_audio.status[id]&~1u)|rb_audio.stop_status[event][id];}
    if(!rb_audio_drain((int32_t *)0x56ad9c,(int32_t *)0x56adcc,(int32_t *)0x56adfc,(RbAudioParameter *)0x56c804,rb_audio.status)){++rb_audio.blocked;rb_tx.failed=1;}
    return 0;
}
static int rb_audio_install(void) {
    const uintptr_t sites[]={0x443ca5,0x453499,0x453a79,0x458745,0x458ae5,0x45a3c5};
    for(unsigned i=0;i<6;++i){uintptr_t at=sites[i];if(*(unsigned char *)at!=0xe8 || at+5+*(int32_t *)(at+1)!=0x476d20)return 0;}
    rb_audio.original=(Tick)0x476d20;unsigned installed=0;
    for(;installed<6;++installed){uintptr_t at=sites[installed];DWORD old;if(!VirtualProtect((void *)at,5,PAGE_EXECUTE_READWRITE,&old))break;
        *(int32_t *)(at+1)=(int32_t)((uintptr_t)rb_audio_service-at-5);FlushInstructionCache(GetCurrentProcess(),(void *)at,5);VirtualProtect((void *)at,5,old,&old);}
    if(installed==6)return 1;
    while(installed){uintptr_t at=sites[--installed];DWORD old;if(VirtualProtect((void *)at,5,PAGE_EXECUTE_READWRITE,&old)){
        *(int32_t *)(at+1)=(int32_t)(0x476d20-at-5);FlushInstructionCache(GetCurrentProcess(),(void *)at,5);VirtualProtect((void *)at,5,old,&old);}}
    return 0;
}
#endif
