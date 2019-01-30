#include <stdint.h>
#include <stdio.h>
#include "ARM.h"
#include "M68k.h"
#include "RegisterAllocator.h"

#define M68K_EA_DA 0x8000
#define M68K_EA_REG 0x7000
#define M68K_EA_WL 0x0800
#define M68K_EA_SCALE 0x0600
#define M68K_EA_FULL 0x0100
#define M68K_EA_OFF8 0x00FF

#define M68K_EA_BS 0x0080
#define M68K_EA_IS 0x0040
#define M68K_EA_BD_SIZE 0x0030
#define M68K_EA_IIS 0x0007

/*
    Emits ARM insns to load effective address and read value from ther to specified register.

    Inputs:
        ptr     pointer to ARM instruction stream
        size    size of data for load operation, can be 4 (long), 2 (short) or 1 (byte).
                If size of 0 is specified the function does not load a value from EA into
                register but rather loads the EA into that register.
                If postincrement or predecrement modes are used and size 0 is specified, then
                the instruction translator is reponsible for increasing/decreasing the address
                register, otherwise it is done in this function!
        arm_reg ARM register to store the EA or value from EA into
        ea      EA encoded field.
        m68k_ptr pointer to m68k instruction stream past the instruction opcode itself. It may
                be increased if further bytes from m68k side are read

    Output:
        ptr     pointer to ARM instruction stream after the newly generated code
*/
uint32_t *EMIT_LoadFromEffectiveAddress(uint32_t *ptr, uint8_t size, uint8_t *arm_reg, uint8_t ea, uint16_t *m68k_ptr, uint8_t *ext_words)
{
    uint8_t mode = ea >> 3;
    uint8_t src_reg = ea & 7;

    if (mode == 0) /* Mode 000: Dn */
    {
        switch (size)
        {
            case 4:
            case 2:
            case 1:
                *arm_reg = RA_MapM68kRegister(&ptr, src_reg);
                break;
            case 0:
                *arm_reg = RA_AllocARMRegister(&ptr);
                *ptr++ = add_immed(*arm_reg, REG_CTX, __builtin_offsetof(struct M68KState, D[src_reg]));
                break;
            default:
                printf("Wrong size\n");
                break;
        }
    }
    else if (mode == 1) /* Mode 001: An */
    {
        switch (size)
        {
            case 4:
            case 2:
                *arm_reg = RA_MapM68kRegister(&ptr, src_reg + 8);
                break;
            case 0:
                *arm_reg = RA_AllocARMRegister(&ptr);
                *ptr++ = add_immed(*arm_reg, REG_CTX, __builtin_offsetof(struct M68KState, A[src_reg]));
                break;
            default:
                printf("Wrong size\n");
                break;
        }
    }
    else
    {
        *arm_reg = RA_AllocARMRegister(&ptr);

        if (mode == 2) /* Mode 002: (An) */
        {
            if (size == 0)
                *ptr++ = ldr_offset(REG_CTX, *arm_reg, __builtin_offsetof(struct M68KState, A[src_reg]));
            else
            {
                uint8_t reg_An = RA_MapM68kRegister(&ptr, src_reg + 8);
                switch (size)
                {
                    case 4:
                        *ptr++ = ldr_offset(reg_An, *arm_reg, 0);
                        break;
                    case 2:
                        *ptr++ = ldrh_offset(reg_An, *arm_reg, 0);
                        break;
                    case 1:
                        *ptr++ = ldrb_offset(reg_An, *arm_reg, 0);
                        break;
                    default:
                        printf("Unknown size opcode\n");
                        break;
                }
            }
        }
        else if (mode == 3) /* Mode 003: (An)+ */
        {
            if (size == 0)
                *ptr++ = ldr_offset(REG_CTX, *arm_reg, __builtin_offsetof(struct M68KState, A[src_reg]));
            else
            {
                uint8_t reg_An = RA_MapM68kRegister(&ptr, src_reg + 8);

                RA_SetDirtyM68kRegister(&ptr, 8 + src_reg);

                switch (size)
                {
                    case 4:
                        *ptr++ = ldr_offset_postindex(reg_An, *arm_reg, 4);
                        break;
                    case 2:
                        *ptr++ = ldrh_offset_postindex(reg_An, *arm_reg, 2);
                        break;
                    case 1:
                        if (src_reg == 7)
                            *ptr++ = ldrb_offset_postindex(reg_An, *arm_reg, 2);
                        else
                            *ptr++ = ldrb_offset_postindex(reg_An, *arm_reg, 1);
                        break;
                    default:
                        printf("Unknown size opcode\n");
                        break;
                }
            }
        }
        else if (mode == 4) /* Mode 004: -(An) */
        {
            if (size == 0)
                *ptr++ = ldr_offset(REG_CTX, *arm_reg, __builtin_offsetof(struct M68KState, A[src_reg]));
            else
            {
                uint8_t reg_An = RA_MapM68kRegister(&ptr, src_reg + 8);
                RA_SetDirtyM68kRegister(&ptr, 8 + src_reg);

                switch (size)
                {
                    case 4:
                        *ptr++ = ldr_offset_preindex(reg_An, *arm_reg, -4);
                        break;
                    case 2:
                        *ptr++ = ldrh_offset_preindex(reg_An, *arm_reg, -2);
                        break;
                    case 1:
                        if (src_reg == 7)
                            *ptr++ = ldrb_offset_preindex(reg_An, *arm_reg, -2);
                        else
                            *ptr++ = ldrb_offset_preindex(reg_An, *arm_reg, -1);
                        break;
                    default:
                        printf("Unknown size opcode\n");
                        break;
                }
            }
        }
        else if (mode == 5) /* Mode 005: (d16, An) */
        {
            uint8_t reg_An = RA_MapM68kRegister(&ptr, src_reg + 8);
            uint8_t reg_d16 = RA_AllocARMRegister(&ptr);
            *ptr++ = ldrsh_offset(REG_PC, reg_d16, 2);
            (*ext_words)++;

            switch (size)
            {
                case 4:
                    *ptr++ = ldr_regoffset(reg_An, *arm_reg, reg_d16, 0);
                    break;
                case 2:
                    *ptr++ = ldrh_regoffset(reg_An, *arm_reg, reg_d16);
                    break;
                case 1:
                    *ptr++ = ldrb_regoffset(reg_An, *arm_reg, reg_d16, 0);
                    break;
                case 0:
                    *ptr++ = add_reg(*arm_reg, reg_An, reg_d16, 0);
                    break;
                default:
                    printf("Unknown size opcode\n");
                    break;
            }
            RA_FreeARMRegister(&ptr, reg_d16);
        }
        else if (mode == 6) /* Mode 006: (d8, An, Xn.SIZE*SCALE) */
        {
            uint16_t brief = m68k_ptr[(*ext_words)++];
            uint8_t extra_reg = (brief >> 12) & 7;

            if ((brief & 0x0100) == 0)
            {
                uint8_t reg_An = RA_MapM68kRegister(&ptr, src_reg + 8);
                uint8_t tmp1 = RA_AllocARMRegister(&ptr);
                uint8_t tmp2 = 0xff;
                int8_t displ = brief & 0xff;

                if (displ > 0)
                {
                    *ptr++ = add_immed(tmp1, reg_An, displ);
                }
                else
                {
                    *ptr++ = sub_immed(tmp1, reg_An, -displ);
                }

                if (brief & (1 << 11))
                {
                    if (brief & 0x8000)
                        tmp2 = RA_MapM68kRegister(&ptr, 8 + extra_reg);
                    else
                        tmp2 = RA_MapM68kRegister(&ptr, extra_reg);
                }
                else
                {
                    tmp2 = RA_AllocARMRegister(&ptr);
                    if (brief & 0x8000)
                        *ptr++ = ldrsh_offset(REG_CTX, tmp2, 2 + __builtin_offsetof(struct M68KState, A[extra_reg]));
                    else
                        *ptr++ = ldrsh_offset(REG_CTX, tmp2, 2 + __builtin_offsetof(struct M68KState, D[extra_reg]));
                }

                switch (size)
                {
                    case 4:
                        *ptr++ = ldr_regoffset(tmp1, *arm_reg, tmp2, (brief >> 9) & 3);
                        break;
                    case 2:
                        *ptr++ = lsl_immed(tmp2, tmp2, (brief >> 9) & 3);
                        *ptr++ = ldrh_regoffset(tmp1, *arm_reg, tmp2);
                        break;
                    case 1:
                        *ptr++ = ldrb_regoffset(tmp1, *arm_reg, tmp2, (brief >> 9) & 3);
                        break;
                    case 0:
                        *ptr++ = add_reg(*arm_reg, tmp1, tmp2, (brief >> 9) & 3);
                        break;
                    default:
                        printf("Unknown size opcode\n");
                        break;
                }
                RA_FreeARMRegister(&ptr, tmp1);
                RA_FreeARMRegister(&ptr, tmp2);
            }
            else
            {
                uint8_t bd_reg = 0xff;
                uint8_t outer_reg = 0xff;
                uint8_t base_reg = 0xff;
                uint8_t index_reg = 0xff;

                /* Check if base register is suppressed */
                if (!(brief & M68K_EA_BS))
                {
                    /* Base register in use. Alloc it and load its contents */
                    base_reg = RA_MapM68kRegister(&ptr, 8 + src_reg);
                }

                /* Check if index register is in use */
                if (!(brief & M68K_EA_IS))
                {
                    /* Index register in use. Alloc it and load its contents */
                    if (brief & (1 << 11))
                    {
                        if (brief & 0x8000)
                            index_reg = RA_MapM68kRegister(&ptr, 8 + extra_reg);
                        else
                            index_reg = RA_MapM68kRegister(&ptr, extra_reg);
                    }
                    else
                    {
                        index_reg = RA_AllocARMRegister(&ptr);
                        if (brief & 0x8000)
                            *ptr++ = ldrsh_offset(REG_CTX, index_reg, 2 + __builtin_offsetof(struct M68KState, A[extra_reg]));
                        else
                            *ptr++ = ldrsh_offset(REG_CTX, index_reg, 2 + __builtin_offsetof(struct M68KState, D[extra_reg]));
                    }
                }

                /* Check if base displacement needs to be fetched */
                switch ((brief & M68K_EA_BD_SIZE) >> 4)
                {
                    case 2: /* Word displacement */
                        bd_reg = RA_AllocARMRegister(&ptr);
                        *ptr++ = ldrsh_offset(REG_PC, bd_reg, 2 + *ext_words * 2);
                        (*ext_words)++;
                        break;
                    case 3: /* Long displacement */
                        bd_reg = RA_AllocARMRegister(&ptr);
                        *ptr++ = ldr_offset(REG_PC, bd_reg, 2 + *ext_words * 2);
                        (*ext_words) += 2;
                        break;
                }

                /* Check if outer displacement needs to be fetched */
                switch ((brief & M68K_EA_IIS) & 3)
                {
                    case 2: /* Word outer displacement */
                        outer_reg = RA_AllocARMRegister(&ptr);
                        *ptr++ = ldrsh_offset(REG_PC, outer_reg, 2 + *ext_words * 2);
                        (*ext_words)++;
                        break;
                    case 3: /* Long outer displacement */
                        outer_reg = RA_AllocARMRegister(&ptr);
                        *ptr++ = ldr_offset(REG_PC, outer_reg, 2 + *ext_words * 2);
                        (*ext_words) += 2;
                        break;
                }

                if ((brief & 0x0f) == 0)
                {
                    /* Address register indirect with index mode */
                    if (base_reg != 0xff && bd_reg != 0xff)
                    {
                        *ptr++ = add_reg(bd_reg, base_reg, bd_reg, 0);
                    }
                    else if (bd_reg == 0xff && base_reg != 0xff)
                    {
                        bd_reg = base_reg;
                    }
                    /*
                        Now, either base register or base displacement were given, if
                        index register was specified, use it.
                    */
                    switch (size)
                    {
                        case 4:
                            if (index_reg != 0xff)
                                *ptr++ = ldr_regoffset(bd_reg, *arm_reg, index_reg, (brief >> 9) & 3);
                            else
                                *ptr++ = ldr_offset(bd_reg, *arm_reg, 0);
                            break;
                        case 2:
                            if (index_reg != 0xff)
                            {
                                uint8_t t = RA_AllocARMRegister(&ptr);
                                *ptr++ = lsl_immed(t, index_reg, (brief >> 9) & 3);
                                *ptr++ = ldrh_regoffset(bd_reg, *arm_reg, t);
                                RA_FreeARMRegister(&ptr, t);
                            }
                            else
                                *ptr++ = ldrh_offset(bd_reg, *arm_reg, 0);
                            break;
                        case 1:
                            if (index_reg != 0xff)
                                *ptr++ = ldrb_regoffset(bd_reg, *arm_reg, index_reg, (brief >> 9) & 3);
                            else
                                *ptr++ = ldrb_offset(bd_reg, *arm_reg, 0);
                            break;
                        case 0:
                            if (index_reg != 0xff)
                                *ptr++ = add_reg(*arm_reg, bd_reg, index_reg, (brief >> 9) & 3);
                            else
                                *ptr++ = mov_reg(*arm_reg, bd_reg);
                            break;
                        default:
                            printf("Unknown size opcode\n");
                            break;
                    }
                }
                else
                {
                    if (bd_reg == 0xff)
                    {
                        bd_reg = RA_AllocARMRegister(&ptr);
                        *ptr++ = mov_reg(bd_reg, base_reg);
                        base_reg = 0xff;
                    }

                    /* Postindexed mode */
                    if (brief & 0x04)
                    {
                        /* Fetch data from base reg */
                        if (base_reg == 0xff)
                            *ptr++ = ldr_offset(bd_reg, bd_reg, 0);
                        else
                            *ptr++ = ldr_regoffset(bd_reg, bd_reg, base_reg, 0);
                        if (outer_reg != 0xff)
                            *ptr++ = add_reg(bd_reg, bd_reg, outer_reg, 0);

                        switch (size)
                        {
                            case 4:
                                *ptr++ = ldr_regoffset(bd_reg, *arm_reg, index_reg, (brief >> 9) & 3);
                                break;
                            case 2:
                            {
                                uint8_t t = RA_AllocARMRegister(&ptr);
                                *ptr++ = lsl_immed(t, index_reg, (brief >> 9) & 3);
                                *ptr++ = ldrh_regoffset(base_reg, *arm_reg, t);
                                RA_FreeARMRegister(&ptr, t);
                                break;
                            }
                            case 1:
                                *ptr++ = ldrb_regoffset(bd_reg, *arm_reg, index_reg, (brief >> 9) & 3);
                                break;
                            case 0:
                                *ptr++ = add_reg(*arm_reg, bd_reg, index_reg, (brief >> 9) & 3);
                                break;
                            default:
                                printf("Unknown size opcode\n");
                                break;
                        }
                    }
                    else /* Preindexed mode */
                    {
                        /* Fetch data from base reg with eventually applied index */
                        if (brief & M68K_EA_IS)
                        {
                            if (bd_reg == 0xff) {
                                bd_reg = RA_AllocARMRegister(&ptr);
                                *ptr++ = ldr_offset(base_reg, bd_reg, 0);
                            }
                            else
                                *ptr++ = ldr_regoffset(base_reg, bd_reg, bd_reg, 0);
                        }
                        else
                        {
                            if (bd_reg != 0xff)
                                *ptr++ = add_reg(bd_reg, base_reg, bd_reg, 0);
                            *ptr++ = ldr_regoffset(bd_reg, bd_reg, index_reg, (brief >> 9) & 3);
                        }

                        if (outer_reg != 0xff)
                            *ptr++ = add_reg(bd_reg, bd_reg, outer_reg, 0);

                        switch (size)
                        {
                            case 4:
                                *ptr++ = ldr_offset(bd_reg, *arm_reg, 0);
                                break;
                            case 2:
                                *ptr++ = ldrh_offset(bd_reg, *arm_reg, 0);
                                break;
                            case 1:
                                *ptr++ = ldrb_offset(bd_reg, *arm_reg, 0);
                                break;
                            case 0:
                                *ptr++ = mov_reg(*arm_reg, bd_reg);
                                break;
                            default:
                                printf("Unknown size opcode\n");
                                break;
                        }
                    }
                }

                if (bd_reg != 0xff)
                    RA_FreeARMRegister(&ptr, bd_reg);
                if (outer_reg != 0xff)
                    RA_FreeARMRegister(&ptr, outer_reg);
                if (base_reg != 0xff)
                    RA_FreeARMRegister(&ptr, base_reg);
                if (index_reg != 0xff)
                    RA_FreeARMRegister(&ptr, index_reg);
            }
        }
        else if (mode == 7)
        {
            if (src_reg == 2) /* (d16, PC) mode */
            {
                uint8_t reg_d16 = RA_AllocARMRegister(&ptr);
                *ptr++ = ldrsh_offset(REG_PC, reg_d16, 2);
                *ptr++ = add_immed(reg_d16, reg_d16, 2);
                (*ext_words)++;

                switch (size)
                {
                    case 4:
                        *ptr++ = ldr_regoffset(REG_PC, *arm_reg, reg_d16, 0);
                        break;
                    case 2:
                        *ptr++ = ldrh_regoffset(REG_PC, *arm_reg, reg_d16);
                        break;
                    case 1:
                        *ptr++ = ldrb_regoffset(REG_PC, *arm_reg, reg_d16, 0);
                        break;
                    case 0:
                        *ptr++ = add_reg(*arm_reg, REG_PC, reg_d16, 0);
                        break;
                    default:
                        printf("Unknown size opcode\n");
                        break;
                }
                RA_FreeARMRegister(&ptr, reg_d16);
            }
            if (src_reg == 0)
            {
                (*ext_words)++;
                if (size == 0)
                    *ptr++ = ldrsh_offset(REG_PC, *arm_reg, 2);
                else
                {
                    uint8_t tmp_reg = RA_AllocARMRegister(&ptr);
                    *ptr++ = ldrsh_offset(REG_PC, tmp_reg, 2);
                    switch (size)
                    {
                        case 4:
                            *ptr++ = ldr_offset(tmp_reg, *arm_reg, 0);
                            break;
                        case 2:
                            *ptr++ = ldrh_offset(tmp_reg, *arm_reg, 0);
                            break;
                        case 1:
                            *ptr++ = ldrb_offset(tmp_reg, *arm_reg, 0);
                            break;
                    }
                    RA_FreeARMRegister(&ptr, tmp_reg);
                }
            }
            else if (src_reg == 1)
            {
                (*ext_words) += 2;

                if (size == 0)
                    *ptr++ = ldr_offset(REG_PC, *arm_reg, 2);
                else
                {
                    uint8_t tmp_reg = RA_AllocARMRegister(&ptr);
                    *ptr++ = ldr_offset(REG_PC, tmp_reg, 2);

                    switch (size)
                    {
                        case 4:
                            *ptr++ = ldr_offset(tmp_reg, *arm_reg, 0);
                            break;
                        case 2:
                            *ptr++ = ldrh_offset(tmp_reg, *arm_reg, 0);
                            break;
                        case 1:
                            *ptr++ = ldrb_offset(tmp_reg, *arm_reg, 0);
                            break;
                    }
                    RA_FreeARMRegister(&ptr, tmp_reg);
                }
            }
            else if (src_reg == 4)
            {
                switch (size)
                {
                    case 4:
                        *ptr++ = ldr_offset(REG_PC, *arm_reg, 2);
                        (*ext_words) += 2;
                        break;
                    case 2:
                        *ptr++ = ldrh_offset(REG_PC, *arm_reg, 2);
                        (*ext_words)++;
                        break;
                    case 1:
                        *ptr++ = ldrb_offset(REG_PC, *arm_reg, 3);
                        (*ext_words)++;
                        break;
                    case 0:
                        *ptr++ = add_immed(*arm_reg, REG_PC, 2);
                        break;
                }
            }
        }
    }

    return ptr;
}

