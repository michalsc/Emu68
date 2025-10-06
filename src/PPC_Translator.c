/*
    Copyright © 2019 Michal Schulz <michal.schulz@gmx.de>
    https://github.com/michalsc

    This Source Code Form is subject to the terms of the
    Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed
    with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#define _GNU_SOURCE 1

#include "PPC.h"
#include "A64.h"
#include "DuffCopy.h"
#include "nodes.h"
#include "config.h"
#include "support.h"
#include "tlsf.h"
#include "disasm.h"
#include "spinlock.h"
#include "doorbell.h"
#include "cache.h"
#include "mmu.h"

register uint32_t PC __asm__("w18");
register void (*ARMCode)() __asm__("x12");

#if 0
/* Disable INSN counter for now (not needed yet) */
#undef EMU68_INSN_COUNTER
#define EMU68_INSN_COUNTER 0
#endif

#define jit_tlsf DO_NOT_USE_jit_tlsf

__attribute__((aligned(4096))) uint8_t ppc_tmp_stack[16384];
__attribute__((aligned(4096))) 
#include "ppc_rom.h"

#define GPR(n)  (n)
#define CRn     32
#define XERn    33
#define LRn     34
#define CTRn    35
#define FPSCRn  36
#define PCn     37

// Mapping for fixed PPC registers, all -1 regs are dynamically allocated
static const uint8_t _int_reg_mapping[] = {
    13, 14, 15, 16, 17, 19, 20, 21, // GPR00 .. GPR07
    22, 23, 24, 25, 26, 27, -1, -1, // GPR08 .. GPR15
    -1, -1, -1, -1, -1, -1, -1, -1, // GPR16 .. GPR23
    -1, -1, -1, -1, -1, -1, -1, -1, // GPR24 .. GPR31
    -1, -1, 28, 29, -1, 18          // CR, XER, LR, CTR, FPSCR, PC
};

#define FPR(n)  (n)

struct RegisterNode {
    struct Node rn_Node;
    uint8_t rn_RegNum;
    uint8_t rn_ARM;
    uint8_t rn_Dirty;
};

#define __used__ __attribute__((used))

static uint32_t ARMTmpPool;
static struct List FreePool;
static struct List GPR_LRU;
static struct List FPR_LRU;
static uint8_t reg_CTX = 0xff;
static struct List ICache[EMU68_HASHSIZE];
static struct List LRU;
static uint32_t *temporary_arm_code;
static void *jit_ppc;
static int32_t _pc_rel = 0;
static uint32_t *ppc_high;
static uint32_t *ppc_low;
static uint32_t insn_count;
static uint32_t * ppc_entry_point;
static uint32_t debug_range_min = 0x00000000;
static uint32_t debug_range_max = 0xffffffff;
static struct PPCLocalState *local_state;

static __used__ void LocalExit(struct TranslatorContext *tc, uint32_t insn_fixup);
static void PPC_PrintContext(struct PPCState *ppc);

static inline struct PPCState *getHostCTX()
{
    struct PPCState *ctx;
    __asm__ volatile("mov %0, "CTX_POINTER_ASM:"=r"(ctx));
    return ctx;
}

static __used__ uint32_t GetTempAllocMask(struct TranslatorContext *)
{
    return ARMTmpPool;
}

static __used__ uint8_t AllocARMRegister(struct TranslatorContext *tc)
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
    struct RegisterNode *rn = (struct RegisterNode *)REMTAIL(&GPR_LRU);

    /* If dirty, store it back to PPC context */
    if (rn->rn_Dirty) {
        /* Store value from ARM register back into PPC context */
        switch(rn->rn_RegNum) {
            case GPR(14): EMIT(tc, mov_reg_to_simd(GPR14, rn->rn_ARM)); break;
            case GPR(15): EMIT(tc, mov_reg_to_simd(GPR15, rn->rn_ARM)); break;
            case GPR(16): EMIT(tc, mov_reg_to_simd(GPR16, rn->rn_ARM)); break;
            case GPR(17): EMIT(tc, mov_reg_to_simd(GPR17, rn->rn_ARM)); break;
            case GPR(18): EMIT(tc, mov_reg_to_simd(GPR18, rn->rn_ARM)); break;
            case GPR(19): EMIT(tc, mov_reg_to_simd(GPR19, rn->rn_ARM)); break;
            case GPR(20): EMIT(tc, mov_reg_to_simd(GPR20, rn->rn_ARM)); break;
            case GPR(21): EMIT(tc, mov_reg_to_simd(GPR21, rn->rn_ARM)); break;
            case GPR(22): EMIT(tc, mov_reg_to_simd(GPR22, rn->rn_ARM)); break;
            case GPR(23): EMIT(tc, mov_reg_to_simd(GPR23, rn->rn_ARM)); break;
            case GPR(24): EMIT(tc, mov_reg_to_simd(GPR24, rn->rn_ARM)); break;
            case GPR(25): EMIT(tc, mov_reg_to_simd(GPR25, rn->rn_ARM)); break;
            case GPR(26): EMIT(tc, mov_reg_to_simd(GPR26, rn->rn_ARM)); break;
            case GPR(27): EMIT(tc, mov_reg_to_simd(GPR27, rn->rn_ARM)); break;
            case GPR(28): EMIT(tc, mov_reg_to_simd(GPR28, rn->rn_ARM)); break;
            case GPR(29): EMIT(tc, mov_reg_to_simd(GPR29, rn->rn_ARM)); break;
            case GPR(30): EMIT(tc, mov_reg_to_simd(GPR30, rn->rn_ARM)); break;
            case GPR(31): EMIT(tc, mov_reg_to_simd(GPR31, rn->rn_ARM)); break;
            case CRn: EMIT(tc, mov_reg_to_simd(REG_CR, rn->rn_ARM)); break;
            case XERn: EMIT(tc, mov_reg_to_simd(REG_XER, rn->rn_ARM)); break;
            case FPSCRn: EMIT(tc, mov_reg_to_simd(REG_FPSCR, rn->rn_ARM)); break;
            default: kprintf("[PPC] Illegal reg %d in IntMapGpr()\n", rn->rn_ARM);
        }
    }

    ADDTAIL(&FreePool, rn);

    return rn->rn_ARM;
}

static __used__ void FreeARMRegister(struct TranslatorContext *, uint8_t arm_reg)
{
    if (arm_reg > 11)
        return;

    ARMTmpPool &= ~(1 << arm_reg);
}

static __used__ uint8_t TryCTX(struct TranslatorContext *)
{
    return reg_CTX;
}

static __used__ uint8_t GetCTX(struct TranslatorContext *ctx)
{
    if (reg_CTX == 0xff)
    {
        reg_CTX = AllocARMRegister(ctx);
        EMIT(ctx, mov_simd_to_reg(reg_CTX, CTX_POINTER));
    }

    return reg_CTX;
}

static __used__ void FlushCTX(struct TranslatorContext *ctx)
{
    if (reg_CTX != 0xff)
    {
        FreeARMRegister(ctx, reg_CTX);
    }

    reg_CTX = 0xff;
}

static __used__ uint8_t IntMapGPR(struct TranslatorContext *tc, uint8_t reg, int load, int set_dirty)
{
    struct RegisterNode *rn, *next;

    /* If register is a fixed-assigned one, return ASAP */
    if (_int_reg_mapping[reg] != 0xff) {
        return _int_reg_mapping[reg];
    }

    /* Check if register is already in LRU */
    ForeachNodeSafe(&GPR_LRU, rn, next)
    {
        if (rn->rn_RegNum == reg)
        {
            /* Found it, move it to the top of LRU, return ARM reg number */
            if (GetHead(&GPR_LRU) != &rn->rn_Node) {
                REMOVE(&rn->rn_Node);
                ADDHEAD(&GPR_LRU, rn);
            }
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
        rn = (struct RegisterNode *)REMHEAD(&FreePool);

        /* Update values in RegisterNode */
        rn->rn_Dirty = set_dirty;
        rn->rn_ARM = arm_reg;
        rn->rn_RegNum = reg;

        if (load) {
            /* Load value from PPC context into ARM register */
            switch(reg) {
                case GPR(14): EMIT(tc, mov_simd_to_reg(arm_reg, GPR14)); break;
                case GPR(15): EMIT(tc, mov_simd_to_reg(arm_reg, GPR15)); break;
                case GPR(16): EMIT(tc, mov_simd_to_reg(arm_reg, GPR16)); break;
                case GPR(17): EMIT(tc, mov_simd_to_reg(arm_reg, GPR17)); break;
                case GPR(18): EMIT(tc, mov_simd_to_reg(arm_reg, GPR18)); break;
                case GPR(19): EMIT(tc, mov_simd_to_reg(arm_reg, GPR19)); break;
                case GPR(20): EMIT(tc, mov_simd_to_reg(arm_reg, GPR20)); break;
                case GPR(21): EMIT(tc, mov_simd_to_reg(arm_reg, GPR21)); break;
                case GPR(22): EMIT(tc, mov_simd_to_reg(arm_reg, GPR22)); break;
                case GPR(23): EMIT(tc, mov_simd_to_reg(arm_reg, GPR23)); break;
                case GPR(24): EMIT(tc, mov_simd_to_reg(arm_reg, GPR24)); break;
                case GPR(25): EMIT(tc, mov_simd_to_reg(arm_reg, GPR25)); break;
                case GPR(26): EMIT(tc, mov_simd_to_reg(arm_reg, GPR26)); break;
                case GPR(27): EMIT(tc, mov_simd_to_reg(arm_reg, GPR27)); break;
                case GPR(28): EMIT(tc, mov_simd_to_reg(arm_reg, GPR28)); break;
                case GPR(29): EMIT(tc, mov_simd_to_reg(arm_reg, GPR29)); break;
                case GPR(30): EMIT(tc, mov_simd_to_reg(arm_reg, GPR30)); break;
                case GPR(31): EMIT(tc, mov_simd_to_reg(arm_reg, GPR31)); break;
                case CRn: EMIT(tc, mov_simd_to_reg(arm_reg, REG_CR)); break;
                case XERn: EMIT(tc, mov_simd_to_reg(arm_reg, REG_XER)); break;
                case FPSCRn: EMIT(tc, mov_simd_to_reg(arm_reg, REG_FPSCR)); break;
                default: kprintf("[PPC] Illegal reg %d in IntMapGpr()\n", reg);
            }
        }

        /* Put into GPR_LRU */
        ADDHEAD(&GPR_LRU, rn);

        return arm_reg;
    }
    
    /* No free ARM register. Remove last entry from GPR_LRU */
    rn = (struct RegisterNode *)REMTAIL(&GPR_LRU);

    /* If dirty, store it back to PPC context */
    if (rn->rn_Dirty) {
        /* Store value from ARM register back into PPC context */
        switch(rn->rn_RegNum) {
            case GPR(14): EMIT(tc, mov_reg_to_simd(GPR14, rn->rn_ARM)); break;
            case GPR(15): EMIT(tc, mov_reg_to_simd(GPR15, rn->rn_ARM)); break;
            case GPR(16): EMIT(tc, mov_reg_to_simd(GPR16, rn->rn_ARM)); break;
            case GPR(17): EMIT(tc, mov_reg_to_simd(GPR17, rn->rn_ARM)); break;
            case GPR(18): EMIT(tc, mov_reg_to_simd(GPR18, rn->rn_ARM)); break;
            case GPR(19): EMIT(tc, mov_reg_to_simd(GPR19, rn->rn_ARM)); break;
            case GPR(20): EMIT(tc, mov_reg_to_simd(GPR20, rn->rn_ARM)); break;
            case GPR(21): EMIT(tc, mov_reg_to_simd(GPR21, rn->rn_ARM)); break;
            case GPR(22): EMIT(tc, mov_reg_to_simd(GPR22, rn->rn_ARM)); break;
            case GPR(23): EMIT(tc, mov_reg_to_simd(GPR23, rn->rn_ARM)); break;
            case GPR(24): EMIT(tc, mov_reg_to_simd(GPR24, rn->rn_ARM)); break;
            case GPR(25): EMIT(tc, mov_reg_to_simd(GPR25, rn->rn_ARM)); break;
            case GPR(26): EMIT(tc, mov_reg_to_simd(GPR26, rn->rn_ARM)); break;
            case GPR(27): EMIT(tc, mov_reg_to_simd(GPR27, rn->rn_ARM)); break;
            case GPR(28): EMIT(tc, mov_reg_to_simd(GPR28, rn->rn_ARM)); break;
            case GPR(29): EMIT(tc, mov_reg_to_simd(GPR29, rn->rn_ARM)); break;
            case GPR(30): EMIT(tc, mov_reg_to_simd(GPR30, rn->rn_ARM)); break;
            case GPR(31): EMIT(tc, mov_reg_to_simd(GPR31, rn->rn_ARM)); break;
            case CRn: EMIT(tc, mov_reg_to_simd(REG_CR, rn->rn_ARM)); break;
            case XERn: EMIT(tc, mov_reg_to_simd(REG_XER, rn->rn_ARM)); break;
            case FPSCRn: EMIT(tc, mov_reg_to_simd(REG_FPSCR, rn->rn_ARM)); break;
            default: kprintf("[PPC] Illegal reg %d in IntMapGpr()\n", rn->rn_ARM);
        }
    }

    /* Update values in RegisterNode */
    rn->rn_Dirty = set_dirty;
    rn->rn_RegNum = reg;

    if (load) {
        /* Load value from PPC context into ARM register */
        switch(reg) {
            case GPR(14): EMIT(tc, mov_simd_to_reg(arm_reg, GPR14)); break;
            case GPR(15): EMIT(tc, mov_simd_to_reg(arm_reg, GPR15)); break;
            case GPR(16): EMIT(tc, mov_simd_to_reg(arm_reg, GPR16)); break;
            case GPR(17): EMIT(tc, mov_simd_to_reg(arm_reg, GPR17)); break;
            case GPR(18): EMIT(tc, mov_simd_to_reg(arm_reg, GPR18)); break;
            case GPR(19): EMIT(tc, mov_simd_to_reg(arm_reg, GPR19)); break;
            case GPR(20): EMIT(tc, mov_simd_to_reg(arm_reg, GPR20)); break;
            case GPR(21): EMIT(tc, mov_simd_to_reg(arm_reg, GPR21)); break;
            case GPR(22): EMIT(tc, mov_simd_to_reg(arm_reg, GPR22)); break;
            case GPR(23): EMIT(tc, mov_simd_to_reg(arm_reg, GPR23)); break;
            case GPR(24): EMIT(tc, mov_simd_to_reg(arm_reg, GPR24)); break;
            case GPR(25): EMIT(tc, mov_simd_to_reg(arm_reg, GPR25)); break;
            case GPR(26): EMIT(tc, mov_simd_to_reg(arm_reg, GPR26)); break;
            case GPR(27): EMIT(tc, mov_simd_to_reg(arm_reg, GPR27)); break;
            case GPR(28): EMIT(tc, mov_simd_to_reg(arm_reg, GPR28)); break;
            case GPR(29): EMIT(tc, mov_simd_to_reg(arm_reg, GPR29)); break;
            case GPR(30): EMIT(tc, mov_simd_to_reg(arm_reg, GPR30)); break;
            case GPR(31): EMIT(tc, mov_simd_to_reg(arm_reg, GPR31)); break;
            case CRn: EMIT(tc, mov_simd_to_reg(arm_reg, REG_CR)); break;
            case XERn: EMIT(tc, mov_simd_to_reg(arm_reg, REG_XER)); break;
            case FPSCRn: EMIT(tc, mov_simd_to_reg(arm_reg, REG_FPSCR)); break;
            default: kprintf("[PPC] Illegal reg %d in IntMapGpr()\n", reg);
        }
    }

    /* Put into GPR_LRU */
    ADDHEAD(&GPR_LRU, rn);

    return rn->rn_ARM;
}

static __used__ uint8_t MapGPRForRead(struct TranslatorContext *tc, uint8_t reg)
{
    return IntMapGPR(tc, reg, 1, 0);
}

static __used__ uint8_t MapGPRForReadAndWrite(struct TranslatorContext *tc, uint8_t reg)
{
    return IntMapGPR(tc, reg, 1, 1);
}

static __used__ uint8_t MapGPRForWrite(struct TranslatorContext *tc, uint8_t reg)
{
    return IntMapGPR(tc, reg, 0, 1);
}

static __used__ uint8_t IsGPRMapped(struct TranslatorContext *, uint8_t reg)
{
    struct RegisterNode *rn;

    /* If register is a fixed-assigned one, return ASAP */
    if (_int_reg_mapping[reg] != 0xff) {
        return _int_reg_mapping[reg];
    }

    /* Not fixed mapped, check GPR_LRU now */
    ForeachNode(&GPR_LRU, rn)
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
    struct RegisterNode *rn;

    /* Register with fixed mapping does not need to be set dirty */
    if (_int_reg_mapping[reg] != 0xff) return;

    /* Check if register is already in LRU */
    ForeachNode(&GPR_LRU, rn)
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
                    EMIT(tc, mov_reg_to_simd(
                                FlushStoreSorted[4 * vn + lane].Vn,
                                FlushStoreSorted[4 * vn + lane].Size,
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

static __used__ void FlushAllGPRs(struct TranslatorContext *tc)
{
    struct RegisterNode *rn, *next;

    bzero(FlushStore, sizeof(FlushStore));

    ForeachNodeSafe(&GPR_LRU, rn, next)
    {
        /* Remove itself from the list */
        REMOVE(&rn->rn_Node);

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
                case FPSCRn: EMIT(tc, mov_reg_to_simd(REG_FPSCR, rn->rn_ARM)); break;
                default: kprintf("[PPC] Illegal reg %d in IntMapGpr()\n", rn->rn_ARM);
            }
        }
        
        /* Mark ARM register as free */
        FreeARMRegister(tc, rn->rn_ARM); 

        /* Add the node itself to free pool */
        ADDTAIL(&FreePool, rn);
    }

    PurgeFlushStore(tc);
}

static __used__ void StoreDirtyGPRs(struct TranslatorContext *tc)
{
    struct RegisterNode *rn;

    bzero(FlushStore, sizeof(FlushStore));

    ForeachNode(&GPR_LRU, rn)
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
                case FPSCRn: EMIT(tc, mov_reg_to_simd(REG_FPSCR, rn->rn_ARM)); break;
                default: kprintf("[PPC] Illegal reg %d in IntMapGpr()\n", rn->rn_ARM);
            }
        }
    }

    PurgeFlushStore(tc);
}

static __used__ void GetOffsetPC(struct TranslatorContext *tc, int8_t *offset)
{
    // Calculate new PC relative offset
    int new_offset = _pc_rel + *offset;

    // If overflow would occur then compute PC and get new offset
    if (new_offset > 127 || new_offset < -127)
    {
        if (_pc_rel > 0)
            EMIT(tc, add_immed(REG_PC, REG_PC, _pc_rel));
        else
            EMIT(tc, sub_immed(REG_PC, REG_PC, -_pc_rel));

        _pc_rel = 0;
        new_offset = *offset;
    }

    *offset = new_offset;
}

static __used__ void AdvancePC(struct TranslatorContext *tc, uint8_t offset)
{
    // Calculate new PC relative offset
    _pc_rel += (int)offset;

    // If overflow would occur then compute PC and get new offset
    if (_pc_rel > 120 || _pc_rel < -120)
    {
        if (_pc_rel > 0)
            EMIT(tc, add_immed(REG_PC, REG_PC, _pc_rel));
        else
            EMIT(tc, sub_immed(REG_PC, REG_PC, -_pc_rel));

        _pc_rel = 0;
    }
}

static __used__ void FlushPC(struct TranslatorContext *tc)
{
    if (_pc_rel > 0)
            EMIT(tc, add_immed(REG_PC, REG_PC, _pc_rel));
    else if (_pc_rel < 0)
            EMIT(tc, sub_immed(REG_PC, REG_PC, -_pc_rel));

    _pc_rel = 0;
}

