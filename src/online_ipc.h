#ifndef COOP_ONLINE_IPC_H
#define COOP_ONLINE_IPC_H
/* Schema 2: fixed 160-byte local-process bridge. Never sent as pointers. */
typedef struct {
    uint32_t magic,schema;
    volatile LONG status;
    uint32_t role,step,kind,local_input,digest,result,p1,p2;
    uint32_t rng[4],options,initial_seed,error,pid,completed;
    uint32_t reserved[12];
    uint32_t players,p3,peer_pids[3],reserved_tail[3];
} CoopOnlineMap;
_Static_assert(sizeof(CoopOnlineMap)==160,"online IPC layout");
#define COOP_ONLINE_MAGIC 0x54484e32u
#endif
