/*
    Copyright © 2019 Michal Schulz <michal.schulz@gmx.de>
    https://github.com/michalsc

    This Source Code Form is subject to the terms of the
    Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed
    with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "RegisterAllocator.h"
#include "ARM.h"
#include "M68k.h"

static struct {
    uint8_t rs_ARMReg;
    uint8_t rs_Dirty;
} LRU_M68kRegisters[16] = {
    {0xff, 0},
    {0xff, 0},
    {0xff, 0},
    {0xff, 0},
    {0xff, 0},
    {0xff, 0},
    {0xff, 0},
    {0xff, 0},
    {0xff, 0},
    {0xff, 0},
    {0xff, 0},
    {0xff, 0},
    {0xff, 0},
    {0xff, 0},
    {0xff, 0},
    {0xff, 0},
};
static int8_t LRU_Table[8] = { -1, -1, -1, -1, -1, -1, -1, -1 };
static uint16_t register_pool = 0;
static uint16_t changed_mask = 0;

static uint8_t FPU_AllocState;
static uint8_t FPU_Reg_State[8] = {0, 0, 0, 0, 0, 0, 0, 0};
#define FPU_LOADED  0x01
#define FPU_DIRTY   0x02

uint16_t RA_GetChangedMask()
{
    return changed_mask;
}

void RA_ClearChangedMask()
{
    changed_mask = 0;
}

void RA_ResetFPUAllocator()
{
    FPU_AllocState = 0;
}

uint8_t RA_AllocFPURegister(uint32_t **arm_stream)
{
    (void)arm_stream;

    for (int i=1; i < 8; i++) {
        if ((FPU_AllocState & (1 << i)) == 0)
        {
            FPU_AllocState |= 1 << i;
            return i;
        }
    }

    return 0xff;
}

void RA_FreeFPURegister(uint32_t **arm_stream, uint8_t arm_reg)
{
    (void)arm_stream;

    if (arm_reg < 8) {
        if (FPU_AllocState & (1 << arm_reg))
            FPU_AllocState &= ~(1 << arm_reg);
    }
}

uint8_t RA_MapFPURegister(uint32_t **arm_stream, uint8_t fpu_reg)
{
    fpu_reg &= 7;

    if (FPU_Reg_State[fpu_reg] & FPU_LOADED)
        return fpu_reg + 8;

    /* FPU registers are 1:1 allocated to the vfp double registers d8-d15 */
    FPU_Reg_State[fpu_reg] = FPU_LOADED;

    /* Emit load of register from m68k context to the vfp register */
    **arm_stream = fldd(fpu_reg + 8, REG_CTX, __builtin_offsetof(struct M68KState, FP[fpu_reg]) / 4);
    (*arm_stream)++;

    return fpu_reg + 8;
}

uint8_t RA_MapFPURegisterForWrite(uint32_t **arm_stream, uint8_t fpu_reg)
{
    (void)arm_stream;
    /*
        Map for write means, we do not load the contents at all. Instead, just mark
        the register as loaded and dirty
    */
   fpu_reg &= 7;
   FPU_Reg_State[fpu_reg] = FPU_DIRTY | FPU_LOADED;

   return fpu_reg + 8;
}

void RA_SetDirtyFPURegister(uint32_t **arm_stream, uint8_t fpu_reg)
{
    fpu_reg &= 7;

    /*
        If register was previously unmapped, map it first. This should never happen though
    */
    RA_MapFPURegister(arm_stream, fpu_reg);
    FPU_Reg_State[fpu_reg] |= FPU_DIRTY;
}

void RA_FlushFPURegs(uint32_t **arm_stream)
{
    for (int i=0; i < 8; i++)
    {
        if (FPU_Reg_State[i] & FPU_DIRTY)
        {
            **arm_stream = fstd(i + 8, REG_CTX, __builtin_offsetof(struct M68KState, FP[i]) / 4);
            (*arm_stream)++;
        }
        FPU_Reg_State[i] = 0;
    }
}

