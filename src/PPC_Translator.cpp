/*
    Copyright © 2019 Michal Schulz <michal.schulz@gmx.de>
    https://github.com/michalsc

    This Source Code Form is subject to the terms of the
    Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed
    with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#define _GNU_SOURCE 1

#define restrict __restrict__

#include <cpp/lists>
#include <cpp/nodes>
#include <cpp/LRUCache>

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

#if 0
/* Disable INSN counter for now (not needed yet) */
#undef EMU68_INSN_COUNTER
#define EMU68_INSN_COUNTER 0
#endif

#define jit_tlsf DO_NOT_USE_jit_tlsf

#define FPR(n)  (n)

TLSF jit_ppc;

uint32_t ARMTmpPool;
uint32_t FPTmpPool;
uint8_t reg_CTX = 0xff;
uint32_t *temporary_arm_code;
uint32_t *ppc_high;
uint32_t *ppc_low;
uint32_t insn_count;
uint32_t * ppc_entry_point;
uint32_t debug_range_min = 0x00000000;
uint32_t debug_range_max = 0xffffffff;
struct PPCLocalState *local_state;

Emu68::List<RegisterNode> FreePool;
Emu68::List<RegisterNode> GPR_LRU;
Emu68::List<RegisterNode> FPR_LRU;

extern Emu68::List<PPCTranslationUnit> ICache[EMU68_HASHSIZE];
extern Emu68::List<TranslationUnitLRU> LRU;

static __used__ uint32_t GetTempAllocMask(TranslatorContext *)
{
    return ARMTmpPool;
}

uint8_t AllocARMRegister(PPCTranslatorContext *tc)
{
    static int last_allocated = 0;

    for (int i=1; i <= 12; i++)
    {
        int reg = (last_allocated + i) % 12;

        if (((ARMTmpPool) & (1 << reg)) == 0)
        {
            ARMTmpPool |= 1 << reg;
            last_allocated = reg;
            return reg;
        }
    }
    /* No free ARM register. Remove last entry from GPR_LRU */
    struct RegisterNode *rn = GPR_LRU.remTail();

    /* If dirty, store it back to PPC context */
    if (rn->rn_Dirty) {
        /* Store value from ARM register back into PPC context */
        switch(rn->rn_RegNum) {
            case GPR(14): tc->EMIT(mov_reg_to_simd(GPR14, rn->rn_ARM)); break;
            case GPR(15): tc->EMIT(mov_reg_to_simd(GPR15, rn->rn_ARM)); break;
            case GPR(16): tc->EMIT(mov_reg_to_simd(GPR16, rn->rn_ARM)); break;
            case GPR(17): tc->EMIT(mov_reg_to_simd(GPR17, rn->rn_ARM)); break;
            case GPR(18): tc->EMIT(mov_reg_to_simd(GPR18, rn->rn_ARM)); break;
            case GPR(19): tc->EMIT(mov_reg_to_simd(GPR19, rn->rn_ARM)); break;
            case GPR(20): tc->EMIT(mov_reg_to_simd(GPR20, rn->rn_ARM)); break;
            case GPR(21): tc->EMIT(mov_reg_to_simd(GPR21, rn->rn_ARM)); break;
            case GPR(22): tc->EMIT(mov_reg_to_simd(GPR22, rn->rn_ARM)); break;
            case GPR(23): tc->EMIT(mov_reg_to_simd(GPR23, rn->rn_ARM)); break;
            case GPR(24): tc->EMIT(mov_reg_to_simd(GPR24, rn->rn_ARM)); break;
            case GPR(25): tc->EMIT(mov_reg_to_simd(GPR25, rn->rn_ARM)); break;
            case GPR(26): tc->EMIT(mov_reg_to_simd(GPR26, rn->rn_ARM)); break;
            case GPR(27): tc->EMIT(mov_reg_to_simd(GPR27, rn->rn_ARM)); break;
            case GPR(28): tc->EMIT(mov_reg_to_simd(GPR28, rn->rn_ARM)); break;
            case GPR(29): tc->EMIT(mov_reg_to_simd(GPR29, rn->rn_ARM)); break;
            case GPR(30): tc->EMIT(mov_reg_to_simd(GPR30, rn->rn_ARM)); break;
            case GPR(31): tc->EMIT(mov_reg_to_simd(GPR31, rn->rn_ARM)); break;
            case CRn: tc->EMIT(mov_reg_to_simd(REG_CR, rn->rn_ARM)); break;
            case XERn: tc->EMIT(mov_reg_to_simd(REG_XER, rn->rn_ARM)); break;
            case FPSCRn: tc->EMIT(mov_reg_to_simd(REG_FPSCR, rn->rn_ARM)); break;
            default: kprintf("[PPC] Illegal reg %d in IntMapGpr()\n", rn->rn_ARM);
        }
    }

    FreePool.addTail(rn);

    return rn->rn_ARM;
}

void FreeARMRegister(struct PPCTranslatorContext *, uint8_t arm_reg)
{
    if (arm_reg > 11)
        return;

    ARMTmpPool &= ~(1 << arm_reg);
}

uint8_t AllocFPRegister(PPCTranslatorContext *tc)
{
    static int last_allocated = 0;

    for (int i=1; i <= 8; i++)
    {
        int reg = (last_allocated + i) % 8;

        if (((FPTmpPool) & (1 << reg)) == 0)
        {
            FPTmpPool |= 1 << reg;
            last_allocated = reg;
            return reg;
        }
    }

    /* No free FP register. Remove last entry from FPR_LRU */
    struct RegisterNode *rn = FPR_LRU.remTail();

    /* If dirty, store it back to PPC context */
    if (rn->rn_Dirty) {
        uint8_t ctx = GetCTX(tc);
        uint16_t base_offset = __builtin_offsetof(PPCState, FPR) >> 3;
        
        /* Store value from ARM register back into PPC context */
        if (rn->rn_RegNum >= 14 && rn->rn_RegNum <= 31)
        {
            tc->EMIT(fstd_pimm(rn->rn_ARM, ctx, base_offset + rn->rn_RegNum));
        }
        else
        {
            kprintf("[PPC] Illegal reg %d in IntMapFpr()\n", rn->rn_ARM);
        }
    }

    FreePool.addTail(rn);

    return rn->rn_ARM;
}

void FreeFPRegister(struct PPCTranslatorContext *, uint8_t fp_reg)
{
    if (fp_reg > 7)
        return;

    FPTmpPool &= ~(1 << fp_reg);
}

static __used__ uint8_t IntMapFPR(struct PPCTranslatorContext *tc, uint8_t reg, int load, int set_dirty)
{
    struct RegisterNode *rn;

    /* If register is a fixed-assigned one, return ASAP */
    if (FP_REG_MAPPING[reg] != 0xff) {
        return FP_REG_MAPPING[reg];
    }

    /* Check if register is already in LRU */
    for(auto rn: FPR_LRU)
    {
        if (rn->rn_RegNum == reg)
        {
            /* Found it, move it to the top of LRU, return ARM reg number */
            rn->remove();
            FPR_LRU.addHead(rn);

            /* Update dirty flag but does not allow to reset it */
            rn->rn_Dirty |= set_dirty;
            return rn->rn_ARM;
        }
    }

    /* Not found. Get a free ARM register */
    uint8_t arm_reg = AllocFPRegister(tc);
    if (arm_reg != 0xff)
    {
        /* Get free RegisterNode, we must have some! */
        rn = FreePool.remHead();

        /* Update values in RegisterNode */
        rn->rn_Dirty = set_dirty;
        rn->rn_ARM = arm_reg;
        rn->rn_RegNum = reg;

        if (load) {
            uint8_t ctx = GetCTX(tc);
            uint16_t base_offset = __builtin_offsetof(PPCState, FPR) >> 3;
        
            /* Load value from PPC context into ARM register */
            if (reg >= 14 && reg <= 31)
            {
                tc->EMIT(fldd_pimm(arm_reg, ctx, base_offset + FPR(reg)));
            }
            else
            {
                kprintf("[PPC] Illegal reg %d in IntMapFpr()\n", reg);
            }
        }

        /* Put into GPR_LRU */
        FPR_LRU.addHead(rn);

        return arm_reg;
    }
    
    kprintf("[PPC] Run out of free FP registers. That should never happen\n");
    while(1) asm volatile("wfi");
}

uint8_t MapFPRForRead(struct PPCTranslatorContext *tc, uint8_t reg)
{
    return IntMapFPR(tc, reg, 1, 0);
}

uint8_t MapFPRForReadAndWrite(struct PPCTranslatorContext *tc, uint8_t reg)
{
    return IntMapFPR(tc, reg, 1, 1);
}

uint8_t MapFPRForWrite(struct PPCTranslatorContext *tc, uint8_t reg)
{
    return IntMapFPR(tc, reg, 0, 1);
}

uint8_t IsFPRMapped(struct PPCTranslatorContext *, uint8_t reg)
{
    /* If register is a fixed-assigned one, return ASAP */
    if (FP_REG_MAPPING[reg] != 0xff) {
        return FP_REG_MAPPING[reg];
    }

    /* Not fixed mapped, check GPR_LRU now */
    for(auto rn: FPR_LRU)
    {
        if (rn->rn_RegNum == reg) {
            return rn->rn_ARM;
        }
    }

    return 0xff;
}

static __used__ void SetDirtyFPR(struct TranslatorContext *, uint8_t reg)
{
    /* Register with fixed mapping does not need to be set dirty */
    if (FP_REG_MAPPING[reg] != 0xff) return;

    /* Check if register is already in LRU */
    for(auto rn: FPR_LRU)
    {
        if (rn->rn_RegNum == reg) {
            rn->rn_Dirty = 1;
            return;
        }
    }
}

static __used__ void FlushAllFPRs(struct PPCTranslatorContext *tc)
{
    struct RegisterNode *rn;//, *next;
    uint8_t ctx = TryCTX(tc);
    bool must_flush_ctx = ctx == 0xff;

    while((rn = FPR_LRU.remHead()) != nullptr)
    {
        /* If dirty, store it back to PPC context */
        if (rn->rn_Dirty) {
            if (ctx == 0xff) {
                ctx = AllocARMRegister(tc);
                tc->EMIT(mov_simd_to_reg(ctx, CTX_POINTER));
            }

            uint16_t base_offset = __builtin_offsetof(PPCState, FPR) >> 3;
            
            /* Store value from ARM register back into PPC context */
            if (rn->rn_RegNum >= 14 && rn->rn_RegNum <= 31)
            {
                tc->EMIT(fstd_pimm(rn->rn_ARM, ctx, base_offset + rn->rn_RegNum));
            }
            else
            {
                kprintf("[PPC] Illegal reg %d in FlushAllFPRs()\n", rn->rn_ARM);
            }
        }
        
        /* Mark ARM register as free */
        FreeFPRegister(tc, rn->rn_ARM);

        /* Add the node itself to free pool */
        FreePool.addTail(rn);
    }

    if (must_flush_ctx)
        FreeARMRegister(tc, ctx);
}

void StoreDirtyFPRs(struct PPCTranslatorContext *tc)
{
    uint8_t ctx = TryCTX(tc);
    bool must_flush_ctx = ctx == 0xff;

    for(auto rn: FPR_LRU)
    {
        /* If dirty, store it back to PPC context */
        if (rn->rn_Dirty) {
            if (ctx == 0xff) {
                ctx = AllocARMRegister(tc);
                tc->EMIT(mov_simd_to_reg(ctx, CTX_POINTER));
            }

            uint16_t base_offset = __builtin_offsetof(PPCState, FPR) >> 3;
            
            /* Store value from ARM register back into PPC context */
            if (rn->rn_RegNum >= 14 && rn->rn_RegNum <= 31)
            {
                tc->EMIT(fstd_pimm(rn->rn_ARM, ctx, base_offset + rn->rn_RegNum));
            }
            else
            {
                kprintf("[PPC] Illegal reg %d in FlushAllFPRs()\n", rn->rn_ARM);
            }

            FreeARMRegister(tc, ctx);
        }
    }

    if (must_flush_ctx)
        FreeARMRegister(tc, ctx);
}

void AddImmediate(struct PPCTranslatorContext *tc, uint8_t rd, int32_t delta) {
    if (delta < 0) {
        delta = -delta;
        if ((delta & 0xfffff000) == 0) {
            tc->EMIT(sub_immed(rd, rd, delta));
        } else if ((delta & 0xff000fff) == 0) {
            tc->EMIT(sub_immed_lsl12(rd, rd, delta >> 12));
        } else {
            uint8_t tmp = AllocARMRegister(tc);
            tc->LoadImmediate(tmp, (uint32_t)delta);
            tc->EMIT(sub_reg(rd, rd, tmp, LSL, 0));
            FreeARMRegister(tc, tmp);
        }
    } else if (delta > 0) {
        if ((delta & 0xfffff000) == 0) {
            tc->EMIT(add_immed(rd, rd, delta));
        } else if ((delta & 0xff000fff) == 0) {
            tc->EMIT(add_immed_lsl12(rd, rd, delta >> 12));
        } else {
            uint8_t tmp = AllocARMRegister(tc);
            tc->LoadImmediate(tmp, (uint32_t)delta);
            tc->EMIT(add_reg(rd, rd, tmp, LSL, 0));
            FreeARMRegister(tc, tmp);
        }
    }
}

uint8_t TryCTX(struct TranslatorContext *)
{
    return reg_CTX;
}

uint8_t GetCTX(struct PPCTranslatorContext *tc)
{
    if (reg_CTX == 0xff)
    {
        reg_CTX = AllocARMRegister(tc);
        tc->EMIT(mov_simd_to_reg(reg_CTX, CTX_POINTER));
    }

    return reg_CTX;
}

static __used__ void FlushCTX(struct PPCTranslatorContext *tc)
{
    if (reg_CTX != 0xff)
    {
        FreeARMRegister(tc, reg_CTX);
    }

    reg_CTX = 0xff;
}

static __used__ uint8_t IntMapGPR(struct PPCTranslatorContext *tc, uint8_t reg, int load, int set_dirty)
{
    struct RegisterNode *rn;

    /* If register is a fixed-assigned one, return ASAP */
    if (INT_REG_MAPPING[reg] != 0xff) {
        return INT_REG_MAPPING[reg];
    }

    /* Check if register is already in LRU */
    for(auto rn: GPR_LRU)
    {
        if (rn->rn_RegNum == reg)
        {
            /* Found it, move it to the top of LRU, return ARM reg number */
            rn->remove();
            GPR_LRU.addHead(rn);

            /* Update dirty flag but does not allow to reset it */
            rn->rn_Dirty |= set_dirty;
            return rn->rn_ARM;
        }
    }

    /* Not found. Check if we have free ARM register */
    uint8_t arm_reg = AllocARMRegister(tc);
    if (arm_reg != 0xff)
    {
        /* Get free RegisterNode, we must have some! */
        rn = FreePool.remHead();

        /* Update values in RegisterNode */
        rn->rn_Dirty = set_dirty;
        rn->rn_ARM = arm_reg;
        rn->rn_RegNum = reg;

        if (load) {
            /* Load value from PPC context into ARM register */
            switch(reg) {
                case GPR(14): tc->EMIT(mov_simd_to_reg(arm_reg, GPR14)); break;
                case GPR(15): tc->EMIT(mov_simd_to_reg(arm_reg, GPR15)); break;
                case GPR(16): tc->EMIT(mov_simd_to_reg(arm_reg, GPR16)); break;
                case GPR(17): tc->EMIT(mov_simd_to_reg(arm_reg, GPR17)); break;
                case GPR(18): tc->EMIT(mov_simd_to_reg(arm_reg, GPR18)); break;
                case GPR(19): tc->EMIT(mov_simd_to_reg(arm_reg, GPR19)); break;
                case GPR(20): tc->EMIT(mov_simd_to_reg(arm_reg, GPR20)); break;
                case GPR(21): tc->EMIT(mov_simd_to_reg(arm_reg, GPR21)); break;
                case GPR(22): tc->EMIT(mov_simd_to_reg(arm_reg, GPR22)); break;
                case GPR(23): tc->EMIT(mov_simd_to_reg(arm_reg, GPR23)); break;
                case GPR(24): tc->EMIT(mov_simd_to_reg(arm_reg, GPR24)); break;
                case GPR(25): tc->EMIT(mov_simd_to_reg(arm_reg, GPR25)); break;
                case GPR(26): tc->EMIT(mov_simd_to_reg(arm_reg, GPR26)); break;
                case GPR(27): tc->EMIT(mov_simd_to_reg(arm_reg, GPR27)); break;
                case GPR(28): tc->EMIT(mov_simd_to_reg(arm_reg, GPR28)); break;
                case GPR(29): tc->EMIT(mov_simd_to_reg(arm_reg, GPR29)); break;
                case GPR(30): tc->EMIT(mov_simd_to_reg(arm_reg, GPR30)); break;
                case GPR(31): tc->EMIT(mov_simd_to_reg(arm_reg, GPR31)); break;
                case CRn: tc->EMIT(mov_simd_to_reg(arm_reg, REG_CR)); break;
                case XERn: tc->EMIT(mov_simd_to_reg(arm_reg, REG_XER)); break;
                case FPSCRn: tc->EMIT(mov_simd_to_reg(arm_reg, REG_FPSCR)); break;
                default: kprintf("[PPC] Illegal reg %d in IntMapGpr()\n", reg);
            }
        }

        /* Put into GPR_LRU */
        GPR_LRU.addHead(rn);

        return arm_reg;
    }
    
    /* No free ARM register. Remove last entry from GPR_LRU */
    rn = GPR_LRU.remTail();

    /* If dirty, store it back to PPC context */
    if (rn->rn_Dirty) {
        /* Store value from ARM register back into PPC context */
        switch(rn->rn_RegNum) {
            case GPR(14): tc->EMIT(mov_reg_to_simd(GPR14, rn->rn_ARM)); break;
            case GPR(15): tc->EMIT(mov_reg_to_simd(GPR15, rn->rn_ARM)); break;
            case GPR(16): tc->EMIT(mov_reg_to_simd(GPR16, rn->rn_ARM)); break;
            case GPR(17): tc->EMIT(mov_reg_to_simd(GPR17, rn->rn_ARM)); break;
            case GPR(18): tc->EMIT(mov_reg_to_simd(GPR18, rn->rn_ARM)); break;
            case GPR(19): tc->EMIT(mov_reg_to_simd(GPR19, rn->rn_ARM)); break;
            case GPR(20): tc->EMIT(mov_reg_to_simd(GPR20, rn->rn_ARM)); break;
            case GPR(21): tc->EMIT(mov_reg_to_simd(GPR21, rn->rn_ARM)); break;
            case GPR(22): tc->EMIT(mov_reg_to_simd(GPR22, rn->rn_ARM)); break;
            case GPR(23): tc->EMIT(mov_reg_to_simd(GPR23, rn->rn_ARM)); break;
            case GPR(24): tc->EMIT(mov_reg_to_simd(GPR24, rn->rn_ARM)); break;
            case GPR(25): tc->EMIT(mov_reg_to_simd(GPR25, rn->rn_ARM)); break;
            case GPR(26): tc->EMIT(mov_reg_to_simd(GPR26, rn->rn_ARM)); break;
            case GPR(27): tc->EMIT(mov_reg_to_simd(GPR27, rn->rn_ARM)); break;
            case GPR(28): tc->EMIT(mov_reg_to_simd(GPR28, rn->rn_ARM)); break;
            case GPR(29): tc->EMIT(mov_reg_to_simd(GPR29, rn->rn_ARM)); break;
            case GPR(30): tc->EMIT(mov_reg_to_simd(GPR30, rn->rn_ARM)); break;
            case GPR(31): tc->EMIT(mov_reg_to_simd(GPR31, rn->rn_ARM)); break;
            case CRn: tc->EMIT(mov_reg_to_simd(REG_CR, rn->rn_ARM)); break;
            case XERn: tc->EMIT(mov_reg_to_simd(REG_XER, rn->rn_ARM)); break;
            case FPSCRn: tc->EMIT(mov_reg_to_simd(REG_FPSCR, rn->rn_ARM)); break;
            default: kprintf("[PPC] Illegal reg %d in IntMapGpr()\n", rn->rn_ARM);
        }
    }

    /* Update values in RegisterNode */
    rn->rn_Dirty = set_dirty;
    rn->rn_RegNum = reg;

    if (load) {
        /* Load value from PPC context into ARM register */
        switch(reg) {
            case GPR(14): tc->EMIT(mov_simd_to_reg(arm_reg, GPR14)); break;
            case GPR(15): tc->EMIT(mov_simd_to_reg(arm_reg, GPR15)); break;
            case GPR(16): tc->EMIT(mov_simd_to_reg(arm_reg, GPR16)); break;
            case GPR(17): tc->EMIT(mov_simd_to_reg(arm_reg, GPR17)); break;
            case GPR(18): tc->EMIT(mov_simd_to_reg(arm_reg, GPR18)); break;
            case GPR(19): tc->EMIT(mov_simd_to_reg(arm_reg, GPR19)); break;
            case GPR(20): tc->EMIT(mov_simd_to_reg(arm_reg, GPR20)); break;
            case GPR(21): tc->EMIT(mov_simd_to_reg(arm_reg, GPR21)); break;
            case GPR(22): tc->EMIT(mov_simd_to_reg(arm_reg, GPR22)); break;
            case GPR(23): tc->EMIT(mov_simd_to_reg(arm_reg, GPR23)); break;
            case GPR(24): tc->EMIT(mov_simd_to_reg(arm_reg, GPR24)); break;
            case GPR(25): tc->EMIT(mov_simd_to_reg(arm_reg, GPR25)); break;
            case GPR(26): tc->EMIT(mov_simd_to_reg(arm_reg, GPR26)); break;
            case GPR(27): tc->EMIT(mov_simd_to_reg(arm_reg, GPR27)); break;
            case GPR(28): tc->EMIT(mov_simd_to_reg(arm_reg, GPR28)); break;
            case GPR(29): tc->EMIT(mov_simd_to_reg(arm_reg, GPR29)); break;
            case GPR(30): tc->EMIT(mov_simd_to_reg(arm_reg, GPR30)); break;
            case GPR(31): tc->EMIT(mov_simd_to_reg(arm_reg, GPR31)); break;
            case CRn: tc->EMIT(mov_simd_to_reg(arm_reg, REG_CR)); break;
            case XERn: tc->EMIT(mov_simd_to_reg(arm_reg, REG_XER)); break;
            case FPSCRn: tc->EMIT(mov_simd_to_reg(arm_reg, REG_FPSCR)); break;
            default: kprintf("[PPC] Illegal reg %d in IntMapGpr()\n", reg);
        }
    }

    /* Put into GPR_LRU */
    GPR_LRU.addHead(rn);

    return rn->rn_ARM;
}

uint8_t MapGPRForRead(struct PPCTranslatorContext *tc, uint8_t reg)
{
    return IntMapGPR(tc, reg, 1, 0);
}

uint8_t MapGPRForReadAndWrite(struct PPCTranslatorContext *tc, uint8_t reg)
{
    return IntMapGPR(tc, reg, 1, 1);
}

uint8_t MapGPRForWrite(struct PPCTranslatorContext *tc, uint8_t reg)
{
    return IntMapGPR(tc, reg, 0, 1);
}

