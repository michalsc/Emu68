/*
    Copyright © 2019-2025 Michal Schulz <michal.schulz@gmx.de>
    https://github.com/michalsc

    This Source Code Form is subject to the terms of the
    Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed
    with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#define _GNU_SOURCE 1

#define restrict __restrict__

#include <emu68/List>
#include <emu68/Node>
#include <emu68/LRUCache>
#include <emu68/ReturnStack>
#include <emu68/GPR>
#include <emu68/FPR>

#include "PPC.h"
#include "intc.h"
#include "A64.h"
#include "disasm.h"
#include "DuffCopy.h"
#include "config.h"
#include "support.h"
#include "tlsf.h"
#include "spinlock.h"
#include "doorbell.h"
#include "cache.h"
#include "mmu.h"

namespace Emu68::PPC {

register uint32_t PC __asm__("w18");
register void (*ARMCode)() __asm__("x12");

#define jit_tlsf DO_NOT_USE_jit_tlsf

TLSF jit_ppc((void*)(0xffffffe000000000 + ((KERNEL_JIT_PAGES / 2) << 21)), (KERNEL_JIT_PAGES / 2) << 21);
PPCTranslatorContext local_translator;
ReturnStack return_stack;

extern List<PPCTranslationUnit> ICache[EMU68_HASHSIZE];
extern List<TranslationUnitLRU> LRU;

PPCLocalState *local_state;

uint32_t *ppc_high;
uint32_t *ppc_low;
uint32_t * ppc_entry_point;
uint32_t debug_range_min = 0x00000000;
uint32_t debug_range_max = 0xffffffff;

namespace Emit {

void setCRnLogic(PPCTranslatorContext *tc, uint8_t cr)
{
    GPR reg_cr = tc->mapGPRForReadAndWrite(CRn);
    GPR tmp = GPR::allocate();

#if PPC_SO_PROPAGATION
    GPR reg_xer = tc->tryGetGPR(XERn);

    /* Shift right XER by 31 so that SO is bit 0 */
    if (reg_xer.isValid())
    {
        /* XER was already in one of GPRs, rotate with tmp as target */
        tc->emit(lsr(tmp, reg_xer, 31));
    }
    else
    {
        /* XER was not mapped, get a copy from PPC context (SIMD register) and rotate */
        tc->emit({
            mov_simd_to_reg(tmp, REG_XER),
            lsr(tmp, tmp, 31)
        });
    }
#endif

    GPR reg_zero_case = GPR::allocate();
    GPR reg_minus_case = GPR::allocate();

    tc->emit({
#if PPC_SO_PROPAGATION
        orr_immed(reg_zero_case, tmp, 1, 31),     // Set EQ flag
        orr_immed(reg_minus_case, tmp, 1, 29),    // Set LT flag
        orr_immed(tmp, tmp, 1, 30),            // Set GT flag
#else
        mov_immed_u16(reg_zero_case, 2, 0),
        mov_immed_u16(reg_minus_case, 8, 0),
        mov_immed_u16(tmp, 4, 0),
#endif
        csel(tmp, reg_zero_case, tmp, A64_CC_EQ),
        csel(tmp, reg_minus_case, tmp, A64_CC_MI),

        /* Insert into CRn */
        bfi(reg_cr, tmp, 4 * (7 - cr), 4)
    });
}

void setCRnLogicNoMinus(PPCTranslatorContext *tc, uint8_t cr)
{
    GPR reg_cr = tc->mapGPRForReadAndWrite(CRn);
    GPR tmp = GPR::allocate();

#if PPC_SO_PROPAGATION
    GPR reg_xer = tc->tryGetGPR(XERn);

    /* Shift right XER by 31 so that SO is bit 0 */
    if (reg_xer.isValid())
    {
        /* XER was already in one of GPRs, rotate with tmp as target */
        tc->emit(lsr(tmp, reg_xer, 31));
    }
    else
    {
        /* XER was not mapped, get a copy from PPC context (SIMD register) and rotate */
        tc->emit({
            mov_simd_to_reg(tmp, REG_XER),
            lsr(tmp, tmp, 31)
        });
    }
#endif

    GPR reg_zero_case = GPR::allocate();

    tc->emit({
#if PPC_SO_PROPAGATION
        orr_immed(reg_zero_case, tmp, 1, 31),     // Set EQ flag
        orr_immed(tmp, tmp, 1, 30),            // Set GT flag
#else
        mov_immed_u16(reg_zero_case, 2, 0),
        mov_immed_u16(tmp, 4, 0),
#endif
        csel(tmp, reg_zero_case, tmp, A64_CC_EQ),

        /* Insert into CRn */
        bfi(reg_cr, tmp, 4 * (7 - cr), 4)
    });
}

void setCRnUnsigned(PPCTranslatorContext *tc, uint8_t cr)
{
    GPR reg_cr = tc->mapGPRForReadAndWrite(CRn);
    GPR tmp = GPR::allocate();

#if PPC_SO_PROPAGATION
    GPR reg_xer = tc->tryGetGPR(XERn);

    /* Shift right XER by 31 so that SO is bit 0 */
    if (reg_xer.isValid())
    {
        /* XER was already in one of GPRs, rotate with tmp as target */
        tc->emit(lsr(tmp, reg_xer, 31));
    }
    else
    {
        /* XER was not mapped, get a copy from PPC context (SIMD register) and rotate */
        tc->emit({
            mov_simd_to_reg(tmp, REG_XER),
            lsr(tmp, tmp, 31)
        });
    }
#endif

    GPR reg_zero_case = GPR::allocate();
    GPR reg_minus_case = GPR::allocate();

    tc->emit({ 
#if PPC_SO_PROPAGATION
        orr_immed(reg_zero_case, tmp, 1, 31),     // Set EQ flag
        orr_immed(reg_minus_case, tmp, 1, 29),    // Set LT flag
        orr_immed(tmp, tmp, 1, 30),            // Set GT flag
#else
        mov_immed_u16(reg_zero_case, 2, 0),
        mov_immed_u16(reg_minus_case, 8, 0),
        mov_immed_u16(tmp, 4, 0),
#endif
        csel(tmp, reg_zero_case, tmp, A64_CC_EQ),
        csel(tmp, reg_minus_case, tmp, A64_CC_CC),

        /* Insert into CRn */
        bfi(reg_cr, tmp, 4 * (7 - cr), 4)
    });
}

void setCRnSigned(PPCTranslatorContext *tc, uint8_t cr)
{
    GPR reg_cr = tc->mapGPRForReadAndWrite(CRn);
    GPR tmp = GPR::allocate();

#if PPC_SO_PROPAGATION
    GPR reg_xer = tc->tryGetGPR(XERn);
    /* Shift right XER by 31 so that SO is bit 0 */
    if (reg_xer.isValid())
    {
        /* XER was already in one of GPRs, rotate with tmp as target */
        tc->emit(lsr(tmp, reg_xer, 31));
    }
    else
    {
        /* XER was not mapped, get a copy from PPC context (SIMD register) and rotate */
        tc->emit({
            mov_simd_to_reg(tmp, REG_XER),
            lsr(tmp, tmp, 31)
        });
    }
#endif

    GPR reg_zero_case = GPR::allocate();
    GPR reg_minus_case = GPR::allocate();

    tc->emit({
#if PPC_SO_PROPAGATION
        orr_immed(reg_zero_case, tmp, 1, 31),     // Set EQ flag
        orr_immed(reg_minus_case, tmp, 1, 29),    // Set LT flag
        orr_immed(tmp, tmp, 1, 30),            // Set GT flag
#else
        mov_immed_u16(reg_zero_case, 2, 0),
        mov_immed_u16(reg_minus_case, 8, 0),
        mov_immed_u16(tmp, 4, 0),
#endif
        csel(tmp, reg_zero_case, tmp, A64_CC_EQ),
        csel(tmp, reg_minus_case, tmp, A64_CC_LT),

        /* Insert into CRn */
        bfi(reg_cr, tmp, 4 * (7 - cr), 4)
    });
}

} // namespace Emit

static __used__ int EMIT_bx(PPCTranslatorContext *tc, uint32_t opcode)
{
    int32_t offset = opcode & 0x03fffffc;
    int update_lr = !!(opcode & 1);
    int is_absolute = !!(opcode & 2);
    int8_t pc_offset = 4;
    struct PPCState *ctx = GET_HOST_CTX();
    uint32_t *old_pc = tc->getPC();
    int32_t var_EMU68_BRANCH_INLINE_DISTANCE = (ctx->JIT_CONTROL >> JCCB_INLINE_RANGE) & JCCB_INLINE_RANGE_MASK;

    tc->getOffsetPC(&pc_offset);

    if (offset & 0x02000000) offset |= 0xfc000000;

    if (update_lr) {
        return_stack.push(tc->getPC() + 1);

        if (pc_offset >= 0) {
            tc->emit( add_immed(REG_LR, REG_PC, pc_offset));
        } else {
            tc->emit( sub_immed(REG_LR, REG_PC, -pc_offset));
        }
    }

    tc->resetOffsetPC();

    if (is_absolute) {
        tc->setPC((uint32_t*)(uintptr_t)(uint32_t)offset);
        tc->emitLoadImmediate(REG_PC, (uint32_t)offset);
    } else {
        tc->setPC(tc->getPC() + (offset >> 2));
        int32_t pc_adj = pc_offset + offset - 4;
        if (pc_adj < 0) {
            pc_adj = -pc_adj;
            if ((pc_adj & 0xfffff000) == 0) {
                tc->emit( sub_immed(REG_PC, REG_PC, pc_adj));
            } else if ((pc_adj & 0xff000fff) == 0) {
                tc->emit( sub_immed_lsl12(REG_PC, REG_PC, pc_adj >> 12));
            } else {
                GPR tmp = GPR::allocate();
                tc->emitLoadImmediate(tmp, (uint32_t)pc_adj);
                tc->emit( sub_reg(REG_PC, REG_PC, tmp, LSL, 0));
            }
        } else if (pc_adj > 0) {
            if ((pc_adj & 0xfffff000) == 0) {
                tc->emit( add_immed(REG_PC, REG_PC, pc_adj));
            } else if ((pc_adj & 0xff000fff) == 0) {
                tc->emit( add_immed_lsl12(REG_PC, REG_PC, pc_adj >> 12));
            } else {
                GPR tmp = GPR::allocate();
                tc->emitLoadImmediate(tmp, (uint32_t)pc_adj);
                tc->emit( add_reg(REG_PC, REG_PC, tmp, LSL, 0));
            }
        } 
    }

    /* If jump distance is larger than allowed, break translation */
    ptrdiff_t distance = 4 * (tc->getPC() - old_pc);
    if (distance > var_EMU68_BRANCH_INLINE_DISTANCE || distance < -var_EMU68_BRANCH_INLINE_DISTANCE) {
        tc->emitStop();
    }

    /* If jump to itself, insert NOP */
    if (distance == 0) tc->emit({ nop(), INSN_TO_LE(MARKER_BREAK) });

    return 1;
}