void RA_StoreDirtyFPURegs(uint32_t **arm_stream)
{
    for (int i=0; i < 8; i++)
    {
        if (FPU_Reg_State[i] & FPU_DIRTY)
        {
            **arm_stream = fstd(i + 8, REG_CTX, __builtin_offsetof(struct M68KState, FP[i]) / 4);
            (*arm_stream)++;
        }
    }
}

/* Touch given register in order to move it to the front */
void RA_TouchM68kRegister(uint32_t **arm_stream, uint8_t index)
{
    (void)arm_stream;
    if (LRU_Table[0] == index)
        return;

    for (int i = 1; i < 8; i++)
    {
        if (LRU_Table[i] == index)
        {
            for (int j = i; j > 0; --j)
            {
                LRU_Table[j] = LRU_Table[j - 1];
            }

            LRU_Table[0] = index;

            return;
        }
    }
}

void RA_SetDirtyM68kRegister(uint32_t **arm_stream, uint8_t m68k_reg)
{
    (void)arm_stream;
    LRU_M68kRegisters[m68k_reg & 15].rs_Dirty = 1;
}

void RA_DiscardM68kRegister(uint32_t **arm_stream, uint8_t m68k_reg)
{
    if (LRU_M68kRegisters[m68k_reg].rs_Dirty)
    {
        if (m68k_reg < 8) {
 //           kprintf("emit: str r%d, [r%d, %d]\n", LRU_M68kRegisters[m68k_reg].rs_ARMReg, REG_CTX,
 //                                     (int)__builtin_offsetof(struct M68KState, D[m68k_reg]));
            **arm_stream = str_offset(REG_CTX, LRU_M68kRegisters[m68k_reg].rs_ARMReg,
                                __builtin_offsetof(struct M68KState, D[m68k_reg]));
        }
        else {
 //           kprintf("emit: str r%d, [r%d, %d]\n", LRU_M68kRegisters[m68k_reg].rs_ARMReg, REG_CTX,
 //                                     (int)__builtin_offsetof(struct M68KState, A[m68k_reg-8]));
            **arm_stream = str_offset(REG_CTX, LRU_M68kRegisters[m68k_reg].rs_ARMReg,
                                      __builtin_offsetof(struct M68KState, A[m68k_reg-8]));
        }
        (*arm_stream)++;
    }
    uint8_t arm_reg = LRU_M68kRegisters[m68k_reg].rs_ARMReg;
    LRU_M68kRegisters[m68k_reg].rs_Dirty = 0;
    LRU_M68kRegisters[m68k_reg].rs_ARMReg = 0xff;
    RA_FreeARMRegister(arm_stream, arm_reg);
}

void RA_StoreDirtyM68kRegister(uint32_t **arm_stream, uint8_t m68k_reg)
{
    if (LRU_M68kRegisters[m68k_reg].rs_Dirty)
    {
        if (m68k_reg < 8)
        {
            //           kprintf("emit: str r%d, [r%d, %d]\n", LRU_M68kRegisters[m68k_reg].rs_ARMReg, REG_CTX,
            //                                     (int)__builtin_offsetof(struct M68KState, D[m68k_reg]));
            **arm_stream = str_offset(REG_CTX, LRU_M68kRegisters[m68k_reg].rs_ARMReg,
                                      __builtin_offsetof(struct M68KState, D[m68k_reg]));
        }
        else
        {
            //           kprintf("emit: str r%d, [r%d, %d]\n", LRU_M68kRegisters[m68k_reg].rs_ARMReg, REG_CTX,
            //                                     (int)__builtin_offsetof(struct M68KState, A[m68k_reg-8]));
            **arm_stream = str_offset(REG_CTX, LRU_M68kRegisters[m68k_reg].rs_ARMReg,
                                      __builtin_offsetof(struct M68KState, A[m68k_reg - 8]));
        }
        (*arm_stream)++;
    }
}

void RA_StoreDirtyM68kRegs(uint32_t **arm_stream)
{
    for (int i = 0; i < 16; i++)
        RA_StoreDirtyM68kRegister(arm_stream, i);
}

void RA_FlushM68kRegs(uint32_t **arm_stream)
{
    for (int i=0; i < 16; i++)
        RA_DiscardM68kRegister(arm_stream, i);

    for (int i=0; i < 8; i++)
        LRU_Table[i] = -1;
}

