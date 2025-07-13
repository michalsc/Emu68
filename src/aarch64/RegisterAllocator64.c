/*
    Copyright © 2019 Michal Schulz <michal.schulz@gmx.de>
    https://github.com/michalsc

    This Source Code Form is subject to the terms of the
    Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed
    with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "support.h"
#include "RegisterAllocator.h"
#include "A64.h"
#include "M68k.h"

static uint16_t register_pool = 0;
static uint16_t changed_mask = 0;
static uint8_t fpu_allocstate;

void RA_ResetFPUAllocator()
{
    fpu_allocstate = 0;
}

uint8_t RA_MapFPURegister(struct TranslatorContext *ctx, uint8_t fpu_reg)
{
    (void)ctx;
    
    fpu_reg &= 7;

    return fpu_reg + 8;
}

uint8_t RA_MapFPURegisterForWrite(struct TranslatorContext *ctx, uint8_t fpu_reg)
{
    (void)ctx;

   fpu_reg &= 7;

   return fpu_reg + 8;
}

void RA_SetDirtyFPURegister(struct TranslatorContext *ctx, uint8_t fpu_reg)
{
    (void)ctx;
    (void)fpu_reg;
}

void RA_ClearChangedMask()
{
}

void RA_StoreDirtyFPURegs(struct TranslatorContext *)
{
}

void RA_FlushFPURegs(struct TranslatorContext *)
{
}

void RA_StoreDirtyM68kRegs(struct TranslatorContext *)
{
}

void RA_FlushM68kRegs(struct TranslatorContext *)
{
}

uint8_t RA_AllocFPURegister(struct TranslatorContext *)
{
    for (int i=2; i < 8; i++) {
        if ((fpu_allocstate & (1 << i)) == 0)
        {
            fpu_allocstate |= 1 << i;
            return i;
        }
    }

    return 0xff;
}

void RA_FreeFPURegister(struct TranslatorContext *, uint8_t arm_reg)
{
    if (arm_reg < 8) {
        if (fpu_allocstate & (1 << arm_reg))
            fpu_allocstate &= ~(1 << arm_reg);
    }
}

static const uint8_t _reg_map_m68k_to_arm[16] = {
    REG_D0, REG_D1, REG_D2, REG_D3, REG_D4, REG_D5, REG_D6, REG_D7,
    REG_A0, REG_A1, REG_A2, REG_A3, REG_A4, REG_A5, REG_A6, REG_A7
};

void RA_SetDirtyM68kRegister(struct TranslatorContext *, uint8_t)
{
}

/* Test if given register is a m68k register */
int RA_IsM68kRegister(uint8_t arm_reg)
{
    const uint32_t test_field = 
        (1 << REG_D0) | (1 << REG_D1) | (1 << REG_D2) | (1 << REG_D3) |
        (1 << REG_D4) | (1 << REG_D5) | (1 << REG_D6) | (1 << REG_D7) |
        (1 << REG_A0) | (1 << REG_A1) | (1 << REG_A2) | (1 << REG_A3) |
        (1 << REG_A4) | (1 << REG_A5) | (1 << REG_A6) | (1 << REG_A7);
    
    if (arm_reg >= 32) return 0;
    else return (test_field & (1 << arm_reg)) != 0;
}

/*
    Make a discardable copy of m68k register (e.g. temporary value from reg which can be later worked on)
*/
uint8_t RA_CopyFromM68kRegister(struct TranslatorContext *ctx, uint8_t m68k_reg)
{
    uint8_t arm_reg = RA_AllocARMRegister(ctx);

    EMIT(ctx, mov_reg(arm_reg, _reg_map_m68k_to_arm[m68k_reg & 15]));

    return arm_reg;
}

/*
    Map m68k register to ARM register

    On AArch64 Dn and An m68k registers are always mapped. Just return the corresponding ARM
    register number here.
*/
uint8_t RA_MapM68kRegister(struct TranslatorContext *, uint8_t m68k_reg)
{
    return _reg_map_m68k_to_arm[m68k_reg & 15];
}

/*
    Map m68k register to ARM register

    On AArch64 Dn and An m68k registers are always mapped. Just return the corresponding ARM
    register number here.
*/
uint8_t RA_MapM68kRegisterForWrite(struct TranslatorContext *, uint8_t m68k_reg)
{
    return _reg_map_m68k_to_arm[m68k_reg & 15];
}