static __used__ void ResetOffsetPC(struct TranslatorContext *)
{
    _pc_rel = 0;
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

void EMIT_set_crn_logic(struct TranslatorContext *tc, uint8_t cr)
{
    uint8_t reg_cr = MapGPRForReadAndWrite(tc, CRn);
    uint8_t tmp = AllocARMRegister(tc);
    uint8_t reg_xer;

    /* Shift right XER by 31 so that SO is bit 0 */
    if ((reg_xer = IsGPRMapped(tc, XERn)) != 0xff)
    {
        /* XER was already in one of GPRs, rotate with tmp as target */
        EMIT(tc, lsr(tmp, reg_xer, 31));
    }
    else
    {
        /* XER was not mapped, get a copy from PPC context (SIMD register) and rotate */
        EMIT(tc,
            mov_simd_to_reg(tmp, REG_XER),
            lsr(tmp, tmp, 31)
        );
    }

    uint8_t reg_zero_case = AllocARMRegister(tc);
    uint8_t reg_minus_case = AllocARMRegister(tc);

    EMIT(tc, 
        orr_immed(reg_zero_case, tmp, 1, 31),     // Set EQ flag
        orr_immed(reg_minus_case, tmp, 1, 29),    // Set LT flag
        orr_immed(tmp, tmp, 1, 30),            // Set GT flag
        csel(tmp, reg_zero_case, tmp, A64_CC_EQ),
        csel(tmp, reg_minus_case, tmp, A64_CC_MI),

        /* Insert into CRn */
        bfi(reg_cr, tmp, 4 * (7 - cr), 4)
    );

    FreeARMRegister(tc, reg_zero_case);
    FreeARMRegister(tc, reg_minus_case);
    FreeARMRegister(tc, tmp);
}

void EMIT_set_crn_unsigned(struct TranslatorContext *tc, uint8_t cr)
{
    uint8_t reg_cr = MapGPRForReadAndWrite(tc, CRn);
    uint8_t tmp = AllocARMRegister(tc);
    uint8_t reg_xer;

    /* Shift right XER by 31 so that SO is bit 0 */
    if ((reg_xer = IsGPRMapped(tc, XERn)) != 0xff)
    {
        /* XER was already in one of GPRs, rotate with tmp as target */
        EMIT(tc, lsr(tmp, reg_xer, 31));
    }
    else
    {
        /* XER was not mapped, get a copy from PPC context (SIMD register) and rotate */
        EMIT(tc,
            mov_simd_to_reg(tmp, REG_XER),
            lsr(tmp, tmp, 31)
        );
    }

    uint8_t reg_zero_case = AllocARMRegister(tc);
    uint8_t reg_minus_case = AllocARMRegister(tc);

    EMIT(tc, 
        orr_immed(reg_zero_case, tmp, 1, 31),     // Set EQ flag
        orr_immed(reg_minus_case, tmp, 1, 29),    // Set LT flag
        orr_immed(tmp, tmp, 1, 30),            // Set GT flag
        csel(tmp, reg_zero_case, tmp, A64_CC_EQ),
        csel(tmp, reg_minus_case, tmp, A64_CC_CC),

        /* Insert into CRn */
        bfi(reg_cr, tmp, 4 * (7 - cr), 4)
    );

    FreeARMRegister(tc, reg_zero_case);
    FreeARMRegister(tc, reg_minus_case);
    FreeARMRegister(tc, tmp);
}

void EMIT_set_crn_signed(struct TranslatorContext *tc, uint8_t cr)
{
    uint8_t reg_cr = MapGPRForReadAndWrite(tc, CRn);
    uint8_t tmp = AllocARMRegister(tc);
    uint8_t reg_xer = MapGPRForRead(tc, XERn);

    /* Shift right XER by 31 so that SO is bit 0 */
    if (reg_xer != 0xff)
    {
        /* XER was already in one of GPRs, rotate with tmp as target */
        EMIT(tc, lsr(tmp, reg_xer, 31));
    }
    else
    {
        /* XER was not mapped, get a copy from PPC context (SIMD register) and rotate */
        EMIT(tc,
            mov_simd_to_reg(tmp, REG_XER),
            lsr(tmp, tmp, 31)
        );
    }

    uint8_t reg_zero_case = AllocARMRegister(tc);
    uint8_t reg_minus_case = AllocARMRegister(tc);

    EMIT(tc, 
        orr_immed(reg_zero_case, tmp, 1, 31),     // Set EQ flag
        orr_immed(reg_minus_case, tmp, 1, 29),    // Set LT flag
        orr_immed(tmp, tmp, 1, 30),            // Set GT flag
        csel(tmp, reg_zero_case, tmp, A64_CC_EQ),
        csel(tmp, reg_minus_case, tmp, A64_CC_LT),

        /* Insert into CRn */
        bfi(reg_cr, tmp, 4 * (7 - cr), 4)
    );

    FreeARMRegister(tc, reg_zero_case);
    FreeARMRegister(tc, reg_minus_case);
    FreeARMRegister(tc, tmp);
}

static __used__ int EMIT_addi(struct TranslatorContext *tc, uint32_t opcode)
{
    uint8_t rd = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint16_t imm = opcode & 0xffff;
    int32_t simm = (int16_t)imm;

    /* If rA == 0 then this is just load immediate */
    if (ra == 0) {
        uint8_t reg_rd = MapGPRForWrite(tc, rd);
        EMIT_LoadImmediate(tc, reg_rd, (uint32_t)simm);
    } else {
        uint8_t arm_rd, arm_ra;

        /* Source register and target are the same */
        if (ra == rd) {
            arm_rd = MapGPRForReadAndWrite(tc, rd);
            arm_ra = arm_rd;
        } else {
            arm_rd = MapGPRForWrite(tc, rd);
            arm_ra = MapGPRForRead(tc, ra);
        }

        /* If negative, we handle subtraction */
        if (simm < 0) {
            simm = -simm;
            if ((simm & 0xfffff000) == 0) {
                EMIT(tc, sub_immed(arm_rd, arm_ra, simm & 0xfff));
            } else if ((simm & 0xffff0fff) == 0) {
                EMIT(tc, sub_immed_lsl12(arm_rd, arm_ra, (simm >> 12) & 0xfff));
            } else {
                uint8_t tmp = AllocARMRegister(tc);
                EMIT_LoadImmediate(tc, tmp, simm);
                EMIT(tc, sub_reg(arm_rd, arm_ra, tmp, LSL, 0));
                FreeARMRegister(tc, tmp);
            }
        } else {
            if ((simm & 0xfffff000) == 0) {
                EMIT(tc, add_immed(arm_rd, arm_ra, simm & 0xfff));
            } else if ((simm & 0xffff0fff) == 0) {
                EMIT(tc, add_immed_lsl12(arm_rd, arm_ra, (simm >> 12) & 0xfff));
            } else {
                uint8_t tmp = AllocARMRegister(tc);
                EMIT_LoadImmediate(tc, tmp, simm);
                EMIT(tc, add_reg(arm_rd, arm_ra, tmp, LSL, 0));
                FreeARMRegister(tc, tmp);
            }
        }
    }

    tc->tc_PPCCodePtr++;
    AdvancePC(tc, 4);
    return 1;
}

static __used__ int EMIT_addis(struct TranslatorContext *tc, uint32_t opcode)
{
    uint8_t rd = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint16_t imm = opcode & 0xffff;

    /* If rA == 0 then this is load upper */
    if (ra == 0) {
        uint8_t arm_rd = MapGPRForWrite(tc, rd);
        EMIT(tc, mov_immed_u16(arm_rd, imm, 1));
    } else {
        uint8_t arm_rd, arm_ra;

        /* Source register and target are the same */
        if (ra == rd) {
            arm_rd = MapGPRForReadAndWrite(tc, rd);
            arm_ra = arm_rd;
        } else {
            arm_rd = MapGPRForWrite(tc, rd);
            arm_ra = MapGPRForRead(tc, ra);
        }

        if (imm & 0xff00) {
            uint8_t tmp = AllocARMRegister(tc);
            EMIT(tc, 
                mov_immed_u16(tmp, imm, 1),
                add_reg(arm_rd, arm_ra, tmp, LSL, 0)
            );
            FreeARMRegister(tc, tmp);
        }
        else {
            EMIT(tc, add_immed_lsl12(arm_rd, arm_ra, (imm & 0xff) << 4));
        }
    }

    tc->tc_PPCCodePtr++;
    AdvancePC(tc, 4);
    return 1;
}

static __used__ int EMIT_cmpi(struct TranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 0x00600000) return -1;

    uint8_t cr = (opcode >> 23) & 7;
    uint8_t ra = (opcode >> 16) & 31;
    uint16_t imm = opcode & 0xffff;
    int32_t simm = (int16_t)imm;

    uint8_t reg_ra = MapGPRForRead(tc, ra);

    /* Is the immediate in range for CMP? */
    if ((simm & 0xfffff000) == 0) {
        EMIT(tc, cmp_immed(reg_ra, simm));
    }
    else if ((simm & 0xff000fff) == 0) {
        EMIT(tc, cmp_immed_lsl12(reg_ra, simm >> 12));
    }
    else {
        uint8_t tmp = AllocARMRegister(tc);

        EMIT_LoadImmediate(tc, tmp, simm);
        EMIT(tc, cmp_reg(reg_ra, tmp, LSL, 0));

        FreeARMRegister(tc, tmp);
    }

    EMIT_set_crn_signed(tc, cr);

    tc->tc_PPCCodePtr++;
    AdvancePC(tc, 4);
    return 1;
}

static __used__ int EMIT_cmpli(struct TranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 0x00600000) return -1;

    uint8_t cr = (opcode >> 23) & 7;
    uint8_t ra = (opcode >> 16) & 31;
    uint32_t imm = opcode & 0xffff;

    uint8_t reg_ra = MapGPRForRead(tc, ra);

    /* Is the immediate in range for CMP? */
    if ((imm & 0xfffff000) == 0) {
        EMIT(tc, cmp_immed(reg_ra, imm));
    }
    else if ((imm & 0xff000fff) == 0) {
        EMIT(tc, cmp_immed_lsl12(reg_ra, imm >> 12));
    }
    else {
        uint8_t tmp = AllocARMRegister(tc);

        EMIT_LoadImmediate(tc, tmp, imm);
        EMIT(tc, cmp_reg(reg_ra, tmp, LSL, 0));

        FreeARMRegister(tc, tmp);
    }

    EMIT_set_crn_unsigned(tc, cr);

    tc->tc_PPCCodePtr++;
    AdvancePC(tc, 4);
    return 1;
}

static __used__ int EMIT_stb(struct TranslatorContext *tc, uint32_t opcode)
{
    uint8_t rs = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    int16_t d = opcode & 0xffff;

    uint8_t reg = MapGPRForRead(tc, rs);

    /* If Ra is 0, then address is the displacement, only */
    if (ra == 0) {
        uint8_t ea = AllocARMRegister(tc);

        if (d < 0) {
            EMIT(tc, movn_immed_u16(ea, ~d & 0xffff, 0));
        } else {
            EMIT(tc, mov_immed_u16(ea, d, 0));
        }

        EMIT(tc, strb_offset(ea, reg, 0));

        FreeARMRegister(tc, ea);
    } else {
        uint8_t base = MapGPRForRead(tc, ra);
        /* Ra is a register, check if displacement can be used for store directly */
        if (d >= 0 && d <= 0xfff) {
            EMIT(tc, strb_offset(base, reg, d));
        } else if (d >= -256 && d < 255) {
            EMIT(tc, sturb_offset(base, reg, d));
        }
        else {
            uint8_t ea = AllocARMRegister(tc);

            if (d < 0) {
                EMIT(tc, movn_immed_u16(ea, ~d & 0xffff, 0));
            }
            else {
                EMIT(tc, mov_immed_u16(ea, d, 0));
            }
            
            EMIT(tc, strb_regoffset(base, reg, ea, SXTX));

            FreeARMRegister(tc, ea);
        }
    }

    tc->tc_PPCCodePtr++;
    AdvancePC(tc, 4);
    return 1;
}

static __used__ int EMIT_stbu(struct TranslatorContext *tc, uint32_t opcode)
{
    uint8_t rs = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    int16_t d = opcode & 0xffff;

    /* It is illegal to have Ra = 0 */
    if (ra == 0) {
        return -1;
    }

    uint8_t reg = MapGPRForRead(tc, rs);
    uint8_t base = MapGPRForReadAndWrite(tc, ra);

    /* Ra is a register, check if displacement can be used for store directly */
    if (d >= -256 && d <= 255) {
        EMIT(tc, strb_offset_preindex(base, reg, d));
    }
    else if (d >= 0 && d <= 0xfff) {
        EMIT(tc, 
            strb_offset(base, reg, d),
            add_immed(base, base, d)
        );
    }
    else {
        uint8_t ea = AllocARMRegister(tc);

        if (d < 0) {
            EMIT(tc, movn_immed_u16(ea, ~d & 0xffff, 0));
        }
        else {
            EMIT(tc, mov_immed_u16(ea, d, 0));
        }
        
        EMIT(tc, 
            strb_regoffset(base, reg, ea, SXTX),
            add_reg(base, base, ea, LSL, 0)
        );

        FreeARMRegister(tc, ea);
    }

    tc->tc_PPCCodePtr++;
    AdvancePC(tc, 4);
    return 1;
}

static __used__ int EMIT_sth(struct TranslatorContext *tc, uint32_t opcode)
{
    uint8_t rs = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    int16_t d = opcode & 0xffff;

    uint8_t reg = MapGPRForRead(tc, rs);

    /* If Ra is 0, then address is the displacement, only */
    if (ra == 0) {
        uint8_t ea = AllocARMRegister(tc);

        if (d < 0) {
            EMIT(tc, movn_immed_u16(ea, ~d & 0xffff, 0));
        } else {
            EMIT(tc, mov_immed_u16(ea, d, 0));
        }

        EMIT(tc, strh_offset(ea, reg, 0));

        FreeARMRegister(tc, ea);
    } else {
        /* Ra is a register, check if displacement can be used for store directly */
        if (d >= 0 && d <= 0x1ffe && (d & 1) == 0) {
            uint8_t base = MapGPRForRead(tc, ra);
            EMIT(tc, strh_offset(base, reg, d));
        }
        else if (d >= -256 && d <= 255) {
            uint8_t base = MapGPRForRead(tc, ra);
            EMIT(tc, sturh_offset(base, reg, d));
        }
        else {
            uint8_t ea = AllocARMRegister(tc);
            uint8_t base = MapGPRForRead(tc, ra);

            if (d < 0) {
                EMIT(tc, movn_immed_u16(ea, ~d & 0xffff, 0));
            }
            else {
                EMIT(tc, mov_immed_u16(ea, d, 0));
            }
            
            EMIT(tc, strh_regoffset(base, reg, ea, SXTX, 0));

            FreeARMRegister(tc, ea);
        }
    }

    tc->tc_PPCCodePtr++;
    AdvancePC(tc, 4);
    return 1;
}

static __used__ int EMIT_lha(struct TranslatorContext *tc, uint32_t opcode)
{
    uint8_t rd = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    int16_t d = opcode & 0xffff;

    uint8_t reg = MapGPRForWrite(tc, rd);

    /* If Ra is 0, then address is the displacement, only */
    if (ra == 0) {
        uint8_t ea = AllocARMRegister(tc);

        if (d < 0) {
            EMIT(tc, movn_immed_u16(ea, ~d & 0xffff, 0));
        } else {
            EMIT(tc, mov_immed_u16(ea, d, 0));
        }

        EMIT(tc, ldrsh_offset(ea, reg, 0));

        FreeARMRegister(tc, ea);
    } else {
        /* Ra is a register, check if displacement can be used for store directly */
        if (d >= 0 && d <= 0x1ffe && (d & 1) == 0) {
            uint8_t base = MapGPRForRead(tc, ra);
            EMIT(tc, ldrsh_offset(base, reg, d));
        }
        else if (d >= -256 && d <= 255) {
            uint8_t base = MapGPRForRead(tc, ra);
            EMIT(tc, ldursh_offset(base, reg, d));
        }
        else {
            uint8_t ea = AllocARMRegister(tc);
            uint8_t base = MapGPRForRead(tc, ra);

            if (d < 0) {
                EMIT(tc, movn_immed_u16(ea, ~d & 0xffff, 0));
            }
            else {
                EMIT(tc, mov_immed_u16(ea, d, 0));
            }
            
            EMIT(tc, ldrsh_regoffset(base, reg, ea, SXTX, 0));

            FreeARMRegister(tc, ea);
        }
    }

    tc->tc_PPCCodePtr++;
    AdvancePC(tc, 4);
    return 1;
}

static __used__ int EMIT_lwz(struct TranslatorContext *tc, uint32_t opcode)
{
    uint8_t rd = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    int16_t d = opcode & 0xffff;

    uint8_t reg = MapGPRForWrite(tc, rd);

    /* If Ra is 0, then address is the displacement, only */
    if (ra == 0) {
        uint8_t ea = AllocARMRegister(tc);

        if (d < 0) {
            EMIT(tc, movn_immed_u16(ea, ~d & 0xffff, 0));
        } else {
            EMIT(tc, mov_immed_u16(ea, d, 0));
        }

        EMIT(tc, ldr_offset(ea, reg, 0));

        FreeARMRegister(tc, ea);
    } else {
        /* Ra is a register, check if displacement can be used for store directly */
        if (d >= 0 && d <= 0x3ffc && (d & 3) == 0) {
            uint8_t base = MapGPRForRead(tc, ra);
            EMIT(tc, ldr_offset(base, reg, d));
        }
        else if (d >= -256 && d <= 255) {
            uint8_t base = MapGPRForRead(tc, ra);
            EMIT(tc, ldur_offset(base, reg, d));
        }
        else {
            uint8_t ea = AllocARMRegister(tc);
            uint8_t base = MapGPRForRead(tc, ra);

            if (d < 0) {
                EMIT(tc, movn_immed_u16(ea, ~d & 0xffff, 0));
            }
            else {
                EMIT(tc, mov_immed_u16(ea, d, 0));
            }
            
            EMIT(tc, ldr_regoffset(base, reg, ea, SXTX, 0));

            FreeARMRegister(tc, ea);
        }
    }

    tc->tc_PPCCodePtr++;
    AdvancePC(tc, 4);
    return 1;
}

static __used__ int EMIT_lwzu(struct TranslatorContext *tc, uint32_t opcode)
{
    uint8_t rd = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    int16_t d = opcode & 0xffff;

    uint8_t reg = MapGPRForWrite(tc, rd);
    uint8_t base = MapGPRForReadAndWrite(tc, ra);

    /* Ra is a register, check if displacement can be used for load directly */
    if (d >= -256 && d <= 255) {
        EMIT(tc, ldr_offset_preindex(base, reg, d));
    }
    else if (d >= 0 && d <= 0xfff && (d & 3) == 0) {
        EMIT(tc, 
            ldr_offset(base, reg, d),
            add_immed(base, base, d)
        );
    }
    else {
        uint8_t ea = AllocARMRegister(tc);

        if (d < 0) {
            EMIT(tc, movn_immed_u16(ea, ~d & 0xffff, 0));
        }
        else {
            EMIT(tc, mov_immed_u16(ea, d, 0));
        }
        
        EMIT(tc, 
            ldr_regoffset(base, reg, ea, SXTX, 0),
            add_reg(base, base, ea, LSL, 0)
        );

        FreeARMRegister(tc, ea);
    }

    tc->tc_PPCCodePtr++;
    AdvancePC(tc, 4);
    return 1;
}

static __used__ int EMIT_lhzu(struct TranslatorContext *tc, uint32_t opcode)
{
    uint8_t rd = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    int16_t d = opcode & 0xffff;

    uint8_t reg = MapGPRForWrite(tc, rd);
    uint8_t base = MapGPRForReadAndWrite(tc, ra);

    /* Ra is a register, check if displacement can be used for load directly */
    if (d >= -256 && d <= 255) {
        EMIT(tc, ldrh_offset_preindex(base, reg, d));
    }
    else if (d >= 0 && d <= 0xfff && (d & 1) == 0) {
        EMIT(tc, 
            ldrh_offset(base, reg, d),
            add_immed(base, base, d)
        );
    }
    else {
        uint8_t ea = AllocARMRegister(tc);

        if (d < 0) {
            EMIT(tc, movn_immed_u16(ea, ~d & 0xffff, 0));
        }
        else {
            EMIT(tc, mov_immed_u16(ea, d, 0));
        }
        
        EMIT(tc, 
            ldrh_regoffset(base, reg, ea, SXTX, 0),
            add_reg(base, base, ea, LSL, 0)
        );

        FreeARMRegister(tc, ea);
    }

    tc->tc_PPCCodePtr++;
    AdvancePC(tc, 4);
    return 1;
}

static __used__ int EMIT_lhau(struct TranslatorContext *tc, uint32_t opcode)
{
    uint8_t rd = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    int16_t d = opcode & 0xffff;

    uint8_t reg = MapGPRForWrite(tc, rd);
    uint8_t base = MapGPRForReadAndWrite(tc, ra);

    /* Ra is a register, check if displacement can be used for load directly */
    if (d >= -256 && d <= 255) {
        EMIT(tc, ldrsh_offset_preindex(base, reg, d));
    }
    else if (d >= 0 && d <= 0xfff && (d & 1) == 0) {
        EMIT(tc, 
            ldrsh_offset(base, reg, d),
            add_immed(base, base, d)
        );
    }
    else {
        uint8_t ea = AllocARMRegister(tc);

        if (d < 0) {
            EMIT(tc, movn_immed_u16(ea, ~d & 0xffff, 0));
        }
        else {
            EMIT(tc, mov_immed_u16(ea, d, 0));
        }
        
        EMIT(tc, 
            ldrsh_regoffset(base, reg, ea, SXTX, 0),
            add_reg(base, base, ea, LSL, 0)
        );

        FreeARMRegister(tc, ea);
    }

    tc->tc_PPCCodePtr++;
    AdvancePC(tc, 4);
    return 1;
}

