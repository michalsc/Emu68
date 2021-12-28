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

uint8_t RA_MapFPURegister(uint32_t **arm_stream, uint8_t fpu_reg)
{
    (void)arm_stream;
    
    fpu_reg &= 7;

    return fpu_reg + 8;
}

uint8_t RA_MapFPURegisterForWrite(uint32_t **arm_stream, uint8_t fpu_reg)
{
    (void)arm_stream;

   fpu_reg &= 7;

   return fpu_reg + 8;
}

void RA_SetDirtyFPURegister(uint32_t **arm_stream, uint8_t fpu_reg)
{
    (void)arm_stream;
    (void)fpu_reg;
}

void RA_ClearChangedMask(uint32_t **arm_stream)
{
    (void)arm_stream;
}

void RA_StoreDirtyFPURegs(uint32_t **arm_stream)
{
    (void)arm_stream;
}

void RA_FlushFPURegs(uint32_t **arm_stream)
{
    (void)arm_stream;
}

void RA_StoreDirtyM68kRegs(uint32_t **arm_stream)
{
    (void)arm_stream;
}

void RA_FlushM68kRegs(uint32_t **arm_stream)
{
    (void)arm_stream;
}

uint8_t RA_AllocFPURegister(uint32_t **arm_stream)
{
    (void)arm_stream;

    for (int i=2; i < 8; i++) {
        if ((fpu_allocstate & (1 << i)) == 0)
        {
            fpu_allocstate |= 1 << i;
            return i;
        }
    }

    return 0xff;
}

void RA_FreeFPURegister(uint32_t **arm_stream, uint8_t arm_reg)
{
    (void)arm_stream;

    if (arm_reg < 8) {
        if (fpu_allocstate & (1 << arm_reg))
            fpu_allocstate &= ~(1 << arm_reg);
    }
}

static const uint8_t _reg_map_m68k_to_arm[16] = {
    REG_D0, REG_D1, REG_D2, REG_D3, REG_D4, REG_D5, REG_D6, REG_D7,
    REG_A0, REG_A1, REG_A2, REG_A3, REG_A4, REG_A5, REG_A6, REG_A7
};

void RA_SetDirtyM68kRegister(uint32_t **arm_stream, uint8_t m68k_reg)
{
    (void)arm_stream;
    (void)m68k_reg;
}

/*
    Make a discardable copy of m68k register (e.g. temporary value from reg which can be later worked on)
*/
uint8_t RA_CopyFromM68kRegister(uint32_t **arm_stream, uint8_t m68k_reg)
{
    uint8_t arm_reg = RA_AllocARMRegister(arm_stream);

    **arm_stream = mov_reg(arm_reg, _reg_map_m68k_to_arm[m68k_reg & 15]);

    (*arm_stream)++;

    return arm_reg;
}

/*
    Map m68k register to ARM register

    On AArch64 Dn and An m68k registers are always mapped. Just return the corresponding ARM
    register number here.
*/
uint8_t RA_MapM68kRegister(uint32_t **arm_stream, uint8_t m68k_reg)
{
    (void)arm_stream;
    return _reg_map_m68k_to_arm[m68k_reg & 15];
}

/*
    Map m68k register to ARM register

    On AArch64 Dn and An m68k registers are always mapped. Just return the corresponding ARM
    register number here.
*/
uint8_t RA_MapM68kRegisterForWrite(uint32_t **arm_stream, uint8_t m68k_reg)
{
    (void)arm_stream;
    return _reg_map_m68k_to_arm[m68k_reg & 15];
}


static uint8_t reg_CC = 0xff;
static uint8_t mod_CC = 0;
static uint8_t reg_CTX = 0xff;
static uint8_t reg_FPCR = 0xff;
static uint8_t mod_FPCR = 0;
static uint8_t reg_FPSR = 0xff;
static uint8_t mod_FPSR = 0;

uint8_t RA_TryCTX(uint32_t **ptr)
{
    (void)ptr;
    return reg_CTX;
}

