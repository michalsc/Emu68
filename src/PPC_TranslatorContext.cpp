/*
    Copyright © 2019-2025 Michal Schulz <michal.schulz@gmx.de>
    https://github.com/michalsc

    This Source Code Form is subject to the terms of the
    Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed
    with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#define restrict __restrict__

#include <emu68/ppc/PPCTranslatorContext>

#include "config.h"
#include "PPC.h"
#include "A64.h"
#include "support.h"

namespace Emu68::PPC {

void PPCTranslatorContext::emitException(uint16_t type)
{
    /* When entering exception, all dirty registers must be stored! */
    flushAllFPRs();
    flushAllGPRs();

    /* Flush program counter */
    flushPC();

    /* Get PPCState to shuffle regs */
    uint8_t ctx = getCTX();
    uint8_t tmp = allocARMRegister();

#if EMU68_INSN_COUNTER
    uint8_t icnt_reg = allocARMRegister();
    if (tc_InsnCount != 0) {
        emit(mov_simd_to_reg(icnt_reg, CTX_INSN_COUNT));
    }
#endif

    emit({
        /* Store MSR into SRR1, Store PC into SRR0 */
        ldr_offset(ctx, tmp, __builtin_offsetof(PPCState, MSR)),
        str_offset(ctx, tmp, __builtin_offsetof(PPCState, SRR1)),
        str_offset(ctx, REG_PC, __builtin_offsetof(PPCState, SRR0)),
        
        /* Set supervisor mode in MSR */
        bic_immed(tmp, tmp, 1, 18),
        str_offset(ctx, tmp, __builtin_offsetof(PPCState, MSR)),

        /* Load new program counter */
        mov_immed_u16(REG_PC, type, 0),
        movk_immed_u16(REG_PC, 0xfff0, 1),
    });

    freeARMRegister(tmp);

#if EMU68_INSN_COUNTER
    if (tc_InsnCount != 0) {
        emit({
            add64_immed(icnt_reg, icnt_reg, tc_InsnCount & 0xfff),
            mov_reg_to_simd(CTX_INSN_COUNT, icnt_reg)
        });
    }
    freeARMRegister(icnt_reg);
#endif

    emit(bx_lr());
}

void PPCTranslatorContext::emitLocalExit(uint32_t insn_fixup)
{
    flushAllGPRs();

#if EMU68_INSN_COUNTER
    uint8_t icnt_reg = allocARMRegister();
    emit(mov_simd_to_reg(icnt_reg, CTX_INSN_COUNT));
#endif

    flushAllFPRs();

    flushPC();

#if EMU68_INSN_COUNTER
    emit({
        add64_immed(icnt_reg, icnt_reg, (tc_InsnCount + insn_fixup) & 0xfff),
        mov_reg_to_simd(CTX_INSN_COUNT, icnt_reg)
    });
    freeARMRegister(icnt_reg);
#else
    (void)insn_fixup;
#endif

    emit(bx_lr());
}

void PPCTranslatorContext::storeDirtyFPRs()
{
    uint8_t ctx = tryCTX();
    bool must_flush_ctx = ctx == 0xff;

    for(auto rn : fpr_lru)
    {
        /* If dirty, store it back to PPC context */
        if (rn->rn_Dirty) {
            if (ctx == 0xff) {
                ctx = allocARMRegister();
                emit(mov_simd_to_reg(ctx, CTX_POINTER));
            }

            uint16_t base_offset = __builtin_offsetof(PPCState, FPR) >> 3;
            
            /* Store value from ARM register back into PPC context */
            if (rn->rn_RegNum >= 14 && rn->rn_RegNum <= 31)
            {
                emit(fstd_pimm(rn->rn_ARM, ctx, base_offset + rn->rn_RegNum));
            }
            else
            {
                kprintf("[PPC] Illegal reg %d in FlushAllFPRs()\n", rn->rn_ARM);
            }

            freeARMRegister(ctx);
        }
    }

    if (must_flush_ctx)
        freeARMRegister(ctx);
}

