#include <stdint.h>
#include <stdio.h>

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
            printf("emit: str r%d, [r%d, %d]\n", LRU_M68kRegisters[m68k_reg].rs_ARMReg, REG_CTX, 
                                      (int)__builtin_offsetof(struct M68KState, D[m68k_reg]));
            **arm_stream = str_offset(REG_CTX, LRU_M68kRegisters[m68k_reg].rs_ARMReg, 
                                __builtin_offsetof(struct M68KState, D[m68k_reg]));
        }
        else {
            printf("emit: str r%d, [r%d, %d]\n", LRU_M68kRegisters[m68k_reg].rs_ARMReg, REG_CTX, 
                                      (int)__builtin_offsetof(struct M68KState, A[m68k_reg-8]));
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

    if (m68k_reg < 8) {
        printf("emit: ldr r%d, [r%d, %d]\n", arm_reg, REG_CTX,
               (int)__builtin_offsetof(struct M68KState, D[m68k_reg]));
        **arm_stream = ldr_offset(REG_CTX, arm_reg,
                                  __builtin_offsetof(struct M68KState, D[m68k_reg]));
    } else {
        printf("emit: ldr r%d, [r%d, %d]\n", arm_reg, REG_CTX,
               (int)__builtin_offsetof(struct M68KState, A[m68k_reg - 8]));
        **arm_stream = ldr_offset(REG_CTX, arm_reg,
                                  __builtin_offsetof(struct M68KState, A[m68k_reg - 8]));
    }
    (*arm_stream)++;

    RA_InsertM68kRegister(arm_stream, m68k_reg);

    return arm_reg;
}


/* Allocate register R0-R9 for JIT */
static uint8_t __int_arm_alloc_reg()
{
    for (int i = 0; i < 10; i++)
    {
        if ((register_pool & (1 << i)) == 0)
        {
            register_pool |= 1 << i;
            return i;
        }
    }
    return 0xff;
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
    
    return __int_arm_alloc_reg();
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
