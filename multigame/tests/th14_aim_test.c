#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#define PLAYER 0x104db67c
#define PTR(a) (*(void **)(uintptr_t)(a))
#define FLT(p,o) (*(float *)((unsigned char *)(p)+(o)))
static struct {void *player; int ghost; unsigned state;} seats[3];
static unsigned players=3,scope_depth;
static int living(unsigned id) {return seats[id].player && !seats[id].ghost && seats[id].state<=1;}
#include "../src/th14/aim14.h"
#define BRIDGE(name) void name(void) {}
BRIDGE(aim14_angle) BRIDGE(aim14_shoot) BRIDGE(aim14_spawn) BRIDGE(aim14_redirect)
BRIDGE(aim14_ecl_x) BRIDGE(aim14_ecl_y) BRIDGE(aim14_ecl_fx) BRIDGE(aim14_ecl_fy)
BRIDGE(aim14_ecl_distance) BRIDGE(aim14_ecl_fdistance) BRIDGE(aim14_damage_state)
static unsigned checks;
#define CHECK(x) do {++checks;if(!(x)){fprintf(stderr,"FAIL line %u: %s\n",__LINE__,#x);exit(1);}} while(0)
static void expect(const float *origin,unsigned output,void *expected) {
    uint32_t regs[8];for(unsigned i=0;i<8;++i)regs[i]=0xabcd0000+i;
    aim14_nearest(regs,origin,output);
    for(unsigned i=0;i<8;++i)CHECK(regs[i]==(i==output?(uint32_t)(uintptr_t)expected:0xabcd0000+i));
}
int main(void) {
    CHECK(VirtualAlloc((void *)0x104d0000,0x10000,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE)==(void *)0x104d0000);
    for(unsigned i=0;i<3;++i){seats[i].player=calloc(1,0x600);CHECK(seats[i].player);FLT(seats[i].player,0x5e0)=(float)i*100;FLT(seats[i].player,0x5e4)=100;}
    PTR(PLAYER)=seats[0].player;
    float origin[2]={190,100};
    for(unsigned out=0;out<8;++out)expect(origin,out,seats[2].player);
    seats[2].ghost=1;expect(origin,7,seats[1].player);
    seats[1].state=2;expect(origin,7,seats[0].player);
    seats[1].state=1;seats[2].ghost=0;seats[0].state=4;
    origin[0]=0;expect(origin,6,seats[1].player); /* Dead P1 is excluded. */
    seats[1].ghost=1;expect(origin,7,seats[2].player);
    seats[1].ghost=0;seats[0].state=0;origin[0]=50;expect(origin,7,seats[0].player); /* Stable seat-order ties. */
    players=2;origin[0]=200;expect(origin,7,seats[1].player);players=3;
    void *third=seats[2].player;seats[2].player=NULL;expect(origin,7,seats[1].player);seats[2].player=third;
    scope_depth=1;PTR(PLAYER)=seats[1].player;expect(origin,7,seats[1].player);scope_depth=0;PTR(PLAYER)=seats[0].player;
    expect(NULL,7,seats[0].player);origin[0]=NAN;expect(origin,7,seats[0].player);
    origin[0]=0;origin[1]=INFINITY;expect(origin,7,seats[0].player);
    origin[1]=100;for(unsigned i=0;i<3;++i)seats[i].ghost=1;expect(origin,7,seats[0].player); /* Native fallback when no living target exists. */
    for(unsigned i=0;i<3;++i)free(seats[i].player);
    CHECK(VirtualFree((void *)0x104d0000,0,MEM_RELEASE));
    printf("PASS TH14 aim selection: %u assertions (no game process; bridge execution not covered)\n",checks);
    return 0;
}