/* Insert new register into LRU table */
void RA_InsertM68kRegister(uint32_t **arm_stream, uint8_t m68k_reg)
{
    (void)arm_stream;
    if (LRU_Table[7] != -1)
        RA_DiscardM68kRegister(arm_stream, LRU_Table[7]);

    for (int i=7; i > 0; --i)
        LRU_Table[i] = LRU_Table[i-1];

    LRU_Table[0] = m68k_reg;
}

/* Remove given register from LRU table */
void RA_RemoveM68kRegister(uint32_t **arm_stream, uint8_t m68k_reg)
{
    (void)arm_stream;
    int found = 0;
    int j = 0;
    for (int i=0; i < 8; i++)
    {
        uint8_t ind = LRU_Table[i];
        if (ind == m68k_reg) {
            found = 1;
            continue;
        }
        LRU_Table[j++] = ind;
    }
    while (j < 8)
        LRU_Table[j++] = -1;

    if (found)
        RA_DiscardM68kRegister(arm_stream, m68k_reg);
}

/*
    Make a discardable copy of m68k register (e.g. temporary value from reg which can be later worked on)
*/
uint8_t RA_CopyFromM68kRegister(uint32_t **arm_stream, uint8_t m68k_reg)
{
    uint8_t arm_reg = RA_AllocARMRegister(arm_stream);

    /* If the register is already mapped, move it's value to temporary without touching mapped reg */
    if (LRU_M68kRegisters[m68k_reg].rs_ARMReg != 0xff) {
        **arm_stream = mov_reg(arm_reg, LRU_M68kRegisters[m68k_reg].rs_ARMReg);
    }
    else
    {
        /* The register was not mapped. Fetch it from m68k state instead */
        if (m68k_reg < 8) {
            **arm_stream = ldr_offset(REG_CTX, arm_reg,
                                  __builtin_offsetof(struct M68KState, D[m68k_reg]));
        } else {
            **arm_stream = ldr_offset(REG_CTX, arm_reg,
                                  __builtin_offsetof(struct M68KState, A[m68k_reg - 8]));
        }
    }

    (*arm_stream)++;

    return arm_reg;
}

/*
    Map m68k register to ARM register
*/
uint8_t RA_MapM68kRegister(uint32_t **arm_stream, uint8_t m68k_reg)
{
    /*
        Check if register is already mapped, if yes, update slot in order to delay
        reassignment.
    */
    if (LRU_M68kRegisters[m68k_reg].rs_ARMReg != 0xff)
    {
        RA_TouchM68kRegister(arm_stream, m68k_reg);
        return LRU_M68kRegisters[m68k_reg].rs_ARMReg;
    }

    /*
        Register not found in the cache. Alloc new ARM register for it, fetch it from
        context and put in front of the register cache
    */
    uint8_t arm_reg = RA_AllocARMRegister(arm_stream);
    LRU_M68kRegisters[m68k_reg].rs_ARMReg = arm_reg;

//    fprintf(stderr, "# MapRegister %X <-> r%d\n", m68k_reg < 8 ? 0xd0 + m68k_reg : 0x98 + m68k_reg, arm_reg);

    if (m68k_reg < 8) {
        //fprintf(stderr, "# emit: ldr r%d, [r%d, %d]\n", arm_reg, REG_CTX,
        //       (int)__builtin_offsetof(struct M68KState, D[m68k_reg]));
        **arm_stream = ldr_offset(REG_CTX, arm_reg,
                                  __builtin_offsetof(struct M68KState, D[m68k_reg]));
    } else {
        //fprintf(stderr, "# emit: ldr r%d, [r%d, %d]\n", arm_reg, REG_CTX,
        //       (int)__builtin_offsetof(struct M68KState, A[m68k_reg - 8]));
        **arm_stream = ldr_offset(REG_CTX, arm_reg,
                                  __builtin_offsetof(struct M68KState, A[m68k_reg - 8]));
    }
    //fprintf(stderr, "    0x%08x\n", **arm_stream);
    (*arm_stream)++;

    RA_InsertM68kRegister(arm_stream, m68k_reg);

    return arm_reg;
}

