#include <stdint.h>
#include <stdarg.h>
#include <stddef.h>

#include "libstructs.h"
#include "version.h"

void  __attribute__((used,aligned(256),section(".vectors"))) __stub_vectors() {
__asm__(
"       .section .vectors               \n"
"       .ascii  \"" VERSION_STRING "\"  \n"
"       .byte 0                         \n"
"       .ascii \"Based on WarpOS project for Sonnet cards by Dennis van der Boon\"\n"
"       .byte 0                         \n"
"       .org 0x100,0                    \n"
"       .globl SystemReset              \n"
"SystemReset:                           \n"
"       lis %r1, 0xffef                 \n"
"       ori %r1, %r1, 0xffe0            \n"
//"       bl PPC_C_Init                   \n"
"1:     b 1b                            \n"

"       .org 0x200,0                    \n"
"       .globl MachineCheck             \n"
"MachineCheck:                          \n"
"1:     b 1b                            \n"

"       .org 0x300,0                    \n"
"       .globl DSI                      \n"
"DSI:                                   \n"
"1:     b 1b                            \n"

"       .org 0x400,0                    \n"
"       .globl ISI                      \n"
"ISI:                                   \n"
"1:     b 1b                            \n"

"       .org 0x500,0                    \n"
"       .globl ExternalInt              \n"
"ExternalInt:                           \n"
"1:     b 1b                            \n"

"       .org 0x600,0                    \n"
"       .globl Alignment                \n"
"Alignment:                             \n"
"1:     b 1b                            \n"

"       .org 0x700,0                    \n"
"       .globl Program                  \n"
"Program:                               \n"
"1:     b 1b                            \n"

"       .org 0xc00,0                    \n"
"       .globl SysCall                  \n"
"SysCall:                               \n"
"       lis %r3, 0xcafe                 \n"
"       ori %r3, %r3, 0xb1ba            \n"
"       rfi                             \n"

);
}

void  __attribute__((used)) __stub_exception_exit() {
/*
    ExceptionExit is called with the exception frame in r31
*/
asm volatile(
"       .globl ExceptionExit                        \n"
"ExceptionExit:                                     \n"
"       mr      %%r1, %%r31                         \n"
"       bl      LoadFrame                           \n"
"       bl      FlushICache                         \n"
"       lwz     %%r0, %[if_context_cr](%%r1)        \n"
"       mtcr    %%r0                                \n"
"       lwz     %%r3, %[if_context_gpr3](%%r1)      \n"
"       lwz     %%r0, %[if_context_gpr0](%%r1)      \n"
"       mtsprg2 %%r0                                \n"
"       lwz     %%r1, %[if_context_gpr1](%%r1)      \n"
"       mfsprg0 %%r0                                \n"
"       mtsrr0  %%r0                                \n"
"       mfsprg1 %%r0                                \n"
"       mtsrr1  %%r0                                \n"
"       mfsprg3 %%r0                                \n"
"       mtlr    %%r0                                \n"
"       mfsprg2 %%r0                                \n"
"       rfi                                         \n"
:
:[if_context_cr]"i"(__builtin_offsetof(struct iframe, if_Context.ec_CR)),
 [if_context_gpr0]"i"(__builtin_offsetof(struct iframe, if_Context.ec_GPR[0])),
 [if_context_gpr1]"i"(__builtin_offsetof(struct iframe, if_Context.ec_GPR[1])),
 [if_context_gpr3]"i"(__builtin_offsetof(struct iframe, if_Context.ec_GPR[3]))
);
}

void __attribute__((used)) __stub_icache_flush() {
asm volatile(
"       .globl FlushICache          \n"
"FlushICache:                       \n"
"       icbi    0, %%r0             \n" // On Emu68 it is sufficient to flush single cache line,
"       blr                         \n" // since Emu68 will flush entire cache anyway
::);
}