uint8_t IsGPRMapped(struct PPCTranslatorContext *, uint8_t reg)
{
    /* If register is a fixed-assigned one, return ASAP */
    if (INT_REG_MAPPING[reg] != 0xff) {
        return INT_REG_MAPPING[reg];
    }

    /* Not fixed mapped, check GPR_LRU now */
    for(auto rn: GPR_LRU)
    {
//        kprintf("testing node %p, reg %d, arm reg %d, dirty %d\n", rn, rn->rn_RegNum, rn->rn_ARM, rn->rn_Dirty);
        if (rn->rn_RegNum == reg) {
            return rn->rn_ARM;
        }
    }

    return 0xff;
}

static __used__ void SetDirtyGPR(struct TranslatorContext *, uint8_t reg)
{
    /* Register with fixed mapping does not need to be set dirty */
    if (INT_REG_MAPPING[reg] != 0xff) return;

    /* Check if register is already in LRU */
    for(auto rn: GPR_LRU)
    {
        if (rn->rn_RegNum == reg) {
            rn->rn_Dirty = 1;
            return;
        }
    }
}

struct FlushItem {
    uint8_t Vn, Size, Pos, ARM;
} FlushStore[20];

inline void MarkForFlush(uint8_t vn, uint8_t size, uint8_t lane, uint8_t arm) {
    FlushStore[lane + (vn - 22) * 4].Vn = vn;
    FlushStore[lane + (vn - 22) * 4].Size = size;
    FlushStore[lane + (vn - 22) * 4].Pos = lane;
    FlushStore[lane + (vn - 22) * 4].ARM = arm;
}

void PurgeFlushStore(struct TranslatorContext *tc)
{
    struct FlushItem FlushStoreSorted[20];
    struct FlushItem *last = FlushStoreSorted;

    bzero(FlushStoreSorted, sizeof(FlushStoreSorted));

    /* Naive sorting by usage of lanes */
    for (int lanes = 4; lanes > 0; lanes--)
    {
        for (int i=0; i < 5; i++)
        {
            int count = 0;
            for (int j=0; j < 4; j++) {
                if (FlushStore[4 * i + j].Vn != 0) count++;
            }

            if (count == lanes) {
                *last++ = FlushStore[4 * i];
                *last++ = FlushStore[4 * i + 1];
                *last++ = FlushStore[4 * i + 2];
                *last++ = FlushStore[4 * i + 3];
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
                if (FlushStoreSorted[4 * vn + lane].Vn != 0)
                {
                    /* EMIT actual store */
                    tc->EMIT(mov_reg_to_simd(
                                FlushStoreSorted[4 * vn + lane].Vn,
                                (TS)FlushStoreSorted[4 * vn + lane].Size,
                                FlushStoreSorted[4 * vn + lane].Pos,
                                FlushStoreSorted[4 * vn + lane].ARM)
                    );
                    
                    FlushStoreSorted[4 * vn + lane].Vn = 0;
                    stored = 1;
                    break;
                }
            }
        }
    } while(stored);
}

static __used__ void FlushAllGPRs(struct PPCTranslatorContext *tc)
{
    struct RegisterNode *rn;//, *next;

    bzero(FlushStore, sizeof(FlushStore));

    while((rn = GPR_LRU.remHead()) != nullptr)
    {
        /* If dirty, store it back to PPC context */
        if (rn->rn_Dirty) {
            /* Store value from ARM register back into PPC context */
            switch(rn->rn_RegNum) {
                case GPR(14): MarkForFlush(GPR14, rn->rn_ARM); break;
                case GPR(15): MarkForFlush(GPR15, rn->rn_ARM); break;
                case GPR(16): MarkForFlush(GPR16, rn->rn_ARM); break;
                case GPR(17): MarkForFlush(GPR17, rn->rn_ARM); break;
                case GPR(18): MarkForFlush(GPR18, rn->rn_ARM); break;
                case GPR(19): MarkForFlush(GPR19, rn->rn_ARM); break;
                case GPR(20): MarkForFlush(GPR20, rn->rn_ARM); break;
                case GPR(21): MarkForFlush(GPR21, rn->rn_ARM); break;
                case GPR(22): MarkForFlush(GPR22, rn->rn_ARM); break;
                case GPR(23): MarkForFlush(GPR23, rn->rn_ARM); break;
                case GPR(24): MarkForFlush(GPR24, rn->rn_ARM); break;
                case GPR(25): MarkForFlush(GPR25, rn->rn_ARM); break;
                case GPR(26): MarkForFlush(GPR26, rn->rn_ARM); break;
                case GPR(27): MarkForFlush(GPR27, rn->rn_ARM); break;
                case GPR(28): MarkForFlush(GPR28, rn->rn_ARM); break;
                case GPR(29): MarkForFlush(GPR29, rn->rn_ARM); break;
                case GPR(30): MarkForFlush(GPR30, rn->rn_ARM); break;
                case GPR(31): MarkForFlush(GPR31, rn->rn_ARM); break;
                case CRn: MarkForFlush(REG_CR, rn->rn_ARM); break;
                case XERn: MarkForFlush(REG_XER, rn->rn_ARM); break;
                case FPSCRn: tc->EMIT(mov_reg_to_simd(REG_FPSCR, rn->rn_ARM)); break;
                default: kprintf("[PPC] Illegal reg %d in IntMapGpr()\n", rn->rn_ARM);
            }
        }
        
        /* Mark ARM register as free */
        FreeARMRegister(tc, rn->rn_ARM);

        /* Add the node itself to free pool */
        FreePool.addTail(rn);
    }

    PurgeFlushStore(tc);
}

void StoreDirtyGPRs(struct PPCTranslatorContext *tc)
{
    bzero(FlushStore, sizeof(FlushStore));

    for(auto rn: GPR_LRU)
    {
        /* If dirty, store it back to PPC context */
        if (rn->rn_Dirty) {
            /* Store value from ARM register back into PPC context */
            switch(rn->rn_RegNum) {
                case GPR(14): MarkForFlush(GPR14, rn->rn_ARM); break;
                case GPR(15): MarkForFlush(GPR15, rn->rn_ARM); break;
                case GPR(16): MarkForFlush(GPR16, rn->rn_ARM); break;
                case GPR(17): MarkForFlush(GPR17, rn->rn_ARM); break;
                case GPR(18): MarkForFlush(GPR18, rn->rn_ARM); break;
                case GPR(19): MarkForFlush(GPR19, rn->rn_ARM); break;
                case GPR(20): MarkForFlush(GPR20, rn->rn_ARM); break;
                case GPR(21): MarkForFlush(GPR21, rn->rn_ARM); break;
                case GPR(22): MarkForFlush(GPR22, rn->rn_ARM); break;
                case GPR(23): MarkForFlush(GPR23, rn->rn_ARM); break;
                case GPR(24): MarkForFlush(GPR24, rn->rn_ARM); break;
                case GPR(25): MarkForFlush(GPR25, rn->rn_ARM); break;
                case GPR(26): MarkForFlush(GPR26, rn->rn_ARM); break;
                case GPR(27): MarkForFlush(GPR27, rn->rn_ARM); break;
                case GPR(28): MarkForFlush(GPR28, rn->rn_ARM); break;
                case GPR(29): MarkForFlush(GPR29, rn->rn_ARM); break;
                case GPR(30): MarkForFlush(GPR30, rn->rn_ARM); break;
                case GPR(31): MarkForFlush(GPR31, rn->rn_ARM); break;
                case CRn: MarkForFlush(REG_CR, rn->rn_ARM); break;
                case XERn: MarkForFlush(REG_XER, rn->rn_ARM); break;
                case FPSCRn: tc->EMIT(mov_reg_to_simd(REG_FPSCR, rn->rn_ARM)); break;
                default: kprintf("[PPC] Illegal reg %d in IntMapGpr()\n", rn->rn_ARM);
            }
        }
    }

    PurgeFlushStore(tc);
}

#define RTSTACK_SIZE    32
static uint32_t *ReturnStack[RTSTACK_SIZE];
static uint32_t ReturnStackDepth = 0;

static __used__ void PushReturnAddress(uint32_t *ret_addr)
{
    if (ReturnStackDepth >= RTSTACK_SIZE) {
        for (int i=1; i < RTSTACK_SIZE; i++) {
            ReturnStack[i-1] = ReturnStack[i];
        }
        ReturnStackDepth--;
    }

    ReturnStack[ReturnStackDepth++] = ret_addr;
}

static __used__ uint32_t *PopReturnAddress(uint8_t *success)
{
    uint32_t *ptr;

    if (EMU68_USE_RETURN_STACK && ReturnStackDepth > 0)
    {
        ptr = ReturnStack[--ReturnStackDepth];

        if (success)
            *success = 1;
    }
    else
    {
        ptr = (uint32_t *)0xffffffff;
        if (success)
            *success = 0;
    }

    return ptr;
}

static __used__ void ResetReturnStack()
{
    ReturnStackDepth = 0;
}



