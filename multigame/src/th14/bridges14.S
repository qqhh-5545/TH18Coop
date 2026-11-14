.text
.globl _menu14_fade_bridge
_menu14_fade_bridge:
    pushal
    movl %esp,%ebx
    andl $-16,%esp
    subl $528,%esp
    fxsave (%esp)
    call _menu14_allow_fade
    movl %eax,28(%ebx)
    fxrstor (%esp)
    movl %ebx,%esp
    popal
    testl %eax,%eax
    jz 1f
    pushl $0x445840
    ret
1:
    xorl %eax,%eax
    ret

.globl _scale14_bridge
_scale14_bridge:
    pushl %ebp
    movl %esp,%ebp
    andl $-16,%esp
    subl $16,%esp
    movss %xmm1,(%esp)
    call _coop_scale14
    movl %ebp,%esp
    popl %ebp
    ret
.globl _native_scale14
_native_scale14:
    movss 4(%esp),%xmm1
    jmp *_scale_original

.macro DAMAGE_WRAP bridge, native, callback, original, count
.globl _\bridge
_\bridge:
    pushl %ebp
    movl %esp,%ebp
    andl $-16,%esp
    subl $16,%esp
    leal 8(%ebp),%eax
    movl %eax,(%esp)
    movss %xmm3,4(%esp)
    call _\callback
    movl %ebp,%esp
    popl %ebp
    ret $(\count*4)
.globl _\native
_\native:
    pushl %ebp
    movl %esp,%ebp
    pushl %esi
    movl 8(%ebp),%esi
    movss 12(%ebp),%xmm3
    .set offset,(\count-1)*4
    .rept \count
    pushl offset(%esi)
    .set offset,offset-4
    .endr
    call *_\original
    popl %esi
    popl %ebp
    ret
.endm
DAMAGE_WRAP damage14_bridge,native_damage14,coop_damage14,damage_original,8

.globl _circle14_bridge
_circle14_bridge:
    pushl %ebp
    movl %esp,%ebp
    andl $-16,%esp
    subl $16,%esp
    leal 8(%ebp),%eax
    movl %eax,(%esp)
    movss %xmm2,4(%esp)
    call _coop_circle14
    movl %ebp,%esp
    popl %ebp
    ret $8
.globl _native_circle14
_native_circle14:
    pushl %ebp
    movl %esp,%ebp
    movl 8(%ebp),%eax
    movss 12(%ebp),%xmm2
    pushl 4(%eax)
    pushl (%eax)
    call *_circle_original
    popl %ebp
    ret
.macro LASER_WRAP bridge,native,callback,original
.globl _\bridge
_\bridge:
    pushl %ebp
    movl %esp,%ebp
    andl $-16,%esp
    subl $16,%esp
    leal 8(%ebp),%eax
    movl %eax,(%esp)
    movss %xmm2,4(%esp)
    movss %xmm3,8(%esp)
    call _\callback
    movl %ebp,%esp
    popl %ebp
    ret $12
.globl _\native
_\native:
    pushl %ebp
    movl %esp,%ebp
    movl 8(%ebp),%eax
    movss 12(%ebp),%xmm2
    movss 16(%ebp),%xmm3
    pushl 8(%eax)
    pushl 4(%eax)
    pushl (%eax)
    call *_\original
    popl %ebp
    ret
.endm
LASER_WRAP laser14_bridge,native_laser14,coop_laser14,laser_original
LASER_WRAP laser214_bridge,native_laser214,coop_laser214,laser2_original

.macro ITEM_CALL callback,redirect=0
    pushfl
    pushal
    movl %esp,%ebx
    andl $-16,%esp
    subl $528,%esp
    fxsave (%esp)
    subl $12,%esp
    pushl %edi
    call _\callback
    addl $16,%esp
    fxrstor (%esp)
    movl %ebx,%esp
    .if \redirect
    testl %eax,%eax
    jz 9f
    popal
    popfl
    pushl $0x438ca2
    ret
9:
    .endif
    popal
    popfl
.endm
.globl _item14_loop_bridge
_item14_loop_bridge:
    movl 0xbf0(%edi),%eax
    cmpl $0,_coop14_battle
    je 3f
    cmpl $0,_item14_pending
    jne 1f
    testl %eax,%eax
    je 2f
    cmpl $5,%eax
    je 2f
1:
    ITEM_CALL item14_begin
2:
    movl 0x4db67c,%esi
    movl 0xbf0(%edi),%eax
3:
    pushl $0x4385b6
    ret
.globl _item14_pickup_bridge
_item14_pickup_bridge:
    cmpl $0,_item14_pending
    je 1f
    ITEM_CALL item14_picked,1
1:
    movl 0xbf4(%edi),%eax
    pushl $0x438b44
    ret

.macro AIM14 name,source,offset,resume,output=7,stack=0
.globl _\name
_\name:
    pushfl
    pushal
    .if \stack
    movl 8(%ebp),%eax
    addl $\offset,%eax
    .else
    leal \offset(\source),%eax
    .endif
    movl %esp,%ebx
    andl $-16,%esp
    subl $528,%esp
    fxsave (%esp)
    subl $4,%esp
    pushl $\output
    pushl %eax
    pushl %ebx
    call _aim14_nearest
    addl $16,%esp
    fxrstor (%esp)
    movl %ebx,%esp
    popal
    popfl
    pushl $\resume
    ret
.endm
AIM14 aim14_angle,%ebp,0,0x44ee0c,6,1
AIM14 aim14_shoot,%ebp,8,0x4191ae,7,1
AIM14 aim14_spawn,%edi,0xbc0,0x4179c1
AIM14 aim14_redirect,%esi,0xbc0,0x419b4b
AIM14 aim14_ecl_x,%ecx,0x1234,0x42a9df
AIM14 aim14_ecl_y,%ecx,0x1234,0x42a9f2
AIM14 aim14_ecl_fx,%ecx,0x1234,0x42b31b
AIM14 aim14_ecl_fy,%ecx,0x1234,0x42b32c
AIM14 aim14_ecl_distance,%ecx,0x1234,0x42abbd
AIM14 aim14_ecl_fdistance,%ecx,0x1234,0x42b918
AIM14 aim14_damage_state,%edi,0x44,0x423ab3

.macro BOMB14 name,resume
.globl _\name
_\name:
    pushfl
    pushal
    movl %esp,%ebx
    andl $-16,%esp
    subl $528,%esp
    fxsave (%esp)
    subl $12,%esp
    pushl %ebx
    call _bomb14_active
    addl $16,%esp
    fxrstor (%esp)
    movl %ebx,%esp
    popal
    popfl
    pushl $\resume
    ret
.endm
BOMB14 bomb14_immunity,0x4238d5
BOMB14 bomb14_modifier,0x423b2c
BOMB14 bomb14_ecl,0x42c1ab
