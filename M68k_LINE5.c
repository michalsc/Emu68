#include <stdint.h>
#include <stdlib.h>

#include <stdio.h>
#include "ARM.h"
#include "M68k.h"
#include "RegisterAllocator.h"


static uint8_t M68K_ccTo_ARM[] = {
    ARM_CC_AL,      // M_CC_T
    0x0f,           // M_CC_F
    ARM_CC_HI,      // M_CC_HI
    ARM_CC_LS,      // M_CC_LS
    ARM_CC_CC,      // M_CC_CC
    ARM_CC_CS,      // M_CC_CS
    ARM_CC_NE,      // M_CC_NE
    ARM_CC_EQ,      // M_CC_EQ
    ARM_CC_VC,      // M_CC_VC
    ARM_CC_VS,      // M_CC_VS
    ARM_CC_PL,      // M_CC_PL
    ARM_CC_MI,      // M_CC_MI
    ARM_CC_GE,      // M_CC_GE
    ARM_CC_LT,      // M_CC_LT
    ARM_CC_GT,      // M_CC_GT
    ARM_CC_LE       // M_CC_LE
};

static uint32_t *EMIT_LoadARMCC(uint32_t *ptr, uint8_t m68k_cc)
{
    uint8_t tmp = RA_AllocARMRegister(&ptr);

    *ptr++ = mov_reg_shift(tmp, m68k_cc, 28);     /* Copy m68k_cc */
    *ptr++ = bic_immed(tmp, tmp, 0x203); /* Clear bits 0 and 1 */
    *ptr++ = tst_immed(tmp, 2);
    *ptr++ = orr_cc_immed(ARM_CC_NE, tmp, tmp, 0x201);
    *ptr++ = tst_immed(tmp, 1);
    *ptr++ = orr_cc_immed(ARM_CC_NE, tmp, tmp, 0x202);
    *ptr++ = msr(tmp, 8);

    RA_FreeARMRegister(&ptr, tmp);

    return ptr;
}