void EMIT_set_crn_logic(struct PPCTranslatorContext *tc, uint8_t cr)
{
    uint8_t reg_cr = MapGPRForReadAndWrite(tc, CRn);
    uint8_t tmp = AllocARMRegister(tc);

#if PPC_SO_PROPAGATION
    uint8_t reg_xer;

    /* Shift right XER by 31 so that SO is bit 0 */
    if ((reg_xer = IsGPRMapped(tc, XERn)) != 0xff)
    {
        /* XER was already in one of GPRs, rotate with tmp as target */
        tc->EMIT(lsr(tmp, reg_xer, 31));
    }
    else
    {
        /* XER was not mapped, get a copy from PPC context (SIMD register) and rotate */
        tc->EMIT({
            mov_simd_to_reg(tmp, REG_XER),
            lsr(tmp, tmp, 31)
        });
    }
#endif

    uint8_t reg_zero_case = AllocARMRegister(tc);
    uint8_t reg_minus_case = AllocARMRegister(tc);

    tc->EMIT({
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

    FreeARMRegister(tc, reg_zero_case);
    FreeARMRegister(tc, reg_minus_case);
    FreeARMRegister(tc, tmp);
}

void EMIT_set_crn_logic_no_minus(struct PPCTranslatorContext *tc, uint8_t cr)
{
    uint8_t reg_cr = MapGPRForReadAndWrite(tc, CRn);
    uint8_t tmp = AllocARMRegister(tc);

#if PPC_SO_PROPAGATION
    uint8_t reg_xer;

    /* Shift right XER by 31 so that SO is bit 0 */
    if ((reg_xer = IsGPRMapped(tc, XERn)) != 0xff)
    {
        /* XER was already in one of GPRs, rotate with tmp as target */
        tc->EMIT(lsr(tmp, reg_xer, 31));
    }
    else
    {
        /* XER was not mapped, get a copy from PPC context (SIMD register) and rotate */
        tc->EMIT({
            mov_simd_to_reg(tmp, REG_XER),
            lsr(tmp, tmp, 31)
        });
    }
#endif

    uint8_t reg_zero_case = AllocARMRegister(tc);

    tc->EMIT({
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

    FreeARMRegister(tc, reg_zero_case);
    FreeARMRegister(tc, tmp);
}

void EMIT_set_crn_unsigned(struct PPCTranslatorContext *tc, uint8_t cr)
{
    uint8_t reg_cr = MapGPRForReadAndWrite(tc, CRn);
    uint8_t tmp = AllocARMRegister(tc);

#if PPC_SO_PROPAGATION
    uint8_t reg_xer;

    /* Shift right XER by 31 so that SO is bit 0 */
    if ((reg_xer = IsGPRMapped(tc, XERn)) != 0xff)
    {
        /* XER was already in one of GPRs, rotate with tmp as target */
        tc->EMIT(lsr(tmp, reg_xer, 31));
    }
    else
    {
        /* XER was not mapped, get a copy from PPC context (SIMD register) and rotate */
        tc->EMIT({
            mov_simd_to_reg(tmp, REG_XER),
            lsr(tmp, tmp, 31)
        });
    }
#endif

    uint8_t reg_zero_case = AllocARMRegister(tc);
    uint8_t reg_minus_case = AllocARMRegister(tc);

    tc->EMIT({ 
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

    FreeARMRegister(tc, reg_zero_case);
    FreeARMRegister(tc, reg_minus_case);
    FreeARMRegister(tc, tmp);
}

void EMIT_set_crn_signed(struct PPCTranslatorContext *tc, uint8_t cr)
{
    uint8_t reg_cr = MapGPRForReadAndWrite(tc, CRn);
    uint8_t tmp = AllocARMRegister(tc);

#if PPC_SO_PROPAGATION
    uint8_t reg_xer = MapGPRForRead(tc, XERn);

    /* Shift right XER by 31 so that SO is bit 0 */
    if (reg_xer != 0xff)
    {
        /* XER was already in one of GPRs, rotate with tmp as target */
        tc->EMIT(lsr(tmp, reg_xer, 31));
    }
    else
    {
        /* XER was not mapped, get a copy from PPC context (SIMD register) and rotate */
        tc->EMIT({
            mov_simd_to_reg(tmp, REG_XER),
            lsr(tmp, tmp, 31)
        });
    }
#endif

    uint8_t reg_zero_case = AllocARMRegister(tc);
    uint8_t reg_minus_case = AllocARMRegister(tc);

    tc->EMIT({
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

    FreeARMRegister(tc, reg_zero_case);
    FreeARMRegister(tc, reg_minus_case);
    FreeARMRegister(tc, tmp);
}

static __used__ int EMIT_addi(struct PPCTranslatorContext *tc, uint32_t opcode)
{
    uint8_t rd = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint16_t imm = opcode & 0xffff;
    int32_t simm = (int16_t)imm;

    /* If rA == 0 then this is just load immediate */
    if (ra == 0) {
        uint8_t reg_rd = MapGPRForWrite(tc, rd);
        tc->LoadImmediate(reg_rd, (uint32_t)simm);
    } else {
        uint8_t arm_rd, arm_ra;

        /* Source register and target are the same */
        if (ra == rd) {
            arm_rd = MapGPRForReadAndWrite(tc, rd);
            arm_ra = arm_rd;
        } else {
            arm_ra = MapGPRForRead(tc, ra);
            arm_rd = MapGPRForWrite(tc, rd);
        }

        /* If negative, we handle subtraction */
        if (simm < 0) {
            simm = -simm;
            if ((simm & 0xfffff000) == 0) {
                tc->EMIT( sub_immed(arm_rd, arm_ra, simm & 0xfff));
            } else if ((simm & 0xffff0fff) == 0) {
                tc->EMIT( sub_immed_lsl12(arm_rd, arm_ra, (simm >> 12) & 0xfff));
            } else {
                uint8_t tmp = AllocARMRegister(tc);
                tc->LoadImmediate(tmp, simm);
                tc->EMIT(sub_reg(arm_rd, arm_ra, tmp, LSL, 0));
                FreeARMRegister(tc, tmp);
            }
        } else {
            if ((simm & 0xfffff000) == 0) {
                tc->EMIT( add_immed(arm_rd, arm_ra, simm & 0xfff));
            } else if ((simm & 0xffff0fff) == 0) {
                tc->EMIT( add_immed_lsl12(arm_rd, arm_ra, (simm >> 12) & 0xfff));
            } else {
                uint8_t tmp = AllocARMRegister(tc);
                tc->LoadImmediate(tmp, simm);
                tc->EMIT( add_reg(arm_rd, arm_ra, tmp, LSL, 0));
                FreeARMRegister(tc, tmp);
            }
        }
    }

    tc->tc_PPCCodePtr++;
    tc->AdvancePC(4);
    return 1;
}

static __used__ int EMIT_addis(struct PPCTranslatorContext *tc, uint32_t opcode)
{
    uint8_t rd = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint16_t imm = opcode & 0xffff;

    /* If rA == 0 then this is load upper */
    if (ra == 0) {
        uint8_t arm_rd = MapGPRForWrite(tc, rd);
        
        /* If next opcode is ori or addi to the same register, then it is a load immediate */
        uint32_t opcode2 = cache_read_32(ICACHE, (uint32_t)(uintptr_t)(tc->tc_PPCCodePtr + 1));
        uint32_t expected = (rd << 21) | (rd << 16);
        /* Check if ori or addi */
        if ((opcode2 & 0xffff0000) == (expected | (24 << 26)) ||     // This is ori
            (opcode2 & 0xffff8000) == (expected | (14 << 26)))       // This is addi, must be positive, thus test bit 15 too
        {
            tc->LoadImmediate(arm_rd, (opcode << 16) | (opcode2 & 0xffff));
            tc->tc_PPCCodePtr+=2;
            tc->AdvancePC(8);
            return 2;
        }
        else
        {
            /* Regular case, just a load immediate shifted */
            tc->EMIT(mov_immed_u16(arm_rd, imm, 1));
        }
    } else {
        uint8_t arm_rd, arm_ra;

        /* Source register and target are the same */
        if (ra == rd) {
            arm_rd = MapGPRForReadAndWrite(tc, rd);
            arm_ra = arm_rd;
        } else {
            arm_ra = MapGPRForRead(tc, ra);
            arm_rd = MapGPRForWrite(tc, rd);
        }

        if (imm & 0xff00) {
            uint8_t tmp = AllocARMRegister(tc);
            tc->EMIT({ 
                mov_immed_u16(tmp, imm, 1),
                add_reg(arm_rd, arm_ra, tmp, LSL, 0)
            });
            FreeARMRegister(tc, tmp);
        }
        else {
            tc->EMIT( add_immed_lsl12(arm_rd, arm_ra, (imm & 0xff) << 4));
        }
    }

    tc->tc_PPCCodePtr++;
    tc->AdvancePC(4);
    return 1;
}

static __used__ int EMIT_cmpi(struct PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 0x00600000) return -1;

    uint8_t cr = (opcode >> 23) & 7;
    uint8_t ra = (opcode >> 16) & 31;
    uint16_t imm = opcode & 0xffff;
    int32_t simm = (int16_t)imm;

    /* Force-load XER and CR */
#if PPC_SO_PROPAGATION
    MapGPRForRead(tc, XERn);
#endif
    MapGPRForReadAndWrite(tc, CRn);

    uint8_t reg_ra = MapGPRForRead(tc, ra);

    /* Is the immediate in range for CMP? */
    if ((simm & 0xfffff000) == 0) {
        tc->EMIT( cmp_immed(reg_ra, simm));
    }
    else if ((simm & 0xff000fff) == 0) {
        tc->EMIT( cmp_immed_lsl12(reg_ra, simm >> 12));
    }
    else {
        uint8_t tmp = AllocARMRegister(tc);

        tc->LoadImmediate(tmp, simm);
        tc->EMIT( cmp_reg(reg_ra, tmp, LSL, 0));

        FreeARMRegister(tc, tmp);
    }

    EMIT_set_crn_signed(tc, cr);

    tc->tc_PPCCodePtr++;
    tc->AdvancePC(4);
    return 1;
}

static __used__ int EMIT_cmpli(struct PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 0x00600000) return -1;

    uint8_t cr = (opcode >> 23) & 7;
    uint8_t ra = (opcode >> 16) & 31;
    uint32_t imm = opcode & 0xffff;

    /* Force-load XER and CR */
#if PPC_SO_PROPAGATION
    MapGPRForRead(tc, XERn);
#endif
    MapGPRForReadAndWrite(tc, CRn);

    uint8_t reg_ra = MapGPRForRead(tc, ra);

    /* Is the immediate in range for CMP? */
    if ((imm & 0xfffff000) == 0) {
        tc->EMIT( cmp_immed(reg_ra, imm));
    }
    else if ((imm & 0xff000fff) == 0) {
        tc->EMIT( cmp_immed_lsl12(reg_ra, imm >> 12));
    }
    else {
        uint8_t tmp = AllocARMRegister(tc);

        tc->LoadImmediate(tmp, imm);
        tc->EMIT( cmp_reg(reg_ra, tmp, LSL, 0));

        FreeARMRegister(tc, tmp);
    }

    EMIT_set_crn_unsigned(tc, cr);

    tc->tc_PPCCodePtr++;
    tc->AdvancePC(4);
    return 1;
}

static __used__ int EMIT_bx(struct PPCTranslatorContext *tc, uint32_t opcode)
{
    int32_t offset = opcode & 0x03fffffc;
    int update_lr = !!(opcode & 1);
    int is_absolute = !!(opcode & 2);
    int8_t pc_offset = 4;
    struct PPCState *ctx = getHostCTX();
    uint32_t *old_pc = tc->tc_PPCCodePtr;
    int32_t var_EMU68_BRANCH_INLINE_DISTANCE = (ctx->JIT_CONTROL >> JCCB_INLINE_RANGE) & JCCB_INLINE_RANGE_MASK;

    tc->GetOffsetPC(&pc_offset);

    if (offset & 0x02000000) offset |= 0xfc000000;

    if (update_lr) {
        PushReturnAddress(tc->tc_PPCCodePtr + 1);

        if (pc_offset >= 0) {
            tc->EMIT( add_immed(REG_LR, REG_PC, pc_offset));
        } else {
            tc->EMIT( sub_immed(REG_LR, REG_PC, -pc_offset));
        }
    }

    tc->ResetOffsetPC();

    if (is_absolute) {
        tc->tc_PPCCodePtr = (uint32_t*)(uintptr_t)(uint32_t)offset;
        tc->LoadImmediate(REG_PC, (uint32_t)offset);
    } else {
        tc->tc_PPCCodePtr += (offset >> 2);
        int32_t pc_adj = pc_offset + offset - 4;
        if (pc_adj < 0) {
            pc_adj = -pc_adj;
            if ((pc_adj & 0xfffff000) == 0) {
                tc->EMIT( sub_immed(REG_PC, REG_PC, pc_adj));
            } else if ((pc_adj & 0xff000fff) == 0) {
                tc->EMIT( sub_immed_lsl12(REG_PC, REG_PC, pc_adj >> 12));
            } else {
                uint8_t tmp = AllocARMRegister(tc);
                tc->LoadImmediate(tmp, (uint32_t)pc_adj);
                tc->EMIT( sub_reg(REG_PC, REG_PC, tmp, LSL, 0));
                FreeARMRegister(tc, tmp);
            }
        } else if (pc_adj > 0) {
            if ((pc_adj & 0xfffff000) == 0) {
                tc->EMIT( add_immed(REG_PC, REG_PC, pc_adj));
            } else if ((pc_adj & 0xff000fff) == 0) {
                tc->EMIT( add_immed_lsl12(REG_PC, REG_PC, pc_adj >> 12));
            } else {
                uint8_t tmp = AllocARMRegister(tc);
                tc->LoadImmediate(tmp, (uint32_t)pc_adj);
                tc->EMIT( add_reg(REG_PC, REG_PC, tmp, LSL, 0));
                FreeARMRegister(tc, tmp);
            }
        } 
    }

    /* If jump distance is larger than allowed, break translation */
    ptrdiff_t distance = 4 * (tc->tc_PPCCodePtr - old_pc);
    if (distance > var_EMU68_BRANCH_INLINE_DISTANCE || distance < -var_EMU68_BRANCH_INLINE_DISTANCE) {
        tc->STOP();
    }

    /* If jump to itself, insert NOP */
    if (distance == 0) tc->EMIT({ nop(), INSN_TO_LE(MARKER_BREAK) });

    return 1;
}

static __used__ int EMIT_bcx(struct PPCTranslatorContext *tc, uint32_t opcode)
{
    int32_t offset = opcode & 0x0000fffc;
    uint32_t branch_target;
    int is_absolute = !!(opcode & 2);
    int8_t pc_offset = 4;
    struct PPCState *ctx = getHostCTX();
    uint32_t *old_pc = tc->tc_PPCCodePtr;
    int32_t var_EMU68_BRANCH_INLINE_DISTANCE = (ctx->JIT_CONTROL >> JCCB_INLINE_RANGE) & JCCB_INLINE_RANGE_MASK;
    uint8_t tmp = AllocARMRegister(tc);

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

    tc->GetOffsetPC(&pc_offset);

//    kprintf("pc_offset = %d\n", pc_offset);

    if (update_lr) {
        PushReturnAddress(tc->tc_PPCCodePtr + 1);

        if (pc_offset >= 0) {
            tc->EMIT( add_immed(REG_LR, REG_PC, pc_offset));
        } else {
            tc->EMIT( sub_immed(REG_LR, REG_PC, -pc_offset));
        }
    }

    tc->ResetOffsetPC();

    /* Branch always! */
    if (bo0 && bo2) {
        if (is_absolute) {
            tc->tc_PPCCodePtr = (uint32_t*)(uintptr_t)(uint32_t)offset;
            tc->LoadImmediate(REG_PC, (uint32_t)offset);
        } else {
            tc->tc_PPCCodePtr += (offset >> 2);
            int32_t pc_adj = pc_offset + offset - 4;
            AddImmediate(tc, REG_PC, pc_adj);
        }
    } else {
        uint8_t success_condition;
        bool use_tbz = false;

        /* BO[2] == 0 - decrement CTR and set condition */
        if (dec_ctr) {
            tc->EMIT( subs_immed(REG_CTR, REG_CTR, 1));
            /* bo3 == 1 <- take branch if CTR == 0; bo3 == 0 <- take branch if CTR != 0 */
            if (bo3) {
                success_condition = A64_CC_EQ;
                if (bo0 == 0) tc->EMIT( cset(tmp, A64_CC_EQ));
            } else {
                success_condition = A64_CC_NE;
                if (bo0 == 0) tc->EMIT( cset(tmp, A64_CC_NE));
            }
            /* if bo0 == 1 there is no need to test condition flags */
            if (bo0 == 0) {
                uint8_t reg_cr = MapGPRForRead(tc, CRn);
                tc->EMIT({ 
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
            //tc->EMIT( tst_immed(reg_cr, 1, (1 + bi) & 31));
            success_condition = condition_true ? A64_CC_NE : A64_CC_EQ;
            use_tbz = true;
        }

        /* If branch is taken by default, invert success condition, since it will jump to local exit point */
        if (take_branch)
        {
            success_condition ^= 1;
            tc->tc_PPCCodePtr = (uint32_t *)(uintptr_t)branch_target;
        }
        else
        {
            tc->tc_PPCCodePtr++;
        }

        /* Emit jump, remember its location and fixup type */
        uint32_t fixup_type = use_tbz ? FIXUP_TBZ : FIXUP_BCC;
        
        if (use_tbz) {
            uint8_t reg_cr = MapGPRForRead(tc, CRn);
            
            tc->EMIT(
                success_condition == A64_CC_EQ ?
                    tbz(reg_cr, 31 - bi, 0):
                    tbnz(reg_cr, 31 - bi, 0)
            );
        } else { 
            tc->EMIT( b_cc(success_condition, 0));
        }

        uint32_t *jump_location = tc->tc_CodePtr - 1;

        /* Here the expected code path follows */
        if (take_branch)
        {
            if (is_absolute) {
                tc->LoadImmediate(REG_PC, (uint32_t)offset);
            } else {
                int32_t pc_adj = pc_offset + offset - 4;
                AddImmediate(tc, REG_PC, pc_adj);
            }
        }
        else
        {
            int32_t pc_adj = pc_offset;
            AddImmediate(tc, REG_PC, pc_adj);
        }

        /* Now insert the other code path - this will be treated as exit code */
        uint32_t *exit_code_start = tc->tc_CodePtr;

        if (!take_branch)
        {
            if (is_absolute) {
                tc->LoadImmediate(REG_PC, (uint32_t)offset);
            } else {
                int32_t pc_adj = pc_offset + offset - 4;
                AddImmediate(tc, REG_PC, pc_adj);
            }
        }
        else
        {
            int32_t pc_adj = pc_offset;
            AddImmediate(tc, REG_PC, pc_adj);
        }

        /* Insert local exit */
        tc->LocalExit(1);
        uint32_t *exit_code_end = tc->tc_CodePtr;

        /* Insert fixup location */
        tc->EMIT({ 
            (uint32_t)(exit_code_end - jump_location),
            fixup_type,
            1,
            (uint32_t)(exit_code_end - exit_code_start),
            INSN_TO_LE(MARKER_EXIT_BLOCK)
        });
    }

    /* If jump distance is larger than allowed, break translation */
    ptrdiff_t distance = 4 * (tc->tc_PPCCodePtr - old_pc);
    if (distance > var_EMU68_BRANCH_INLINE_DISTANCE || distance < -var_EMU68_BRANCH_INLINE_DISTANCE) {
        tc->STOP();
    }
    
    FreeARMRegister(tc, tmp);
    return 1;
}

static __used__ int EMIT_ori(struct PPCTranslatorContext *tc, uint32_t opcode)
{
    uint32_t immed = opcode & 0xffff;
    uint32_t mask = number_to_mask(immed);
    uint8_t rs = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    
    uint8_t reg_rS = MapGPRForRead(tc, rs);
    uint8_t reg_rA = MapGPRForReadAndWrite(tc, ra);

    if (immed == 0) {
        if (rs == ra)
            tc->EMIT( nop());
        else
            tc->EMIT( mov_reg(reg_rA, reg_rS));
    }
    else {
        if (mask == 0) {
            uint8_t tmp = AllocARMRegister(tc);
            tc->EMIT({
                mov_immed_u16(tmp, immed, 0),
                orr_reg(reg_rA, reg_rS, tmp, LSL, 0)
            });
            FreeARMRegister(tc, tmp);
        } else {
            tc->EMIT( orr_immed(reg_rA, reg_rS, (mask >> 16) & 0x3f, mask & 0x3f));
        }
    }

    tc->tc_PPCCodePtr++;
    tc->AdvancePC(4);
    return 1;
}

static __used__ int EMIT_oris(struct PPCTranslatorContext *tc, uint32_t opcode)
{
    uint32_t immed = opcode & 0xffff;
    uint32_t mask = number_to_mask(immed << 16);
    uint8_t rs = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    
    uint8_t reg_rS = MapGPRForRead(tc, rs);
    uint8_t reg_rA = MapGPRForReadAndWrite(tc, ra);

    if (mask == 0) {
        uint8_t tmp = AllocARMRegister(tc);
        tc->EMIT({
            mov_immed_u16(tmp, immed, 1),
            orr_reg(reg_rA, reg_rS, tmp, LSL, 0)
        });
        FreeARMRegister(tc, tmp);
    } else {
        tc->EMIT( orr_immed(reg_rA, reg_rS, (mask >> 16) & 0x3f, mask & 0x3f));
    }

    tc->tc_PPCCodePtr++;
    tc->AdvancePC(4);
    return 1;
}

static __used__ int EMIT_andi_dot(struct PPCTranslatorContext *tc, uint32_t opcode)
{
    uint32_t immed = opcode & 0xffff;
    uint32_t mask = number_to_mask(immed);
    uint8_t rs = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    
    uint8_t reg_rS = MapGPRForRead(tc, rs);
    uint8_t reg_rA = MapGPRForReadAndWrite(tc, ra);

    if (mask == 0) {
        uint8_t tmp = AllocARMRegister(tc);
        tc->EMIT({
            mov_immed_u16(tmp, immed, 0),
            ands_reg(reg_rA, reg_rS, tmp, LSL, 0)
        });
        FreeARMRegister(tc, tmp);
    } else {
        tc->EMIT( ands_immed(reg_rA, reg_rS, (mask >> 16) & 0x3f, mask & 0x3f));
    }

    EMIT_set_crn_logic_no_minus(tc, 0);

    tc->tc_PPCCodePtr++;
    tc->AdvancePC(4);
    return 1;
}


static __used__ int EMIT_andis_dot(struct PPCTranslatorContext *tc, uint32_t opcode)
{
    uint32_t immed = opcode & 0xffff;
    uint32_t mask = number_to_mask(immed << 16);
    uint8_t rs = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    
    uint8_t reg_rS = MapGPRForRead(tc, rs);
    uint8_t reg_rA = MapGPRForReadAndWrite(tc, ra);

    if (mask == 0) {
        uint8_t tmp = AllocARMRegister(tc);
        tc->EMIT({
            mov_immed_u16(tmp, immed, 1),
            ands_reg(reg_rA, reg_rS, tmp, LSL, 0)
        });
        FreeARMRegister(tc, tmp);
    } else {
        tc->EMIT( ands_immed(reg_rA, reg_rS, (mask >> 16) & 0x3f, mask & 0x3f));
    }

    if (immed & 0x8000)
        EMIT_set_crn_logic(tc, 0);
    else
        EMIT_set_crn_logic_no_minus(tc, 0);

    tc->tc_PPCCodePtr++;
    tc->AdvancePC(4);
    return 1;
}

static __used__ int EMIT_xori(struct PPCTranslatorContext *tc, uint32_t opcode)
{
    uint32_t immed = opcode & 0xffff;
    uint32_t mask = number_to_mask(immed);
    uint8_t rs = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    
    uint8_t reg_rS = MapGPRForRead(tc, rs);
    uint8_t reg_rA = MapGPRForReadAndWrite(tc, ra);

    if (mask == 0) {
        uint8_t tmp = AllocARMRegister(tc);
        tc->EMIT({
            mov_immed_u16(tmp, immed, 0),
            eor_reg(reg_rA, reg_rS, tmp, LSL, 0)
        });
        FreeARMRegister(tc, tmp);
    } else {
        tc->EMIT( eor_immed(reg_rA, reg_rS, (mask >> 16) & 0x3f, mask & 0x3f));
    }

    tc->tc_PPCCodePtr++;
    tc->AdvancePC(4);
    return 1;
}

static __used__ int EMIT_xoris(struct PPCTranslatorContext *tc, uint32_t opcode)
{
    uint32_t immed = opcode & 0xffff;
    uint32_t mask = number_to_mask(immed << 16);
    uint8_t rs = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    
    uint8_t reg_rS = MapGPRForRead(tc, rs);
    uint8_t reg_rA = MapGPRForReadAndWrite(tc, ra);

    if (mask == 0) {
        uint8_t tmp = AllocARMRegister(tc);
        tc->EMIT({
            mov_immed_u16(tmp, immed, 1),
            eor_reg(reg_rA, reg_rS, tmp, LSL, 0)
        });
        FreeARMRegister(tc, tmp);
    } else {
        tc->EMIT( eor_immed(reg_rA, reg_rS, (mask >> 16) & 0x3f, mask & 0x3f));
    }

    tc->tc_PPCCodePtr++;
    tc->AdvancePC(4);
    return 1;
}

static __used__ int EMIT_isync(struct PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 0x03fff801) return -1;

    tc->EMIT({ 
        isb(),
        INSN_TO_LE(MARKER_STOP)
    });

    tc->tc_PPCCodePtr++;
    tc->AdvancePC(4);
    return 1;
}

static __used__ int EMIT_mcrf(struct PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 0x63F801) return -1;

    int dst = (opcode >> 23) & 7;
    int src = (opcode >> 18) & 7;

    uint8_t tmp = AllocARMRegister(tc);
    uint8_t cr = MapGPRForReadAndWrite(tc, CRn);

    tc->EMIT({
        lsr(tmp, cr, (7 - src) * 4),
        bfi(cr, tmp, (7 - dst) * 4, 4)
    });

    FreeARMRegister(tc, tmp);
    
    tc->tc_PPCCodePtr++;
    tc->AdvancePC(4);
    return 1;
}

static __used__ int EMIT_crandc(struct PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 1) return -1;

    uint8_t crd = (opcode >> 21) & 31;
    uint8_t cra = (opcode >> 16) & 31;
    uint8_t crb = (opcode >> 11) & 31;

    uint8_t cr_reg = MapGPRForReadAndWrite(tc, CRn);
    uint8_t tmp = AllocARMRegister(tc);

    tc->EMIT({
        neg_reg(tmp, cr_reg, LSR, (31 - crb)),
        and_reg(tmp, tmp, cr_reg, LSR, (31 - cra)),
        bfi(cr_reg, tmp, 31 - crd, 1)
    });

    FreeARMRegister(tc, tmp);
    
    tc->tc_PPCCodePtr++;
    tc->AdvancePC(4);
    return 1;
}

static __used__ int EMIT_creqv(struct PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 1) return -1;

    uint8_t crd = (opcode >> 21) & 31;
    uint8_t cra = (opcode >> 16) & 31;
    uint8_t crb = (opcode >> 11) & 31;

    uint8_t cr_reg = MapGPRForReadAndWrite(tc, CRn);
    uint8_t tmp = AllocARMRegister(tc);

    if (cra == crb) {
        /* Bit set */
        tc->EMIT(
            orr_immed(cr_reg, cr_reg, 1, (crd + 1) & 31)
        );
    }
    else {
        tc->EMIT({
            lsr(tmp, cr_reg, (31 - cra)),
            eon_reg(tmp, tmp, cr_reg, LSR, (31 - crb)),
            bfi(cr_reg, tmp, 31 - crd, 1)
        });
    }

    FreeARMRegister(tc, tmp);
    
    tc->tc_PPCCodePtr++;
    tc->AdvancePC(4);
    return 1;
}

static __used__ int EMIT_crand(struct PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 1) return -1;

    uint8_t crd = (opcode >> 21) & 31;
    uint8_t cra = (opcode >> 16) & 31;
    uint8_t crb = (opcode >> 11) & 31;

    uint8_t cr_reg = MapGPRForReadAndWrite(tc, CRn);
    uint8_t tmp = AllocARMRegister(tc);

    if (cra == crb) {
        /* Bit move */
        tc->EMIT({
            lsr(tmp, cr_reg, (31 - cra)),
            bfi(cr_reg, tmp, 31 - crd, 1)
        });
    } else {
        tc->EMIT({
            lsr(tmp, cr_reg, (31 - cra)),
            and_reg(tmp, tmp, cr_reg, LSR, (31 - crb)),
            bfi(cr_reg, tmp, 31 - crd, 1)
        });
    }

    FreeARMRegister(tc, tmp);
    
    tc->tc_PPCCodePtr++;
    tc->AdvancePC(4);
    return 1;
}

static __used__ int EMIT_crnand(struct PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 1) return -1;

    uint8_t crd = (opcode >> 21) & 31;
    uint8_t cra = (opcode >> 16) & 31;
    uint8_t crb = (opcode >> 11) & 31;

    uint8_t cr_reg = MapGPRForReadAndWrite(tc, CRn);
    uint8_t tmp = AllocARMRegister(tc);

    tc->EMIT({
        lsr(tmp, cr_reg, (31 - cra)),
        and_reg(tmp, tmp, cr_reg, LSR, (31 - crb)),
        neg_reg(tmp, tmp, LSL, 0),
        bfi(cr_reg, tmp, 31 - crd, 1)
    });

    FreeARMRegister(tc, tmp);
    
    tc->tc_PPCCodePtr++;
    tc->AdvancePC(4);
    return 1;
}

static __used__ int EMIT_crnor(struct PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 1) return -1;

    uint8_t crd = (opcode >> 21) & 31;
    uint8_t cra = (opcode >> 16) & 31;
    uint8_t crb = (opcode >> 11) & 31;

    uint8_t cr_reg = MapGPRForReadAndWrite(tc, CRn);
    uint8_t tmp = AllocARMRegister(tc);

    if (cra == crb) {
        /* Bit change */
        tc->EMIT(
            eor_immed(cr_reg, cr_reg, 1, (crd + 1) & 31)
        );
    } else {
        tc->EMIT({
            lsr(tmp, cr_reg, (31 - cra)),
            orr_reg(tmp, tmp, cr_reg, LSR, (31 - crb)),
            neg_reg(tmp, tmp, LSL, 0),
            bfi(cr_reg, tmp, 31 - crd, 1)
        });
    }

    FreeARMRegister(tc, tmp);
    
    tc->tc_PPCCodePtr++;
    tc->AdvancePC(4);
    return 1;
}

static __used__ int EMIT_cror(struct PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 1) return -1;

    uint8_t crd = (opcode >> 21) & 31;
    uint8_t cra = (opcode >> 16) & 31;
    uint8_t crb = (opcode >> 11) & 31;

    uint8_t cr_reg = MapGPRForReadAndWrite(tc, CRn);
    uint8_t tmp = AllocARMRegister(tc);

    if (cra == crb) {
        /* Bit move */
        tc->EMIT({
            lsr(tmp, cr_reg, (31 - cra)),
            bfi(cr_reg, tmp, 31 - crd, 1)
        });
    } else {
        tc->EMIT({
            lsr(tmp, cr_reg, (31 - cra)),
            orr_reg(tmp, tmp, cr_reg, LSR, (31 - crb)),
            bfi(cr_reg, tmp, 31 - crd, 1)
        });
    }

    FreeARMRegister(tc, tmp);
    
    tc->tc_PPCCodePtr++;
    tc->AdvancePC(4);
    return 1;
}

static __used__ int EMIT_crorc(struct PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 1) return -1;

    uint8_t crd = (opcode >> 21) & 31;
    uint8_t cra = (opcode >> 16) & 31;
    uint8_t crb = (opcode >> 11) & 31;

    uint8_t cr_reg = MapGPRForReadAndWrite(tc, CRn);
    uint8_t tmp = AllocARMRegister(tc);

    if (cra == crb) {
        /* Bit set */
        tc->EMIT(
            orr_immed(cr_reg, cr_reg, 1, (crd + 1) & 31)
        );
    } else {
        tc->EMIT({
            lsr(tmp, cr_reg, (31 - cra)),
            orn_reg(tmp, tmp, cr_reg, LSR, (31 - crb)),
            bfi(cr_reg, tmp, 31 - crd, 1)
        });
    }

    FreeARMRegister(tc, tmp);
    
    tc->tc_PPCCodePtr++;
    tc->AdvancePC(4);
    return 1;
}

static __used__ int EMIT_crxor(struct PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 1) return -1;

    uint8_t crd = (opcode >> 21) & 31;
    uint8_t cra = (opcode >> 16) & 31;
    uint8_t crb = (opcode >> 11) & 31;

    uint8_t cr_reg = MapGPRForReadAndWrite(tc, CRn);

    if (cra == crb) {
        /* Bit clear */
        tc->EMIT(
            bic_immed(cr_reg, cr_reg, 1, (crd + 1) & 31)
        );
    } else {
        uint8_t tmp = AllocARMRegister(tc);
        tc->EMIT({
            lsr(tmp, cr_reg, (31 - cra)),
            orr_reg(tmp, tmp, cr_reg, LSR, (31 - crb)),
            bfi(cr_reg, tmp, 31 - crd, 1)
        });
        FreeARMRegister(tc, tmp);
    }

    tc->tc_PPCCodePtr++;
    tc->AdvancePC(4);
    return 1;
}

static __used__ int EMIT_bclrx(struct PPCTranslatorContext *tc, uint32_t opcode)
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
        uint8_t success = 0;
        uint32_t *last_pc = PopReturnAddress(&success);

        /* if LR needs to be updated, do it now */
        if (update_lr) {
            int8_t pc_offset = 4;
            uint8_t tmp = AllocARMRegister(tc);

            tc->GetOffsetPC(&pc_offset);

            PushReturnAddress(tc->tc_PPCCodePtr + 1);

            tc->EMIT( bic_immed(tmp, REG_LR, 2, 0));

            if (pc_offset >= 0) {
                tc->EMIT( add_immed(REG_LR, REG_PC, pc_offset));
            } else {
                tc->EMIT( sub_immed(REG_LR, REG_PC, -pc_offset));
            }

            tc->EMIT( mov_reg(REG_PC, tmp));

            FreeARMRegister(tc, tmp);
        }
        else
        {
            /* Move LR to PC */
            tc->EMIT( bic_immed(REG_PC, REG_LR, 2, 0));
        }

        if (success) {
            tc->tc_PPCCodePtr = last_pc;
        } else {
            /* The return address stack was not available, stop now */
            tc->STOP();
        }

        tc->ResetOffsetPC();
    } else {
        uint8_t success = 0;
        uint8_t success_condition;
        uint8_t tmp = AllocARMRegister(tc);
        uint32_t *last_pc = PopReturnAddress(&success);
        int8_t pc_offset = 4;
        
        ResetReturnStack();

        tc->GetOffsetPC(&pc_offset);

        /* BO[2] == 0 - decrement CTR and set condition */
        if (dec_ctr) {
            tc->EMIT(subs_immed(REG_CTR, REG_CTR, 1));
            /* bo3 == 1 <- take branch if CTR == 0; bo3 == 0 <- take branch if CTR != 0 */
            if (bo3) {
                success_condition = A64_CC_EQ;
                if (bo0 == 0) tc->EMIT(cset(tmp, A64_CC_EQ));
            } else {
                success_condition = A64_CC_NE;
                if (bo0 == 0) tc->EMIT(cset(tmp, A64_CC_NE));
            }
            /* if bo0 == 1 there is no need to test condition flags */
            if (bo0 == 0) {
                uint8_t reg_cr = MapGPRForRead(tc, CRn);
                tc->EMIT({ 
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
            uint8_t reg_cr = MapGPRForRead(tc, CRn);
            /* Check the condition */
            tc->EMIT(tst_immed(reg_cr, 1, (1 + bi) & 31));
            success_condition = condition_true ? A64_CC_NE : A64_CC_EQ;
        }

        /* If branch is taken by default, invert success condition, since it will jump to local exit point */
        if (take_branch) {
            success_condition ^= 1;
        }

        /* Emit jump, remember its location and fixup type */
        uint32_t fixup_type = FIXUP_BCC;
        uint32_t *jump_location = tc->tc_CodePtr;
        tc->EMIT( b_cc(success_condition, 0));

        /* Here is expected code path */
        if (take_branch)
        {
            /* if LR needs to be updated, do it now */
            if (update_lr) {
                uint8_t tmp = AllocARMRegister(tc);

                PushReturnAddress(tc->tc_PPCCodePtr + 1);

                tc->EMIT( bic_immed(tmp, REG_LR, 2, 0));

                if (pc_offset >= 0) {
                    tc->EMIT( add_immed(REG_LR, REG_PC, pc_offset));
                } else {
                    tc->EMIT( sub_immed(REG_LR, REG_PC, -pc_offset));
                }

                tc->EMIT( mov_reg(REG_PC, tmp));

                FreeARMRegister(tc, tmp);
            }
            else
            {
                /* Move LR to PC */
                tc->EMIT( bic_immed(REG_PC, REG_LR, 2, 0));
            }

            if (success) {
                tc->tc_PPCCodePtr = last_pc;
            } else {
                /* The return address stack was not available, stop now */
                tc->STOP();
            }
        }
        else
        {
            int32_t pc_adj = pc_offset;
            AddImmediate(tc, REG_PC, pc_adj);
            tc->tc_PPCCodePtr++;
        }

        /* Now insert the other code path - this will be treated as exit code */
        uint32_t *exit_code_start = tc->tc_CodePtr;

        if (!take_branch)
        {
            /* if LR needs to be updated, do it now */
            if (update_lr) {
                uint8_t tmp = AllocARMRegister(tc);

                PushReturnAddress(tc->tc_PPCCodePtr + 1);

                tc->EMIT( bic_immed(tmp, REG_LR, 2, 0));

                if (pc_offset >= 0) {
                    tc->EMIT( add_immed(REG_LR, REG_PC, pc_offset));
                } else {
                    tc->EMIT( sub_immed(REG_LR, REG_PC, -pc_offset));
                }

                tc->EMIT( mov_reg(REG_PC, tmp));

                FreeARMRegister(tc, tmp);
            }
            else
            {
                /* Move LR to PC */
                tc->EMIT( bic_immed(REG_PC, REG_LR, 2, 0));
            }
        }
        else
        {
            int32_t pc_adj = pc_offset;
            AddImmediate(tc, REG_PC, pc_adj);
        }

        tc->ResetOffsetPC();

        /* Insert local exit */
        tc->LocalExit(1);
        uint32_t *exit_code_end = tc->tc_CodePtr;

        /* Insert fixup location */
        tc->EMIT({ 
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
                tc->LoadImmediate(REG_PC, (uint32_t)offset);
            } else {
                int32_t pc_adj = pc_offset + offset - 4;
                AddImmediate(tc, REG_PC, pc_adj);
            }
        }
        else
        {
            int32_t pc_adj = pc_offset;
            AddImmediate(tc, REG_PC, pc_adj);
        }

        /* Now insert the other code path - this will be treated as exit code */
        uint32_t *exit_code_start = tc->tc_CodePtr;

        if (!take_branch)
        {
            if (is_absolute) {
                tc->LoadImmediate(REG_PC, (uint32_t)offset);
            } else {
                int32_t pc_adj = pc_offset + offset - 4;
                AddImmediate(tc, REG_PC, pc_adj);
            }
        }
        else
        {
            int32_t pc_adj = pc_offset;
            AddImmediate(tc, REG_PC, pc_adj);
        }

        /* Insert local exit */
        LocalExit(tc, 1);
        uint32_t *exit_code_end = tc->tc_CodePtr;

        /* Insert fixup location */
        tc->EMIT({ 
            (uint32_t)(exit_code_end - jump_location),
            fixup_type,
            1,
            (uint32_t)(exit_code_end - exit_code_start),
            INSN_TO_LE(MARKER_EXIT_BLOCK)
        });
        #endif

        FreeARMRegister(tc, tmp);
        
//        (void)bi;
//        (void)take_branch;
        return 1;
    }

    return 1;
}

static __used__ int EMIT_bcctrx(struct PPCTranslatorContext *tc, uint32_t opcode)
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

            tc->GetOffsetPC(&pc_offset);

            if (pc_offset >= 0) {
                tc->EMIT( add_immed(REG_LR, REG_PC, pc_offset));
            } else {
                tc->EMIT( sub_immed(REG_LR, REG_PC, -pc_offset));
            }

            tc->EMIT( bic_immed(REG_PC, REG_CTR, 2, 0));
        }
        else
        {
            /* Move LR to PC */
            tc->EMIT( bic_immed(REG_PC, REG_CTR, 2, 0));
        }

        /* The return address stack was not available, stop now */
        tc->STOP();

        tc->ResetOffsetPC();
    } else {
        (void)bi;
        (void)take_branch;
        return -1;
    }

    return 1;
}