static __used__ int EMIT_lwzx(struct TranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 1) return -1;

    uint8_t rd = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;

    uint8_t reg_rd = MapGPRForWrite(tc, rd);
    uint8_t reg_rb = MapGPRForRead(tc, rb);
    
    /* If Ra is 0, then address is the displacement, only */
    if (ra == 0) {
        EMIT(tc, ldr_offset(reg_rb, reg_rd, 0));
    } else {
        uint8_t reg_ra = MapGPRForRead(tc, ra);
        EMIT(tc, ldr_regoffset(reg_ra, reg_rd, reg_rb, SXTX, 0));
    }

    tc->tc_PPCCodePtr++;
    AdvancePC(tc, 4);
    return 1;
}

static __used__ int EMIT_lwbrx(struct TranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 1) return -1;

    uint8_t rd = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;

    uint8_t reg_rd = MapGPRForWrite(tc, rd);
    uint8_t reg_rb = MapGPRForRead(tc, rb);
    
    /* If Ra is 0, then address is the displacement, only */
    if (ra == 0) {
        EMIT(tc, ldr_offset(reg_rb, reg_rd, 0));
    } else {
        uint8_t reg_ra = MapGPRForRead(tc, ra);
        EMIT(tc, ldr_regoffset(reg_ra, reg_rd, reg_rb, SXTX, 0));
    }

    EMIT(tc, rev(reg_rd, reg_rd));

    tc->tc_PPCCodePtr++;
    AdvancePC(tc, 4);
    return 1;
}

static __used__ int EMIT_lhzx(struct TranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 1) return -1;

    uint8_t rd = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;

    uint8_t reg_rd = MapGPRForWrite(tc, rd);
    uint8_t reg_rb = MapGPRForRead(tc, rb);
    
    /* If Ra is 0, then address is the displacement, only */
    if (ra == 0) {
        EMIT(tc, ldrh_offset(reg_rb, reg_rd, 0));
    } else {
        uint8_t reg_ra = MapGPRForRead(tc, ra);
        EMIT(tc, ldrh_regoffset(reg_ra, reg_rd, reg_rb, SXTX, 0));
    }

    tc->tc_PPCCodePtr++;
    AdvancePC(tc, 4);
    return 1;
}

static __used__ int EMIT_lhbrx(struct TranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 1) return -1;

    uint8_t rd = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;

    uint8_t reg_rd = MapGPRForWrite(tc, rd);
    uint8_t reg_rb = MapGPRForRead(tc, rb);
    
    /* If Ra is 0, then address is the displacement, only */
    if (ra == 0) {
        EMIT(tc, ldrh_offset(reg_rb, reg_rd, 0));
    } else {
        uint8_t reg_ra = MapGPRForRead(tc, ra);
        EMIT(tc, ldrh_regoffset(reg_ra, reg_rd, reg_rb, SXTX, 0));
    }

    EMIT(tc, rev16(reg_rd, reg_rd));

    tc->tc_PPCCodePtr++;
    AdvancePC(tc, 4);
    return 1;
}

static __used__ int EMIT_lhzux(struct TranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 1) return -1;

    uint8_t rd = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;

    if (ra == 0 || ra == rd) return -1;

    uint8_t reg_rd = MapGPRForWrite(tc, rd);
    uint8_t reg_rb = MapGPRForRead(tc, rb);
    uint8_t reg_ra = MapGPRForReadAndWrite(tc, ra);

    EMIT(tc, 
        ldrh_regoffset(reg_ra, reg_rd, reg_rb, SXTX, 0),
        add_reg(reg_ra, reg_ra, reg_rb, LSL, 0)
    );

    tc->tc_PPCCodePtr++;
    AdvancePC(tc, 4);
    return 1;
}

static __used__ int EMIT_lbzux(struct TranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 1) return -1;

    uint8_t rd = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;

    if (ra == 0 || ra == rd) return -1;

    uint8_t reg_rd = MapGPRForWrite(tc, rd);
    uint8_t reg_rb = MapGPRForRead(tc, rb);
    uint8_t reg_ra = MapGPRForReadAndWrite(tc, ra);

    EMIT(tc, 
        ldrb_regoffset(reg_ra, reg_rd, reg_rb, SXTX),
        add_reg(reg_ra, reg_ra, reg_rb, LSL, 0)
    );

    tc->tc_PPCCodePtr++;
    AdvancePC(tc, 4);
    return 1;
}

static __used__ int EMIT_lhax(struct TranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 1) return -1;

    uint8_t rd = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;

    uint8_t reg_rd = MapGPRForWrite(tc, rd);
    uint8_t reg_rb = MapGPRForRead(tc, rb);
    
    /* If Ra is 0, then address is the displacement, only */
    if (ra == 0) {
        EMIT(tc, ldrsh_offset(reg_rb, reg_rd, 0));
    } else {
        uint8_t reg_ra = MapGPRForRead(tc, ra);
        EMIT(tc, ldrsh_regoffset(reg_ra, reg_rd, reg_rb, SXTX, 0));
    }

    tc->tc_PPCCodePtr++;
    AdvancePC(tc, 4);
    return 1;
}

static __used__ int EMIT_lhaux(struct TranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 1) return -1;

    uint8_t rd = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;

    if (ra == 0 || rd == ra) return -1;

    uint8_t reg_rd = MapGPRForWrite(tc, rd);
    uint8_t reg_rb = MapGPRForRead(tc, rb);
    uint8_t reg_ra = MapGPRForReadAndWrite(tc, ra);

    EMIT(tc, 
        ldrsh_regoffset(reg_ra, reg_rd, reg_rb, SXTX, 0),
        add_reg(reg_ra, reg_ra, reg_rb, LSL, 0)
    );

    tc->tc_PPCCodePtr++;
    AdvancePC(tc, 4);
    return 1;
}

static __used__ int EMIT_lbzx(struct TranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 1) return -1;

    uint8_t rd = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;

    uint8_t reg_rd = MapGPRForWrite(tc, rd);
    uint8_t reg_rb = MapGPRForRead(tc, rb);
    
    /* If Ra is 0, then address is the displacement, only */
    if (ra == 0) {
        EMIT(tc, ldrb_offset(reg_rb, reg_rd, 0));
    } else {
        uint8_t reg_ra = MapGPRForRead(tc, ra);
        EMIT(tc, ldrb_regoffset(reg_ra, reg_rd, reg_rb, SXTX));
    }

    tc->tc_PPCCodePtr++;
    AdvancePC(tc, 4);
    return 1;
}

static __used__ int EMIT_stbx(struct TranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 1) return -1;

    uint8_t rs = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;

    uint8_t reg_rd = MapGPRForRead(tc, rs);
    uint8_t reg_rb = MapGPRForRead(tc, rb);
    
    /* If Ra is 0, then address is the displacement, only */
    if (ra == 0) {
        EMIT(tc, strb_offset(reg_rb, reg_rd, 0));
    } else {
        uint8_t reg_ra = MapGPRForRead(tc, ra);
        EMIT(tc, strb_regoffset(reg_ra, reg_rd, reg_rb, SXTX));
    }

    tc->tc_PPCCodePtr++;
    AdvancePC(tc, 4);
    return 1;
}

static __used__ int EMIT_stbux(struct TranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 1) return -1;

    uint8_t rs = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;

    if (ra == 0 || ra == rs) return -1;

    uint8_t reg_rd = MapGPRForRead(tc, rs);
    uint8_t reg_rb = MapGPRForRead(tc, rb);
    uint8_t reg_ra = MapGPRForReadAndWrite(tc, ra);

    EMIT(tc, 
        strb_regoffset(reg_ra, reg_rd, reg_rb, SXTX),
        add_reg(reg_ra, reg_ra, reg_rb, LSL, 0)
    );

    tc->tc_PPCCodePtr++;
    AdvancePC(tc, 4);
    return 1;
}

static __used__ int EMIT_sthx(struct TranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 1) return -1;

    uint8_t rs = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;

    uint8_t reg_rd = MapGPRForRead(tc, rs);
    uint8_t reg_rb = MapGPRForRead(tc, rb);
    
    /* If Ra is 0, then address is the displacement, only */
    if (ra == 0) {
        EMIT(tc, strh_offset(reg_rb, reg_rd, 0));
    } else {
        uint8_t reg_ra = MapGPRForRead(tc, ra);
        EMIT(tc, strh_regoffset(reg_ra, reg_rd, reg_rb, SXTX, 0));
    }

    tc->tc_PPCCodePtr++;
    AdvancePC(tc, 4);
    return 1;
}

static __used__ int EMIT_sthbrx(struct TranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 1) return -1;

    uint8_t rs = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;

    uint8_t reg_rs = MapGPRForRead(tc, rs);
    uint8_t reg_rb = MapGPRForRead(tc, rb);
    uint8_t tmp = AllocARMRegister(tc);
    
    EMIT(tc, rev16(tmp, reg_rs));

    /* If Ra is 0, then address is the displacement, only */
    if (ra == 0) {
        EMIT(tc, strh_offset(reg_rb, tmp, 0));
    } else {
        uint8_t reg_ra = MapGPRForRead(tc, ra);
        EMIT(tc, strh_regoffset(reg_ra, tmp, reg_rb, SXTX, 0));
    }

    FreeARMRegister(tc, tmp);

    tc->tc_PPCCodePtr++;
    AdvancePC(tc, 4);
    return 1;
}

static __used__ int EMIT_sthux(struct TranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 1) return -1;

    uint8_t rs = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;

    if (ra == 0 || ra == rs) return -1;

    uint8_t reg_rd = MapGPRForRead(tc, rs);
    uint8_t reg_rb = MapGPRForRead(tc, rb);
    uint8_t reg_ra = MapGPRForReadAndWrite(tc, ra);

    EMIT(tc, 
        strh_regoffset(reg_ra, reg_rd, reg_rb, SXTX, 0),
        add_reg(reg_ra, reg_ra, reg_rb, LSL, 0)
    );

    tc->tc_PPCCodePtr++;
    AdvancePC(tc, 4);
    return 1;
}

static __used__ int EMIT_stwx(struct TranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 1) return -1;

    uint8_t rs = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;

    uint8_t reg_rd = MapGPRForRead(tc, rs);
    uint8_t reg_rb = MapGPRForRead(tc, rb);
    
    /* If Ra is 0, then address is the displacement, only */
    if (ra == 0) {
        EMIT(tc, str_offset(reg_rb, reg_rd, 0));
    } else {
        uint8_t reg_ra = MapGPRForRead(tc, ra);
        EMIT(tc, str_regoffset(reg_ra, reg_rd, reg_rb, SXTX, 0));
    }

    tc->tc_PPCCodePtr++;
    AdvancePC(tc, 4);
    return 1;
}

static __used__ int EMIT_stwbrx(struct TranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 1) return -1;

    uint8_t rs = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;

    uint8_t reg_rs = MapGPRForRead(tc, rs);
    uint8_t reg_rb = MapGPRForRead(tc, rb);
    uint8_t tmp = AllocARMRegister(tc);
    
    EMIT(tc, rev(tmp, reg_rs));

    /* If Ra is 0, then address is the displacement, only */
    if (ra == 0) {
        EMIT(tc, str_offset(reg_rb, tmp, 0));
    } else {
        uint8_t reg_ra = MapGPRForRead(tc, ra);
        EMIT(tc, str_regoffset(reg_ra, tmp, reg_rb, SXTX, 0));
    }

    FreeARMRegister(tc, tmp);

    tc->tc_PPCCodePtr++;
    AdvancePC(tc, 4);
    return 1;
}

static __used__ int EMIT_stwux(struct TranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 1) return -1;

    uint8_t rs = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;

    if (ra == 0 || ra == rs) return -1;

    uint8_t reg_rd = MapGPRForRead(tc, rs);
    uint8_t reg_rb = MapGPRForRead(tc, rb);
    uint8_t reg_ra = MapGPRForReadAndWrite(tc, ra);

    EMIT(tc, 
        str_regoffset(reg_ra, reg_rd, reg_rb, SXTX, 0),
        add_reg(reg_ra, reg_ra, reg_rb, LSL, 0)
    );

    tc->tc_PPCCodePtr++;
    AdvancePC(tc, 4);
    return 1;
}

static __used__ int EMIT_lwzux(struct TranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 1) return -1;

    uint8_t rd = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;

    if (ra == 0 || ra == rd) return -1;

    uint8_t reg_rd = MapGPRForWrite(tc, rd);
    uint8_t reg_rb = MapGPRForRead(tc, rb);
    uint8_t reg_ra = MapGPRForReadAndWrite(tc, ra);

    /* If Ra is 0, then address is the displacement, only */
    
    EMIT(tc, 
        ldr_regoffset(reg_ra, reg_rd, reg_rb, SXTX, 0),
        add_reg(reg_ra, reg_ra, reg_rb, LSL, 0)
    );

    tc->tc_PPCCodePtr++;
    AdvancePC(tc, 4);
    return 1;
}

static __used__ int EMIT_lhz(struct TranslatorContext *tc, uint32_t opcode)
{
    uint8_t rd = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    int16_t d = opcode & 0xffff;

    uint8_t reg = MapGPRForRead(tc, rd);

    /* If Ra is 0, then address is the displacement, only */
    if (ra == 0) {
        uint8_t ea = AllocARMRegister(tc);

        if (d < 0) {
            EMIT(tc, movn_immed_u16(ea, ~d & 0xffff, 0));
        } else {
            EMIT(tc, mov_immed_u16(ea, d, 0));
        }

        EMIT(tc, ldrh_offset(ea, reg, 0));

        FreeARMRegister(tc, ea);
    } else {
        /* Ra is a register, check if displacement can be used for store directly */
        if (d >= 0 && d <= 0x1ffe && (d & 1) == 0) {
            uint8_t base = MapGPRForRead(tc, ra);
            EMIT(tc, ldrh_offset(base, reg, d));
        }
        else if (d >= -256 && d <= 255) {
            uint8_t base = MapGPRForRead(tc, ra);
            EMIT(tc, ldurh_offset(base, reg, d));
        }
        else {
            uint8_t ea = AllocARMRegister(tc);
            uint8_t base = MapGPRForRead(tc, ra);

            if (d < 0) {
                EMIT(tc, movn_immed_u16(ea, ~d & 0xffff, 0));
            }
            else {
                EMIT(tc, mov_immed_u16(ea, d, 0));
            }
            
            EMIT(tc, ldrh_regoffset(base, reg, ea, SXTX, 0));

            FreeARMRegister(tc, ea);
        }
    }

    tc->tc_PPCCodePtr++;
    AdvancePC(tc, 4);
    return 1;
}

static __used__ int EMIT_lbz(struct TranslatorContext *tc, uint32_t opcode)
{
    uint8_t rd = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    int16_t d = opcode & 0xffff;

    uint8_t reg = MapGPRForWrite(tc, rd);

    /* If Ra is 0, then address is the displacement, only */
    if (ra == 0) {
        uint8_t ea = AllocARMRegister(tc);

        if (d < 0) {
            EMIT(tc, movn_immed_u16(ea, ~d & 0xffff, 0));
        } else {
            EMIT(tc, mov_immed_u16(ea, d, 0));
        }

        EMIT(tc, ldrb_offset(ea, reg, 0));

        FreeARMRegister(tc, ea);
    } else {
        /* Ra is a register, check if displacement can be used for store directly */
        if (d >= 0 && d <= 0xfff) {
            uint8_t base = MapGPRForRead(tc, ra);
            EMIT(tc, ldrb_offset(base, reg, d));
        }
        else if (d >= -256 && d <= 255) {
            uint8_t base = MapGPRForRead(tc, ra);
            EMIT(tc, ldurb_offset(base, reg, d));
        }
        else {
            uint8_t ea = AllocARMRegister(tc);
            uint8_t base = MapGPRForRead(tc, ra);

            if (d < 0) {
                EMIT(tc, movn_immed_u16(ea, ~d & 0xffff, 0));
            }
            else {
                EMIT(tc, mov_immed_u16(ea, d, 0));
            }
            
            EMIT(tc, ldrb_regoffset(base, reg, ea, SXTX));

            FreeARMRegister(tc, ea);
        }
    }

    tc->tc_PPCCodePtr++;
    AdvancePC(tc, 4);
    return 1;
}

static __used__ int EMIT_lbzu(struct TranslatorContext *tc, uint32_t opcode)
{
    uint8_t rd = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    int16_t d = opcode & 0xffff;

    uint8_t reg = MapGPRForWrite(tc, rd);
    uint8_t base = MapGPRForReadAndWrite(tc, ra);

    /* Ra is a register, check if displacement can be used for load directly */
    if (d >= -256 && d <= 255) {
        EMIT(tc, ldrb_offset_preindex(base, reg, d));
    }
    else if (d >= 0 && d <= 0xfff) {
        EMIT(tc, 
            ldrb_offset(base, reg, d),
            add_immed(base, base, d)
        );
    }
    else {
        uint8_t ea = AllocARMRegister(tc);

        if (d < 0) {
            EMIT(tc, movn_immed_u16(ea, ~d & 0xffff, 0));
        }
        else {
            EMIT(tc, mov_immed_u16(ea, d, 0));
        }
        
        EMIT(tc, 
            ldrb_regoffset(base, reg, ea, SXTX),
            add_reg(base, base, ea, LSL, 0)
        );

        FreeARMRegister(tc, ea);
    }

    tc->tc_PPCCodePtr++;
    AdvancePC(tc, 4);
    return 1;
}

static __used__ int EMIT_stw(struct TranslatorContext *tc, uint32_t opcode)
{
    uint8_t rs = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    int16_t d = opcode & 0xffff;

    uint8_t reg = MapGPRForRead(tc, rs);

    /* If Ra is 0, then address is the displacement, only */
    if (ra == 0) {
        uint8_t ea = AllocARMRegister(tc);

        if (d < 0) {
            EMIT(tc, movn_immed_u16(ea, ~d & 0xffff, 0));
        } else {
            EMIT(tc, mov_immed_u16(ea, d, 0));
        }

        EMIT(tc, str_offset(ea, reg, 0));

        FreeARMRegister(tc, ea);
    } else {
        /* Ra is a register, check if displacement can be used for store directly */
        if (d >= 0 && d <= 0x3ffc && (d & 3) == 0) {
            uint8_t base = MapGPRForRead(tc, ra);
            EMIT(tc, str_offset(base, reg, d));
        }
        else if (d >= -256 && d <= 255) {
            uint8_t base = MapGPRForRead(tc, ra);
            EMIT(tc, stur_offset(base, reg, d));
        }
        else {
            uint8_t ea = AllocARMRegister(tc);
            uint8_t base = MapGPRForRead(tc, ra);

            if (d < 0) {
                EMIT(tc, movn_immed_u16(ea, ~d & 0xffff, 0));
            }
            else {
                EMIT(tc, mov_immed_u16(ea, d, 0));
            }
            
            EMIT(tc, str_regoffset(base, reg, ea, SXTX, 0));

            FreeARMRegister(tc, ea);
        }
    }

    tc->tc_PPCCodePtr++;
    AdvancePC(tc, 4);
    return 1;
}

static __used__ int EMIT_stwu(struct TranslatorContext *tc, uint32_t opcode)
{
    uint8_t rs = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    int16_t d = opcode & 0xffff;

    /* It is illegal to have Ra = 0 */
    if (ra == 0) {
        return -1;
    }

    uint8_t reg = MapGPRForRead(tc, rs);
    uint8_t base = MapGPRForReadAndWrite(tc, ra);

    /* Ra is a register, check if displacement can be used for store directly */
    if (d >= -256 && d <= 255) {
        EMIT(tc, str_offset_preindex(base, reg, d));
    }
    else if (d >= 0 && d <= 0xfff && (d & 3) == 0) {
        EMIT(tc, 
            str_offset(base, reg, d),
            add_immed(base, base, d)
        );
    }
    else {
        uint8_t ea = AllocARMRegister(tc);

        if (d < 0) {
            EMIT(tc, movn_immed_u16(ea, ~d & 0xffff, 0));
        }
        else {
            EMIT(tc, mov_immed_u16(ea, d, 0));
        }
        
        EMIT(tc, 
            str_regoffset(base, reg, ea, SXTX, 0),
            add_reg(base, base, ea, LSL, 0)
        );

        FreeARMRegister(tc, ea);
    }

    tc->tc_PPCCodePtr++;
    AdvancePC(tc, 4);
    return 1;
}