/*
    Map m68k register to ARM register
*/
uint8_t RA_MapM68kRegisterForWrite(uint32_t **arm_stream, uint8_t m68k_reg)
{
    /*
        Check if register is already mapped, if yes, update slot in order to delay
        reassignment.
    */
    if (LRU_M68kRegisters[m68k_reg].rs_ARMReg != 0xff)
    {
        RA_SetDirtyM68kRegister(arm_stream, m68k_reg);
        RA_TouchM68kRegister(arm_stream, m68k_reg);
        return LRU_M68kRegisters[m68k_reg].rs_ARMReg;
    }

    /*
        Register not found in the cache. Alloc new ARM register for it, fetch it from
        context and put in front of the register cache
    */
    uint8_t arm_reg = RA_AllocARMRegister(arm_stream);
    LRU_M68kRegisters[m68k_reg].rs_ARMReg = arm_reg;

    RA_SetDirtyM68kRegister(arm_stream, m68k_reg);
    RA_InsertM68kRegister(arm_stream, m68k_reg);

    return arm_reg;
}

uint8_t RA_IsARMRegisterMapped(uint8_t arm_reg)
{
    arm_reg &= 0xf;
    for (int i=0; i < 16; i++)
        if (LRU_M68kRegisters[i].rs_ARMReg == arm_reg)
            return 1;
    return 0;
}

uint8_t RA_GetMappedARMRegister(uint8_t m68k_reg)
{
    m68k_reg &= 0xf;
    return (LRU_M68kRegisters[m68k_reg].rs_ARMReg) | (LRU_M68kRegisters[m68k_reg].rs_Dirty ? 0x80 : 0);
}

void RA_AssignM68kRegister(uint32_t **arm_stream, uint8_t m68k_reg, uint8_t arm_reg)
{
    arm_reg &= 0x0f;
    uint8_t old_reg = LRU_M68kRegisters[m68k_reg].rs_ARMReg;

    LRU_M68kRegisters[m68k_reg].rs_ARMReg = arm_reg;
    LRU_M68kRegisters[m68k_reg].rs_Dirty = 1;

    RA_FreeARMRegister(arm_stream, old_reg);

    for (int i=0; i < 8; i++) {
        if (LRU_Table[i] == m68k_reg) {
            RA_TouchM68kRegister(arm_stream, m68k_reg);
            return;
        }
    }

    RA_InsertM68kRegister(arm_stream, m68k_reg);
}

/* Allocate register R0-R9 for JIT */
static uint8_t __int_arm_alloc_reg()
{
    for (int i = 0; i < 10; i++)
    {
        if ((register_pool & (1 << i)) == 0)
        {
            register_pool |= 1 << i;
            changed_mask |= 1 << i;
            return i;
        }
    }

    return 0xff;
}

uint16_t RA_GetTempAllocMask()
{
    uint16_t map = 0;

    for (int i=0; i < 10; i++)
    {
        if (register_pool & (1 << i))
        {
            int found = 0;
            for (int j=0; j < 16; j++)
            {
                if (LRU_M68kRegisters[j].rs_ARMReg == i)
                {
                    found = 1;
                    break;
                }
            }

            if (!found)
                map |= 1 << i;
        }
    }
    return map;
}

uint8_t RA_AllocARMRegister(uint32_t **arm_stream)
{
    uint8_t reg = __int_arm_alloc_reg();

    if (reg != 0xff)
        return reg;

    for (int i=7; i >= 0; --i)
    {
        if (LRU_Table[i] != -1)
        {
            RA_RemoveM68kRegister(arm_stream, LRU_Table[i]);
            break;
        }
    }

    reg = __int_arm_alloc_reg();

    if (reg != 0xff)
        return reg;

    RA_FlushFPCR(arm_stream);
    RA_FlushFPSR(arm_stream);

    reg = __int_arm_alloc_reg();

    if (reg != 0xff)
        return reg;

    kprintf("[JIT] ARM Register allocator exhausted!!!\n");

    return 0xff;
}

