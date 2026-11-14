#ifndef TH14_AIM_H
#define TH14_AIM_H
void aim14_nearest(uint32_t *regs,const float *origin,unsigned output) {
    void *best=PTR(PLAYER);float distance=INFINITY;
    if(!scope_depth && origin && isfinite(origin[0]) && isfinite(origin[1]))for(unsigned id=0;id<players;++id)if(living(id)) {
        void *p=seats[id].player;float dx=origin[0]-FLT(p,0x5e0),dy=origin[1]-FLT(p,0x5e4),d=dx*dx+dy*dy;
        if(d<distance){distance=d;best=p;}
    }
    regs[output]=(uint32_t)(uintptr_t)best;
}
extern void aim14_angle(void),aim14_shoot(void),aim14_spawn(void),aim14_redirect(void);
extern void aim14_ecl_x(void),aim14_ecl_y(void),aim14_ecl_fx(void),aim14_ecl_fy(void),aim14_ecl_distance(void),aim14_ecl_fdistance(void);
extern void aim14_damage_state(void);
static const struct {uintptr_t address;unsigned length;void *bridge;} aim14_sites[]={
    {0x44ee06,6,aim14_angle},{0x4191a9,5,aim14_shoot},{0x4179bc,5,aim14_spawn},{0x419b46,5,aim14_redirect},
    {0x42a9da,5,aim14_ecl_x},{0x42a9ed,5,aim14_ecl_y},{0x42b316,5,aim14_ecl_fx},{0x42b327,5,aim14_ecl_fy},
    {0x42abb8,5,aim14_ecl_distance},{0x42b913,5,aim14_ecl_fdistance},{0x423aae,5,aim14_damage_state}
};
#endif