static __used__ int EMIT_bcx(PPCTranslatorContext *tc, uint32_t opcode)
{
    int32_t offset = opcode & 0x0000fffc;
    uint32_t branch_target;
    int is_absolute = !!(opcode & 2);
    int8_t pc_offset = 4;
    struct PPCState *ctx = GET_HOST_CTX();
    uint32_t *old_pc = tc->getPC();
    int32_t var_EMU68_BRANCH_INLINE_DISTANCE = (ctx->JIT_CONTROL >> JCCB_INLINE_RANGE) & JCCB_INLINE_RANGE_MASK;
    
    GPR tmp = GPR::allocate();

    uint8_t bo = (opcode >> 21) & 31;
    uint8_t bi = (opcode >> 16) & 31;
    uint8_t update_lr = opcode & 1;

    /* Calculate if the branch shall be considered taken */
    uint8_t bo0 = (bo >> 4) & 1;
    uint8_t bo1 = (bo >> 3) & 1;
    uint8_t bo2 = (bo >> 2) & 1;
    uint8_t bo3 = (bo >> 1) & 1;
    uint8_t dec_ctr = bo2 == 0;
    uint8_t condition_true = bo1 == 1;
    uint8_t bo4 = bo & 1;
    uint8_t sign = (opcode >> 15) & 1;
    uint8_t take_branch = 0;

    /* In case of bcx instruction branch is predicted taken if bo4 == 0 for negative jumps, or bo4 == 1 for positive jumps */
    if ((bo4 == 0 && sign != 0) ||
        (bo4 != 0 && sign == 0)) take_branch = 1;

    /* Sign-extend the offset */
    if (offset & 0x00008000) offset |= 0xffff0000;

    if (is_absolute) {
        branch_target = offset;
    } else {
        branch_target = (uint32_t)(uintptr_t)old_pc + offset;
    }

//    kprintf("bcx: bo = %d, bi = %d, take_branch = %d\n", bo, bi, take_branch);
//    kprintf("bcx: branch_target = %08x, old_pc = %08x, offset = %08x\n", branch_target, (uint32_t)(uintptr_t)old_pc, offset);

    tc->getOffsetPC(&pc_offset);

//    kprintf("pc_offset = %d\n", pc_offset);

    if (update_lr) {
        return_stack.push(tc->getPC() + 1);

        if (pc_offset >= 0) {
            tc->emit( add_immed(REG_LR, REG_PC, pc_offset));
        } else {
            tc->emit( sub_immed(REG_LR, REG_PC, -pc_offset));
        }
    }

    tc->resetOffsetPC();

    /* Branch always! */
    if (bo0 && bo2) {
        if (is_absolute) {
            tc->setPC((uint32_t*)(uintptr_t)(uint32_t)offset);
            tc->emitLoadImmediate(REG_PC, (uint32_t)offset);
        } else {
            tc->setPC(tc->getPC() + (offset >> 2));
            int32_t pc_adj = pc_offset + offset - 4;
            tc->emitAddImmediate(REG_PC, pc_adj);
        }
    } else {
        uint8_t success_condition;
        bool use_tbz = false;

        /* BO[2] == 0 - decrement CTR and set condition */
        if (dec_ctr) {
            tc->emit( subs_immed(REG_CTR, REG_CTR, 1));
            /* bo3 == 1 <- take branch if CTR == 0; bo3 == 0 <- take branch if CTR != 0 */
            if (bo3) {
                success_condition = A64_CC_EQ;
                if (bo0 == 0) tc->emit( cset(tmp, A64_CC_EQ));
            } else {
                success_condition = A64_CC_NE;
                if (bo0 == 0) tc->emit( cset(tmp, A64_CC_NE));
            }
            /* if bo0 == 1 there is no need to test condition flags */
            if (bo0 == 0) {
                GPR reg_cr = tc->mapGPRForRead(CRn);
                tc->emit({ 
                    /* Test condition */
                    tst_immed(reg_cr, 1, (1 + bi) & 31),
                    /* Increase tmp if condition is met */
                    cinc(tmp, tmp, condition_true ? A64_CC_NE : A64_CC_EQ),
                    /* If both CTR condition and CR conditions are met, tmp == 2. Test it. */
                    tst_immed(tmp, 1, 31)
                });
                success_condition = A64_CC_NE;
            }
        } else {
            //uint8_t reg_cr = MapGPRForRead(tc, CRn);
            /* Check the condition */
            //tc->emit( tst_immed(reg_cr, 1, (1 + bi) & 31));
            success_condition = condition_true ? A64_CC_NE : A64_CC_EQ;
            use_tbz = true;
        }

        /* If branch is taken by default, invert success condition, since it will jump to local exit point */
        if (take_branch)
        {
            success_condition ^= 1;
            tc->setPC((uint32_t *)(uintptr_t)branch_target);
        }
        else
        {
            tc->setPC(tc->getPC() + 1);
        }

        /* Emit jump, remember its location and fixup type */
        uint32_t fixup_type = use_tbz ? FIXUP_TBZ : FIXUP_BCC;
        
        if (use_tbz) {
            GPR reg_cr = tc->mapGPRForRead(CRn);
            
            tc->emit(
                success_condition == A64_CC_EQ ?
                    tbz(reg_cr, 31 - bi, 0):
                    tbnz(reg_cr, 31 - bi, 0)
            );
        } else { 
            tc->emit( b_cc(success_condition, 0));
        }

        uint32_t *jump_location = tc->tc_CodePtr - 1;

        /* Here the expected code path follows */
        if (take_branch)
        {
            if (is_absolute) {
                tc->emitLoadImmediate(REG_PC, (uint32_t)offset);
            } else {
                int32_t pc_adj = pc_offset + offset - 4;
                tc->emitAddImmediate(REG_PC, pc_adj);
            }
        }
        else
        {
            int32_t pc_adj = pc_offset;
            tc->emitAddImmediate(REG_PC, pc_adj);
        }

        /* Now insert the other code path - this will be treated as exit code */
        uint32_t *exit_code_start = tc->save();

        if (!take_branch)
        {
            if (is_absolute) {
                tc->emitLoadImmediate(REG_PC, (uint32_t)offset);
            } else {
                int32_t pc_adj = pc_offset + offset - 4;
                tc->emitAddImmediate(REG_PC, pc_adj);
            }
        }
        else
        {
            int32_t pc_adj = pc_offset;
            tc->emitAddImmediate(REG_PC, pc_adj);
        }

        /* Insert local exit */
        tc->emitLocalExit(1);
        
        uint32_t *exit_code_end = tc->restore();
        tc->tc_CodePtr = exit_code_end;

        /* Insert fixup location */
        tc->emit({ 
            (uint32_t)(exit_code_end - jump_location),
            fixup_type,
            1,
            (uint32_t)(exit_code_end - exit_code_start),
            INSN_TO_LE(MARKER_EXIT_BLOCK)
        });
    }

    /* If jump distance is larger than allowed, break translation */
    ptrdiff_t distance = 4 * (tc->getPC() - old_pc);
    if (distance > var_EMU68_BRANCH_INLINE_DISTANCE || distance < -var_EMU68_BRANCH_INLINE_DISTANCE) {
        tc->emitStop();
    }

    return 1;
}

static __used__ int EMIT_mcrf(PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 0x63F801) return -1;

    int dst = (opcode >> 23) & 7;
    int src = (opcode >> 18) & 7;

    GPR tmp = GPR::allocate();
    GPR cr = tc->mapGPRForReadAndWrite(CRn);

    tc->emit({
        lsr(tmp, cr, (7 - src) * 4),
        bfi(cr, tmp, (7 - dst) * 4, 4)
    });

    tc->advancePC(4);

    return 1;
}

static __used__ int EMIT_crandc(PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 1) return -1;

    uint8_t crd = (opcode >> 21) & 31;
    uint8_t cra = (opcode >> 16) & 31;
    uint8_t crb = (opcode >> 11) & 31;

    GPR cr_reg = tc->mapGPRForReadAndWrite(CRn);
    GPR tmp = GPR::allocate();

    tc->emit({
        neg_reg(tmp, cr_reg, LSR, (31 - crb)),
        and_reg(tmp, tmp, cr_reg, LSR, (31 - cra)),
        bfi(cr_reg, tmp, 31 - crd, 1)
    });
    
    tc->advancePC(4);

    return 1;
}