static __used__ int EMIT_sthu(struct TranslatorContext *tc, uint32_t opcode)
{
    uint8_t rs = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    int16_t d = opcode & 0xffff;

    /* It is illegal to have Ra = 0 */
    if (ra == 0) {
        return -1;
    }

    uint8_t reg = MapGPRForRead(tc, rs);
    uint8_t base = MapGPRForReadAndWrite(tc, ra);

    /* Ra is a register, check if displacement can be used for store directly */
    if (d >= -256 && d <= 255) {
        EMIT(tc, strh_offset_preindex(base, reg, d));
    }
    else if (d >= 0 && d <= 0xfff && (d & 1) == 0) {
        EMIT(tc, 
            strh_offset(base, reg, d),
            add_immed(base, base, d)
        );
    }
    else {
        uint8_t ea = AllocARMRegister(tc);

        if (d < 0) {
            EMIT(tc, movn_immed_u16(ea, ~d & 0xffff, 0));
        }
        else {
            EMIT(tc, mov_immed_u16(ea, d, 0));
        }
        
        EMIT(tc, 
            strh_regoffset(base, reg, ea, SXTX, 0),
            add_reg(base, base, ea, LSL, 0)
        );

        FreeARMRegister(tc, ea);
    }

    tc->tc_PPCCodePtr++;
    AdvancePC(tc, 4);
    return 1;
}

static __used__ int EMIT_bx(struct TranslatorContext *tc, uint32_t opcode)
{
    int32_t offset = opcode & 0x03fffffc;
    int update_lr = !!(opcode & 1);
    int is_absolute = !!(opcode & 2);
    int8_t pc_offset = 4;
    struct PPCState *ctx = getHostCTX();
    uint32_t *old_pc = tc->tc_PPCCodePtr;
    int32_t var_EMU68_BRANCH_INLINE_DISTANCE = (ctx->JIT_CONTROL >> JCCB_INLINE_RANGE) & JCCB_INLINE_RANGE_MASK;

    GetOffsetPC(tc, &pc_offset);

    if (offset & 0x02000000) offset |= 0xfc000000;

    if (update_lr) {
        PushReturnAddress(tc->tc_PPCCodePtr + 1);

        if (pc_offset >= 0) {
            EMIT(tc, add_immed(REG_LR, REG_PC, pc_offset));
        } else {
            EMIT(tc, sub_immed(REG_LR, REG_PC, -pc_offset));
        }
    }

    ResetOffsetPC(tc);

    if (is_absolute) {
        tc->tc_PPCCodePtr = (uint32_t*)(uintptr_t)(uint32_t)offset;
        EMIT_LoadImmediate(tc, REG_PC, (uint32_t)offset);
    } else {
        tc->tc_PPCCodePtr += (offset >> 2);
        int32_t pc_adj = pc_offset + offset - 4;
        if (pc_adj < 0) {
            pc_adj = -pc_adj;
            if ((pc_adj & 0xfffff000) == 0) {
                EMIT(tc, sub_immed(REG_PC, REG_PC, pc_adj));
            } else if ((pc_adj & 0xff000fff) == 0) {
                EMIT(tc, sub_immed_lsl12(REG_PC, REG_PC, pc_adj >> 12));
            } else {
                uint8_t tmp = AllocARMRegister(tc);
                EMIT_LoadImmediate(tc, tmp, (uint32_t)pc_adj);
                EMIT(tc, sub_reg(REG_PC, REG_PC, tmp, LSL, 0));
                FreeARMRegister(tc, tmp);
            }
        } else if (pc_adj > 0) {
            if ((pc_adj & 0xfffff000) == 0) {
                EMIT(tc, add_immed(REG_PC, REG_PC, pc_adj));
            } else if ((pc_adj & 0xff000fff) == 0) {
                EMIT(tc, add_immed_lsl12(REG_PC, REG_PC, pc_adj >> 12));
            } else {
                uint8_t tmp = AllocARMRegister(tc);
                EMIT_LoadImmediate(tc, tmp, (uint32_t)pc_adj);
                EMIT(tc, add_reg(REG_PC, REG_PC, tmp, LSL, 0));
                FreeARMRegister(tc, tmp);
            }
        } 
    }

    /* If jump distance is larger than allowed, break translation */
    ptrdiff_t distance = 4 * (tc->tc_PPCCodePtr - old_pc);
    if (distance > var_EMU68_BRANCH_INLINE_DISTANCE || distance < -var_EMU68_BRANCH_INLINE_DISTANCE) {
        EMIT(tc, INSN_TO_LE(MARKER_STOP));
    }

    return 1;
}

static __used__ int EMIT_bcx(struct TranslatorContext *tc, uint32_t opcode)
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

    GetOffsetPC(tc, &pc_offset);

//    kprintf("pc_offset = %d\n", pc_offset);

    if (update_lr) {
        PushReturnAddress(tc->tc_PPCCodePtr + 1);

        if (pc_offset >= 0) {
            EMIT(tc, add_immed(REG_LR, REG_PC, pc_offset));
        } else {
            EMIT(tc, sub_immed(REG_LR, REG_PC, -pc_offset));
        }
    }

    ResetOffsetPC(tc);

    /* Branch always! */
    if (bo0 && bo2) {
        if (is_absolute) {
            tc->tc_PPCCodePtr = (uint32_t*)(uintptr_t)(uint32_t)offset;
            EMIT_LoadImmediate(tc, REG_PC, (uint32_t)offset);
        } else {
            tc->tc_PPCCodePtr += (offset >> 2);
            int32_t pc_adj = pc_offset + offset - 4;
            if (pc_adj < 0) {
                pc_adj = -pc_adj;
                if ((pc_adj & 0xfffff000) == 0) {
                    EMIT(tc, sub_immed(REG_PC, REG_PC, pc_adj));
                } else if ((pc_adj & 0xff000fff) == 0) {
                    EMIT(tc, sub_immed_lsl12(REG_PC, REG_PC, pc_adj >> 12));
                } else {
                    EMIT_LoadImmediate(tc, tmp, (uint32_t)pc_adj);
                    EMIT(tc, sub_reg(REG_PC, REG_PC, tmp, LSL, 0));
                }
            } else if (pc_adj > 0) {
                if ((pc_adj & 0xfffff000) == 0) {
                    EMIT(tc, add_immed(REG_PC, REG_PC, pc_adj));
                } else if ((pc_adj & 0xff000fff) == 0) {
                    EMIT(tc, add_immed_lsl12(REG_PC, REG_PC, pc_adj >> 12));
                } else {
                    EMIT_LoadImmediate(tc, tmp, (uint32_t)pc_adj);
                    EMIT(tc, add_reg(REG_PC, REG_PC, tmp, LSL, 0));
                }
            }
        }
    } else {
        uint8_t success_condition;
        uint8_t reg_cr = MapGPRForRead(tc, CRn);

        /* BO[2] == 0 - decrement CTR and set condition */
        if (dec_ctr) {
            EMIT(tc, subs_immed(REG_CTR, REG_CTR, 1));
            /* bo3 == 1 <- take branch if CTR == 0; bo3 == 0 <- take branch if CTR != 0 */
            if (bo3) {
                EMIT(tc, cset(tmp, A64_CC_EQ));
            } else {
                EMIT(tc, cset(tmp, A64_CC_NE));
            }
            /* if bo0 == 1 there is no need to test condition flags */
            if (bo0 == 0) {
                EMIT(tc, 
                    /* Test condition */
                    tst_immed(reg_cr, 1, (1 + bi) & 31),
                    /* Increase tmp if condition is met */
                    cinc(tmp, tmp, condition_true ? A64_CC_NE : A64_CC_EQ),
                    /* If both CTR condition and CR conditions are met, tmp == 2. Test it. */
                    tst_immed(tmp, 1, 31)
                );
            }
            success_condition = A64_CC_NE;
        } else {
            /* Check the condition */
            EMIT(tc, tst_immed(reg_cr, 1, (1 + bi) & 31));
            success_condition = condition_true ? A64_CC_NE : A64_CC_EQ;
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
        uint32_t fixup_type = FIXUP_BCC;
        uint32_t *jump_location = tc->tc_CodePtr;
        EMIT(tc, b_cc(success_condition, 0));

        /* Here the expected code path follows */
        if (take_branch)
        {
            if (is_absolute) {
                EMIT_LoadImmediate(tc, REG_PC, (uint32_t)offset);
            } else {
                int32_t pc_adj = pc_offset + offset - 4;
                if (pc_adj < 0) {
                    pc_adj = -pc_adj;
                    if ((pc_adj & 0xfffff000) == 0) {
                        EMIT(tc, sub_immed(REG_PC, REG_PC, pc_adj));
                    } else if ((pc_adj & 0xff000fff) == 0) {
                        EMIT(tc, sub_immed_lsl12(REG_PC, REG_PC, pc_adj >> 12));
                    } else {
                        EMIT_LoadImmediate(tc, tmp, (uint32_t)pc_adj);
                        EMIT(tc, sub_reg(REG_PC, REG_PC, tmp, LSL, 0));
                    }
                } else if (pc_adj > 0) {
                    if ((pc_adj & 0xfffff000) == 0) {
                        EMIT(tc, add_immed(REG_PC, REG_PC, pc_adj));
                    } else if ((pc_adj & 0xff000fff) == 0) {
                        EMIT(tc, add_immed_lsl12(REG_PC, REG_PC, pc_adj >> 12));
                    } else {
                        EMIT_LoadImmediate(tc, tmp, (uint32_t)pc_adj);
                        EMIT(tc, add_reg(REG_PC, REG_PC, tmp, LSL, 0));
                    }
                }
            }
        }
        else
        {
            int32_t pc_adj = pc_offset;
            if (pc_adj < 0) {
                pc_adj = -pc_adj;
                if ((pc_adj & 0xfffff000) == 0) {
                    EMIT(tc, sub_immed(REG_PC, REG_PC, pc_adj));
                } else if ((pc_adj & 0xff000fff) == 0) {
                    EMIT(tc, sub_immed_lsl12(REG_PC, REG_PC, pc_adj >> 12));
                } else {
                    EMIT_LoadImmediate(tc, tmp, (uint32_t)pc_adj);
                    EMIT(tc, sub_reg(REG_PC, REG_PC, tmp, LSL, 0));
                }
            } else if (pc_adj > 0) {
                if ((pc_adj & 0xfffff000) == 0) {
                    EMIT(tc, add_immed(REG_PC, REG_PC, pc_adj));
                } else if ((pc_adj & 0xff000fff) == 0) {
                    EMIT(tc, add_immed_lsl12(REG_PC, REG_PC, pc_adj >> 12));
                } else {
                    EMIT_LoadImmediate(tc, tmp, (uint32_t)pc_adj);
                    EMIT(tc, add_reg(REG_PC, REG_PC, tmp, LSL, 0));
                }
            }
        }

        /* Now insert the other code path - this will be treated as exit code */
        uint32_t *exit_code_start = tc->tc_CodePtr;

        if (!take_branch)
        {
            if (is_absolute) {
                EMIT_LoadImmediate(tc, REG_PC, (uint32_t)offset);
            } else {
                int32_t pc_adj = pc_offset + offset - 4;
                if (pc_adj < 0) {
                    pc_adj = -pc_adj;
                    if ((pc_adj & 0xfffff000) == 0) {
                        EMIT(tc, sub_immed(REG_PC, REG_PC, pc_adj));
                    } else if ((pc_adj & 0xff000fff) == 0) {
                        EMIT(tc, sub_immed_lsl12(REG_PC, REG_PC, pc_adj >> 12));
                    } else {
                        EMIT_LoadImmediate(tc, tmp, (uint32_t)pc_adj);
                        EMIT(tc, sub_reg(REG_PC, REG_PC, tmp, LSL, 0));
                    }
                } else if (pc_adj > 0) {
                    if ((pc_adj & 0xfffff000) == 0) {
                        EMIT(tc, add_immed(REG_PC, REG_PC, pc_adj));
                    } else if ((pc_adj & 0xff000fff) == 0) {
                        EMIT(tc, add_immed_lsl12(REG_PC, REG_PC, pc_adj >> 12));
                    } else {
                        EMIT_LoadImmediate(tc, tmp, (uint32_t)pc_adj);
                        EMIT(tc, add_reg(REG_PC, REG_PC, tmp, LSL, 0));
                    }
                }
            }
        }
        else
        {
            int32_t pc_adj = pc_offset;
            if (pc_adj < 0) {
                pc_adj = -pc_adj;
                if ((pc_adj & 0xfffff000) == 0) {
                    EMIT(tc, sub_immed(REG_PC, REG_PC, pc_adj));
                } else if ((pc_adj & 0xff000fff) == 0) {
                    EMIT(tc, sub_immed_lsl12(REG_PC, REG_PC, pc_adj >> 12));
                } else {
                    EMIT_LoadImmediate(tc, tmp, (uint32_t)pc_adj);
                    EMIT(tc, sub_reg(REG_PC, REG_PC, tmp, LSL, 0));
                }
            } else if (pc_adj > 0) {
                if ((pc_adj & 0xfffff000) == 0) {
                    EMIT(tc, add_immed(REG_PC, REG_PC, pc_adj));
                } else if ((pc_adj & 0xff000fff) == 0) {
                    EMIT(tc, add_immed_lsl12(REG_PC, REG_PC, pc_adj >> 12));
                } else {
                    EMIT_LoadImmediate(tc, tmp, (uint32_t)pc_adj);
                    EMIT(tc, add_reg(REG_PC, REG_PC, tmp, LSL, 0));
                }
            }
        }

        /* Insert local exit */
        LocalExit(tc, 1);
        uint32_t *exit_code_end = tc->tc_CodePtr;

        /* Insert fixup location */
        EMIT(tc, 
            exit_code_end - jump_location,
            fixup_type,
            exit_code_end - exit_code_start,
            INSN_TO_LE(MARKER_EXIT_BLOCK)
        );
    }

    /* If jump distance is larger than allowed, break translation */
    ptrdiff_t distance = 4 * (tc->tc_PPCCodePtr - old_pc);
    if (distance > var_EMU68_BRANCH_INLINE_DISTANCE || distance < -var_EMU68_BRANCH_INLINE_DISTANCE) {
        EMIT(tc, INSN_TO_LE(MARKER_STOP));
    }
    
    FreeARMRegister(tc, tmp);
    return 1;
}

static __used__ int EMIT_ori(struct TranslatorContext *tc, uint32_t opcode)
{
    uint32_t immed = opcode & 0xffff;
    uint32_t mask = number_to_mask(immed);
    uint8_t rs = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    
    uint8_t reg_rS = MapGPRForRead(tc, rs);
    uint8_t reg_rA = MapGPRForReadAndWrite(tc, ra);

    if (immed == 0) {
        if (rs == ra)
            EMIT(tc, nop());
        else
            EMIT(tc, mov_reg(reg_rA, reg_rS));
    }
    else {
        if (mask == 0) {
            uint8_t tmp = AllocARMRegister(tc);
            EMIT(tc,
                mov_immed_u16(tmp, immed, 0),
                orr_reg(reg_rA, reg_rS, tmp, LSL, 0)
            );
            FreeARMRegister(tc, tmp);
        } else {
            EMIT(tc, orr_immed(reg_rA, reg_rS, (mask >> 16) & 0x3f, mask & 0x3f));
        }
    }

    tc->tc_PPCCodePtr++;
    AdvancePC(tc, 4);
    return 1;
}

static __used__ int EMIT_oris(struct TranslatorContext *tc, uint32_t opcode)
{
    uint32_t immed = opcode & 0xffff;
    uint32_t mask = number_to_mask(immed << 16);
    uint8_t rs = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    
    uint8_t reg_rS = MapGPRForRead(tc, rs);
    uint8_t reg_rA = MapGPRForReadAndWrite(tc, ra);

    if (mask == 0) {
        uint8_t tmp = AllocARMRegister(tc);
        EMIT(tc,
            mov_immed_u16(tmp, immed, 1),
            orr_reg(reg_rA, reg_rS, tmp, LSL, 0)
        );
        FreeARMRegister(tc, tmp);
    } else {
        EMIT(tc, orr_immed(reg_rA, reg_rS, (mask >> 16) & 0x3f, mask & 0x3f));
    }

    tc->tc_PPCCodePtr++;
    AdvancePC(tc, 4);
    return 1;
}

static __used__ int EMIT_andi_dot(struct TranslatorContext *tc, uint32_t opcode)
{
    uint32_t immed = opcode & 0xffff;
    uint32_t mask = number_to_mask(immed);
    uint8_t rs = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    
    uint8_t reg_rS = MapGPRForRead(tc, rs);
    uint8_t reg_rA = MapGPRForReadAndWrite(tc, ra);

    if (mask == 0) {
        uint8_t tmp = AllocARMRegister(tc);
        EMIT(tc,
            mov_immed_u16(tmp, immed, 0),
            and_reg(reg_rA, reg_rS, tmp, LSL, 0)
        );
        FreeARMRegister(tc, tmp);
    } else {
        EMIT(tc, and_immed(reg_rA, reg_rS, (mask >> 16) & 0x3f, mask & 0x3f));
    }

    EMIT_set_crn_logic(tc, 0);

    tc->tc_PPCCodePtr++;
    AdvancePC(tc, 4);
    return 1;
}


static __used__ int EMIT_andis_dot(struct TranslatorContext *tc, uint32_t opcode)
{
    uint32_t immed = opcode & 0xffff;
    uint32_t mask = number_to_mask(immed << 16);
    uint8_t rs = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    
    uint8_t reg_rS = MapGPRForRead(tc, rs);
    uint8_t reg_rA = MapGPRForReadAndWrite(tc, ra);

    if (mask == 0) {
        uint8_t tmp = AllocARMRegister(tc);
        EMIT(tc,
            mov_immed_u16(tmp, immed, 1),
            and_reg(reg_rA, reg_rS, tmp, LSL, 0)
        );
        FreeARMRegister(tc, tmp);
    } else {
        EMIT(tc, and_immed(reg_rA, reg_rS, (mask >> 16) & 0x3f, mask & 0x3f));
    }

    EMIT_set_crn_logic(tc, 0);

    tc->tc_PPCCodePtr++;
    AdvancePC(tc, 4);
    return 1;
}

static __used__ int EMIT_xori(struct TranslatorContext *tc, uint32_t opcode)
{
    uint32_t immed = opcode & 0xffff;
    uint32_t mask = number_to_mask(immed);
    uint8_t rs = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    
    uint8_t reg_rS = MapGPRForRead(tc, rs);
    uint8_t reg_rA = MapGPRForReadAndWrite(tc, ra);

    if (mask == 0) {
        uint8_t tmp = AllocARMRegister(tc);
        EMIT(tc,
            mov_immed_u16(tmp, immed, 0),
            eor_reg(reg_rA, reg_rS, tmp, LSL, 0)
        );
        FreeARMRegister(tc, tmp);
    } else {
        EMIT(tc, eor_immed(reg_rA, reg_rS, (mask >> 16) & 0x3f, mask & 0x3f));
    }

    tc->tc_PPCCodePtr++;
    AdvancePC(tc, 4);
    return 1;
}

static __used__ int EMIT_xoris(struct TranslatorContext *tc, uint32_t opcode)
{
    uint32_t immed = opcode & 0xffff;
    uint32_t mask = number_to_mask(immed << 16);
    uint8_t rs = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    
    uint8_t reg_rS = MapGPRForRead(tc, rs);
    uint8_t reg_rA = MapGPRForReadAndWrite(tc, ra);

    if (mask == 0) {
        uint8_t tmp = AllocARMRegister(tc);
        EMIT(tc,
            mov_immed_u16(tmp, immed, 1),
            eor_reg(reg_rA, reg_rS, tmp, LSL, 0)
        );
        FreeARMRegister(tc, tmp);
    } else {
        EMIT(tc, eor_immed(reg_rA, reg_rS, (mask >> 16) & 0x3f, mask & 0x3f));
    }

    tc->tc_PPCCodePtr++;
    AdvancePC(tc, 4);
    return 1;
}

static __used__ int EMIT_isync(struct TranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 0x03fff801) return -1;

    EMIT(tc, 
        isb(),
        INSN_TO_LE(MARKER_STOP)
    );

    tc->tc_PPCCodePtr++;
    AdvancePC(tc, 4);
    return 1;
}

static __used__ int EMIT_mcrf(struct TranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 0x63F801) return -1;

    int dst = (opcode >> 23) & 7;
    int src = (opcode >> 18) & 7;

    uint8_t tmp = AllocARMRegister(tc);
    uint8_t cr = MapGPRForReadAndWrite(tc, CRn);

    EMIT(tc,
        lsr(tmp, cr, (7 - src) * 4),
        bfi(cr, tmp, (7 - dst) * 4, 4)
    );

    FreeARMRegister(tc, tmp);
    
    tc->tc_PPCCodePtr++;
    AdvancePC(tc, 4);
    return 1;
}

