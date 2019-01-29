#include <stdint.h>
#include <stdio.h>

#include "RegisterAllocator.h"

static uint8_t PLRU_Tree;

static struct {
    uint8_t rs_m68kReg;
    uint8_t rs_ARMReg;
    uint8_t rs_Dirty;
} PLRU_RegisterSlots[8];

/*
    Assign new slot for PLRU entry. Used to maintain recently used m68k register cache.
    Returns position in PLRU_RegisterSlots where the m68k register is moved.
*/
uint8_t RA_SelectNewSlot()
{
    int i=1;

    if (PLRU_Tree & (1 << i)) {
        PLRU_Tree ^= (1 << i);
        i = i * 2 + 1;
    } else {
        PLRU_Tree ^= (1 << i);
        i = i * 2;
    }

    if (PLRU_Tree & (1 << i)) {
        PLRU_Tree ^= (1 << i);
        i = i * 2 + 1;
    } else {
        PLRU_Tree ^= (1 << i);
        i = i * 2;
    }

    if (PLRU_Tree & (1 << i)) {
        PLRU_Tree ^= (1 << i);
        i = i * 2 + 1;
    } else {
        PLRU_Tree ^= (1 << i);
        i = i * 2;
    }

    return i - 8;
}

void RA_UpdateSlot(uint8_t slot)
{
    if (slot > 7)
        return;

    slot += 8;

    if (slot & 1) {
        slot >>= 1;
        PLRU_Tree &= ~(1 << slot);
    } else {
        slot >>= 1;
        PLRU_Tree |= 1 << slot;
    }
    
    if (slot & 1) {
        slot >>= 1;
        PLRU_Tree &= ~(1 << slot);
    } else {
        slot >>= 1;
        PLRU_Tree |= 1 << slot;
    }

    if (slot & 1) {
        slot >>= 1;
        PLRU_Tree &= ~(1 << slot);
    } else {
        slot >>= 1;
        PLRU_Tree |= 1 << slot;
    }
}

/*
    Map m68k register to ARM register
*/
uint8_t RA_MapM68kRegister(uint8_t m68k_reg)
{
    /* 
        Check if register is already mapped, if yes, update slot in order to delay
        reassignment.
    */
    for (int i=0; i < 8; i++)
    {
        if (PLRU_RegisterSlots[i].rs_m68kReg == m68k_reg)
        {
            RA_UpdateSlot(i);
            return PLRU_RegisterSlots[i].rs_ARMReg;
        }
    }

    /*
        Register not found in the cache. Get new place for it and discard old contents. If register
        was dirty, flush it back in ARM instruction stream.
    */
    uint8_t pos = RA_SelectNewSlot();
    if (PLRU_RegisterSlots[pos].rs_Dirty)
    {

    }
}