uint8_t RA_GetCTX(uint32_t **ptr)
{
    if (reg_CTX == 0xff)
    {
        reg_CTX = RA_AllocARMRegister(ptr);
        **ptr = mrs(reg_CTX, 3, 3, 13, 0, 3);
        (*ptr)++;
    }

    return reg_CTX;
}

void RA_FlushCTX(uint32_t **ptr)
{
    if (reg_CTX != 0xff)
    {
        RA_FreeARMRegister(ptr, reg_CTX);
    }

    reg_CTX = 0xff;
}

uint8_t RA_GetFPCR(uint32_t **ptr)
{
    if (reg_FPCR == 0xff)
    {
        reg_FPCR = RA_AllocARMRegister(ptr);
        uint8_t reg_CTX = RA_GetCTX(ptr);
        **ptr = ldrh_offset(reg_CTX, reg_FPCR, __builtin_offsetof(struct M68KState, FPCR));
        (*ptr)++;
        mod_FPCR = 0;
    }

    return reg_FPCR;
}

uint8_t RA_ModifyFPCR(uint32_t **ptr)
{
    uint8_t fpcr = RA_GetFPCR(ptr);
    mod_FPCR = 1;
    return fpcr;
}

void RA_StoreFPCR(uint32_t **ptr)
{
    if (reg_FPCR != 0xff && mod_FPCR)
    {
        uint8_t reg_CTX = RA_GetCTX(ptr);
        **ptr = strh_offset(reg_CTX, reg_FPCR, __builtin_offsetof(struct M68KState, FPCR));
        (*ptr)++;
    }
}

void RA_FlushFPCR(uint32_t **ptr)
{
    if (reg_FPCR != 0xff)
    {
        if (mod_FPCR)
        {
            uint8_t reg_CTX = RA_GetCTX(ptr);
            **ptr = strh_offset(reg_CTX, reg_FPCR, __builtin_offsetof(struct M68KState, FPCR));
            (*ptr)++;
        }
        RA_FreeARMRegister(ptr, reg_FPCR);
    }
    reg_FPCR = 0xff;
    mod_FPCR = 0;
}

uint8_t RA_GetFPSR(uint32_t **ptr)
{
    if (reg_FPSR == 0xff)
    {
        uint8_t reg_CTX = RA_GetCTX(ptr);
        reg_FPSR = RA_AllocARMRegister(ptr);
        **ptr = ldr_offset(reg_CTX, reg_FPSR, __builtin_offsetof(struct M68KState, FPSR));
        (*ptr)++;
        mod_FPSR = 0;
    }

    return reg_FPSR;
}

uint8_t RA_ModifyFPSR(uint32_t **ptr)
{
    uint8_t fpsr = RA_GetFPSR(ptr);
    mod_FPSR = 1;
    return fpsr;
}

void RA_StoreFPSR(uint32_t **ptr)
{
    if (reg_FPSR != 0xff && mod_FPSR)
    {
        uint8_t reg_CTX = RA_GetCTX(ptr);
        **ptr = str_offset(reg_CTX, reg_FPSR, __builtin_offsetof(struct M68KState, FPSR));
        (*ptr)++;
    }
}

void RA_FlushFPSR(uint32_t **ptr)
{
    if (reg_FPSR != 0xff)
    {
        if (mod_FPSR)
        {
            uint8_t reg_CTX = RA_GetCTX(ptr);
            **ptr = str_offset(reg_CTX, reg_FPSR, __builtin_offsetof(struct M68KState, FPSR));
            (*ptr)++;
        }
        RA_FreeARMRegister(ptr, reg_FPSR);
    }
    reg_FPSR = 0xff;
    mod_FPSR = 0;
}