static __used__ int EMIT_crandc(struct TranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 1) return -1;

    uint8_t crd = (opcode >> 21) & 31;
    uint8_t cra = (opcode >> 16) & 31;
    uint8_t crb = (opcode >> 11) & 31;

    uint8_t cr_reg = MapGPRForReadAndWrite(tc, CRn);
    uint8_t tmp = AllocARMRegister(tc);

    EMIT(tc,
        neg_reg(tmp, cr_reg, LSR, (31 - crb)),
        and_reg(tmp, tmp, cr_reg, LSR, (31 - cra)),
        bfi(cr_reg, tmp, 31 - crd, 1)
    );

    FreeARMRegister(tc, tmp);
    
    tc->tc_PPCCodePtr++;
    AdvancePC(tc, 4);
    return 1;
}

static __used__ int EMIT_creqv(struct TranslatorContext *tc, uint32_t opcode)
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
        EMIT(tc,
            orr_immed(cr_reg, cr_reg, 1, (crd + 1) & 31)
        );
    }
    else {
        EMIT(tc,
            lsr(tmp, cr_reg, (31 - cra)),
            eon_reg(tmp, tmp, cr_reg, LSR, (31 - crb)),
            bfi(cr_reg, tmp, 31 - crd, 1)
        );
    }

    FreeARMRegister(tc, tmp);
    
    tc->tc_PPCCodePtr++;
    AdvancePC(tc, 4);
    return 1;
}

static __used__ int EMIT_crand(struct TranslatorContext *tc, uint32_t opcode)
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
        EMIT(tc,
            lsr(tmp, cr_reg, (31 - cra)),
            bfi(cr_reg, tmp, 31 - crd, 1)
        );
    } else {
        EMIT(tc,
            lsr(tmp, cr_reg, (31 - cra)),
            and_reg(tmp, tmp, cr_reg, LSR, (31 - crb)),
            bfi(cr_reg, tmp, 31 - crd, 1)
        );
    }

    FreeARMRegister(tc, tmp);
    
    tc->tc_PPCCodePtr++;
    AdvancePC(tc, 4);
    return 1;
}

static __used__ int EMIT_crnand(struct TranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 1) return -1;

    uint8_t crd = (opcode >> 21) & 31;
    uint8_t cra = (opcode >> 16) & 31;
    uint8_t crb = (opcode >> 11) & 31;

    uint8_t cr_reg = MapGPRForReadAndWrite(tc, CRn);
    uint8_t tmp = AllocARMRegister(tc);

    EMIT(tc,
        lsr(tmp, cr_reg, (31 - cra)),
        and_reg(tmp, tmp, cr_reg, LSR, (31 - crb)),
        neg_reg(tmp, tmp, LSL, 0),
        bfi(cr_reg, tmp, 31 - crd, 1)
    );

    FreeARMRegister(tc, tmp);
    
    tc->tc_PPCCodePtr++;
    AdvancePC(tc, 4);
    return 1;
}

static __used__ int EMIT_crnor(struct TranslatorContext *tc, uint32_t opcode)
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
        EMIT(tc,
            eor_immed(cr_reg, cr_reg, 1, (crd + 1) & 31)
        );
    } else {
        EMIT(tc,
            lsr(tmp, cr_reg, (31 - cra)),
            orr_reg(tmp, tmp, cr_reg, LSR, (31 - crb)),
            neg_reg(tmp, tmp, LSL, 0),
            bfi(cr_reg, tmp, 31 - crd, 1)
        );
    }

    FreeARMRegister(tc, tmp);
    
    tc->tc_PPCCodePtr++;
    AdvancePC(tc, 4);
    return 1;
}

static __used__ int EMIT_cror(struct TranslatorContext *tc, uint32_t opcode)
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
        EMIT(tc,
            lsr(tmp, cr_reg, (31 - cra)),
            bfi(cr_reg, tmp, 31 - crd, 1)
        );
    } else {
        EMIT(tc,
            lsr(tmp, cr_reg, (31 - cra)),
            orr_reg(tmp, tmp, cr_reg, LSR, (31 - crb)),
            bfi(cr_reg, tmp, 31 - crd, 1)
        );
    }

    FreeARMRegister(tc, tmp);
    
    tc->tc_PPCCodePtr++;
    AdvancePC(tc, 4);
    return 1;
}

static __used__ int EMIT_crorc(struct TranslatorContext *tc, uint32_t opcode)
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
        EMIT(tc,
            orr_immed(cr_reg, cr_reg, 1, (crd + 1) & 31)
        );
    } else {
        EMIT(tc,
            lsr(tmp, cr_reg, (31 - cra)),
            orn_reg(tmp, tmp, cr_reg, LSR, (31 - crb)),
            bfi(cr_reg, tmp, 31 - crd, 1)
        );
    }

    FreeARMRegister(tc, tmp);
    
    tc->tc_PPCCodePtr++;
    AdvancePC(tc, 4);
    return 1;
}

static __used__ int EMIT_crxor(struct TranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 1) return -1;

    uint8_t crd = (opcode >> 21) & 31;
    uint8_t cra = (opcode >> 16) & 31;
    uint8_t crb = (opcode >> 11) & 31;

    uint8_t cr_reg = MapGPRForReadAndWrite(tc, CRn);

    if (cra == crb) {
        /* Bit clear */
        EMIT(tc,
            bic_immed(cr_reg, cr_reg, 1, (crd + 1) & 31)
        );
    } else {
        uint8_t tmp = AllocARMRegister(tc);
        EMIT(tc,
            lsr(tmp, cr_reg, (31 - cra)),
            orr_reg(tmp, tmp, cr_reg, LSR, (31 - crb)),
            bfi(cr_reg, tmp, 31 - crd, 1)
        );
        FreeARMRegister(tc, tmp);
    }

    tc->tc_PPCCodePtr++;
    AdvancePC(tc, 4);
    return 1;
}

static __used__ int EMIT_bclrx(struct TranslatorContext *tc, uint32_t opcode)
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

//    kprintf("bo = %d, bi = %d, take_branch = %d\n", bo, bi, take_branch);

    /* Branch always */
    if (bo & 0b10100) {
        uint8_t success = 0;
        uint32_t *last_pc = PopReturnAddress(&success);

        /* if LR needs to be updated, do it now */
        if (update_lr) {
            int8_t pc_offset = 4;
            uint8_t tmp = AllocARMRegister(tc);

            GetOffsetPC(tc, &pc_offset);

            PushReturnAddress(tc->tc_PPCCodePtr + 1);

            EMIT(tc, bic_immed(tmp, REG_LR, 2, 0));

            if (pc_offset >= 0) {
                EMIT(tc, add_immed(REG_LR, REG_PC, pc_offset));
            } else {
                EMIT(tc, sub_immed(REG_LR, REG_PC, -pc_offset));
            }

            EMIT(tc, mov_reg(REG_PC, tmp));

            FreeARMRegister(tc, tmp);
        }
        else
        {
            /* Move LR to PC */
            EMIT(tc, bic_immed(REG_PC, REG_LR, 2, 0));
        }

        if (success) {
            tc->tc_PPCCodePtr = last_pc;
        } else {
            /* The return address stack was not available, stop now */
            EMIT(tc, INSN_TO_LE(MARKER_STOP));
        }

        ResetOffsetPC(tc);
    } else {
        (void)bi;
        (void)take_branch;
        return -1;
    }

    return 1;
}

static __used__ int EMIT_bcctrx(struct TranslatorContext *tc, uint32_t opcode)
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
    if (bo & 0b10100) {
        /* if LR needs to be updated, do it now */
        if (update_lr) {
            int8_t pc_offset = 4;

            GetOffsetPC(tc, &pc_offset);

            if (pc_offset >= 0) {
                EMIT(tc, add_immed(REG_LR, REG_PC, pc_offset));
            } else {
                EMIT(tc, sub_immed(REG_LR, REG_PC, -pc_offset));
            }

            EMIT(tc, bic_immed(REG_PC, REG_CTR, 2, 0));
        }
        else
        {
            /* Move LR to PC */
            EMIT(tc, bic_immed(REG_PC, REG_CTR, 2, 0));
        }

        /* The return address stack was not available, stop now */
        EMIT(tc, INSN_TO_LE(MARKER_STOP));

        ResetOffsetPC(tc);
    } else {
        kprintf("UNIMPLEMENTED bcctrlx with condition\n");
        (void)bi;
        (void)take_branch;
        return -1;
    }

    return 1;
}

static __used__ int EMIT_mfcr(struct TranslatorContext *tc, uint32_t opcode)
{
    /* Stanity check */
    if (opcode & 0x001ff801) return -1;

    uint8_t rd = (opcode >> 21) & 31;
    uint8_t reg_rd = MapGPRForWrite(tc, rd);
    uint8_t reg_cr = IsGPRMapped(tc, CRn);

    if (reg_cr != 0xff) {
        EMIT(tc, mov_reg(reg_rd, reg_cr));
    } else {
        EMIT(tc, mov_simd_to_reg(reg_rd, REG_CR));
    }

    tc->tc_PPCCodePtr++;
    AdvancePC(tc, 4);
    return 1;
}

static __used__ int EMIT_mftb(struct TranslatorContext *tc, uint32_t opcode)
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

    EMIT(tc, mrs(tmp, 3, 3, 14, 0, 1));

    if (tbr == 268) // TBL
        EMIT(tc, mov_reg(reg_rd, tmp));
    else // TBH
        EMIT(tc, lsr64(reg_rd, tmp, 32));

    FreeARMRegister(tc, tmp);

    tc->tc_PPCCodePtr++;
    AdvancePC(tc, 4);
    return 1;
}

static __used__ int EMIT_mcrxr(struct TranslatorContext *tc, uint32_t opcode)
{
    /* Stanity check */
    if (opcode & 0x007ff801) return -1;

    uint8_t tmp = AllocARMRegister(tc);
    uint8_t xer_reg = MapGPRForReadAndWrite(tc, XERn);
    uint8_t cr_reg = MapGPRForReadAndWrite(tc, CRn);
    uint8_t crn = (opcode >> 23) & 7;

    EMIT(tc,
        lsr(tmp, xer_reg, 28),
        bfi(cr_reg, tmp, 4 * (7 - crn), 4),
        bic_immed(xer_reg, xer_reg, 4, 4)
    );

    FreeARMRegister(tc, tmp);

    tc->tc_PPCCodePtr++;
    AdvancePC(tc, 4);
    return 1;
}

static __used__ int EMIT_mfspr(struct TranslatorContext *tc, uint32_t opcode)
{
    /* Stanity check */
    if (opcode & 1) return -1;

    uint32_t spr = (opcode >> 11) & 0x3ff;
    uint8_t rd = (opcode >> 21) & 31;
    /* tbr is a split field, fix it */
    spr = ((spr >> 5) & 0x1f) | ((spr & 0x1f) << 5);

    uint8_t reg_rd = MapGPRForWrite(tc, rd);

    if (spr & 0x10) {

        /* Accessing supervisor level SPRs */
        /* TODO: Implement Supervisor SPRs */
        return -1;

    } else {
        /* Accessing user level SPRs */
        switch(spr) {
            case 1:
                EMIT(tc, mov_reg(reg_rd, MapGPRForRead(tc, XERn)));
                break;
            case 8:
                EMIT(tc, mov_reg(reg_rd, MapGPRForRead(tc, LRn)));
                break;
            case 9:
                EMIT(tc, mov_reg(reg_rd, MapGPRForRead(tc, CTRn)));
                break;
            default:
                return -1;
        }
    }

    tc->tc_PPCCodePtr++;
    AdvancePC(tc, 4);
    return 1;
}

static __used__ int EMIT_mtspr(struct TranslatorContext *tc, uint32_t opcode)
{
    /* Stanity check */
    if (opcode & 1) return -1;

    uint32_t spr = (opcode >> 11) & 0x3ff;
    uint8_t rs = (opcode >> 21) & 31;
    /* tbr is a split field, fix it */
    spr = ((spr >> 5) & 0x1f) | ((spr & 0x1f) << 5);

    uint8_t reg_rs = MapGPRForRead(tc, rs);

    if (spr & 0x10) {

        /* Accessing supervisor level SPRs */
        /* TODO: Implement Supervisor SPRs */
        return -1;

    } else {
        /* Accessing user level SPRs */
        switch(spr) {
            case 1:
            {
                uint8_t reg_xer = IsGPRMapped(tc, XERn);
                if (reg_xer != 0xff) {
                    EMIT(tc, mov_reg(reg_xer, reg_rs));
                    SetDirtyGPR(tc, XERn);
                } else {
                    EMIT(tc, mov_reg_to_simd(REG_XER, reg_rs));
                }
                break;
            }
            case 8:
                EMIT(tc, mov_reg(MapGPRForWrite(tc, LRn), reg_rs));
                ResetReturnStack();
                break;
            case 9:
                EMIT(tc, mov_reg(MapGPRForWrite(tc, CTRn), reg_rs));
                break;
            default:
                return -1;
        }
    }

    tc->tc_PPCCodePtr++;
    AdvancePC(tc, 4);
    return 1;
}

static __used__ int EMIT_rlwinmx(struct TranslatorContext *tc, uint32_t opcode)
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

    /* If sh is set, rotate left */
    if (sh) {
        EMIT(tc, ror(reg_ra, reg_rs, (32 - sh) & 31));
        and_src = reg_ra;
    }

    /* Mask result if me - mb is not 31 */
    if (((me - mb) & 31) != 31)
    {
        if (mb <= me)
        {
            /* mb < me - mask of type 0x0f...f0 */
            EMIT(tc, update_cr ?
                ands_immed(reg_ra, and_src, 1 + me - mb, 31 & (me + 1)) :
                and_immed(reg_ra, and_src, 1 + me - mb, 31 & (me + 1))
            );
        }
        else if (me < mb)
        {
            /* mb < me - mask of type 0xf..0..f */
            EMIT(tc, update_cr ?
                ands_immed(reg_ra, and_src, mb - me - 1, me + 1) :
                and_immed(reg_ra, and_src, mb - me - 1, me + 1)
            );
        }
    } else if (update_cr) {
        if (!sh)
            EMIT(tc, adds_immed(reg_ra, reg_rs, 0));
        else
            EMIT(tc, tst_immed(reg_ra, 32, 0));
    }

    if (update_cr) 
    {
        EMIT_set_crn_logic(tc, 0);
    }

    tc->tc_PPCCodePtr++;
    AdvancePC(tc, 4);
    return 1;
}

static __used__ int EMIT_orx(struct TranslatorContext *tc, uint32_t opcode)
{
    uint8_t update_cr = opcode & 1;
    uint8_t rs = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;

    uint8_t reg_rs = MapGPRForRead(tc, rs);
    uint8_t reg_rb = MapGPRForRead(tc, rb);
    uint8_t reg_ra = MapGPRForWrite(tc, ra);

    if (rb == rs) {
        EMIT(tc, mov_reg(reg_ra, reg_rs));
    } else {
        EMIT(tc, orr_reg(reg_ra, reg_rs, reg_rb, LSL, 0));
    }

    if (update_cr) {
        EMIT(tc, cmp_immed(reg_ra, 0));
        EMIT_set_crn_logic(tc, 0);
    }

    tc->tc_PPCCodePtr++;
    AdvancePC(tc, 4);
    return 1;
}

static __used__ int EMIT_andx(struct TranslatorContext *tc, uint32_t opcode)
{
    uint8_t update_cr = opcode & 1;
    uint8_t rs = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;

    uint8_t reg_rs = MapGPRForRead(tc, rs);
    uint8_t reg_rb = MapGPRForRead(tc, rb);
    uint8_t reg_ra = MapGPRForWrite(tc, ra);

    if (update_cr) {
        EMIT(tc, ands_reg(reg_ra, reg_rs, reg_rb, LSL, 0));
        EMIT_set_crn_logic(tc, 0);
    } else {
        EMIT(tc, and_reg(reg_ra, reg_rs, reg_rb, LSL, 0));
    }

    tc->tc_PPCCodePtr++;
    AdvancePC(tc, 4);
    return 1;
}

static __used__ int EMIT_norx(struct TranslatorContext *tc, uint32_t opcode)
{
    uint8_t update_cr = opcode & 1;
    uint8_t rs = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;

    uint8_t reg_rs = MapGPRForRead(tc, rs);
    uint8_t reg_rb = MapGPRForRead(tc, rb);
    uint8_t reg_ra = MapGPRForWrite(tc, ra);

    if (rs == rb) {
        EMIT(tc, mvn_reg(reg_ra, reg_rb, LSL, 0));
    } else {
        EMIT(tc, 
            orr_reg(reg_ra, reg_rs, reg_rb, LSL, 0),
            mvn_reg(reg_ra, reg_ra, LSL, 0)
        );
    }
    
    if (update_cr) {
        EMIT(tc, cmp_immed(reg_ra, 0));
        EMIT_set_crn_logic(tc, 0);
    }

    tc->tc_PPCCodePtr++;
    AdvancePC(tc, 4);
    return 1;
}

static __used__ int EMIT_andcx(struct TranslatorContext *tc, uint32_t opcode)
{
    uint8_t update_cr = opcode & 1;
    uint8_t rs = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;

    uint8_t reg_rs = MapGPRForRead(tc, rs);
    uint8_t reg_rb = MapGPRForRead(tc, rb);
    uint8_t reg_ra = MapGPRForWrite(tc, ra);
    
    if (update_cr) {
        EMIT(tc, bics_reg(reg_ra, reg_rs, reg_rb, LSL, 0));
        EMIT_set_crn_logic(tc, 0);
    } else {
        EMIT(tc, bic_reg(reg_ra, reg_rs, reg_rb, LSL, 0));
    }

    tc->tc_PPCCodePtr++;
    AdvancePC(tc, 4);
    return 1;
}

static __used__ int EMIT_eqvx(struct TranslatorContext *tc, uint32_t opcode)
{
    uint8_t update_cr = opcode & 1;
    uint8_t rs = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;

    uint8_t reg_rs = MapGPRForRead(tc, rs);
    uint8_t reg_rb = MapGPRForRead(tc, rb);
    uint8_t reg_ra = MapGPRForWrite(tc, ra);

    if (rb == rs) {
        EMIT(tc, mvn_reg(reg_ra, WZR, LSL, 0));
    } else {
        EMIT(tc, eon_reg(reg_ra, reg_rs, reg_rb, LSL, 0));
    }

    if (update_cr) {
        EMIT(tc, cmp_immed(reg_ra, 0));
        EMIT_set_crn_logic(tc, 0);
    }

    tc->tc_PPCCodePtr++;
    AdvancePC(tc, 4);
    return 1;
}

static __used__ int EMIT_xorx(struct TranslatorContext *tc, uint32_t opcode)
{
    uint8_t update_cr = opcode & 1;
    uint8_t rs = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;

    uint8_t reg_rs = MapGPRForRead(tc, rs);
    uint8_t reg_rb = MapGPRForRead(tc, rb);
    uint8_t reg_ra = MapGPRForWrite(tc, ra);

    EMIT(tc, eor_reg(reg_ra, reg_rs, reg_rb, LSL, 0));

    if (update_cr) {
        EMIT(tc, cmp_immed(reg_ra, 0));
        EMIT_set_crn_logic(tc, 0);
    }

    tc->tc_PPCCodePtr++;
    AdvancePC(tc, 4);
    return 1;
}

static __used__ int EMIT_orcx(struct TranslatorContext *tc, uint32_t opcode)
{
    uint8_t update_cr = opcode & 1;
    uint8_t rs = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;

    uint8_t reg_rs = MapGPRForRead(tc, rs);
    uint8_t reg_rb = MapGPRForRead(tc, rb);
    uint8_t reg_ra = MapGPRForWrite(tc, ra);

    EMIT(tc, orn_reg(reg_ra, reg_rs, reg_rb, LSL, 0));

    if (update_cr) {
        EMIT(tc, cmp_immed(reg_ra, 0));
        EMIT_set_crn_logic(tc, 0);
    }

    tc->tc_PPCCodePtr++;
    AdvancePC(tc, 4);
    return 1;
}

static __used__ int EMIT_nandx(struct TranslatorContext *tc, uint32_t opcode)
{
    uint8_t update_cr = opcode & 1;
    uint8_t rs = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;

    uint8_t reg_rs = MapGPRForRead(tc, rs);
    uint8_t reg_rb = MapGPRForRead(tc, rb);
    uint8_t reg_ra = MapGPRForWrite(tc, ra);

    EMIT(tc, 
        and_reg(reg_ra, reg_rs, reg_rb, LSL, 0),
        mvn_reg(reg_ra, reg_ra, LSL, 0)
    );

    if (update_cr) {
        EMIT(tc, cmp_immed(reg_ra, 0));
        EMIT_set_crn_logic(tc, 0);
    }

    tc->tc_PPCCodePtr++;
    AdvancePC(tc, 4);
    return 1;
}

