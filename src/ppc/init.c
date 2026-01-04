#include <stdint.h>
#include <stdarg.h>
#include <stddef.h>

#include "powerpc.h"
#include "libstructs.h"
#include "version.h"
#include "support.h"

void  __attribute__((used,aligned(256),section(".vectors"))) __stub_vectors() {
__asm__(
"       .section .vectors               \n"
"       .byte 0                         \n"
"       .balign 16                      \n"
"       .ascii  \"" VERSION_STRING "\"  \n"
"       .byte 0                         \n"
"       .balign 16                      \n"
"       .ascii \"Based on WarpOS project for Sonnet cards by Dennis van der Boon\"\n"
"       .byte 0                         \n"
"       .org 0x100,0                    \n"
"SystemReset:                           \n"
"       lis %r1, 0xffef                 \n" // Set up stack to a known address - Emu68 keeps it safe
"       ori %r1, %r1, 0xff00            \n"
"       bl Start                        \n"
"1:     b 1b                            \n"

"       .org 0x200,0                    \n"
"MachineCheck:                          \n"
"       mtsprg3 %r0                     \n" // Store r0
"       mflr %r0                        \n" // Back up original LR in r0
"       bl ExceptionEntry               \n" // Jump updating LR, this way ExceptionEntry knows vector number

"       .org 0x300,0                    \n"
"DSI:                                   \n"
"       mtsprg3 %r0                     \n"
"       mflr %r0                        \n"
"       bl ExceptionEntry               \n"

"       .org 0x400,0                    \n"
"ISI:                                   \n"
"       mtsprg3 %r0                     \n"
"       mflr %r0                        \n"
"       bl ExceptionEntry               \n"

"       .org 0x500,0                    \n"
"ExternalInt:                           \n"
"       mtsprg3 %r0                     \n"
"       mflr %r0                        \n"
"       bl ExceptionEntry               \n"

"       .org 0x600,0                    \n"
"Alignment:                             \n"
"       mtsprg3 %r0                     \n"
"       mflr %r0                        \n"
"       bl ExceptionEntry               \n"

"       .org 0x700,0                    \n"
"Program:                               \n"
"       mtsprg3 %r0                     \n"
"       mflr %r0                        \n"
"       bl ExceptionEntry               \n"

"       .org 0x800,0                    \n"
"FPU:                                   \n"
"       mtsprg3 %r0                     \n"
"       mflr %r0                        \n"
"       bl ExceptionEntry               \n"

"       .org 0x900,0                    \n"
"Decrementer:                           \n"
"       mtsprg3 %r0                     \n"
"       mflr %r0                        \n"
"       bl ExceptionEntry               \n"

"       .org 0xc00,0                    \n"
"SysCall:                               \n"
"       mtsprg3 %r0                     \n"
"       mflr %r0                        \n"
"       bl ExceptionEntry               \n"

"       .org 0xd00,0                    \n"
"Trace:                                 \n"
"       mtsprg3 %r0                     \n"
"       mflr %r0                        \n"
"       bl ExceptionEntry               \n"

"       .org 0xe00,0                    \n"
"FPU_Assist:                            \n"
"       mtsprg3 %r0                     \n"
"       mflr %r0                        \n"
"       bl ExceptionEntry               \n"

/* Padding */
"       .org 0x2ff0,0                   \n"
);
}