GPR PPCTranslatorContext::getCTX()
{
    if (reg_ctx == 0xff)
    {
        reg_ctx = allocARMRegister();
        emit(mov_simd_to_reg(reg_ctx, CTX_POINTER));
    }

    return GPR(reg_ctx);
}

void PPCTranslatorContext::flushCTX()
{
    if (reg_ctx != 0xff)
    {
        freeARMRegister(reg_ctx);
    }

    reg_ctx = 0xff;
}


uint8_t PPCTranslatorContext::allocARMRegister()
{
    static int last_allocated = 0;

    for (int i=1; i <= 12; i++)
    {
        int reg = (last_allocated + i) % 12;

        if (((gpr_tmp_pool) & (1 << reg)) == 0)
        {
            gpr_tmp_pool |= 1 << reg;
            last_allocated = reg;
            return reg;
        }
    }

    /* No free ARM register. Remove last entry from GPR_LRU */
    struct RegisterNode *rn = gpr_lru.remTail();

    /* If dirty, store it back to PPC context */
    if (rn->rn_Dirty) {
        /* Store value from ARM register back into PPC context */
        switch(rn->rn_RegNum) {
            case GPRn(14): emit(mov_reg_to_simd(GPR14, rn->rn_ARM)); break;
            case GPRn(15): emit(mov_reg_to_simd(GPR15, rn->rn_ARM)); break;
            case GPRn(16): emit(mov_reg_to_simd(GPR16, rn->rn_ARM)); break;
            case GPRn(17): emit(mov_reg_to_simd(GPR17, rn->rn_ARM)); break;
            case GPRn(18): emit(mov_reg_to_simd(GPR18, rn->rn_ARM)); break;
            case GPRn(19): emit(mov_reg_to_simd(GPR19, rn->rn_ARM)); break;
            case GPRn(20): emit(mov_reg_to_simd(GPR20, rn->rn_ARM)); break;
            case GPRn(21): emit(mov_reg_to_simd(GPR21, rn->rn_ARM)); break;
            case GPRn(22): emit(mov_reg_to_simd(GPR22, rn->rn_ARM)); break;
            case GPRn(23): emit(mov_reg_to_simd(GPR23, rn->rn_ARM)); break;
            case GPRn(24): emit(mov_reg_to_simd(GPR24, rn->rn_ARM)); break;
            case GPRn(25): emit(mov_reg_to_simd(GPR25, rn->rn_ARM)); break;
            case GPRn(26): emit(mov_reg_to_simd(GPR26, rn->rn_ARM)); break;
            case GPRn(27): emit(mov_reg_to_simd(GPR27, rn->rn_ARM)); break;
            case GPRn(28): emit(mov_reg_to_simd(GPR28, rn->rn_ARM)); break;
            case GPRn(29): emit(mov_reg_to_simd(GPR29, rn->rn_ARM)); break;
            case GPRn(30): emit(mov_reg_to_simd(GPR30, rn->rn_ARM)); break;
            case GPRn(31): emit(mov_reg_to_simd(GPR31, rn->rn_ARM)); break;
            case CRn: emit(mov_reg_to_simd(REG_CR, rn->rn_ARM)); break;
            case XERn: emit(mov_reg_to_simd(REG_XER, rn->rn_ARM)); break;
            case FPSCRn: emit(mov_reg_to_simd(REG_FPSCR, rn->rn_ARM)); break;
            default: kprintf("[PPC] Illegal reg %d in IntMapGpr()\n", rn->rn_ARM);
        }
    }

    free_pool.addTail(rn);

    return rn->rn_ARM;
}


void PPCTranslatorContext::freeARMRegister(uint8_t arm_reg)
{
    if (arm_reg > 11)
        return;

    gpr_tmp_pool &= ~(1 << arm_reg);
}