static __used__ int EMIT_mfcr(struct PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Stanity check */
    if (opcode & 0x001ff801) return -1;

    uint8_t rd = (opcode >> 21) & 31;
    uint8_t reg_rd = MapGPRForWrite(tc, rd);
    uint8_t reg_cr = IsGPRMapped(tc, CRn);

    if (reg_cr != 0xff) {
        tc->EMIT( mov_reg(reg_rd, reg_cr));
    } else {
        tc->EMIT( mov_simd_to_reg(reg_rd, REG_CR));
    }

    tc->tc_PPCCodePtr++;
    tc->AdvancePC(4);
    return 1;
}

static __used__ int EMIT_mftb(struct PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Stanity check */
    if (opcode & 1) return -1;

    uint32_t tbr = (opcode >> 11) & 0x3ff;
    uint8_t rd = (opcode >> 21) & 31;
    /* tbr is a split field, fix it */
    tbr = ((tbr >> 5) & 0x1f) | ((tbr & 0x1f) << 5);

    if (tbr != 268 && tbr != 269) return -1;

    uint8_t reg_rd = MapGPRForWrite(tc, rd);
    uint8_t tmp = AllocARMRegister(tc);

    tc->EMIT(mrs(tmp, sys_CNTPCT_EL0));

    if (tbr == 268) // TBL
        tc->EMIT( mov_reg(reg_rd, tmp));
    else // TBH
        tc->EMIT( lsr64(reg_rd, tmp, 32));

    FreeARMRegister(tc, tmp);

    tc->tc_PPCCodePtr++;
    tc->AdvancePC(4);
    return 1;
}

static __used__ int EMIT_mcrxr(struct PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Stanity check */
    if (opcode & 0x007ff801) return -1;

    uint8_t tmp = AllocARMRegister(tc);
    uint8_t xer_reg = MapGPRForReadAndWrite(tc, XERn);
    uint8_t cr_reg = MapGPRForReadAndWrite(tc, CRn);
    uint8_t crn = (opcode >> 23) & 7;

    tc->EMIT({
        lsr(tmp, xer_reg, 28),
        bfi(cr_reg, tmp, 4 * (7 - crn), 4),
        bic_immed(xer_reg, xer_reg, 4, 4)
    });

    FreeARMRegister(tc, tmp);

    tc->tc_PPCCodePtr++;
    tc->AdvancePC(4);
    return 1;
}

static __used__ int EMIT_mfspr(struct PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Stanity check */
    if (opcode & 1) return -1;

    uint32_t spr = (opcode >> 11) & 0x3ff;
    uint8_t rd = (opcode >> 21) & 31;
    /* tbr is a split field, fix it */
    spr = ((spr >> 5) & 0x1f) | ((spr & 0x1f) << 5);

    uint8_t reg_rd = MapGPRForWrite(tc, rd);

    if (spr & 0x10)
    {
        uint8_t ctx = GetCTX(tc);
        uint8_t tmp = AllocARMRegister(tc);

        /* 
            This fetches must not be context synchronizing and one supervisor check per
            code block is sufficient.
        */
        if (!tc->tc_SupervisorChecked)
        {
            /* We need to flush PC now, just in case */
            tc->FlushPC();

            /* 
                Fetch MSR and check if exceptions are enabled - it is illegal to call
                RFI from user
            */
            tc->EMIT({
                ldr_offset(ctx, tmp, __builtin_offsetof(PPCState, MSR)),
                tbnz(tmp, 14, 0)
            });
        }

        /* Emit jump, remember its location and fixup type */
        uint32_t fixup_type = FIXUP_TBZ;
        uint32_t *jump_location = tc->tc_CodePtr - 1;

        switch(spr) {
            case 18:    /* DSISR */
                tc->EMIT(ldr_offset(ctx, reg_rd, __builtin_offsetof(PPCState, DSISR)));
                break;
            case 19:    /* DAR */
                tc->EMIT(ldr_offset(ctx, reg_rd, __builtin_offsetof(PPCState, DAR)));
                break;
            case 22:    /* DEC */
                tc->EMIT(mrs(reg_rd, sys_CNTP_TVAL_EL0));
                break;
            case 26:    /* SRR0 */
                tc->EMIT(ldr_offset(ctx, reg_rd, __builtin_offsetof(PPCState, SRR0)));
                break;
            case 27:    /* SRR1 */
                tc->EMIT(ldr_offset(ctx, reg_rd, __builtin_offsetof(PPCState, SRR1)));
                break;
            case 272:   /* SPRG0 */
                tc->EMIT(ldr_offset(ctx, reg_rd, __builtin_offsetof(PPCState, SPRG[0])));
                break;
            case 273:   /* SPRG1 */
                tc->EMIT(ldr_offset(ctx, reg_rd, __builtin_offsetof(PPCState, SPRG[1])));
                break;
            case 274:   /* SPRG2 */
                tc->EMIT(ldr_offset(ctx, reg_rd, __builtin_offsetof(PPCState, SPRG[2])));
                break;
            case 275:   /* SPRG3 */
                tc->EMIT(ldr_offset(ctx, reg_rd, __builtin_offsetof(PPCState, SPRG[3])));
                break;
            case 287:   /* PVR */
                tc->EMIT({
                    mov_immed_u16(reg_rd, (EMU68_VERSION_MAJOR << 8) | EMU68_VERSION_MINOR, 0),
                    movk_immed_u16(reg_rd, 0xee68, 1)
                });
                break;
            case 916:   /* JIT_CACHE_TOTAL */
                tc->EMIT(ldr_offset(ctx, reg_rd, __builtin_offsetof(PPCState, JIT_CACHE_TOTAL)));
                break;
            case 917:   /* JIT_CACHE_FREE */
                tc->EMIT(ldr_offset(ctx, reg_rd, __builtin_offsetof(PPCState, JIT_CACHE_FREE)));
                break;
            case 918:   /* JIT_CACHE_MISS */
                tc->EMIT(ldr_offset(ctx, reg_rd, __builtin_offsetof(PPCState, JIT_CACHE_MISS)));
                break;
            case 919:   /* JIT_UNIT_COUNT */
                tc->EMIT(ldr_offset(ctx, reg_rd, __builtin_offsetof(PPCState, JIT_UNIT_COUNT)));
                break;
            case 920:   /* JIT_CONTROL */
                tc->EMIT(ldr_offset(ctx, reg_rd, __builtin_offsetof(PPCState, JIT_CONTROL)));
                break;
            case 921:   /* JIT_CONTROL_2 */
                tc->EMIT(ldr_offset(ctx, reg_rd, __builtin_offsetof(PPCState, JIT_CONTROL2)));
                break;
            case 944:   /* BASE */
                tc->EMIT(ldr_offset(ctx, reg_rd, __builtin_offsetof(PPCState, BASEREG)));
                break;
        }

        FreeARMRegister(tc, tmp);

        if (!tc->tc_SupervisorChecked)
        {
            /* Now insert the program exception path - raise exception if RFI was not allowed */
            uint32_t *exit_code_start = tc->tc_CodePtr;
            tc->EMIT_Exception(0x700);
            uint32_t *exit_code_end = tc->tc_CodePtr;

            /* Insert fixup location */
            tc->EMIT({ 
                (uint32_t)(exit_code_end - jump_location),
                fixup_type,
                1,
                (uint32_t)(exit_code_end - exit_code_start),
                INSN_TO_LE(MARKER_EXIT_BLOCK)
            });

            tc->tc_SupervisorChecked = true;
        }

        tc->AdvancePC(4);
        tc->tc_PPCCodePtr++;
        return 1;
    }
    else
    {
        uint8_t tmp;

        /* Accessing user level SPRs */
        switch(spr) {
            case 1:
                tc->EMIT( mov_reg(reg_rd, MapGPRForRead(tc, XERn)));
                break;
            case 8:
                tc->EMIT( mov_reg(reg_rd, MapGPRForRead(tc, LRn)));
                break;
            case 9:
                tc->EMIT( mov_reg(reg_rd, MapGPRForRead(tc, CTRn)));
                break;
            case 900: /* INSNCNTLO - lower 32 bits of PPC instruction counter */
                tmp = AllocARMRegister(tc);
                tc->EMIT({ 
                    mov_simd_to_reg(tmp, CTX_INSN_COUNT),
                    add64_immed(tmp, tmp, insn_count & 0xfff)
                });
                if (insn_count & 0xfff000)
                    tc->EMIT(add64_immed_lsl12(tmp, tmp, insn_count >> 12));
                tc->EMIT(mov_reg(reg_rd, tmp));
                FreeARMRegister(tc, tmp);
                break;
            case 901: /* INSNCNTHI - higher 32 bits of PPC instruction counter */
                tmp = AllocARMRegister(tc);
                tc->EMIT({
                    mov_simd_to_reg(tmp, CTX_INSN_COUNT),
                    add64_immed(tmp, tmp, insn_count & 0xfff)
                });
                if (insn_count & 0xfff000)
                    tc->EMIT(add64_immed_lsl12(tmp, tmp, insn_count >> 12));
                tc->EMIT(lsr64(reg_rd, tmp, 32));
                FreeARMRegister(tc, tmp);
                break;
            case 902: /* ARMCNTLO - lower 32 bits of ARM instruction counter */
                tmp = AllocARMRegister(tc);
                tc->EMIT({
                    mrs(tmp, sys_PMCCNTR_EL0),
                    mov_reg(reg_rd, tmp)
                });
                FreeARMRegister(tc, tmp);
                break;
            case 903: /* ARMCNTHI - higher 32 bits of ARM instruction counter */
                tmp = AllocARMRegister(tc);
                tc->EMIT({
                    mrs(tmp, sys_PMCCNTR_EL0),
                    lsr64(reg_rd, tmp, 32)
                });
                FreeARMRegister(tc, tmp);
                break;

            default:
                return -1;
        }
    }

    tc->tc_PPCCodePtr++;
    tc->AdvancePC(4);
    return 1;
}

static __used__ int EMIT_mfmsr(struct PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Stanity check */
    if (opcode & 0x001ff801) return -1;

    uint8_t rd = (opcode >> 21) & 31;
    uint8_t reg_rd = MapGPRForWrite(tc, rd);
    uint8_t ctx = GetCTX(tc);
    uint8_t tmp = AllocARMRegister(tc);

    /* 
        This fetches must not be context synchronizing and one supervisor check per
        code block is sufficient.
    */
    if (!tc->tc_SupervisorChecked)
    {
        /* We need to flush PC now, just in case */
        tc->FlushPC();

        /* 
            Fetch MSR and check if exceptions are enabled - it is illegal to call
            RFI from user
        */
        tc->EMIT({
            ldr_offset(ctx, tmp, __builtin_offsetof(PPCState, MSR)),
            tbnz(tmp, 14, 0)
        });
    }

    /* Emit jump, remember its location and fixup type */
    uint32_t fixup_type = FIXUP_TBZ;
    uint32_t *jump_location = tc->tc_CodePtr - 1;

    tc->EMIT(ldr_offset(ctx, reg_rd, __builtin_offsetof(PPCState, MSR)));

    FreeARMRegister(tc, tmp);

    if (!tc->tc_SupervisorChecked)
    {
        /* Now insert the program exception path - raise exception if RFI was not allowed */
        uint32_t *exit_code_start = tc->tc_CodePtr;
        tc->EMIT_Exception(0x700);
        uint32_t *exit_code_end = tc->tc_CodePtr;

        /* Insert fixup location */
        tc->EMIT({ 
            (uint32_t)(exit_code_end - jump_location),
            fixup_type,
            1,
            (uint32_t)(exit_code_end - exit_code_start),
            INSN_TO_LE(MARKER_EXIT_BLOCK)
        });

        tc->tc_SupervisorChecked = true;
    }

    tc->AdvancePC(4);
    tc->tc_PPCCodePtr++;
    return 1;
}

static __used__ int EMIT_mtmsr(struct PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Stanity check */
    if (opcode & 0x001ff801) return -1;

    uint8_t rs = (opcode >> 21) & 31;
    uint8_t reg_rs = MapGPRForRead(tc, rs);
    uint8_t ctx = GetCTX(tc);
    uint8_t tmp = AllocARMRegister(tc);

    /* 
        This fetches must not be context synchronizing and one supervisor check per
        code block is sufficient.
    */
    if (!tc->tc_SupervisorChecked)
    {
        /* We need to flush PC now, just in case */
        tc->FlushPC();

        /* 
            Fetch MSR and check if exceptions are enabled - it is illegal to call
            RFI from user
        */
        tc->EMIT({
            ldr_offset(ctx, tmp, __builtin_offsetof(PPCState, MSR)),
            tbnz(tmp, 14, 0)
        });
    }

    /* Emit jump, remember its location and fixup type */
    uint32_t fixup_type = FIXUP_TBZ;
    uint32_t *jump_location = tc->tc_CodePtr - 1;

    tc->EMIT({
        /* Clear top + POW, ILE */
        bic_immed(tmp, reg_rs, 16, 16),
        /* Clear IP, IR, DR, RI, LE */
        bic_immed(tmp, tmp, 8, 0),
        /* Set IP again */
        orr_immed(tmp, tmp, 1, 26),
        str_offset(ctx, tmp, __builtin_offsetof(PPCState, MSR))
    });

    FreeARMRegister(tc, tmp);

    if (!tc->tc_SupervisorChecked)
    {
        /* Now insert the program exception path - raise exception if RFI was not allowed */
        uint32_t *exit_code_start = tc->tc_CodePtr;
        tc->EMIT_Exception(0x700);
        uint32_t *exit_code_end = tc->tc_CodePtr;

        /* Insert fixup location */
        tc->EMIT({ 
            (uint32_t)(exit_code_end - jump_location),
            fixup_type,
            1,
            (uint32_t)(exit_code_end - exit_code_start),
            INSN_TO_LE(MARKER_EXIT_BLOCK)
        });

        tc->tc_SupervisorChecked = true;
    }

    /* Stop here */
    tc->tc_PPCCodePtr++;
    tc->AdvancePC(4);
    tc->STOP();

    return 1;
}

static __used__ int EMIT_mtspr(struct PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Stanity check */
    if (opcode & 1) return -1;

    uint32_t spr = (opcode >> 11) & 0x3ff;
    uint8_t rs = (opcode >> 21) & 31;
    /* tbr is a split field, fix it */
    spr = ((spr >> 5) & 0x1f) | ((spr & 0x1f) << 5);

    uint8_t reg_rs = MapGPRForRead(tc, rs);

    if (spr & 0x10) {
        uint8_t ctx = GetCTX(tc);
        uint8_t tmp = AllocARMRegister(tc);
        uint8_t tmp2;

        /* 
            This fetches must not be context synchronizing and one supervisor check per
            code block is sufficient.
        */
        if (!tc->tc_SupervisorChecked)
        {
            /* We need to flush PC now, just in case */
            tc->FlushPC();

            /* 
                Fetch MSR and check if exceptions are enabled - it is illegal to call
                RFI from user
            */
            tc->EMIT({
                ldr_offset(ctx, tmp, __builtin_offsetof(PPCState, MSR)),
                tbnz(tmp, 14, 0)
            });
        }

        /* Emit jump, remember its location and fixup type */
        uint32_t fixup_type = FIXUP_TBZ;
        uint32_t *jump_location = tc->tc_CodePtr - 1;

        switch(spr) {
            case 18:   /* DSISR */
                tc->EMIT(str_offset(ctx, reg_rs, __builtin_offsetof(PPCState, DSISR)));
                break;
            case 19:   /* DAR */
                tc->EMIT(str_offset(ctx, reg_rs, __builtin_offsetof(PPCState, DAR)));
                break;
            case 22:    /* DEC */
                tc->EMIT({
                    /* Set new counter value, starts immediately counting */
                    msr(reg_rs, sys_CNTP_TVAL_EL0),
                    mov_immed_u16(tmp, 1, 0),
                    /* Set 1 to CNTP_CTL_EL0 to enable timer and unmask interrupt */
                    msr(tmp, sys_CNTP_CTL_EL0)
                });
                break;
            case 26:    /* SRR0 */
                tc->EMIT({
                    bic_immed(tmp, reg_rs, 2, 0), 
                    str_offset(ctx, tmp, __builtin_offsetof(PPCState, SRR0))
                });
                break;
            case 27:    /* SRR1 */
                tc->EMIT({
                    /* Set IP */
                    orr_immed(tmp, reg_rs, 1, 26),
                    str_offset(ctx, tmp, __builtin_offsetof(PPCState, SRR1))
                });
                break;
            case 272:   /* SPRG0 */
                tc->EMIT(str_offset(ctx, reg_rs, __builtin_offsetof(PPCState, SPRG[0])));
                break;
            case 273:   /* SPRG1 */
                tc->EMIT(str_offset(ctx, reg_rs, __builtin_offsetof(PPCState, SPRG[1])));
                break;
            case 274:   /* SPRG2 */
                tc->EMIT(str_offset(ctx, reg_rs, __builtin_offsetof(PPCState, SPRG[2])));
                break;
            case 275:   /* SPRG3 */
                tc->EMIT(str_offset(ctx, reg_rs, __builtin_offsetof(PPCState, SPRG[3])));
                break;
            case 920:   /* JIT_CONTROL */
                kprintf("[PPC] JIT_CONTROL written to, need update\n");
                tc->EMIT(str_offset(ctx, reg_rs, __builtin_offsetof(PPCState, JIT_CONTROL)));
                break;
            case 921:   /* JIT_CONTROL_2 */
                kprintf("[PPC] JIT_CONTROL2 written to, need update\n");
                tmp2 = AllocARMRegister(tc);
                tc->EMIT({
                    /* Test if topmost bit was set, if not, skip causing m68k interrupt */
                    tbz(reg_rs, 31, 6),
                        ldr64_offset(ctx, tmp, __builtin_offsetof(PPCState, M68K_FLAG)),
                        mov_immed_u16(tmp2, 255, 0),
                        strb_offset(tmp, tmp2, 0),
                        dmb_ish(),
                        sev(),
                    and_immed(tmp, reg_rs, 31, 0),
                    str_offset(ctx, tmp, __builtin_offsetof(PPCState, JIT_CONTROL2))
                });
                FreeARMRegister(tc, tmp2);
                break;
            case 944:   /* BASE */
                tc->EMIT(str_offset(ctx, reg_rs, __builtin_offsetof(PPCState, BASEREG)));
                break;
        }

        FreeARMRegister(tc, tmp);

        if (!tc->tc_SupervisorChecked)
        {
            /* Now insert the program exception path - raise exception if RFI was not allowed */
            uint32_t *exit_code_start = tc->tc_CodePtr;
            tc->EMIT_Exception(0x700);
            uint32_t *exit_code_end = tc->tc_CodePtr;

            /* Insert fixup location */
            tc->EMIT({ 
                (uint32_t)(exit_code_end - jump_location),
                fixup_type,
                1,
                (uint32_t)(exit_code_end - exit_code_start),
                INSN_TO_LE(MARKER_EXIT_BLOCK)
            });

            tc->tc_SupervisorChecked = true;
        }

        tc->AdvancePC(4);
        tc->tc_PPCCodePtr++;
        return 1;
    } else {
        /* Accessing user level SPRs */
        switch(spr) {
            case 1:
            {
                uint8_t reg_xer = IsGPRMapped(tc, XERn);
                if (reg_xer != 0xff) {
                    tc->EMIT( mov_reg(reg_xer, reg_rs));
                    SetDirtyGPR(tc, XERn);
                } else {
                    tc->EMIT( mov_reg_to_simd(REG_XER, reg_rs));
                }
                break;
            }
            case 8:
                tc->EMIT( mov_reg(MapGPRForWrite(tc, LRn), reg_rs));
                ResetReturnStack();
                break;
            case 9:
                tc->EMIT( mov_reg(MapGPRForWrite(tc, CTRn), reg_rs));
                break;
            default:
                return -1;
        }
    }

    tc->tc_PPCCodePtr++;
    tc->AdvancePC(4);
    return 1;
}