void __attribute__((used)) __stub_frame_store() {
asm volatile(
"       .globl StoreFrame                   \n"
"StoreFrame:                                \n"
#if HAVE_FPU
"       stfd    %%f0,[IF_CONTEXT_FPR](r3)   \n"
#endif
"       mfsprg0 %%r0                        \n"
"       stwu    %%r0, 4(%%r3)               \n"
"       mfsprg1 %%r0                        \n"
"       stwu    %%r0, 4(%%r3)               \n"
"       mfdar   %%r0                        \n"
"       stwu    %%r0, 4(%%r3)               \n"
"       mfdsisr %%r0                        \n"
"       stwu    %%r0, 4(%%r3)               \n"
"       lwz     %%r0, 104(%%r1)             \n" // cr - change to number!!!
"       stwu    %%r0, 4(%%r3)               \n"
"       mfctr   %%r0                        \n"
"       stwu    %%r0, 4(%%r3)               \n"
"       mfsprg2 %%r0                        \n"
"       stwu    %%r0, 4(%%r3)               \n" // lr
#if HAVE_FPU
"       mffs    f0                          \n"
"       stfdu   f0,4(r3)                    \n"
"       mfxer   r0                          \n"
"       stw     r0,0(r3)                    \n"
#else
"       mfxer   %%r0                        \n"
"       stwu    %%r0, 4(%%r3)               \n"
#endif
"       lwz     %%r0, 0(%%r1)               \n" //
"       stwu    %%r0, 12(%%r3)              \n" // r1, skipped r0
"       stwu    %%r2, 4(%%r3)               \n"
"       stwu    %%r5, 12(%%r3)              \n" // skipped r3 and r4. Need to be stored seperately
"       stwu    %%r6, 4(%%r3)               \n"
"       stwu    %%r7, 4(%%r3)               \n"
"       stwu    %%r8, 4(%%r3)               \n"
"       stwu    %%r9, 4(%%r3)               \n"
"       stwu    %%r10, 4(%%r3)              \n"
"       stwu    %%r11, 4(%%r3)              \n"
"       stwu    %%r12, 4(%%r3)              \n"
"       stwu    %%r13, 4(%%r3)              \n"
"       stwu    %%r14, 4(%%r3)              \n"
"       stwu    %%r15, 4(%%r3)              \n"
"       stwu    %%r16, 4(%%r3)              \n"
"       stwu    %%r17, 4(%%r3)              \n"
"       stwu    %%r18, 4(%%r3)              \n"
"       stwu    %%r19, 4(%%r3)              \n"
"       stwu    %%r20, 4(%%r3)              \n"
"       stwu    %%r21, 4(%%r3)              \n"
"       stwu    %%r22, 4(%%r3)              \n"
"       stwu    %%r23, 4(%%r3)              \n"
"       stwu    %%r24, 4(%%r3)              \n"
"       stwu    %%r25, 4(%%r3)              \n"
"       stwu    %%r26, 4(%%r3)              \n"
"       stwu    %%r27, 4(%%r3)              \n"
"       stwu    %%r28, 4(%%r3)              \n"
"       stwu    %%r29, 4(%%r3)              \n"
"       stwu    %%r30, 4(%%r3)              \n"
"       stwu    %%r31, 4(%%r3)              \n"
#if HAVE_FPU
        stfdu   f1,12(r3)                    #skipped f0 (see above)
        stfdu   f2,8(r3)
        stfdu   f3,8(r3)
        stfdu   f4,8(r3)
        stfdu   f5,8(r3)
        stfdu   f6,8(r3)
        stfdu   f7,8(r3)
        stfdu   f8,8(r3)
        stfdu   f9,8(r3)
        stfdu   f10,8(r3)
        stfdu   f11,8(r3)
        stfdu   f12,8(r3)
        stfdu   f13,8(r3)
        stfdu   f14,8(r3)
        stfdu   f15,8(r3)
        stfdu   f16,8(r3)
        stfdu   f17,8(r3)
        stfdu   f18,8(r3)
        stfdu   f19,8(r3)
        stfdu   f20,8(r3)
        stfdu   f21,8(r3)
        stfdu   f22,8(r3)
        stfdu   f23,8(r3)
        stfdu   f24,8(r3)
        stfdu   f25,8(r3)
        stfdu   f26,8(r3)
        stfdu   f27,8(r3)
        stfdu   f28,8(r3)
        stfdu   f29,8(r3)
        stfdu   f30,8(r3)
        stfdu   f31,8(r3)
#endif
#if HAVE_ALTIVEC
        mfsprg1 r0
        andis.  r0,r0,PSL_VEC@h
        bne     .DoVMX
        addi    r3,r3,8+8+512+16           #f31, alignstore, vectors, vscr
        b       .NoVMX

.DoVMX: mfmsr   r0
        oris    r0,r0,PSL_VEC@h
        mtmsr   r0
        isync

        addi    r3,r3,32
        stvx	v0,r0,r3
        subi    r3,r3,16
        mfvscr  v0
        stvx    v0,r0,r3
        addi    r3,r3,32
		stvx	v1,r0,r3
		addi	r3,r3,16
		stvx	v2,r0,r3
		addi	r3,r3,16
		stvx	v3,r0,r3
		addi	r3,r3,16
		stvx	v4,r0,r3
		addi	r3,r3,16
		stvx	v5,r0,r3
		addi	r3,r3,16
		stvx	v6,r0,r3
		addi	r3,r3,16
		stvx	v7,r0,r3
		addi	r3,r3,16
		stvx	v8,r0,r3
		addi	r3,r3,16
		stvx	v9,r0,r3
		addi	r3,r3,16
		stvx	v10,r0,r3
		addi	r3,r3,16
		stvx	v11,r0,r3
		addi	r3,r3,16
		stvx	v12,r0,r3
		addi	r3,r3,16
		stvx	v13,r0,r3
		addi	r3,r3,16
		stvx	v14,r0,r3
		addi	r3,r3,16
		stvx	v15,r0,r3
		addi	r3,r3,16
		stvx	v16,r0,r3
		addi	r3,r3,16
		stvx	v17,r0,r3
		addi	r3,r3,16
		stvx	v18,r0,r3
		addi	r3,r3,16
		stvx	v19,r0,r3
		addi	r3,r3,16
		stvx	v20,r0,r3
		addi	r3,r3,16
		stvx	v21,r0,r3
		addi	r3,r3,16
		stvx	v22,r0,r3
		addi	r3,r3,16
		stvx	v23,r0,r3
		addi	r3,r3,16
		stvx	v24,r0,r3
		addi	r3,r3,16
		stvx	v25,r0,r3
		addi	r3,r3,16
		stvx	v26,r0,r3
		addi	r3,r3,16
		stvx	v27,r0,r3
		addi	r3,r3,16
		stvx	v28,r0,r3
		addi	r3,r3,16
		stvx	v29,r0,r3
		addi	r3,r3,16
		stvx	v30,r0,r3
		addi	r3,r3,16
		stvx	v31,r0,r3
		mfspr	r0,VRSAVE
		stwu    r0,16(r3)
#endif
#if HAVE_BAT
.NoVMX: mfibatu r0,0
        stwu    r0,4(r3)
        mfibatl r0,0
        stwu    r0,4(r3)
        mfdbatu r0,0
        stwu    r0,4(r3)
        mfdbatl r0,0
        stwu    r0,4(r3)
        mfibatu r0,1
        stwu    r0,4(r3)
        mfibatl r0,1
        stwu    r0,4(r3)
        mfdbatu r0,1
        stwu    r0,4(r3)
        mfdbatl r0,1
        stwu    r0,4(r3)
        mfibatu r0,2
        stwu    r0,4(r3)
        mfibatl r0,2
        stwu    r0,4(r3)
        mfdbatu r0,2
        stwu    r0,4(r3)
        mfdbatl r0,2
        stwu    r0,4(r3)
        mfibatu r0,3
        stwu    r0,4(r3)
        mfibatl r0,3
        stwu    r0,4(r3)
        mfdbatu r0,3
        stwu    r0,4(r3)
        mfdbatl r0,3
        stwu    r0,4(r3)
#endif
#if HAVE_SEGMENTS
        mfsr    r0,0
        stwu    r0,4(r3)
        mfsr    r0,1
        stwu    r0,4(r3)
        mfsr    r0,2
        stwu    r0,4(r3)
        mfsr    r0,3
        stwu    r0,4(r3)
        mfsr    r0,4
        stwu    r0,4(r3)
        mfsr    r0,5
        stwu    r0,4(r3)
        mfsr    r0,6
        stwu    r0,4(r3)
        mfsr    r0,7
        stwu    r0,4(r3)
        mfsr    r0,8
        stwu    r0,4(r3)
        mfsr    r0,9
        stwu    r0,4(r3)
        mfsr    r0,10
        stwu    r0,4(r3)
        mfsr    r0,11
        stwu    r0,4(r3)
        mfsr    r0,12
        stwu    r0,4(r3)
        mfsr    r0,13
        stwu    r0,4(r3)
        mfsr    r0,14
        stwu    r0,4(r3)
        mfsr    r0,15
        stwu    r0,4(r3)
#endif
"       blr                                 \n"
:
:[IF_CONTEXT_FPR]"i"(__builtin_offsetof(struct iframe, if_Context.ec_FPR[0]))
);
}