uint8_t PPCTranslatorContext::allocFPRegister()
{
    static int last_allocated = 0;

    for (int i=1; i <= 8; i++)
    {
        int reg = (last_allocated + i) % 8;

        if ((fpr_tmp_pool & (1 << reg)) == 0)
        {
            fpr_tmp_pool |= 1 << reg;
            last_allocated = reg;
            return reg;
        }
    }

    /* No free FP register. Remove last entry from FPR_LRU */
    struct RegisterNode *rn = fpr_lru.remTail();

    /* If dirty, store it back to PPC context */
    if (rn->rn_Dirty) {
        uint8_t ctx = getCTX();
        uint16_t base_offset = __builtin_offsetof(PPCState, FPR) >> 3;
        
        /* Store value from ARM register back into PPC context */
        if (rn->rn_RegNum >= 14 && rn->rn_RegNum <= 31)
        {
            emit(fstd_pimm(rn->rn_ARM, ctx, base_offset + rn->rn_RegNum));
        }
        else
        {
            kprintf("[PPC] Illegal reg %d in IntMapFpr()\n", rn->rn_ARM);
        }
    }

    free_pool.addTail(rn);

    return rn->rn_ARM;
}

void PPCTranslatorContext::freeFPRegister(uint8_t fp_reg)
{
    if (fp_reg > 7)
        return;

    fpr_tmp_pool &= ~(1 << fp_reg);
}


void PPCTranslatorContext::emitAddImmediate(uint8_t rd, int32_t delta)
{
    if (delta < 0) {
        delta = -delta;
        if ((delta & 0xfffff000) == 0) {
            emit(sub_immed(rd, rd, delta));
        } else if ((delta & 0xff000fff) == 0) {
            emit(sub_immed_lsl12(rd, rd, delta >> 12));
        } else {
            uint8_t tmp = allocARMRegister();
            emitLoadImmediate(tmp, (uint32_t)delta);
            emit(sub_reg(rd, rd, tmp, LSL, 0));
            freeARMRegister(tmp);
        }
    } else if (delta > 0) {
        if ((delta & 0xfffff000) == 0) {
            emit(add_immed(rd, rd, delta));
        } else if ((delta & 0xff000fff) == 0) {
            emit(add_immed_lsl12(rd, rd, delta >> 12));
        } else {
            uint8_t tmp = allocARMRegister();
            emitLoadImmediate(tmp, (uint32_t)delta);
            emit(add_reg(rd, rd, tmp, LSL, 0));
            freeARMRegister(tmp);
        }
    }
}

uint8_t PPCTranslatorContext::intMapFPR(uint8_t reg, int load, int set_dirty)
{
    struct RegisterNode *rn;

    /* If register is a fixed-assigned one, return ASAP */
    if (FP_REG_MAPPING[reg] != 0xff) {
        return FP_REG_MAPPING[reg];
    }

    /* Check if register is already in LRU */
    for(auto rn : fpr_lru)
    {
        if (rn->rn_RegNum == reg)
        {
            /* Found it, move it to the top of LRU, return ARM reg number */
            rn->remove();
            fpr_lru.addHead(rn);

            /* Update dirty flag but does not allow to reset it */
            rn->rn_Dirty |= set_dirty;
            return rn->rn_ARM;
        }
    }

    /* Not found. Get a free ARM register */
    uint8_t arm_reg = allocFPRegister();
    if (arm_reg != 0xff)
    {
        /* Get free RegisterNode, we must have some! */
        rn = free_pool.remHead();

        /* Update values in RegisterNode */
        rn->rn_Dirty = set_dirty;
        rn->rn_ARM = arm_reg;
        rn->rn_RegNum = reg;

        if (load) {
            uint8_t ctx = getCTX();
            uint16_t base_offset = __builtin_offsetof(PPCState, FPR) >> 3;
        
            /* Load value from PPC context into ARM register */
            if (reg >= 14 && reg <= 31)
            {
                emit(fldd_pimm(arm_reg, ctx, base_offset + FPR(reg)));
            }
            else
            {
                kprintf("[PPC] Illegal reg %d in IntMapFpr()\n", reg);
            }
        }

        /* Put into FPR_LRU */
        fpr_lru.addHead(rn);

        return arm_reg;
    }
    
    kprintf("[PPC] Run out of free FP registers. That should never happen\n");
    while(1) asm volatile("wfi");
}