static __used__ int EMIT_rlwimix(struct PPCTranslatorContext *tc, uint32_t opcode)
{
    uint8_t update_cr = opcode & 1;
    uint8_t rs = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t sh = (opcode >> 11) & 31;
    uint8_t mb = (opcode >> 6) & 31;
    uint8_t me = (opcode >> 1) & 31;

    uint8_t reg_rs = MapGPRForRead(tc, rs);
    uint8_t reg_ra = MapGPRForWrite(tc, ra);
    uint8_t tmp = AllocARMRegister(tc);
    
    /* TODO: add obvious shortcuts! */

    /* If sh is set, rotate left */
    if (sh) {
        tc->EMIT(ror(tmp, reg_rs, (32 - sh) & 31));
    }

    /* Mask result if me - mb is not 31 */
    if (((me - mb) & 31) != 31)
    {
        if (mb <= me)
        {
            /* mb < me - mask of type 0x0f...f0 */
            tc->EMIT({
                bic_immed(reg_ra, reg_ra, 1 + me - mb, 31 & (me + 1)),
                and_immed(tmp, tmp, 1 + me - mb, 31 & (me + 1)),
                orr_reg(reg_ra, reg_ra, tmp, LSL, 0)
            });
        }
        else if (me < mb)
        {
            /* mb < me - mask of type 0xf..0..f */
            tc->EMIT({
                bic_immed(reg_ra, reg_ra, mb - me - 1, me + 1),
                and_immed(tmp, tmp, mb - me - 1, me + 1),
                orr_reg(reg_ra, reg_ra, tmp, LSL, 0)
            });
        }
    } else if (update_cr) {
        if (!sh)
            tc->EMIT(adds_immed(reg_ra, reg_rs, 0));
        else
            tc->EMIT(adds_immed(reg_ra, tmp, 0));
    }

    if (update_cr) 
    {
        EMIT_set_crn_logic(tc, 0);
    }

    FreeARMRegister(tc, tmp);

    tc->tc_PPCCodePtr++;
    tc->AdvancePC(4);
    return 1;
}

static __used__ int EMIT_rlwinmx(struct PPCTranslatorContext *tc, uint32_t opcode)
{
    uint8_t update_cr = opcode & 1;
    uint8_t rs = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t sh = (opcode >> 11) & 31;
    uint8_t mb = (opcode >> 6) & 31;
    uint8_t me = (opcode >> 1) & 31;

    uint8_t reg_rs = MapGPRForRead(tc, rs);
    uint8_t reg_ra = MapGPRForWrite(tc, ra);
    uint8_t and_src = reg_rs;

    /* TODO: add obvious shortcuts! */

    if (mb == 0 && me == (31 - sh))
    {
        /* shifting left, leaving zeros on right */
        tc->EMIT(lsl(reg_ra, reg_rs, sh));
    }
    else if (me == 31 && sh == (32 - mb))
    {
        /* shifting right, zeros on the left */
        tc->EMIT(lsr(reg_ra, reg_rs, 32 - sh));
    }
    else
    {
        /* If sh is set, rotate left */
        if (sh) {
            tc->EMIT( ror(reg_ra, reg_rs, (32 - sh) & 31));
            and_src = reg_ra;
        }

        /* Mask result if me - mb is not 31 */
        if (((me - mb) & 31) != 31)
        {
            if (mb <= me)
            {
                /* mb < me - mask of type 0x0f...f0 */
                tc->EMIT( update_cr ?
                    ands_immed(reg_ra, and_src, 1 + me - mb, 31 & (me + 1)) :
                    and_immed(reg_ra, and_src, 1 + me - mb, 31 & (me + 1))
                );
            }
            else if (me < mb)
            {
                /* mb < me - mask of type 0xf..0..f */
                tc->EMIT( update_cr ?
                    ands_immed(reg_ra, and_src, mb - me - 1, me + 1) :
                    and_immed(reg_ra, and_src, mb - me - 1, me + 1)
                );
            }
        } else if (update_cr) {
            if (!sh)
                tc->EMIT( adds_immed(reg_ra, reg_rs, 0));
            else
                tc->EMIT( tst_immed(reg_ra, 32, 0));
        }
    }

    if (update_cr) 
    {
        EMIT_set_crn_logic(tc, 0);
    }

    tc->tc_PPCCodePtr++;
    tc->AdvancePC(4);
    return 1;
}

static __used__ int EMIT_rlwnmx(struct PPCTranslatorContext *tc, uint32_t opcode)
{
    uint8_t update_cr = opcode & 1;
    uint8_t rs = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;
    uint8_t mb = (opcode >> 6) & 31;
    uint8_t me = (opcode >> 1) & 31;

    uint8_t reg_rs = MapGPRForRead(tc, rs);
    uint8_t reg_rb = MapGPRForRead(tc, rb);
    uint8_t reg_ra = MapGPRForWrite(tc, ra);

    /* TODO: add obvious shortcuts! */

    tc->EMIT({
        neg_reg(reg_ra, reg_rb, LSL, 0),
        rorv(reg_ra, reg_rs, reg_ra)
    });

    /* Mask result if me - mb is not 31 */
    if (((me - mb) & 31) != 31)
    {
        if (mb <= me)
        {
            /* mb < me - mask of type 0x0f...f0 */
            tc->EMIT( update_cr ?
                ands_immed(reg_ra, reg_ra, 1 + me - mb, 31 & (me + 1)) :
                and_immed(reg_ra, reg_ra, 1 + me - mb, 31 & (me + 1))
            );
        }
        else if (me < mb)
        {
            /* mb < me - mask of type 0xf..0..f */
            tc->EMIT( update_cr ?
                ands_immed(reg_ra, reg_ra, mb - me - 1, me + 1) :
                and_immed(reg_ra, reg_ra, mb - me - 1, me + 1)
            );
        }
    } else if (update_cr) {
        tc->EMIT(cmp_immed(reg_ra, 0));
    }

    if (update_cr) 
    {
        EMIT_set_crn_logic(tc, 0);
    }

    tc->tc_PPCCodePtr++;
    tc->AdvancePC(4);
    return 1;
}

static __used__ int EMIT_orx(struct PPCTranslatorContext *tc, uint32_t opcode)
{
    uint8_t update_cr = opcode & 1;
    uint8_t rs = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;

    if (rs == 27 && ra == 27 && rb == 27) {
        tc->EMIT(yield());
        tc->tc_PPCCodePtr++;
        tc->AdvancePC(4);
        return 1;
    }

    uint8_t reg_rs = MapGPRForRead(tc, rs);
    uint8_t reg_rb = MapGPRForRead(tc, rb);
    uint8_t reg_ra = MapGPRForWrite(tc, ra);

    if (rb == rs) {
        tc->EMIT(mov_reg(reg_ra, reg_rs));
    } else {
        tc->EMIT(orr_reg(reg_ra, reg_rs, reg_rb, LSL, 0));
    }

    if (update_cr) {
        tc->EMIT(cmp_immed(reg_ra, 0));
        EMIT_set_crn_logic(tc, 0);
    }

    tc->tc_PPCCodePtr++;
    tc->AdvancePC(4);
    return 1;
}

static __used__ int EMIT_cntlzwx(struct PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 0x0000f800) return -1;

    uint8_t update_cr = opcode & 1;
    uint8_t rs = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;

    uint8_t reg_rs = MapGPRForRead(tc, rs);
    uint8_t reg_ra = MapGPRForWrite(tc, ra);

    tc->EMIT(clz(reg_ra, reg_rs));

    if (update_cr) {
        tc->EMIT(cmp_immed(reg_ra, 0));
        EMIT_set_crn_logic(tc, 0);
    }

    tc->tc_PPCCodePtr++;
    tc->AdvancePC(4);
    return 1;
}

static __used__ int EMIT_mulhwux(struct PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & (0x80000000 >> 21)) return -1;

    uint8_t update_cr = opcode & 1;
    uint8_t rd = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;

    uint8_t reg_ra = MapGPRForRead(tc, ra);
    uint8_t reg_rb = MapGPRForRead(tc, rb);
    uint8_t reg_rd = MapGPRForWrite(tc, rd);

    tc->EMIT({
        umull(reg_rd, reg_ra, reg_rb),
        lsr64(reg_rd, reg_rd, 32)
    });


    if (update_cr) {
        tc->EMIT(cmp_immed(reg_rd, 0));
        EMIT_set_crn_signed(tc, 0);
    }

    tc->tc_PPCCodePtr++;
    tc->AdvancePC(4);
    return 1;
}

static __used__ int EMIT_mulhwx(struct PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & (0x80000000 >> 21)) return -1;

    uint8_t update_cr = opcode & 1;
    uint8_t rd = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;

    uint8_t reg_ra = MapGPRForRead(tc, ra);
    uint8_t reg_rb = MapGPRForRead(tc, rb);
    uint8_t reg_rd = MapGPRForWrite(tc, rd);

    tc->EMIT({
        smull(reg_rd, reg_ra, reg_rb),
        lsr64(reg_rd, reg_rd, 32)
    });

    if (update_cr) {
        tc->EMIT(cmp_immed(reg_rd, 0));
        EMIT_set_crn_signed(tc, 0);
    }

    tc->tc_PPCCodePtr++;
    tc->AdvancePC(4);
    return 1;
}

static __used__ int EMIT_andx(struct PPCTranslatorContext *tc, uint32_t opcode)
{
    uint8_t update_cr = opcode & 1;
    uint8_t rs = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;

    uint8_t reg_rs = MapGPRForRead(tc, rs);
    uint8_t reg_rb = MapGPRForRead(tc, rb);
    uint8_t reg_ra = MapGPRForWrite(tc, ra);

    if (update_cr) {
        tc->EMIT( ands_reg(reg_ra, reg_rs, reg_rb, LSL, 0));
        EMIT_set_crn_logic(tc, 0);
    } else {
        tc->EMIT( and_reg(reg_ra, reg_rs, reg_rb, LSL, 0));
    }

    tc->tc_PPCCodePtr++;
    tc->AdvancePC(4);
    return 1;
}

static __used__ int EMIT_norx(struct PPCTranslatorContext *tc, uint32_t opcode)
{
    uint8_t update_cr = opcode & 1;
    uint8_t rs = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;

    uint8_t reg_rs = MapGPRForRead(tc, rs);
    uint8_t reg_rb = MapGPRForRead(tc, rb);
    uint8_t reg_ra = MapGPRForWrite(tc, ra);

    if (rs == rb) {
        tc->EMIT( mvn_reg(reg_ra, reg_rb, LSL, 0));
    } else {
        tc->EMIT({ 
            orr_reg(reg_ra, reg_rs, reg_rb, LSL, 0),
            mvn_reg(reg_ra, reg_ra, LSL, 0)
        });
    }
    
    if (update_cr) {
        tc->EMIT( cmp_immed(reg_ra, 0));
        EMIT_set_crn_logic(tc, 0);
    }

    tc->tc_PPCCodePtr++;
    tc->AdvancePC(4);
    return 1;
}

static __used__ int EMIT_andcx(struct PPCTranslatorContext *tc, uint32_t opcode)
{
    uint8_t update_cr = opcode & 1;
    uint8_t rs = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;

    uint8_t reg_rs = MapGPRForRead(tc, rs);
    uint8_t reg_rb = MapGPRForRead(tc, rb);
    uint8_t reg_ra = MapGPRForWrite(tc, ra);
    
    if (update_cr) {
        tc->EMIT( bics_reg(reg_ra, reg_rs, reg_rb, LSL, 0));
        EMIT_set_crn_logic(tc, 0);
    } else {
        tc->EMIT( bic_reg(reg_ra, reg_rs, reg_rb, LSL, 0));
    }

    tc->tc_PPCCodePtr++;
    tc->AdvancePC(4);
    return 1;
}

static __used__ int EMIT_eqvx(struct PPCTranslatorContext *tc, uint32_t opcode)
{
    uint8_t update_cr = opcode & 1;
    uint8_t rs = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;

    uint8_t reg_rs = MapGPRForRead(tc, rs);
    uint8_t reg_rb = MapGPRForRead(tc, rb);
    uint8_t reg_ra = MapGPRForWrite(tc, ra);

    if (rb == rs) {
        tc->EMIT( mvn_reg(reg_ra, WZR, LSL, 0));
    } else {
        tc->EMIT( eon_reg(reg_ra, reg_rs, reg_rb, LSL, 0));
    }

    if (update_cr) {
        tc->EMIT( cmp_immed(reg_ra, 0));
        EMIT_set_crn_logic(tc, 0);
    }

    tc->tc_PPCCodePtr++;
    tc->AdvancePC(4);
    return 1;
}

static __used__ int EMIT_xorx(struct PPCTranslatorContext *tc, uint32_t opcode)
{
    uint8_t update_cr = opcode & 1;
    uint8_t rs = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;

    uint8_t reg_rs = MapGPRForRead(tc, rs);
    uint8_t reg_rb = MapGPRForRead(tc, rb);
    uint8_t reg_ra = MapGPRForWrite(tc, ra);

    tc->EMIT( eor_reg(reg_ra, reg_rs, reg_rb, LSL, 0));

    if (update_cr) {
        tc->EMIT( cmp_immed(reg_ra, 0));
        EMIT_set_crn_logic(tc, 0);
    }

    tc->tc_PPCCodePtr++;
    tc->AdvancePC(4);
    return 1;
}

static __used__ int EMIT_orcx(struct PPCTranslatorContext *tc, uint32_t opcode)
{
    uint8_t update_cr = opcode & 1;
    uint8_t rs = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;

    uint8_t reg_rs = MapGPRForRead(tc, rs);
    uint8_t reg_rb = MapGPRForRead(tc, rb);
    uint8_t reg_ra = MapGPRForWrite(tc, ra);

    tc->EMIT( orn_reg(reg_ra, reg_rs, reg_rb, LSL, 0));

    if (update_cr) {
        tc->EMIT( cmp_immed(reg_ra, 0));
        EMIT_set_crn_logic(tc, 0);
    }

    tc->tc_PPCCodePtr++;
    tc->AdvancePC(4);
    return 1;
}

static __used__ int EMIT_nandx(struct PPCTranslatorContext *tc, uint32_t opcode)
{
    uint8_t update_cr = opcode & 1;
    uint8_t rs = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;

    uint8_t reg_rs = MapGPRForRead(tc, rs);
    uint8_t reg_rb = MapGPRForRead(tc, rb);
    uint8_t reg_ra = MapGPRForWrite(tc, ra);

    tc->EMIT({ 
        and_reg(reg_ra, reg_rs, reg_rb, LSL, 0),
        mvn_reg(reg_ra, reg_ra, LSL, 0)
    });

    if (update_cr) {
        tc->EMIT( cmp_immed(reg_ra, 0));
        EMIT_set_crn_logic(tc, 0);
    }

    tc->tc_PPCCodePtr++;
    tc->AdvancePC(4);
    return 1;
}

static __used__ int EMIT_mulli(struct PPCTranslatorContext *tc, uint32_t opcode)
{
    uint8_t rd = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint16_t immed = opcode & 0xffff;

    uint8_t reg_ra = MapGPRForRead(tc, ra);
    uint8_t reg_rd = MapGPRForWrite(tc, rd);
    uint8_t tmp = AllocARMRegister(tc);

    if (immed & 0x8000) {
        tc->EMIT( movn_immed_u16(tmp, ~immed & 0xffff, 0));
    } else {
        tc->EMIT( mov_immed_u16(tmp, immed, 0));
    }

    tc->EMIT( mul(reg_rd, reg_ra, tmp));

    FreeARMRegister(tc, tmp);

    tc->tc_PPCCodePtr++;
    tc->AdvancePC(4);
    return 1;
}

static __used__ int EMIT_addx(struct PPCTranslatorContext *tc, uint32_t opcode)
{
    uint8_t rd = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;
    uint8_t oe = (opcode >> 10) & 1;
    uint8_t rc = opcode & 1;

    uint8_t reg_ra = MapGPRForRead(tc, ra);
    uint8_t reg_rb = MapGPRForRead(tc, rb);
    uint8_t reg_rd = MapGPRForWrite(tc, rd);

    if (oe || rc) {
        tc->EMIT( adds_reg(reg_rd, reg_ra, reg_rb, LSL, 0));
    } else {
        tc->EMIT( add_reg(reg_rd, reg_ra, reg_rb, LSL, 0));
    }

    if (oe) {
        uint8_t reg_xer = MapGPRForReadAndWrite(tc, XERn);
        uint8_t tmp = AllocARMRegister(tc);

        tc->EMIT({
            orr_immed(reg_xer, reg_xer, 2, 2),
            bic_immed(tmp, reg_xer, 1, 2),
            csel(reg_xer, reg_xer, tmp, A64_CC_VS)
        });

        FreeARMRegister(tc, tmp);
    }

    if (rc) {
        EMIT_set_crn_signed(tc, 0);
    }

    tc->tc_PPCCodePtr++;
    tc->AdvancePC(4);
    return 1;
}

static __used__ int EMIT_addic(struct PPCTranslatorContext *tc, uint32_t opcode)
{
    uint8_t rd = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint16_t imm = (opcode & 0xffff);

    uint8_t reg_ra = MapGPRForRead(tc, ra);
    uint8_t reg_rd = MapGPRForWrite(tc, rd);
    uint8_t tmp = AllocARMRegister(tc);
    uint8_t reg_xer = MapGPRForReadAndWrite(tc, XERn);

    if (imm & 0x8000) {
        tc->EMIT( movn_immed_u16(tmp, ~imm & 0xffff, 0));
    } else {
        tc->EMIT( mov_immed_u16(tmp, imm, 0));
    }

    tc->EMIT({ 
        bic_immed(reg_xer, reg_xer, 1, 3),      // Clear CA flag in xer
        adds_reg(reg_rd, tmp, reg_ra, LSL, 0),
        orr_immed(tmp, reg_xer, 1, 3),          // Set CA flag from xer into tmp
        csel(reg_xer, tmp, reg_xer, A64_CC_CS)  // Select CA set or clear in XER depending on A64 C flag
    });

    FreeARMRegister(tc, tmp);

    tc->tc_PPCCodePtr++;
    tc->AdvancePC(4);
    return 1;
}

static __used__ int EMIT_addic_dot(struct PPCTranslatorContext *tc, uint32_t opcode)
{
    uint8_t rd = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint16_t imm = (opcode & 0xffff);

    uint8_t reg_ra = MapGPRForRead(tc, ra);
    uint8_t reg_rd = MapGPRForWrite(tc, rd);
    uint8_t tmp = AllocARMRegister(tc);
    uint8_t reg_xer = MapGPRForReadAndWrite(tc, XERn);

    if (imm & 0x8000) {
        tc->EMIT( movn_immed_u16(tmp, ~imm & 0xffff, 0));
    } else {
        tc->EMIT( mov_immed_u16(tmp, imm, 0));
    }

    tc->EMIT({ 
        bic_immed(reg_xer, reg_xer, 1, 3),      // Clear CA flag in xer
        adds_reg(reg_rd, tmp, reg_ra, LSL, 0),
        orr_immed(tmp, reg_xer, 1, 3),          // Set CA flag from xer into tmp
        csel(reg_xer, tmp, reg_xer, A64_CC_CS)  // Select CA set or clear in XER depending on A64 C flag
    });

    FreeARMRegister(tc, tmp);

    EMIT_set_crn_signed(tc, 0);

    tc->tc_PPCCodePtr++;
    tc->AdvancePC(4);
    return 1;
}

static __used__ int EMIT_subfx(struct PPCTranslatorContext *tc, uint32_t opcode)
{
    uint8_t rd = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;
    uint8_t oe = (opcode >> 10) & 1;
    uint8_t rc = opcode & 1;

    uint8_t reg_ra = MapGPRForRead(tc, ra);
    uint8_t reg_rb = MapGPRForRead(tc, rb);
    uint8_t reg_rd = MapGPRForWrite(tc, rd);

    if (oe || rc) {
        tc->EMIT( subs_reg(reg_rd, reg_rb, reg_ra, LSL, 0));
    } else {
        tc->EMIT( sub_reg(reg_rd, reg_rb, reg_ra, LSL, 0));
    }

    if (oe) {
        uint8_t reg_xer = MapGPRForReadAndWrite(tc, XERn);
        uint8_t tmp = AllocARMRegister(tc);

        tc->EMIT({
            orr_immed(reg_xer, reg_xer, 2, 2),
            bic_immed(tmp, reg_xer, 1, 2),
            csel(reg_xer, reg_xer, tmp, A64_CC_VS)
        });

        FreeARMRegister(tc, tmp);
    }

    if (rc) {
        EMIT_set_crn_signed(tc, 0);
    }

    tc->tc_PPCCodePtr++;
    tc->AdvancePC(4);
    return 1;
}

static __used__ int EMIT_subfic(struct PPCTranslatorContext *tc, uint32_t opcode)
{
    uint8_t rd = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint16_t imm = (opcode & 0xffff);

    uint8_t reg_ra = MapGPRForRead(tc, ra);
    uint8_t reg_rd = MapGPRForWrite(tc, rd);
    uint8_t tmp = AllocARMRegister(tc);
    uint8_t reg_xer = MapGPRForReadAndWrite(tc, XERn);

    if (imm & 0x8000) {
        tc->EMIT( movn_immed_u16(tmp, ~imm & 0xffff, 0));
    } else {
        tc->EMIT( mov_immed_u16(tmp, imm, 0));
    }

    tc->EMIT({ 
        bic_immed(reg_xer, reg_xer, 1, 3),      // Clear CA flag in xer
        subs_reg(reg_rd, tmp, reg_ra, LSL, 0),
        orr_immed(tmp, reg_xer, 1, 3),          // Set CA flag from xer into tmp
        csel(reg_xer, tmp, reg_xer, A64_CC_CS)  // Select CA set or clear in XER depending on A64 C flag
    });

    FreeARMRegister(tc, tmp);

    tc->tc_PPCCodePtr++;
    tc->AdvancePC(4);
    return 1;
}

static __used__ int EMIT_subfcx(struct PPCTranslatorContext *tc, uint32_t opcode)
{
    uint8_t rd = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;
    uint8_t oe = (opcode >> 10) & 1;
    uint8_t rc = opcode & 1;

    uint8_t reg_ra = MapGPRForRead(tc, ra);
    uint8_t reg_rb = MapGPRForRead(tc, rb);
    uint8_t reg_rd = MapGPRForWrite(tc, rd);
    uint8_t tmp = AllocARMRegister(tc);
    uint8_t reg_xer = MapGPRForReadAndWrite(tc, XERn);

    // TODO: Verify if flag set correctly

    tc->EMIT({ 
        bic_immed(reg_xer, reg_xer, 1, 3),      // Clear CA flag in xer
        subs_reg(reg_rd, reg_rb, reg_ra, LSL, 0),
        orr_immed(tmp, reg_xer, 1, 3),          // Set CA flag from xer into tmp
        csel(reg_xer, tmp, reg_xer, A64_CC_CS)  // Select CA set or clear in XER depending on A64 C flag
    });

    if (oe) {
        tc->EMIT({
            orr_immed(reg_xer, reg_xer, 2, 2),
            bic_immed(tmp, reg_xer, 1, 2),
            csel(reg_xer, reg_xer, tmp, A64_CC_VS)
        });
    }

    FreeARMRegister(tc, tmp);

    if (rc) {
        EMIT_set_crn_signed(tc, 0);
    }

    tc->tc_PPCCodePtr++;
    tc->AdvancePC(4);
    return 1;
}