void RA_FreeARMRegister(uint32_t **arm_stream, uint8_t arm_reg)
{
    if (arm_reg > 15)
        return;

    (void)arm_stream;
    for (int i=0; i < 16; i++)
    {
        if (LRU_M68kRegisters[i].rs_ARMReg == arm_reg)
            return;
    }

    register_pool &= ~(1 << arm_reg);
}

static uint8_t got_CC = 0;
static uint8_t mod_CC = 0;

uint8_t M68K_GetCC(uint32_t **ptr) __attribute__((alias("RA_GetCC")));
uint8_t M68K_ModifyCC(uint32_t **ptr) __attribute__((alias("RA_ModifyCC")));
void M68K_FlushCC(uint32_t **ptr) __attribute__((alias("RA_FlushCC")));
void M68K_StoreCC(uint32_t **ptr) __attribute__((alias("RA_StoreCC")));

int RA_IsCCLoaded()
{
    return (got_CC != 0);
}

int RA_IsCCModified()
{
    return (mod_CC != 0);
}

uint8_t RA_GetCC(uint32_t **ptr)
{
    if (got_CC == 0)
    {
        **ptr = ldrh_offset(REG_CTX, REG_SR, __builtin_offsetof(struct M68KState, SR));
        (*ptr)++;
        got_CC = 1;
        mod_CC = 0;
    }

    return REG_SR;
}

uint8_t RA_ModifyCC(uint32_t **ptr)
{
    uint8_t reg = RA_GetCC(ptr);
    mod_CC = 1;

    return reg;
}

void RA_StoreCC(uint32_t **ptr)
{
    if (got_CC && mod_CC)
    {
        **ptr = strh_offset(REG_CTX, REG_SR, __builtin_offsetof(struct M68KState, SR));
        (*ptr)++;
    }
}

void RA_FlushCC(uint32_t **ptr)
{
    if (got_CC && mod_CC)
    {
        **ptr = strh_offset(REG_CTX, REG_SR, __builtin_offsetof(struct M68KState, SR));
        (*ptr)++;
    }
    got_CC = 0;
}

static uint8_t reg_FPCR = 0xff;
static uint8_t mod_FPCR = 0;
static uint8_t reg_FPSR = 0xff;
static uint8_t mod_FPSR = 0;

uint8_t RA_GetFPCR(uint32_t **ptr)
{
    if (reg_FPCR == 0xff)
    {
        reg_FPCR = RA_AllocARMRegister(ptr);
        **ptr = ldrh_offset(REG_CTX, reg_FPCR, __builtin_offsetof(struct M68KState, FPCR));
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
        **ptr = strh_offset(REG_CTX, reg_FPCR, __builtin_offsetof(struct M68KState, FPCR));
        (*ptr)++;
    }
}

void RA_FlushFPCR(uint32_t **ptr)
{
    if (reg_FPCR != 0xff && mod_FPCR)
    {
        **ptr = strh_offset(REG_CTX, reg_FPCR, __builtin_offsetof(struct M68KState, FPCR));
        (*ptr)++;
        RA_FreeARMRegister(ptr, reg_FPCR);
    }
    reg_FPCR = 0xff;
    mod_FPCR = 0;
}

uint8_t RA_GetFPSR(uint32_t **ptr)
{
    if (reg_FPSR == 0xff)
    {
        reg_FPSR = RA_AllocARMRegister(ptr);
        **ptr = ldr_offset(REG_CTX, reg_FPSR, __builtin_offsetof(struct M68KState, FPSR));
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
        **ptr = str_offset(REG_CTX, reg_FPSR, __builtin_offsetof(struct M68KState, FPSR));
        (*ptr)++;
    }
}

void RA_FlushFPSR(uint32_t **ptr)
{
    if (reg_FPSR != 0xff && mod_FPSR)
    {
        **ptr = str_offset(REG_CTX, reg_FPSR, __builtin_offsetof(struct M68KState, FPSR));
        (*ptr)++;
        RA_FreeARMRegister(ptr, reg_FPSR);
    }
    reg_FPSR = 0xff;
    mod_FPSR = 0;
}