uint8_t RA_GetCC(uint32_t **ptr)
{
    if (reg_CC == 0xff)
    {
        reg_CC = RA_AllocARMRegister(ptr);
//        uint8_t reg_CTX = RA_GetCTX(ptr);
//        **ptr = ldrh_offset(reg_CTX, reg_CC, __builtin_offsetof(struct M68KState, SR));
        **ptr = mrs(reg_CC, 3, 3, 13, 0, 2);
        (*ptr)++;
        mod_CC = 0;
    }

    return reg_CC;
}

uint8_t RA_ModifyCC(uint32_t **ptr)
{
    uint8_t cc = RA_GetCC(ptr);
    mod_CC = 1;
    return cc;
}

void RA_StoreCC(uint32_t **ptr)
{
    if (reg_CC != 0xff && mod_CC)
    {
//        uint8_t reg_CTX = RA_GetCTX(ptr);
//        **ptr = strh_offset(reg_CTX, reg_CC, __builtin_offsetof(struct M68KState, SR));
        **ptr = msr(reg_CC, 3, 3, 13, 0, 2);
        (*ptr)++;
    }
}

void RA_FlushCC(uint32_t **ptr)
{
    if (reg_CC != 0xff)
    {
        if (mod_CC)
        {
//        uint8_t reg_CTX = RA_GetCTX(ptr);
//        **ptr = strh_offset(reg_CTX, reg_CC, __builtin_offsetof(struct M68KState, SR));
            **ptr = msr(reg_CC, 3, 3, 13, 0, 2);
            (*ptr)++;
        }
        RA_FreeARMRegister(ptr, reg_CC);
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
    int reg = __builtin_ctz(~register_pool);

    if (reg < 12) {
        register_pool |= 1 << reg;
        changed_mask |= 1 << reg;
        return reg;
    }

    return 0xff;
}

uint8_t RA_AllocARMRegister(uint32_t **arm_stream)
{
    uint8_t reg = __int_arm_alloc_reg();

    if (reg != 0xff)
        return reg;

    RA_FlushFPCR(arm_stream);

    reg = __int_arm_alloc_reg();

    if (reg != 0xff)
        return reg;

    RA_FlushFPSR(arm_stream);

    reg = __int_arm_alloc_reg();

    if (reg != 0xff)
        return reg;

    RA_FlushCC(arm_stream);

    reg = __int_arm_alloc_reg();

    if (reg != 0xff)
        return reg;

    kprintf("[JIT] ARM Register allocator exhausted!!!\n");

    return 0xff;
}


void RA_FreeARMRegister(uint32_t **arm_stream, uint8_t arm_reg)
{
    if (arm_reg > 11)
        return;

    (void)arm_stream;

    register_pool &= ~(1 << arm_reg);
}

uint16_t RA_GetTempAllocMask()
{
    return register_pool;
}

uint32_t *EMIT_SaveRegFrame(uint32_t *ptr, uint32_t mask)
{
    uint8_t cnt = __builtin_popcount(mask);

    if (cnt != 0)
    {
        // Reserve place on stack
        if (cnt & 1)
            *ptr++ = sub64_immed(31, 31, 8*(cnt + 1));
        else
            *ptr++ = sub64_immed(31, 31, 8*cnt);
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

            *ptr++ = stp64(31, r1, r2, 16 * off++);

            cnt -= 2;
        }

        if (cnt) {
            while(0 == (mask & m)) { m <<= 1; r++; }
            *ptr++ = str64_offset(31, r, 16 * off);
        }
    }

    return ptr;
}

uint32_t *EMIT_RestoreRegFrame(uint32_t *ptr, uint32_t mask)
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

            *ptr++ = ldp64(31, r1, r2, 16 * off++);

            cnt -= 2;
        }

        if (cnt) {
            while(0 == (mask & m)) { m <<= 1; r++; }
            *ptr++ = ldr64_offset(31, r, 16 * off);
        }

        // Reclaim place on stack
        if (cnt_orig & 1)
            *ptr++ = add64_immed(31, 31, 8*(cnt_orig + 1));
        else
            *ptr++ = add64_immed(31, 31, 8*cnt_orig);
    }
    
    return ptr;
}
