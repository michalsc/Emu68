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

uint8_t EMIT_TestCondition(uint32_t **pptr, uint8_t m68k_condition)
{
    uint32_t *ptr = *pptr;
    uint8_t success_condition = 0;
    uint8_t cond_tmp = 0xff;

    uint8_t cc = RA_GetCC(&ptr);

    switch (m68k_condition)
    {
        case M_CC_EQ:
            *ptr++ = tst_immed(cc, 1, 31 & (32 - SRB_Z));
            success_condition = A64_CC_NE;
            break;

        case M_CC_NE:
            *ptr++ = tst_immed(cc, 1, 31 & (32 - SRB_Z));
            success_condition = A64_CC_EQ;
            break;

        case M_CC_CS:
            *ptr++ = tst_immed(cc, 1, 31 & (32 - SRB_C));
            success_condition = A64_CC_NE;
            break;

        case M_CC_CC:
            *ptr++ = tst_immed(cc, 1, 31 & (32 - SRB_C));
            success_condition = A64_CC_EQ;
            break;

        case M_CC_PL:
            *ptr++ = tst_immed(cc, 1, 31 & (32 - SRB_N));
            success_condition = A64_CC_EQ;
            break;

        case M_CC_MI:
            *ptr++ = tst_immed(cc, 1, 31 & (32 - SRB_N));
            success_condition = A64_CC_NE;
            break;

        case M_CC_VS:
            *ptr++ = tst_immed(cc, 1, 31 & (32 - SRB_V));
            success_condition = A64_CC_NE;
            break;

        case M_CC_VC:
            *ptr++ = tst_immed(cc, 1, 31 & (32 - SRB_V));
            success_condition = A64_CC_EQ;
            break;

        case M_CC_LS:   /* C == 1 || Z == 1 */
            cond_tmp = RA_AllocARMRegister(&ptr);
            *ptr++ = mov_immed_u8(cond_tmp, SR_Z | SR_C);
            *ptr++ = tst_reg(cc, cond_tmp, LSL, 0);
/*
            *ptr++ = tst_immed(cc, 1, 31 & (32 - SRB_Z));
            *ptr++ = b_cc(A64_CC_NE, 2);
            *ptr++ = tst_immed(cc, 1, 31 & (32 - SRB_C));
*/
            success_condition = A64_CC_NE;
            break;

        case M_CC_HI:   /* C == 0 && Z == 0 */
            cond_tmp = RA_AllocARMRegister(&ptr);
            *ptr++ = mov_immed_u8(cond_tmp, SR_Z | SR_C);
            *ptr++ = tst_reg(cc, cond_tmp, LSL, 0);
/*
            *ptr++ = tst_immed(cc, 1, 31 & (32 - SRB_Z));
            *ptr++ = b_cc(A64_CC_NE, 2);
            *ptr++ = tst_immed(cc, 1, 31 & (32 - SRB_C));
*/
            success_condition = A64_CC_EQ;
            break;

        case M_CC_GE:   /* N ==V -> (N==0 && V==0) || (N==1 && V==1) */
            cond_tmp = RA_AllocARMRegister(&ptr);
            *ptr++ = eor_reg(cond_tmp, cc, cc, LSL, (SRB_N - SRB_V)); /* Calculate N ^ V. If both are equal, it returns 0 */
            *ptr++ = tst_immed(cond_tmp, 1, 31 & (32 - SRB_N));
            success_condition = A64_CC_EQ;
            break;

        case M_CC_LT:
            cond_tmp = RA_AllocARMRegister(&ptr);
            *ptr++ = eor_reg(cond_tmp, cc, cc, LSL, (SRB_N - SRB_V)); /* Calculate N ^ V. If both are equal, it returns 0 */
            *ptr++ = tst_immed(cond_tmp, 1, 31 & (32 - SRB_N));
            success_condition = A64_CC_NE;
            break;

        case M_CC_GT:
            cond_tmp = RA_AllocARMRegister(&ptr);
            *ptr++ = tst_immed(cc, 1, 31 & (32 - SRB_Z));
            *ptr++ = b_cc(A64_CC_NE, 3);
            *ptr++ = eor_reg(cond_tmp, cc, cc, LSL, (SRB_N - SRB_V)); /* Calculate N ^ V. If both are equal, it returns 0 */
            *ptr++ = tst_immed(cond_tmp, 1, 31 & (32 - SRB_N));
            success_condition = A64_CC_EQ;
            break;

        case M_CC_LE:
            cond_tmp = RA_AllocARMRegister(&ptr);
            *ptr++ = tst_immed(cc, 1, 31 & (32 - SRB_Z));
            *ptr++ = b_cc(A64_CC_NE, 3);
            *ptr++ = eor_reg(cond_tmp, cc, cc, LSL, (SRB_N - SRB_V)); /* Calculate N ^ V. If both are equal, it returns 0 */
            *ptr++ = tst_immed(cond_tmp, 1, 31 & (32 - SRB_N));
            success_condition = A64_CC_NE;
            break;

        default:
            kprintf("Default CC called! Can't be!\n");
            *ptr++ = udf(0x0bcc);
            break;
    }

    RA_FreeARMRegister(&ptr, cond_tmp);

    *pptr = ptr;
    return success_condition;
}