FPR PPCTranslatorContext::tryGetFPR(uint8_t reg)
{
    /* If register is a fixed-assigned one, return ASAP */
    if (FP_REG_MAPPING[reg] != 0xff) {
        return FPR(FP_REG_MAPPING[reg]);
    }

    /* Not fixed mapped, check FPR_LRU now */
    for(auto rn : fpr_lru)
    {
        if (rn->rn_RegNum == reg) {
            return FPR(rn->rn_ARM);
        }
    }

    return FPR();
}

void PPCTranslatorContext::setDirtyFPR(uint8_t reg)
{
    /* Register with fixed mapping does not need to be set dirty */
    if (FP_REG_MAPPING[reg] != 0xff) return;

    /* Check if register is already in LRU */
    for(auto rn : fpr_lru)
    {
        if (rn->rn_RegNum == reg) {
            rn->rn_Dirty = 1;
            return;
        }
    }
}

void PPCTranslatorContext::flushAllFPRs()
{
    struct RegisterNode *rn;//, *next;
    uint8_t ctx = tryCTX();
    bool must_flush_ctx = ctx == 0xff;

    while((rn = fpr_lru.remHead()) != nullptr)
    {
        /* If dirty, store it back to PPC context */
        if (rn->rn_Dirty) {
            if (ctx == 0xff) {
                ctx = allocARMRegister();
                emit(mov_simd_to_reg(ctx, CTX_POINTER));
            }

            uint16_t base_offset = __builtin_offsetof(PPCState, FPR) >> 3;
            
            /* Store value from ARM register back into PPC context */
            if (rn->rn_RegNum >= 14 && rn->rn_RegNum <= 31)
            {
                emit(fstd_pimm(rn->rn_ARM, ctx, base_offset + rn->rn_RegNum));
            }
            else
            {
                kprintf("[PPC] Illegal reg %d in FlushAllFPRs()\n", rn->rn_ARM);
            }
        }
        
        /* Mark ARM register as free */
        freeFPRegister(rn->rn_ARM);

        /* Add the node itself to free pool */
        free_pool.addTail(rn);
    }

    if (must_flush_ctx)
        freeARMRegister(ctx);
}

uint8_t PPCTranslatorContext::intMapGPR(uint8_t reg, int load, int set_dirty)
{
    struct RegisterNode *rn;

    /* If register is a fixed-assigned one, return ASAP */
    if (INT_REG_MAPPING[reg] != 0xff) {
        return INT_REG_MAPPING[reg];
    }

    /* Check if register is already in LRU */
    for(auto rn : gpr_lru)
    {
        if (rn->rn_RegNum == reg)
        {
            /* Found it, move it to the top of LRU, return ARM reg number */
            rn->remove();
            gpr_lru.addHead(rn);

            /* Update dirty flag but does not allow to reset it */
            rn->rn_Dirty |= set_dirty;
            return rn->rn_ARM;
        }
    }

    /* Not found. Check if we have free ARM register */
    uint8_t arm_reg = allocARMRegister();
    if (arm_reg != 0xff)
    {
        /* Get free RegisterNode, we must have some! */
        rn = free_pool.remHead();

        /* Update values in RegisterNode */
        rn->rn_Dirty = set_dirty;
        rn->rn_ARM = arm_reg;
        rn->rn_RegNum = reg;

        if (load) {
            /* Load value from PPC context into ARM register */
            switch(reg) {
                case GPRn(14): emit(mov_simd_to_reg(arm_reg, GPR14)); break;
                case GPRn(15): emit(mov_simd_to_reg(arm_reg, GPR15)); break;
                case GPRn(16): emit(mov_simd_to_reg(arm_reg, GPR16)); break;
                case GPRn(17): emit(mov_simd_to_reg(arm_reg, GPR17)); break;
                case GPRn(18): emit(mov_simd_to_reg(arm_reg, GPR18)); break;
                case GPRn(19): emit(mov_simd_to_reg(arm_reg, GPR19)); break;
                case GPRn(20): emit(mov_simd_to_reg(arm_reg, GPR20)); break;
                case GPRn(21): emit(mov_simd_to_reg(arm_reg, GPR21)); break;
                case GPRn(22): emit(mov_simd_to_reg(arm_reg, GPR22)); break;
                case GPRn(23): emit(mov_simd_to_reg(arm_reg, GPR23)); break;
                case GPRn(24): emit(mov_simd_to_reg(arm_reg, GPR24)); break;
                case GPRn(25): emit(mov_simd_to_reg(arm_reg, GPR25)); break;
                case GPRn(26): emit(mov_simd_to_reg(arm_reg, GPR26)); break;
                case GPRn(27): emit(mov_simd_to_reg(arm_reg, GPR27)); break;
                case GPRn(28): emit(mov_simd_to_reg(arm_reg, GPR28)); break;
                case GPRn(29): emit(mov_simd_to_reg(arm_reg, GPR29)); break;
                case GPRn(30): emit(mov_simd_to_reg(arm_reg, GPR30)); break;
                case GPRn(31): emit(mov_simd_to_reg(arm_reg, GPR31)); break;
                case CRn: emit(mov_simd_to_reg(arm_reg, REG_CR)); break;
                case XERn: emit(mov_simd_to_reg(arm_reg, REG_XER)); break;
                case FPSCRn: emit(mov_simd_to_reg(arm_reg, REG_FPSCR)); break;
                default: kprintf("[PPC] Illegal reg %d in IntMapGpr()\n", reg);
            }
        }

        /* Put into GPR_LRU */
        gpr_lru.addHead(rn);

        return arm_reg;
    }
    
    /* No free ARM register? Impossible since AllocARMRegister should flush something! */
    kprintf("[PPC] Run out of free GP registers. That should never happen\n");
    while(1) asm volatile("wfi");
}

