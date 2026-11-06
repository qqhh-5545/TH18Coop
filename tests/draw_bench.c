/* Isolates mod callback CPU overhead. No game, window, D3D or fixed image map.
   Compile against the packaged 0.13.0 source for the before measurement. */
#ifndef COOP_SOURCE
#define COOP_SOURCE "../src/coop.c"
#endif
#include COOP_SOURCE
static volatile uint32_t draw_sink;
static unsigned char *bench_vm;
int TC coop_hud(void *unused) {(void)unused;return 1;}
void coop_menu_hud(int owner) {(void)owner;}
static int TC bench_native(void *manager,void *vm) {
    (void)manager;draw_sink^=FIELD(vm,0x524);return 1;
}
static int __cdecl bench_scene(void) {
    for(unsigned i=0;i<1000;++i) {
        /* Most VMs are world effects, the rest player shots/options. */
        FIELD(bench_vm,0x1c)=i%3==0?18:31;
        anm_draw(NULL,bench_vm);
    }
    return 1;
}
int main(void) {
    p1=calloc(1,0x479d0);current_guest=calloc(1,0x479d0);bench_vm=calloc(1,0x624);
    if(!p1 || !current_guest || !bench_vm)return 1;
    FIELD(p1,0x476ac)=FIELD(current_guest,0x476ac)=1;FLT(p1,0x620)=0;FLT(current_guest,0x620)=60;
    FIELD(bench_vm,0x524)=FIELD(bench_vm,0x528)=0xffffffff;
    anm_draw_original=bench_native;scene_draw_original=bench_scene;
    LARGE_INTEGER frequency,start,end;QueryPerformanceFrequency(&frequency);
    for(unsigned trial=0;trial<3;++trial) {
        QueryPerformanceCounter(&start);
        for(unsigned frame=0;frame<2000;++frame)scene_draw();
        QueryPerformanceCounter(&end);
        printf("draw_bench trial=%u frames=2000 vm_per_frame=1000 total_ms=%.3f sink=%lu\n",
            trial,1000.0*(end.QuadPart-start.QuadPart)/frequency.QuadPart,(unsigned long)draw_sink);
    }
    return 0;
}