static __used__ int EMIT_mulli(struct TranslatorContext *tc, uint32_t opcode)
{
    uint8_t rd = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint16_t immed = opcode & 0xffff;

    uint8_t reg_ra = MapGPRForRead(tc, ra);
    uint8_t reg_rd = MapGPRForWrite(tc, rd);
    uint8_t tmp = AllocARMRegister(tc);

    if (immed & 0x8000) {
        EMIT(tc, movn_immed_u16(tmp, ~immed & 0xffff, 0));
    } else {
        EMIT(tc, mov_immed_u16(tmp, immed, 0));
    }

    EMIT(tc, mul(reg_rd, reg_ra, tmp));

    FreeARMRegister(tc, tmp);

    tc->tc_PPCCodePtr++;
    AdvancePC(tc, 4);
    return 1;
}

static __used__ int EMIT_addx(struct TranslatorContext *tc, uint32_t opcode)
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
        EMIT(tc, adds_reg(reg_rd, reg_ra, reg_rb, LSL, 0));
    } else {
        EMIT(tc, add_reg(reg_rd, reg_ra, reg_rb, LSL, 0));
    }

    if (oe) {
        uint8_t reg_xer = MapGPRForReadAndWrite(tc, XERn);
        uint8_t tmp = AllocARMRegister(tc);

        EMIT(tc,
            orr_immed(reg_xer, reg_xer, 2, 2),
            bic_immed(tmp, reg_xer, 1, 2),
            csel(reg_xer, reg_xer, tmp, A64_CC_VS)
        );

        FreeARMRegister(tc, tmp);
    }

    if (rc) {
        EMIT_set_crn_signed(tc, 0);
    }

    tc->tc_PPCCodePtr++;
    AdvancePC(tc, 4);
    return 1;
}

static __used__ int EMIT_subfx(struct TranslatorContext *tc, uint32_t opcode)
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
        EMIT(tc, subs_reg(reg_rd, reg_rb, reg_ra, LSL, 0));
    } else {
        EMIT(tc, sub_reg(reg_rd, reg_rb, reg_ra, LSL, 0));
    }

    if (oe) {
        uint8_t reg_xer = MapGPRForReadAndWrite(tc, XERn);
        uint8_t tmp = AllocARMRegister(tc);

        EMIT(tc,
            orr_immed(reg_xer, reg_xer, 2, 2),
            bic_immed(tmp, reg_xer, 1, 2),
            csel(reg_xer, reg_xer, tmp, A64_CC_VS)
        );

        FreeARMRegister(tc, tmp);
    }

    if (rc) {
        EMIT_set_crn_signed(tc, 0);
    }

    tc->tc_PPCCodePtr++;
    AdvancePC(tc, 4);
    return 1;
}

static __used__ int EMIT_subfic(struct TranslatorContext *tc, uint32_t opcode)
{
    uint8_t rd = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint16_t imm = (opcode & 0xffff);

    uint8_t reg_ra = MapGPRForRead(tc, ra);
    uint8_t reg_rd = MapGPRForWrite(tc, rd);
    uint8_t tmp = AllocARMRegister(tc);
    uint8_t reg_xer = MapGPRForReadAndWrite(tc, XERn);

    if (imm & 0x8000) {
        EMIT(tc, movn_immed_u16(tmp, ~imm & 0xffff, 0));
    } else {
        EMIT(tc, mov_immed_u16(tmp, imm, 0));
    }

    // TODO: Verify if flag set correctly

    EMIT(tc, 
        bic_immed(reg_xer, reg_xer, 1, 3),      // Clear CA flag in xer
        subs_reg(reg_rd, tmp, reg_ra, LSL, 0),
        orr_immed(tmp, reg_xer, 1, 3),          // Set CA flag from xer into tmp
        csel(reg_xer, tmp, reg_xer, A64_CC_CC)  // Select CA set or clear in XER depending on A64 C flag
    );

    FreeARMRegister(tc, tmp);

    tc->tc_PPCCodePtr++;
    AdvancePC(tc, 4);
    return 1;
}

static __used__ int EMIT_subfcx(struct TranslatorContext *tc, uint32_t opcode)
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

    EMIT(tc, 
        bic_immed(reg_xer, reg_xer, 1, 3),      // Clear CA flag in xer
        subs_reg(reg_rd, reg_rb, reg_ra, LSL, 0),
        orr_immed(tmp, reg_xer, 1, 3),          // Set CA flag from xer into tmp
        csel(reg_xer, tmp, reg_xer, A64_CC_CC)  // Select CA set or clear in XER depending on A64 C flag
    );

    if (oe) {
        EMIT(tc,
            orr_immed(reg_xer, reg_xer, 2, 2),
            bic_immed(tmp, reg_xer, 1, 2),
            csel(reg_xer, reg_xer, tmp, A64_CC_VS)
        );
    }

    FreeARMRegister(tc, tmp);

    if (rc) {
        EMIT_set_crn_signed(tc, 0);
    }

    tc->tc_PPCCodePtr++;
    AdvancePC(tc, 4);
    return 1;
}

static __used__ int EMIT_addex(struct TranslatorContext *tc, uint32_t opcode)
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

    EMIT(tc, 
        bfxil(tmp, reg_xer, 29, 1),                 // Extract CA field into tmp
        subs_immed(WZR, tmp, 1),                    // Subtract 1, this will set carry bit
        bic_immed(reg_xer, reg_xer, 1, 3),          // Clear CA flag in xer
        adcs(reg_rd, reg_ra, reg_rb),               // Add with carry
        orr_immed(tmp, reg_xer, 1, 3),              // Set CA flag from xer into tmp
        csel(reg_xer, tmp, reg_xer, A64_CC_CS)      // Select CA set or clear in XER depending on A64 C flag
    );

    if (oe) {
        EMIT(tc,
            orr_immed(reg_xer, reg_xer, 2, 2),
            bic_immed(tmp, reg_xer, 1, 2),
            csel(reg_xer, reg_xer, tmp, A64_CC_VS)
        );
    }

    FreeARMRegister(tc, tmp);

    if (rc) {
        EMIT_set_crn_signed(tc, 0);
    }

    tc->tc_PPCCodePtr++;
    AdvancePC(tc, 4);
    return 1;
}

static __used__ int EMIT_cmp(struct TranslatorContext *tc, uint32_t opcode)
{
    /* sanity check */
    if (opcode & 0x00600001) return -1;

    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;
    uint8_t cr = (opcode >> 23) & 7;

    uint8_t reg_ra = MapGPRForRead(tc, ra);
    uint8_t reg_rb = MapGPRForRead(tc, rb);

    EMIT(tc, cmp_reg(reg_ra, reg_rb, LSL, 0));

    EMIT_set_crn_signed(tc, cr);

    tc->tc_PPCCodePtr++;
    AdvancePC(tc, 4);
    return 1;
}

static __used__ int EMIT_cmpl(struct TranslatorContext *tc, uint32_t opcode)
{
    /* sanity check */
    if (opcode & 0x00600001) return -1;

    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;
    uint8_t cr = (opcode >> 23) & 7;

    uint8_t reg_ra = MapGPRForRead(tc, ra);
    uint8_t reg_rb = MapGPRForRead(tc, rb);

    EMIT(tc, cmp_reg(reg_ra, reg_rb, LSL, 0));

    EMIT_set_crn_unsigned(tc, cr);

    tc->tc_PPCCodePtr++;
    AdvancePC(tc, 4);
    return 1;
}

static __used__ int EMIT_negx(struct TranslatorContext *tc, uint32_t opcode)
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
        EMIT(tc, negs_reg(reg_rd, reg_ra, LSL, 0));
    } else {
        EMIT(tc, neg_reg(reg_rd, reg_ra, LSL, 0));
    }

    if (oe) {
        uint8_t reg_xer = MapGPRForReadAndWrite(tc, XERn);
        uint8_t tmp = AllocARMRegister(tc);

        EMIT(tc,
            orr_immed(reg_xer, reg_xer, 2, 2),
            bic_immed(tmp, reg_xer, 1, 2),
            csel(reg_xer, reg_xer, tmp, A64_CC_VS)
        );

        FreeARMRegister(tc, tmp);
    }

    if (rc) {
        EMIT_set_crn_signed(tc, 0);
    }

    tc->tc_PPCCodePtr++;
    AdvancePC(tc, 4);
    return 1;
}

static __used__ int EMIT_divwux(struct TranslatorContext *tc, uint32_t opcode)
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

        EMIT(tc,
            cmp_immed(reg_rb, 0),
            orr_immed(reg_xer, reg_xer, 2, 2),
            bic_immed(tmp, reg_xer, 1, 2),
            csel(reg_xer, reg_xer, tmp, A64_CC_EQ)
        );

        FreeARMRegister(tc, tmp);
    }

    EMIT(tc, udiv(reg_rd, reg_ra, reg_rb));

    if (rc) {
        EMIT(tc, cmp_immed(reg_rd, 0));
        EMIT_set_crn_signed(tc, 0);
    }

    tc->tc_PPCCodePtr++;
    AdvancePC(tc, 4);
    return 1;
}

static __used__ int EMIT_divwx(struct TranslatorContext *tc, uint32_t opcode)
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

        EMIT(tc,
            cmp_immed(reg_rb, 0),
            orr_immed(reg_xer, reg_xer, 2, 2),
            bic_immed(tmp, reg_xer, 1, 2),
            csel(reg_xer, reg_xer, tmp, A64_CC_EQ)
        );

        FreeARMRegister(tc, tmp);
    }

    EMIT(tc, sdiv(reg_rd, reg_ra, reg_rb));

    if (rc) {
        EMIT(tc, cmp_immed(reg_rd, 0));
        EMIT_set_crn_signed(tc, 0);
    }

    tc->tc_PPCCodePtr++;
    AdvancePC(tc, 4);
    return 1;
}

static __used__ int EMIT_mullwx(struct TranslatorContext *tc, uint32_t opcode)
{
    uint8_t rd = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;
    
    uint8_t oe = (opcode >> 10) & 1;
    uint8_t rc = opcode & 1;

    uint8_t reg_ra = MapGPRForRead(tc, ra);
    uint8_t reg_rb = MapGPRForRead(tc, rb);
    uint8_t reg_rd = MapGPRForWrite(tc, rd);

    EMIT(tc, smull(reg_rd, reg_ra, reg_rb));

    if (oe) {
        uint8_t tmp = AllocARMRegister(tc);
        uint8_t reg_xer = MapGPRForReadAndWrite(tc, XERn);

        EMIT(tc,
            sxtw64(tmp, reg_rd),
            cmp_reg(tmp, reg_rd, LSL, 0),
            orr_immed(reg_xer, reg_xer, 2, 2),
            bic_immed(tmp, reg_xer, 1, 2),
            csel(reg_xer, reg_xer, tmp, A64_CC_NE)
        );

        FreeARMRegister(tc, tmp);
    }

    if (rc) {
        EMIT(tc, cmp_immed(reg_rd, 0));
        EMIT_set_crn_signed(tc, 0);
    }

    tc->tc_PPCCodePtr++;
    AdvancePC(tc, 4);
    return 1;
}

static __used__ int EMIT_srwx(struct TranslatorContext *tc, uint32_t opcode)
{
    uint8_t rs = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;
    
    uint8_t rc = opcode & 1;

    uint8_t reg_ra = MapGPRForRead(tc, ra);
    uint8_t reg_rb = MapGPRForRead(tc, rb);
    uint8_t reg_rs = MapGPRForWrite(tc, rs);

    EMIT(tc, lsrv(reg_ra, reg_rs, reg_rb));

    if (rc) {
        EMIT(tc, cmp_immed(reg_ra, 0));
        EMIT_set_crn_signed(tc, 0);
    }

    tc->tc_PPCCodePtr++;
    AdvancePC(tc, 4);
    return 1;
}

static __used__ int EMIT_srawix(struct TranslatorContext *tc, uint32_t opcode)
{
    uint8_t rs = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t sh = (opcode >> 11) & 31;
    
    uint8_t rc = opcode & 1;

    uint8_t reg_ra = MapGPRForRead(tc, ra);
    uint8_t reg_rs = MapGPRForWrite(tc, rs);

    uint8_t tmp = AllocARMRegister(tc);
    uint8_t reg_xer = MapGPRForReadAndWrite(tc, XERn);

    if (sh != 0) {
        EMIT(tc,
            /* Test if source is signed */
            tst_immed(reg_rs, 1, 1),
            /* If source is signed, put reg_rs to tmp, otherwise zero it */
            csel(tmp, reg_rs, XZR, A64_CC_EQ),
            /* Check the mask - if any bit is 1, set CA to 1 */
            tst_immed(tmp, sh, 0),
            orr_immed(reg_xer, reg_xer, 1, 3),
            bic_immed(tmp, reg_xer, 1, 3),
            csel(reg_xer, reg_xer, tmp, A64_CC_NE)
        );
    } else {
        EMIT(tc, bic_immed(reg_xer, reg_xer, 1, 3));
    }
    
    EMIT(tc, asr(reg_ra, reg_rs, sh));

    FreeARMRegister(tc, tmp);

    if (rc) {
        EMIT(tc, cmp_immed(reg_ra, 0));
        EMIT_set_crn_signed(tc, 0);
    }

    tc->tc_PPCCodePtr++;
    AdvancePC(tc, 4);
    return 1;
}

static __used__ int EMIT_srawx(struct TranslatorContext *tc, uint32_t opcode)
{
    uint8_t rs = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;
    
    uint8_t rc = opcode & 1;

    uint8_t reg_ra = MapGPRForRead(tc, ra);
    uint8_t reg_rb = MapGPRForRead(tc, rb);
    uint8_t reg_rs = MapGPRForWrite(tc, rs);

    uint8_t tmp = AllocARMRegister(tc);
    uint8_t mask = AllocARMRegister(tc);
    uint8_t reg_xer = MapGPRForReadAndWrite(tc, XERn);

    EMIT(tc,
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
    );
    
    EMIT(tc, asrv(reg_ra, reg_rs, reg_rb));

    FreeARMRegister(tc, mask);
    FreeARMRegister(tc, tmp);

    if (rc) {
        EMIT(tc, cmp_immed(reg_ra, 0));
        EMIT_set_crn_signed(tc, 0);
    }

    tc->tc_PPCCodePtr++;
    AdvancePC(tc, 4);
    return 1;
}

static __used__ int EMIT_extsbx(struct TranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 0x0000f800) return -1;

    uint8_t rs = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    
    uint8_t rc = opcode & 1;

    uint8_t reg_ra = MapGPRForRead(tc, ra);
    uint8_t reg_rs = MapGPRForWrite(tc, rs);
    
    EMIT(tc, sxtb(reg_ra, reg_rs));

    if (rc) {
        EMIT(tc, cmp_immed(reg_ra, 0));
        EMIT_set_crn_logic(tc, 0);
    }

    tc->tc_PPCCodePtr++;
    AdvancePC(tc, 4);
    return 1;
}

static __used__ int EMIT_extshx(struct TranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 0x0000f800) return -1;

    uint8_t rs = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    
    uint8_t rc = opcode & 1;

    uint8_t reg_ra = MapGPRForRead(tc, ra);
    uint8_t reg_rs = MapGPRForWrite(tc, rs);
    
    EMIT(tc, sxth(reg_ra, reg_rs));

    if (rc) {
        EMIT(tc, cmp_immed(reg_ra, 0));
        EMIT_set_crn_logic(tc, 0);
    }

    tc->tc_PPCCodePtr++;
    AdvancePC(tc, 4);
    return 1;
}

static __used__ int EMIT_eieio(struct TranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 0x03fff800) return -1;

    EMIT(tc, dmb_sy());

    tc->tc_PPCCodePtr++;
    AdvancePC(tc, 4);
    return 1;
}

static __used__ int EMIT_sync(struct TranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 0x03fff800) return -1;

    EMIT(tc, isb());

    tc->tc_PPCCodePtr++;
    AdvancePC(tc, 4);
    return 1;
}

static __used__ int EMIT_mtcrf(struct TranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 0x00100801) return -1;

    uint8_t rs = (opcode >> 21) & 31;
    uint8_t mask = (opcode >> 12) & 255;

    uint8_t reg_rs = MapGPRForRead(tc, rs);
    uint8_t reg_cr = MapGPRForReadAndWrite(tc, CRn);

    if (mask == 0xff) {
        EMIT(tc, mov_reg(reg_cr, reg_rs));
    } else if (mask == 0) {
        EMIT(tc, mov_reg(reg_cr, WZR));
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

            EMIT_LoadImmediate(tc, imm, mask32);
            EMIT(tc,
                bic_reg(reg_cr, reg_cr, imm, LSL, 0),
                and_reg(tmp, tmp, imm, LSL, 0),
                orr_reg(reg_cr, reg_cr, tmp, LSL, 0)
            );

            FreeARMRegister(tc, imm);
        } else {
            EMIT(tc,
                bic_immed(reg_cr, reg_cr, (mask >> 16) & 0x3f, mask & 0x3f),
                and_immed(tmp, tmp, (mask >> 16) & 0x3f, mask & 0x3f),
                orr_reg(reg_cr, reg_cr, tmp, LSL, 0)
            );
        }

        FreeARMRegister(tc, tmp);
    }

    tc->tc_PPCCodePtr++;
    AdvancePC(tc, 4);
    return 1;
}

static inline int globalDebug() {
    extern int debug;
    return debug;
}

static inline int globalDisasm() {
    extern int disasm;
    return disasm;
}