GPR PPCTranslatorContext::tryGetGPR(uint8_t reg)
{
    /* If register is a fixed-assigned one, return ASAP */
    if (INT_REG_MAPPING[reg] != 0xff) {
        return GPR(INT_REG_MAPPING[reg]);
    }

    /* Not fixed mapped, check GPR_LRU now */
    for(auto rn : gpr_lru)
    {
        if (rn->rn_RegNum == reg) {
            return GPR(rn->rn_ARM);
        }
    }

    return GPR();
}

void PPCTranslatorContext::setDirtyGPR(uint8_t reg)
{
    /* Register with fixed mapping does not need to be set dirty */
    if (INT_REG_MAPPING[reg] != 0xff) return;

    /* Check if register is already in LRU */
    for(auto rn : gpr_lru)
    {
        if (rn->rn_RegNum == reg) {
            rn->rn_Dirty = 1;
            return;
        }
    }
}

void PPCTranslatorContext::purgeFlushStore()
{
    struct FlushItem flush_store_sorted[20];
    struct FlushItem *last = flush_store_sorted;

    bzero(flush_store_sorted, sizeof(flush_store_sorted));

    /* Naive sorting by usage of lanes */
    for (int lanes = 4; lanes > 0; lanes--)
    {
        for (int i=0; i < 5; i++)
        {
            int count = 0;
            for (int j=0; j < 4; j++) {
                if (flush_store[4 * i + j].Vn != 0) count++;
            }

            if (count == lanes) {
                *last++ = flush_store[4 * i];
                *last++ = flush_store[4 * i + 1];
                *last++ = flush_store[4 * i + 2];
                *last++ = flush_store[4 * i + 3];
            }
        }
    }

    /* Emit stores, register by register, starting at first lane for given register which is there to be stored. */
    int stored = 0;
    do
    {
        stored = 0;
        for (unsigned int vn=0; vn < 5; vn++)
        {
            for (unsigned int lane=0; lane < 4; lane++) {
                if (flush_store_sorted[4 * vn + lane].Vn != 0)
                {
                    /* EMIT actual store */
                    emit(mov_reg_to_simd(
                                flush_store_sorted[4 * vn + lane].Vn,
                                (TS)flush_store_sorted[4 * vn + lane].Size,
                                flush_store_sorted[4 * vn + lane].Pos,
                                flush_store_sorted[4 * vn + lane].ARM)
                    );

                    flush_store_sorted[4 * vn + lane].Vn = 0;
                    stored = 1;
                    break;
                }
            }
        }
    } while(stored);
}

