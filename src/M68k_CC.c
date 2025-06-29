#include <stdint.h>
/*
    Copyright © 2020 Michal Schulz <michal.schulz@gmx.de>
    https://github.com/michalsc

    This Source Code Form is subject to the terms of the
    Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed
    with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "support.h"
#include "M68k.h"
#include "RegisterAllocator.h"

void EMIT_JumpOnCondition(struct TranslatorContext *ctx, uint8_t m68k_condition, uint32_t distance, uint32_t *jump_type)
{
	uint8_t cond_tmp = 0xff;
	uint8_t cc = RA_GetCC(ctx);

    switch (m68k_condition)
    {
        case M_CC_EQ:
            if (host_flags & SR_Z) {
                EMIT(ctx, b_cc(A64_CC_EQ, distance));
                if (jump_type) *jump_type = FIXUP_BCC;
            } else {
                EMIT(ctx, tbnz(cc, SRB_Z, distance));
                if (jump_type) *jump_type = FIXUP_TBZ;
            }
            break;

        case M_CC_NE:
            EMIT(ctx, 
                host_flags & SR_Z ? 
                    b_cc(A64_CC_NE, distance) : 
                    tbz(cc, SRB_Z, distance)
            );
            if (jump_type) *jump_type = host_flags & SR_Z ? FIXUP_BCC : FIXUP_TBZ;
            break;

        case M_CC_CS:
            EMIT(ctx, 
                host_flags & SR_C ? 
                    b_cc(A64_CC_CS, distance) : 
                    tbnz(cc, SRB_Calt, distance)
            );
            if (jump_type) *jump_type = host_flags & SR_C ? FIXUP_BCC : FIXUP_TBZ;
            break;

        case M_CC_CC:
            EMIT(ctx, 
                host_flags & SR_C ? 
                    b_cc(A64_CC_CC, distance) : 
                    tbz(cc, SRB_Calt, distance)
            );
            if (jump_type) *jump_type = host_flags & SR_C ? FIXUP_BCC : FIXUP_TBZ;
            break;

        case M_CC_PL:
            EMIT(ctx, 
                host_flags & SR_N ? 
                    b_cc(A64_CC_PL, distance) : 
                    tbz(cc, SRB_N, distance)
            );
            if (jump_type) *jump_type = host_flags & SR_N ? FIXUP_BCC : FIXUP_TBZ;
            break;

        case M_CC_MI:
            EMIT(ctx, 
                host_flags & SR_N ? 
                    b_cc(A64_CC_MI, distance) : 
                    tbnz(cc, SRB_N, distance)
            );
            if (jump_type) *jump_type = host_flags & SR_N ? FIXUP_BCC : FIXUP_TBZ;
            break;

        case M_CC_VS:
            EMIT(ctx, 
                host_flags & SR_V ? 
                    b_cc(A64_CC_VS, distance) : 
                    tbnz(cc, SRB_Valt, distance)
            );
            if (jump_type) *jump_type = host_flags & SR_V ? FIXUP_BCC : FIXUP_TBZ;
            break;

        case M_CC_VC:
            EMIT(ctx, 
                host_flags & SR_V ? 
                    b_cc(A64_CC_VC, distance) : 
                    tbz(cc, SRB_Valt, distance)
            );
            if (jump_type) *jump_type = host_flags & SR_V ? FIXUP_BCC : FIXUP_TBZ;
            break;

        case M_CC_LS:   /* C == 1 || Z == 1 */
            EMIT(ctx, 
                tst_immed(cc, 2, 31), // xnZCv
                b_cc(A64_CC_NE, distance)
            );
            if (jump_type) *jump_type = FIXUP_BCC;
            break;

        case M_CC_HI:   /* C == 0 && Z == 0 */
            EMIT(ctx, 
                tst_immed(cc, 2, 31), // xnZCv
                b_cc(A64_CC_EQ, distance)
            );
            if (jump_type) *jump_type = FIXUP_BCC;
            break;

        case M_CC_GE:   /* N ==V -> (N==0 && V==0) || (N==1 && V==1) */
            if ((host_flags & SR_NV) == SR_NV) {
                EMIT(ctx, b_cc(A64_CC_GE, distance));
            }
            else {
                cond_tmp = RA_AllocARMRegister(ctx);
                EMIT(ctx,
                    ror(cond_tmp, cc, 4),
                    set_nzcv(cond_tmp),
                    b_cc(A64_CC_GE, distance)
                );
            }
            if (jump_type) *jump_type = FIXUP_BCC;
            break;

        case M_CC_LT:
            if ((host_flags & SR_NV) == SR_NV) {
                EMIT(ctx, b_cc(A64_CC_LT, distance));
            }
            else {
                cond_tmp = RA_AllocARMRegister(ctx);
                EMIT(ctx,
                    ror(cond_tmp, cc, 4),
                    set_nzcv(cond_tmp),
                    b_cc(A64_CC_LT, distance)
                );
            }
            if (jump_type) *jump_type = FIXUP_BCC;
            break;

        case M_CC_GT:
            if ((host_flags & SR_NZV) == SR_NZV) {
                EMIT(ctx, b_cc(A64_CC_GT, distance));
            }
            else {
                cond_tmp = RA_AllocARMRegister(ctx);
                EMIT(ctx, 
                    ror(cond_tmp, cc, 4),
                    set_nzcv(cond_tmp),
                    b_cc(A64_CC_GT, distance)
                );
            }
            if (jump_type) *jump_type = FIXUP_BCC;
            break;

        case M_CC_LE:
            if ((host_flags & SR_NZV) == SR_NZV) {
                EMIT(ctx, b_cc(A64_CC_LE, distance));
            }
            else {
                cond_tmp = RA_AllocARMRegister(ctx);
                EMIT(ctx, 
                    ror(cond_tmp, cc, 4),
                    set_nzcv(cond_tmp),
                    b_cc(A64_CC_LE, distance)
                );
            }
            if (jump_type) *jump_type = FIXUP_BCC;
            break;

        default:
            kprintf("Default CC called! Can't be!\n");
            EMIT(ctx, udf(0x0bcc));
            break;
    }

    RA_FreeARMRegister(ctx, cond_tmp);
}