static inline int EMIT_Group_19(struct TranslatorContext *tc, uint32_t opcode)
{
    uint32_t secondary = (opcode >> 1) & 0x3ff;

//    kprintf("Group 19, secondary=%d\n");

    switch (secondary) {
        case 0b0000000000: return EMIT_mcrf(tc, opcode);
        case 0b0000010000: return EMIT_bclrx(tc, opcode);
        case 0b0000100001: return EMIT_crnor(tc, opcode);
        //case 0b0000110010: return EMIT_rfi(tc, opcode);
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

static inline int EMIT_Group_31(struct TranslatorContext *tc, uint32_t opcode)
{
    uint32_t secondary = (opcode >> 1) & 0x3ff;

    switch (secondary) {
        case 0b0000000000: return EMIT_cmp(tc, opcode);
        //case 0b0000000100: return EMIT_tw(tc, opcode);        
        case 0b0000001000: return EMIT_subfcx(tc, opcode);
        //case 0b0000001010: return EMIT_addcx(tc, opcode);
        //case 0b0000001011: return EMIT_mulhwux(tc, opcode);
        case 0b0000010011: return EMIT_mfcr(tc, opcode);
        //case 0b0000010100: return EMIT_lwarx(tc, opcode);
        case 0b0000010111: return EMIT_lwzx(tc, opcode);
        //case 0b0000011000: return EMIT_slwx(tc, opcode);
        //case 0b0000011010: return EMIT_cntlzwx(tc, opcode);
        case 0b0000011100: return EMIT_andx(tc, opcode);
        case 0b0000100000: return EMIT_cmpl(tc, opcode);
        case 0b0000101000: return EMIT_subfx(tc, opcode);
        //case 0b0000110110: return EMIT_dcbst(tc, opcode);     // VEA
        case 0b0000110111: return EMIT_lwzux(tc, opcode);
        case 0b0000111100: return EMIT_andcx(tc, opcode);
        //case 0b0001001011: return EMIT_mulhwx(tc, opcode);
        //case 0b0001010011: return EMIT_mfmsr(tc, opcode);     // OEA, supervisor
        //case 0b0001010110: return EMIT_dbcf(tc, opcode);      // VEA
        case 0b0001010111: return EMIT_lbzx(tc, opcode);
        case 0b0001101000: return EMIT_negx(tc, opcode);
        case 0b0001110111: return EMIT_lbzux(tc, opcode);
        case 0b0001111100: return EMIT_norx(tc, opcode);
        //case 0b0010001000: return EMIT_subfex(tc, opcode);
        case 0b0010001010: return EMIT_addex(tc, opcode);
        case 0b0010010000: return EMIT_mtcrf(tc, opcode);
        //case 0b0010010010: return EMIT_mtmsr(tc, opcode);     // OEA, supervisor
        //case 0b0010010110: return EMIT_stwcx_dot(tc, opcode);
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
        //case 0b0011110110: return EMIT_dcbtst(tc, opcode);    // VEA
        case 0b0011110111: return EMIT_stbux(tc, opcode);
        case 0b0100001010: return EMIT_addx(tc, opcode);
        //case 0b0100010110: return EMIT_dcbt(tc, opcode);      // VEA
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
        //case 0b0111010110: return EMIT_dcbi(tc, opcode);      // VEA, supervisor
        case 0b0111011100: return EMIT_nandx(tc, opcode);
        case 0b0111101011: return EMIT_divwx(tc, opcode);
        case 0b1000000000: return EMIT_mcrxr(tc, opcode);
        //case 0b1000010101: return EMIT_lswx(tc, opcode);
        case 0b1000010110: return EMIT_lwbrx(tc, opcode);
        //case 0b1000010111: return EMIT_lfsx(tc, opcode);      // FPU
        case 0b1000011000: return EMIT_srwx(tc, opcode);
        //case 0b1000110110: return EMIT_tlbsync(tc, opcode);   // OEA, supervisor, optional
        //case 0b1000110111: return EMIT_lfsux(tc, opcode);     // FPU
        //case 0b1001010011: return EMIT_mfsr(tc, opcode);      // OEA, supervisor
        //case 0b1001010101: return EMIT_lswi(tc, opcode);
        case 0b1001010110: return EMIT_sync(tc, opcode);
        //case 0b1001010111: return EMIT_lfdx(tc, opcode);      // FPU
        //case 0b1001110111: return EMIT_lfdux(tc, opcode);     // FPU
        //case 0b1010010011: return EMIT_mfsrin(tc, opcode);    // OEA
        //case 0b1010010101: return EMIT_stswx(tc, opcode);
        case 0b1010010110: return EMIT_stwbrx(tc, opcode);
        //case 0b1010010111: return EMIT_stfsx(tc, opcode);     // FPU
        //case 0b1010110111: return EMIT_stfsux(tc, opcode);    // FPU
        //case 0b1011010101: return EMIT_stswi(tc, opcode);     // FPU
        //case 0b1011010111: return EMIT_stfdx(tc, opcode);     // FPU
        //case 0b1011110110: return EMIT_dcba(tc, opcode);      // VEA, optional
        //case 0b1011110111: return EMIT_stfdux(tc, opcode);    // FPU
        case 0b1100010110: return EMIT_lhbrx(tc, opcode);
        case 0b1100011000: return EMIT_srawx(tc, opcode);
        case 0b1100111000: return EMIT_srawix(tc, opcode);
        case 0b1101010110: return EMIT_eieio(tc, opcode);
        case 0b1110010110: return EMIT_sthbrx(tc, opcode);
        case 0b1110011010: return EMIT_extshx(tc, opcode);
        case 0b1110111010: return EMIT_extsbx(tc, opcode);    
        //case 0b1111010110: return EMIT_icbi(tc, opcode);      // VEA
        //case 0b1111010111: return EMIT_stfiwx(tc, opcode);    // FPU
        //case 0b1111110110: return EMIT_dcbz(tc, opcode);      // VEA
        default: return -1;
    }
}

static inline int EmitINSN(struct TranslatorContext *tc)
{
    uint32_t opcode = cache_read_32(ICACHE, (uint32_t)(uintptr_t)tc->tc_PPCCodePtr);
    uint8_t group = opcode >> 26;
    int count = -1;

//    kprintf("[PPC] EmitINSN @ %08x, opcode %08x, group %d\n", (uint32_t)(uintptr_t)tc->tc_PPCCodePtr, opcode, group);

    switch (group) {
        // case 0b000011: count = EMIT_twi(tc, opcode); break;
        case 0b000111: count = EMIT_mulli(tc, opcode); break;
        case 0b001000: count = EMIT_subfic(tc, opcode); break;
        case 0b001010: count = EMIT_cmpli(tc, opcode); break;
        case 0b001011: count = EMIT_cmpi(tc, opcode); break;
        // case 0b001100: count = EMIT_addic(tc, opcode); break;
        // case 0b001101: count = EMIT_addic_dot(tc, opcode); break;
        case 0b001110: count = EMIT_addi(tc, opcode); break;
        case 0b001111: count = EMIT_addis(tc, opcode); break;
        case 0b010000: count = EMIT_bcx(tc, opcode); break;
        // case 0b010001: count = EMIT_sc(tc, opcode); break;
        case 0b010010: count = EMIT_bx(tc, opcode); break;
        case 0b010011: count = EMIT_Group_19(tc, opcode); break;
        // case 0b010100: count = EMIT_rlwimix(tc, opcode); break;
        case 0b010101: count = EMIT_rlwinmx(tc, opcode); break;
        // case 0b010111: count = EMIT_rlwnmx(tc, opcode); break;
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
        //case 0b101110: count = EMIT_lmw(tc, opcode); break;
        //case 0b101111: count = EMIT_stmw(tc, opcode); break;
        //case 0b110000: count = EMIT_lfs(tc, opcode); break;
        //case 0b110001: count = EMIT_lfsu(tc, opcode); break;
        //case 0b110010: count = EMIT_lfd(tc, opcode); break;
        //case 0b110011: count = EMIT_lfdu(tc, opcode); break;
        //case 0b110100: count = EMIT_stfs(tc, opcode); break;
        //case 0b110101: count = EMIT_stfsu(tc, opcode); break;
        //case 0b110110: count = EMIT_stfd(tc, opcode); break;
        //case 0b110111: count = EMIT_stfdu(tc, opcode); break;
        //case 0b111011: count = EMIT_Group_59(tc, opcode); break;
        //case 0b111111: count = EMIT_Group_63(tc, opcode); break;
        default: break;
    }

    if (count < 1) {
        kprintf("[PPC] UNIMPLEMENTED %08x @ %08x...", opcode, (uint32_t)(uintptr_t)tc->tc_PPCCodePtr);
        PPC_PrintContext(getHostCTX());
        while(1);
    }

    return count;
}

static __used__ void LocalExit(struct TranslatorContext *tc, uint32_t insn_fixup)
{
#if EMU68_INSN_COUNTER
    uint8_t icnt_reg = AllocARMRegister(tc);
    EMIT(tc, mov_simd_to_reg(icnt_reg, CTX_INSN_COUNT));
#endif

    //StoreDirtyFPURegs(tc);
    StoreDirtyGPRs(tc);

    FlushPC(tc);

#if EMU68_INSN_COUNTER
    EMIT(tc,
        add64_immed(icnt_reg, icnt_reg, (insn_count + insn_fixup) & 0xfff),
        mov_reg_to_simd(CTX_INSN_COUNT, icnt_reg)
    );
    FreeARMRegister(tc, icnt_reg);
#else
    (void)insn_fixup;
#endif

    EMIT(tc, bx_lr());
}

static struct DisasmOut {
    uint32_t *do_PPCAddr;
    uint32_t *do_ArmAddr;
    uint32_t do_PPCCount;
    uint32_t do_ArmCount;
} disasm_items[512], *disasm_ptr;

static inline uintptr_t PPC_Translate(uint32_t *PPCCodePtr)
{
    struct List exitList;
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

    struct TranslatorContext tc;

    tc.tc_CodePtr = temporary_arm_code;
    tc.tc_CodeStart = temporary_arm_code;
    tc.tc_PPCCodePtr = PPCCodePtr;
    tc.tc_PPCCodeStart = PPCCodePtr;

    uint32_t *last_rev_jump = (uint32_t *)0xffffffff;

    NEWLIST(&exitList);

    disasm_ptr = disasm_items;

    int debug = 0;
    int disasm = 0;

    if ((uint32_t)(uintptr_t)PPCCodePtr >= debug_range_min && (uint32_t)(uintptr_t)PPCCodePtr <= debug_range_max) {
        debug = globalDebug();
        disasm = globalDisasm();
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
        local_state[insn_count].pls_PCRel = _pc_rel;
        for (int i=0; i < 38; i++)
        {
            uint8_t map = _int_reg_mapping[i];
            if (map == 0xff) {
                struct RegisterNode *rn;
                ForeachNode(&GPR_LRU, rn)
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
                
                tc.tc_CodePtr -= 4;
                
                uint32_t insn_count = tc.tc_CodePtr[2];
                uint32_t fixup_type = tc.tc_CodePtr[1];
                uint32_t fixup_target = tc.tc_CodePtr[0];

                eb = tlsf_malloc(tlsf, sizeof(struct ExitBlock) + 4 * insn_count);

                eb->eb_Type = MARKER_EXIT_BLOCK;
                eb->eb_InstructionCount = insn_count;
                eb->eb_FixupType = fixup_type;
                eb->eb_FixupLocation = tc.tc_CodePtr - fixup_target;

                tc.tc_CodePtr -= insn_count;

                for (unsigned i=0; i < insn_count; i++) {
                    eb->eb_ARMCode[i] = tc.tc_CodePtr[i];
                }

                ADDTAIL(&exitList, eb);
            }
            else if (tc.tc_CodePtr[-1] == INSN_TO_LE(MARKER_DOUBLE_EXIT))
            {
                struct DoubleExitBlock *eb;

                tc.tc_CodePtr -= 6;

                uint32_t insn_count = tc.tc_CodePtr[4];
                uint32_t fixup2_type = tc.tc_CodePtr[3];
                uint32_t fixup2_target = tc.tc_CodePtr[2];
                uint32_t fixup1_type = tc.tc_CodePtr[1];
                uint32_t fixup1_target = tc.tc_CodePtr[0];

                eb = tlsf_malloc(tlsf, sizeof(struct DoubleExitBlock) + 4 * insn_count);

                eb->eb_Type = MARKER_DOUBLE_EXIT;
                eb->eb_InstructionCount = insn_count;
                eb->eb_Fixup1Type = fixup1_type;
                eb->eb_Fixup1Location = tc.tc_CodePtr - fixup1_target;
                eb->eb_Fixup2Type = fixup2_type;
                eb->eb_Fixup2Location = tc.tc_CodePtr - fixup2_target;

                tc.tc_CodePtr -= insn_count;

                for (unsigned i = 0; i < insn_count; i++)
                {
                    eb->eb_ARMCode[i] = tc.tc_CodePtr[i];
                }

                ADDTAIL(&exitList, eb);
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
                    LocalExit(&tc, 0);
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
                max_rev_jumps = var_EMU68_MAX_LOOP_COUNT - 1;
            }
        }

        if (!break_loop && (orig_ppccodeptr == tc.tc_PPCCodePtr))
        {
            inner_loop = TRUE;
            if (!soft_break) break;
        }
    }

    uint32_t *out_code = tc.tc_CodePtr;

#if EMU68_INSN_COUNTER
    uint8_t icnt_reg = AllocARMRegister(&tc);
    EMIT(&tc, mov_simd_to_reg(icnt_reg, CTX_INSN_COUNT));
#endif
    FlushAllGPRs(&tc);
    FlushPC(&tc);

#if EMU68_INSN_COUNTER
    EMIT(&tc, add64_immed(icnt_reg, icnt_reg, insn_count & 0xfff));
#endif

    uint8_t tmp2 = AllocARMRegister(&tc);
    if (inner_loop)
    {
        uint8_t cpuctx = GetCTX(&tc);
        EMIT(&tc, ldr_offset(cpuctx, tmp2, __builtin_offsetof(struct PPCState, INT)));
    }

#if EMU68_INSN_COUNTER
    EMIT(&tc, mov_reg_to_simd(CTX_INSN_COUNT, icnt_reg));
    FreeARMRegister(&tc, icnt_reg);
#endif

    if (inner_loop)
    {
        uint32_t *tmpptr = tc.tc_CodePtr;
        EMIT(&tc, cbz(tmp2, tc.tc_CodeStart - tmpptr));
    }
    EMIT(&tc, bx_lr());
    
    uint32_t *_tmpptr = tc.tc_CodePtr;
    FreeARMRegister(&tc, tmp2);
    FlushCTX(&tc);
    tc.tc_CodePtr = _tmpptr;

    if (disasm) {
        disasm_ptr->do_PPCAddr = NULL;
        disasm_ptr->do_PPCCount = 0;
        disasm_ptr->do_ArmAddr = out_code;
        disasm_ptr->do_ArmCount = tc.tc_CodePtr - out_code;
        disasm_ptr++;
    }

    /* Get all exit entries and append them here */
    struct ExitBlock *n = NULL;
    //int exit_num = 0;
    while ((n = (struct ExitBlock *)REMHEAD(&exitList)))
    {
        uint32_t *old_end = tc.tc_CodePtr;
        uint32_t op;

        if (n->eb_Type == MARKER_DOUBLE_EXIT)
        {
            struct DoubleExitBlock *eb2 = (struct DoubleExitBlock *)n;

            for (unsigned i = 0; i < eb2->eb_InstructionCount; i++)
            {
                EMIT(&tc, eb2->eb_ARMCode[i]);
            }

            switch (eb2->eb_Fixup1Type)
            {
                case FIXUP_BCC:
                    op = I32(*eb2->eb_Fixup1Location);
                    op &= ~(0x7ffff << 5);
                    op |= ((old_end - eb2->eb_Fixup1Location) & 0x7ffff) << 5;
                    *eb2->eb_Fixup1Location = I32(op);
                    break;

                case FIXUP_TBZ:
                    op = I32(*eb2->eb_Fixup1Location);
                    op &= ~(0x3fff << 5);
                    op |= ((old_end - eb2->eb_Fixup1Location) & 0x3fff) << 5;
                    *eb2->eb_Fixup1Location = I32(op);
                    break;

                default:
                    kprintf("[JIT] I don't know how to deal with fixup type 0x%08x\n", eb2->eb_Fixup1Type);
            }

            switch (eb2->eb_Fixup2Type)
            {
                case FIXUP_BCC:
                    op = I32(*eb2->eb_Fixup2Location);
                    op &= ~(0x7ffff << 5);
                    op |= ((old_end - eb2->eb_Fixup2Location) & 0x7ffff) << 5;
                    *eb2->eb_Fixup2Location = I32(op);
                    break;

                case FIXUP_TBZ:
                    op = I32(*eb2->eb_Fixup2Location);
                    op &= ~(0x3fff << 5);
                    op |= ((old_end - eb2->eb_Fixup2Location) & 0x3fff) << 5;
                    *eb2->eb_Fixup2Location = I32(op);
                    break;

                default:
                    kprintf("[JIT] I don't know how to deal with fixup type 0x%08x\n", eb2->eb_Fixup2Type);
            }
        }
        else
        {
            struct ExitBlock *eb = n;
            
            for (unsigned i = 0; i < eb->eb_InstructionCount; i++)
            {
                EMIT(&tc, eb->eb_ARMCode[i]);
            }

            switch (eb->eb_FixupType)
            {
                case FIXUP_BCC:
                    op = I32(*eb->eb_FixupLocation);
                    op &= ~(0x7ffff << 5);
                    op |= ((old_end - eb->eb_FixupLocation) & 0x7ffff) << 5;
                    *eb->eb_FixupLocation = I32(op);
                    break;

                case FIXUP_TBZ:
                    op = I32(*eb->eb_FixupLocation);
                    op &= ~(0x3fff << 5);
                    op |= ((old_end - eb->eb_FixupLocation) & 0x3fff) << 5;
                    *eb->eb_FixupLocation = I32(op);
                    break;

                default:
                    kprintf("[JIT] I don't know how to deal with fixup type 0x%08x\n", eb->eb_FixupType);
            }
        }

        if (disasm) {
            disasm_ptr->do_PPCAddr = NULL;
            disasm_ptr->do_PPCCount = 0;
            disasm_ptr->do_ArmAddr = old_end;
            disasm_ptr->do_ArmCount = tc.tc_CodePtr - old_end;
            disasm_ptr++;
        }

        tlsf_free(tlsf, n);
    }

    disasm_ptr->do_ArmAddr = NULL;

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
        uint32_t mean = 100 * (tc.tc_CodePtr - tc.tc_CodeStart); // - (prologue_size + epilogue_size));
        mean = mean / insn_count;
        uint32_t mean_n = mean / 100;
        uint32_t mean_f = mean % 100;
        kprintf("[PPC] Mean ARM instructions per PPC instruction: %d.%02d\n", mean_n, mean_f);
    }

    if (inner_loop && insn_count == 1) {
        struct PPCState *ctx = getHostCTX();
        kprintf("[PPC] Endless loop detected. We are done here\n");
        PPC_PrintContext(ctx);
        kprintf("[PPC] Instructions executed: %ld\n", ctx->INSN_COUNT);
        while(1);
    }

    // Put a marker at the end of translation unit
    EMIT(&tc, 0xffffffff);

    return (uintptr_t)tc.tc_CodePtr - (uintptr_t)tc.tc_CodeStart;
}

/*
    Get PPC code unit from the instruction cache. Return NULL if code was not found and needs to be
    translated first.

    If the code was found, update its position in the LRU cache.
*/
struct PPCTranslationUnit *PPC_GetTranslationUnit(uint32_t *ppccodeptr)
{
    extern uint32_t debug_range_min;
    extern uint32_t debug_range_max;

    struct PPCState *ctx = getHostCTX();
    struct PPCTranslationUnit *unit = NULL;
    uintptr_t hash = (uintptr_t)ppccodeptr;
    uint32_t *orig_ppccodeptr = ppccodeptr;

    int debug = 0;

    if ((uint32_t)(uintptr_t)ppccodeptr >= debug_range_min && (uint32_t)(uintptr_t)ppccodeptr <= debug_range_max) {
        debug = globalDebug();
    }

    /* Get 16-bit has from the pointer to PPC code */
    hash = (hash >> EMU68_HASHSHIFT) & EMU68_HASHMASK;

    if (debug > 2)
        kprintf("[PPC] GetTranslationUnit(%08x)\n[PPC] Hash: 0x%04x\n", (void*)ppccodeptr, (int)hash);

    uintptr_t line_length = PPC_Translate(ppccodeptr);
    uintptr_t arm_insn_count = line_length/4 - 1;

    uintptr_t unit_length = (line_length + 63 + sizeof(struct PPCTranslationUnit)) & ~63;

    do {
        unit = tlsf_malloc_aligned(jit_ppc, unit_length, 64);

        ctx->JIT_CACHE_FREE = tlsf_get_free_size(jit_ppc);

        if (unit == NULL)
        {
            if (debug > 0) {
                kprintf("[PPC] Requested block was %d bytes long\n", unit_length);
            }

            for (int i=0; i < 8; i++) {
                struct Node *n = REMTAIL(&LRU);

                if (n == NULL)
                    break;

                void *ptr = (char *)n - __builtin_offsetof(struct PPCTranslationUnit, ptu_LRUNode);
                REMOVE((struct Node *)ptr);
                if (debug > 0)
                {    
                    kprintf("[PPC] Run out of cache. Removing least recently used cache line node @ %p\n", ptr);
                }
                tlsf_free(jit_ppc, ptr);
                ctx->JIT_UNIT_COUNT--;
            }
            ctx->JIT_CACHE_FREE = tlsf_get_free_size(jit_ppc);
            
            __asm__ volatile("mov "CTX_LAST_PC_ASM", %w0"::"r"(0xffffffff));
        }
    } while(unit == NULL);

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
    unit->ptu_CRC32 = CalcCRC32(ppc_low, ppc_high);
    #if 0
    unit->ptu_UseCount = 0;
    unit->ptu_FetchCount = 0;
    unit->mt_PrologueSize = prologue_size;
    unit->mt_EpilogueSize = epilogue_size;
    unit->mt_Conditionals = conditionals_count;
    #endif

    ADDHEAD(&LRU, &unit->ptu_LRUNode);
    ADDHEAD(&ICache[hash], &unit->ptu_HashNode);

    ctx->JIT_UNIT_COUNT++;
    ctx->JIT_CACHE_MISS++;

    if (debug) {
        kprintf("[PPC] Block checksum: %08x, Fingerprint: %08x\n", unit->ptu_CRC32, unit->ptu_Fingerprint);
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

void __used__ PPC_LoadContext(struct PPCState *ctx)
{
    __asm__ volatile("mov "CTX_POINTER_ASM", %0\n"::"r"(ctx));

    __asm__ volatile("mov "CTX_INSN_COUNT_ASM", %0"::"r"(ctx->INSN_COUNT));
    __asm__ volatile("mov "REG_FPSCR_ASM", %w0"::"r"(ctx->FPSCR));

    __asm__ volatile("ldp w%0, w%1, %2"::"i"(_int_reg_mapping[0]),"i"(_int_reg_mapping[1]),"m"(ctx->GPR[0]));
    __asm__ volatile("ldp w%0, w%1, %2"::"i"(_int_reg_mapping[2]),"i"(_int_reg_mapping[3]),"m"(ctx->GPR[2]));
    __asm__ volatile("ldp w%0, w%1, %2"::"i"(_int_reg_mapping[4]),"i"(_int_reg_mapping[5]),"m"(ctx->GPR[4]));
    __asm__ volatile("ldp w%0, w%1, %2"::"i"(_int_reg_mapping[6]),"i"(_int_reg_mapping[7]),"m"(ctx->GPR[6]));
    __asm__ volatile("ldp w%0, w%1, %2"::"i"(_int_reg_mapping[8]),"i"(_int_reg_mapping[9]),"m"(ctx->GPR[8]));
    __asm__ volatile("ldp w%0, w%1, %2"::"i"(_int_reg_mapping[10]),"i"(_int_reg_mapping[11]),"m"(ctx->GPR[10]));
    __asm__ volatile("ldp w%0, w%1, %2"::"i"(_int_reg_mapping[12]),"i"(_int_reg_mapping[13]),"m"(ctx->GPR[12]));

    __asm__ volatile("ldr w%0, %1"::"i"(_int_reg_mapping[LRn]),"m"(ctx->LR));
    __asm__ volatile("ldr w%0, %1"::"i"(_int_reg_mapping[CTRn]),"m"(ctx->CTR));
    __asm__ volatile("ldr w%0, %1"::"i"(REG_PC),"m"(ctx->PC));

    __asm__ volatile("ld1 {v%0.4s}, [%1]"::"i"(GPR14_VN),"r"(&ctx->GPR[14]));
    __asm__ volatile("ld1 {v%0.4s}, [%1]"::"i"(GPR18_VN),"r"(&ctx->GPR[18]));
    __asm__ volatile("ld1 {v%0.4s}, [%1]"::"i"(GPR22_VN),"r"(&ctx->GPR[22]));
    __asm__ volatile("ld1 {v%0.4s}, [%1]"::"i"(GPR26_VN),"r"(&ctx->GPR[26]));
    __asm__ volatile("ld1 {v%0.4s}, [%1]"::"i"(GPR30_VN),"r"(&ctx->GPR[30]));

    /* FPU part... */
}

void __used__ PPC_SaveContext(struct PPCState *ctx)
{
    __asm__ volatile("mov x1, "CTX_INSN_COUNT_ASM"; str x1, %0"::"m"(ctx->INSN_COUNT):"x1");
    __asm__ volatile("mov w1, "REG_FPSCR_ASM"; str w1, %0"::"m"(ctx->FPSCR):"x1");

    __asm__ volatile("stp w%0, w%1, %2"::"i"(_int_reg_mapping[0]),"i"(_int_reg_mapping[1]),"m"(ctx->GPR[0]));
    __asm__ volatile("stp w%0, w%1, %2"::"i"(_int_reg_mapping[2]),"i"(_int_reg_mapping[3]),"m"(ctx->GPR[2]));
    __asm__ volatile("stp w%0, w%1, %2"::"i"(_int_reg_mapping[4]),"i"(_int_reg_mapping[5]),"m"(ctx->GPR[4]));
    __asm__ volatile("stp w%0, w%1, %2"::"i"(_int_reg_mapping[6]),"i"(_int_reg_mapping[7]),"m"(ctx->GPR[6]));
    __asm__ volatile("stp w%0, w%1, %2"::"i"(_int_reg_mapping[8]),"i"(_int_reg_mapping[9]),"m"(ctx->GPR[8]));
    __asm__ volatile("stp w%0, w%1, %2"::"i"(_int_reg_mapping[10]),"i"(_int_reg_mapping[11]),"m"(ctx->GPR[10]));
    __asm__ volatile("stp w%0, w%1, %2"::"i"(_int_reg_mapping[12]),"i"(_int_reg_mapping[13]),"m"(ctx->GPR[12]));

    __asm__ volatile("str w%0, %1"::"i"(_int_reg_mapping[LRn]),"m"(ctx->LR));
    __asm__ volatile("str w%0, %1"::"i"(_int_reg_mapping[CTRn]),"m"(ctx->CTR));
    __asm__ volatile("str w%0, %1"::"i"(REG_PC),"m"(ctx->PC));

    __asm__ volatile("st1 {v%0.4s}, [%1]"::"i"(GPR14_VN),"r"(&ctx->GPR[14]));
    __asm__ volatile("st1 {v%0.4s}, [%1]"::"i"(GPR18_VN),"r"(&ctx->GPR[18]));
    __asm__ volatile("st1 {v%0.4s}, [%1]"::"i"(GPR22_VN),"r"(&ctx->GPR[22]));
    __asm__ volatile("st1 {v%0.4s}, [%1]"::"i"(GPR26_VN),"r"(&ctx->GPR[26]));
    __asm__ volatile("st1 {v%0.4s}, [%1]"::"i"(GPR30_VN),"r"(&ctx->GPR[30]));

    /* FPU part... */
}

static void PPC_PrintContext(struct PPCState *ppc)
{
    kprintf("[PPC] PPC Context: ");

    for (int i=0; i < 32; i++) {
        if (i % 4 == 0)
            kprintf("\n[PPC] ");
        kprintf("   r%02d = 0x%08x", i, BE32(ppc->GPR[i]));
    }
    kprintf("\n[PPC]\n[PPC] ");

    kprintf("   PC = 0x%08x   LR = 0x%08x   CTR = 0x%08x   XER = 0x%08x", BE32(ppc->PC), BE32(ppc->LR), BE32(ppc->CTR), BE32(ppc->XER));
    kprintf("\n[JIT] ");
    kprintf("   CR = 0x%08x\n", BE32(ppc->CR));
}

struct Entry {
    uintptr_t ppc;
    uint32_t *arm;
};

static struct Entry    LRU_cache[EMU68_LRU_WAY_COUNT * EMU68_LRU_SET_COUNT] __attribute__((aligned(64)));
static uint32_t        LRU_alloc[EMU68_LRU_SET_COUNT];

#define ADDR_2_SET(addr) (((addr) >> 2) % EMU68_LRU_SET_COUNT)
#define BIT_MASK (((1ULL << EMU68_LRU_WAY_COUNT) - 1) << (32 - EMU68_LRU_WAY_COUNT))

static uint32_t *PPC_LRU_FindBlock(uint32_t address)
{
    const uint32_t set = ADDR_2_SET(address);
    struct Entry *e = &LRU_cache[set * EMU68_LRU_WAY_COUNT];
    uint32_t mask = 0x80000000;
    
    for (int i=0; i < EMU68_LRU_WAY_COUNT; i++, mask >>= 1)
    {
        if (likely(e[i].ppc == address))
        {
            uint32_t current = LRU_alloc[set] | mask; 
            if (current == BIT_MASK) current = mask;
            LRU_alloc[set] = current;
            
            /* Tell CPU we are going to execute the code soon, give it time to prefetch eventually */
            asm volatile ("prfm plil1keep, [%0]"::"r"(e[i].arm));

            return e[i].arm;
        }
    }

    return NULL;
}

static __used__ void PPC_LRU_InvalidateByARMAddress(uint32_t *addr)
{
    for (int i = 0; i < EMU68_LRU_SET_COUNT * EMU68_LRU_WAY_COUNT; i++)
    {
        if (LRU_cache[i].arm == addr)
        {
            LRU_cache[i].arm = (void*)0;
            LRU_cache[i].ppc = 0xffffffff;
            break;
        }
    }
}

static __used__ void PPC_LRU_InvalidateByM68kAddress(uint32_t addr)
{
    const uint32_t set = ADDR_2_SET(addr);
    struct Entry *e = &LRU_cache[set * EMU68_LRU_WAY_COUNT];

    for (int i = 0; i < EMU68_LRU_WAY_COUNT; i++)
    {
        if (e[i].ppc == addr)
        {
            e[i].arm= (void*)0;
            e[i].ppc = 0xffffffff;
            LRU_alloc[set] &= ~(0x80000000 >> i);
            break;
        }
    }
}

static void PPC_LRU_InvalidateAll()
{
    for (int i = 0; i < EMU68_LRU_SET_COUNT * EMU68_LRU_WAY_COUNT; i++)
    {
        LRU_cache[i].ppc = 0xffffffff;
        LRU_cache[i].arm = (void*)0;
    }

    for (int i = 0; i < EMU68_LRU_SET_COUNT; i++)
    {
        LRU_alloc[i] = 0;
    }
}

static void PPC_LRU_InsertBlock(struct PPCTranslationUnit *unit)
{
    const uint32_t set = ADDR_2_SET(unit->ptu_PPCAddress);
    struct Entry *e = &LRU_cache[set * EMU68_LRU_WAY_COUNT];
    int loc = __builtin_clz(~LRU_alloc[set]);
    uint32_t mask = 0x80000000 >> loc;

    // Insert new entry
    e[loc].ppc = unit->ptu_PPCAddress;
    e[loc].arm = unit->ptu_ARMEntryPoint;

    // Touch the last used
    uint32_t current = LRU_alloc[set] | mask; 
    if (current == BIT_MASK) current = mask;
    LRU_alloc[set] = current;
}

static inline uint32_t * FindUnitQuick()
{
#if EMU68_USE_LRU
    uint32_t *code = PPC_LRU_FindBlock(PC);

    if (likely(code != NULL))
        return code;
#endif
    struct PPCTranslationUnit *node;

    /* Perform search */
    uint32_t hash = (PC >> EMU68_HASHSHIFT) & EMU68_HASHMASK;
    struct List *bucket = &ICache[hash];

    /* Go through the list of translated units */
    ForeachNode(bucket, node)
    {
        /* Check if unit is found */
        if (node->ptu_PPCAddress == PC)
        {
            /* Tell CPU we are going to execute the code soon, give it time to prefetch eventually */
            asm volatile ("prfm plil1keep, [%0]"::"r"(node->ptu_ARMEntryPoint));

#if EMU68_USE_LRU
            PPC_LRU_InsertBlock(node);
#endif
            return node->ptu_ARMEntryPoint;
        }
    }

    return NULL;
}

static inline struct PPCTranslationUnit *FindUnit()
{
    struct PPCTranslationUnit *node;

    /* Perform search */
    uint32_t hash = (PC >> EMU68_HASHSHIFT) & EMU68_HASHMASK;
    struct List *bucket = &ICache[hash];

    /* Go through the list of translated units */
    ForeachNode(bucket, node)
    {
        /* Check if unit is found */
        if (node->ptu_PPCAddress == PC)
        {
#if EMU68_USE_LRU
            PPC_LRU_InsertBlock(node);
#endif
            return node;
        }
    }

    return NULL;
}

static inline uint32_t getLastPC()
{
    uint32_t lastPC;
    __asm__ volatile("mov %w0, "CTX_LAST_PC_ASM"":"=r"(lastPC));
    return lastPC;
}

static inline struct PPCState *getCTX()
{
    struct PPCState *ctx;
    __asm__ volatile("mov %0, "CTX_POINTER_ASM:"=r"(ctx));
    return ctx;
}

static inline void setLastPC(uint32_t pc)
{
    __asm__ volatile("mov "CTX_LAST_PC_ASM", %w0": :"r"(pc));
}

static void PPCMainLoop()
{
    uint32_t LastPC;
    struct PPCState *ctx = getHostCTX();

    PPC_LRU_InvalidateAll();

    PPC_LoadContext(ctx);

    /* The JIT loop is running forever */
    while(1)
    {   
        /* Load m68k context and last used PC counter into temporary register */ 
        LastPC = getLastPC();
        ctx = getHostCTX();

#if 0
        /* If (unlikely) there was interrupt pending, check if it needs to be processed */
        if (unlikely(ctx->INT32 != 0))
        {
            uint32_t SR, SRcopy;
            int level = 0;
            uint32_t vector;
            uint32_t vbr;

            /* Find out requested IPL level based on ARM state and real IPL line */
            if (ctx->INT.ARM_err)
            {
                level = 7;
                ctx->INT.ARM_err = 0;
            }
            else
            {
                if (ctx->INT.ARM)
                {
                    level = 6;
                    ctx->INT.ARM = 0;
                }
#if defined(PISTORM)
                /* On PiStorm32 IPL level is obtained by second CPU core from the GPIO directly */
                if (ctx->INT.IPL > level)
                {
                    level = ctx->INT.IPL;
                }
#else
                /* On classic pistorm we need to obtain IPL from PiStorm status register */
                if (ctx->INT.IPL)
                {
                    int ipl_level;

#if PISTORM_WRITE_BUFFER
                    while(__atomic_test_and_set(&bus_lock, __ATOMIC_ACQUIRE)) { __asm__ volatile("yield"); }
#endif

                    ipl_level = GetIPLLevel();

#if PISTORM_WRITE_BUFFER
                    __atomic_clear(&bus_lock, __ATOMIC_RELEASE);
#endif
                    /* Obtained IPL level higher than until now detected? */
                    if (ipl_level > level)
                    {
                        level = ipl_level;
                    }
                }           
#endif
            }

            /* Get SR and test the IPL mask value */
            SR = getSR();

            int IPL_mask = (SR & SR_IPL) >> SRB_IPL;

            /* Any unmasked interrupts? Proceess them */
            if (level == 7 || level > IPL_mask)
            {
                register uint64_t sp __asm__("r29");

                if (likely((SR & SR_S) == 0))
                {
                    /* If we are not yet in supervisor mode, the USP needs to be updated */
                    __asm__ volatile("mov "REG_USP_ASM", %w0": :"r"(sp));

                    /* Load eiter ISP or MSP */
                    if (unlikely((SR & SR_M) != 0))
                    {
                        __asm__ volatile("mov %w0, "REG_MSP_ASM:"=r"(sp));
                    }
                    else
                    {
                        __asm__ volatile("mov %w0, "REG_ISP_ASM:"=r"(sp));
                    }
                }
                
                SRcopy = SR;
                /* Swap C and V flags in the copy */
                if ((SRcopy & 3) != 0 && (SRcopy & 3) != 3)
                SRcopy ^= 3;
                vector = 0x60 + (level << 2);

                /* Set supervisor mode */
                SR |= SR_S;

                /* Clear Trace mode */
                SR &= ~(SR_T0 | SR_T1);

                /* Insert current level into SR */
                SR &= ~SR_IPL;
                SR |= ((level & 7) << SRB_IPL);

                /* Push exception frame */
                __asm__ volatile("strh %w1, [%0, #-8]!":"=r"(sp):"r"(SRcopy),"0"(sp));
                __asm__ volatile("str %w1, [%0, #2]": :"r"(sp),"r"(PC));
                __asm__ volatile("strh %w1, [%0, #6]": :"r"(sp),"r"(vector));

                /* Set SR */
                setSR(SR);

                /* Get VBR */
                vbr = ctx->VBR;

                /* Load PC */
                __asm__ volatile("ldr %w0, [%1, %2]":"=r"(PC):"r"(vbr),"r"(vector)); 
            }

            /* All interrupts masked or new PC loaded and stack swapped, continue with code execution */
        }
#endif

        /* The last PC is the same as currently set PC? */
        if (LastPC == PC)
        {
            /* Jump to the code now */
            ARMCode();
            continue;
        }
        else
        {
            /* Find unit in the hashtable based on the PC value */
            uint32_t *code = FindUnitQuick();

            /* Unit exists ? */
            if (code != NULL)
            {
                /* Store m68k PC of corresponding ARM code in CTX_LAST_PC */
                __asm__ volatile("mov "CTX_LAST_PC_ASM", %w0": :"r"(PC));

                /* This is the case, load entry point into x12 */
                ARMCode = (void*)code;
                
                ARMCode();

                /* Go back to beginning of the loop */
                continue;
            }

            /* If we are that far there was no JIT unit found */
            uint32_t copyPC = PC;
            PPC_SaveContext(ctx);
            /* Get the code. This never fails */
            struct PPCTranslationUnit *node = PPC_GetTranslationUnit((void*)(uintptr_t)copyPC);
#if EMU68_USE_LRU
            PPC_LRU_InsertBlock(node);
#endif
            /* Load CPU context */
            PPC_LoadContext(getHostCTX());
            __asm__ volatile("mov "CTX_LAST_PC_ASM", %w0": :"r"(PC));
            /* Prepare ARM pointer in x12 and call it */
            ARMCode = node->ptu_ARMEntryPoint;
            ARMCode();
        }
    }
}

spinlock_t PPCStart;

void InitPPC()
{
    static struct RegisterNode rn[64];

    NEWLIST(&GPR_LRU);
    NEWLIST(&FPR_LRU);
    NEWLIST(&FreePool);
    
    kprintf("[PPC] InitPPC()\n");

    jit_ppc = tlsf_init_with_memory((void*)(0xffffffe000000000 + ((KERNEL_JIT_PAGES / 2) << 21)), (KERNEL_JIT_PAGES / 2) << 21);

    kprintf("[PPC] JIT memory at %p\n", (void*)(0xffffffe000000000 + ((KERNEL_JIT_PAGES / 2) << 21)));

    for (int i=0; i < 64; i++) ADDHEAD(&FreePool, &rn[i]);

    kprintf("[PPC] Setting up LRU\n");
    NEWLIST(&LRU);

    kprintf("[PPC] Setting up ICache\n");

    temporary_arm_code = tlsf_malloc(jit_ppc, (JCCB_INSN_DEPTH_MASK + 1) * 16 * 64);
    kprintf("[PPC] Temporary code at %p\n", temporary_arm_code);
    local_state = tlsf_malloc(tlsf, sizeof(struct PPCLocalState)*(JCCB_INSN_DEPTH_MASK + 1)*2);
    kprintf("[PPC] ICache array at %p\n", ICache);

    for (int i=0; i < 65536; i++)
        NEWLIST(&ICache[i]);

    kprintf("[PPC] Mapping PPC ROM at 0x%08x - 0x%08x\n", 0xfff00000, 0xfff00000 + ppc_rom_img_len - 1);
    kprintf("[PPC] Mapping PPC boot stack at 0x%08x - 0x%08x\n", 0xfff00000 - sizeof(ppc_tmp_stack), 0xfff00000 - 1);
    mmu_map(mmu_virt2phys((uintptr_t)ppc_rom_img), 0xfff00000, ppc_rom_img_len, MMU_ACCESS | MMU_ISHARE | MMU_ALLOW_EL0 | MMU_READ_ONLY | MMU_ATTR_CACHED, 0);
    mmu_map(mmu_virt2phys((uintptr_t)ppc_tmp_stack), 0xfff00000 - sizeof(ppc_tmp_stack), sizeof(ppc_tmp_stack), MMU_ACCESS | MMU_ISHARE | MMU_ALLOW_EL0 | MMU_ATTR_CACHED, 0);
}

void StartupPPC()
{
    static struct PPCState ppc;

    /* Init spinlock as busy */
    PPCStart.lock = 1;

    /* Init PPCState */
    bzero(&ppc, sizeof(ppc));

    /* Init FPR with NAN */
    for (int fp=0; fp < 32; fp++) {
        ppc.FPR_u64[fp] = 0x7fffffffffffffffULL;
    }

    ppc.JIT_CACHE_TOTAL = tlsf_get_total_size(jit_ppc);
    ppc.JIT_CACHE_FREE = tlsf_get_free_size(jit_ppc);
    ppc.JIT_UNIT_COUNT = 0;
    ppc.JIT_SOFTFLUSH_THRESH = EMU68_WEAK_CFLUSH_LIMIT;
    ppc.JIT_CONTROL = EMU68_WEAK_CFLUSH ? JCCF_SOFT : 0;
    ppc.JIT_CONTROL |= (EMU68_M68K_INSN_DEPTH & JCCB_INSN_DEPTH_MASK) << JCCB_INSN_DEPTH;
    ppc.JIT_CONTROL |= (EMU68_BRANCH_INLINE_DISTANCE & JCCB_INLINE_RANGE_MASK) << JCCB_INLINE_RANGE;
    ppc.JIT_CONTROL |= (EMU68_MAX_LOOP_COUNT & JCCB_LOOP_COUNT_MASK) << JCCB_LOOP_COUNT;
    ppc.JIT_CONTROL2 = (EMU68_CCR_SCAN_DEPTH << JC2B_CCR_SCAN_DEPTH);
    
    /* Start at reset vector */
    ppc.PC = 0xfff00100;

    /* Put some start parameters for now, remove later */
    extern uint16_t *framebuffer;
    extern uint32_t pitch;
    extern uint32_t fb_width;
    extern uint32_t fb_height;

    /* Set PPC context pointer */
    __asm__ volatile("mov "CTX_POINTER_ASM", %0\n"::"r"(&ppc));

    kprintf("[PPC] Waiting for startup\n");

    spinlock_acquire(&PPCStart);

    kprintf("[PPC] Starting up!\n");

    ppc.GPR[3] = BE32((uint32_t)(intptr_t)framebuffer);
    ppc.GPR[4] = BE32((uint32_t)fb_width);
    ppc.GPR[5] = BE32((uint32_t)fb_height);
    ppc.GPR[6] = BE32((uint32_t)pitch);

    PPC_PrintContext(&ppc);

    PPCMainLoop();
}