uint8_t EMIT_TestFPUCondition(uint32_t **pptr, uint8_t predicate)
{
    uint32_t *ptr = *pptr;
    uint8_t success_condition = 0;
    uint8_t tmp_cc = 0xff;
    uint8_t fpsr = RA_GetFPSR(&ptr);

	/* Test predicate with masked signalling bit, operations are the same */
	switch (predicate & 0x0f)
	{
		case F_CC_EQ: /* Z == 0 */
			*ptr++ = tst_immed(fpsr, 1, 31 & (32 - FPSRB_Z));
			success_condition = A64_CC_NE;
			break;

		case F_CC_NE: /* Z == 1 */
			*ptr++ = tst_immed(fpsr, 1, 31 & (32 - FPSRB_Z));
			success_condition = A64_CC_EQ;
			break;

		case F_CC_OGT: /* NAN == 0 && Z == 0 && N == 0 */
			tmp_cc = RA_AllocARMRegister(&ptr);
			*ptr++ = mov_immed_u16(tmp_cc, (FPSR_Z | FPSR_N | FPSR_NAN) >> 16, 1);
			*ptr++ = tst_reg(fpsr, tmp_cc, LSL, 0);
			success_condition = ARM_CC_EQ;
			break;

		case F_CC_ULE: /* NAN == 1 || Z == 1 || N == 1 */
			tmp_cc = RA_AllocARMRegister(&ptr);
			*ptr++ = mov_immed_u16(tmp_cc, (FPSR_Z | FPSR_N | FPSR_NAN) >> 16, 1);
			*ptr++ = tst_reg(fpsr, tmp_cc, LSL, 0);
			success_condition = ARM_CC_NE;
			break;

		case F_CC_OGE: // Z == 1 || (N == 0 && NAN == 0)
			tmp_cc = RA_AllocARMRegister(&ptr);
			*ptr++ = tst_immed(fpsr, 1, 31 & (32 - FPSRB_Z));
			*ptr++ = b_cc(A64_CC_NE, 4);
			*ptr++ = orr_reg(tmp_cc, fpsr, fpsr, LSL, 3); // N | NAN -> N (== 0 only if N=0 && NAN=0)
			*ptr++ = eor_immed(tmp_cc, tmp_cc, 1, 31 & (32 - FPSRB_N)); // !N -> N
			*ptr++ = tst_immed(tmp_cc, 1, 31 & (32 - FPSRB_N));
			success_condition = A64_CC_NE;
			break;

		case F_CC_ULT: // NAN == 1 || (N == 1 && Z == 0)
			tmp_cc = RA_AllocARMRegister(&ptr);
			*ptr++ = tst_immed(fpsr, 1, 31 & (32 - FPSRB_NAN));
			*ptr++ = b_cc(A64_CC_NE, 4);
			*ptr++ = eor_immed(tmp_cc, fpsr, 1, 31 & (32 - FPSRB_Z)); // Invert Z
			*ptr++ = and_reg(tmp_cc, tmp_cc, tmp_cc, LSL, 1); // !Z & N -> N
			*ptr++ = tst_immed(tmp_cc, 1, 31 & (32 - FPSRB_N));
			success_condition = A64_CC_NE;
			break;

		case F_CC_OLT: // N == 1 && (NAN == 0 && Z == 0)
			tmp_cc = RA_AllocARMRegister(&ptr);
			*ptr++ = bic_immed(tmp_cc, fpsr, 1, 31 & (32 - FPSRB_I));
			*ptr++ = orr_reg(tmp_cc, tmp_cc, tmp_cc, LSL, 2); // NAN | Z -> Z
			*ptr++ = eor_immed(tmp_cc, tmp_cc, 1, 31 & (32 - FPSRB_N)); // Invert N
			*ptr++ = tst_immed(tmp_cc, 2, 31 & (32 - FPSRB_Z)); // Test N==0 && Z == 0
			success_condition = A64_CC_EQ;
			break;

		case F_CC_UGE: // NAN == 1 || (Z == 1 || N == 0)
			tmp_cc = RA_AllocARMRegister(&ptr);
			*ptr++ = eor_immed(tmp_cc, fpsr, 1, 31 & (32 - FPSRB_N));
			*ptr++ = bic_immed(tmp_cc, tmp_cc, 1, 31 & (32 - FPSRB_I));
			*ptr++ = tst_immed(tmp_cc, 4, 31 & (32 - FPSRB_NAN));
			success_condition = A64_CC_NE;
			break;

		case F_CC_OLE: // Z == 1 || (N == 1 && NAN == 0)
			tmp_cc = RA_AllocARMRegister(&ptr);
			*ptr++ = tst_immed(fpsr, 1, 31 & (32 - FPSRB_Z));
			*ptr++ = b_cc(A64_CC_NE, 4);
			*ptr++ = eor_immed(tmp_cc, fpsr, 1, 31 & (32 - FPSRB_NAN)); // Invert NAN
			*ptr++ = and_reg(tmp_cc, tmp_cc, tmp_cc, LSL, 3);   // !NAN & N -> N
			*ptr++ = tst_immed(tmp_cc, 1, 31 & (32 - FPSRB_N));
			success_condition = A64_CC_NE;
			break;

		case F_CC_UGT: // NAN == 1 || (N == 0 && Z == 0)
			tmp_cc = RA_AllocARMRegister(&ptr);
			*ptr++ = tst_immed(fpsr, 1, 31 & (32 - FPSRB_NAN));
			*ptr++ = b_cc(A64_CC_NE, 4);
			*ptr++ = orr_reg(tmp_cc, fpsr, fpsr, LSR, 1);
			*ptr++ = mvn_reg(tmp_cc, tmp_cc, LSL, 0); //eor_immed(tmp_cc, tmp_cc, 1, 31 & (32 - FPSRB_Z));
			*ptr++ = tst_immed(tmp_cc, 1, 31 & (32 - FPSRB_Z));
			success_condition = A64_CC_NE;
			break;

		case F_CC_OGL:
			tmp_cc = RA_AllocARMRegister(&ptr);
			*ptr++ = mov_immed_u16(tmp_cc, (FPSR_Z | FPSR_NAN) >> 16, 1);
			*ptr++ = tst_reg(fpsr, tmp_cc, LSL, 0);
			success_condition = A64_CC_EQ;
			break;

		case F_CC_UEQ:
			tmp_cc = RA_AllocARMRegister(&ptr);
			*ptr++ = mov_immed_u16(tmp_cc, (FPSR_Z | FPSR_NAN) >> 16, 1);
			*ptr++ = tst_reg(fpsr, tmp_cc, LSL, 0);
			success_condition = A64_CC_NE;
			break;

		case F_CC_OR:
			*ptr++ = tst_immed(fpsr, 1, 31 & (32 - FPSRB_NAN));
			success_condition = A64_CC_EQ;
			break;

		case F_CC_UN:
			*ptr++ = tst_immed(fpsr, 1, 31 & (32 - FPSRB_NAN));
			success_condition = A64_CC_NE;
			break;

		case F_CC_F:    // This is NOP - handled one "if" before
			success_condition = A64_CC_NV;
			break;

		case F_CC_T:    // Unconditional branch to target
			success_condition = A64_CC_AL;
			break;
	}
	RA_FreeARMRegister(&ptr, tmp_cc);

    *pptr = ptr;
    return success_condition;
}