static __used__ int EMIT_creqv(PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 1) return -1;

    uint8_t crd = (opcode >> 21) & 31;
    uint8_t cra = (opcode >> 16) & 31;
    uint8_t crb = (opcode >> 11) & 31;

    GPR cr_reg = tc->mapGPRForReadAndWrite(CRn);
    GPR tmp = GPR::allocate();

    if (cra == crb) {
        /* Bit set */
        tc->emit(
            orr_immed(cr_reg, cr_reg, 1, (crd + 1) & 31)
        );
    }
    else {
        tc->emit({
            lsr(tmp, cr_reg, (31 - cra)),
            eon_reg(tmp, tmp, cr_reg, LSR, (31 - crb)),
            bfi(cr_reg, tmp, 31 - crd, 1)
        });
    }
    
    tc->advancePC(4);

    return 1;
}

static __used__ int EMIT_crand(PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 1) return -1;

    uint8_t crd = (opcode >> 21) & 31;
    uint8_t cra = (opcode >> 16) & 31;
    uint8_t crb = (opcode >> 11) & 31;

    GPR cr_reg = tc->mapGPRForReadAndWrite(CRn);
    GPR tmp = GPR::allocate();

    if (cra == crb) {
        /* Bit move */
        tc->emit({
            lsr(tmp, cr_reg, (31 - cra)),
            bfi(cr_reg, tmp, 31 - crd, 1)
        });
    } else {
        tc->emit({
            lsr(tmp, cr_reg, (31 - cra)),
            and_reg(tmp, tmp, cr_reg, LSR, (31 - crb)),
            bfi(cr_reg, tmp, 31 - crd, 1)
        });
    }

    tc->advancePC(4);

    return 1;
}

static __used__ int EMIT_crnand(PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 1) return -1;

    uint8_t crd = (opcode >> 21) & 31;
    uint8_t cra = (opcode >> 16) & 31;
    uint8_t crb = (opcode >> 11) & 31;

    GPR cr_reg = tc->mapGPRForReadAndWrite(CRn);
    GPR tmp = GPR::allocate();

    tc->emit({
        lsr(tmp, cr_reg, (31 - cra)),
        and_reg(tmp, tmp, cr_reg, LSR, (31 - crb)),
        neg_reg(tmp, tmp, LSL, 0),
        bfi(cr_reg, tmp, 31 - crd, 1)
    });
    
    tc->advancePC(4);

    return 1;
}

static __used__ int EMIT_crnor(PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 1) return -1;

    uint8_t crd = (opcode >> 21) & 31;
    uint8_t cra = (opcode >> 16) & 31;
    uint8_t crb = (opcode >> 11) & 31;

    GPR cr_reg = tc->mapGPRForReadAndWrite(CRn);
    GPR tmp = GPR::allocate();

    if (cra == crb) {
        /* Bit change */
        tc->emit(
            eor_immed(cr_reg, cr_reg, 1, (crd + 1) & 31)
        );
    } else {
        tc->emit({
            lsr(tmp, cr_reg, (31 - cra)),
            orr_reg(tmp, tmp, cr_reg, LSR, (31 - crb)),
            neg_reg(tmp, tmp, LSL, 0),
            bfi(cr_reg, tmp, 31 - crd, 1)
        });
    }
    
    tc->advancePC(4);

    return 1;
}

static __used__ int EMIT_cror(PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 1) return -1;

    uint8_t crd = (opcode >> 21) & 31;
    uint8_t cra = (opcode >> 16) & 31;
    uint8_t crb = (opcode >> 11) & 31;

    GPR cr_reg = tc->mapGPRForReadAndWrite(CRn);
    GPR tmp = GPR::allocate();

    if (cra == crb) {
        /* Bit move */
        tc->emit({
            lsr(tmp, cr_reg, (31 - cra)),
            bfi(cr_reg, tmp, 31 - crd, 1)
        });
    } else {
        tc->emit({
            lsr(tmp, cr_reg, (31 - cra)),
            orr_reg(tmp, tmp, cr_reg, LSR, (31 - crb)),
            bfi(cr_reg, tmp, 31 - crd, 1)
        });
    }
    
    tc->advancePC(4);

    return 1;
}

static __used__ int EMIT_crorc(PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 1) return -1;

    uint8_t crd = (opcode >> 21) & 31;
    uint8_t cra = (opcode >> 16) & 31;
    uint8_t crb = (opcode >> 11) & 31;

    GPR cr_reg = tc->mapGPRForReadAndWrite(CRn);
    GPR tmp = GPR::allocate();

    if (cra == crb) {
        /* Bit set */
        tc->emit(
            orr_immed(cr_reg, cr_reg, 1, (crd + 1) & 31)
        );
    } else {
        tc->emit({
            lsr(tmp, cr_reg, (31 - cra)),
            orn_reg(tmp, tmp, cr_reg, LSR, (31 - crb)),
            bfi(cr_reg, tmp, 31 - crd, 1)
        });
    }
    
    tc->advancePC(4);

    return 1;
}

static __used__ int EMIT_crxor(PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 1) return -1;

    uint8_t crd = (opcode >> 21) & 31;
    uint8_t cra = (opcode >> 16) & 31;
    uint8_t crb = (opcode >> 11) & 31;

    GPR cr_reg = tc->mapGPRForReadAndWrite(CRn);

    if (cra == crb) {
        /* Bit clear */
        tc->emit(
            bic_immed(cr_reg, cr_reg, 1, (crd + 1) & 31)
        );
    } else {
        GPR tmp = GPR::allocate();
        tc->emit({
            lsr(tmp, cr_reg, (31 - cra)),
            orr_reg(tmp, tmp, cr_reg, LSR, (31 - crb)),
            bfi(cr_reg, tmp, 31 - crd, 1)
        });
    }

    tc->advancePC(4);

    return 1;
}