uint32_t *EMIT_line5(uint32_t *ptr, uint16_t **m68k_ptr)
{
    uint16_t opcode = BE16((*m68k_ptr)[0]);
    (*m68k_ptr)++;

    if ((opcode & 0xf0c0) == 0x50c0)
    {
        /* Scc/TRAPcc/DBcc */
        if ((opcode & 0x38) == 0x08)
        {
            /* DBcc */
            uint8_t counter_reg = RA_MapM68kRegister(&ptr, opcode & 7);
            uint8_t m68k_condition = (opcode >> 8) & 0x0f;
            uint8_t arm_condition = M68K_ccTo_ARM[m68k_condition];
            uint32_t *branch_1 = NULL;
            uint32_t *branch_2 = NULL;

            (*m68k_ptr)++;

            /* Selcom case of DBT which does nothing */
            if (m68k_condition == M_CC_T)
            {
                ptr = EMIT_AdvancePC(ptr, 4);
            }
            else
            {
                ptr = EMIT_FlushPC(ptr);

                /* If condition was not false check the condition and eventually break the loop */
                if (m68k_condition != M_CC_F)
                {
                    ptr = EMIT_LoadARMCC(ptr, REG_SR);

                    /* Adjust PC, negated CC is loop condition, CC is loop break condition */
                    *ptr++ = add_cc_immed(arm_condition^1, REG_PC, REG_PC, 2);
                    *ptr++ = add_cc_immed(arm_condition, REG_PC, REG_PC, 4);

                    /* conditionally exit loop */
                    branch_1 = ptr;
                    *ptr++ = b_cc(arm_condition, 0);
                }

                /* Copy register to temporary, shift 16 bits left */
                uint8_t reg = RA_AllocARMRegister(&ptr);
                *ptr++ = mov_reg_shift(reg, counter_reg, 16);

                /* Substract 0x10000 from temporary, copare with 0xffff0000 */
                *ptr++ = sub_immed(reg, reg, 0x801);
                *ptr++ = cmn_immed(reg, 0x801);

                /* Bit shift result back and copy it into counter register */
                *ptr++ = lsr_immed(reg, reg, 16);
                *ptr++ = bfi(counter_reg, reg, 0, 16);
                RA_SetDirtyM68kRegister(&ptr, opcode & 7);

                /* If counter was 0xffff (temprary reg 0xffff0000) break the loop */
                *ptr++ = add_cc_immed(ARM_CC_EQ, REG_PC, REG_PC, 4);
                branch_2 = ptr;
                *ptr++ = b_cc(ARM_CC_EQ, 2);

                *ptr++ = add_immed(REG_PC, REG_PC, 2);
                /* Load PC-relative offset */
                *ptr++ = ldrsh_offset(REG_PC, reg, 0);

                *ptr++ = add_reg(REG_PC, REG_PC, reg, 0);
                RA_FreeARMRegister(&ptr, reg);
                if (branch_1) {
                    *branch_1 = INSN_TO_LE(INSN_TO_LE(*branch_1) + (int)(branch_2 - branch_1));
                    *ptr++ = (uint32_t)branch_1;
                }
                *ptr++ = (uint32_t)branch_2;
                *ptr++ = branch_1 == NULL ? 1 : 2;
                *ptr++ = INSN_TO_LE(0xfffffffe);

                RA_FreeARMRegister(&ptr, reg);
            }
        }
        else if ((opcode & 0x38) == 0x38)
        {
            /* TRAPcc */
        }
        else
        {
            /* Scc */
            uint8_t m68k_condition = (opcode >> 8) & 0x0f;
            uint8_t arm_condition = M68K_ccTo_ARM[m68k_condition];
            uint8_t ext_count = 0;

            if ((opcode & 0x38) == 0)
            {
                /* Scc Dx case */
                uint8_t dest = RA_MapM68kRegister(&ptr, opcode & 7);
                RA_SetDirtyM68kRegister(&ptr, opcode & 7);

                /* T condition always sets lowest 8 bis, F condition always clears them */
                if ((opcode & 0x0f00) == 0x0100)
                {
                    *ptr++ = bfc(dest, 0, 8);
                }
                else if ((opcode & 0x0f00) == 0x0000)
                {
                    *ptr++ = orr_immed(dest, dest, 0xff);
                }
                else
                {
                    /* Load m68k flags to arm flags and perform either bit clear or bit set */
                    ptr = EMIT_LoadARMCC(ptr, REG_SR);
                    *ptr++ = orr_cc_immed(arm_condition, dest, dest, 0xff);
                    *ptr++ = bfc_cc(arm_condition^1, dest, 0, 8);
                }
            }
            else
            {
                /* Load effective address */
                uint8_t dest = 0;
                ptr = EMIT_LoadFromEffectiveAddress(ptr, 0, &dest, opcode & 0x3f, *m68k_ptr, &ext_count);
                uint8_t tmp = RA_AllocARMRegister(&ptr);
                uint8_t mode = (opcode & 0x0038) >> 3;

                /* Fetch data into temporary register, perform add, store it back */
                if (mode == 4)
                {
                    *ptr++ = ldrb_offset_preindex(dest, tmp, (opcode & 7) == 7 ? -2 : -1);
                    RA_SetDirtyM68kRegister(&ptr, 8 + (opcode & 7));
                }
                else
                    *ptr++ = ldrb_offset(dest, tmp, 0);

                /* T condition always sets lowest 8 bis, F condition always clears them */
                if ((opcode & 0x0f00) == 0)
                {
                    *ptr++ = bfc(tmp, 0, 8);
                }
                else if ((opcode & 0x0f00) == 0x0100)
                {
                    *ptr++ = orr_immed(tmp, tmp, 0xff);
                }
                else
                {
                    /* Load m68k flags to arm flags and perform either bit clear or bit set */
                    ptr = EMIT_LoadARMCC(ptr, REG_SR);
                    *ptr++ = orr_cc_immed(arm_condition, tmp, tmp, 0xff);
                    *ptr++ = bfc_cc(arm_condition^1, tmp, 0, 8);
                }

                if (mode == 3)
                {
                    *ptr++ = strb_offset_postindex(dest, tmp, (opcode & 7) == 7 ? 2 : 1);
                    RA_SetDirtyM68kRegister(&ptr, 8 + (opcode & 7));
                }
                else
                    *ptr++ = strb_offset(dest, tmp, 0);

                RA_FreeARMRegister(&ptr, tmp);

            }

            ptr = EMIT_AdvancePC(ptr, 2 * (ext_count + 1));
        }

    }
    else if ((opcode & 0xf100) == 0x5100)
    {
        /* SUBQ */
    }
    else if ((opcode & 0xf100) == 0x5000)
    {
        /* ADDQ */
    }

    return ptr;
}