void __attribute__((used)) __stub_frame_load() {
asm volatile(
/*
    Restore registers from exception frame. The frame is given in r31
*/
"       .globl LoadFrame                            \n"
"LoadFrame:                                         \n"
"       lwzu    %%r3,4(%%r31)                       \n"
"       mtsprg0 %%r3                                \n"
"       lwzu    %%r3,4(%%r31)                       \n"
"       mtsprg1 %%r3                                \n"
"       lwzu    %%r3,4(%%r31)                       \n"
"       mtdar   %%r3                                \n"
"       lwzu    %%r3,4(%%r31)                       \n"
"       mtdsisr %%r3                                \n"
"       lwzu    %%r3,8(%%r31)                       \n" // skip cr
"       mtctr   %%r3                                \n"
"       lwzu    %%r3,4(%%r31)                       \n"
"       mtsprg3 %%r3                                \n"
"       lwzu    %%r3,4(%%r31)                       \n"
"       mtxer   %%r3                                \n"
#if HAVE_FPU
"       lfd     %%f0,0(%%r31)                       \n"
"       mtfsf   0xff,%%f0                           \n"
#endif
"       lwzu    %%r2,16(%%r31)                      \n" // skip r0-r1, is loaded seperately
"       lwzu    %%r4,8(%%r31)                       \n" // skip r3, is loaded seperately
"       lwzu    %%r5,4(%%r31)                       \n"
"       lwzu    %%r6,4(%%r31)                       \n"
"       lwzu    %%r7,4(%%r31)                       \n"
"       lwzu    %%r8,4(%%r31)                       \n"
"       lwzu    %%r9,4(%%r31)                       \n"
"       lwzu    %%r10,4(%%r31)                      \n"
"       lwzu    %%r11,4(%%r31)                      \n"
"       lwzu    %%r12,4(%%r31)                      \n"
"       lwzu    %%r13,4(%%r31)                      \n"
"       lwzu    %%r14,4(%%r31)                      \n"
"       lwzu    %%r15,4(%%r31)                      \n"
"       lwzu    %%r16,4(%%r31)                      \n"
"       lwzu    %%r17,4(%%r31)                      \n"
"       lwzu    %%r18,4(%%r31)                      \n"
"       lwzu    %%r19,4(%%r31)                      \n"
"       lwzu    %%r20,4(%%r31)                      \n"
"       lwzu    %%r21,4(%%r31)                      \n"
"       lwzu    %%r22,4(%%r31)                      \n"
"       lwzu    %%r23,4(%%r31)                      \n"
"       lwzu    %%r24,4(%%r31)                      \n"
"       lwzu    %%r25,4(%%r31)                      \n"
"       lwzu    %%r26,4(%%r31)                      \n"
"       lwzu    %%r27,4(%%r31)                      \n"
"       lwzu    %%r28,4(%%r31)                      \n"
"       lwzu    %%r29,4(%%r31)                      \n"
"       lwzu    %%r30,4(%%r31)                      \n"
"       lwzu    %%r0,4(%%r31)                       \n" // temp store r31 in r0
#if HAVE_FPU
        lfdu    f0,4(r31)
        lfdu    f1,8(r31)
        lfdu    f2,8(r31)
        lfdu    f3,8(r31)
        lfdu    f4,8(r31)
        lfdu    f5,8(r31)
        lfdu    f6,8(r31)
        lfdu    f7,8(r31)
        lfdu    f8,8(r31)
        lfdu    f9,8(r31)
        lfdu    f10,8(r31)
        lfdu    f11,8(r31)
        lfdu    f12,8(r31)
        lfdu    f13,8(r31)
        lfdu    f14,8(r31)
        lfdu    f15,8(r31)
        lfdu    f16,8(r31)
        lfdu    f17,8(r31)
        lfdu    f18,8(r31)
        lfdu    f19,8(r31)
        lfdu    f20,8(r31)
        lfdu    f21,8(r31)
        lfdu    f22,8(r31)
        lfdu    f23,8(r31)
        lfdu    f24,8(r31)
        lfdu    f25,8(r31)
        lfdu    f26,8(r31)
        lfdu    f27,8(r31)
        lfdu    f28,8(r31)
        lfdu    f29,8(r31)
        lfdu    f30,8(r31)
        lfdu    f31,8(r31)
#endif
#if HAVE_ALTIVEC
        mfsprg1 r3
        andis.  r3,r3,PSL_VEC@h
        bne     .DoAV
        addi    %%r31,%%r31,8+8+512+16
        b       .NoAV

.DoAV:  mfmsr   r3
        oris    r3,r3,PSL_VEC@h
        mtmsr   r3
        isync

        addi    r31,r31,16
        lvx     v0,r0,r31
        mtvscr  v0
        addi    r31,r31,16
        lvx     v0,r0,r31
		addi    r31,r31,16
		lvx     v1,r0,r31
		addi    r31,r31,16
		lvx     v2,r0,r31
		addi    r31,r31,16
		lvx     v3,r0,r31
		addi    r31,r31,16
		lvx     v4,r0,r31
		addi    r31,r31,16
		lvx     v5,r0,r31
		addi    r31,r31,16
		lvx     v6,r0,r31
		addi    r31,r31,16
		lvx     v7,r0,r31
		addi    r31,r31,16
		lvx     v8,r0,r31
		addi    r31,r31,16
		lvx     v9,r0,r31
		addi    r31,r31,16
		lvx     v10,r0,r31
		addi    r31,r31,16
		lvx     v11,r0,r31
		addi    r31,r31,16
		lvx     v12,r0,r31
		addi    r31,r31,16
		lvx     v13,r0,r31
		addi    r31,r31,16
		lvx     v14,r0,r31
		addi    r31,r31,16
		lvx     v15,r0,r31
		addi    r31,r31,16
		lvx     v16,r0,r31
		addi    r31,r31,16
		lvx     v17,r0,r31
		addi    r31,r31,16
		lvx     v18,r0,r31
		addi    r31,r31,16
		lvx     v19,r0,r31
		addi    r31,r31,16
		lvx     v20,r0,r31
		addi    r31,r31,16
		lvx     v21,r0,r31
		addi    r31,r31,16
		lvx     v22,r0,r31
		addi    r31,r31,16
		lvx     v23,r0,r31
		addi    r31,r31,16
		lvx     v24,r0,r31
		addi    r31,r31,16
		lvx     v25,r0,r31
		addi    r31,r31,16
		lvx     v26,r0,r31
		addi    r31,r31,16
		lvx     v27,r0,r31
		addi    r31,r31,16
		lvx     v28,r0,r31
		addi    r31,r31,16
		lvx     v29,r0,r31
		addi    r31,r31,16
		lvx     v30,r0,r31
        addi    r31,r31,16
        lvx     v31,r0,r31
        lwzu    r3,16(r31)
		mtspr	VRSAVE,r3
#endif
#if HAVE_BAT
.NoAV:  lwzu    r3,4(r31)
        mtibatu 0,r3
        lwzu    r3,4(r31)
        mtibatl 0,r3
        lwzu    r3,4(r31)
        mtdbatu 0,r3
        lwzu    r3,4(r31)
        mtdbatl 0,r3
        lwzu    r3,4(r31)
        mtibatu 1,r3
        lwzu    r3,4(r31)
        mtibatl 1,r3
        lwzu    r3,4(r31)
        mtdbatu 1,r3
        lwzu    r3,4(r31)
        mtdbatl 1,r3
        lwzu    r3,4(r31)
        mtibatu 2,r3
        lwzu    r3,4(r31)
        mtibatl 2,r3
        lwzu    r3,4(r31)
        mtdbatu 2,r3
        lwzu    r3,4(r31)
        mtdbatl 2,r3
        lwzu    r3,4(r31)
        mtibatu 3,r3
        lwzu    r3,4(r31)
        mtibatl 3,r3
        lwzu    r3,4(r31)
        mtdbatu 3,r3
        lwzu    r3,4(r31)
        mtdbatl 3,r3
#endif
#if HAVE_SEGMENTS
        lwzu    r3,4(r31)
        mtsr    0,r3
        isync
        lwzu    r3,4(r31)
        mtsr    1,r3
        isync
        lwzu    r3,4(r31)
        mtsr    2,r3
        isync
        lwzu    r3,4(r31)
        mtsr    3,r3
        isync
        lwzu    r3,4(r31)
        mtsr    4,r3
        isync
        lwzu    r3,4(r31)
        mtsr    5,r3
        isync
        lwzu    r3,4(r31)
        mtsr    6,r3
        isync
        lwzu    r3,4(r31)
        mtsr    7,r3
        isync
        lwzu    r3,4(r31)
        mtsr    8,r3
        isync
        lwzu    r3,4(r31)
        mtsr    9,r3
        isync
        lwzu    r3,4(r31)
        mtsr    10,r3
        isync
        lwzu    r3,4(r31)
        mtsr    11,r3
        isync
        lwzu    r3,4(r31)
        mtsr    12,r3
        isync
        lwzu    r3,4(r31)
        mtsr    13,r3
        isync
        lwzu    r3,4(r31)
        mtsr    14,r3
        isync
        lwzu    r3,4(r31)
        mtsr    15,r3
        isync
#endif
"       mr      %%r31, %%r0                         \n"
"       blr                                         \n"
::);
}