static __used__ int EMIT_bclrx(PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Stanity check */
    if (opcode & 0x0000f800) return -1;

    uint8_t bo = (opcode >> 21) & 31;
    uint8_t bi = (opcode >> 16) & 31;
    uint8_t update_lr = opcode & 1;

    /* Calculate if the branch shall be considered taken */
    uint8_t bo0 = (bo >> 4) & 1;
    uint8_t bo1 = (bo >> 3) & 1;
    uint8_t bo2 = (bo >> 2) & 1;
    uint8_t bo3 = (bo >> 1) & 1;
    uint8_t dec_ctr = bo2 == 0;
    uint8_t condition_true = bo1 == 1;
    uint8_t take_branch = (bo & 1);

    /* Branch always */
    if ((bo & 0b10100) == 0b10100) {
        bool success = 0;
        uint32_t *last_pc = return_stack.pop(&success);

        /* if LR needs to be updated, do it now */
        if (update_lr) {
            int8_t pc_offset = 4;
            GPR tmp = GPR::allocate();

            tc->getOffsetPC(&pc_offset);

            return_stack.push(tc->getPC() + 1);

            tc->emit( bic_immed(tmp, REG_LR, 2, 0));

            if (pc_offset >= 0) {
                tc->emit( add_immed(REG_LR, REG_PC, pc_offset));
            } else {
                tc->emit( sub_immed(REG_LR, REG_PC, -pc_offset));
            }

            tc->emit( mov_reg(REG_PC, tmp));
        }
        else
        {
            /* Move LR to PC */
            tc->emit( bic_immed(REG_PC, REG_LR, 2, 0));
        }

        if (success) {
            tc->setPC(last_pc);
        } else {
            /* The return address stack was not available, stop now */
            tc->emitStop();
        }

        tc->resetOffsetPC();
    } else {
        bool success = 0;
        uint8_t success_condition;
        GPR tmp = GPR::allocate();
        uint32_t *last_pc = return_stack.pop(&success);
        int8_t pc_offset = 4;
        
        return_stack.reset();

        tc->getOffsetPC(&pc_offset);
        tc->resetOffsetPC();

        /* BO[2] == 0 - decrement CTR and set condition */
        if (dec_ctr) {
            tc->emit(subs_immed(REG_CTR, REG_CTR, 1));
            /* bo3 == 1 <- take branch if CTR == 0; bo3 == 0 <- take branch if CTR != 0 */
            if (bo3) {
                success_condition = A64_CC_EQ;
                if (bo0 == 0) tc->emit(cset(tmp, A64_CC_EQ));
            } else {
                success_condition = A64_CC_NE;
                if (bo0 == 0) tc->emit(cset(tmp, A64_CC_NE));
            }
            /* if bo0 == 1 there is no need to test condition flags */
            if (bo0 == 0) {
                GPR reg_cr = tc->mapGPRForRead(CRn);
                tc->emit({ 
                    /* Test condition */
                    tst_immed(reg_cr, 1, (1 + bi) & 31),
                    /* Increase tmp if condition is met */
                    cinc(tmp, tmp, condition_true ? A64_CC_NE : A64_CC_EQ),
                    /* If both CTR condition and CR conditions are met, tmp == 2. Test it. */
                    tst_immed(tmp, 1, 31)
                });
                success_condition = A64_CC_NE;
            }
        } else {
            GPR reg_cr = tc->mapGPRForRead(CRn);
            /* Check the condition */
            tc->emit(tst_immed(reg_cr, 1, (1 + bi) & 31));
            success_condition = condition_true ? A64_CC_NE : A64_CC_EQ;
        }

        /* If branch is taken by default, invert success condition, since it will jump to local exit point */
        if (take_branch) {
            success_condition ^= 1;
        }

        /* Emit jump, remember its location and fixup type */
        uint32_t fixup_type = FIXUP_BCC;
        uint32_t *jump_location = tc->tc_CodePtr;
        tc->emit( b_cc(success_condition, 0));

        /* Here is expected code path */
        if (take_branch)
        {
            /* if LR needs to be updated, do it now */
            if (update_lr) {
                GPR tmp = GPR::allocate();

                return_stack.push(tc->getPC() + 1);

                tc->emit( bic_immed(tmp, REG_LR, 2, 0));

                if (pc_offset >= 0) {
                    tc->emit( add_immed(REG_LR, REG_PC, pc_offset));
                } else {
                    tc->emit( sub_immed(REG_LR, REG_PC, -pc_offset));
                }

                tc->emit( mov_reg(REG_PC, tmp));
            }
            else
            {
                /* Move LR to PC */
                tc->emit( bic_immed(REG_PC, REG_LR, 2, 0));
            }

            if (success) {
                tc->setPC(last_pc);
            } else {
                /* The return address stack was not available, stop now */
                tc->emitStop();
            }
        }
        else
        {
            int32_t pc_adj = pc_offset;
            tc->emitAddImmediate(REG_PC, pc_adj);
            tc->setPC(tc->getPC() + 1);
        }

        /* Now insert the other code path - this will be treated as exit code */
        uint32_t *exit_code_start = tc->save();
        
        if (!take_branch)
        {
            /* if LR needs to be updated, do it now */
            if (update_lr) {
                GPR tmp = GPR::allocate();

                return_stack.push(tc->getPC() + 1);

                tc->emit( bic_immed(tmp, REG_LR, 2, 0));

                if (pc_offset >= 0) {
                    tc->emit( add_immed(REG_LR, REG_PC, pc_offset));
                } else {
                    tc->emit( sub_immed(REG_LR, REG_PC, -pc_offset));
                }

                tc->emit( mov_reg(REG_PC, tmp));
            }
            else
            {
                /* Move LR to PC */
                tc->emit( bic_immed(REG_PC, REG_LR, 2, 0));
            }
        }
        else
        {
            int32_t pc_adj = pc_offset;
            tc->emitAddImmediate(REG_PC, pc_adj);
        }

        tc->resetOffsetPC();

        /* Insert local exit */
        tc->emitLocalExit(1);
        
        uint32_t *exit_code_end = tc->restore();
        tc->tc_CodePtr = exit_code_end;

        /* Insert fixup location */
        tc->emit({ 
            (uint32_t)(exit_code_end - jump_location),
            fixup_type,
            1,
            (uint32_t)(exit_code_end - exit_code_start),
            INSN_TO_LE(MARKER_EXIT_BLOCK)
        });

        #if 0
        
        if (take_branch)
        {
            success_condition ^= 1;
            tc->tc_PPCCodePtr = (uint32_t *)(uintptr_t)branch_target;
        }
        else
        {
            tc->tc_PPCCodePtr++;
        }

        /* Here the expected code path follows */
        if (take_branch)
        {
            if (is_absolute) {
                tc->emitLoadImmediate(REG_PC, (uint32_t)offset);
            } else {
                int32_t pc_adj = pc_offset + offset - 4;
                tc->emitAddImmediate(REG_PC, pc_adj);
            }
        }
        else
        {
            int32_t pc_adj = pc_offset;
            tc->emitAddImmediate(REG_PC, pc_adj);
        }

        /* Now insert the other code path - this will be treated as exit code */
        uint32_t *exit_code_start = tc->tc_CodePtr;

        if (!take_branch)
        {
            if (is_absolute) {
                tc->emitLoadImmediate(REG_PC, (uint32_t)offset);
            } else {
                int32_t pc_adj = pc_offset + offset - 4;
                tc->emitAddImmediate(REG_PC, pc_adj);
            }
        }
        else
        {
            int32_t pc_adj = pc_offset;
            tc->emitAddImmediate(REG_PC, pc_adj);
        }

        /* Insert local exit */
        tc->emitLocalExit(1);
        uint32_t *exit_code_end = tc->tc_CodePtr;

        /* Insert fixup location */
        tc->emit({ 
            (uint32_t)(exit_code_end - jump_location),
            fixup_type,
            1,
            (uint32_t)(exit_code_end - exit_code_start),
            INSN_TO_LE(MARKER_EXIT_BLOCK)
        });
        #endif
        
//        (void)bi;
//        (void)take_branch;
        return 1;
    }

    return 1;
}

static __used__ int EMIT_bcctrx(PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Stanity check */
    if (opcode & 0x0000f800) return -1;

    uint8_t bo = (opcode >> 21) & 31;
    uint8_t bi = (opcode >> 16) & 31;
    uint8_t update_lr = opcode & 1;

    /* Calculate if the branch shall be considered taken */
    uint8_t bo0 = (bo >> 4) & 1;
    uint8_t bo2 = (bo >> 2) & 1;
    uint8_t bo4 = bo & 1;
    uint8_t sign = (opcode >> 15) & 1;
    uint8_t take_branch = ((bo0 & bo2) | sign) == bo4;

    /* Branch always */
    if ((bo & 0b10100) == 0b10100) {
        /* if LR needs to be updated, do it now */
        if (update_lr) {
            int8_t pc_offset = 4;

            tc->getOffsetPC(&pc_offset);

            if (pc_offset >= 0) {
                tc->emit( add_immed(REG_LR, REG_PC, pc_offset));
            } else {
                tc->emit( sub_immed(REG_LR, REG_PC, -pc_offset));
            }

            tc->emit( bic_immed(REG_PC, REG_CTR, 2, 0));
        }
        else
        {
            /* Move LR to PC */
            tc->emit( bic_immed(REG_PC, REG_CTR, 2, 0));
        }

        /* The return address stack was not available, stop now */
        tc->emitStop();

        tc->resetOffsetPC();
    } else {
        (void)bi;
        (void)take_branch;
        return -1;
    }

    return 1;
}

static __used__ int EMIT_mfcr(PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Stanity check */
    if (opcode & 0x001ff801) return -1;

    uint8_t rd = (opcode >> 21) & 31;
    GPR reg_rd = tc->mapGPRForWrite(rd);
    GPR reg_cr = tc->tryGetGPR(CRn);

    if (reg_cr.isValid()) {
        tc->emit( mov_reg(reg_rd, reg_cr));
    } else {
        tc->emit( mov_simd_to_reg(reg_rd, REG_CR));
    }

    tc->advancePC(4);

    return 1;
}

static __used__ int EMIT_mcrxr(PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Stanity check */
    if (opcode & 0x007ff801) return -1;

    GPR tmp = GPR::allocate();
    GPR xer_reg = tc->mapGPRForReadAndWrite(XERn);
    GPR cr_reg = tc->mapGPRForReadAndWrite(CRn);
    uint8_t crn = (opcode >> 23) & 7;

    tc->emit({
        lsr(tmp, xer_reg, 28),
        bfi(cr_reg, tmp, 4 * (7 - crn), 4),
        bic_immed(xer_reg, xer_reg, 4, 4)
    });

    tc->advancePC(4);

    return 1;
}

static __used__ int EMIT_mtcrf(PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 0x00100801) return -1;

    uint8_t rs = (opcode >> 21) & 31;
    uint8_t mask = (opcode >> 12) & 255;

    GPR reg_rs = tc->mapGPRForRead(rs);
    GPR reg_cr = tc->mapGPRForReadAndWrite(CRn);

    if (mask == 0xff) {
        tc->emit( mov_reg(reg_cr, reg_rs));
    } else if (mask == 0) {
        tc->emit( mov_reg(reg_cr, WZR));
    } else {
        GPR tmp = GPR::allocate();
        uint32_t mask32 = 0;
        uint32_t encoded;

        for (int i=0; i < 8; i++) {
            if (mask & (1 << i)) {
                mask32 |= 15 << (4 * i);
            }
        }

        encoded = number_to_mask(mask32);

        if (encoded == 0) {
            GPR imm = GPR::allocate();

            tc->emitLoadImmediate(imm, mask32);
            tc->emit({
                bic_reg(reg_cr, reg_cr, imm, LSL, 0),
                and_reg(tmp, tmp, imm, LSL, 0),
                orr_reg(reg_cr, reg_cr, tmp, LSL, 0)
            });
        } else {
            tc->emit({
                bic_immed(reg_cr, reg_cr, (encoded >> 16) & 0x3f, encoded & 0x3f),
                and_immed(tmp, tmp, (encoded >> 16) & 0x3f, encoded & 0x3f),
                orr_reg(reg_cr, reg_cr, tmp, LSL, 0)
            });
        }
    }

    tc->advancePC(4);

    return 1;
}

static __used__ int EMIT_subfex(PPCTranslatorContext *tc, uint32_t opcode)
{
    uint8_t rd = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;
    uint8_t rc = opcode & 1;
    uint8_t oe = (opcode >> 10) & 1;

    GPR reg_xer = tc->mapGPRForReadAndWrite(XERn);

    /* Shortcut - subfe rX, rX, rX gives the CA flag in rX */
    if (rc == 0 && oe == 0 && rd == ra && rd == rb) {
        GPR reg_rd = tc->mapGPRForWrite(rd);
        tc->emit({
            tst_immed(reg_xer, 1, 3),
            csetm(reg_rd, A64_CC_EQ)
        });

        tc->advancePC(4);

        return 1;
    }

    return -1;
}


extern "C" {

extern int debug;
extern int disasm;

}

static inline int globalDebug() {
    return debug;
}

static inline int globalDisasm() {
    return disasm;
}


