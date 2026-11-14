#ifndef TH18_NATIVE_ROLLBACK_SCENARIOS_H
#define TH18_NATIVE_ROLLBACK_SCENARIOS_H
/* Explicit no-commit local scenario only. Exercise native shop/scene entry
   without requiring an unattended bot to clear the entire stage. These
   injected lifecycle tests are not reported as a natural Boss clear. */
static int rb_scenario_boundary(uint32_t frame) {
    void *pause=PTR(0x4cf40c);
    int playing=p1 && ship(1) && (party_count()==2 || ship(2)) && frames>=120 && !(pause && FIELD(pause,0x1ec));
    return rb_live.scenario && ((rb_live.scenario==1 && frame>=900 && playing) || (rb_live.scenario==2 && frame==rb_live.scenario_next));
}
static int rb_scenario_before(uint32_t frame) {
    if(!rb_scenario_boundary(frame))return 1;
    if(rb_live.scenario==1){
        if(!p1 || !ship(1) || PTR(0x4cf2a4) || scope_depth)return 0;
        /* Synthetic shop fixture: provide a bounded temporary balance so the
           original mandatory purchase screen can be exercised. No unlock
           flags or permanent profile are changed by this local test. */
        U32(0x4ccd34)+=1000;for(unsigned id=1;id<party_count();++id)guests[id-1].funds+=1000;
        float at[3]={0,96,0};void *object=((void *(__cdecl *)(unsigned))0x48dc71)(0xe3c);if(!object)return 0;
        memset(object,0,0xe3c);FIELD(object,0)=2;PTR(0x4cf2a4)=object;
        ((void (TC *)(void *))0x416cd0)(p1);
        if(shop_init(object,at))return 0;
        log_line("RB_SCENARIO injected native shop frame=%lu ghost_after=%d/%d/%d",(unsigned long)frame,partner[0].ghost,partner[1].ghost,partner[2].ghost);
        rb_live.scenario=2;rb_live.scenario_next=UINT32_MAX;
    }else{
        if(PTR(0x4cf2a4) || shop_pending_owner || shop_owner)return 0;
        if(U32(0x4cccdc)==1){U32(0x4cccdc)=2;PTR(0x4cf428)=(void *)(0x4c9410+2*0xd4);U32(0x4cd5e8)=12;}
        log_line("RB_SCENARIO injected native next-stage request frame=%lu stage=%lu",(unsigned long)frame,(unsigned long)U32(0x4cccdc));rb_live.scenario=3;
    }return 1;
}
static void rb_scenario_after(uint32_t frame,int exact) {
    if(exact && rb_live.scenario==2 && rb_live.scenario_next==UINT32_MAX && !PTR(0x4cf2a4) && !shop_pending_owner && !shop_owner){
        rb_live.scenario_next=(frame/120+2)*120;
        log_line("RB_SCENARIO all native shops closed frame=%lu transition_at=%lu",(unsigned long)frame,(unsigned long)rb_live.scenario_next);}
}
#endif
