.text
/* Preserve the caller's x87/SSE registers and flags. Only the original MOV's
   EAX result changes. EBX is saved before being used as the aligned frame. */
.macro PARTY_AIM name, source, offset, resume, output=7, homing=0
.globl _\name
_\name:
    pushfl
    pushal
    leal \offset(\source), %eax
    movl %esp, %ebx
    andl $-16, %esp
    subl $528, %esp
    fxsave (%esp)
    subl $4, %esp
    pushl $\output
    pushl %eax
    pushl %ebx
    call _coop_nearest_aim
    addl $16, %esp
    fxrstor (%esp)
    movl %ebx, %esp
    popal
    popfl
    .if \homing
    movl 0x690(%esi), %eax
    .endif
    pushl $\resume
    ret
.endm
PARTY_AIM aim_spawn_bridge, %ebx, 0xe38, 0x425fe0
PARTY_AIM aim_redirect1_bridge, %ebx, 0x638, 0x42615f
PARTY_AIM aim_redirect2_bridge, %ebx, 0x638, 0x4267a0
PARTY_AIM aim_redirect3_bridge, %ebx, 0x638, 0x426a6c
PARTY_AIM aim_redirect4_bridge, %ebx, 0x638, 0x427109
PARTY_AIM aim_redirect5_bridge, %ebx, 0x638, 0x42734c
PARTY_AIM aim_tick_bridge, %esi, 0x638, 0x428493
PARTY_AIM aim_laser_bridge, %edi, 0x54, 0x449643
PARTY_AIM aim_ecl_x_bridge, %ecx, 0x1270, 0x4374b2
PARTY_AIM aim_ecl_y_bridge, %ecx, 0x1270, 0x4374c5
PARTY_AIM aim_ecl_fx_bridge, %ecx, 0x1270, 0x4380fc
PARTY_AIM aim_ecl_fy_bridge, %ecx, 0x1270, 0x43810d
PARTY_AIM aim_ecl_distance_bridge, %ecx, 0x1270, 0x4375da
PARTY_AIM aim_ecl_fdistance_bridge, %ecx, 0x1270, 0x438373
PARTY_AIM aim_ecl_side_bridge, %eax, 0, 0x433461, 6
PARTY_AIM aim_ecl_angle1_bridge, %edi, 0x1270, 0x435eb3, 6
PARTY_AIM aim_ecl_angle2_bridge, %edi, 0x1270, 0x435ebe, 6
PARTY_AIM aim_homing_bridge, %esi, 0x638, 0x438e03, 4, 1
PARTY_AIM aim_emit_distance_bridge, %edx, 0, 0x425303
PARTY_AIM aim_laser_radial_bridge, %esi, 0x54, 0x44aade
PARTY_AIM aim_laser_curve_bridge, %esi, 0x54, 0x44cf98
.macro PARTY_AIM_XMM name, resume
.globl _\name
_\name:
    pushfl
    pushal
    movl %esp, %ebx
    andl $-16, %esp
    subl $528, %esp
    fxsave (%esp)
    /* These two native laser paths already evaluated the emission origin
       into XMM2/XMM3, which may differ from the laser object's base. */
    movss 192(%esp), %xmm0
    movss %xmm0, 512(%esp)
    movss 208(%esp), %xmm0
    movss %xmm0, 516(%esp)
    leal 512(%esp), %eax
    subl $4, %esp
    pushl $7
    pushl %eax
    pushl %ebx
    call _coop_nearest_aim
    addl $16, %esp
    fxrstor (%esp)
    movl %ebx, %esp
    popal
    popfl
    pushl $\resume
    ret
.endm
PARTY_AIM_XMM aim_laser_emit_bridge, 0x449ada
PARTY_AIM_XMM aim_laser_spawn_bridge, 0x45102b
.globl _aim_pattern_bridge
_aim_pattern_bridge:
    /* At this entry the native pattern argument has not reached EDI yet. */
    pushl %edi
    movl 8(%ebp), %edi
    call 1f
    popl %edi
    pushl $0x42781e
    ret
1:
    /* Reuse the same ABI envelope, returning to the small wrapper above. */
    pushfl
    pushal
    leal 8(%edi), %eax
    movl %esp, %ebx
    andl $-16, %esp
    subl $528, %esp
    fxsave (%esp)
    subl $4, %esp
    pushl $7
    pushl %eax
    pushl %ebx
    call _coop_nearest_aim
    addl $16, %esp
    fxrstor (%esp)
    movl %ebx, %esp
    popal
    popfl
    ret