static inline int EMIT_Group_63(PPCTranslatorContext *tc, uint32_t opcode)
{
    uint32_t secondary_short = (opcode >> 1) & 0x1f;
    uint32_t secondary_long = (opcode >> 1) & 0x3ff;

    switch (secondary_long) {
        case 0b0000000000: return Emit::fcmpu(tc, opcode);
        case 0b0001001000: return Emit::fmrx(tc, opcode);
    }

    switch (secondary_short) {
        case 0b10100: return Emit::fsubx(tc, opcode);
        case 0b10101: return Emit::faddx(tc, opcode);
        case 0b11001: return Emit::fmulx(tc, opcode);
        case 0b10010: return Emit::fdivx(tc, opcode);
        case 0b01111: return Emit::fctiwzx(tc, opcode);
        case 0b11101: return Emit::fmadd(tc, opcode);
    }

    return -1;
}

static inline int EMIT_Group_19(PPCTranslatorContext *tc, uint32_t opcode)
{
    uint32_t secondary = (opcode >> 1) & 0x3ff;

    switch (secondary) {
        case 0b0000000000: return EMIT_mcrf(tc, opcode);
        case 0b0000010000: return EMIT_bclrx(tc, opcode);
        case 0b0000100001: return EMIT_crnor(tc, opcode);
        case 0b0000110010: return Emit::rfi(tc, opcode);
        case 0b0010000001: return EMIT_crandc(tc, opcode);
        case 0b0010010110: return Emit::isync(tc, opcode);
        case 0b0011000001: return EMIT_crxor(tc, opcode);
        case 0b0011100001: return EMIT_crnand(tc, opcode);
        case 0b0100000001: return EMIT_crand(tc, opcode);
        case 0b0100100001: return EMIT_creqv(tc, opcode);
        case 0b0110100001: return EMIT_crorc(tc, opcode);
        case 0b0111000001: return EMIT_cror(tc, opcode);
        case 0b1000010000: return EMIT_bcctrx(tc, opcode);
        default: return -1;
    }
}

static inline int EMIT_Group_31(PPCTranslatorContext *tc, uint32_t opcode)
{
    uint32_t secondary = (opcode >> 1) & 0x3ff;

    switch (secondary) {
        case 0b0000000000: return Emit::cmp(tc, opcode);
        case 0b0000000100: return Emit::tw(tc, opcode);
        case 0b0000001000: return Emit::subfcx(tc, opcode);
        case 0b0000001010: return Emit::addcx(tc, opcode);
        case 0b0000001011: return Emit::mulhwux(tc, opcode);
        case 0b0000010011: return EMIT_mfcr(tc, opcode);
        case 0b0000010100: return Emit::lwarx(tc, opcode);
        case 0b0000010111: return Emit::lwzx(tc, opcode);
        case 0b0000011000: return Emit::slwx(tc, opcode);
        case 0b0000011010: return Emit::cntlzwx(tc, opcode);
        case 0b0000011100: return Emit::andx(tc, opcode);
        case 0b0000100000: return Emit::cmpl(tc, opcode);
        case 0b0000101000: return Emit::subfx(tc, opcode);
        case 0b0000110110: return Emit::dcbst(tc, opcode);
        case 0b0000110111: return Emit::lwzux(tc, opcode);
        case 0b0000111100: return Emit::andcx(tc, opcode);
        case 0b0001001011: return Emit::mulhwx(tc, opcode);
        case 0b0001010011: return Emit::mfmsr(tc, opcode);     // OEA, supervisor
        case 0b0001010110: return Emit::dcbf(tc, opcode);
        case 0b0001010111: return Emit::lbzx(tc, opcode);
        case 0b0001101000: return Emit::negx(tc, opcode);
        case 0b0001110111: return Emit::lbzux(tc, opcode);
        case 0b0001111100: return Emit::norx(tc, opcode);
        case 0b0010001000: return EMIT_subfex(tc, opcode);
        case 0b0010001010: return Emit::addex(tc, opcode);
        case 0b0010010000: return EMIT_mtcrf(tc, opcode);
        case 0b0010010010: return Emit::mtmsr(tc, opcode);     // OEA, supervisor
        case 0b0010010110: return Emit::stwcx_dot(tc, opcode);
        case 0b0010010111: return Emit::stwx(tc, opcode);
        case 0b0010110111: return Emit::stwux(tc, opcode);
        //case 0b0011001000: return EMIT_subfzex(tc, opcode);
        //case 0b0011001010: return EMIT_addzex(tc, opcode);
        //case 0b0011010010: return EMIT_mtsr(tc, opcode);      // OEA, supervisor
        case 0b0011010111: return Emit::stbx(tc, opcode);
        //case 0b0011101000: return EMIT_subfmex(tc, opcode);
        //case 0b0011101010: return EMIT_addmex(tc, opcode);
        case 0b0011101011: return Emit::mullwx(tc, opcode);
        //case 0b0011110010: return EMIT_mtsrin(tc, opcode);    // OEA, supervisor
        case 0b0011110110: return Emit::dcbtst(tc, opcode);    // VEA
        case 0b0011110111: return Emit::stbux(tc, opcode);
        case 0b0100001010: return Emit::addx(tc, opcode);
        case 0b0100010110: return Emit::dcbt(tc, opcode);      // VEA
        case 0b0100010111: return Emit::lhzx(tc, opcode);
        case 0b0100011100: return Emit::eqvx(tc, opcode);
        //case 0b0100110010: return EMIT_tlbie(tc, opcode);     // OEA, supervisor, optional
        //case 0b0100110110: return EMIT_eciwx(tc, opcode);     // optional
        case 0b0100110111: return Emit::lhzux(tc, opcode);
        case 0b0100111100: return Emit::xorx(tc, opcode);
        case 0b0101010011: return Emit::mfspr(tc, opcode);
        case 0b0101010111: return Emit::lhax(tc, opcode);
        //case 0b0101110010: return EMIT_tlbia(tc, opcode);     // OEA, supervisor, optional
        case 0b0101110011: return Emit::mftb(tc, opcode);
        case 0b0101110111: return Emit::lhaux(tc, opcode);
        case 0b0110010111: return Emit::sthx(tc, opcode);
        case 0b0110011100: return Emit::orcx(tc, opcode);
        //case 0b0110110110: return EMIT_ecowx(tc, opcode);     // optional
        case 0b0110110111: return Emit::sthux(tc, opcode);
        case 0b0110111100: return Emit::orx(tc, opcode);
        case 0b0111001011: return Emit::divwux(tc, opcode);
        case 0b0111010011: return Emit::mtspr(tc, opcode);
        case 0b0111010110: return Emit::dcbi(tc, opcode);      // VEA, supervisor
        case 0b0111011100: return Emit::nandx(tc, opcode);
        case 0b0111101011: return Emit::divwx(tc, opcode);
        case 0b1000000000: return EMIT_mcrxr(tc, opcode);
        //case 0b1000010101: return EMIT_lswx(tc, opcode);
        case 0b1000010110: return Emit::lwbrx(tc, opcode);
        case 0b1000011000: return Emit::srwx(tc, opcode);
        //case 0b1000110110: return EMIT_tlbsync(tc, opcode);   // OEA, supervisor, optional
        //case 0b1001010011: return EMIT_mfsr(tc, opcode);      // OEA, supervisor
        //case 0b1001010101: return EMIT_lswi(tc, opcode);
        case 0b1001010110: return Emit::sync(tc, opcode);
        //case 0b1010010011: return EMIT_mfsrin(tc, opcode);    // OEA
        //case 0b1010010101: return EMIT_stswx(tc, opcode);
        case 0b1010010110: return Emit::stwbrx(tc, opcode);
        case 0b1011110110: return Emit::dcba(tc, opcode);      // VEA, optional
        case 0b1100010110: return Emit::lhbrx(tc, opcode);
        case 0b1100011000: return Emit::srawx(tc, opcode);
        case 0b1100111000: return Emit::srawix(tc, opcode);
        case 0b1101010110: return Emit::eieio(tc, opcode);
        case 0b1110010110: return Emit::sthbrx(tc, opcode);
        case 0b1110011010: return Emit::extshx(tc, opcode);
        case 0b1110111010: return Emit::extsbx(tc, opcode);
        case 0b1111010110: return Emit::icbi(tc, opcode);
        case 0b1111110110: return Emit::dcbz(tc, opcode);      // VEA

        /* FPU part */
        //case 0b1000010111: return EMIT_lfsx(tc, opcode);      // FPU
        //case 0b1000110111: return EMIT_lfsux(tc, opcode);     // FPU
        //case 0b1001010111: return EMIT_lfdx(tc, opcode);      // FPU
        //case 0b1001110111: return EMIT_lfdux(tc, opcode);     // FPU
        case 0b1111010111: return Emit::stfiwx(tc, opcode);    // FPU
        //case 0b1011110111: return EMIT_stfdux(tc, opcode);    // FPU
        //case 0b1010010111: return EMIT_stfsx(tc, opcode);     // FPU
        //case 0b1010110111: return EMIT_stfsux(tc, opcode);    // FPU
        //case 0b1011010101: return EMIT_stswi(tc, opcode);     // FPU
        //case 0b1011010111: return EMIT_stfdx(tc, opcode);     // FPU
        
        default: return -1;
    }
}

