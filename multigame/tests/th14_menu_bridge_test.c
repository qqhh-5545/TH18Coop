#include <stdint.h>
#include <stdio.h>
volatile int fade_allowed,native_calls;
volatile float captured_fade;
int menu14_allow_fade(void){__asm__ volatile("xorps %%xmm1,%%xmm1":::"xmm1");return fade_allowed;}
__asm__(".text\n.globl _test_native_fade\n_test_native_fade:\nmovss %xmm1,_captured_fade\nincl _native_calls\nxorl %eax,%eax\nret\n");
int main(void){
    for(unsigned allowed=0;allowed<=1;++allowed){
        const float duration=32.f;uintptr_t before,after;float restored;
        fade_allowed=allowed;native_calls=0;captured_fade=0;
        __asm__ volatile("movl %%esp,%0":"=r"(before));
        __asm__ volatile("movss %1,%%xmm1\ncall _menu14_fade_bridge\nmovss %%xmm1,%0":"=m"(restored):"m"(duration):"eax","ecx","edx","xmm0","xmm1","xmm2","xmm3","memory","cc");
        __asm__ volatile("movl %%esp,%0":"=r"(after));
        if(before!=after || native_calls!=(int)allowed || restored!=duration || (allowed && captured_fade!=duration)){fputs("FAIL TH14 menu fade bridge\n",stderr);return 1;}
    }
    puts("PASS menu BGM bridge: handoff skip, final native fade, XMM1 preservation and stack balance");return 0;
}