static uint8_t reg_CC = 0xff;
static uint8_t mod_CC = 0;
static uint8_t reg_CTX = 0xff;
static uint8_t reg_FPCR = 0xff;
static uint8_t mod_FPCR = 0;
static uint8_t reg_FPSR = 0xff;
static uint8_t mod_FPSR = 0;

uint8_t RA_TryCTX(struct TranslatorContext *)
{
    return reg_CTX;
}

uint8_t RA_GetCTX(struct TranslatorContext *ctx)
{
    if (reg_CTX == 0xff)
    {
        reg_CTX = RA_AllocARMRegister(ctx);
        EMIT(ctx, mov_simd_to_reg(reg_CTX, CTX_POINTER));
    }

    return reg_CTX;
}

void RA_FlushCTX(struct TranslatorContext *ctx)
{
    if (reg_CTX != 0xff)
    {
        RA_FreeARMRegister(ctx, reg_CTX);
    }

    reg_CTX = 0xff;
}

uint8_t RA_GetFPCR(struct TranslatorContext *ctx)
{
    if (reg_FPCR == 0xff)
    {
        reg_FPCR = RA_AllocARMRegister(ctx);

        EMIT(ctx, mov_simd_to_reg(reg_FPCR, 29, TS_H, 4));

        mod_FPCR = 0;
    }

    return reg_FPCR;
}

uint8_t RA_ModifyFPCR(struct TranslatorContext *ctx)
{
    uint8_t fpcr = RA_GetFPCR(ctx);
    mod_FPCR = 1;
    return fpcr;
}

void RA_StoreFPCR(struct TranslatorContext *ctx)
{
    if (reg_FPCR != 0xff && mod_FPCR)
    {
        EMIT(ctx, mov_reg_to_simd(29, TS_H, 4, reg_FPCR));
    }
}

void RA_FlushFPCR(struct TranslatorContext *ctx)
{
    if (reg_FPCR != 0xff)
    {
        if (mod_FPCR)
        {
            EMIT(ctx, mov_reg_to_simd(29, TS_H, 4, reg_FPCR));
        }
        RA_FreeARMRegister(ctx, reg_FPCR);
    }
    reg_FPCR = 0xff;
    mod_FPCR = 0;
}

uint8_t RA_GetFPSR(struct TranslatorContext *ctx)
{
    if (reg_FPSR == 0xff)
    {
        reg_FPSR = RA_AllocARMRegister(ctx);

        EMIT(ctx, mov_simd_to_reg(reg_FPSR, 29, TS_S, 0));

        mod_FPSR = 0;
    }

    return reg_FPSR;
}

uint8_t RA_ModifyFPSR(struct TranslatorContext *ctx)
{
    uint8_t fpsr = RA_GetFPSR(ctx);
    mod_FPSR = 1;
    return fpsr;
}

void RA_StoreFPSR(struct TranslatorContext *ctx)
{
    if (reg_FPSR != 0xff && mod_FPSR)
    {
        EMIT(ctx, mov_reg_to_simd(29, TS_S, 0, reg_FPSR));
    }
}

void RA_FlushFPSR(struct TranslatorContext *ctx)
{
    if (reg_FPSR != 0xff)
    {
        if (mod_FPSR)
        {
            EMIT(ctx, mov_reg_to_simd(29, TS_S, 0, reg_FPSR));
        }
        RA_FreeARMRegister(ctx, reg_FPSR);
    }
    reg_FPSR = 0xff;
    mod_FPSR = 0;
}

/* Note! CC in ARM register has swapped C and V bits!!! */
uint8_t RA_GetCC(struct TranslatorContext *ctx)
{
    if (reg_CC == 0xff)
    {
        reg_CC = RA_AllocARMRegister(ctx);

        EMIT(ctx, mrs(reg_CC, 3, 3, 13, 0, 2));

        mod_CC = 0;
    }

    return reg_CC;
}

uint8_t RA_ModifyCC(struct TranslatorContext *ctx)
{
    uint8_t cc = RA_GetCC(ctx);
    mod_CC = 1;
    return cc;
}

void RA_StoreCC(struct TranslatorContext *ctx)
{
    if (reg_CC != 0xff && mod_CC)
    {
        EMIT(ctx, msr(reg_CC, 3, 3, 13, 0, 2));
    }
}

