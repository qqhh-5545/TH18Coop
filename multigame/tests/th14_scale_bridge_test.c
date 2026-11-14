#include <stdio.h>
#include <stdint.h>
volatile float captured_hook,captured_native;
void coop_scale14(float value){captured_hook=value;}
extern void native_scale14(float),test_native_scale(void);
void *scale_original=test_native_scale;
__asm__(".text\n.globl _test_native_scale\n_test_native_scale:\nmovss %xmm1,_captured_native\nret\n");
int main(void) {
    const float values[]={1.f,3.f,0.5f};
    for(unsigned i=0;i<3;++i){
        float value=values[i];uintptr_t before,after;
        __asm__ volatile("movl %%esp,%0":"=r"(before));
        __asm__ volatile("movss %0,%%xmm1\ncall _scale14_bridge"::"m"(value):"eax","ecx","edx","xmm0","xmm1","xmm2","xmm3","memory","cc");
        __asm__ volatile("movl %%esp,%0":"=r"(after));
        if(before!=after || captured_hook!=value){fputs("FAIL native-to-C scale bridge\n",stderr);return 1;}
        native_scale14(value);
        if(captured_native!=value){fputs("FAIL C-to-native scale bridge\n",stderr);return 1;}
    }
    puts("PASS TH14 scale bridge: native XMM1 argument, C callback, native trampoline argument and stack balance");return 0;
}
