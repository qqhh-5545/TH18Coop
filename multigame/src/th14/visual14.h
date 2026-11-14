#ifndef TH14_VISUAL_H
#define TH14_VISUAL_H
static void *anm_draw_original,*pause_original,*pause_draw_original;
static unsigned alpha14(unsigned id) {
    if(seats[id].ghost)return 112;
    unsigned local=online_map?online_map->role:0;
    if(id==local || !living(id))return 255;
    if(!living(local))return 128;
    float dx=FLT(seats[id].player,0x5e0)-FLT(seats[local].player,0x5e0),dy=FLT(seats[id].player,0x5e4)-FLT(seats[local].player,0x5e4),distance=sqrtf(dx*dx+dy*dy);
    if(distance<50)distance=50;unsigned alpha=(unsigned)(51.f+(distance-50.f)*4.08f);return alpha<128?alpha:128;
}
static int TC anm_draw14(void *manager,void *vm) {
    int seat=-1;
    if(vm && !ending)for(unsigned id=0;id<players;++id)if(seats[id].player &&
       (vm==OFF(seats[id].player,0x14) || (seats[id].anm && FIELD(vm,0x30)==(uint32_t)(uintptr_t)seats[id].anm))){seat=(int)id;break;}
    if(seat<0)return ((int (TC *)(void *,void *))anm_draw_original)(manager,vm);
    uint32_t colors[3];unsigned alpha=alpha14((unsigned)seat);
    for(unsigned i=0;i<3;++i){unsigned offset=0x534+i*4;colors[i]=FIELD(vm,offset);FIELD(vm,offset)=(colors[i]&0xffffff)|((colors[i]>>24)*alpha/255)<<24;}
    int result=((int (TC *)(void *,void *))anm_draw_original)(manager,vm);
    for(unsigned i=0;i<3;++i)FIELD(vm,0x534+i*4)=colors[i];return result;
}
// Only the native pause menu sees zero credits. Its disabled-menu handling
// supplies the original grey Continue artwork and excludes that selection.
// Keep the real credits counter, score and progress untouched.
static int TC pause14(void *menu) {
    if(!coop14_battle)return ((Callback)pause_original)(menu);
    uint32_t credits=U32(0x4f58c8);U32(0x4f58c8)=0;
    if(FIELD(menu,4)==21 && FIELD(menu,0x38)==0){FIELD(menu,4)=15;FIELD(menu,0x38)=FIELD(menu,0x3c)=1;timer(menu,0x10,30);}
    int result=((Callback)pause_original)(menu);U32(0x4f58c8)=credits;return result;
}
static int TC pause_draw14(void *menu) {
    if(!coop14_battle)return ((Callback)pause_draw_original)(menu);
    uint32_t credits=U32(0x4f58c8);U32(0x4f58c8)=0;
    int result=((Callback)pause_draw_original)(menu);U32(0x4f58c8)=credits;return result;
}
void bomb14_active(uint32_t *regs) {
    void *bomb=PTR(BOMB);
    for(unsigned id=0;id<players;++id)if(seats[id].bomb && FIELD(seats[id].bomb,0x40)==1){bomb=seats[id].bomb;break;}
    regs[7]=(uint32_t)(uintptr_t)bomb;
}
extern void bomb14_immunity(void),bomb14_modifier(void),bomb14_ecl(void);
static const struct {uintptr_t address;void *bridge;} bomb14_sites[]={
    {0x4238d0,bomb14_immunity},{0x423b27,bomb14_modifier},{0x42c1a6,bomb14_ecl}
};
#endif
