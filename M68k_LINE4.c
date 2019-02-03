#include <stdint.h>
#include <stdlib.h>

#include <stdio.h>
#include "ARM.h"
#include "M68k.h"
#include "RegisterAllocator.h"

uint32_t *EMIT_MUL_DIV(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr);

uint32_t *EMIT_CLR(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
{
    uint8_t ext_count = 0;
    uint8_t dest;
    uint8_t size = 0;

    /* Determine the size of operation */
    switch (opcode & 0x00c0)
    {
        case 0x0000: /* Byte operation */
            size = 1;
            break;
        case 0x0040: /* Short operation */
            size = 2;
            break;
        case 0x0080: /* Long operation */
            size = 4;
            break;
    }

    /* handle clearing D register here */
    if ((opcode & 0x0038) == 0)
    {
        /* If size = 4 then just clear the register witout fetching it */
        if (size == 4)
        {
            dest = RA_MapM68kRegisterForWrite(&ptr, opcode & 7);
            *ptr++ = mov_immed_u8(dest, 0);
        }
        else
        {
            /* Fetch m68k register for write */
            dest = RA_MapM68kRegister(&ptr, opcode & 7);

            /* Mark register dirty */
            RA_SetDirtyM68kRegister(&ptr, opcode & 7);

            switch(size)
            {
                case 2:
                    *ptr++ = lsr_immed(dest, dest, 16);
                    *ptr++ = lsl_immed(dest, dest, 16);
                    break;
                case 1:
                    *ptr++ = lsr_immed(dest, dest, 8);
                    *ptr++ = lsl_immed(dest, dest, 8);
                    break;
            }
        }
    }
    else
    {
        /* Load effective address */
        ptr = EMIT_LoadFromEffectiveAddress(ptr, 0, &dest, opcode & 0x3f, *m68k_ptr, &ext_count);
        uint8_t tmp = RA_AllocARMRegister(&ptr);
        uint8_t mode = (opcode & 0x0038) >> 3;
        *ptr++ = mov_immed_u8(tmp, 0);

        /* Fetch data into temporary register, perform add, store it back */
        switch (size)
        {
        case 4:
            if (mode == 4)
            {
                *ptr++ = sub_immed(dest, dest, 4);
                RA_SetDirtyM68kRegister(&ptr, 8 + (opcode & 7));
            }
            if (mode == 3)
            {
                *ptr++ = str_offset_postindex(dest, tmp, 4);
                RA_SetDirtyM68kRegister(&ptr, 8 + (opcode & 7));
            }
            else
                *ptr++ = str_offset(dest, tmp, 0);
            break;
        case 2:
            if (mode == 4)
            {
                *ptr++ = sub_immed(dest, dest, 2);
                RA_SetDirtyM68kRegister(&ptr, 8 + (opcode & 7));
            }
            if (mode == 3)
            {
                *ptr++ = strh_offset_postindex(dest, tmp, 2);
                RA_SetDirtyM68kRegister(&ptr, 8 + (opcode & 7));
            }
            else
                *ptr++ = strh_offset(dest, tmp, 0);
            break;
        case 1:
            if (mode == 4)
            {
                *ptr++ = sub_immed(dest, dest, (opcode & 7) == 7 ? 2 : 1);
                RA_SetDirtyM68kRegister(&ptr, 8 + (opcode & 7));
            }
            if (mode == 3)
            {
                *ptr++ = strb_offset_postindex(dest, tmp, (opcode & 7) == 7 ? 2 : 1);
                RA_SetDirtyM68kRegister(&ptr, 8 + (opcode & 7));
            }
            else
                *ptr++ = strb_offset(dest, tmp, 0);
            break;
        }

        RA_FreeARMRegister(&ptr, tmp);
    }

    RA_FreeARMRegister(&ptr, dest);

    *ptr++ = add_immed(REG_PC, REG_PC, 2 * (ext_count + 1));
    (*m68k_ptr) += ext_count;

    uint8_t mask = M68K_GetSRMask(BE16((*m68k_ptr)[0]));
    uint8_t update_mask = (SR_X | SR_C | SR_V | SR_Z | SR_N) & ~mask;

    if (update_mask)
    {
        *ptr++ = bic_immed(REG_SR, REG_SR, update_mask);
        if (update_mask & SR_Z)
            *ptr++ = orr_immed(REG_SR, REG_SR, SR_Z);
    }
    return ptr;
}

uint32_t *EMIT_NOT(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
{
    uint8_t ext_count = 0;
    uint8_t dest;
    uint8_t size = 0;

    /* Determine the size of operation */
    switch (opcode & 0x00c0)
    {
        case 0x0000: /* Byte operation */
            size = 1;
            break;
        case 0x0040: /* Short operation */
            size = 2;
            break;
        case 0x0080: /* Long operation */
            size = 4;
            break;
    }

    /* handle clearing D register here */
    if ((opcode & 0x0038) == 0)
    {
        if (size == 4)
        {
            dest = RA_MapM68kRegister(&ptr, opcode & 7);
            RA_SetDirtyM68kRegister(&ptr, opcode & 7);
            *ptr++ = mvns_reg(dest, dest, 0);
        }
        else
        {
            uint8_t tmp = RA_AllocARMRegister(&ptr);

            /* Fetch m68k register for write */
            dest = RA_MapM68kRegister(&ptr, opcode & 7);

            /* Mark register dirty */
            RA_SetDirtyM68kRegister(&ptr, opcode & 7);

            switch(size)
            {
                case 2:
                    *ptr++ = sxth(tmp, dest, 0);        /* Extract lower 16 bits */
                    *ptr++ = mvns_reg(tmp, tmp, 0);     /* Negate */
                    *ptr++ = lsr_immed(dest, dest, 16); /* Clear destination register */
                    *ptr++ = lsl_immed(dest, dest, 16);
                    *ptr++ = uxtah(dest, dest, tmp, 0);
                    break;
                case 1:
                    *ptr++ = sxtb(tmp, dest, 0);
                    *ptr++ = mvns_reg(tmp, tmp, 0);
                    *ptr++ = lsr_immed(dest, dest, 8);
                    *ptr++ = lsl_immed(dest, dest, 8);
                    *ptr++ = uxtab(dest, dest, tmp, 0);
                    break;
            }

            RA_FreeARMRegister(&ptr, tmp);
        }
    }
    else
    {
        /* Load effective address */
        ptr = EMIT_LoadFromEffectiveAddress(ptr, 0, &dest, opcode & 0x3f, *m68k_ptr, &ext_count);
        uint8_t tmp = RA_AllocARMRegister(&ptr);
        uint8_t mode = (opcode & 0x0038) >> 3;

        /* Fetch data into temporary register, perform add, store it back */
        switch (size)
        {
        case 4:
            if (mode == 4)
            {
                *ptr++ = ldr_offset_preindex(dest, tmp, -4);
                RA_SetDirtyM68kRegister(&ptr, 8 + (opcode & 7));
            }
            else
                *ptr++ = ldr_offset(dest, tmp, 0);

            *ptr++ = mvns_reg(tmp, tmp, 0);

            if (mode == 3)
            {
                *ptr++ = str_offset_postindex(dest, tmp, 4);
                RA_SetDirtyM68kRegister(&ptr, 8 + (opcode & 7));
            }
            else
                *ptr++ = str_offset(dest, tmp, 0);
            break;
        case 2:
            if (mode == 4)
            {
                *ptr++ = ldrh_offset_preindex(dest, tmp, -2);
                RA_SetDirtyM68kRegister(&ptr, 8 + (opcode & 7));
            }
            else
                *ptr++ = ldrh_offset(dest, tmp, 0);

            *ptr++ = sxth(tmp, tmp, 0);
            *ptr++ = mvns_reg(tmp, tmp, 0);

            if (mode == 3)
            {
                *ptr++ = strh_offset_postindex(dest, tmp, 2);
                RA_SetDirtyM68kRegister(&ptr, 8 + (opcode & 7));
            }
            else
                *ptr++ = strh_offset(dest, tmp, 0);
            break;
        case 1:
            if (mode == 4)
            {
                *ptr++ = ldrb_offset_preindex(dest, tmp, (opcode & 7) == 7 ? -2 : -1);
                RA_SetDirtyM68kRegister(&ptr, 8 + (opcode & 7));
            }
            else
                *ptr++ = ldrb_offset(dest, tmp, 0);

            *ptr++ = sxtb(tmp, tmp, 0);
            *ptr++ = mvns_reg(tmp, tmp, 0);

            if (mode == 3)
            {
                *ptr++ = strb_offset_postindex(dest, tmp, (opcode & 7) == 7 ? 2 : 1);
                RA_SetDirtyM68kRegister(&ptr, 8 + (opcode & 7));
            }
            else
                *ptr++ = strb_offset(dest, tmp, 0);
            break;
        }

        RA_FreeARMRegister(&ptr, tmp);
    }

    RA_FreeARMRegister(&ptr, dest);

    *ptr++ = add_immed(REG_PC, REG_PC, 2 * (ext_count + 1));
    (*m68k_ptr) += ext_count;

    uint8_t mask = M68K_GetSRMask(BE16((*m68k_ptr)[0]));
    uint8_t update_mask = (SR_C | SR_V | SR_Z | SR_N) & ~mask;

    if (update_mask)
    {
        *ptr++ = bic_immed(REG_SR, REG_SR, update_mask);
        if (update_mask & SR_Z)
            *ptr++ = orr_cc_immed(ARM_CC_EQ, REG_SR, REG_SR, SR_Z);
        if (update_mask & SR_N)
            *ptr++ = orr_cc_immed(ARM_CC_MI, REG_SR, REG_SR, SR_N);
    }

    return ptr;
}

uint32_t *EMIT_NEG(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
{
    uint8_t ext_count = 0;
    uint8_t dest;
    uint8_t size = 0;

    /* Determine the size of operation */
    switch (opcode & 0x00c0)
    {
        case 0x0000: /* Byte operation */
            size = 1;
            break;
        case 0x0040: /* Short operation */
            size = 2;
            break;
        case 0x0080: /* Long operation */
            size = 4;
            break;
    }

    /* handle clearing D register here */
    if ((opcode & 0x0038) == 0)
    {
        if (size == 4)
        {
            dest = RA_MapM68kRegister(&ptr, opcode & 7);
            RA_SetDirtyM68kRegister(&ptr, opcode & 7);
            *ptr++ = rsbs_immed(dest, dest, 0);
        }
        else
        {
            uint8_t tmp = RA_AllocARMRegister(&ptr);

            /* Fetch m68k register for write */
            dest = RA_MapM68kRegister(&ptr, opcode & 7);

            /* Mark register dirty */
            RA_SetDirtyM68kRegister(&ptr, opcode & 7);

            switch(size)
            {
                case 2:
                    *ptr++ = rsbs_immed(tmp, dest, 0);
                    *ptr++ = lsr_immed(dest, dest, 16);
                    *ptr++ = lsl_immed(dest, dest, 16);
                    *ptr++ = uxtah(dest, dest, tmp, 2);
                    break;
                case 1:
                    *ptr++ = rsbs_immed(tmp, dest, 0);
                    *ptr++ = lsr_immed(dest, dest, 8);
                    *ptr++ = lsl_immed(dest, dest, 8);
                    *ptr++ = uxtab(dest, dest, tmp, 3);
                    break;
            }

            RA_FreeARMRegister(&ptr, tmp);
        }
    }
    else
    {
        /* Load effective address */
        ptr = EMIT_LoadFromEffectiveAddress(ptr, 0, &dest, opcode & 0x3f, *m68k_ptr, &ext_count);
        uint8_t tmp = RA_AllocARMRegister(&ptr);
        uint8_t mode = (opcode & 0x0038) >> 3;

        /* Fetch data into temporary register, perform add, store it back */
        switch (size)
        {
        case 4:
            if (mode == 4)
            {
                *ptr++ = ldr_offset_preindex(dest, tmp, -4);
                RA_SetDirtyM68kRegister(&ptr, 8 + (opcode & 7));
            }
            else
                *ptr++ = ldr_offset(dest, tmp, 0);

            *ptr++ = rsbs_immed(tmp, tmp, 0);

            if (mode == 3)
            {
                *ptr++ = str_offset_postindex(dest, tmp, 4);
                RA_SetDirtyM68kRegister(&ptr, 8 + (opcode & 7));
            }
            else
                *ptr++ = str_offset(dest, tmp, 0);
            break;
        case 2:
            if (mode == 4)
            {
                *ptr++ = ldrh_offset_preindex(dest, tmp, -2);
                RA_SetDirtyM68kRegister(&ptr, 8 + (opcode & 7));
            }
            else
                *ptr++ = ldrh_offset(dest, tmp, 0);

            *ptr++ = sxth(tmp, tmp, 0);
            *ptr++ = rsbs_immed(tmp, tmp, 0);

            if (mode == 3)
            {
                *ptr++ = strh_offset_postindex(dest, tmp, 2);
                RA_SetDirtyM68kRegister(&ptr, 8 + (opcode & 7));
            }
            else
                *ptr++ = strh_offset(dest, tmp, 0);
            break;
        case 1:
            if (mode == 4)
            {
                *ptr++ = ldrb_offset_preindex(dest, tmp, (opcode & 7) == 7 ? -2 : -1);
                RA_SetDirtyM68kRegister(&ptr, 8 + (opcode & 7));
            }
            else
                *ptr++ = ldrb_offset(dest, tmp, 0);

            *ptr++ = sxtb(tmp, tmp, 0);
            *ptr++ = rsbs_immed(tmp, tmp, 0);

            if (mode == 3)
            {
                *ptr++ = strb_offset_postindex(dest, tmp, (opcode & 7) == 7 ? 2 : 1);
                RA_SetDirtyM68kRegister(&ptr, 8 + (opcode & 7));
            }
            else
                *ptr++ = strb_offset(dest, tmp, 0);
            break;
        }

        RA_FreeARMRegister(&ptr, tmp);
    }

    RA_FreeARMRegister(&ptr, dest);

    *ptr++ = add_immed(REG_PC, REG_PC, 2 * (ext_count + 1));
    (*m68k_ptr) += ext_count;

    uint8_t mask = M68K_GetSRMask(BE16((*m68k_ptr)[0]));
    uint8_t update_mask = (SR_X | SR_C | SR_V | SR_Z | SR_N) & ~mask;

    if (update_mask)
    {
        *ptr++ = bic_immed(REG_SR, REG_SR, update_mask);
        if (update_mask & SR_Z)
            *ptr++ = orr_cc_immed(ARM_CC_EQ, REG_SR, REG_SR, SR_Z);
        if (update_mask & SR_N)
            *ptr++ = orr_cc_immed(ARM_CC_MI, REG_SR, REG_SR, SR_N);
        if (update_mask & SR_V)
            *ptr++ = orr_cc_immed(ARM_CC_VS, REG_SR, REG_SR, SR_V);
        if (update_mask & (SR_C | SR_X))
            *ptr++ = orr_cc_immed(ARM_CC_NE, REG_SR, REG_SR, SR_C | SR_X);
    }

    return ptr;
}

uint32_t *EMIT_NEGX(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
{
    uint8_t ext_count = 0;
    uint8_t dest;
    uint8_t size = 0;

    /* Determine the size of operation */
    switch (opcode & 0x00c0)
    {
        case 0x0000: /* Byte operation */
            size = 1;
            break;
        case 0x0040: /* Short operation */
            size = 2;
            break;
        case 0x0080: /* Long operation */
            size = 4;
            break;
    }

    /* handle clearing D register here */
    if ((opcode & 0x0038) == 0)
    {
        if (size == 4)
        {
            dest = RA_MapM68kRegister(&ptr, opcode & 7);
            RA_SetDirtyM68kRegister(&ptr, opcode & 7);
            *ptr++ = tst_immed(REG_SR, SR_X);                   /* If X was set perform 0 - (dest + 1), otherwise 0 - dest */
            *ptr++ = add_cc_immed(ARM_CC_NE, dest, dest, 1);
            *ptr++ = rsbs_immed(dest, dest, 0);
        }
        else
        {
            uint8_t tmp = RA_AllocARMRegister(&ptr);

            *ptr++ = tst_immed(REG_SR, SR_X);
            *ptr++ = mov_cc_immed_u8(ARM_CC_EQ, tmp, 0);
            *ptr++ = mvn_cc_immed_u8(ARM_CC_NE, tmp, 0);

            /* Fetch m68k register for write */
            dest = RA_MapM68kRegister(&ptr, opcode & 7);

            /* Mark register dirty */
            RA_SetDirtyM68kRegister(&ptr, opcode & 7);

            switch(size)
            {
                case 2:
                    *ptr++ = rsbs_reg(tmp, dest, tmp, 0);
                    *ptr++ = lsr_immed(dest, dest, 16);
                    *ptr++ = lsl_immed(dest, dest, 16);
                    *ptr++ = uxtah(dest, dest, tmp, 2);
                    break;
                case 1:
                    *ptr++ = rsbs_reg(tmp, dest, tmp, 0);
                    *ptr++ = lsr_immed(dest, dest, 8);
                    *ptr++ = lsl_immed(dest, dest, 8);
                    *ptr++ = uxtab(dest, dest, tmp, 3);
                    break;
            }

            RA_FreeARMRegister(&ptr, tmp);
        }
    }
    else
    {
        /* Load effective address */
        ptr = EMIT_LoadFromEffectiveAddress(ptr, 0, &dest, opcode & 0x3f, *m68k_ptr, &ext_count);
        uint8_t tmp = RA_AllocARMRegister(&ptr);
        uint8_t mode = (opcode & 0x0038) >> 3;
        uint8_t tmp_zero = RA_AllocARMRegister(&ptr);

        *ptr++ = tst_immed(REG_SR, SR_X);
        *ptr++ = mov_cc_immed_u8(ARM_CC_EQ, tmp_zero, 0);
        *ptr++ = mvn_cc_immed_u8(ARM_CC_NE, tmp_zero, 0);

        /* Fetch data into temporary register, perform add, store it back */
        switch (size)
        {
        case 4:
            if (mode == 4)
            {
                *ptr++ = ldr_offset_preindex(dest, tmp, -4);
                RA_SetDirtyM68kRegister(&ptr, 8 + (opcode & 7));
            }
            else
                *ptr++ = ldr_offset(dest, tmp, 0);

            *ptr++ = rsbs_reg(tmp, tmp, tmp_zero, 0);

            if (mode == 3)
            {
                *ptr++ = str_offset_postindex(dest, tmp, 4);
                RA_SetDirtyM68kRegister(&ptr, 8 + (opcode & 7));
            }
            else
                *ptr++ = str_offset(dest, tmp, 0);
            break;
        case 2:
            if (mode == 4)
            {
                *ptr++ = ldrh_offset_preindex(dest, tmp, -2);
                RA_SetDirtyM68kRegister(&ptr, 8 + (opcode & 7));
            }
            else
                *ptr++ = ldrh_offset(dest, tmp, 0);

            *ptr++ = sxth(tmp, tmp, 0);
            *ptr++ = rsbs_reg(tmp, tmp, tmp_zero, 0);

            if (mode == 3)
            {
                *ptr++ = strh_offset_postindex(dest, tmp, 2);
                RA_SetDirtyM68kRegister(&ptr, 8 + (opcode & 7));
            }
            else
                *ptr++ = strh_offset(dest, tmp, 0);
            break;
        case 1:
            if (mode == 4)
            {
                *ptr++ = ldrb_offset_preindex(dest, tmp, (opcode & 7) == 7 ? -2 : -1);
                RA_SetDirtyM68kRegister(&ptr, 8 + (opcode & 7));
            }
            else
                *ptr++ = ldrb_offset(dest, tmp, 0);

            *ptr++ = sxtb(tmp, tmp, 0);
            *ptr++ = rsbs_reg(tmp, tmp, tmp_zero, 0);

            if (mode == 3)
            {
                *ptr++ = strb_offset_postindex(dest, tmp, (opcode & 7) == 7 ? 2 : 1);
                RA_SetDirtyM68kRegister(&ptr, 8 + (opcode & 7));
            }
            else
                *ptr++ = strb_offset(dest, tmp, 0);
            break;
        }

        RA_FreeARMRegister(&ptr, tmp);
        RA_FreeARMRegister(&ptr, tmp_zero);
    }

    RA_FreeARMRegister(&ptr, dest);

    *ptr++ = add_immed(REG_PC, REG_PC, 2 * (ext_count + 1));
    (*m68k_ptr) += ext_count;

    uint8_t mask = M68K_GetSRMask(BE16((*m68k_ptr)[0]));
    uint8_t update_mask = (SR_X | SR_C | SR_V | SR_Z | SR_N) & ~mask;

    if (update_mask)
    {
        *ptr++ = bic_immed(REG_SR, REG_SR, update_mask & ~SR_Z);
        if (update_mask & SR_Z)
            *ptr++ = bic_cc_immed(ARM_CC_NE, REG_SR, REG_SR, SR_Z);
        if (update_mask & SR_N)
            *ptr++ = orr_cc_immed(ARM_CC_MI, REG_SR, REG_SR, SR_N);
        if (update_mask & SR_V)
            *ptr++ = orr_cc_immed(ARM_CC_VS, REG_SR, REG_SR, SR_V);
        if (update_mask & (SR_C | SR_X))
            *ptr++ = orr_cc_immed(ARM_CC_CS, REG_SR, REG_SR, SR_C | SR_X);
    }

    return ptr;
}

uint32_t *EMIT_TST(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
{
    uint8_t ext_count = 0;
    uint8_t immed = RA_AllocARMRegister(&ptr);
    uint8_t dest;
    uint8_t size = 0;

    *ptr++ = mov_immed_u8(immed, 0);

    /* Load immediate into the register */
    switch (opcode & 0x00c0)
    {
        case 0x0000:    /* Byte operation */
            size = 1;
            break;
        case 0x0040:    /* Short operation */
            size = 2;
            break;
        case 0x0080:    /* Long operation */
            size = 4;
            break;
    }

    /* handle adding to register here */
    if ((opcode & 0x0038) == 0)
    {
        /* Fetch m68k register */
        dest = RA_MapM68kRegister(&ptr, opcode & 7);

        /* Perform add operation */
        switch (size)
        {
            case 4:
                *ptr++ = rsbs_reg(immed, immed, dest, 0);
                break;
            case 2:
                *ptr++ = rsbs_reg(immed, immed, dest, 16);
                break;
            case 1:
                *ptr++ = rsbs_reg(immed, immed, dest, 24);
                break;
        }
    }
    else
    {
        /* Load effective address */
        ptr = EMIT_LoadFromEffectiveAddress(ptr, size, &dest, opcode & 0x3f, *m68k_ptr, &ext_count);

        /* Fetch data into temporary register, perform add, store it back */
        switch (size)
        {
        case 4:
            /* Perform calcualtion */
            *ptr++ = rsbs_reg(immed, immed, dest, 0);
            break;
        case 2:
            /* Perform calcualtion */
            *ptr++ = rsbs_reg(immed, immed, dest, 16);
            break;
        case 1:
            /* Perform calcualtion */
            *ptr++ = rsbs_reg(immed, immed, dest, 24);
            break;
        }
    }

    RA_FreeARMRegister(&ptr, immed);
    RA_FreeARMRegister(&ptr, dest);

    *ptr++ = add_immed(REG_PC, REG_PC, 2 * (ext_count + 1));
    (*m68k_ptr) += ext_count;

    uint8_t mask = M68K_GetSRMask(BE16((*m68k_ptr)[0]));
    uint8_t update_mask = (SR_C | SR_V | SR_Z | SR_N) & ~mask;

    if (update_mask)
    {
        *ptr++ = bic_immed(REG_SR, REG_SR, update_mask);
        if (update_mask & SR_N)
            *ptr++ = orr_cc_immed(ARM_CC_MI, REG_SR, REG_SR, SR_N);
        if (update_mask & SR_Z)
            *ptr++ = orr_cc_immed(ARM_CC_EQ, REG_SR, REG_SR, SR_Z);
    }
    return ptr;
}


uint32_t *EMIT_line4(uint32_t *ptr, uint16_t **m68k_ptr)
{
    uint16_t opcode = BE16((*m68k_ptr)[0]);
    (*m68k_ptr)++;

    /* 0100000011xxxxxx - MOVE from SR */
    if ((opcode & 0xffc0) == 0x40c0)
    {
        printf("[LINE4] Not implemented MOVE from SR\n");
    }
    /* 0100001011xxxxxx - MOVE from CCR */
    else if ((opcode &0xffc0) == 0x42c0)
    {
        printf("[LINE4] Not implemented MOVE from CCR\n");
    }
    /* 01000000ssxxxxxx - NEGX */
    else if ((opcode & 0xff00) == 0x4000 && (opcode & 0xc0) != 0xc0)
    {
        ptr = EMIT_NEGX(ptr, opcode, m68k_ptr);
    }
    /* 01000010ssxxxxxx - CLR */
    else if ((opcode & 0xff00) == 0x4200 && (opcode & 0xc0) != 0xc0)
    {
        ptr = EMIT_CLR(ptr, opcode, m68k_ptr);
    }
    /* 0100010011xxxxxx - MOVE to CCR */
    else if ((opcode &0xffc0) == 0x44c0)
    {
        printf("[LINE4] Not implemented MOVE to CCR\n");
    }
    /* 01000100ssxxxxxx - NEG */
    else if ((opcode &0xff00) == 0x4400 && (opcode & 0xc0) != 0xc0)
    {
        ptr = EMIT_NEG(ptr, opcode, m68k_ptr);
    }
    /* 0100011011xxxxxx - MOVE to SR */
    else if ((opcode &0xffc0) == 0x46c0)
    {
        printf("[LINE4] Not implemented MOVE to CCR\n");
    }
    /* 01000110ssxxxxxx - NOT */
    else if ((opcode &0xff00) == 0x4600 && (opcode & 0xc0) != 0xc0)
    {
        ptr = EMIT_NOT(ptr, opcode, m68k_ptr);
    }
    /* 0100100xxx000xxx - EXT, EXTB */
    else if ((opcode & 0xfeb8) == 0x4880)
    {
        uint8_t reg = RA_MapM68kRegister(&ptr, opcode & 7);
        uint8_t tmp = reg;
        RA_SetDirtyM68kRegister(&ptr, opcode & 7);
        uint8_t mode = (opcode >> 6) & 7;

        switch (mode)
        {
            case 2: /* Byte to Word */
                tmp = RA_AllocARMRegister(&ptr);
                *ptr++ = sxtb(tmp, reg, 0);
                *ptr++ = lsr_immed(reg, reg, 16);
                *ptr++ = lsl_immed(reg, reg, 16);
                *ptr++ = uxtah(reg, reg, tmp, 0);
                break;
            case 3: /* Word to Long */
                *ptr++ = sxth(reg, reg, 0);
                break;
            case 7: /* Byte to Long */
                *ptr++ = sxtb(reg, reg, 0);
                break;
        }

        *ptr++ = add_immed(REG_PC, REG_PC, 2);

        uint8_t mask = M68K_GetSRMask(BE16((*m68k_ptr)[0]));
        uint8_t update_mask = (SR_C | SR_V | SR_Z | SR_N) & ~mask;

        if (update_mask)
        {
            *ptr++ = cmp_immed(tmp, 0);
            *ptr++ = bic_immed(REG_SR, REG_SR, update_mask);
            if (update_mask & SR_N)
                *ptr++ = orr_cc_immed(ARM_CC_MI, REG_SR, REG_SR, SR_N);
            if (update_mask & SR_Z)
                *ptr++ = orr_cc_immed(ARM_CC_EQ, REG_SR, REG_SR, SR_Z);
        }
    }
    /* 0100100000001xxx - LINK - 32 bit offset */
    else if ((opcode & 0xfff8) == 0x4808)
    {
        uint8_t sp;
        uint8_t displ;
        uint8_t reg;

        displ = RA_AllocARMRegister(&ptr);
        *ptr++ = ldr_offset(REG_PC, displ, 2);
        sp = RA_MapM68kRegister(&ptr, 15);
        reg = RA_MapM68kRegister(&ptr, 8 + (opcode & 7));
        *ptr++ = str_offset_preindex(sp, reg, -4);  /* SP = SP - 4; An -> (SP) */
        *ptr++ = mov_reg(reg, sp);
        RA_SetDirtyM68kRegister(&ptr, 8 + (opcode & 7));
        *ptr++ = add_reg(sp, sp, displ, 0);
        RA_SetDirtyM68kRegister(&ptr, 15);

        (*m68k_ptr)+=2;

        *ptr++ = add_immed(REG_PC, REG_PC, 6);
        RA_FreeARMRegister(&ptr, displ);
    }
    /* 0100100000xxxxxx - NBCD */
    else if ((opcode & 0xffc0) == 0x4800 && (opcode & 0x08) != 0x08)
    {
        printf("[LINE4] Not implemented NBCD\n");
    }
    /* 0100100001000xxx - SWAP */
    else if ((opcode & 0xfff8) == 0x4840)
    {
        uint8_t reg = RA_MapM68kRegister(&ptr, opcode & 7);
        RA_SetDirtyM68kRegister(&ptr, opcode & 7);
        *ptr++ = rors_immed(reg, reg, 16);

        *ptr++ = add_immed(REG_PC, REG_PC, 2);

        uint8_t mask = M68K_GetSRMask(BE16((*m68k_ptr)[0]));
        uint8_t update_mask = (SR_C | SR_V | SR_Z | SR_N) & ~mask;

        if (update_mask)
        {
            *ptr++ = bic_immed(REG_SR, REG_SR, update_mask);
            if (update_mask & SR_N)
                *ptr++ = orr_cc_immed(ARM_CC_MI, REG_SR, REG_SR, SR_N);
            if (update_mask & SR_Z)
                *ptr++ = orr_cc_immed(ARM_CC_EQ, REG_SR, REG_SR, SR_Z);
        }
    }
    /* 0100100001001xxx - BKPT */
    else if ((opcode & 0xfff8) == 0x4848)
    {
        printf("[LINE4] Not implemented BKPT\n");
    }
    /* 0100100001xxxxxx - PEA */
    else if ((opcode & 0xffc0) == 0x4840 && (opcode & 0x38) != 0x08)
    {
        uint8_t sp;
        uint8_t ea;
        uint8_t ext_words = 0;

        ptr = EMIT_LoadFromEffectiveAddress(ptr, 0, &ea, opcode & 0x3f, (*m68k_ptr), &ext_words);
        (*m68k_ptr) += ext_words;

        sp = RA_MapM68kRegister(&ptr, 15);
        RA_SetDirtyM68kRegister(&ptr, 15);

        *ptr++ = str_offset_preindex(sp, ea, -4);

        RA_FreeARMRegister(&ptr, sp);
        RA_FreeARMRegister(&ptr, ea);

        *ptr++ = add_immed(REG_PC, REG_PC, 2 * (ext_words + 1));
    }
    /* 0100101011111100 - ILLEGAL */
    else if (opcode == 0x4afc)
    {
        *ptr++ = udf(0x4afc);
    }
    /* 0100101011xxxxxx - TAS */
    else if ((opcode & 0xffc0) == 0x4ac0)
    {
        printf("[LINE4] Not implemented TAS\n");
    }
    /* 0100101011xxxxxx - TST */
    else if ((opcode & 0xff00) == 0x4a00 && (opcode & 0xc0) != 0xc0)
    {
        ptr = EMIT_TST(ptr, opcode, m68k_ptr);
    }
    /* 0100110000xxxxxx - MULU, MULS, DIVU, DIVUL, DIVS, DIVSL */
    else if ((opcode & 0xff80) == 0x4c00 || (opcode == 0x83c0))
    {
        ptr = EMIT_MUL_DIV(ptr, opcode, m68k_ptr);
    }
    /* 010011100100xxxx - TRAP */
    else if ((opcode & 0xfff0) == 0x4e40)
    {
        printf("[LINE4] Not implemented TRAP\n");
    }
    /* 0100111001010xxx - LINK */
    else if ((opcode & 0xfff8) == 0x4e50)
    {
        uint8_t sp;
        uint8_t displ;
        uint8_t reg;

        displ = RA_AllocARMRegister(&ptr);
        *ptr++ = ldrsh_offset(REG_PC, displ, 2);
        sp = RA_MapM68kRegister(&ptr, 15);
        reg = RA_MapM68kRegister(&ptr, 8 + (opcode & 7));
        *ptr++ = str_offset_preindex(sp, reg, -4);  /* SP = SP - 4; An -> (SP) */
        *ptr++ = mov_reg(reg, sp);
        RA_SetDirtyM68kRegister(&ptr, 8 + (opcode & 7));
        *ptr++ = add_reg(sp, sp, displ, 0);
        RA_SetDirtyM68kRegister(&ptr, 15);

        (*m68k_ptr)++;

        *ptr++ = add_immed(REG_PC, REG_PC, 4);
        RA_FreeARMRegister(&ptr, displ);
    }
    /* 0100111001011xxx - UNLK */
    else if ((opcode & 0xfff8) == 0x4e58)
    {
        uint8_t sp;
        uint8_t reg;

        sp = RA_MapM68kRegisterForWrite(&ptr, 15);
        reg = RA_MapM68kRegister(&ptr, 8 + (opcode & 7));

        *ptr++ = mov_reg(sp, reg);
        *ptr++ = ldr_offset_postindex(sp, reg, 4);

        RA_SetDirtyM68kRegister(&ptr, 8 + (opcode & 7));
        RA_SetDirtyM68kRegister(&ptr, 15);

        *ptr++ = add_immed(REG_PC, REG_PC, 2);
    }
    /* 010011100110xxxx - MOVE USP */
    else if ((opcode & 0xfff0) == 0x4e60)
    {
        printf("[LINE4] Not implemented MOVE USP\n");
    }
    /* 0100111001110000 - RESET */
    else if (opcode == 0x4e70)
    {
        printf("[LINE4] Not implemented RESET\n");
    }
    /* 0100111001110000 - NOP */
    else if (opcode == 0x4e71)
    {
        *ptr++ = add_immed(REG_PC, REG_PC, 2);
    }
    /* 0100111001110010 - STOP */
    else if (opcode == 0x4e72)
    {
        printf("[LINE4] Not implemented STOP\n");
    }
    /* 0100111001110011 - RTE */
    else if (opcode == 0x4e73)
    {
        printf("[LINE4] Not implemented RTE\n");
    }
    /* 0100111001110100 - RTD */
    else if (opcode == 0x4e74)
    {
        uint8_t tmp = RA_AllocARMRegister(&ptr);
        uint8_t tmp2 = RA_AllocARMRegister(&ptr);
        uint8_t sp = RA_MapM68kRegister(&ptr, 15);

        /* Fetch return address from stack */
        *ptr++ = ldr_offset_postindex(sp, tmp2, 4);
        *ptr++ = ldrsh_offset(REG_PC, tmp, 2);
        *ptr++ = add_reg(sp, sp, tmp, 0);
        *ptr++ = mov_reg(REG_PC, tmp2);
        RA_SetDirtyM68kRegister(&ptr, 15);
        *ptr++ = INSN_TO_LE(0xffffffff);
        RA_FreeARMRegister(&ptr, tmp);
        RA_FreeARMRegister(&ptr, tmp2);
    }
    /* 0100111001110101 - RTS */
    else if (opcode == 0x4e75)
    {
        uint8_t tmp = RA_AllocARMRegister(&ptr);
        uint8_t sp = RA_MapM68kRegister(&ptr, 15);

        /* Fetch return address from stack */
        *ptr++ = ldr_offset_postindex(sp, tmp, 4);
        *ptr++ = mov_reg(REG_PC, tmp);
        RA_SetDirtyM68kRegister(&ptr, 15);
        *ptr++ = INSN_TO_LE(0xffffffff);
        RA_FreeARMRegister(&ptr, tmp);
    }
    /* 0100111001110110 - TRAPV */
    else if (opcode == 0x4e76)
    {
        printf("[LINE4] Not implemented TRAPV\n");
    }
    /* 0100111001110111 - RTR */
    else if (opcode == 0x4e77)
    {
        uint8_t tmp = RA_AllocARMRegister(&ptr);
        uint8_t sp = RA_MapM68kRegister(&ptr, 15);

        /* Fetch status byte from stack */
        *ptr++ = ldrh_offset_postindex(sp, tmp, 2);
        *ptr++ = bic_immed(REG_SR, REG_SR, 0x1f);
        *ptr++ = bic_immed(tmp, tmp, 0x1f);
        *ptr++ = orr_reg(REG_SR, REG_SR, tmp, 0);
        /* Fetch return address from stack */
        *ptr++ = ldr_offset_postindex(sp, tmp, 4);
        *ptr++ = mov_reg(REG_PC, tmp);
        RA_SetDirtyM68kRegister(&ptr, 15);
        *ptr++ = INSN_TO_LE(0xffffffff);
        RA_FreeARMRegister(&ptr, tmp);
    }
    /* 010011100111101x - MOVEC */
    else if ((opcode & 0xfffe) == 0x4e7a)
    {
        printf("[LINE4] Not implemented MOVEC\n");
    }
    /* 0100111010xxxxxx - JSR */
    else if ((opcode & 0xffc0) == 0x4e80)
    {
        uint8_t ext_words = 0;
        uint8_t ea;
        uint8_t sp;

        sp = RA_MapM68kRegister(&ptr, 15);
        ptr = EMIT_LoadFromEffectiveAddress(ptr, 0, &ea, opcode & 0x3f, (*m68k_ptr), &ext_words);
        *ptr++ = add_immed(REG_PC, REG_PC, 2 * (ext_words + 1));
        *ptr++ = str_offset_preindex(sp, REG_PC, -4);
        RA_SetDirtyM68kRegister(&ptr, 15);
        *ptr++ = mov_reg(REG_PC, ea);
        (*m68k_ptr) += ext_words;
        RA_FreeARMRegister(&ptr, ea);
        *ptr++ = INSN_TO_LE(0xffffffff);
    }
    /* 0100111011xxxxxx - JMP */
    else if ((opcode & 0xffc0) == 0x4ec0)
    {
        uint8_t ext_words = 0;
        uint8_t ea;

        ptr = EMIT_LoadFromEffectiveAddress(ptr, 0, &ea, opcode & 0x3f, (*m68k_ptr), &ext_words);
        *ptr++ = mov_reg(REG_PC, ea);
        (*m68k_ptr) += ext_words;
        RA_FreeARMRegister(&ptr, ea);
        *ptr++ = INSN_TO_LE(0xffffffff);
    }
    /* 01001x001xxxxxxx - MOVEM */
    else if ((opcode & 0xfb80) == 0x4880)
    {
        printf("[LINE4] Not implemented MOVEM\n");
    }
    /* 0100xxx111xxxxxx - LEA */
    else if ((opcode & 0xf1c0) == 0x41c0)
    {
        uint8_t dest = RA_MapM68kRegisterForWrite(&ptr, 8 + ((opcode >> 9) & 7));
        uint8_t ea;
        uint8_t ext_words = 0;

        ptr = EMIT_LoadFromEffectiveAddress(ptr, 0, &ea, opcode & 0x3f, (*m68k_ptr), &ext_words);
        *ptr++ = mov_reg(dest, ea);
        (*m68k_ptr) += ext_words;

        *ptr++ = add_immed(REG_PC, REG_PC, 2 * (ext_words + 1));
        RA_FreeARMRegister(&ptr, ea);
    }
    /* 0100xxx1x0xxxxxx - CHK */
    else if ((opcode & 0xf140) == 0x4100)
    {
        printf("[LINE4] Not implemented CHK\n");
    }

    return ptr;
}