void RA_FlushCC(struct TranslatorContext *ctx)
{
    if (reg_CC != 0xff)
    {
        if (mod_CC)
        {
            EMIT(ctx, msr(reg_CC, 3, 3, 13, 0, 2));
        }
        RA_FreeARMRegister(ctx, reg_CC);
    }
    reg_CC = 0xff;
    mod_CC = 0;
}

int RA_IsCCLoaded()
{
    return (reg_CC != 0xff);
}

int RA_IsCCModified()
{
    return (mod_CC != 0);
}

/* Allocate register x0-x11 for JIT */
static uint8_t __int_arm_alloc_reg()
{
    static int last_allocated = 0;
    for (int i=1; i <= 12; i++)
    {
        int reg = (last_allocated + i) % 12;

        if (((register_pool | 15) & (1 << reg)) == 0)
        {
            register_pool |= 1 << reg;
            changed_mask |= 1 << reg;
            last_allocated = reg;
            return reg;
        }
    }

    return 0xff;
}

uint8_t RA_AllocARMRegister(struct TranslatorContext *ctx)
{
    uint8_t reg = __int_arm_alloc_reg();

    if (reg != 0xff)
        return reg;

    kprintf("!!!warning - flushing regs!\n");

    RA_FlushFPCR(ctx);

    reg = __int_arm_alloc_reg();

    if (reg != 0xff)
        return reg;

    RA_FlushFPSR(ctx);

    reg = __int_arm_alloc_reg();

    if (reg != 0xff)
        return reg;

    RA_FlushCC(ctx);

    reg = __int_arm_alloc_reg();

    if (reg != 0xff)
        return reg;

    kprintf("[JIT] ARM Register allocator exhausted!!!\n");

    return 0xff;
}


void RA_FreeARMRegister(struct TranslatorContext *, uint8_t arm_reg)
{
    if (arm_reg > 11)
        return;

    register_pool &= ~(1 << arm_reg);
}

uint16_t RA_GetTempAllocMask()
{
    return register_pool;
}

void EMIT_SaveRegFrame(struct TranslatorContext *ctx, uint32_t mask)
{
    uint8_t cnt = __builtin_popcount(mask);

    if (cnt != 0)
    {
        // Reserve place on stack
        if (cnt & 1)
            EMIT(ctx, sub64_immed(31, 31, 8*(cnt + 1)));
        else
            EMIT(ctx, sub64_immed(31, 31, 8*cnt));
        uint32_t m = 1;
        uint8_t r = 0;
        uint8_t off = 0;

        while(cnt > 1) {
            uint8_t r1;
            uint8_t r2;

            while(0 == (mask & m)) { m <<= 1; r++; }
            r1 = r++;
            m <<= 1;
            while(0 == (mask & m)) { m <<= 1; r++; }
            r2 = r++;
            m <<= 1;

            EMIT(ctx, stp64(31, r1, r2, 16 * off++));

            cnt -= 2;
        }

        if (cnt) {
            while(0 == (mask & m)) { m <<= 1; r++; }
            EMIT(ctx, str64_offset(31, r, 16 * off));
        }
    }
}

void EMIT_RestoreRegFrame(struct TranslatorContext *ctx, uint32_t mask)
{
    uint8_t cnt = __builtin_popcount(mask);
    uint8_t cnt_orig = cnt;

    if (cnt != 0)
    {
        uint32_t m = 1;
        uint8_t r = 0;
        uint8_t off = 0;

        while(cnt > 1) {
            uint8_t r1;
            uint8_t r2;

            while(0 == (mask & m)) { m <<= 1; r++; }
            r1 = r++;
            m <<= 1;
            while(0 == (mask & m)) { m <<= 1; r++; }
            r2 = r++;
            m <<= 1;

            EMIT(ctx, ldp64(31, r1, r2, 16 * off++));

            cnt -= 2;
        }

        if (cnt) {
            while(0 == (mask & m)) { m <<= 1; r++; }
            EMIT(ctx, ldr64_offset(31, r, 16 * off));
        }

        // Reclaim place on stack
        if (cnt_orig & 1)
            EMIT(ctx, add64_immed(31, 31, 8*(cnt_orig + 1)));
        else
            EMIT(ctx, add64_immed(31, 31, 8*cnt_orig));
    }
}