void PPCTranslatorContext::flushAllGPRs()
{
    struct RegisterNode *rn;//, *next;

    bzero(flush_store, sizeof(flush_store));

    while((rn = gpr_lru.remHead()) != nullptr)
    {
        /* If dirty, store it back to PPC context */
        if (rn->rn_Dirty) {
            /* Store value from ARM register back into PPC context */
            switch(rn->rn_RegNum) {
                case GPRn(14): markForFlush(GPR14, rn->rn_ARM); break;
                case GPRn(15): markForFlush(GPR15, rn->rn_ARM); break;
                case GPRn(16): markForFlush(GPR16, rn->rn_ARM); break;
                case GPRn(17): markForFlush(GPR17, rn->rn_ARM); break;
                case GPRn(18): markForFlush(GPR18, rn->rn_ARM); break;
                case GPRn(19): markForFlush(GPR19, rn->rn_ARM); break;
                case GPRn(20): markForFlush(GPR20, rn->rn_ARM); break;
                case GPRn(21): markForFlush(GPR21, rn->rn_ARM); break;
                case GPRn(22): markForFlush(GPR22, rn->rn_ARM); break;
                case GPRn(23): markForFlush(GPR23, rn->rn_ARM); break;
                case GPRn(24): markForFlush(GPR24, rn->rn_ARM); break;
                case GPRn(25): markForFlush(GPR25, rn->rn_ARM); break;
                case GPRn(26): markForFlush(GPR26, rn->rn_ARM); break;
                case GPRn(27): markForFlush(GPR27, rn->rn_ARM); break;
                case GPRn(28): markForFlush(GPR28, rn->rn_ARM); break;
                case GPRn(29): markForFlush(GPR29, rn->rn_ARM); break;
                case GPRn(30): markForFlush(GPR30, rn->rn_ARM); break;
                case GPRn(31): markForFlush(GPR31, rn->rn_ARM); break;
                case CRn: markForFlush(REG_CR, rn->rn_ARM); break;
                case XERn: markForFlush(REG_XER, rn->rn_ARM); break;
                case FPSCRn: emit(mov_reg_to_simd(REG_FPSCR, rn->rn_ARM)); break;
                default: kprintf("[PPC] Illegal reg %d in IntMapGpr()\n", rn->rn_ARM);
            }
        }
        
        /* Mark ARM register as free */
        freeARMRegister(rn->rn_ARM);

        /* Add the node itself to free pool */
        free_pool.addTail(rn);
    }

    purgeFlushStore();
}

void PPCTranslatorContext::storeDirtyGPRs()
{
    bzero(flush_store, sizeof(flush_store));

    for(auto rn : gpr_lru)
    {
        /* If dirty, store it back to PPC context */
        if (rn->rn_Dirty) {
            /* Store value from ARM register back into PPC context */
            switch(rn->rn_RegNum) {
                case GPRn(14): markForFlush(GPR14, rn->rn_ARM); break;
                case GPRn(15): markForFlush(GPR15, rn->rn_ARM); break;
                case GPRn(16): markForFlush(GPR16, rn->rn_ARM); break;
                case GPRn(17): markForFlush(GPR17, rn->rn_ARM); break;
                case GPRn(18): markForFlush(GPR18, rn->rn_ARM); break;
                case GPRn(19): markForFlush(GPR19, rn->rn_ARM); break;
                case GPRn(20): markForFlush(GPR20, rn->rn_ARM); break;
                case GPRn(21): markForFlush(GPR21, rn->rn_ARM); break;
                case GPRn(22): markForFlush(GPR22, rn->rn_ARM); break;
                case GPRn(23): markForFlush(GPR23, rn->rn_ARM); break;
                case GPRn(24): markForFlush(GPR24, rn->rn_ARM); break;
                case GPRn(25): markForFlush(GPR25, rn->rn_ARM); break;
                case GPRn(26): markForFlush(GPR26, rn->rn_ARM); break;
                case GPRn(27): markForFlush(GPR27, rn->rn_ARM); break;
                case GPRn(28): markForFlush(GPR28, rn->rn_ARM); break;
                case GPRn(29): markForFlush(GPR29, rn->rn_ARM); break;
                case GPRn(30): markForFlush(GPR30, rn->rn_ARM); break;
                case GPRn(31): markForFlush(GPR31, rn->rn_ARM); break;
                case CRn: markForFlush(REG_CR, rn->rn_ARM); break;
                case XERn: markForFlush(REG_XER, rn->rn_ARM); break;
                case FPSCRn: emit(mov_reg_to_simd(REG_FPSCR, rn->rn_ARM)); break;
                default: kprintf("[PPC] Illegal reg %d in IntMapGpr()\n", rn->rn_ARM);
            }
        }
    }

    purgeFlushStore();
}