void __attribute__((used)) __stub_exception_entry() {
asm volatile(
"       .globl ExceptionEntry                           \n"
"ExceptionEntry:                                        \n"
"       mtsprg2 %%r0                                    \n" // SPRG3 == r0, SPRG2 == lr

"       mfsrr0  %%r0                                    \n"
"       mtsprg0 %%r0                                    \n" // SPRG0 == SRR0
"       mfsrr1  %%r0                                    \n"
"       mtsprg1 %%r0                                    \n" // SPRG1 == SRR1

"       mfmsr   %%r0                                    \n"
"       ori     %%r0, %%r0, %[MSR_FLAGS]                \n"
"       mtmsr   %%r0                                    \n" // Reenable MMU
"       sync                                            \n" // Also reenable FPU
"       isync                                           \n"

"       stwu    %%r1, -%[STACK_ALLOC](%%r1)             \n" // Create frame on stack large enough to skip red zone

"       mfcr    %%r0                                    \n" // Back-up CR on stack
"       stw     %%r0, 4(%%r1)                           \n"
"       stw     %%r4, 8(%%r1)                           \n" // Back-up r4 on stack

"       mfspr   %%r4, %[BASEREG]                        \n" // Load PowerPC base from special SPR
"       lwz     %%r4, %[POWERPCBASE_THISPPCTASK](%%r4)  \n" // Get PowerPC ThisTask
"       mr.     %%r4, %%r4                              \n" // Check if null
"       bne     .NoIdl                                  \n" // If ThisPPCTask is not null, we are not idling

/* PPC's ThisTask is not set (NULL), so store iframe on the stack instead of Task's context */

"       mfspr   %%r4, %[BASEREG]                        \n" // Get PowerPC base again, it was overwritten
"       lwz     %%r4, %[NULL_FRAME](%%r4)               \n" // Get pointer of null frame
"       b       1f                                      \n"

".NoIdl:                                                \n"
"       lwz     %%r4, %[PPCTASK_CONTEXMEM](%%r4)        \n" // iFrame
"1:     lwz     %%r0, 8(%%r1)                           \n"
"       stw     %%r0, %[IF_CONTEXT_GPR4](%%r4)          \n"
"       stw     %%r3, %[IF_CONTEXT_GPR3](%%r4)          \n"
"       mfsprg3 %%r0                                    \n"
"       stw     %%r0, %[IF_CONTEXT_GPR0](%%r4)          \n"
"       mr      %%r3, %%r4                              \n"

".DoExc:                                                \n" 
"       mflr    %%r0                                    \n"
"       stw     %%r0, %[IF_CONTEXT](%%r3)               \n"
"       bl      StoreFrame                              \n" // r0, r3 and r4 are skipped in this routine and were saved above

"       mfspr   %%r3, %[BASEREG]                        \n" // Loads PowerPCBase
"       bl      Exception_Entry                         \n"

"       mfspr   %%r31, %[BASEREG]                       \n"
"       lwz     %%r31, %[POWERPCBASE_THISPPCTASK](%%r31)\n"
"       mr.     %%r31, %%r31                            \n"
"       bne+    .NI2                                    \n" // trash everything, is idle task.

"       mfspr   %%r31, %[BASEREG]                       \n"
"       lwz     %%r31, %[NULL_FRAME](%%r31)             \n" // go to idle, don't care about registers
"       b       ExceptionExit                           \n"

".NI2:  lwz     %%r31, %[PPCTASK_CONTEXMEM](%%r31)      \n"
"       b       ExceptionExit                           \n"
:
:[POWERPCBASE_THISPPCTASK]"i"(__builtin_offsetof(struct PrivatePPCBase, pp_ThisPPCProc)),
 [PPCTASK_CONTEXMEM]"i"(__builtin_offsetof(struct TaskPPC, tp_ContextMem)),
 [IF_CONTEXT]"i"(__builtin_offsetof(struct iframe, if_Context)),
 [IF_CONTEXT_GPR0]"i"(__builtin_offsetof(struct iframe, if_Context.ec_GPR[0])),
 [IF_CONTEXT_GPR3]"i"(__builtin_offsetof(struct iframe, if_Context.ec_GPR[3])),
 [IF_CONTEXT_GPR4]"i"(__builtin_offsetof(struct iframe, if_Context.ec_GPR[4])),
 [MSR_FLAGS]"i"(MSR_IR|MSR_DR|MSR_FP),
 [BASEREG]"i"(SPR_BASEREG),
 [STACK_ALLOC]"i"(STACK_ALLOC_SIZE),
 [NULL_FRAME]"i"(__builtin_offsetof(struct PrivatePPCBase, pp_iFrame))
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
"       mfsprg0 %%r0                        \n" // SRR0
"       stwu    %%r0, 4(%%r3)               \n" 
"       mfsprg1 %%r0                        \n" // SRR1
"       stwu    %%r0, 4(%%r3)               \n"
"       mfdar   %%r0                        \n" // DAR
"       stwu    %%r0, 4(%%r3)               \n"
"       mfdsisr %%r0                        \n" // DSISR
"       stwu    %%r0, 4(%%r3)               \n"
"       lwz     %%r0, 4(%%r1)               \n" // CR from stack
"       stwu    %%r0, 4(%%r3)               \n"
"       mfctr   %%r0                        \n" // CTR
"       stwu    %%r0, 4(%%r3)               \n"
"       mfsprg2 %%r0                        \n" // LR
"       stwu    %%r0, 4(%%r3)               \n"
#if HAVE_FPU
// TODO: store f0 first!
"       mffs    f0                          \n" // FPSCR is stored as 64 bit!!!
"       stfdu   f0,4(r3)                    \n" // Store and advance R3 by 4 bytes
"       mfxer   r0                          \n" // XER
"       stw     r0,0(r3)                    \n" // r3 points to XER
#else
"       mfxer   %%r0                        \n" // XER
"       stwu    %%r0, 4(%%r3)               \n" // Store with skipping FPSCR, r3 points to XER
#endif
"       lwz     %%r0, 0(%%r1)               \n" //
"       stwu    %%r0, 12(%%r3)              \n" // r1, skipped r0
"       stwu    %%r2, 4(%%r3)               \n"
"       stwu    %%r5, 12(%%r3)              \n" // skipped r3 and r4. Were stored seperately
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