#if 0


/*********** SUPPORT *************/

static inline __attribute__((always_inline)) uint64_t BE64(uint64_t x)
{
    return x;
}

static inline __attribute__((always_inline)) uint64_t LE64(uint64_t x)
{
    return __builtin_bswap64(x);
}

static inline __attribute__((always_inline)) uint32_t BE32(uint32_t x)
{
    return x;
}

static inline __attribute__((always_inline)) uint32_t LE32(uint32_t x)
{
    return __builtin_bswap32(x);
}

static inline __attribute__((always_inline)) uint16_t BE16(uint16_t x)
{
    return x;
}

static inline __attribute__((always_inline)) uint16_t LE16(uint16_t x)
{
    return __builtin_bswap16(x);
}


static inline uint32_t rd32le(uint32_t iobase) {
    return LE32(*(volatile uint32_t *)(iobase));
}

static inline uint32_t rd32be(uint32_t iobase) {
    return BE32(*(volatile uint32_t *)(iobase));
}

static inline uint16_t rd16le(uint32_t iobase) {
    return LE16(*(volatile uint16_t *)(iobase));
}

static inline uint16_t rd16be(uint32_t iobase) {
    return BE16(*(volatile uint16_t *)(iobase));
}

static inline uint8_t rd8(uint32_t iobase) {
    return *(volatile uint8_t *)(iobase);
}