void PPCTranslatorContext::putToLocalState(PPCLocalState *local_state)
{
    local_state->pls_ARMOffset = tc_CodePtr - tc_CodeStart;
    local_state->pls_PPCPtr = tc_PPCCodePtr;
    local_state->pls_PCRel = tc_pc_rel;
    for (unsigned i=0; i < sizeof(INT_REG_MAPPING); i++)
    {
        uint8_t map = INT_REG_MAPPING[i];
        if (map == 0xff) {
            for(const auto rn : gpr_lru)
            {
                if (rn->rn_RegNum == i) {
                    map = rn->rn_RegNum;
                    if (rn->rn_Dirty) map |= 0x80;
                    break;
                }
            }
        }
        local_state->pls_RegMap[i] = map;
    }
}
#if 0
static const char* regs[] = { 
    "r00", "r01", "r02", "r03", "r04", "r05", "r06", "r07",
    "r08", "r09", "r10", "r11", "r12", "r13", "r14", "r15",
    "r16", "r17", "r18", "r19", "r20", "r21", "r22", "r23",
    "r24", "r25", "r26", "r27", "r28", "r29", "r30", "r31",
    "cr", "xer", "lr", "ctr", "fpscr" };
#endif
uint32_t* PPCTranslatorContext::save()
{
    RegisterSnapshot *snap = new RegisterSnapshot();

    snap->tc_pc_rel = tc_pc_rel;
    snap->reg_ctx = reg_ctx;
    snap->gpr_tmp_pool = gpr_tmp_pool;
    snap->fpr_tmp_pool = fpr_tmp_pool;

    snap->code_ptr = tc_CodePtr;

    memcpy(&snap->rn, &rn, sizeof(rn));
    memcpy(&snap->free_pool, &free_pool, sizeof(free_pool));
    memcpy(&snap->gpr_lru, &gpr_lru, sizeof(gpr_lru));
    memcpy(&snap->fpr_lru, &fpr_lru, sizeof(fpr_lru));
#if 0
    kprintf("[PPC] save() dirty regs: ");
    for (auto rn : gpr_lru) {
        if (rn->rn_Dirty) {
            kprintf("%s(w%02d)  ", regs[rn->rn_RegNum], rn->rn_ARM);
        }
    }
    kprintf("\n");
#endif
    snapshots.addHead(snap);

    return tc_CodePtr;
}

uint32_t* PPCTranslatorContext::restore()
{
    RegisterSnapshot *snap = snapshots.remHead();

    if (snap == nullptr) {
        kprintf("[PPC] PPCTranslatorContext: restore() attempted without previous save()\n");
        while(1) asm volatile("wfi");
    }

    uint32_t* old_ptr = tc_CodePtr;

    memcpy(&rn, &snap->rn, sizeof(rn));
    memcpy(&free_pool, &snap->free_pool, sizeof(free_pool));
    memcpy(&gpr_lru, &snap->gpr_lru, sizeof(gpr_lru));
    memcpy(&fpr_lru, &snap->fpr_lru, sizeof(fpr_lru));

    tc_pc_rel = snap->tc_pc_rel;
    reg_ctx = snap->reg_ctx;
    gpr_tmp_pool = snap->gpr_tmp_pool;
    fpr_tmp_pool = snap->fpr_tmp_pool;
    tc_CodePtr = snap->code_ptr;

    delete snap;

    return old_ptr;
}

} // namespace Emu68::PPC