static inline int EmitINSN(PPCTranslatorContext *tc)
{
    uint32_t opcode = cache_read_32(ICACHE, (uint32_t)(uintptr_t)tc->getPC());
    uint8_t group = opcode >> 26;
    int count = -1;

    //kprintf("[PPC] EmitINSN @ %08x, opcode %08x, group %d\n", (uint32_t)(uintptr_t)tc->getPC(), opcode, group);

    switch (group) {
        case 0b000011: count = Emit::twi(tc, opcode); break;
        case 0b000111: count = Emit::mulli(tc, opcode); break;
        case 0b001000: count = Emit::subfic(tc, opcode); break;
        case 0b001010: count = Emit::cmpli(tc, opcode); break;
        case 0b001011: count = Emit::cmpi(tc, opcode); break;
        case 0b001100: count = Emit::addic(tc, opcode); break;
        case 0b001101: count = Emit::addic_dot(tc, opcode); break;
        case 0b001110: count = Emit::addi(tc, opcode); break;
        case 0b001111: count = Emit::addis(tc, opcode); break;
        case 0b010000: count = EMIT_bcx(tc, opcode); break;
        case 0b010001: count = Emit::sc(tc, opcode); break;
        case 0b010010: count = EMIT_bx(tc, opcode); break;
        case 0b010011: count = EMIT_Group_19(tc, opcode); break;
        case 0b010100: count = Emit::rlwimix(tc, opcode); break;
        case 0b010101: count = Emit::rlwinmx(tc, opcode); break;
        case 0b010111: count = Emit::rlwnmx(tc, opcode); break;
        case 0b011000: count = Emit::ori(tc, opcode); break;
        case 0b011001: count = Emit::oris(tc, opcode); break;
        case 0b011010: count = Emit::xori(tc, opcode); break;
        case 0b011011: count = Emit::xoris(tc, opcode); break;
        case 0b011100: count = Emit::andi_dot(tc, opcode); break;
        case 0b011101: count = Emit::andis_dot(tc, opcode); break;
        case 0b011111: count = EMIT_Group_31(tc, opcode); break;
        case 0b100000: count = Emit::lwz(tc, opcode); break;
        case 0b100001: count = Emit::lwzu(tc, opcode); break;
        case 0b100010: count = Emit::lbz(tc, opcode); break;
        case 0b100011: count = Emit::lbzu(tc, opcode); break;
        case 0b100100: count = Emit::stw(tc, opcode); break;
        case 0b100101: count = Emit::stwu(tc, opcode); break;
        case 0b100110: count = Emit::stb(tc, opcode); break;
        case 0b100111: count = Emit::stbu(tc, opcode); break;
        case 0b101000: count = Emit::lhz(tc, opcode); break;
        case 0b101001: count = Emit::lhzu(tc, opcode); break;
        case 0b101010: count = Emit::lha(tc, opcode); break;
        case 0b101011: count = Emit::lhau(tc, opcode); break;
        case 0b101100: count = Emit::sth(tc, opcode); break;
        case 0b101101: count = Emit::sthu(tc, opcode); break;
        //case 0b101110: count = EMIT_lmw(tc, opcode); break;   // Need to be interpreted
        //case 0b101111: count = EMIT_stmw(tc, opcode); break;  // Need to be interpreted
        case 0b110000: count = Emit::lfs(tc, opcode); break;
        //case 0b110001: count = EMIT_lfsu(tc, opcode); break;
        case 0b110010: count = Emit::lfd(tc, opcode); break;
        //case 0b110011: count = EMIT_lfdu(tc, opcode); break;
        case 0b110100: count = Emit::stfs(tc, opcode); break;
        //case 0b110101: count = EMIT_stfsu(tc, opcode); break;
        case 0b110110: count = Emit::stfd(tc, opcode); break;
        //case 0b110111: count = EMIT_stfdu(tc, opcode); break;
        //case 0b111011: count = EMIT_Group_59(tc, opcode); break;
        case 0b111111: count = EMIT_Group_63(tc, opcode); break;
        default: break;
    }

    if (count < 1) {
        kprintf("[PPC] UNIMPLEMENTED %08x @ %08x...\n", opcode, (uint32_t)(uintptr_t)tc->getPC());

        if (!disasm) {
            disasm_open();
        }

        disasm_print_ppc_only(tc->getPC());

        while(1);
    }

    return count;
}



static struct DisasmOut {
    uint32_t *do_PPCAddr;
    uint32_t *do_ArmAddr;
    uint32_t do_PPCCount;
    uint32_t do_ArmCount;
} disasm_items[512], *disasm_ptr;