static __used__ int EMIT_addcx(struct PPCTranslatorContext *tc, uint32_t opcode)
{
    uint8_t rd = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;
    uint8_t oe = (opcode >> 10) & 1;
    uint8_t rc = opcode & 1;

    uint8_t reg_ra = MapGPRForRead(tc, ra);
    uint8_t reg_rb = MapGPRForRead(tc, rb);
    uint8_t reg_rd = MapGPRForWrite(tc, rd);
    uint8_t tmp = AllocARMRegister(tc);
    uint8_t reg_xer = MapGPRForReadAndWrite(tc, XERn);

    // TODO: Verify if flag set correctly

    tc->EMIT({ 
        bic_immed(reg_xer, reg_xer, 1, 3),      // Clear CA flag in xer
        adds_reg(reg_rd, reg_rb, reg_ra, LSL, 0),
        orr_immed(tmp, reg_xer, 1, 3),          // Set CA flag from xer into tmp
        csel(reg_xer, tmp, reg_xer, A64_CC_CS)  // Select CA set or clear in XER depending on A64 C flag
    });

    if (oe) {
        tc->EMIT({
            orr_immed(reg_xer, reg_xer, 2, 2),
            bic_immed(tmp, reg_xer, 1, 2),
            csel(reg_xer, reg_xer, tmp, A64_CC_VS)
        });
    }

    FreeARMRegister(tc, tmp);

    if (rc) {
        EMIT_set_crn_signed(tc, 0);
    }

    tc->tc_PPCCodePtr++;
    tc->AdvancePC(4);
    return 1;
}

static __used__ int EMIT_addex(struct PPCTranslatorContext *tc, uint32_t opcode)
{
    uint8_t rd = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;
    uint8_t oe = (opcode >> 10) & 1;
    uint8_t rc = opcode & 1;

    uint8_t reg_ra = MapGPRForRead(tc, ra);
    uint8_t reg_rb = MapGPRForRead(tc, rb);
    uint8_t reg_rd = MapGPRForWrite(tc, rd);
    uint8_t tmp = AllocARMRegister(tc);
    uint8_t reg_xer = MapGPRForReadAndWrite(tc, XERn);

    // TODO: Verify if flag set correctly

    tc->EMIT({ 
        ubfx(tmp, reg_xer, 29, 1),                 // Extract CA field into tmp
        subs_immed(WZR, tmp, 1),                    // Subtract 1, this will set carry bit
        bic_immed(reg_xer, reg_xer, 1, 3),          // Clear CA flag in xer
        adcs(reg_rd, reg_ra, reg_rb),               // Add with carry
        orr_immed(tmp, reg_xer, 1, 3),              // Set CA flag from xer into tmp
        csel(reg_xer, tmp, reg_xer, A64_CC_CS)      // Select CA set or clear in XER depending on A64 C flag
    });

    if (oe) {
        tc->EMIT({
            orr_immed(reg_xer, reg_xer, 2, 2),
            bic_immed(tmp, reg_xer, 1, 2),
            csel(reg_xer, reg_xer, tmp, A64_CC_VS)
        });
    }

    FreeARMRegister(tc, tmp);

    if (rc) {
        EMIT_set_crn_signed(tc, 0);
    }

    tc->tc_PPCCodePtr++;
    tc->AdvancePC(4);
    return 1;
}

static __used__ int EMIT_cmp(struct PPCTranslatorContext *tc, uint32_t opcode)
{
    /* sanity check */
    if (opcode & 0x00600001) return -1;

    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;
    uint8_t cr = (opcode >> 23) & 7;

    uint8_t reg_ra = MapGPRForRead(tc, ra);
    uint8_t reg_rb = MapGPRForRead(tc, rb);

    /* Force-load XER and CR */
#if PPC_SO_PROPAGATION
    MapGPRForRead(tc, XERn);
#endif
    MapGPRForReadAndWrite(tc, CRn);

    tc->EMIT( cmp_reg(reg_ra, reg_rb, LSL, 0));

    EMIT_set_crn_signed(tc, cr);

    tc->tc_PPCCodePtr++;
    tc->AdvancePC(4);
    return 1;
}

static __used__ int EMIT_tw(struct PPCTranslatorContext *tc, uint32_t opcode)
{
    /* sanity check */
    if (opcode & 0x00000001) return -1;

    uint8_t to = (opcode >> 21) & 31;

    if (to == 0)
    {
        /* Not expecting any condition generating exception so skip */
        tc->EMIT(nop());
        
        tc->tc_PPCCodePtr++;
        tc->AdvancePC(4);
        return 1;
    }
    else if (to == 31)
    {
        /* All bits set, trap always */
        tc->EMIT_Exception(0x700);
        tc->STOP();
        return 1;
    }

    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;

    uint8_t reg_ra = MapGPRForRead(tc, ra);
    uint8_t reg_rb = MapGPRForRead(tc, rb);
    int number_of_cases = 0;
    uint32_t *locations[5];

    /* Emit comparison */
    tc->EMIT(cmp_reg(reg_ra, reg_rb, LSL, 0));
    
    if (to & 1) {
        
        tc->EMIT(b_cc(A64_CC_HI, 0));
        locations[number_of_cases++] = tc->tc_CodePtr - 1;
    }
    if (to & 2) {
        
        tc->EMIT(b_cc(A64_CC_CC, 0));
        locations[number_of_cases++] = tc->tc_CodePtr - 1;
    }
    if (to & 4) {
        
        tc->EMIT(b_cc(A64_CC_EQ, 0));
        locations[number_of_cases++] = tc->tc_CodePtr - 1;
    }
    if (to & 8) {
        
        tc->EMIT(b_cc(A64_CC_GT, 0));
        locations[number_of_cases++] = tc->tc_CodePtr - 1;
    }
    if (to & 16) {
        
        tc->EMIT(b_cc(A64_CC_LT, 0));
        locations[number_of_cases++] = tc->tc_CodePtr - 1;
    }

    uint32_t *exit_code_start = tc->tc_CodePtr;
    tc->EMIT_Exception(0x700);
    uint32_t *exit_code_end = tc->tc_CodePtr;

    /* Insert additional exit points */
    for (int i=0; i < number_of_cases; i++) {
        tc->EMIT({ 
            (uint32_t)(exit_code_end - locations[i]),
            FIXUP_BCC
        });
    }

    tc->EMIT({
        (uint32_t)number_of_cases,
        (uint32_t)(exit_code_end - exit_code_start),
        INSN_TO_LE(MARKER_EXIT_BLOCK)
    });

    tc->tc_PPCCodePtr++;
    tc->AdvancePC(4);
    return 1;
}

static __used__ int EMIT_twi(struct PPCTranslatorContext *tc, uint32_t opcode)
{
    uint8_t to = (opcode >> 21) & 31;

    if (to == 0)
    {
        /* Not expecting any condition generating exception so skip */
        tc->EMIT(nop());
        
        tc->tc_PPCCodePtr++;
        tc->AdvancePC(4);
        return 1;
    }
    else if (to == 31)
    {
        /* All bits set, trap always */
        tc->EMIT_Exception(0x700);
        tc->STOP();
        return 1;
    }

    uint8_t ra = (opcode >> 16) & 31;
    uint16_t imm = opcode & 0xffff;
    int32_t simm = (int16_t)imm;

    uint8_t reg_ra = MapGPRForRead(tc, ra);

    int number_of_cases = 0;
    uint32_t *locations[5];

    /* Is the immediate in range for CMP? */
    if ((simm & 0xfffff000) == 0) {
        tc->EMIT( cmp_immed(reg_ra, simm));
    }
    else if ((simm & 0xff000fff) == 0) {
        tc->EMIT( cmp_immed_lsl12(reg_ra, simm >> 12));
    }
    else {
        uint8_t tmp = AllocARMRegister(tc);

        tc->LoadImmediate(tmp, simm);
        tc->EMIT( cmp_reg(reg_ra, tmp, LSL, 0));

        FreeARMRegister(tc, tmp);
    }

    if (to & 1) {
        
        tc->EMIT(b_cc(A64_CC_HI, 0));
        locations[number_of_cases++] = tc->tc_CodePtr - 1;
    }
    if (to & 2) {
        
        tc->EMIT(b_cc(A64_CC_CC, 0));
        locations[number_of_cases++] = tc->tc_CodePtr - 1;
    }
    if (to & 4) {
        
        tc->EMIT(b_cc(A64_CC_EQ, 0));
        locations[number_of_cases++] = tc->tc_CodePtr - 1;
    }
    if (to & 8) {
        
        tc->EMIT(b_cc(A64_CC_GT, 0));
        locations[number_of_cases++] = tc->tc_CodePtr - 1;
    }
    if (to & 16) {
        
        tc->EMIT(b_cc(A64_CC_LT, 0));
        locations[number_of_cases++] = tc->tc_CodePtr - 1;
    }

    uint32_t *exit_code_start = tc->tc_CodePtr;
    tc->EMIT_Exception(0x700);
    uint32_t *exit_code_end = tc->tc_CodePtr;

    /* Insert additional exit points */
    for (int i=0; i < number_of_cases; i++) {
        tc->EMIT({ 
            (uint32_t)(exit_code_end - locations[i]),
            FIXUP_BCC
        });
    }

    tc->EMIT({
        (uint32_t)number_of_cases,
        (uint32_t)(exit_code_end - exit_code_start),
        INSN_TO_LE(MARKER_EXIT_BLOCK)
    });

    tc->tc_PPCCodePtr++;
    tc->AdvancePC(4);
    return 1;
}

static __used__ int EMIT_cmpl(struct PPCTranslatorContext *tc, uint32_t opcode)
{
    /* sanity check */
    if (opcode & 0x00600001) return -1;

    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;
    uint8_t cr = (opcode >> 23) & 7;

    /* Force-load XER and CR */
#if PPC_SO_PROPAGATION
    MapGPRForRead(tc, XERn);
#endif
    MapGPRForReadAndWrite(tc, CRn);

    uint8_t reg_ra = MapGPRForRead(tc, ra);
    uint8_t reg_rb = MapGPRForRead(tc, rb);

    tc->EMIT( cmp_reg(reg_ra, reg_rb, LSL, 0));

    EMIT_set_crn_unsigned(tc, cr);

    tc->tc_PPCCodePtr++;
    tc->AdvancePC(4);
    return 1;
}

static __used__ int EMIT_negx(struct PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 0x0000f800) return -1;

    uint8_t rd = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    
    uint8_t oe = (opcode >> 10) & 1;
    uint8_t rc = opcode & 1;

    uint8_t reg_ra = MapGPRForRead(tc, ra);
    uint8_t reg_rd = MapGPRForWrite(tc, rd);

    if (oe || rc) {
        tc->EMIT( negs_reg(reg_rd, reg_ra, LSL, 0));
    } else {
        tc->EMIT( neg_reg(reg_rd, reg_ra, LSL, 0));
    }

    if (oe) {
        uint8_t reg_xer = MapGPRForReadAndWrite(tc, XERn);
        uint8_t tmp = AllocARMRegister(tc);

        tc->EMIT({
            orr_immed(reg_xer, reg_xer, 2, 2),
            bic_immed(tmp, reg_xer, 1, 2),
            csel(reg_xer, reg_xer, tmp, A64_CC_VS)
        });

        FreeARMRegister(tc, tmp);
    }

    if (rc) {
        EMIT_set_crn_signed(tc, 0);
    }

    tc->tc_PPCCodePtr++;
    tc->AdvancePC(4);
    return 1;
}

static __used__ int EMIT_divwux(struct PPCTranslatorContext *tc, uint32_t opcode)
{
    uint8_t rd = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;
    
    uint8_t oe = (opcode >> 10) & 1;
    uint8_t rc = opcode & 1;

    uint8_t reg_ra = MapGPRForRead(tc, ra);
    uint8_t reg_rb = MapGPRForRead(tc, rb);
    uint8_t reg_rd = MapGPRForWrite(tc, rd);

    if (oe) {
        uint8_t tmp = AllocARMRegister(tc);
        uint8_t reg_xer = MapGPRForReadAndWrite(tc, XERn);

        tc->EMIT({
            cmp_immed(reg_rb, 0),
            orr_immed(reg_xer, reg_xer, 2, 2),
            bic_immed(tmp, reg_xer, 1, 2),
            csel(reg_xer, reg_xer, tmp, A64_CC_EQ)
        });

        FreeARMRegister(tc, tmp);
    }

    tc->EMIT( udiv(reg_rd, reg_ra, reg_rb));

    if (rc) {
        tc->EMIT( cmp_immed(reg_rd, 0));
        EMIT_set_crn_signed(tc, 0);
    }

    tc->tc_PPCCodePtr++;
    tc->AdvancePC(4);
    return 1;
}

static __used__ int EMIT_divwx(struct PPCTranslatorContext *tc, uint32_t opcode)
{
    uint8_t rd = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;
    
    uint8_t oe = (opcode >> 10) & 1;
    uint8_t rc = opcode & 1;

    uint8_t reg_ra = MapGPRForRead(tc, ra);
    uint8_t reg_rb = MapGPRForRead(tc, rb);
    uint8_t reg_rd = MapGPRForWrite(tc, rd);

    if (oe) {
        uint8_t tmp = AllocARMRegister(tc);
        uint8_t reg_xer = MapGPRForReadAndWrite(tc, XERn);

        // TODO: set XER OV if 0x80000000 / -1 is attempted

        tc->EMIT({
            cmp_immed(reg_rb, 0),
            orr_immed(reg_xer, reg_xer, 2, 2),
            bic_immed(tmp, reg_xer, 1, 2),
            csel(reg_xer, reg_xer, tmp, A64_CC_EQ)
        });

        FreeARMRegister(tc, tmp);
    }

    tc->EMIT( sdiv(reg_rd, reg_ra, reg_rb));

    if (rc) {
        tc->EMIT( cmp_immed(reg_rd, 0));
        EMIT_set_crn_signed(tc, 0);
    }

    tc->tc_PPCCodePtr++;
    tc->AdvancePC(4);
    return 1;
}

static __used__ int EMIT_mullwx(struct PPCTranslatorContext *tc, uint32_t opcode)
{
    uint8_t rd = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;
    
    uint8_t oe = (opcode >> 10) & 1;
    uint8_t rc = opcode & 1;

    uint8_t reg_ra = MapGPRForRead(tc, ra);
    uint8_t reg_rb = MapGPRForRead(tc, rb);
    uint8_t reg_rd = MapGPRForWrite(tc, rd);

    tc->EMIT( smull(reg_rd, reg_ra, reg_rb));

    if (oe) {
        uint8_t tmp = AllocARMRegister(tc);
        uint8_t reg_xer = MapGPRForReadAndWrite(tc, XERn);

        tc->EMIT({
            sxtw64(tmp, reg_rd),
            cmp_reg(tmp, reg_rd, LSL, 0),
            orr_immed(reg_xer, reg_xer, 2, 2),
            bic_immed(tmp, reg_xer, 1, 2),
            csel(reg_xer, reg_xer, tmp, A64_CC_NE)
        });

        FreeARMRegister(tc, tmp);
    }

    if (rc) {
        tc->EMIT( cmp_immed(reg_rd, 0));
        EMIT_set_crn_signed(tc, 0);
    }

    tc->tc_PPCCodePtr++;
    tc->AdvancePC(4);
    return 1;
}

static __used__ int EMIT_srwx(struct PPCTranslatorContext *tc, uint32_t opcode)
{
    uint8_t rs = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;
    
    uint8_t rc = opcode & 1;

    uint8_t reg_rb = MapGPRForRead(tc, rb);
    uint8_t reg_rs = MapGPRForRead(tc, rs);
    uint8_t reg_ra = MapGPRForWrite(tc, ra);

    tc->EMIT( lsrv(reg_ra, reg_rs, reg_rb));

    if (rc) {
        tc->EMIT( cmp_immed(reg_ra, 0));
        EMIT_set_crn_signed(tc, 0);
    }

    tc->tc_PPCCodePtr++;
    tc->AdvancePC(4);
    return 1;
}

static __used__ int EMIT_slwx(struct PPCTranslatorContext *tc, uint32_t opcode)
{
    uint8_t rs = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;
    
    uint8_t rc = opcode & 1;

    uint8_t reg_rs = MapGPRForRead(tc, rs);
    uint8_t reg_rb = MapGPRForRead(tc, rb);
    uint8_t reg_ra = MapGPRForWrite(tc, ra);

    tc->EMIT( lslv(reg_ra, reg_rs, reg_rb));

    if (rc) {
        tc->EMIT( cmp_immed(reg_ra, 0));
        EMIT_set_crn_signed(tc, 0);
    }

    tc->tc_PPCCodePtr++;
    tc->AdvancePC(4);
    return 1;
}

static __used__ int EMIT_srawix(struct PPCTranslatorContext *tc, uint32_t opcode)
{
    uint8_t rs = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t sh = (opcode >> 11) & 31;
    
    uint8_t rc = opcode & 1;

    uint8_t reg_rs = MapGPRForRead(tc, rs);
    uint8_t reg_ra = MapGPRForWrite(tc, ra);

    uint8_t tmp = AllocARMRegister(tc);
    uint8_t reg_xer = MapGPRForReadAndWrite(tc, XERn);

    if (sh != 0) {
        tc->EMIT({
            /* Test if source is signed */
            tst_immed(reg_rs, 1, 1),
            /* If source is signed, put reg_rs to tmp, otherwise zero it */
            csel(tmp, reg_rs, XZR, A64_CC_EQ),
            /* Check the mask - if any bit is 1, set CA to 1 */
            tst_immed(tmp, sh, 0),
            orr_immed(reg_xer, reg_xer, 1, 3),
            bic_immed(tmp, reg_xer, 1, 3),
            csel(reg_xer, reg_xer, tmp, A64_CC_NE)
        });
    } else {
        tc->EMIT( bic_immed(reg_xer, reg_xer, 1, 3));
    }
    
    tc->EMIT( asr(reg_ra, reg_rs, sh));

    FreeARMRegister(tc, tmp);

    if (rc) {
        tc->EMIT( cmp_immed(reg_ra, 0));
        EMIT_set_crn_signed(tc, 0);
    }

    tc->tc_PPCCodePtr++;
    tc->AdvancePC(4);
    return 1;
}

static __used__ int EMIT_srawx(struct PPCTranslatorContext *tc, uint32_t opcode)
{
    uint8_t rs = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;
    
    uint8_t rc = opcode & 1;

    uint8_t reg_rs = MapGPRForRead(tc, rs);
    uint8_t reg_rb = MapGPRForRead(tc, rb);
    uint8_t reg_ra = MapGPRForWrite(tc, ra);

    uint8_t tmp = AllocARMRegister(tc);
    uint8_t mask = AllocARMRegister(tc);
    uint8_t reg_xer = MapGPRForReadAndWrite(tc, XERn);

    tc->EMIT({
        /* Test if source is signed */
        tst_immed(reg_rs, 1, 1),
        /* If source is signed, put reg_rs to tmp, otherwise zero it */
        csel(tmp, reg_rs, XZR, A64_CC_EQ),
        /* Check the mask - if any bit is 1, set CA to 1 */
        mov_immed_u16(mask, 1, 0),
        lslv64(mask, mask, reg_rb),
        sub64_immed(mask, mask, 1),
        tst_reg(tmp, mask, LSL, 0),
        orr_immed(reg_xer, reg_xer, 1, 3),
        bic_immed(tmp, reg_xer, 1, 3),
        csel(reg_xer, reg_xer, tmp, A64_CC_NE)
    });
    
    tc->EMIT( asrv(reg_ra, reg_rs, reg_rb));

    FreeARMRegister(tc, mask);
    FreeARMRegister(tc, tmp);

    if (rc) {
        tc->EMIT( cmp_immed(reg_ra, 0));
        EMIT_set_crn_signed(tc, 0);
    }

    tc->tc_PPCCodePtr++;
    tc->AdvancePC(4);
    return 1;
}

static __used__ int EMIT_extsbx(struct PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 0x0000f800) return -1;

    uint8_t rs = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    
    uint8_t rc = opcode & 1;

    uint8_t reg_rs = MapGPRForRead(tc, rs);
    uint8_t reg_ra = MapGPRForWrite(tc, ra);
    
    tc->EMIT( sxtb(reg_ra, reg_rs));

    if (rc) {
        tc->EMIT( cmp_immed(reg_ra, 0));
        EMIT_set_crn_logic(tc, 0);
    }

    tc->tc_PPCCodePtr++;
    tc->AdvancePC(4);
    return 1;
}

static __used__ int EMIT_extshx(struct PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 0x0000f800) return -1;

    uint8_t rs = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    
    uint8_t rc = opcode & 1;

    uint8_t reg_rs = MapGPRForRead(tc, rs);
    uint8_t reg_ra = MapGPRForWrite(tc, ra);
    
    tc->EMIT( sxth(reg_ra, reg_rs));

    if (rc) {
        tc->EMIT( cmp_immed(reg_ra, 0));
        EMIT_set_crn_logic(tc, 0);
    }

    tc->tc_PPCCodePtr++;
    tc->AdvancePC(4);
    return 1;
}

static __used__ int EMIT_eieio(struct PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 0x03fff800) return -1;

    tc->EMIT( dmb_sy());

    tc->tc_PPCCodePtr++;
    tc->AdvancePC(4);
    return 1;
}

static __used__ int EMIT_icbi(struct PPCTranslatorContext *tc, uint32_t opcode)
{
    extern LRUCache cache;

    /* Sanity check */
    if (opcode & 0x03e00001) return -1;

    uint8_t base = AllocARMRegister(tc);
    uint8_t cnt = AllocARMRegister(tc);
    uint8_t fill = AllocARMRegister(tc);
    
    /*
        PPC Architecture allows that icbi flushes more than requested. This is what
        will be done here.
    */
    tc->EMIT({
        /* Load offset for LRU cache */
        mov_immed_u16(base, (uint16_t)(uintptr_t)cache.cacheLoc(), 0),
        movk_immed_u16(base, (uint16_t)((uintptr_t)cache.cacheLoc() >> 16), 1),

        /* make "high" address from offset */
        orr64_immed(base, base, 25, 25, 1),

        /* Load fill values */
        movn64_immed_u16(fill, 0, 0),

        /* Load counter */
        mov_immed_u16(cnt, EMU68_LRU_SET_COUNT * EMU68_LRU_WAY_COUNT, 0),

        /* In the loop: */
        stp64_postindex(base, fill, XZR, 16),
        subs_immed(cnt, cnt, 1),
        b_cc(A64_CC_NE, -2),

        /* Upper base bits are still valid, update lower ones */
        movk_immed_u16(base, (uint16_t)(uintptr_t)cache.allocLoc(), 0),
        movk_immed_u16(base, (uint16_t)((uintptr_t)cache.allocLoc() >> 16), 1),

        /* Load counter, we clear 4 items with one stp */
        mov_immed_u16(cnt, EMU68_LRU_SET_COUNT / 4, 0),

        /* In the loop: */
        stp64_postindex(base, fill, fill, 16),
        subs_immed(cnt, cnt, 1),
        b_cc(A64_CC_NE, -2),

        /* Clear last PC value so that a short path in JIT loop is avoided, fill register is already there */
        mov_reg_to_simd(CTX_LAST_PC, fill),

        /* Last but not least - increase epoch */
        mov_simd_to_reg(cnt, EPOCH),
        add_immed(cnt, cnt, 1),
        mov_reg_to_simd(EPOCH, cnt),
    });
    
    FreeARMRegister(tc, cnt);
    FreeARMRegister(tc, fill);
    FreeARMRegister(tc, base);

    tc->tc_PPCCodePtr++;
    tc->AdvancePC(4);
    return 1;
}

