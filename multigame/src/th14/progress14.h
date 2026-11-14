#ifndef TH14_PROGRESS_H
#define TH14_PROGRESS_H
#include <stdint.h>
static uint32_t *progress14_counter(void *score,unsigned loadout,unsigned difficulty,unsigned base) {
    return (uint32_t *)((unsigned char *)score+base+loadout*0x5298+difficulty*4);
}
static void progress14_counts(void *score,const unsigned *characters,const unsigned *styles,const unsigned *stages,unsigned count,unsigned difficulty,unsigned stage,unsigned base,uint32_t host_before) {
    if(!score || !count || count>3 || difficulty>4 || (stage!=6 && stage!=7) || characters[0]>2 || styles[0]>1 || (base!=0x7c8 && base!=0x7e4))return;
    unsigned host=characters[0]*2+styles[0],credited=1u<<host;
    uint32_t after=*progress14_counter(score,host,difficulty,base);
    if(host_before>=99999 || after!=host_before+1)return;
    for(unsigned id=1;id<count;++id)if(stages[id]==stage && characters[id]<3 && styles[id]<2){
        unsigned loadout=characters[id]*2+styles[id];if(credited&(1u<<loadout))continue;credited|=1u<<loadout;
        uint32_t *counter=progress14_counter(score,loadout,difficulty,base);if(*counter<99999)++*counter;
    }
}
/* Native ending records 1..6: character*2 + weapon + 1.
   Bit 0 records the good ending; bit 4 records Normal-or-higher completion.
   Credit only the actual stage-six roster, never unplayed loadouts. */
static void progress14_credit(unsigned char *records,const unsigned *characters,const unsigned *styles,const unsigned *stages,unsigned count,unsigned difficulty,unsigned ending_index) {
    if(!records || ending_index<1 || ending_index>6 || count>3)return;
    unsigned char earned=(unsigned char)(1u|(difficulty>=1?0x10u:0u));
    for(unsigned id=0;id<count;++id)if(stages[id]==6 && characters[id]<3 && styles[id]<2)
        records[characters[id]*2+styles[id]+1]|=earned;
}
#endif
