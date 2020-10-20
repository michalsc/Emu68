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

#ifdef __aarch64__
    uint8_t cc = RA_GetCC(&ptr);
#else
    M68K_GetCC(&ptr);
#endif

    switch (m68k_condition)
    {
        case M_CC_EQ:
#ifdef __aarch64__
            *ptr++ = tst_immed(cc, 1, 31 & (32 - SRB_Z));
            success_condition = A64_CC_NE;
#else
            *ptr++ = tst_immed(REG_SR, SR_Z);
            success_condition = ARM_CC_NE;
#endif
            break;

        case M_CC_NE:
#ifdef __aarch64__
            *ptr++ = tst_immed(cc, 1, 31 & (32 - SRB_Z));
            success_condition = A64_CC_EQ;
#else
            *ptr++ = tst_immed(REG_SR, SR_Z);
            success_condition = ARM_CC_EQ;
#endif
            break;

        case M_CC_CS:
#ifdef __aarch64__
            *ptr++ = tst_immed(cc, 1, 31 & (32 - SRB_C));
            success_condition = A64_CC_NE;
#else
            *ptr++ = tst_immed(REG_SR, SR_C);
            success_condition = ARM_CC_NE;
#endif
            break;

        case M_CC_CC:
#ifdef __aarch64__
            *ptr++ = tst_immed(cc, 1, 31 & (32 - SRB_C));
            success_condition = A64_CC_EQ;
#else
            *ptr++ = tst_immed(REG_SR, SR_C);
            success_condition = ARM_CC_EQ;
#endif
            break;

        case M_CC_PL:
#ifdef __aarch64__
            *ptr++ = tst_immed(cc, 1, 31 & (32 - SRB_N));
            success_condition = A64_CC_EQ;
#else
            *ptr++ = tst_immed(REG_SR, SR_N);
            success_condition = ARM_CC_EQ;
#endif
            break;

        case M_CC_MI:
#ifdef __aarch64__
            *ptr++ = tst_immed(cc, 1, 31 & (32 - SRB_N));
            success_condition = A64_CC_NE;
#else
            *ptr++ = tst_immed(REG_SR, SR_N);
            success_condition = ARM_CC_NE;
#endif
            break;

        case M_CC_VS:
#ifdef __aarch64__
            *ptr++ = tst_immed(cc, 1, 31 & (32 - SRB_V));
            success_condition = A64_CC_NE;
#else
            *ptr++ = tst_immed(REG_SR, SR_V);
            success_condition = ARM_CC_NE;
#endif
            break;

        case M_CC_VC:
#ifdef __aarch64__
            *ptr++ = tst_immed(cc, 1, 31 & (32 - SRB_V));
            success_condition = A64_CC_EQ;
#else
            *ptr++ = tst_immed(REG_SR, SR_V);
            success_condition = ARM_CC_EQ;
#endif
            break;

        case M_CC_LS:   /* C == 1 || Z == 1 */
#ifdef __aarch64__
            *ptr++ = tst_immed(cc, 1, 31 & (32 - SRB_Z));
            *ptr++ = b_cc(A64_CC_NE, 2);
            *ptr++ = tst_immed(cc, 1, 31 & (32 - SRB_C));
            success_condition = A64_CC_NE;
#else
            *ptr++ = tst_immed(REG_SR, SR_Z | SR_C);
            success_condition = ARM_CC_NE;
#endif
            break;

        case M_CC_HI:   /* C == 0 && Z == 0 */
#ifdef __aarch64__
            *ptr++ = tst_immed(cc, 1, 31 & (32 - SRB_Z));
            *ptr++ = b_cc(A64_CC_NE, 2);
            *ptr++ = tst_immed(cc, 1, 31 & (32 - SRB_C));
            success_condition = A64_CC_EQ;
#else
            *ptr++ = tst_immed(REG_SR, SR_Z);
            *ptr++ = tst_cc_immed(ARM_CC_EQ, REG_SR, SR_C);
            success_condition = ARM_CC_EQ;
#endif
            break;

        case M_CC_GE:   /* N ==V -> (N==0 && V==0) || (N==1 && V==1) */
#ifdef __aarch64__
            cond_tmp = RA_AllocARMRegister(&ptr);
            *ptr++ = eor_reg(cond_tmp, cc, cc, LSL, (SRB_N - SRB_V)); /* Calculate N ^ V. If both are equal, it returns 0 */
            *ptr++ = tst_immed(cond_tmp, 1, 31 & (32 - SRB_N));
            success_condition = A64_CC_EQ;
#else
            cond_tmp = RA_AllocARMRegister(&ptr);
            *ptr++ = ands_immed(cond_tmp, REG_SR, SR_N | SR_V); /* Extract N and V, set ARM_CC_EQ if both clear */
            *ptr++ = teq_cc_immed(ARM_CC_NE, cond_tmp, SR_N | SR_V); /* If N and V != 0, perform equality check */
            success_condition = ARM_CC_EQ;
#endif
            break;

        case M_CC_LT:
#ifdef __aarch64__
            cond_tmp = RA_AllocARMRegister(&ptr);
            *ptr++ = eor_reg(cond_tmp, cc, cc, LSL, (SRB_N - SRB_V)); /* Calculate N ^ V. If both are equal, it returns 0 */
            *ptr++ = tst_immed(cond_tmp, 1, 31 & (32 - SRB_N));
            success_condition = A64_CC_NE;
#else
            cond_tmp = RA_AllocARMRegister(&ptr);
            *ptr++ = and_immed(cond_tmp, REG_SR, SR_N | SR_V); /* Extract N and V */
            *ptr++ = teq_immed(cond_tmp, SR_N); /* Check N==1 && V==0 */
            *ptr++ = teq_cc_immed(ARM_CC_NE, cond_tmp, SR_V); /* Check N==0 && V==1 */
            success_condition = ARM_CC_EQ;
#endif
            break;

        case M_CC_GT:
#ifdef __aarch64__
            cond_tmp = RA_AllocARMRegister(&ptr);
            *ptr++ = tst_immed(cc, 1, 31 & (32 - SRB_Z));
            *ptr++ = b_cc(A64_CC_NE, 3);
            *ptr++ = eor_reg(cond_tmp, cc, cc, LSL, (SRB_N - SRB_V)); /* Calculate N ^ V. If both are equal, it returns 0 */
            *ptr++ = tst_immed(cond_tmp, 1, 31 & (32 - SRB_N));
            success_condition = A64_CC_EQ;
#else
            cond_tmp = RA_AllocARMRegister(&ptr);
            *ptr++ = ands_immed(cond_tmp, REG_SR, SR_N | SR_V | SR_Z); /* Extract Z, N and V, set ARM_CC_EQ if both clear */
            *ptr++ = teq_cc_immed(ARM_CC_NE, cond_tmp, SR_N | SR_V); /* If above fails, check if Z==0, N==1 and V==1 */
            success_condition = ARM_CC_EQ;
#endif
            break;

        case M_CC_LE:
#ifdef __aarch64__
            cond_tmp = RA_AllocARMRegister(&ptr);
            *ptr++ = tst_immed(cc, 1, 31 & (32 - SRB_Z));
            *ptr++ = b_cc(A64_CC_NE, 3);
            *ptr++ = eor_reg(cond_tmp, cc, cc, LSL, (SRB_N - SRB_V)); /* Calculate N ^ V. If both are equal, it returns 0 */
            *ptr++ = tst_immed(cond_tmp, 1, 31 & (32 - SRB_N));
            success_condition = A64_CC_NE;
#else
            cond_tmp = RA_AllocARMRegister(&ptr);
            *ptr++ = and_immed(cond_tmp, REG_SR, SR_N | SR_V); /* Extract N and V, set ARM_CC_EQ if both clear */
            *ptr++ = teq_immed(cond_tmp, SR_N); /* Check N==1 && V==0 */
            *ptr++ = teq_cc_immed(ARM_CC_NE, cond_tmp, SR_V); /* Check N==0 && V==1 */
            *ptr++ = and_cc_immed(ARM_CC_NE, cond_tmp, REG_SR, SR_Z); /* If failed, extract Z flag */
            *ptr++ = teq_cc_immed(ARM_CC_NE, cond_tmp, SR_Z); /* Check if Z is set */
            success_condition = ARM_CC_EQ;
#endif
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
