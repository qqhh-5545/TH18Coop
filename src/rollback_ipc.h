#ifndef TH18_ROLLBACK_IPC_H
#define TH18_ROLLBACK_IPC_H
#define RB_PIPE_MAGIC 0x54485233u
#define RB_PIPE_SLOTS 64u
#define RB_PIPE_PAYLOAD 1024u
typedef struct {uint32_t peer,size;unsigned char data[RB_PIPE_PAYLOAD];} RbPipePacket;
typedef struct {
    uint32_t magic;volatile LONG status;uint32_t role,players,session[2],flags;
    volatile uint32_t frame,confirmed,corrections,replayed,waits,error;
    volatile uint32_t end_request;uint32_t padding[2];
    volatile LONG out_write,out_read,in_write,in_read;uint32_t padding2[12];
    RbPipePacket outgoing[RB_PIPE_SLOTS],incoming[RB_PIPE_SLOTS];
} RbPipe;
_Static_assert(offsetof(RbPipe,out_write)==64,"rollback ring counters");
_Static_assert(offsetof(RbPipe,outgoing)==128,"rollback ring start");
_Static_assert(sizeof(RbPipePacket)==1032,"rollback ring slot");
static RbPipe *rb_pipe;static HANDLE rb_pipe_mapping;
static int rb_pipe_send(unsigned peer,const void *bytes,unsigned size) {
    if(!rb_pipe || !size || size>RB_PIPE_PAYLOAD || peer>=party_count() || peer==online_map->role)return 0;
    unsigned write=(unsigned)rb_pipe->out_write,read=(unsigned)rb_pipe->out_read;
    if(write-read>=RB_PIPE_SLOTS)return 0;
    RbPipePacket *p=&rb_pipe->outgoing[write%RB_PIPE_SLOTS];p->peer=peer;p->size=size;memcpy(p->data,bytes,size);
    MemoryBarrier();InterlockedExchange(&rb_pipe->out_write,(LONG)(write+1));return 1;
}
static int rb_pipe_open(const wchar_t *token) {
    wchar_t name[100];swprintf(name,100,L"Local\\TH18CoopRollback-%ls",token);
    rb_pipe_mapping=OpenFileMappingW(FILE_MAP_ALL_ACCESS,FALSE,name);if(!rb_pipe_mapping)return 0;
    rb_pipe=MapViewOfFile(rb_pipe_mapping,FILE_MAP_ALL_ACCESS,0,0,sizeof(*rb_pipe));
    return rb_pipe && rb_pipe->magic==RB_PIPE_MAGIC && rb_pipe->role==online_map->role && rb_pipe->players==party_count();
}
#endif
