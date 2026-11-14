#ifndef TH18_NATIVE_WORLD_DIGEST_H
#define TH18_NATIVE_WORLD_DIGEST_H
/* Address-independent native battle checks. These are semantic sentinels;
   restoration still uses the full owned-memory history, not this digest. */
static uint32_t online_hash_bullets(uint32_t h,void *manager){
    if(!valid(manager,0x7a41f8))return online_hash_word(h,0);
    h=online_hash_word(h,1);h=online_hash_word(h,FIELD(manager,0xa8));
    uintptr_t begin=(uintptr_t)manager+0xec,end=begin+2001u*0xfa0u;
    void *node=(void *)(uintptr_t)FIELD(manager,0xc0);unsigned count=0;
    /* Native424c50 traverses the active intrusive list at+c0;423e10 and
       424eb0 consume the timer, flags and position fields below. */
    while(node && count<2001){uintptr_t n=(uintptr_t)node;
        if(n<begin || n>end-12)return online_hash_word(h,UINT32_MAX);
        void *bullet=(void *)(uintptr_t)FIELD(node,0);uintptr_t b=(uintptr_t)bullet;
        if(b<begin || b>end-0xfa0 || (b-begin)%0xfa0)return online_hash_word(h,UINT32_MAX);
        h=online_hash_word(h,(uint32_t)((b-begin)/0xfa0));
        const unsigned offsets[]={0x20,0x638,0x63c,0x640,0x654,0x684,0xf68,0xf70,0xf84,0xf88};
        for(unsigned i=0;i<sizeof(offsets)/sizeof(offsets[0]);++i)h=online_hash_word(h,FIELD(bullet,offsets[i]));
        ++count;node=(void *)(uintptr_t)FIELD(node,4);
    }
    h=online_hash_word(h,node?UINT32_MAX:count);return h;
}
static uint32_t online_hash_enemies(uint32_t h,void *manager){
    if(!valid(manager,0x2cc))return online_hash_word(h,0);h=online_hash_word(h,1);
    for(unsigned i=0;i<16;++i)h=online_hash_word(h,FIELD(manager,0x48+i*4));
    void *node=(void *)(uintptr_t)FIELD(manager,0x18c);unsigned count=0;
    /* Native43c0f3 searches this ECL list by+6830, then reads current/max
       HP at6220/6224 and stage/attack flags at6320/635c. Never hash pointers. */
    while(node && count<1024){
        if(!valid(node,8))return online_hash_word(h,UINT32_MAX);
        void *script=(void *)(uintptr_t)FIELD(node,0);if(!valid(script,0x6834))return online_hash_word(h,UINT32_MAX);
        const unsigned offsets[]={0x6830,0x6220,0x6224,0x6320,0x635c};
        for(unsigned i=0;i<sizeof(offsets)/sizeof(offsets[0]);++i)h=online_hash_word(h,FIELD(script,offsets[i]));
        ++count;node=(void *)(uintptr_t)FIELD(node,4);
    }return online_hash_word(h,node?UINT32_MAX:count);
}
static uint32_t online_hash_items(uint32_t h,void *manager){
    if(!valid(manager,15121192))return online_hash_word(h,0);h=online_hash_word(h,1);unsigned count=0;
    for(unsigned i=0;i<0x1258;++i){void *item=OFF(manager,0x14+i*0xc94);uint32_t state=FIELD(item,0xc74);if(!state)continue;
        h=online_hash_word(h,i);h=online_hash_word(h,state);
        const unsigned offsets[]={0xc2c,0xc30,0xc50,0xc78,0xc7c,0xc80};
        for(unsigned n=0;n<sizeof(offsets)/sizeof(offsets[0]);++n)h=online_hash_word(h,FIELD(item,offsets[n]));++count;
    }return online_hash_word(h,count);
}
#endif