static inline void wr32le(uint32_t iobase, uint32_t value) {
    *(volatile uint32_t *)(iobase) = LE32(value);
}

static inline void wr32be(uint32_t iobase, uint32_t value) {
    *(volatile uint32_t *)(iobase) = BE32(value);
}

static inline void wr16le(uint32_t iobase, uint16_t value) {
    *(volatile uint16_t *)(iobase) = LE16(value);
}

static inline void wr16be(uint32_t iobase, uint16_t value) {
    *(volatile uint16_t *)(iobase) = BE16(value);
}

static inline void wr8(uint32_t iobase, uint8_t value) {
    *(volatile uint8_t *)(iobase) = value;
}

typedef void (*putc_func)(void *data, char c);


char *
strcpy(char *s1, const char *s2)
{
    char *s = s1;
    while ((*s++ = *s2++) != 0)
	;
    return (s1);
}

int
strcmp(const char *s1, const char *s2)
{
    for ( ; *s1 == *s2; s1++, s2++)
	if (*s1 == '\0')
	    return 0;
    return ((*(unsigned char *)s1 < *(unsigned char *)s2) ? -1 : +1);
}

int int_strlen(char *buf)
{
    int len = 0;

    if (buf)
        while(*buf++)
            len++;

    return len;
}


void int_itoa(char *buf, char base, uintptr_t value, char zero_pad, int precision, int size_mod, char big, int alternate_form, int neg, char sign)
{
    int length = 0;

    do {
        char c = value % base;

        if (c >= 10) {
            if (big)
                c += 'A'-10;
            else
                c += 'a'-10;
        }
        else
            c += '0';

        value = value / base;
        buf[length++] = c;
    } while(value != 0);

    if (precision != 0)
    {
        while (length < precision)
            buf[length++] = '0';
    }
    else if (size_mod != 0 && zero_pad)
    {
        int sz_mod = size_mod;
        if (alternate_form)
        {
            if (base == 16) sz_mod -= 2;
            else if (base == 8) sz_mod -= 1;
        }
        if (neg)
            sz_mod -= 1;

        while (length < sz_mod)
            buf[length++] = '0';
    }
    if (alternate_form)
    {
        if (base == 8)
            buf[length++] = '0';
        if (base == 16) {
            buf[length++] = big ? 'X' : 'x';
            buf[length++] = '0';
        }
    }

    if (neg)
        buf[length++] = '-';
    else {
        if (sign == '+')
            buf[length++] = '+';
        else if (sign == ' ')
            buf[length++] = ' ';
    }

    for (int i=0; i < length/2; i++)
    {
        char tmp = buf[i];
        buf[i] = buf[length - i - 1];
        buf[length - i - 1] = tmp;
    }

    buf[length] = 0;
}