static __used__ int EMIT_sync(struct PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 0x001ff800) return -1;

    uint8_t op = (opcode >> 21) & 31;

    switch(op) {
        case 0:
            tc->EMIT(isb());
            break;
        case 1:
            tc->EMIT(dmb_sy());
            break;
        default:
            return -1;
    }

    tc->tc_PPCCodePtr++;
    tc->AdvancePC(4);
    return 1;
}

static __used__ int EMIT_dcbst(struct PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 0x03e00001) return -1;

    uint8_t ra = (opcode >> 21) & 31;
    uint8_t rb = (opcode >> 16) & 31;
    uint8_t reg_rb = MapGPRForRead(tc, rb);
    uint8_t base;

    if (ra == 0) {
        base = reg_rb;
    } else {
        uint8_t reg_ra = MapGPRForRead(tc, ra);
        base = AllocARMRegister(tc);
        tc->EMIT(add_reg(base, reg_ra, reg_rb, LSL, 0));
    }

    tc->EMIT({
        dsb_sy(),
        dc_cvac(base),
        dsb_sy()
    });

    if (ra != 0)
        FreeARMRegister(tc, base);

    tc->tc_PPCCodePtr++;
    tc->AdvancePC(4);
    return 1;
}

static __used__ int EMIT_dcbtst(struct PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 0x03e00001) return -1;

    uint8_t ra = (opcode >> 21) & 31;
    uint8_t rb = (opcode >> 16) & 31;
    uint8_t reg_rb = MapGPRForRead(tc, rb);
    uint8_t base;

    if (ra == 0) {
        base = reg_rb;
    } else {
        uint8_t reg_ra = MapGPRForRead(tc, ra);
        base = AllocARMRegister(tc);
        tc->EMIT(add_reg(base, reg_ra, reg_rb, LSL, 0));
    }

    tc->EMIT(prfm_pst(base));

    if (ra != 0)
        FreeARMRegister(tc, base);

    tc->tc_PPCCodePtr++;
    tc->AdvancePC(4);
    return 1;
}

static __used__ int EMIT_dcbt(struct PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 0x03e00001) return -1;

    uint8_t ra = (opcode >> 21) & 31;
    uint8_t rb = (opcode >> 16) & 31;
    uint8_t reg_rb = MapGPRForRead(tc, rb);
    uint8_t base;

    if (ra == 0) {
        base = reg_rb;
    } else {
        uint8_t reg_ra = MapGPRForRead(tc, ra);
        base = AllocARMRegister(tc);
        tc->EMIT(add_reg(base, reg_ra, reg_rb, LSL, 0));
    }

    tc->EMIT(prfm_pld(base));

    if (ra != 0)
        FreeARMRegister(tc, base);

    tc->tc_PPCCodePtr++;
    tc->AdvancePC(4);
    return 1;
}

static __used__ int EMIT_dcbf(struct PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 0x03e00001) return -1;

    uint8_t ra = (opcode >> 21) & 31;
    uint8_t rb = (opcode >> 16) & 31;
    uint8_t reg_rb = MapGPRForRead(tc, rb);
    uint8_t base;

    if (ra == 0) {
        base = reg_rb;
    } else {
        uint8_t reg_ra = MapGPRForRead(tc, ra);
        base = AllocARMRegister(tc);
        tc->EMIT(add_reg(base, reg_ra, reg_rb, LSL, 0));
    }

    tc->EMIT({
        dsb_sy(),
        dc_civac(base),
        dsb_sy()
    });

    if (ra != 0)
        FreeARMRegister(tc, base);

    tc->tc_PPCCodePtr++;
    tc->AdvancePC(4);
    return 1;
}

static __used__ int EMIT_dcbz(struct PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 0x03e00001) return -1;

    uint8_t ra = (opcode >> 21) & 31;
    uint8_t rb = (opcode >> 16) & 31;
    uint8_t reg_rb = MapGPRForRead(tc, rb);
    uint8_t base;

    if (ra == 0) {
        base = reg_rb;
    } else {
        uint8_t reg_ra = MapGPRForRead(tc, ra);
        base = AllocARMRegister(tc);
        tc->EMIT(add_reg(base, reg_ra, reg_rb, LSL, 0));
    }

    tc->EMIT({
        dsb_sy(),
        dc_zva(base),
        dsb_sy()
    });

    if (ra != 0)
        FreeARMRegister(tc, base);

    tc->tc_PPCCodePtr++;
    tc->AdvancePC(4);
    return 1;
}

static __used__ int EMIT_dcba(struct PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 0x03e00001) return -1;

    uint8_t ra = (opcode >> 21) & 31;
    uint8_t rb = (opcode >> 16) & 31;
    uint8_t reg_rb = MapGPRForRead(tc, rb);
    uint8_t base;

    if (ra == 0) {
        base = reg_rb;
    } else {
        uint8_t reg_ra = MapGPRForRead(tc, ra);
        base = AllocARMRegister(tc);
        tc->EMIT(add_reg(base, reg_ra, reg_rb, LSL, 0));
    }

    tc->EMIT({
        prfm_pst(base)
    });

    if (ra != 0)
        FreeARMRegister(tc, base);

    tc->tc_PPCCodePtr++;
    tc->AdvancePC(4);
    return 1;
}

static __used__ int EMIT_dcbi(struct PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 0x03e00001) return -1;

    uint8_t ra = (opcode >> 21) & 31;
    uint8_t rb = (opcode >> 16) & 31;
    uint8_t reg_rb = MapGPRForRead(tc, rb);
    uint8_t base;

    uint8_t ctx = GetCTX(tc);
    uint8_t tmp = AllocARMRegister(tc);

    /* 
        This fetches must not be context synchronizing and one supervisor check per
        code block is sufficient.
    */
    if (!tc->tc_SupervisorChecked)
    {
        /* We need to flush PC now, just in case */
        tc->FlushPC();

        /* 
            Fetch MSR and check if exceptions are enabled - it is illegal to call
            RFI from user
        */
        tc->EMIT({
            ldr_offset(ctx, tmp, __builtin_offsetof(PPCState, MSR)),
            tbnz(tmp, 14, 0)
        });
    }

    uint32_t fixup_type = FIXUP_TBZ;
    uint32_t *jump_location = tc->tc_CodePtr - 1;

    if (ra == 0) {
        base = reg_rb;
    } else {
        uint8_t reg_ra = MapGPRForRead(tc, ra);
        base = AllocARMRegister(tc);
        tc->EMIT(add_reg(base, reg_ra, reg_rb, LSL, 0));
    }

    tc->EMIT({
        dc_ivac(base)
    });

    if (ra != 0)
        FreeARMRegister(tc, base);

    if (!tc->tc_SupervisorChecked)
    {
        /* Now insert the program exception path - raise exception if RFI was not allowed */
        uint32_t *exit_code_start = tc->tc_CodePtr;
        tc->EMIT_Exception(0x700);
        uint32_t *exit_code_end = tc->tc_CodePtr;

        /* Insert fixup location */
        tc->EMIT({ 
            (uint32_t)(exit_code_end - jump_location),
            fixup_type,
            1,
            (uint32_t)(exit_code_end - exit_code_start),
            INSN_TO_LE(MARKER_EXIT_BLOCK)
        });

        tc->tc_SupervisorChecked = true;
    }
    
    tc->tc_PPCCodePtr++;
    tc->AdvancePC(4);
    return 1;
}

static __used__ int EMIT_mtcrf(struct PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 0x00100801) return -1;

    uint8_t rs = (opcode >> 21) & 31;
    uint8_t mask = (opcode >> 12) & 255;

    uint8_t reg_rs = MapGPRForRead(tc, rs);
    uint8_t reg_cr = MapGPRForReadAndWrite(tc, CRn);

    if (mask == 0xff) {
        tc->EMIT( mov_reg(reg_cr, reg_rs));
    } else if (mask == 0) {
        tc->EMIT( mov_reg(reg_cr, WZR));
    } else {
        uint8_t tmp = AllocARMRegister(tc);
        uint32_t mask32 = 0;
        uint32_t encoded;

        for (int i=0; i < 8; i++) {
            if (mask & (1 << i)) {
                mask32 |= 15 << (4 * i);
            }
        }

        encoded = number_to_mask(mask32);

        if (encoded == 0) {
            uint32_t imm = AllocARMRegister(tc);

            tc->LoadImmediate(imm, mask32);
            tc->EMIT({
                bic_reg(reg_cr, reg_cr, imm, LSL, 0),
                and_reg(tmp, tmp, imm, LSL, 0),
                orr_reg(reg_cr, reg_cr, tmp, LSL, 0)
            });

            FreeARMRegister(tc, imm);
        } else {
            tc->EMIT({
                bic_immed(reg_cr, reg_cr, (encoded >> 16) & 0x3f, encoded & 0x3f),
                and_immed(tmp, tmp, (encoded >> 16) & 0x3f, encoded & 0x3f),
                orr_reg(reg_cr, reg_cr, tmp, LSL, 0)
            });
        }

        FreeARMRegister(tc, tmp);
    }

    tc->tc_PPCCodePtr++;
    tc->AdvancePC(4);
    return 1;
}

static __used__ int EMIT_sc(struct PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode != 0x44000002) return -1;

    /* Advance program counter by 4 */
    tc->AdvancePC(4);

    /* Flush it - this is a point of no return */
    tc->FlushPC();

    /* Emit exception itself */
    uint8_t ctx = GetCTX(tc);
    uint8_t tmp = AllocARMRegister(tc);

    tc->EMIT({
        /* Store MSR into SRR1, Store PC into SRR0 */
        ldr_offset(ctx, tmp, __builtin_offsetof(PPCState, MSR)),
        str_offset(ctx, tmp, __builtin_offsetof(PPCState, SRR1)),
        str_offset(ctx, REG_PC, __builtin_offsetof(PPCState, SRR0)),
        
        /* Set supervisor mode in MSR */
        bic_immed(tmp, tmp, 1, 18),

        /* Load new program counter */
        mov_immed_u16(REG_PC, 0xc00, 0),
        movk_immed_u16(REG_PC, 0xfff0, 1),

        /* Store updated MSR */
        str_offset(ctx, tmp, __builtin_offsetof(PPCState, MSR))
    });

    FreeARMRegister(tc, tmp);

    tc->STOP();

    return 1;
}

static __used__ int EMIT_subfex(struct PPCTranslatorContext *tc, uint32_t opcode)
{
    uint8_t rd = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;
    uint8_t rc = opcode & 1;
    uint8_t oe = (opcode >> 10) & 1;

    uint8_t reg_xer = MapGPRForReadAndWrite(tc, XERn);

    /* Shortcut - subfe rX, rX, rX gives the CA flag in rX */
    if (rc == 0 && oe == 0 && rd == ra && rd == rb) {
        uint8_t reg_rd = MapGPRForWrite(tc, rd);

        tc->EMIT({
            tst_immed(reg_xer, 1, 3),
            csetm(reg_rd, A64_CC_EQ)
        });

        tc->tc_PPCCodePtr++;
        tc->AdvancePC(4);
        return 1;
    }

    return -1;
}

static __used__ int EMIT_rfi(struct PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode != 0x4c000064) return -1;

    uint8_t ctx = GetCTX(tc);
    uint8_t tmp = AllocARMRegister(tc);

    /* 
        Fetch MSR and check if exceptions are enabled - it is illegal to call
        RFI from user
    */
    tc->EMIT({
        ldr_offset(ctx, tmp, __builtin_offsetof(PPCState, MSR)),
        tbnz(tmp, 14, 0)
    });

    /* Emit jump, remember its location and fixup type */
    uint32_t fixup_type = FIXUP_TBZ;
    uint32_t *jump_location = tc->tc_CodePtr - 1;

    tc->EMIT({
        /* Load SRR1, clear POW bit, store into MSR */
        ldr_offset(ctx, tmp, __builtin_offsetof(PPCState, SRR1)),
        bic_immed(tmp, tmp, 1, 14),
        str_offset(ctx, tmp, __builtin_offsetof(PPCState, MSR)),

        /* Load SRR0, store into PC */
        ldr_offset(ctx, REG_PC, __builtin_offsetof(PPCState, SRR0)),
        bic_immed(REG_PC, REG_PC, 2, 0),
    });

    FreeARMRegister(tc, tmp);

    /* This instruction exits the JIT loop */
    tc->STOP();

    /* Now insert the program exception path - raise exception if RFI was not allowed */
    uint32_t *exit_code_start = tc->tc_CodePtr;
    tc->EMIT_Exception(0x700);
    uint32_t *exit_code_end = tc->tc_CodePtr;

    /* Insert fixup location */
    tc->EMIT({ 
        (uint32_t)(exit_code_end - jump_location),
        fixup_type,
        1,
        (uint32_t)(exit_code_end - exit_code_start),
        INSN_TO_LE(MARKER_EXIT_BLOCK)
    });

    return 1;
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

static __used__ int EMIT_fsubx(struct PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 0x000007c0) return -1;

    uint8_t rd = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;
    uint8_t rc = opcode & 1;

    uint8_t reg_ra = MapFPRForRead(tc, ra);
    uint8_t reg_rb = MapFPRForRead(tc, rb);
    uint8_t reg_rd = MapFPRForWrite(tc, rd);

    if (rc) {
        kprintf("fsub. not supported yet!");
        return -1;
    }

    tc->EMIT(fsubd(reg_rd, reg_ra, reg_rb));

    tc->tc_PPCCodePtr++;
    tc->AdvancePC(4);
    return 1;
}

static __used__ int EMIT_faddx(struct PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 0x000007c0) return -1;

    uint8_t rd = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;
    uint8_t rc = opcode & 1;

    uint8_t reg_ra = MapFPRForRead(tc, ra);
    uint8_t reg_rb = MapFPRForRead(tc, rb);
    uint8_t reg_rd = MapFPRForWrite(tc, rd);

    if (rc) {
        kprintf("fadd. not supported yet!");
        return -1;
    }

    tc->EMIT(faddd(reg_rd, reg_ra, reg_rb));

    tc->tc_PPCCodePtr++;
    tc->AdvancePC(4);
    return 1;
}

static __used__ int EMIT_fmulx(struct PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 0x0000f800) return -1;

    uint8_t rd = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 6) & 31;
    uint8_t rc = opcode & 1;

    uint8_t reg_ra = MapFPRForRead(tc, ra);
    uint8_t reg_rb = MapFPRForRead(tc, rb);
    uint8_t reg_rd = MapFPRForWrite(tc, rd);

    if (rc) {
        kprintf("fmul. not supported yet!");
        return -1;
    }

    tc->EMIT(fmuld(reg_rd, reg_ra, reg_rb));

    tc->tc_PPCCodePtr++;
    tc->AdvancePC(4);
    return 1;
}

static __used__ int EMIT_fdivx(struct PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 0x000007c0) return -1;

    uint8_t rd = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;
    uint8_t rc = opcode & 1;

    uint8_t reg_ra = MapFPRForRead(tc, ra);
    uint8_t reg_rb = MapFPRForRead(tc, rb);
    uint8_t reg_rd = MapFPRForWrite(tc, rd);

    if (rc) {
        kprintf("fdiv. not supported yet!");
        return -1;
    }

    tc->EMIT(fdivd(reg_rd, reg_ra, reg_rb));

    tc->tc_PPCCodePtr++;
    tc->AdvancePC(4);
    return 1;
}

static __used__ int EMIT_fctiwzx(struct PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 0x001f07c0) return -1;

    uint8_t rd = (opcode >> 21) & 31;
    uint8_t rb = (opcode >> 11) & 31;
    uint8_t rc = opcode & 1;

    uint8_t reg_rb = MapFPRForRead(tc, rb);
    uint8_t reg_rd = MapFPRForWrite(tc, rd);
    uint8_t tmp = AllocARMRegister(tc);

    if (rc) {
        kprintf("fctiwzx. not supported yet!");
        return -1;
    }

    tc->EMIT({
        fcvtzs_Dto32(tmp, reg_rb),
        mov_reg_to_simd(reg_rd, TS_S, 0, tmp)
    });

    FreeARMRegister(tc, tmp);

    tc->tc_PPCCodePtr++;
    tc->AdvancePC(4);
    return 1;
}

static __used__ int EMIT_fmrx(struct PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 0x001f0000) return -1;

    uint8_t rd = (opcode >> 21) & 31;
    uint8_t rb = (opcode >> 11) & 31;
    uint8_t rc = opcode & 1;

    uint8_t reg_rb = MapFPRForRead(tc, rb);
    uint8_t reg_rd = MapFPRForWrite(tc, rd);

    if (rc) {
        kprintf("fmr. not supported yet!");
        return -1;
    }

    tc->EMIT(fcpyd(reg_rd, reg_rb));

    tc->tc_PPCCodePtr++;
    tc->AdvancePC(4);
    return 1;
}

static __used__ int EMIT_fcmpu(struct PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 0x00600001) return -1;

    uint8_t crn = (opcode >> 23) & 7;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;

    uint8_t reg_rb = MapFPRForRead(tc, rb);
    uint8_t reg_ra = MapFPRForRead(tc, ra);

    tc->EMIT(fcmpd(reg_ra, reg_rb));

    EMIT_set_crn_signed(tc, crn);

    tc->tc_PPCCodePtr++;
    tc->AdvancePC(4);
    return 1;
}

static __used__ int EMIT_fmadd(struct PPCTranslatorContext *tc, uint32_t opcode)
{
    uint8_t rd = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;
    uint8_t rc = (opcode >> 6) & 31;
    uint8_t c = opcode & 1;

    uint8_t reg_ra = MapFPRForRead(tc, ra);
    uint8_t reg_rb = MapFPRForRead(tc, rb);
    uint8_t reg_rc = MapFPRForRead(tc, rc);
    uint8_t reg_rd = MapFPRForWrite(tc, rd);

    if (c) {
        kprintf("fmadd. not supported yet!");
        return -1;
    }

    tc->EMIT(fmaddd(reg_rd, reg_ra, reg_rc, reg_rb));

    tc->tc_PPCCodePtr++;
    tc->AdvancePC(4);
    return 1;
}

static inline int EMIT_Group_63(struct PPCTranslatorContext *tc, uint32_t opcode)
{
    uint32_t secondary_short = (opcode >> 1) & 0x1f;
    uint32_t secondary_long = (opcode >> 1) & 0x3ff;

    switch (secondary_long) {
        case 0b0000000000: return EMIT_fcmpu(tc, opcode);
        case 0b0001001000: return EMIT_fmrx(tc, opcode);
    }

    switch (secondary_short) {
        case 0b10100: return EMIT_fsubx(tc, opcode);
        case 0b10101: return EMIT_faddx(tc, opcode);
        case 0b11001: return EMIT_fmulx(tc, opcode);
        case 0b10010: return EMIT_fdivx(tc, opcode);
        case 0b01111: return EMIT_fctiwzx(tc, opcode);
        case 0b11101: return EMIT_fmadd(tc, opcode);
    }

    return -1;
}

