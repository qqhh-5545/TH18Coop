#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../src/th14/progress14.h"
static unsigned checks;
#define CHECK(x) do {++checks;if(!(x)){fprintf(stderr,"FAIL line %u: %s\n",__LINE__,#x);return 1;}} while(0)
int main(void) {
    unsigned char records[14]={0},before[14];
    unsigned characters[3]={0,1,2},styles[3]={0,1,0},stages[3]={6,6,6};
    records[4]=0x80;progress14_credit(records,characters,styles,stages,2,1,1);
    CHECK(records[1]==0x11 && records[4]==0x91);
    for(unsigned i=0;i<14;++i)if(i!=1 && i!=4)CHECK(records[i]==0);
    memcpy(before,records,sizeof(records));progress14_credit(records,characters,styles,stages,2,1,1);CHECK(!memcmp(before,records,sizeof(records)));
    memset(records,0,sizeof(records));progress14_credit(records,characters,styles,stages,3,0,1);
    CHECK(records[1]==1 && records[4]==1 && records[5]==1); /* Easy never grants Normal completion. */
    memset(records,0,sizeof(records));stages[1]=5;characters[2]=3;
    progress14_credit(records,characters,styles,stages,3,3,1);CHECK(records[1]==0x11 && records[4]==0 && records[7]==0);
    memcpy(before,records,sizeof(records));
    for(unsigned ending=7;ending<14;++ending)progress14_credit(records,characters,styles,stages,3,1,ending);
    progress14_credit(records,characters,styles,stages,3,1,0);progress14_credit(records,characters,styles,stages,4,1,1);
    CHECK(!memcmp(before,records,sizeof(records))); /* No continued endings, invalid roster, or unrelated records. */
    void *score=calloc(1,0x24254);CHECK(score);characters[0]=0;characters[1]=1;characters[2]=1;styles[2]=1;stages[1]=stages[2]=6;
    *progress14_counter(score,0,1,0x7e4)=1;
    progress14_counts(score,characters,styles,stages,3,1,6,0x7e4,0);
    CHECK(*progress14_counter(score,0,1,0x7e4)==1 && *progress14_counter(score,3,1,0x7e4)==1); /* Same guest loadout credited once. */
    CHECK(*progress14_counter(score,3,0,0x7e4)==0 && *progress14_counter(score,2,1,0x7e4)==0);
    progress14_counts(score,characters,styles,stages,3,1,6,0x7e4,1);
    CHECK(*progress14_counter(score,3,1,0x7e4)==1); /* No native increment means no invented clear. */
    stages[1]=stages[2]=7;*progress14_counter(score,0,4,0x7e4)=1;
    progress14_counts(score,characters,styles,stages,3,4,7,0x7e4,0);CHECK(*progress14_counter(score,3,4,0x7e4)==1);
    *progress14_counter(score,0,1,0x7c8)=1;stages[1]=stages[2]=6;
    progress14_counts(score,characters,styles,stages,3,1,6,0x7c8,0);CHECK(*progress14_counter(score,3,1,0x7c8)==1);
    free(score);printf("PASS TH14 progressive completion: %u assertions\n",checks);return 0;
}