void vkprintf_pc(putc_func putc_f, void *putc_data, const char * restrict format, va_list args)
{
    char tmpbuf[32];

    while(*format)
    {
        char c;
        char alternate_form = 0;
        int size_mod = 0;
        int length_mod = 0;
        int precision = 0;
        char zero_pad = 0;
        char *str;
        char sign = 0;
        char leftalign = 0;
        uintptr_t value = 0;
        intptr_t ivalue = 0;

        char big = 0;

        c = *format++;

        if (c != '%')
        {
            putc_f(putc_data, c);
        }
        else
        {
            c = *format++;

            if (c == '#') {
                alternate_form = 1;
                c = *format++;
            }

            if (c == '-') {
                leftalign = 1;
                c = *format++;
            }

            if (c == ' ' || c == '+') {
                sign = c;
                c = *format++;
            }

            if (c == '0') {
                zero_pad = 1;
                c = *format++;
            }

            while(c >= '0' && c <= '9') {
                size_mod = size_mod * 10;
                size_mod = size_mod + c - '0';
                c = *format++;
            }

            if (c == '.') {
                c = *format++;
                while(c >= '0' && c <= '9') {
                    precision = precision * 10;
                    precision = precision + c - '0';
                    c = *format++;
                }
            }

            big = 0;

            if (c == 'h')
            {
                c = *format++;
                if (c == 'h')
                {
                    c = *format++;
                    length_mod = 1;
                }
                else length_mod = 2;
            }
            else if (c == 'l')
            {
                c = *format++;
                if (c == 'l')
                {
                    c = *format++;
                    length_mod = 8;
                }
                else length_mod = 4;
            }
            else if (c == 'j')
            {
                c = *format++;
                length_mod = 9;
            }
            else if (c == 't')
            {
                c = *format++;
                length_mod = 10;
            }
            else if (c == 'z')
            {
                c = *format++;
                length_mod = 11;
            }

            switch (c) {
                case 0:
                    return;

                case '%':
                    putc_f(putc_data, '%');
                    break;

                case 'X':
                    big = 1;
                    /* fallthrough */
                case 'x':
                    switch (length_mod) {
                        case 8:
                            value = va_arg(args, uint64_t);
                            break;
                        case 9:
                            value = va_arg(args, uintmax_t);
                            break;
                        case 10:
                            value = va_arg(args, uintptr_t);
                            break;
                        case 11:
                            value = va_arg(args, size_t);
                            break;
                        default:
                            value = va_arg(args, unsigned int);
                            break;
                    }
                    int_itoa(tmpbuf, 16, value, zero_pad, precision, size_mod, big, alternate_form, 0, sign);
                    str = tmpbuf;
                    size_mod -= int_strlen(str);
                    if (!leftalign)
                        while(size_mod-- > 0)
                            putc_f(putc_data, ' ');
                    do {
                        putc_f(putc_data, *str);
                    } while(*str++);
                    if (leftalign)
                        while(size_mod-- > 0)
                            putc_f(putc_data, ' ');

                    break;

                case 'u':
                    switch (length_mod) {
                        case 8:
                            value = va_arg(args, uint64_t);
                            break;
                        case 9:
                            value = va_arg(args, uintmax_t);
                            break;
                        case 10:
                            value = va_arg(args, uintptr_t);
                            break;
                        case 11:
                            value = va_arg(args, size_t);
                            break;
                        default:
                            value = va_arg(args, unsigned int);
                            break;
                    }
                    int_itoa(tmpbuf, 10, value, zero_pad, precision, size_mod, 0, alternate_form, 0, sign);
                    str = tmpbuf;
                    size_mod -= int_strlen(str);
                    if (!leftalign)
                        while(size_mod-- > 0)
                            putc_f(putc_data, ' ');
                    do {
                        putc_f(putc_data, *str);
                    } while(*str++);
                    if (leftalign)
                        while(size_mod-- > 0)
                            putc_f(putc_data, ' ');
                    break;

                case 'd':
                case 'i':
                    switch (length_mod) {
                        case 8:
                            ivalue = va_arg(args, int64_t);
                            break;
                        case 9:
                            ivalue = va_arg(args, intmax_t);
                            break;
                        case 10:
                            ivalue = va_arg(args, intptr_t);
                            break;
                        case 11:
                            ivalue = va_arg(args, size_t);
                            break;
                        default:
                            ivalue = va_arg(args, int);
                            break;
                    }
                    if (ivalue < 0)
                        int_itoa(tmpbuf, 10, -ivalue, zero_pad, precision, size_mod, 0, alternate_form, 1, sign);
                    else
                        int_itoa(tmpbuf, 10, ivalue, zero_pad, precision, size_mod, 0, alternate_form, 0, sign);
                    str = tmpbuf;
                    size_mod -= int_strlen(str);
                    if (!leftalign)
                        while(size_mod-- > 0)
                            putc_f(putc_data, ' ');
                    do {
                        putc_f(putc_data, *str);
                    } while(*str++);
                    if (leftalign)
                        while(size_mod-- > 0)
                            putc_f(putc_data, ' ');
                    break;

                case 'o':
                    switch (length_mod) {
                        case 8:
                            value = va_arg(args, uint64_t);
                            break;
                        case 9:
                            value = va_arg(args, uintmax_t);
                            break;
                        case 10:
                            value = va_arg(args, uintptr_t);
                            break;
                        case 11:
                            value = va_arg(args, size_t);
                            break;
                        default:
                            value = va_arg(args, uint32_t);
                            break;
                    }
                    int_itoa(tmpbuf, 8, value, zero_pad, precision, size_mod, 0, alternate_form, 0, sign);
                    str = tmpbuf;
                    size_mod -= int_strlen(str);
                    if (!leftalign)
                        while(size_mod-- > 0)
                            putc_f(putc_data, ' ');
                    do {
                        putc_f(putc_data, *str);
                    } while(*str++);
                    if (leftalign)
                        while(size_mod-- > 0)
                            putc_f(putc_data, ' ');
                    break;

                case 'c':
                    putc_f(putc_data, va_arg(args, int));
                    break;

                case 's':
                    {
                        str = va_arg(args, char *);
                        do {
                            if (*str == 0)
                                break;
                            else
                                putc_f(putc_data, *str);
                        } while(*str++ && --precision);
                    }
                    break;

                default:
                    putc_f(putc_data, c);
                    break;
            }
        }
    }
}