static inline uintptr_t PPC_Translate(uint32_t *PPCCodePtr, uint32_t *InsnCount)
{
    Emu68::List<ExitBlock> exitList;
    struct PPCState *ctx = GET_HOST_CTX();
    ppc_entry_point = PPCCodePtr;
    uint32_t *orig_ppccodeptr = PPCCodePtr;
    uintptr_t hash = (uintptr_t)PPCCodePtr;

    int var_EMU68_MAX_LOOP_COUNT = (ctx->JIT_CONTROL >> JCCB_LOOP_COUNT) & JCCB_LOOP_COUNT_MASK;
    if (var_EMU68_MAX_LOOP_COUNT == 0)
        var_EMU68_MAX_LOOP_COUNT = JCCB_LOOP_COUNT_MASK + 1;
    uint32_t var_EMU68_PPC_INSN_DEPTH = (ctx->JIT_CONTROL >> JCCB_INSN_DEPTH) & JCCB_INSN_DEPTH_MASK;
    if (var_EMU68_PPC_INSN_DEPTH == 0)
        var_EMU68_PPC_INSN_DEPTH = JCCB_INSN_DEPTH_MASK + 1;

    GPR::setContext(&local_translator);
    FPR::setContext(&local_translator);

    local_translator.tc_CodePtr = local_translator.tc_CodeStart;
    local_translator.setPC(PPCCodePtr);
    local_translator.setPCStart(PPCCodePtr);
    local_translator.tc_SupervisorChecked = false;
    local_translator.tc_InsnCount = 0;

    uint32_t *last_rev_jump = (uint32_t *)0xffffffff;

    disasm_ptr = disasm_items;

    int debug = 0;
    int disasm = 0;

    if ((uint32_t)(uintptr_t)PPCCodePtr >= debug_range_min && (uint32_t)(uintptr_t)PPCCodePtr <= debug_range_max) {
        debug = globalDebug();
        disasm = globalDisasm();
    }

#if 0
    if (!gpr_lru.isEmpty()) {
        kprintf("[PPC] GPR_LRU list is not empty!\n");
        while(1);
    }

    if (!fpr_lru.isEmpty()) {
        kprintf("[PPC] FPR_LRU list is not empty!\n");
        while(1);
    }
#endif

    if (local_translator.getTempAllocMask()) {
        kprintf("[PPC] Temporary register alloc mask on translate start is non-zero %x\n", local_translator.getTempAllocMask());
        while(1);
    }

    if (disasm) {
        disasm_open();
    }

    return_stack.reset();

    if (debug) {
        uint32_t hash_calc = (hash >> EMU68_HASHSHIFT) & EMU68_HASHMASK;
        kprintf("[PPC] Creating new translation unit with hash %04x (PPC code @ %p)\n", hash_calc, (void*)PPCCodePtr);
    }

    int break_loop = FALSE;
    int inner_loop = FALSE;
    int soft_break = FALSE;
    int max_rev_jumps = 0;

    ppc_low = local_translator.getPC();
    ppc_high = local_translator.getPC() + 4;

    int inner_loop_length = 0;
    int inner_loop_limit = 0;

    while (break_loop == FALSE && soft_break == FALSE && local_translator.tc_InsnCount < var_EMU68_PPC_INSN_DEPTH)
    {
        uint16_t insn_consumed;
        uint32_t * const in_code = local_translator.getPC();
        uint32_t * const out_code = local_translator.tc_CodePtr;

        if (local_translator.tc_InsnCount && ((uintptr_t)local_translator.getPC() < (uintptr_t)local_state[local_translator.tc_InsnCount-1].pls_PPCPtr))
        {
            int found = -1;

            for (int i=local_translator.tc_InsnCount - 1; i >= 0; --i)
            {
                if (local_state[i].pls_PPCPtr == local_translator.getPC())
                {
                    found = i;
                    break;
                }
            }

            if (found > 0)
            {
                if ((local_translator.tc_InsnCount - found - 1) > (var_EMU68_PPC_INSN_DEPTH - local_translator.tc_InsnCount))
                {
                    break;
                }
            }
        }

        local_translator.putToLocalState(&local_state[local_translator.tc_InsnCount]);

        insn_consumed = EmitINSN(&local_translator);

        if (local_translator.getPC() < ppc_low)
            ppc_low = local_translator.getPC();
        if (local_translator.getPC() + 4 > ppc_high)
            ppc_high = local_translator.getPC() + 4;

        local_translator.tc_InsnCount += insn_consumed;
        
        int process_markers = 1;

        while(process_markers)
        {
            if (local_translator.tc_CodePtr[-1] == INSN_TO_LE(MARKER_STOP))
            {
                local_translator.tc_CodePtr--;
                break_loop = TRUE;
            }
            else if (local_translator.tc_CodePtr[-1] == INSN_TO_LE(MARKER_BREAK))
            {
                local_translator.tc_CodePtr--;
                soft_break = TRUE;
            }
            else if (local_translator.tc_CodePtr[-1] == INSN_TO_LE(MARKER_EXIT_BLOCK))
            {
                struct ExitBlock *eb;
                
                local_translator.tc_CodePtr -= 3;

                uint32_t insn_count = local_translator.tc_CodePtr[1];
                uint32_t fixup_count = local_translator.tc_CodePtr[0];

                local_translator.tc_CodePtr -= 2 * fixup_count;

                eb = (ExitBlock *)tlsf_malloc(tlsf, sizeof(struct ExitBlock) + 4 * insn_count + sizeof(eb->eb_Fixup[0]) * fixup_count);

                eb->eb_FixupCount = fixup_count;
                eb->eb_InstructionCount = insn_count;
                eb->eb_ARMCode = (uint32_t *)((uintptr_t)eb + sizeof(ExitBlock));
                eb->eb_Fixup = (typeof(eb->eb_Fixup[0]) *)((uintptr_t)eb + sizeof(ExitBlock) + 4 * insn_count);

                for (uint32_t i=0; i < fixup_count; i++) {
                    uint32_t fixup_type = local_translator.tc_CodePtr[2 * i + 1];
                    uint32_t fixup_target = local_translator.tc_CodePtr[2 * i];

                    eb->eb_Fixup[i].type = fixup_type;
                    eb->eb_Fixup[i].location = local_translator.tc_CodePtr - fixup_target;
                }

                local_translator.tc_CodePtr -= insn_count;

                for (unsigned i=0; i < insn_count; i++) {
                    eb->eb_ARMCode[i] = local_translator.tc_CodePtr[i];
                }

                exitList.addTail(eb);
            }
            else if (local_translator.tc_CodePtr[-1] == INSN_TO_LE(0xfffffffe))
            {
                kprintf("[PPC] Special marker 0xfffffffe unsupported\n");
                while(1) asm volatile("wfi");

                #if 0
                uint32_t *tmpptr;
                uint32_t *branch_mod[10];
                uint32_t branch_cnt;
                int local_branch_done = 0;
                local_translator.tc_CodePtr--;
                local_translator.tc_CodePtr--;  /* Remove branch target (unused!) */
                branch_cnt = *--local_translator.tc_CodePtr;

                for (unsigned i=0; i < branch_cnt; i++)
                {
                    uintptr_t ptr = *(uint32_t *)--local_translator.tc_CodePtr;
                    ptr |= (uintptr_t)local_translator.tc_CodePtr & 0xffffffff00000000;
                    branch_mod[i] = (uint32_t *)ptr;
                }

                tmpptr = local_translator.tc_CodePtr;

                if (!local_branch_done)
                {
                    local_translator.emitLocalExit(0);
                }
                int distance = local_translator.tc_CodePtr - tmpptr;

                for (unsigned i=0; i < branch_cnt; i++) {
                    //kprintf("[PPC] Branch modification at %p : distance increase by %d\n", (void*) branch_mod[i], distance);
                    *(branch_mod[i]) = INSN_TO_LE((INSN_TO_LE(*(branch_mod[i])) + (distance << 5)));
                }
                #endif
            }
            else
            {
                process_markers = 0;
            }
        }

        if (disasm) {
            disasm_ptr->do_PPCAddr = in_code;
            disasm_ptr->do_ArmAddr = out_code;
            disasm_ptr->do_PPCCount = insn_consumed;
            disasm_ptr->do_ArmCount = local_translator.tc_CodePtr - out_code;
            disasm_ptr++;
        }

        if (in_code >= local_translator.getPC())
        {
            if (last_rev_jump == local_translator.getPC()) {
                if (--max_rev_jumps == 0) {
                    break;
                }
            }
            else {
                last_rev_jump = local_translator.getPC();
                max_rev_jumps = var_EMU68_MAX_LOOP_COUNT;
            }
        }

        if (!break_loop && (orig_ppccodeptr == local_translator.getPC()))
        {
            inner_loop = TRUE;

            if (inner_loop_length == 0) {
                inner_loop_length = local_translator.tc_InsnCount;
                int capacity = var_EMU68_PPC_INSN_DEPTH / local_translator.tc_InsnCount;
                if (capacity > var_EMU68_MAX_LOOP_COUNT) capacity = var_EMU68_MAX_LOOP_COUNT;

                if (capacity <= 1) break;

                inner_loop_limit = capacity - 1;
            } else {
                if (--inner_loop_limit == 0) break;
            }

            if (soft_break) break;
        }
    }

    if (inner_loop && local_translator.tc_InsnCount == 1 && local_translator.getPC()[0] == 0x48000000) {
        kprintf("[PPC] Replacing ARM opcode %08x for the endless PPC loop\n", local_translator.tc_CodePtr[-1]);
        
        /* This is an endless loop, intentional or not. Change it to WFI/WFE loop to conserve power */
        local_translator.tc_CodePtr--;
        local_translator.emit(wfe());
    }

    uint32_t *out_code = local_translator.tc_CodePtr;

#if EMU68_INSN_COUNTER
    uint8_t icnt_reg = local_translator.allocARMRegister();
    local_translator.emit(mov_simd_to_reg(icnt_reg, CTX_INSN_COUNT));
#endif
    local_translator.flushAllFPRs();
    local_translator.flushAllGPRs();
    local_translator.flushPC();

#if EMU68_INSN_COUNTER
    local_translator.emit(add64_immed(icnt_reg, icnt_reg, local_translator.tc_InsnCount & 0xfff));
#endif

    uint8_t tmp2 = local_translator.allocARMRegister();
    if (inner_loop)
    {
        uint8_t cpuctx = local_translator.getCTX();
        local_translator.emit(ldr64_offset(cpuctx, tmp2, __builtin_offsetof(PPCState, INT64)));
    }

#if EMU68_INSN_COUNTER
    local_translator.emit(mov_reg_to_simd(CTX_INSN_COUNT, icnt_reg));
    local_translator.freeARMRegister(icnt_reg);
#endif

    if (inner_loop)
    {
        uint32_t *tmpptr = local_translator.tc_CodePtr;
        local_translator.emit(cbz_64(tmp2, local_translator.tc_CodeStart - tmpptr));
    }
    local_translator.emit(bx_lr());
    
    uint32_t *main_block_end = local_translator.tc_CodePtr;

    uint32_t *_tmpptr = local_translator.tc_CodePtr;
    local_translator.freeARMRegister(tmp2);
    local_translator.flushCTX();
    local_translator.tc_CodePtr = _tmpptr;

    if (disasm) {
        disasm_ptr->do_PPCAddr = nullptr;
        disasm_ptr->do_PPCCount = 0;
        disasm_ptr->do_ArmAddr = out_code;
        disasm_ptr->do_ArmCount = local_translator.tc_CodePtr - out_code;
        disasm_ptr++;
    }

    /* Get all exit entries and append them here */
    struct ExitBlock *eb = nullptr;
    //int exit_num = 0;
    while ((eb = exitList.remHead()))
    {
        uint32_t *old_end = local_translator.tc_CodePtr;
        uint32_t op;

        for (unsigned i = 0; i < eb->eb_InstructionCount; i++)
        {
            local_translator.emit(eb->eb_ARMCode[i]);
        }

        for (uint32_t i=0; i < eb->eb_FixupCount; i++)
        {
            switch (eb->eb_Fixup[i].type)
            {
                case FIXUP_BCC:
                    op = I32(*eb->eb_Fixup[i].location);
                    op &= ~(0x7ffff << 5);
                    op |= ((old_end - eb->eb_Fixup[i].location) & 0x7ffff) << 5;
                    *eb->eb_Fixup[i].location = I32(op);
                    break;

                case FIXUP_TBZ:
                    op = I32(*eb->eb_Fixup[i].location);
                    op &= ~(0x3fff << 5);
                    op |= ((old_end - eb->eb_Fixup[i].location) & 0x3fff) << 5;
                    *eb->eb_Fixup[i].location = I32(op);
                    break;

                default:
                    kprintf("[PPC] I don't know how to deal with fixup type 0x%08x\n", eb->eb_Fixup[i].type);
            }
        }

        if (disasm) {
            disasm_ptr->do_PPCAddr = nullptr;
            disasm_ptr->do_PPCCount = 0;
            disasm_ptr->do_ArmAddr = old_end;
            disasm_ptr->do_ArmCount = local_translator.tc_CodePtr - old_end;
            disasm_ptr++;
        }

        tlsf_free(tlsf, eb);
    }

    disasm_ptr->do_ArmAddr = nullptr;

    if (disasm) {
        int exit_num = 0;
        for (disasm_ptr = disasm_items; disasm_ptr->do_ArmAddr; disasm_ptr++)
        {
            if (disasm_ptr->do_PPCAddr == NULL) {
                if (exit_num == 0) {
                    kprintf("[PPC] EXIT_DEF:\n");
                } else {
                    kprintf("[PPC] EXIT_%03d:\n", exit_num);
                }
                exit_num++;
            }
            disasm_print_ppc(
                disasm_ptr->do_PPCAddr, disasm_ptr->do_PPCCount,
                disasm_ptr->do_ArmAddr, 4 * disasm_ptr->do_ArmCount, local_translator.tc_CodeStart);
        }
        disasm_close();
    }

    if (debug)
    {
        kprintf("[PPC] Translated %d PPC instructions to %d ARM instructions\n", local_translator.tc_InsnCount, (int)(local_translator.tc_CodePtr - local_translator.tc_CodeStart));
        //kprintf("[PPC] Prologue size: %d, Epilogue size: %d, Conditionals: %d\n",
        //    prologue_size, epilogue_size, conditionals_count);
        //kprintf("[PPC]   Mean epilogue size pro exit point: %d\n", epilogue_size / (1 + conditionals_count));
        uint32_t mean = 100 * (main_block_end - local_translator.tc_CodeStart); // - (prologue_size + epilogue_size));
        mean = mean / local_translator.tc_InsnCount;
        uint32_t mean_n = mean / 100;
        uint32_t mean_f = mean % 100;
        kprintf("[PPC] Mean ARM instructions per PPC instruction: %d.%02d\n", mean_n, mean_f);
    }

    // Put a marker at the end of translation unit
    local_translator.emitStop();

    if (InsnCount != nullptr)
        *InsnCount = local_translator.tc_InsnCount;

    return (uintptr_t)local_translator.tc_CodePtr - (uintptr_t)local_translator.tc_CodeStart;
}