uint8_t EMIT_TestCondition(struct TranslatorContext *ctx, uint8_t m68k_condition)
{
    uint8_t success_condition = 0;
    uint8_t cond_tmp = 0xff;

    uint8_t cc = RA_GetCC(ctx);

    switch (m68k_condition)
    {
        case M_CC_EQ:
            if (host_flags & SR_Z) {
                success_condition = A64_CC_EQ;
            } else {
                EMIT(ctx, tst_immed(cc, 1, 31 & (32 - SRB_Z)));
                success_condition = A64_CC_NE;
            }
            break;

        case M_CC_NE:
            if (host_flags & SR_Z) {
                success_condition = A64_CC_NE;
            } else {
                EMIT(ctx, tst_immed(cc, 1, 31 & (32 - SRB_Z)));
                success_condition = A64_CC_EQ;
            }
            break;

        case M_CC_CS:
            if (host_flags & SR_C) {
                success_condition = A64_CC_CS;
            } else {
                EMIT(ctx, tst_immed(cc, 1, 31 & (32 - SRB_Calt)));
                success_condition = A64_CC_NE;
            }
            break;

        case M_CC_CC:
            if (host_flags & SR_C) {
                success_condition = A64_CC_CC;
            } else {
                EMIT(ctx, tst_immed(cc, 1, 31 & (32 - SRB_Calt)));
                success_condition = A64_CC_EQ;
            }
            break;

        case M_CC_PL:
            if (host_flags & SR_N) {
                success_condition = A64_CC_PL;
            } else {
                EMIT(ctx, tst_immed(cc, 1, 31 & (32 - SRB_N)));
                success_condition = A64_CC_EQ;
            }
            break;

        case M_CC_MI:
        if (host_flags & SR_N) {
                success_condition = A64_CC_MI;
            } else {
                EMIT(ctx, tst_immed(cc, 1, 31 & (32 - SRB_N)));
                success_condition = A64_CC_NE;
            }
            break;

        case M_CC_VS:
            if (host_flags & SR_V) {
                success_condition = A64_CC_VS;
            } else {
                EMIT(ctx, tst_immed(cc, 1, 31 & (32 - SRB_Valt)));
                success_condition = A64_CC_NE;
            }
            break;

        case M_CC_VC:
            if (host_flags & SR_V) {
                success_condition = A64_CC_VC;
            } else {
                EMIT(ctx, tst_immed(cc, 1, 31 & (32 - SRB_Valt)));
                success_condition = A64_CC_EQ;
            }
            break;

        case M_CC_LS: /* C == 1 || Z == 1 */
            EMIT(ctx, tst_immed(cc, 2, 31)); // xnZCv
            success_condition = A64_CC_NE;
            break;

        case M_CC_HI: /* C == 0 && Z == 0 */
            EMIT(ctx, tst_immed(cc, 2, 31)); // xnZCv
            success_condition = A64_CC_EQ;
            break;

        case M_CC_GE: /* N ==V -> (N==0 && V==0) || (N==1 && V==1) */
            if ((host_flags & SR_NV) != SR_NV)
            {
                cond_tmp = RA_AllocARMRegister(ctx);
                EMIT(ctx, 
                    ror(cond_tmp, cc, 4),
                    set_nzcv(cond_tmp)
                );
            }
            success_condition = A64_CC_GE;
            break;

        case M_CC_LT:
            if ((host_flags & SR_NV) != SR_NV)
            {
                cond_tmp = RA_AllocARMRegister(ctx);
                EMIT(ctx, 
                    ror(cond_tmp, cc, 4),
                    set_nzcv(cond_tmp)
                );
            }
            success_condition = A64_CC_LT;
            break;

        case M_CC_GT:
            if ((host_flags & SR_NZV) != SR_NZV)
            {
                cond_tmp = RA_AllocARMRegister(ctx);
                EMIT(ctx, 
                    ror(cond_tmp, cc, 4),
                    set_nzcv(cond_tmp)
                );
            }
            success_condition = A64_CC_GT;
            break;

        case M_CC_LE:
            if ((host_flags & SR_NZV) != SR_NZV)
            {
                cond_tmp = RA_AllocARMRegister(ctx);
                EMIT(ctx, 
                    ror(cond_tmp, cc, 4),
                    set_nzcv(cond_tmp)
                );
            }
            success_condition = A64_CC_LE;
            break;

        default:
            kprintf("Default CC called! Can't be!\n");
            EMIT(ctx, udf(0x0bcc));
            break;
    }

    RA_FreeARMRegister(ctx, cond_tmp);

    return success_condition;
}

