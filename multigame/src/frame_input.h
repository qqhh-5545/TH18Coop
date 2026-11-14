#ifndef COOP_FRAME_INPUT_H
#define COOP_FRAME_INPUT_H
#include <stdint.h>
#include <string.h>
#define COOP_P2_INPUT_MASK 0xcfbu
#define COOP_TRACE_LIMIT (16u*1024u*1024u)
typedef struct {uint32_t sequence,p1,p2,stage,phase,owner,flags,p3,checksum;} CoopInputRecord;
typedef struct {char magic[8];uint32_t format,record_size;char build[16];} CoopInputHeader;
typedef struct {uint32_t sequence,p1,p2;int valid;uint32_t p3;} CoopInputLatch;
_Static_assert(sizeof(CoopInputHeader)==32,"input header size");
_Static_assert(sizeof(CoopInputRecord)==36,"input record size");
static uint32_t coop_input_checksum(const CoopInputRecord *record) {
    const unsigned char *bytes=(const unsigned char *)record;uint32_t result=2166136261u;
    for(unsigned i=0;i<sizeof(*record)-4;++i)result=(result^bytes[i])*16777619u;
    return result;
}
static void coop_input_capture(CoopInputLatch *latch,uint32_t host,uint32_t guest) {
    if(latch->valid)++latch->sequence;
    latch->p1=host;latch->p2=guest&COOP_P2_INPUT_MASK;latch->valid=1;
}
static CoopInputRecord coop_input_record(const CoopInputLatch *latch,uint32_t stage,uint32_t phase,uint32_t owner,uint32_t flags) {
    CoopInputRecord record={latch->sequence,latch->p1,latch->p2,stage,phase,owner,flags,latch->p3,0};
    record.checksum=coop_input_checksum(&record);return record;
}
static CoopInputHeader coop_input_header(const char *build) {
    CoopInputHeader header={{'T','H','1','8','I','N','P',0},2,36,{0}};
    strncpy(header.build,build,sizeof(header.build)-1);return header;
}
#endif