/*
    Get PPC code unit from the instruction cache. Return NULL if code was not found and needs to be
    translated first.

    If the code was found, update its position in the LRU cache.
*/
PPCTranslationUnit *PPC_GetTranslationUnit(uint32_t *ppccodeptr)
{
    struct PPCState *ctx = GET_HOST_CTX();
    PPCTranslationUnit *unit = nullptr;
    uintptr_t hash = (uintptr_t)ppccodeptr;
    uint32_t *orig_ppccodeptr = ppccodeptr;
    uint64_t time_start, time_end;

    int debug = 0;

    if ((uint32_t)(uintptr_t)ppccodeptr >= debug_range_min && (uint32_t)(uintptr_t)ppccodeptr <= debug_range_max) {
        debug = globalDebug();
    }

    /* Get 16-bit has from the pointer to PPC code */
    hash = (hash >> EMU68_HASHSHIFT) & EMU68_HASHMASK;

    if (debug > 2)
        kprintf("[PPC] GetTranslationUnit(%08x)\n[PPC] Hash: 0x%04x\n", (void*)ppccodeptr, (int)hash);

    asm volatile("mrs %0, CNTPCT_EL0":"=r"(time_start));
    uint32_t insn_count = 0;
    uintptr_t line_length = PPC_Translate(ppccodeptr, &insn_count);
    uintptr_t arm_insn_count = line_length/4 - 1;
    uintptr_t unit_length = (line_length + 63 + sizeof(PPCTranslationUnit)) & ~63;
    asm volatile("mrs %0, CNTPCT_EL0":"=r"(time_end));

    do {
        unit = (PPCTranslationUnit *)jit_ppc.malloc_aligned(unit_length, 64);
        
        ctx->JIT_CACHE_FREE = jit_ppc.free_size();

        if (unit == NULL)
        {
            if (debug > 0) {
                kprintf("[PPC] Requested block was %d bytes long\n", sizeof(PPCTranslationUnit));
            }

            for (int i=0; i < 8; i++) {
                auto n = LRU.remTail()->unit;

                if (n == nullptr)
                    break;

                n->remove();
                if (debug > 0)
                {    
                    kprintf("[PPC] Run out of cache. Removing least recently used cache line node @ %p\n", n);
                }

                jit_ppc.free(n);
                ctx->JIT_UNIT_COUNT--;
            }
            ctx->JIT_CACHE_FREE = jit_ppc.free_size();
            
            __asm__ volatile("mov " CTX_LAST_PC_ASM ", %w0"::"r"(0xffffffff));
        }
    } while(unit == NULL);

    /* Store JIT translation time */
    unit->ptu_CompileTime = time_end - time_start;

    /* Set-up entry point */
    unit->ptu_ARMEntryPoint = &unit->ptu_ARMCode[0];
    unit->ptu_ARMEntryPoint = (void *)((uintptr_t)unit->ptu_ARMEntryPoint | 0x0000001000000000ULL);

    /* Copy the code to the new location */
    DuffCopy(&unit->ptu_ARMCode[0], local_translator.tc_CodeStart, line_length/4);

    /* The code is ready, so flush the caches*/
    arm_flush_cache((uintptr_t)&unit->ptu_ARMCode, line_length);
    arm_icache_invalidate((intptr_t)unit->ptu_ARMEntryPoint, line_length);

    /* Tell CPU we are going to execute the code soon, give it time to prefetch while CRC is still calculated */
    asm volatile ("prfm plil1keep, [%0]"::"r"(unit->ptu_ARMEntryPoint));
    /* If more than 16 ARM instructions were generated, prefetch another line of cache */
    if (arm_insn_count > 16)
        asm volatile ("prfm plil1keep, [%0, #64]"::"r"(unit->ptu_ARMEntryPoint));

    unit->ptu_PPCInsnCnt = insn_count;
    unit->ptu_ARMInsnCnt = arm_insn_count;
    
    unit->ptu_PPCAddress = (uint32_t)(uintptr_t)orig_ppccodeptr;
    unit->ptu_PPCLow = (uint32_t)(uintptr_t)ppc_low;
    unit->ptu_PPCHigh = (uint32_t)(uintptr_t)ppc_high;
    unit->ptu_Fingerprint = cache_read_32(ICACHE, unit->ptu_PPCAddress) ^ cache_read_32(ICACHE, unit->ptu_PPCAddress + 4);
    
    asm volatile("mrs %0, CNTPCT_EL0":"=r"(time_start));
    unit->ptu_CRC32 = CalcCRC32(ppc_low, ppc_high);
    asm volatile("mrs %0, CNTPCT_EL0":"=r"(time_end));
    unit->ptu_VerifyTime = time_end - time_start;
    #if 0
    unit->ptu_UseCount = 0;
    unit->ptu_FetchCount = 0;
    unit->mt_PrologueSize = prologue_size;
    unit->mt_EpilogueSize = epilogue_size;
    unit->mt_Conditionals = conditionals_count;
    #endif
    unit->ptu_Epoch = GET_EPOCH();

    unit->ptu_LRU.unit = unit;
    LRU.addHead(&unit->ptu_LRU);
    ICache[hash].addHead(unit);

    ctx->JIT_UNIT_COUNT++;
    ctx->JIT_CACHE_MISS++;

    if (debug) {
        kprintf("[PPC] Block checksum: %08x, Fingerprint: %08x\n", unit->ptu_CRC32, unit->ptu_Fingerprint);
        kprintf("[PPC] Compile time: %d, Verify time: %d\n", unit->ptu_CompileTime, unit->ptu_VerifyTime);
        kprintf("[PPC] ARM code at %p\n", unit->ptu_ARMEntryPoint);
    }

    if (debug)
    {
        kprintf("-- ARM Code dump --\n");
        for (uint32_t i=0; i < unit->ptu_ARMInsnCnt; i++)
        {
            if ((i % 5) == 0)
                kprintf("   ");
            uint32_t insn = LE32(unit->ptu_ARMCode[i]);
            kprintf(" %02x %02x %02x %02x", insn & 0xff, (insn >> 8) & 0xff, (insn >> 16) & 0xff, (insn >> 24) & 0xff);
            if ((i % 5) == 4)
                kprintf("\n");
        }
        if (unit->ptu_ARMInsnCnt % 5 != 0)
            kprintf("\n");
        if (debug > 3)
        {
            kprintf("\n-- Local State --\n");
            #if 0
            for (unsigned i=0; i < insn_count; i++)
            {
                kprintf("    %p -> %08x", local_state[i].mls_PPCPtr, local_state[i].mls_ARMOffset);
                for (int r=0; r < 16; r++) {
                    if (local_state[i].mls_RegMap[r] != 0xff) {
                        kprintf(" %c%d=r%d%s", r < 8 ? 'D' : 'A', r % 8, local_state[i].mls_RegMap[r] & 15,
                        local_state[i].mls_RegMap[r] & 0x80 ? "!":"");
                    }
                }
                kprintf(" PC_Rel=%d\n", local_state[i].mls_PCRel);
            }
            #endif
        }
    }

    return unit;
}


/*
    Verify if the translated code has changed since the unit was created. In order
    to do this MD5 sum of the block is compared with the previousy calculated one.

    If th sums are not same, the block is removed form LRU cache and hashtable and memory
    is released.

    The function returns poitner to verified unit or NULL if the unit changed
*/
PPCTranslationUnit *ppcVerifyUnit(PPCTranslationUnit *unit)
{
    if (unit)
    {
        uint32_t crc = 0;

        /* Quick path - ROM is always valid as long as we don't use any fancy remapping, at least on Amiga */
        if (unit->ptu_PPCAddress >= 0xfff00000 && unit->ptu_PPCAddress < 0xffff0000) {
            /* Update EPOCH of the unit */
            unit->ptu_Epoch = GET_EPOCH();

            /* Move the unit to the beginning of LRU list */
            unit->ptu_LRU.remove();
            LRU.addHead(&unit->ptu_LRU);
            
            return unit;
        }

        /* 
            First check fingerprint - if this one changed then there is no need to calculate CRC32
            of the whole block
        */
        uint32_t fp = cache_read_32(ICACHE, unit->ptu_PPCAddress) ^ cache_read_32(ICACHE, unit->ptu_PPCAddress + 4);

        /* If FP matches, calculate CRC32 */
        if (fp == unit->ptu_Fingerprint)
        {
            crc = CalcCRC32((void *)(uintptr_t)unit->ptu_PPCLow, (void*)(uintptr_t)unit->ptu_PPCHigh);
        }

        /* In case of FP or CRC mismatch, remove the unit and reclaim memory */
        if (fp != unit->ptu_Fingerprint || crc != unit->ptu_CRC32)
        {
            auto ctx = GET_HOST_CTX();
            unit->remove();
            unit->ptu_LRU.remove();
            jit_ppc.free(unit);

            ctx->JIT_UNIT_COUNT--;
            ctx->JIT_CACHE_FREE = jit_ppc.free_size();

            unit = nullptr;
        }
        else
        {
            /* Update EPOCH of the unit */
            unit->ptu_Epoch = GET_EPOCH();

            /* Move the unit to the beginning of LRU list */
            unit->ptu_LRU.remove();
            LRU.addHead(&unit->ptu_LRU);
        }
    }

    return unit;
}

} // Emu68::PPC