uint8_t EMIT_TestFPUCondition(struct TranslatorContext *ctx, uint8_t predicate)
{
    uint8_t success_condition = 0;
    uint8_t tmp_cc = 0xff;
    uint8_t fpsr = RA_GetFPSR(ctx);

    /* Test predicate with masked signalling bit, operations are the same */
    switch (predicate & 0x0f)
    {
        case F_CC_EQ: /* Z == 0 */
            EMIT(ctx, tst_immed(fpsr, 1, 31 & (32 - FPSRB_Z)));
            success_condition = A64_CC_NE;
            break;

        case F_CC_NE: /* Z == 1 */
            EMIT(ctx, tst_immed(fpsr, 1, 31 & (32 - FPSRB_Z)));
            success_condition = A64_CC_EQ;
            break;

        case F_CC_OGT: /* NAN == 0 && Z == 0 && N == 0 */
            tmp_cc = RA_AllocARMRegister(ctx);
            EMIT(ctx, 
                mov_immed_u16(tmp_cc, (FPSR_Z | FPSR_N | FPSR_NAN) >> 16, 1),
                tst_reg(fpsr, tmp_cc, LSL, 0)
            );
            success_condition = ARM_CC_EQ;
            break;

        case F_CC_ULE: /* NAN == 1 || Z == 1 || N == 1 */
            tmp_cc = RA_AllocARMRegister(ctx);
            EMIT(ctx, 
                mov_immed_u16(tmp_cc, (FPSR_Z | FPSR_N | FPSR_NAN) >> 16, 1),
                tst_reg(fpsr, tmp_cc, LSL, 0)
            );
            success_condition = ARM_CC_NE;
            break;

        case F_CC_OGE: // Z == 1 || (N == 0 && NAN == 0)
            tmp_cc = RA_AllocARMRegister(ctx);
            EMIT(ctx, 
                tst_immed(fpsr, 1, 31 & (32 - FPSRB_Z)),
                b_cc(A64_CC_NE, 4),
                orr_reg(tmp_cc, fpsr, fpsr, LSL, 3), // N | NAN -> N (== 0 only if N=0 && NAN=0)
                eor_immed(tmp_cc, tmp_cc, 1, 31 & (32 - FPSRB_N)), // !N -> N
                tst_immed(tmp_cc, 1, 31 & (32 - FPSRB_N))
            );
            success_condition = A64_CC_NE;
            break;

        case F_CC_ULT: // NAN == 1 || (N == 1 && Z == 0)
            tmp_cc = RA_AllocARMRegister(ctx);
            EMIT(ctx, 
                tst_immed(fpsr, 1, 31 & (32 - FPSRB_NAN)),
                b_cc(A64_CC_NE, 4),
                eor_immed(tmp_cc, fpsr, 1, 31 & (32 - FPSRB_Z)), // Invert Z
                and_reg(tmp_cc, tmp_cc, tmp_cc, LSL, 1), // !Z & N -> N
                tst_immed(tmp_cc, 1, 31 & (32 - FPSRB_N))
            );
            success_condition = A64_CC_NE;
            break;

        case F_CC_OLT: // N == 1 && (NAN == 0 && Z == 0)
            tmp_cc = RA_AllocARMRegister(ctx);
            EMIT(ctx, 
                bic_immed(tmp_cc, fpsr, 1, 31 & (32 - FPSRB_I)),
                orr_reg(tmp_cc, tmp_cc, tmp_cc, LSL, 2), // NAN | Z -> Z
                eor_immed(tmp_cc, tmp_cc, 1, 31 & (32 - FPSRB_N)), // Invert N
                tst_immed(tmp_cc, 2, 31 & (32 - FPSRB_Z)) // Test N==0 && Z == 0
            );
            success_condition = A64_CC_EQ;
            break;

        case F_CC_UGE: // NAN == 1 || (Z == 1 || N == 0)
            tmp_cc = RA_AllocARMRegister(ctx);
            EMIT(ctx, 
                eor_immed(tmp_cc, fpsr, 1, 31 & (32 - FPSRB_N)),
                bic_immed(tmp_cc, tmp_cc, 1, 31 & (32 - FPSRB_I)),
                tst_immed(tmp_cc, 4, 31 & (32 - FPSRB_NAN))
            );
            success_condition = A64_CC_NE;
            break;

        case F_CC_OLE: // Z == 1 || (N == 1 && NAN == 0)
            tmp_cc = RA_AllocARMRegister(ctx);
            EMIT(ctx, 
                tst_immed(fpsr, 1, 31 & (32 - FPSRB_Z)),
                b_cc(A64_CC_NE, 4),
                eor_immed(tmp_cc, fpsr, 1, 31 & (32 - FPSRB_NAN)), // Invert NAN
                and_reg(tmp_cc, tmp_cc, tmp_cc, LSL, 3),   // !NAN & N -> N
                tst_immed(tmp_cc, 1, 31 & (32 - FPSRB_N))
            );
            success_condition = A64_CC_NE;
            break;

        case F_CC_UGT: // NAN == 1 || (N == 0 && Z == 0)
            tmp_cc = RA_AllocARMRegister(ctx);
            EMIT(ctx, 
                tst_immed(fpsr, 1, 31 & (32 - FPSRB_NAN)),
                b_cc(A64_CC_NE, 4),
                orr_reg(tmp_cc, fpsr, fpsr, LSR, 1),
                mvn_reg(tmp_cc, tmp_cc, LSL, 0), //eor_immed(tmp_cc, tmp_cc, 1, 31 & (32 - FPSRB_Z));
                tst_immed(tmp_cc, 1, 31 & (32 - FPSRB_Z))
            );
            success_condition = A64_CC_NE;
            break;

        case F_CC_OGL:
            tmp_cc = RA_AllocARMRegister(ctx);
            EMIT(ctx, 
                mov_immed_u16(tmp_cc, (FPSR_Z | FPSR_NAN) >> 16, 1),
                tst_reg(fpsr, tmp_cc, LSL, 0)
            );
            success_condition = A64_CC_EQ;
            break;

        case F_CC_UEQ:
            tmp_cc = RA_AllocARMRegister(ctx);
            EMIT(ctx, 
                mov_immed_u16(tmp_cc, (FPSR_Z | FPSR_NAN) >> 16, 1),
                tst_reg(fpsr, tmp_cc, LSL, 0)
            );
            success_condition = A64_CC_NE;
            break;

        case F_CC_OR:
            EMIT(ctx, tst_immed(fpsr, 1, 31 & (32 - FPSRB_NAN)));
            success_condition = A64_CC_EQ;
            break;

        case F_CC_UN:
            EMIT(ctx, tst_immed(fpsr, 1, 31 & (32 - FPSRB_NAN)));
            success_condition = A64_CC_NE;
            break;

        case F_CC_F:    // This is NOP - handled one "if" before
            success_condition = A64_CC_NV;
            break;

        case F_CC_T:    // Unconditional branch to target
            success_condition = A64_CC_AL;
            break;
    }

    RA_FreeARMRegister(ctx, tmp_cc);

    return success_condition;
}