void putByte(void *data, char c)
{
    (void)data;
    *(volatile uint8_t *)0xdeadbeef = c;
}

void kprintf(const char * restrict format, ...)
{
    va_list v;
    va_start(v, format);
    vkprintf_pc(putByte, 0, format, v);
    va_end(v);
}

asm(
"delay_loop:    \n"
"    mtctr   %r3 \n"     /* number of iterations in r3 */
"1:  bdnz+    1b \n"
"    blr");

void delay_loop(uint32_t count);

void GetBogoMIPS()
{
    uint32_t Begin_Time, End_Time;

    asm volatile("lwbrx %0, 0, %1":"=r"(Begin_Time):"r"(0xf2003004));
    delay_loop(100000000);
    asm volatile("lwbrx %0, 0, %1":"=r"(End_Time):"r"(0xf2003004));

    kprintf("100000000 loop cycles in %d us -> %d BogoMIPS\n", End_Time - Begin_Time, 100000000 / (End_Time - Begin_Time));
}

void foo()
{
    asm volatile("sc");
    asm volatile("icbi %r0, %r0");
    GetBogoMIPS();
    asm volatile("icbi %r0, %r0");
    GetBogoMIPS();
    while(1);
}

void PPC_C_Init(uint16_t *framebuffer, uint32_t fb_width, uint32_t fb_height, uint32_t pitch)
{
    uint32_t Begin_Time, End_Time;
    uint32_t Begin_Cycles, End_Cycles;

    kprintf("Hello, PPC\n");
    kprintf("Here is Emu68, %s, speaking ;)\n", "or maybe EmuPPC");
    kprintf("Testing literals:\n");
    kprintf("  %%s: %s\n", "this is a text");
    kprintf("  %%c: %c\n", 'A');
    kprintf("  %%d: %d\n", 1536);
    kprintf("  %%x: %x\n", 0xdeadbeef);

    GetBogoMIPS();

    uint32_t start, end;

    asm volatile("mfspr %0, 900":"=r"(start));

    asm volatile("lwbrx %0, 0, %1":"=r"(Begin_Time):"r"(0xf2003004));
    asm volatile("mftbl %0":"=r"(Begin_Cycles));

    const uint32_t w = 400;
    const uint32_t h = 300;
    const uint32_t start_x = (fb_width - w) / 2;
    const uint32_t start_y = (fb_height - h) / 2;
    (void)pitch;
    uint32_t c = 0;
    
    for (int i=0; i < 10000; i++)
    {
        uint16_t *ptr = framebuffer + start_y * fb_width + start_x;

        for (unsigned y = 0; y < h; y++) {
            for (unsigned x=0; x < w; x++) {
                ptr[x] = c++;
            }
            ptr += fb_width;
        }
    }
    asm volatile("icbi %r0, %r0");
    kprintf("%d\n", c);

    asm volatile("lwbrx %0, 0, %1":"=r"(End_Time):"r"(0xf2003004));
    asm volatile("mftbl %0":"=r"(End_Cycles));

    asm volatile("mfspr %0, 900":"=r"(end));

    kprintf("Test loop time: %d us\n", End_Time - Begin_Time);
    kprintf("Test loop cycles: %d\n", End_Cycles - Begin_Cycles);
    kprintf("Test loop instructions: %u\n", end - start);
    uint32_t speed = (((end - start) / ((End_Time - Begin_Time) / 1000)) ) / 100;
    kprintf("Test loop speed: %u.%u MIPS\n", speed / 10, speed % 10);

    asm volatile("mtsrr0 %0; mtsrr1 %1; rfi"::"r"(foo), "r"(1 << 14));
}

#endif