static inline int EMIT_Group_19(struct PPCTranslatorContext *tc, uint32_t opcode)
{
    uint32_t secondary = (opcode >> 1) & 0x3ff;

    switch (secondary) {
        case 0b0000000000: return EMIT_mcrf(tc, opcode);
        case 0b0000010000: return EMIT_bclrx(tc, opcode);
        case 0b0000100001: return EMIT_crnor(tc, opcode);
        case 0b0000110010: return EMIT_rfi(tc, opcode);
        case 0b0010000001: return EMIT_crandc(tc, opcode);
        case 0b0010010110: return EMIT_isync(tc, opcode);
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

static inline int EMIT_Group_31(struct PPCTranslatorContext *tc, uint32_t opcode)
{
    uint32_t secondary = (opcode >> 1) & 0x3ff;

    switch (secondary) {
        case 0b0000000000: return EMIT_cmp(tc, opcode);
        case 0b0000000100: return EMIT_tw(tc, opcode);
        case 0b0000001000: return EMIT_subfcx(tc, opcode);
        case 0b0000001010: return EMIT_addcx(tc, opcode);
        case 0b0000001011: return EMIT_mulhwux(tc, opcode);
        case 0b0000010011: return EMIT_mfcr(tc, opcode);
        case 0b0000010100: return EMIT_lwarx(tc, opcode);
        case 0b0000010111: return EMIT_lwzx(tc, opcode);
        case 0b0000011000: return EMIT_slwx(tc, opcode);
        case 0b0000011010: return EMIT_cntlzwx(tc, opcode);
        case 0b0000011100: return EMIT_andx(tc, opcode);
        case 0b0000100000: return EMIT_cmpl(tc, opcode);
        case 0b0000101000: return EMIT_subfx(tc, opcode);
        case 0b0000110110: return EMIT_dcbst(tc, opcode);
        case 0b0000110111: return EMIT_lwzux(tc, opcode);
        case 0b0000111100: return EMIT_andcx(tc, opcode);
        case 0b0001001011: return EMIT_mulhwx(tc, opcode);
        case 0b0001010011: return EMIT_mfmsr(tc, opcode);     // OEA, supervisor
        case 0b0001010110: return EMIT_dcbf(tc, opcode);
        case 0b0001010111: return EMIT_lbzx(tc, opcode);
        case 0b0001101000: return EMIT_negx(tc, opcode);
        case 0b0001110111: return EMIT_lbzux(tc, opcode);
        case 0b0001111100: return EMIT_norx(tc, opcode);
        case 0b0010001000: return EMIT_subfex(tc, opcode);
        case 0b0010001010: return EMIT_addex(tc, opcode);
        case 0b0010010000: return EMIT_mtcrf(tc, opcode);
        case 0b0010010010: return EMIT_mtmsr(tc, opcode);     // OEA, supervisor
        case 0b0010010110: return EMIT_stwcx_dot(tc, opcode);
        case 0b0010010111: return EMIT_stwx(tc, opcode);
        case 0b0010110111: return EMIT_stwux(tc, opcode);
        //case 0b0011001000: return EMIT_subfzex(tc, opcode);
        //case 0b0011001010: return EMIT_addzex(tc, opcode);
        //case 0b0011010010: return EMIT_mtsr(tc, opcode);      // OEA, supervisor
        case 0b0011010111: return EMIT_stbx(tc, opcode);
        //case 0b0011101000: return EMIT_subfmex(tc, opcode);
        //case 0b0011101010: return EMIT_addmex(tc, opcode);
        case 0b0011101011: return EMIT_mullwx(tc, opcode);
        //case 0b0011110010: return EMIT_mtsrin(tc, opcode);    // OEA, supervisor
        case 0b0011110110: return EMIT_dcbtst(tc, opcode);    // VEA
        case 0b0011110111: return EMIT_stbux(tc, opcode);
        case 0b0100001010: return EMIT_addx(tc, opcode);
        case 0b0100010110: return EMIT_dcbt(tc, opcode);      // VEA
        case 0b0100010111: return EMIT_lhzx(tc, opcode);
        case 0b0100011100: return EMIT_eqvx(tc, opcode);
        //case 0b0100110010: return EMIT_tlbie(tc, opcode);     // OEA, supervisor, optional
        //case 0b0100110110: return EMIT_eciwx(tc, opcode);     // optional
        case 0b0100110111: return EMIT_lhzux(tc, opcode);
        case 0b0100111100: return EMIT_xorx(tc, opcode);
        case 0b0101010011: return EMIT_mfspr(tc, opcode);
        case 0b0101010111: return EMIT_lhax(tc, opcode);
        //case 0b0101110010: return EMIT_tlbia(tc, opcode);     // OEA, supervisor, optional
        case 0b0101110011: return EMIT_mftb(tc, opcode);
        case 0b0101110111: return EMIT_lhaux(tc, opcode);
        case 0b0110010111: return EMIT_sthx(tc, opcode);
        case 0b0110011100: return EMIT_orcx(tc, opcode);
        //case 0b0110110110: return EMIT_ecowx(tc, opcode);     // optional
        case 0b0110110111: return EMIT_sthux(tc, opcode);
        case 0b0110111100: return EMIT_orx(tc, opcode);
        case 0b0111001011: return EMIT_divwux(tc, opcode);
        case 0b0111010011: return EMIT_mtspr(tc, opcode);
        case 0b0111010110: return EMIT_dcbi(tc, opcode);      // VEA, supervisor
        case 0b0111011100: return EMIT_nandx(tc, opcode);
        case 0b0111101011: return EMIT_divwx(tc, opcode);
        case 0b1000000000: return EMIT_mcrxr(tc, opcode);
        //case 0b1000010101: return EMIT_lswx(tc, opcode);
        case 0b1000010110: return EMIT_lwbrx(tc, opcode);
        case 0b1000011000: return EMIT_srwx(tc, opcode);
        //case 0b1000110110: return EMIT_tlbsync(tc, opcode);   // OEA, supervisor, optional
        //case 0b1001010011: return EMIT_mfsr(tc, opcode);      // OEA, supervisor
        //case 0b1001010101: return EMIT_lswi(tc, opcode);
        case 0b1001010110: return EMIT_sync(tc, opcode);
        //case 0b1010010011: return EMIT_mfsrin(tc, opcode);    // OEA
        //case 0b1010010101: return EMIT_stswx(tc, opcode);
        case 0b1010010110: return EMIT_stwbrx(tc, opcode);
        case 0b1011110110: return EMIT_dcba(tc, opcode);      // VEA, optional
        case 0b1100010110: return EMIT_lhbrx(tc, opcode);
        case 0b1100011000: return EMIT_srawx(tc, opcode);
        case 0b1100111000: return EMIT_srawix(tc, opcode);
        case 0b1101010110: return EMIT_eieio(tc, opcode);
        case 0b1110010110: return EMIT_sthbrx(tc, opcode);
        case 0b1110011010: return EMIT_extshx(tc, opcode);
        case 0b1110111010: return EMIT_extsbx(tc, opcode);
        case 0b1111010110: return EMIT_icbi(tc, opcode);
        case 0b1111110110: return EMIT_dcbz(tc, opcode);      // VEA

        /* FPU part */
        //case 0b1000010111: return EMIT_lfsx(tc, opcode);      // FPU
        //case 0b1000110111: return EMIT_lfsux(tc, opcode);     // FPU
        //case 0b1001010111: return EMIT_lfdx(tc, opcode);      // FPU
        //case 0b1001110111: return EMIT_lfdux(tc, opcode);     // FPU
        case 0b1111010111: return EMIT_stfiwx(tc, opcode);    // FPU
        //case 0b1011110111: return EMIT_stfdux(tc, opcode);    // FPU
        //case 0b1010010111: return EMIT_stfsx(tc, opcode);     // FPU
        //case 0b1010110111: return EMIT_stfsux(tc, opcode);    // FPU
        //case 0b1011010101: return EMIT_stswi(tc, opcode);     // FPU
        //case 0b1011010111: return EMIT_stfdx(tc, opcode);     // FPU
        
        default: return -1;
    }
}

static inline int EmitINSN(struct PPCTranslatorContext *tc)
{
    uint32_t opcode = cache_read_32(ICACHE, (uint32_t)(uintptr_t)tc->tc_PPCCodePtr);
    uint8_t group = opcode >> 26;
    int count = -1;

    //kprintf("[PPC] EmitINSN @ %08x, opcode %08x, group %d\n", (uint32_t)(uintptr_t)tc->tc_PPCCodePtr, opcode, group);

    switch (group) {
        case 0b000011: count = EMIT_twi(tc, opcode); break;
        case 0b000111: count = EMIT_mulli(tc, opcode); break;
        case 0b001000: count = EMIT_subfic(tc, opcode); break;
        case 0b001010: count = EMIT_cmpli(tc, opcode); break;
        case 0b001011: count = EMIT_cmpi(tc, opcode); break;
        case 0b001100: count = EMIT_addic(tc, opcode); break;
        case 0b001101: count = EMIT_addic_dot(tc, opcode); break;
        case 0b001110: count = EMIT_addi(tc, opcode); break;
        case 0b001111: count = EMIT_addis(tc, opcode); break;
        case 0b010000: count = EMIT_bcx(tc, opcode); break;
        case 0b010001: count = EMIT_sc(tc, opcode); break;
        case 0b010010: count = EMIT_bx(tc, opcode); break;
        case 0b010011: count = EMIT_Group_19(tc, opcode); break;
        case 0b010100: count = EMIT_rlwimix(tc, opcode); break;
        case 0b010101: count = EMIT_rlwinmx(tc, opcode); break;
        case 0b010111: count = EMIT_rlwnmx(tc, opcode); break;
        case 0b011000: count = EMIT_ori(tc, opcode); break;
        case 0b011001: count = EMIT_oris(tc, opcode); break;
        case 0b011010: count = EMIT_xori(tc, opcode); break;
        case 0b011011: count = EMIT_xoris(tc, opcode); break;
        case 0b011100: count = EMIT_andi_dot(tc, opcode); break;
        case 0b011101: count = EMIT_andis_dot(tc, opcode); break;
        case 0b011111: count = EMIT_Group_31(tc, opcode); break;
        case 0b100000: count = EMIT_lwz(tc, opcode); break;
        case 0b100001: count = EMIT_lwzu(tc, opcode); break;
        case 0b100010: count = EMIT_lbz(tc, opcode); break;
        case 0b100011: count = EMIT_lbzu(tc, opcode); break;
        case 0b100100: count = EMIT_stw(tc, opcode); break;
        case 0b100101: count = EMIT_stwu(tc, opcode); break;
        case 0b100110: count = EMIT_stb(tc, opcode); break;
        case 0b100111: count = EMIT_stbu(tc, opcode); break;
        case 0b101000: count = EMIT_lhz(tc, opcode); break;
        case 0b101001: count = EMIT_lhzu(tc, opcode); break;
        case 0b101010: count = EMIT_lha(tc, opcode); break;
        case 0b101011: count = EMIT_lhau(tc, opcode); break;
        case 0b101100: count = EMIT_sth(tc, opcode); break;
        case 0b101101: count = EMIT_sthu(tc, opcode); break;
        //case 0b101110: count = EMIT_lmw(tc, opcode); break;   // Need to be interpreted
        //case 0b101111: count = EMIT_stmw(tc, opcode); break;  // Need to be interpreted
        case 0b110000: count = EMIT_lfs(tc, opcode); break;
        //case 0b110001: count = EMIT_lfsu(tc, opcode); break;
        case 0b110010: count = EMIT_lfd(tc, opcode); break;
        //case 0b110011: count = EMIT_lfdu(tc, opcode); break;
        case 0b110100: count = EMIT_stfs(tc, opcode); break;
        //case 0b110101: count = EMIT_stfsu(tc, opcode); break;
        case 0b110110: count = EMIT_stfd(tc, opcode); break;
        //case 0b110111: count = EMIT_stfdu(tc, opcode); break;
        //case 0b111011: count = EMIT_Group_59(tc, opcode); break;
        case 0b111111: count = EMIT_Group_63(tc, opcode); break;
        default: break;
    }

    if (count < 1) {
        kprintf("[PPC] UNIMPLEMENTED %08x @ %08x...\n", opcode, (uint32_t)(uintptr_t)tc->tc_PPCCodePtr);

        if (!disasm) {
            disasm_open();
        }

        disasm_print_ppc_only(tc->tc_PPCCodePtr);

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

static inline uintptr_t PPC_Translate(uint32_t *PPCCodePtr)
{
    Emu68::List<ExitBlock> exitList;
    struct PPCState *ctx = getHostCTX();
    ppc_entry_point = PPCCodePtr;
    uint32_t *orig_ppccodeptr = PPCCodePtr;
    uintptr_t hash = (uintptr_t)PPCCodePtr;

    int var_EMU68_MAX_LOOP_COUNT = (ctx->JIT_CONTROL >> JCCB_LOOP_COUNT) & JCCB_LOOP_COUNT_MASK;
    if (var_EMU68_MAX_LOOP_COUNT == 0)
        var_EMU68_MAX_LOOP_COUNT = JCCB_LOOP_COUNT_MASK + 1;
    uint32_t var_EMU68_PPC_INSN_DEPTH = (ctx->JIT_CONTROL >> JCCB_INSN_DEPTH) & JCCB_INSN_DEPTH_MASK;
    if (var_EMU68_PPC_INSN_DEPTH == 0)
        var_EMU68_PPC_INSN_DEPTH = JCCB_INSN_DEPTH_MASK + 1;

    PPCTranslatorContext tc;

    tc.tc_CodePtr = temporary_arm_code;
    tc.tc_CodeStart = temporary_arm_code;
    tc.tc_PPCCodePtr = PPCCodePtr;
    tc.tc_PPCCodeStart = PPCCodePtr;
    tc.tc_SupervisorChecked = false;

    uint32_t *last_rev_jump = (uint32_t *)0xffffffff;

    disasm_ptr = disasm_items;

    int debug = 0;
    int disasm = 0;

    if ((uint32_t)(uintptr_t)PPCCodePtr >= debug_range_min && (uint32_t)(uintptr_t)PPCCodePtr <= debug_range_max) {
        debug = globalDebug();
        disasm = globalDisasm();
    }

    if (!GPR_LRU.isEmpty()) {
        kprintf("[PPC] GPR_LRU list is not empty!\n");
        while(1);
    }

    if (!FPR_LRU.isEmpty()) {
        kprintf("[PPC] FPR_LRU list is not empty!\n");
        while(1);
    }

    if (GetTempAllocMask(&tc)) {
        kprintf("[PPC] Temporary register alloc mask on translate start is non-zero %x\n", GetTempAllocMask(&tc));
        while(1);
    }

    if (disasm) {
        disasm_open();
    }

    ResetReturnStack();

    if (debug) {
        uint32_t hash_calc = (hash >> EMU68_HASHSHIFT) & EMU68_HASHMASK;
        kprintf("[PPC] Creating new translation unit with hash %04x (PPC code @ %p)\n", hash_calc, (void*)PPCCodePtr);
    }

    insn_count = 0;

    int break_loop = FALSE;
    int inner_loop = FALSE;
    int soft_break = FALSE;
    int max_rev_jumps = 0;

    ppc_low = tc.tc_PPCCodePtr;
    ppc_high = tc.tc_PPCCodePtr + 16;

    int inner_loop_length = 0;
    int inner_loop_limit = 0;

    while (break_loop == FALSE && soft_break == FALSE && insn_count < var_EMU68_PPC_INSN_DEPTH)
    {
        uint16_t insn_consumed;
        uint32_t * const in_code = tc.tc_PPCCodePtr;
        uint32_t * const out_code = tc.tc_CodePtr;

        if (insn_count && ((uintptr_t)tc.tc_PPCCodePtr < (uintptr_t)local_state[insn_count-1].pls_PPCPtr))
        {
            int found = -1;

            for (int i=insn_count - 1; i >= 0; --i)
            {
                if (local_state[i].pls_PPCPtr == tc.tc_PPCCodePtr)
                {
                    found = i;
                    break;
                }
            }

            if (found > 0)
            {
                if ((insn_count - found - 1) > (var_EMU68_PPC_INSN_DEPTH - insn_count))
                {
                    break;
                }
            }
        }

        local_state[insn_count].pls_ARMOffset = tc.tc_CodePtr - tc.tc_CodeStart;
        local_state[insn_count].pls_PPCPtr = tc.tc_PPCCodePtr;
        local_state[insn_count].pls_PCRel = tc._pc_rel;
        for (unsigned i=0; i < sizeof(INT_REG_MAPPING); i++)
        {
            uint8_t map = INT_REG_MAPPING[i];
            if (map == 0xff) {
                for(auto rn: GPR_LRU)
                {
                    if (rn->rn_RegNum == i) {
                        map = rn->rn_RegNum;
                        if (rn->rn_Dirty) map |= 0x80;
                        break;
                    }
                }
            }
            local_state[insn_count].pls_RegMap[i] = map;
        }

        insn_consumed = EmitINSN(&tc);

        if (tc.tc_PPCCodePtr < ppc_low)
            ppc_low = tc.tc_PPCCodePtr;
        if (tc.tc_PPCCodePtr + 16 > ppc_high)
            ppc_high = tc.tc_PPCCodePtr + 16;

        insn_count+=insn_consumed;
        
        int process_markers = 1;

        while(process_markers)
        {
            if (tc.tc_CodePtr[-1] == INSN_TO_LE(MARKER_STOP))
            {
                tc.tc_CodePtr--;
                break_loop = TRUE;
            }
            else if (tc.tc_CodePtr[-1] == INSN_TO_LE(MARKER_BREAK))
            {
                tc.tc_CodePtr--;
                soft_break = TRUE;
            }
            else if (tc.tc_CodePtr[-1] == INSN_TO_LE(MARKER_EXIT_BLOCK))
            {
                struct ExitBlock *eb;
                
                tc.tc_CodePtr -= 3;

                uint32_t insn_count = tc.tc_CodePtr[1];
                uint32_t fixup_count = tc.tc_CodePtr[0];

                tc.tc_CodePtr -= 2 * fixup_count;

                eb = (ExitBlock *)tlsf_malloc(tlsf, sizeof(struct ExitBlock) + 4 * insn_count + sizeof(eb->eb_Fixup[0]) * fixup_count);

                eb->eb_FixupCount = fixup_count;
                eb->eb_InstructionCount = insn_count;
                eb->eb_ARMCode = (uint32_t *)((uintptr_t)eb + sizeof(ExitBlock));
                eb->eb_Fixup = (typeof(eb->eb_Fixup[0]) *)((uintptr_t)eb + sizeof(ExitBlock) + 4 * insn_count);

                for (uint32_t i=0; i < fixup_count; i++) {
                    uint32_t fixup_type = tc.tc_CodePtr[2 * i + 1];
                    uint32_t fixup_target = tc.tc_CodePtr[2 * i];

                    eb->eb_Fixup[i].type = fixup_type;
                    eb->eb_Fixup[i].location = tc.tc_CodePtr - fixup_target;
                }

                tc.tc_CodePtr -= insn_count;

                for (unsigned i=0; i < insn_count; i++) {
                    eb->eb_ARMCode[i] = tc.tc_CodePtr[i];
                }

                exitList.addTail(eb);
            }
            else if (tc.tc_CodePtr[-1] == INSN_TO_LE(0xfffffffe))
            {
                uint32_t *tmpptr;
                uint32_t *branch_mod[10];
                uint32_t branch_cnt;
                int local_branch_done = 0;
                tc.tc_CodePtr--;
                tc.tc_CodePtr--;  /* Remove branch target (unused!) */
                branch_cnt = *--tc.tc_CodePtr;

                for (unsigned i=0; i < branch_cnt; i++)
                {
                    uintptr_t ptr = *(uint32_t *)--tc.tc_CodePtr;
                    ptr |= (uintptr_t)tc.tc_CodePtr & 0xffffffff00000000;
                    branch_mod[i] = (uint32_t *)ptr;
                }

                tmpptr = tc.tc_CodePtr;

                if (!local_branch_done)
                {
                    tc.LocalExit(0);
                }
                int distance = tc.tc_CodePtr - tmpptr;

                for (unsigned i=0; i < branch_cnt; i++) {
                    //kprintf("[PPC] Branch modification at %p : distance increase by %d\n", (void*) branch_mod[i], distance);
                    *(branch_mod[i]) = INSN_TO_LE((INSN_TO_LE(*(branch_mod[i])) + (distance << 5)));
                }
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
            disasm_ptr->do_ArmCount = tc.tc_CodePtr - out_code;
            disasm_ptr++;
        }

        if (in_code >= tc.tc_PPCCodePtr)
        {
            if (last_rev_jump == tc.tc_PPCCodePtr) {
                if (--max_rev_jumps == 0) {
                    break;
                }
            }
            else {
                last_rev_jump = tc.tc_PPCCodePtr;
                max_rev_jumps = var_EMU68_MAX_LOOP_COUNT;
            }
        }

        if (!break_loop && (orig_ppccodeptr == tc.tc_PPCCodePtr))
        {
            inner_loop = TRUE;

            if (inner_loop_length == 0) {
                inner_loop_length = insn_count;
                int capacity = var_EMU68_PPC_INSN_DEPTH / insn_count;
                if (capacity > var_EMU68_MAX_LOOP_COUNT) capacity = var_EMU68_MAX_LOOP_COUNT;

                if (capacity <= 1) break;

                inner_loop_limit = capacity - 1;
            } else {
                if (--inner_loop_limit == 0) break;
            }

            if (soft_break) break;
        }
    }

    if (inner_loop && insn_count == 1 && tc.tc_PPCCodePtr[0] == 0x48000000) {
        kprintf("Replacing ARM opcode %08x for the endless PPC loop\n", tc.tc_CodePtr[-1]);
        
        /* This is an endless loop, intentional or not. Change it to WFI/WFE loop to conserve power */
        tc.tc_CodePtr--;
        tc.EMIT(wfe());
    }

    uint32_t *out_code = tc.tc_CodePtr;

#if EMU68_INSN_COUNTER
    uint8_t icnt_reg = AllocARMRegister(&tc);
    tc.EMIT(mov_simd_to_reg(icnt_reg, CTX_INSN_COUNT));
#endif
    FlushAllFPRs(&tc);
    FlushAllGPRs(&tc);
    tc.FlushPC();

#if EMU68_INSN_COUNTER
    tc.EMIT(add64_immed(icnt_reg, icnt_reg, insn_count & 0xfff));
#endif

    uint8_t tmp2 = AllocARMRegister(&tc);
    if (inner_loop)
    {
        uint8_t cpuctx = GetCTX(&tc);
        tc.EMIT(ldr64_offset(cpuctx, tmp2, __builtin_offsetof(struct PPCState, INT64)));
    }

#if EMU68_INSN_COUNTER
    tc.EMIT(mov_reg_to_simd(CTX_INSN_COUNT, icnt_reg));
    FreeARMRegister(&tc, icnt_reg);
#endif

    if (inner_loop)
    {
        uint32_t *tmpptr = tc.tc_CodePtr;
        tc.EMIT(cbz_64(tmp2, tc.tc_CodeStart - tmpptr));
    }
    tc.EMIT(bx_lr());
    
    uint32_t *main_block_end = tc.tc_CodePtr;

    uint32_t *_tmpptr = tc.tc_CodePtr;
    FreeARMRegister(&tc, tmp2);
    FlushCTX(&tc);
    tc.tc_CodePtr = _tmpptr;

    if (disasm) {
        disasm_ptr->do_PPCAddr = nullptr;
        disasm_ptr->do_PPCCount = 0;
        disasm_ptr->do_ArmAddr = out_code;
        disasm_ptr->do_ArmCount = tc.tc_CodePtr - out_code;
        disasm_ptr++;
    }

    /* Get all exit entries and append them here */
    struct ExitBlock *eb = nullptr;
    //int exit_num = 0;
    while ((eb = exitList.remHead()))
    {
        uint32_t *old_end = tc.tc_CodePtr;
        uint32_t op;

        for (unsigned i = 0; i < eb->eb_InstructionCount; i++)
        {
            tc.EMIT(eb->eb_ARMCode[i]);
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
            disasm_ptr->do_ArmCount = tc.tc_CodePtr - old_end;
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
                disasm_ptr->do_ArmAddr, 4 * disasm_ptr->do_ArmCount, temporary_arm_code);
        }
        disasm_close();
    }

    if (debug)
    {
        kprintf("[PPC] Translated %d PPC instructions to %d ARM instructions\n", insn_count, (int)(tc.tc_CodePtr - tc.tc_CodeStart));
        //kprintf("[PPC] Prologue size: %d, Epilogue size: %d, Conditionals: %d\n",
        //    prologue_size, epilogue_size, conditionals_count);
        //kprintf("[PPC]   Mean epilogue size pro exit point: %d\n", epilogue_size / (1 + conditionals_count));
        uint32_t mean = 100 * (main_block_end - tc.tc_CodeStart); // - (prologue_size + epilogue_size));
        mean = mean / insn_count;
        uint32_t mean_n = mean / 100;
        uint32_t mean_f = mean % 100;
        kprintf("[PPC] Mean ARM instructions per PPC instruction: %d.%02d\n", mean_n, mean_f);
    }

    // Put a marker at the end of translation unit
    tc.EMIT(0xffffffff);

    return (uintptr_t)tc.tc_CodePtr - (uintptr_t)tc.tc_CodeStart;
}

/*
    Get PPC code unit from the instruction cache. Return NULL if code was not found and needs to be
    translated first.

    If the code was found, update its position in the LRU cache.
*/
struct PPCTranslationUnit *PPC_GetTranslationUnit(uint32_t *ppccodeptr)
{
    struct PPCState *ctx = getHostCTX();
    struct PPCTranslationUnit *unit = nullptr;
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
    uintptr_t line_length = PPC_Translate(ppccodeptr);
    uintptr_t arm_insn_count = line_length/4 - 1;
    uintptr_t unit_length = (line_length + 63 + sizeof(struct PPCTranslationUnit)) & ~63;
    asm volatile("mrs %0, CNTPCT_EL0":"=r"(time_end));

    do {
        unit = (PPCTranslationUnit *)jit_ppc.malloc_aligned(unit_length, 64);
        
        ctx->JIT_CACHE_FREE = jit_ppc.free_size();

        if (unit == NULL)
        {
            if (debug > 0) {
                kprintf("[PPC] Requested block was %d bytes long\n", sizeof(struct PPCTranslationUnit));
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
    DuffCopy(&unit->ptu_ARMCode[0], temporary_arm_code, line_length/4);

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
    unit->ptu_Epoch = getEPOCH();

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
struct PPCTranslationUnit *PPC_VerifyUnit(struct PPCTranslationUnit *unit)
{
    if (unit)
    {
        uint32_t crc = 0;

        /* Quick path - ROM is always valid as long as we don't use any fancy remapping, at least on Amiga */
        if (unit->ptu_PPCAddress >= 0xfff00000 && unit->ptu_PPCAddress < 0xffff0000) {
            /* Update EPOCH of the unit */
            unit->ptu_Epoch = getEPOCH();

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
            auto ctx = getHostCTX();
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
            unit->ptu_Epoch = getEPOCH();

            /* Move the unit to the beginning of LRU list */
            unit->ptu_LRU.remove();
            LRU.addHead(&unit->ptu_LRU);
        }
    }

    return unit;
}

} // Emu68::PPC