/*
    Emits ARM insns to load effective address and store value from specified register to the EA.

    Inputs:
        ptr     pointer to ARM instruction stream
        size    size of data for load operation, can be 4 (long), 2 (short) or 1 (byte).
                If size of 0 is specified the function does not load a value from EA into
                register but rather loads the EA into that register.
                If postincrement or predecrement modes are used and size 0 is specified, then
                the instruction translator is reponsible for increasing/decreasing the address
                register, otherwise it is done in this function!
        arm_reg ARM register to store the EA or value from EA into
        ea      EA encoded field.
        m68k_ptr pointer to m68k instruction stream past the instruction opcode itself. It may
                be increased if further bytes from m68k side are read

    Output:
        ptr     pointer to ARM instruction stream after the newly generated code
*/
uint32_t *EMIT_StoreToEffectiveAddress(uint32_t *ptr, uint8_t size, uint8_t *arm_reg, uint8_t ea, uint16_t *m68k_ptr, uint8_t *ext_words)
{
    uint8_t mode = ea >> 3;
    uint8_t src_reg = ea & 7;

    if (size == 0)
        *arm_reg = RA_AllocARMRegister(&ptr);

    if (mode == 0) /* Mode 000: Dn */
    {
        uint8_t reg_dest;
        switch (size)
        {
            case 4:
                reg_dest = RA_MapM68kRegisterForWrite(&ptr, src_reg);
                *ptr++ = mov_reg(reg_dest, *arm_reg);
                break;
            case 2:
                reg_dest = RA_MapM68kRegister(&ptr, src_reg);
                RA_SetDirtyM68kRegister(&ptr, src_reg);
                *ptr++ = lsr_immed(reg_dest, reg_dest, 16);
                *ptr++ = lsl_immed(reg_dest, reg_dest, 16);
                *ptr++ = uxtah(reg_dest, reg_dest, *arm_reg);
                break;
            case 1:
                reg_dest = RA_MapM68kRegister(&ptr, src_reg);
                RA_SetDirtyM68kRegister(&ptr, src_reg);
                *ptr++ = bic_immed(reg_dest, reg_dest, 0xff);
                *ptr++ = uxtab(reg_dest, reg_dest, *arm_reg);
                break;
            case 0:
                *ptr++ = add_immed(*arm_reg, REG_CTX, __builtin_offsetof(struct M68KState, D[src_reg]));
                break;
            default:
                printf("Wrong size\n");
                break;
        }
    }
    else if (mode == 1) /* Mode 001: An */
    {
        uint8_t reg_dest;
        switch (size)
        {
            case 4:
                reg_dest = RA_MapM68kRegisterForWrite(&ptr, 8 + src_reg);
                *ptr++ = mov_reg(reg_dest, *arm_reg);
                break;
            case 2:
                reg_dest = RA_MapM68kRegister(&ptr, 8 + src_reg);
                RA_SetDirtyM68kRegister(&ptr, src_reg);
                *ptr++ = lsr_immed(reg_dest, reg_dest, 16);
                *ptr++ = lsl_immed(reg_dest, reg_dest, 16);
                *ptr++ = uxtah(reg_dest, reg_dest, *arm_reg);
                break;
                ;
            case 0:
                *ptr++ = add_immed(*arm_reg, REG_CTX, __builtin_offsetof(struct M68KState, A[src_reg]));
                break;
            default:
                printf("Wrong size\n");
                break;
        }
    }
    else
    {
        if (mode == 2) /* Mode 002: (An) */
        {
            if (size == 0)
                *ptr++ = ldr_offset(REG_CTX, *arm_reg, __builtin_offsetof(struct M68KState, A[src_reg]));
            else
            {
                uint8_t reg_An = RA_MapM68kRegister(&ptr, src_reg + 8);
                switch (size)
                {
                case 4:
                    *ptr++ = str_offset(reg_An, *arm_reg, 0);
                    break;
                case 2:
                    *ptr++ = strh_offset(reg_An, *arm_reg, 0);
                    break;
                case 1:
                    *ptr++ = strb_offset(reg_An, *arm_reg, 0);
                    break;
                default:
                    printf("Unknown size opcode\n");
                    break;
                }
            }
        }
        else if (mode == 3) /* Mode 003: (An)+ */
        {
            if (size == 0)
                *ptr++ = ldr_offset(REG_CTX, *arm_reg, __builtin_offsetof(struct M68KState, A[src_reg]));
            else
            {
                uint8_t reg_An = RA_MapM68kRegister(&ptr, src_reg + 8);

                RA_SetDirtyM68kRegister(&ptr, 8 + src_reg);

                switch (size)
                {
                case 4:
                    *ptr++ = str_offset_postindex(reg_An, *arm_reg, 4);
                    break;
                case 2:
                    *ptr++ = strh_offset_postindex(reg_An, *arm_reg, 2);
                    break;
                case 1:
                    if (src_reg == 7)
                        *ptr++ = strb_offset_postindex(reg_An, *arm_reg, 2);
                    else
                        *ptr++ = strb_offset_postindex(reg_An, *arm_reg, 1);
                    break;
                default:
                    printf("Unknown size opcode\n");
                    break;
                }
            }
        }
        else if (mode == 4) /* Mode 004: -(An) */
        {
            if (size == 0)
                *ptr++ = ldr_offset(REG_CTX, *arm_reg, __builtin_offsetof(struct M68KState, A[src_reg]));
            else
            {
                uint8_t reg_An = RA_MapM68kRegister(&ptr, src_reg + 8);

                RA_SetDirtyM68kRegister(&ptr, 8 + src_reg);

                switch (size)
                {
                case 4:
                    *ptr++ = str_offset_preindex(reg_An, *arm_reg, -4);
                    break;
                case 2:
                    *ptr++ = strh_offset_preindex(reg_An, *arm_reg, -2);
                    break;
                case 1:
                    if (src_reg == 7)
                        *ptr++ = strb_offset_preindex(reg_An, *arm_reg, -2);
                    else
                        *ptr++ = strb_offset_preindex(reg_An, *arm_reg, -1);
                    break;
                default:
                    printf("Unknown size opcode\n");
                    break;
                }
            }
        }
        else if (mode == 5) /* Mode 005: (d16, An) */
        {
            uint8_t reg_An = RA_MapM68kRegister(&ptr, src_reg + 8);
            uint8_t reg_d16 = RA_AllocARMRegister(&ptr);
            *ptr++ = ldrsh_offset(REG_PC, reg_d16, 2);
            (*ext_words)++;

            switch (size)
            {
            case 4:
                *ptr++ = str_regoffset(reg_An, *arm_reg, reg_d16, 0);
                break;
            case 2:
                *ptr++ = strh_regoffset(reg_An, *arm_reg, reg_d16);
                break;
            case 1:
                *ptr++ = strb_regoffset(reg_An, *arm_reg, reg_d16, 0);
                break;
            case 0:
                *ptr++ = add_reg(*arm_reg, reg_An, reg_d16, 0);
                break;
            default:
                printf("Unknown size opcode\n");
                break;
            }
            RA_FreeARMRegister(&ptr, reg_d16);
        }
        else if (mode == 6) /* Mode 006: (d8, An, Xn.SIZE*SCALE) */
        {
            uint16_t brief = m68k_ptr[(*ext_words)++];
            uint8_t extra_reg = (brief >> 12) & 7;

            if ((brief & 0x0100) == 0)
            {
                uint8_t reg_An = RA_MapM68kRegister(&ptr, src_reg + 8);
                uint8_t tmp1 = RA_AllocARMRegister(&ptr);
                uint8_t tmp2 = 0xff;
                int8_t displ = brief & 0xff;

                if (displ > 0)
                {
                    *ptr++ = add_immed(tmp1, reg_An, displ);
                }
                else
                {
                    *ptr++ = sub_immed(tmp1, reg_An, -displ);
                }

                if (brief & (1 << 11))
                {
                    if (brief & 0x8000)
                        tmp2 = RA_MapM68kRegister(&ptr, 8 + extra_reg);
                    else
                        tmp2 = RA_MapM68kRegister(&ptr, extra_reg);
                }
                else
                {
                    tmp2 = RA_AllocARMRegister(&ptr);
                    if (brief & 0x8000)
                        *ptr++ = ldrsh_offset(REG_CTX, tmp2, 2 + __builtin_offsetof(struct M68KState, A[extra_reg]));
                    else
                        *ptr++ = ldrsh_offset(REG_CTX, tmp2, 2 + __builtin_offsetof(struct M68KState, D[extra_reg]));
                }

                switch (size)
                {
                case 4:
                    *ptr++ = str_regoffset(tmp1, *arm_reg, tmp2, (brief >> 9) & 3);
                    break;
                case 2:
                {
                    uint8_t t = RA_AllocARMRegister(&ptr);
                    *ptr++ = lsl_immed(t, tmp2, (brief >> 9) & 3);
                    *ptr++ = strh_regoffset(tmp1, *arm_reg, t);
                    RA_FreeARMRegister(&ptr, t);
                    break;
                }
                case 1:
                    *ptr++ = strb_regoffset(tmp1, *arm_reg, tmp2, (brief >> 9) & 3);
                    break;
                case 0:
                    *ptr++ = add_reg(*arm_reg, tmp1, tmp2, (brief >> 9) & 3);
                    break;
                default:
                    printf("Unknown size opcode\n");
                    break;
                }
                RA_FreeARMRegister(&ptr, tmp1);
                RA_FreeARMRegister(&ptr, tmp2);
            }
            else
            {
                uint8_t bd_reg = 0xff;
                uint8_t outer_reg = 0xff;
                uint8_t base_reg = 0xff;
                uint8_t index_reg = 0xff;

                /* Check if base register is suppressed */
                if (!(brief & M68K_EA_BS))
                {
                    /* Base register in use. Alloc it and load its contents */
                    base_reg = RA_MapM68kRegister(&ptr, 8 + src_reg);
                }

                /* Check if index register is in use */
                if (!(brief & M68K_EA_IS))
                {
                    /* Index register in use. Alloc it and load its contents */
                    if (brief & (1 << 11))
                    {
                        if (brief & 0x8000)
                            index_reg = RA_MapM68kRegister(&ptr, 8 + extra_reg);
                        else
                            index_reg = RA_MapM68kRegister(&ptr, extra_reg);
                    }
                    else
                    {
                        index_reg = RA_AllocARMRegister(&ptr);
                        if (brief & 0x8000)
                            *ptr++ = ldrsh_offset(REG_CTX, index_reg, 2 + __builtin_offsetof(struct M68KState, A[extra_reg]));
                        else
                            *ptr++ = ldrsh_offset(REG_CTX, index_reg, 2 + __builtin_offsetof(struct M68KState, D[extra_reg]));
                    }
                }

                /* Check if base displacement needs to be fetched */
                switch ((brief & M68K_EA_BD_SIZE) >> 4)
                {
                case 2: /* Word displacement */
                    bd_reg = RA_AllocARMRegister(&ptr);
                    *ptr++ = ldrsh_offset(REG_PC, bd_reg, 2 + *ext_words * 2);
                    (*ext_words)++;
                    break;
                case 3: /* Long displacement */
                    bd_reg = RA_AllocARMRegister(&ptr);
                    *ptr++ = ldr_offset(REG_PC, bd_reg, 2 + *ext_words * 2);
                    (*ext_words) += 2;
                    break;
                }

                /* Check if outer displacement needs to be fetched */
                switch ((brief & M68K_EA_IIS) & 3)
                {
                case 2: /* Word outer displacement */
                    outer_reg = RA_AllocARMRegister(&ptr);
                    *ptr++ = ldrsh_offset(REG_PC, outer_reg, 2 + *ext_words * 2);
                    (*ext_words)++;
                    break;
                case 3: /* Long outer displacement */
                    outer_reg = RA_AllocARMRegister(&ptr);
                    *ptr++ = ldr_offset(REG_PC, outer_reg, 2 + *ext_words * 2);
                    (*ext_words) += 2;
                    break;
                }

                if ((brief & 0x0f) == 0)
                {
                    /* Address register indirect with index mode */
                    if (base_reg != 0xff && bd_reg != 0xff)
                    {
                        *ptr++ = add_reg(bd_reg, base_reg, bd_reg, 0);
                    }
                    else if (bd_reg == 0xff && base_reg != 0xff)
                    {
                        bd_reg = base_reg;
                    }
                    /*
                        Now, either base register or base displacement were given, if
                        index register was specified, use it.
                    */
                    switch (size)
                    {
                    case 4:
                        if (index_reg != 0xff)
                            *ptr++ = str_regoffset(bd_reg, *arm_reg, index_reg, (brief >> 9) & 3);
                        else
                            *ptr++ = str_offset(bd_reg, *arm_reg, 0);
                        break;
                    case 2:
                        if (index_reg != 0xff)
                        {
                            uint8_t t = RA_AllocARMRegister(&ptr);
                            *ptr++ = lsl_immed(t, index_reg, (brief >> 9) & 3);
                            *ptr++ = strh_regoffset(bd_reg, *arm_reg, t);
                            RA_FreeARMRegister(&ptr, t);
                        }
                        else
                            *ptr++ = strh_offset(bd_reg, *arm_reg, 0);
                        break;
                    case 1:
                        if (index_reg != 0xff)
                            *ptr++ = strb_regoffset(bd_reg, *arm_reg, index_reg, (brief >> 9) & 3);
                        else
                            *ptr++ = strb_offset(bd_reg, *arm_reg, 0);
                        break;
                    case 0:
                        if (index_reg != 0xff)
                            *ptr++ = add_reg(*arm_reg, bd_reg, index_reg, (brief >> 9) & 3);
                        else
                            *ptr++ = mov_reg(*arm_reg, bd_reg);
                        break;
                    default:
                        printf("Unknown size opcode\n");
                        break;
                    }
                }
                else
                {
                    if (bd_reg == 0xff)
                    {
                        bd_reg = RA_AllocARMRegister(&ptr);
                        *ptr++ = mov_reg(bd_reg, base_reg);
                        base_reg = 0xff;
                    }

                    /* Postindexed mode */
                    if (brief & 0x04)
                    {
                        /* Fetch data from base reg */
                        if (base_reg == 0xff)
                            *ptr++ = ldr_offset(bd_reg, bd_reg, 0);
                        else
                            *ptr++ = ldr_regoffset(bd_reg, bd_reg, base_reg, 0);
                        if (outer_reg != 0xff)
                            *ptr++ = add_reg(bd_reg, bd_reg, outer_reg, 0);

                        switch (size)
                        {
                        case 4:
                            *ptr++ = str_regoffset(bd_reg, *arm_reg, index_reg, (brief >> 9) & 3);
                            break;
                        case 2:
                        {
                            uint8_t t = RA_AllocARMRegister(&ptr);
                            *ptr++ = lsl_immed(t, index_reg, (brief >> 9) & 3);
                            *ptr++ = strh_regoffset(base_reg, *arm_reg, t);
                            RA_FreeARMRegister(&ptr, t);
                            break;
                        }
                        case 1:
                            *ptr++ = strb_regoffset(bd_reg, *arm_reg, index_reg, (brief >> 9) & 3);
                            break;
                        case 0:
                            *ptr++ = add_reg(*arm_reg, bd_reg, index_reg, (brief >> 9) & 3);
                            break;
                        default:
                            printf("Unknown size opcode\n");
                            break;
                        }
                    }
                    else /* Preindexed mode */
                    {
                        /* Fetch data from base reg with eventually applied index */
                        if (brief & M68K_EA_IS)
                        {
                            if (bd_reg == 0xff)
                            {
                                bd_reg = RA_AllocARMRegister(&ptr);
                                *ptr++ = ldr_offset(base_reg, bd_reg, 0);
                            }
                            else
                                *ptr++ = ldr_regoffset(base_reg, bd_reg, bd_reg, 0);
                        }
                        else
                        {
                            if (bd_reg != 0xff)
                                *ptr++ = add_reg(bd_reg, base_reg, bd_reg, 0);
                            *ptr++ = ldr_regoffset(bd_reg, bd_reg, index_reg, (brief >> 9) & 3);
                        }

                        if (outer_reg != 0xff)
                            *ptr++ = add_reg(bd_reg, bd_reg, outer_reg, 0);

                        switch (size)
                        {
                        case 4:
                            *ptr++ = str_offset(bd_reg, *arm_reg, 0);
                            break;
                        case 2:
                            *ptr++ = strh_offset(bd_reg, *arm_reg, 0);
                            break;
                        case 1:
                            *ptr++ = strb_offset(bd_reg, *arm_reg, 0);
                            break;
                        case 0:
                            *ptr++ = mov_reg(*arm_reg, bd_reg);
                            break;
                        default:
                            printf("Unknown size opcode\n");
                            break;
                        }
                    }
                }

                if (bd_reg != 0xff)
                    RA_FreeARMRegister(&ptr, bd_reg);
                if (outer_reg != 0xff)
                    RA_FreeARMRegister(&ptr, outer_reg);
                if (base_reg != 0xff)
                    RA_FreeARMRegister(&ptr, base_reg);
                if (index_reg != 0xff)
                    RA_FreeARMRegister(&ptr, index_reg);
            }
        }
        else if (mode == 7)
        {
            if (src_reg == 0)
            {
                ext_words++;
                if (size == 0)
                    *ptr++ = ldrsh_offset(REG_PC, *arm_reg, 2);
                else
                {
                    uint8_t tmp_reg = RA_AllocARMRegister(&ptr);
                    *ptr++ = ldrsh_offset(REG_PC, tmp_reg, 2);
                    switch (size)
                    {
                    case 4:
                        *ptr++ = str_offset(tmp_reg, *arm_reg, 0);
                        break;
                    case 2:
                        *ptr++ = strh_offset(tmp_reg, *arm_reg, 0);
                        break;
                    case 1:
                        *ptr++ = strb_offset(tmp_reg, *arm_reg, 0);
                        break;
                    }
                    RA_FreeARMRegister(&ptr, tmp_reg);
                }
            }
            else if (src_reg == 1)
            {
                ext_words += 2;

                if (size == 0)
                    *ptr++ = ldr_offset(REG_PC, *arm_reg, 2);
                else
                {
                    uint8_t tmp_reg = RA_AllocARMRegister(&ptr);
                    *ptr++ = ldr_offset(REG_PC, tmp_reg, 2);

                    switch (size)
                    {
                    case 4:
                        *ptr++ = str_offset(tmp_reg, *arm_reg, 0);
                        break;
                    case 2:
                        *ptr++ = strh_offset(tmp_reg, *arm_reg, 0);
                        break;
                    case 1:
                        *ptr++ = strb_offset(tmp_reg, *arm_reg, 0);
                        break;
                    }
                    RA_FreeARMRegister(&ptr, tmp_reg);
                }
            }
        }
    }

    return ptr;
}