.macro PARTY_BOMB name, resume
.globl _\name
_\name:
    pushfl
    pushal
    movl %esp, %ebx
    andl $-16, %esp
    subl $528, %esp
    fxsave (%esp)
    subl $12, %esp
    pushl %ebx
    call _coop_any_bomb
    addl $16, %esp
    fxrstor (%esp)
    movl %ebx, %esp
    popal
    popfl
    pushl $\resume
    ret
.endm
PARTY_BOMB bomb_immunity_bridge, 0x42ed4b
PARTY_BOMB bomb_modifier_bridge, 0x42f238
/* 40f4de passes a 2D size stored at EBP-1c/-18 to448f10, which also
   copies [size+8] into LaserManager+7b4. EBP-14 is never initialized.
   Define that ignored Z lane without changing X/Y, flags or xmm3. */
.globl _rb_rect_zero_z_bridge
_rb_rect_zero_z_bridge:
    pushl %eax
    movl 12(%esp), %eax
    movl $0, 8(%eax)
    popl %eax
    pushl $0x448f10
    ret

.globl _item_loop_bridge
.globl _item_pickup_bridge
/* The original successful-pickup branch. MOV preserves every register,
   EFLAGS and the FP/SSE state before continuing the native release path. */
_item_pickup_bridge:
    movl $1, _coop_pickup_confirmed
    movl $0, 0xc74(%edi)
    pushl $0x44659f
    ret

_item_loop_bridge:
    pushfl
    pushal
    movl %esp, %ebx
    andl $-16, %esp
    subl $528, %esp
    fxsave (%esp)
    subl $12, %esp
    pushl %ebx
    call _coop_select_item
    addl $16, %esp
    fxrstor (%esp)
    movl %ebx, %esp
    popal
    popfl
    movl 0xc74(%edi), %eax
    pushl $0x445ae6
    ret

/* Original damage query: seven stack arguments and rotation in xmm3. */
.globl _boss_damage_bridge
_boss_damage_bridge:
    pushfl
    pushal
    movl %esp, %ebx
    andl $-16, %esp
    subl $528, %esp
    fxsave (%esp)
    subl $12, %esp
    pushl %ebx
    call _coop_scale_boss_damage
    addl $16, %esp
    fxrstor (%esp)
    movl %ebx, %esp
    popal
    popfl
    testb $0x10, 0x5130(%ebx)
    pushl $0x42f2c0
    ret

.globl _damage_bridge
_damage_bridge:
    pushl %ebp
    movl %esp, %ebp
    andl $-16, %esp
    subl $16, %esp
    leal 8(%ebp), %eax
    movl %eax, (%esp)
    movss %xmm3, 4(%esp)
    call _coop_damage
    movl %ebp, %esp
    popl %ebp
    ret $28

.globl _native_damage
_native_damage:
    pushl %ebp
    movl %esp, %ebp
    pushl %esi
    movl 8(%ebp), %esi
    movss 12(%ebp), %xmm3
    pushl 24(%esi)
    pushl 20(%esi)
    pushl 16(%esi)
    pushl 12(%esi)
    pushl 8(%esi)
    pushl 4(%esi)
    pushl (%esi)
    call *_damage_original
    popl %esi
    popl %ebp
    ret

/* Original circular hit test: two stack arguments and radius in xmm2. */
.globl _circle_bridge
_circle_bridge:
    pushl %ebp
    movl %esp, %ebp
    andl $-16, %esp
    subl $16, %esp
    leal 8(%ebp), %eax
    movl %eax, (%esp)
    movss %xmm2, 4(%esp)
    call _coop_circle
    movl %ebp, %esp
    popl %ebp
    ret $8

.globl _native_circle
_native_circle:
    pushl %ebp
    movl %esp, %ebp
    movl 8(%ebp), %eax
    movss 12(%ebp), %xmm2
    pushl 4(%eax)
    pushl (%eax)
    call *_circle_original
    popl %ebp
    ret

.globl _laser_bridge
_laser_bridge:
    pushl %ebp
    movl %esp, %ebp
    andl $-16, %esp
    subl $16, %esp
    leal 8(%ebp), %eax
    movl %eax, (%esp)
    movss %xmm2, 4(%esp)
    movss %xmm3, 8(%esp)
    call _coop_laser
    movl %ebp, %esp
    popl %ebp
    ret $12

.globl _native_laser
_native_laser:
    pushl %ebp
    movl %esp, %ebp
    movl 8(%ebp), %eax
    movss 12(%ebp), %xmm2
    movss 16(%ebp), %xmm3
    pushl 8(%eax)
    pushl 4(%eax)
    pushl (%eax)
    call *_laser_original
    popl %ebp
    ret
