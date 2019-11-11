/*
    Copyright © 2019 Michal Schulz <michal.schulz@gmx.de>
    https://github.com/michalsc

    This Source Code Form is subject to the terms of the
    Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed
    with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <stdint.h>
#include <stdlib.h>

#include <math.h>
#include <stdio.h>
#include "ARM.h"
#include "M68k.h"
#include "RegisterAllocator.h"

#define USE_POLY_21 1

enum {
    C_PI = 0,
    C_PI_2,
    C_PI_4,
    C_1_PI,
    C_2_PI,
    C_2_SQRTPI,
    C_SQRT2,
    C_SQRT1_2,
    C_0_5,
    C_1_5,
    C_LOG10_2 = 0x0b,
    C_E,
    C_LOG2E,
    C_LOG10E,
    C_ZERO,
    C_SIN_COEFF = 0x10,  /* 21-poly for sine approximation - error margin within double precision */
    C_COS_COEFF = 0x20,  /* 20-poly for cosine approximation -error margin within double precision */

    C_LN2 = 0x30,
    C_LN10,
    C_10P0,
    C_10P1,
    C_10P2,
    C_10P4,
    C_10P8,
    C_10P16,
    C_10P32,
    C_10P64,
    C_10P128,
    C_10P256,
    C_10P512,
    C_10P1024,
    C_10P2048,
    C_10P4096
};

long double constants[128] = {
    [C_PI] =        M_PI,               /* Official */
    [C_PI_2] =      M_PI_2,
    [C_PI_4] =      M_PI_4,
    [C_1_PI] =      M_1_PI,
    [C_2_PI] =      M_2_PI,
    [C_2_SQRTPI] =  M_2_SQRTPI,
    [C_SQRT2] =     M_SQRT2,
    [C_SQRT1_2] =   M_SQRT1_2,
    [C_0_5] =       0.5,
    [C_1_5] =       1.5,
    [C_LOG10_2] =   0.301029995663981195214, /* Official - Log10(2) */
    [C_E] =         M_E,                /* Official */
    [C_LOG2E] =     M_LOG2E,            /* Official */
    [C_LOG10E] =    M_LOG10E,           /* Official */
    [C_ZERO] =      0.0,                /* Official */

    /* Polynom coefficients for sin(x) */

    [C_SIN_COEFF] = -2.11100178050346585936E-5,
                    4.65963708473294521719E-4,
                    -7.37035513524020578156E-3,
                    8.21458769726032277098E-2,
                    -5.99264528627362954518E-1,
                    2.55016403985097679243,
                    -5.16771278004952168888,
                    3.14159265358979102647,

    [C_COS_COEFF] = 4.15383875943350535407E-6,
                    -1.04570624685965272291E-4,
                    1.92955784205552168426E-3,
                    -2.58068890507489103003E-2,
                    2.35330630164104256943E-1,
                    -1.33526276884550367708,
                    4.05871212641655666324,
                    -4.93480220054467742126,
                    9.99999999999999997244E-1,

#if 0
#if USE_POLY_21_
    [C_SIN_COEFF] = 1.71343967861184034706E-20,
                    -8.15103676569049647059E-18,
                    2.81031414820239505995E-15,
                    -7.64704549064188225994E-13,
                    1.60590358573163197959E-10,
                    -2.50521080326538396825E-8,
                    2.75573192139840283187E-6,
                    -1.98412698410970543592E-4,
                    8.33333333333168248238E-3,
                    -1.66666666666665944649E-1,
                    9.99999999999999907365E-1,

    [C_COS_COEFF] = 3.57574533982325995917E-19,
                    -1.54745332630529127915E-16,
                    4.77724279405039943569E-14,
                    -1.14705306393171244460E-11,
                    2.08767436959979435384E-9,
                    -2.75573186964419045529E-7,
                    2.48015872885505928507E-5,
                    -1.38888888887012801533E-3,
                    4.16666666666528513249E-2,
                    -4.99999999999996043059E-1,
                    9.99999999999999813877E-1,

#else
    [C_SIN_COEFF] = -2.05342856289746600727E-08,
                    2.70405218307799040084E-06,
                    -1.98125763417806681909E-04,
                    8.33255814755188010464E-03,
                    -1.66665772196961623983E-01,
                    9.99999707044156546685E-01,

    [C_COS_COEFF] = -2.21941782786353727022E-07,
                    2.42532401381033027481E-05,
                    -1.38627507062573673756E-03,
                    4.16610337354021107429E-02,
                    -4.99995582499065048420E-01,
                    9.99999443739537210853E-01,
#endif
#endif

    /* Polynom coefficients for cos(x) */

    [C_LN2] =       M_LN2,              /* Official */
    [C_LN10] =      M_LN10,             /* Official */
    [C_10P0] =      1.0,                /* Official */
    [C_10P1] =      1E1,                /* Official */
    [C_10P2] =      1E2,                /* Official */
    [C_10P4] =      1E4,                /* Official */
    [C_10P8] =      1E8,                /* Official */
    [C_10P16] =     1E16,               /* Official */
    [C_10P32] =     1E32,               /* Official */
    [C_10P64] =     1E64,               /* Official */
    [C_10P128] =    1E128,              /* Official */
    [C_10P256] =    1E256,              /* Official */
    [C_10P512] =    HUGE_VAL,           /* Official 1E512 - too large for double! */
    [C_10P1024] =   HUGE_VAL,           /* Official 1E1024 - too large for double! */
    [C_10P2048] =   HUGE_VAL,           /* Official 1E2048 - too large for double! */
    [C_10P4096] =   HUGE_VAL,           /* Official 1E4096 - too large for double! */
};

/*
    Returns reminder of absolute double number divided by 2, i.e. for any number it calculates result
    of number mod 2. Used by trigonometric functions
*/
double TrimDoubleRange(double a)
{
    union {
        uint64_t i;
        uint32_t i32[2];
        double d;
    } n, out;

    n.d = a;

    uint32_t exp = (n.i32[0] >> 20) & 0x7ff;
    uint64_t man = n.i & 0x000fffffffffffffULL;

    if (man && exp > 0x3ff && exp < (0x3ff + 52))
    {
        man = (man << (exp - 0x3ff)) & 0x001fffffffffffffULL;
        exp = 0x3ff;

        if (man) {
            int d = __builtin_clzll(man) - 11;

            if (d) {
                man = (man << (d)) & 0x000fffffffffffffULL;
                exp = exp - d;
            }
        }
        else
        {
            exp=0;
        }
    }
    else if (!man && exp > 0x3ff)
    {
        exp = 0;
    }

    out.i = man & ~0x0010000000000000ULL;
    out.i32[0] |= exp << 20;

    return out.d;
}


uint32_t *EMIT_lineF(uint32_t *ptr, uint16_t **m68k_ptr)
{
    uint16_t opcode = BE16((*m68k_ptr)[0]);
    uint16_t opcode2 = BE16((*m68k_ptr)[1]);
    uint8_t ext_count = 1;
    (*m68k_ptr)++;

    /* FABS.X reg-reg */
    if (opcode == 0xf200 && (opcode2 & 0x407f) == 0x0018) // <- fix second word!
    {
        uint8_t fp_src = (opcode2 >> 10) & 7;
        uint8_t fp_dst = (opcode2 >> 7) & 7;
        *ptr++ = fabsd(fp_dst, fp_src);

        ptr = EMIT_AdvancePC(ptr, 2 * (ext_count + 1));
        (*m68k_ptr) += ext_count;
    }
    /* FMOVECR reg */
    if (opcode == 0xf200 && (opcode2 & 0xfc00) == 0x5c00)
    {
        uint8_t fp_dst = (opcode2 >> 7) & 7;
        uint8_t base_reg = RA_AllocARMRegister(&ptr);
        uint8_t offset = opcode2 & 0x7f;

        /* Alloc destination FP register for write */
        fp_dst = RA_MapFPURegisterForWrite(&ptr, fp_dst);

        /*
            Load pointer to constants into base register, then load the value from table into
            destination VFP register, finally skip the base address (which is not an ARM INSN)
        */
        *ptr++ = ldr_offset(15, base_reg, 4);
        *ptr++ = fldd(fp_dst, base_reg, offset * 2);
        *ptr++ = b_cc(ARM_CC_AL, 0);
        *ptr++ = BE32((uint32_t)&constants[0]);

        RA_FreeARMRegister(&ptr, base_reg);
        ptr = EMIT_AdvancePC(ptr, 2 * (ext_count + 1));
        (*m68k_ptr) += ext_count;
    }
    /* FSIN.X reg, reg */
    else if (opcode == 0xf200 && (opcode2 & 0xe07f) == 0x000e)
    {
        uint8_t fp_dst = RA_MapFPURegister(&ptr, (opcode2 >> 7) & 7);
        uint8_t fp_src = RA_MapFPURegister(&ptr, (opcode2 >> 10) & 7);
        uint8_t base_reg = RA_AllocARMRegister(&ptr);
        uint8_t fp_tmp1 = RA_AllocFPURegister(&ptr);
        uint8_t fp_tmp2 = RA_AllocFPURegister(&ptr);

        /* Alloc destination FP register for write */
        fp_dst = RA_MapFPURegisterForWrite(&ptr, fp_dst);

        *ptr++ = ldr_offset(15, base_reg, 0);
        *ptr++ = b_cc(ARM_CC_AL, 0);
        *ptr++ = BE32((uint32_t)(&constants[0]));

        /* Multiply argument with 1/Pi - full 2Pi range reduces to 2 */
        *ptr++ = fldd(fp_tmp1, base_reg, C_1_PI * 2);
        *ptr++ = fmuld(fp_tmp2, fp_tmp1, fp_src);

        /*
            Load pointer to constants into base register, then load the value from table into
            destination VFP register, finally skip the base address (which is not an ARM INSN)
        */
        *ptr++ = ldr_offset(15, base_reg, 0);
        *ptr++ = b_cc(ARM_CC_AL, 0);
        *ptr++ = BE32((uint32_t)(&constants[C_SIN_COEFF]));

#if USE_POLY_21
        *ptr++ = fldd(fp_tmp2, base_reg, 0);        /* c0 -> tmp2 */
        *ptr++ = fmuld(fp_tmp1, fp_src, fp_src);    /* Get tmp1 = x^2 */
        *ptr++ = fldd(fp_dst, base_reg, 2);         /* c1 -> dst */
        *ptr++ = fmacd(fp_dst, fp_tmp2, fp_tmp1);   /* c0 * x^2 + c1 -> dst */
        *ptr++ = fldd(fp_tmp2, base_reg, 4);        /* c2 -> tmp2 */
        *ptr++ = fmacd(fp_tmp2, fp_dst, fp_tmp1);   /* dst * x^2 + c2-> tmp2 */
        *ptr++ = fldd(fp_dst, base_reg, 6);         /* c3 -> dst */
        *ptr++ = fmacd(fp_dst, fp_tmp2, fp_tmp1);   /* tmp2 * x^2 + c3 -> dst */
        *ptr++ = fldd(fp_tmp2, base_reg, 8);        /* c4 -> tmp2 */
        *ptr++ = fmacd(fp_tmp2, fp_dst, fp_tmp1);   /* dst * x^2 + c4 -> tmp2 */
        *ptr++ = fldd(fp_dst, base_reg, 10);        /* c5 -> dst */
        *ptr++ = fmacd(fp_dst, fp_tmp2, fp_tmp1);   /* tmp2 * x^2 + c5 -> dst */
        *ptr++ = fldd(fp_tmp2, base_reg, 12);        /* c6 -> tmp2 */
        *ptr++ = fmacd(fp_tmp2, fp_dst, fp_tmp1);   /* dst * x^2 + c6 -> tmp2 */
        *ptr++ = fldd(fp_dst, base_reg, 14);        /* c7 -> dst */
        *ptr++ = fmacd(fp_dst, fp_tmp2, fp_tmp1);   /* tmp2 * x^2 + c7 -> dst */
        *ptr++ = fldd(fp_tmp2, base_reg, 16);        /* c8 -> tmp2 */
        *ptr++ = fmacd(fp_tmp2, fp_dst, fp_tmp1);   /* dst * x^2 + c8 -> tmp2 */
        *ptr++ = fldd(fp_dst, base_reg, 18);        /* c9 -> dst */
        *ptr++ = fmacd(fp_dst, fp_tmp2, fp_tmp1);   /* tmp2 * x^2 + c9 -> dst */
        *ptr++ = fldd(fp_tmp2, base_reg, 20);        /* c10 -> tmp2 */
        *ptr++ = fmacd(fp_tmp2, fp_dst, fp_tmp1);   /* dst * x^2 + c10 -> tmp2 */
        *ptr++ = fmuld(fp_dst, fp_tmp2, fp_src);     /* tmp2 * x -> dst */
#else
        *ptr++ = fldd(fp_tmp2, base_reg, 0);        /* c0 -> tmp2 */
        *ptr++ = fmuld(fp_tmp1, fp_src, fp_src);    /* Get tmp1 = x^2 */
        *ptr++ = fldd(fp_dst, base_reg, 2);         /* c1 -> dst */
        *ptr++ = fmacd(fp_dst, fp_tmp2, fp_tmp1);   /* c0 * x^2 + c1 -> dst */
        *ptr++ = fldd(fp_tmp2, base_reg, 4);        /* c2 -> tmp2 */
        *ptr++ = fmacd(fp_tmp2, fp_dst, fp_tmp1);   /* dst * x^2 + c2-> tmp2 */
        *ptr++ = fldd(fp_dst, base_reg, 6);         /* c3 -> dst */
        *ptr++ = fmacd(fp_dst, fp_tmp2, fp_tmp1);   /* tmp2 * x^2 + c3 -> dst */
        *ptr++ = fldd(fp_tmp2, base_reg, 8);        /* c4 -> tmp2 */
        *ptr++ = fmacd(fp_tmp2, fp_dst, fp_tmp1);   /* dst * x^2 + c4 -> tmp2 */
        *ptr++ = fldd(fp_dst, base_reg, 10);        /* c5 -> dst */
        *ptr++ = fmacd(fp_dst, fp_tmp2, fp_tmp1);   /* tmp2 * x^2 + c5 -> dst */
        *ptr++ = fmuld(fp_dst, fp_dst, fp_src);     /* dst * x -> dst */
#endif

        RA_FreeFPURegister(&ptr, fp_tmp1);
        RA_FreeFPURegister(&ptr, fp_tmp2);
        RA_FreeARMRegister(&ptr, base_reg);
        ptr = EMIT_AdvancePC(ptr, 2 * (ext_count + 1));
        (*m68k_ptr) += ext_count;
    }
    /* FCOS.X reg, reg */
    else if (opcode == 0xf200 && (opcode2 & 0xe07f) == 0x001d)
    {
        uint8_t fp_dst = RA_MapFPURegister(&ptr, (opcode2 >> 7) & 7);
        uint8_t fp_src = RA_MapFPURegister(&ptr, (opcode2 >> 10) & 7);
        uint8_t base_reg = RA_AllocARMRegister(&ptr);
        uint8_t top_half = RA_AllocARMRegister(&ptr);
        uint8_t cmp_num = RA_AllocARMRegister(&ptr);
        uint8_t fp_tmp1 = RA_AllocFPURegister(&ptr);
        uint8_t fp_tmp2 = RA_AllocFPURegister(&ptr);
        uint32_t *tmp_ptr;
        uint32_t *exit_1;
        uint32_t *exit_2;
        uint32_t *exit_3;

        /* Alloc destination FP register for write */
        fp_dst = RA_MapFPURegisterForWrite(&ptr, fp_dst);

        /*
            Load pointer to constants into base register, then load the value from table into
            destination VFP register, finally skip the base address (which is not an ARM INSN)
        */
        *ptr++ = ldr_offset(15, base_reg, 0);
        *ptr++ = b_cc(ARM_CC_AL, 0);
        *ptr++ = BE32((uint32_t)(&constants[0]));

        *ptr++ = movw_immed_u16(cmp_num, 0);

        /* cos(x)=cos(-x) -> tmp1 = |x| */
        *ptr++ = fabsd(fp_tmp1, fp_src);

        /* Divide x by Pi -> result into d0 */
        *ptr++ = fldd(fp_tmp2, base_reg, C_1_PI * 2);
        *ptr++ = fmuld(0, fp_tmp1, fp_tmp2);

        /* Trim range to 0..2 */
        /* Keep r0-r3,lr and ip safe on the stack. Exclude reg_quot and reg_rem in case they were allocated in r0..r4 range */
        *ptr++ = push((1 << base_reg) | 0x0f | (1 << 12));
        *ptr++ = ldr_offset(15, 12, 4);
        *ptr++ = blx_cc_reg(ARM_CC_AL, 12);
        *ptr++ = b_cc(ARM_CC_AL, 0);
        *ptr++ = BE32((uint32_t)&TrimDoubleRange);
        *ptr++ = pop((1 << base_reg) | 0x0f | (1 << 12));

        /*
            At this point d0 contains range trimmed to 0..2 corresponding to 0..2Pi.
            Load top half of the d0 register into ARM register
        */
        *ptr++ = INSN_TO_LE(0xee300b10 | (top_half << 12));

        /* Range 1: 0..0.5Pi - cos(x) uses cos(x) table */
        *ptr++ = movt_immed_u16(cmp_num, 0x3fe0);
        *ptr++ = cmp_reg(top_half, cmp_num);
        tmp_ptr = ptr;
        *ptr++ = b_cc(ARM_CC_GT, 0);

        *ptr++ = fldd(fp_tmp1, base_reg, 2*C_COS_COEFF + 0);        /* c0 -> tmp1 */
        *ptr++ = fmuld(0, 0, 0);
        *ptr++ = fldd(fp_tmp2, base_reg, 2*C_COS_COEFF + 2);        /* c1 -> tmp2 */
        *ptr++ = fmacd(fp_tmp2, fp_tmp1, 0);                        /* c0 * x^2 + c1 -> dst */
        *ptr++ = fldd(fp_tmp1, base_reg, 2*C_COS_COEFF + 4);        /* c1 -> tmp2 */
        *ptr++ = fmacd(fp_tmp1, fp_tmp2, 0);                        /* c0 * x^2 + c1 -> dst */
        *ptr++ = fldd(fp_tmp2, base_reg, 2*C_COS_COEFF + 6);        /* c1 -> tmp2 */
        *ptr++ = fmacd(fp_tmp2, fp_tmp1, 0);                        /* c0 * x^2 + c1 -> dst */
        *ptr++ = fldd(fp_tmp1, base_reg, 2*C_COS_COEFF + 8);        /* c1 -> tmp2 */
        *ptr++ = fmacd(fp_tmp1, fp_tmp2, 0);                        /* c0 * x^2 + c1 -> dst */
        *ptr++ = fldd(fp_tmp2, base_reg, 2*C_COS_COEFF + 10);        /* c1 -> tmp2 */
        *ptr++ = fmacd(fp_tmp2, fp_tmp1, 0);                        /* c0 * x^2 + c1 -> dst */
        *ptr++ = fldd(fp_tmp1, base_reg, 2*C_COS_COEFF + 12);        /* c1 -> tmp2 */
        *ptr++ = fmacd(fp_tmp1, fp_tmp2, 0);                        /* c0 * x^2 + c1 -> dst */
        *ptr++ = fldd(fp_tmp2, base_reg, 2*C_COS_COEFF + 14);        /* c1 -> tmp2 */
        *ptr++ = fmacd(fp_tmp2, fp_tmp1, 0);                        /* c0 * x^2 + c1 -> dst */
        *ptr++ = fldd(fp_dst, base_reg, 2*C_COS_COEFF + 16);        /* c1 -> tmp2 */
        *ptr++ = fmacd(fp_dst, fp_tmp2, 0);                        /* c0 * x^2 + c1 -> dst */
        exit_1 = ptr;
        *ptr++ = b_cc(ARM_CC_AL, 0);
        *tmp_ptr |= INSN_TO_LE(ptr - tmp_ptr - 2);

        /* Range 2: 0.5Pi..1, cos(x) = -sin(x-0.5Pi) */
        *ptr++ = movt_immed_u16(cmp_num, 0x3ff0);
        *ptr++ = cmp_reg(top_half, cmp_num);
        tmp_ptr = ptr;
        *ptr++ = b_cc(ARM_CC_GT, 0);
        *ptr++ = fldd(fp_tmp1, base_reg, C_0_5 * 2);
        *ptr++ = fsubd(0, 0, fp_tmp1);
        *ptr++ = fldd(fp_tmp1, base_reg, 2*C_SIN_COEFF + 0);        /* c0 -> tmp1 */
        *ptr++ = fmuld(fp_dst, 0, 0);
        *ptr++ = fldd(fp_tmp2, base_reg, 2*C_SIN_COEFF + 2);        /* c1 -> tmp2 */
        *ptr++ = fmacd(fp_tmp2, fp_tmp1, fp_dst);                        /* c0 * x^2 + c1 -> dst */
        *ptr++ = fldd(fp_tmp1, base_reg, 2*C_SIN_COEFF + 4);        /* c1 -> tmp2 */
        *ptr++ = fmacd(fp_tmp1, fp_tmp2, fp_dst);                        /* c0 * x^2 + c1 -> dst */
        *ptr++ = fldd(fp_tmp2, base_reg, 2*C_SIN_COEFF + 6);        /* c1 -> tmp2 */
        *ptr++ = fmacd(fp_tmp2, fp_tmp1, fp_dst);                        /* c0 * x^2 + c1 -> dst */
        *ptr++ = fldd(fp_tmp1, base_reg, 2*C_SIN_COEFF + 8);        /* c1 -> tmp2 */
        *ptr++ = fmacd(fp_tmp1, fp_tmp2, fp_dst);                        /* c0 * x^2 + c1 -> dst */
        *ptr++ = fldd(fp_tmp2, base_reg, 2*C_SIN_COEFF + 10);        /* c1 -> tmp2 */
        *ptr++ = fmacd(fp_tmp2, fp_tmp1, fp_dst);                        /* c0 * x^2 + c1 -> dst */
        *ptr++ = fldd(fp_tmp1, base_reg, 2*C_SIN_COEFF + 12);        /* c1 -> tmp2 */
        *ptr++ = fmacd(fp_tmp1, fp_tmp2, 0);                        /* c0 * x^2 + c1 -> dst */
        *ptr++ = fldd(fp_tmp2, base_reg, 2*C_SIN_COEFF + 14);        /* c1 -> tmp2 */
        *ptr++ = fmacd(fp_tmp2, fp_tmp1, fp_dst);
        *ptr++ = fmuld(fp_dst, fp_dst, fp_tmp2);
        *ptr++ = fnegd(fp_dst, fp_dst);
        exit_2 = ptr;
        *ptr++ = b_cc(ARM_CC_AL, 0);
        *tmp_ptr |= INSN_TO_LE(ptr - tmp_ptr - 2);

        /* Range 3: 1..1.5Pi, cos(x) = -cos(x-Pi) */
        *ptr++ = movt_immed_u16(cmp_num, 0x3ff8);
        *ptr++ = cmp_reg(top_half, cmp_num);
        tmp_ptr = ptr;
        *ptr++ = b_cc(ARM_CC_GT, 0);
        *ptr++ = fldd(fp_tmp1, base_reg, C_10P0 * 2);
        *ptr++ = fsubd(0, 0, fp_tmp1);
        *ptr++ = fldd(fp_tmp1, base_reg, 2*C_COS_COEFF + 0);        /* c0 -> tmp1 */
        *ptr++ = fmuld(0, 0, 0);
        *ptr++ = fldd(fp_tmp2, base_reg, 2*C_COS_COEFF + 2);        /* c1 -> tmp2 */
        *ptr++ = fmacd(fp_tmp2, fp_tmp1, 0);                        /* c0 * x^2 + c1 -> dst */
        *ptr++ = fldd(fp_tmp1, base_reg, 2*C_COS_COEFF + 4);        /* c1 -> tmp2 */
        *ptr++ = fmacd(fp_tmp1, fp_tmp2, 0);                        /* c0 * x^2 + c1 -> dst */
        *ptr++ = fldd(fp_tmp2, base_reg, 2*C_COS_COEFF + 6);        /* c1 -> tmp2 */
        *ptr++ = fmacd(fp_tmp2, fp_tmp1, 0);                        /* c0 * x^2 + c1 -> dst */
        *ptr++ = fldd(fp_tmp1, base_reg, 2*C_COS_COEFF + 8);        /* c1 -> tmp2 */
        *ptr++ = fmacd(fp_tmp1, fp_tmp2, 0);                        /* c0 * x^2 + c1 -> dst */
        *ptr++ = fldd(fp_tmp2, base_reg, 2*C_COS_COEFF + 10);        /* c1 -> tmp2 */
        *ptr++ = fmacd(fp_tmp2, fp_tmp1, 0);                        /* c0 * x^2 + c1 -> dst */
        *ptr++ = fldd(fp_tmp1, base_reg, 2*C_COS_COEFF + 12);        /* c1 -> tmp2 */
        *ptr++ = fmacd(fp_tmp1, fp_tmp2, 0);                        /* c0 * x^2 + c1 -> dst */
        *ptr++ = fldd(fp_tmp2, base_reg, 2*C_COS_COEFF + 14);        /* c1 -> tmp2 */
        *ptr++ = fmacd(fp_tmp2, fp_tmp1, 0);                        /* c0 * x^2 + c1 -> dst */
        *ptr++ = fldd(fp_dst, base_reg, 2*C_COS_COEFF + 16);        /* c1 -> tmp2 */
        *ptr++ = fmacd(fp_dst, fp_tmp2, 0);                        /* c0 * x^2 + c1 -> dst */
        *ptr++ = fnegd(fp_dst, fp_dst);
        exit_3 = ptr;
        *ptr++ = b_cc(ARM_CC_AL, 0);
        *tmp_ptr |= INSN_TO_LE(ptr - tmp_ptr - 2);

        /* Range 4: 1.5Pi..2Pi, cos(x) = sin(x-1.5Pi) */
        *ptr++ = fldd(fp_tmp1, base_reg, C_1_5 * 2);
        *ptr++ = fsubd(0, 0, fp_tmp1);
        *ptr++ = fldd(fp_tmp1, base_reg, 2*C_SIN_COEFF + 0);        /* c0 -> tmp1 */
        *ptr++ = fmuld(fp_dst, 0, 0);
        *ptr++ = fldd(fp_tmp2, base_reg, 2*C_SIN_COEFF + 2);        /* c1 -> tmp2 */
        *ptr++ = fmacd(fp_tmp2, fp_tmp1, fp_dst);                        /* c0 * x^2 + c1 -> dst */
        *ptr++ = fldd(fp_tmp1, base_reg, 2*C_SIN_COEFF + 4);        /* c1 -> tmp2 */
        *ptr++ = fmacd(fp_tmp1, fp_tmp2, fp_dst);                        /* c0 * x^2 + c1 -> dst */
        *ptr++ = fldd(fp_tmp2, base_reg, 2*C_SIN_COEFF + 6);        /* c1 -> tmp2 */
        *ptr++ = fmacd(fp_tmp2, fp_tmp1, fp_dst);                        /* c0 * x^2 + c1 -> dst */
        *ptr++ = fldd(fp_tmp1, base_reg, 2*C_SIN_COEFF + 8);        /* c1 -> tmp2 */
        *ptr++ = fmacd(fp_tmp1, fp_tmp2, fp_dst);                        /* c0 * x^2 + c1 -> dst */
        *ptr++ = fldd(fp_tmp2, base_reg, 2*C_SIN_COEFF + 10);        /* c1 -> tmp2 */
        *ptr++ = fmacd(fp_tmp2, fp_tmp1, fp_dst);                        /* c0 * x^2 + c1 -> dst */
        *ptr++ = fldd(fp_tmp1, base_reg, 2*C_SIN_COEFF + 12);        /* c1 -> tmp2 */
        *ptr++ = fmacd(fp_tmp1, fp_tmp2, 0);                        /* c0 * x^2 + c1 -> dst */
        *ptr++ = fldd(fp_tmp2, base_reg, 2*C_SIN_COEFF + 14);        /* c1 -> tmp2 */
        *ptr++ = fmacd(fp_tmp2, fp_tmp1, fp_dst);
        *ptr++ = fmuld(fp_dst, fp_dst, fp_tmp2);

        *exit_1 |= INSN_TO_LE(ptr - exit_1 - 2);
        *exit_2 |= INSN_TO_LE(ptr - exit_2 - 2);
        *exit_3 |= INSN_TO_LE(ptr - exit_3 - 2);

        RA_FreeFPURegister(&ptr, fp_tmp1);
        RA_FreeFPURegister(&ptr, fp_tmp2);
        RA_FreeARMRegister(&ptr, base_reg);
        RA_FreeARMRegister(&ptr, top_half);
        RA_FreeARMRegister(&ptr, cmp_num);
        ptr = EMIT_AdvancePC(ptr, 2 * (ext_count + 1));
        (*m68k_ptr) += ext_count;

        *ptr++ = INSN_TO_LE(0xfffffff0);
    }
    /* FNOP */
    else if (opcode == 0xf280 && opcode2 == 0)
    {
        ptr = EMIT_AdvancePC(ptr, 2 * (ext_count + 1));
        (*m68k_ptr) += ext_count;
        ptr = EMIT_FlushPC(ptr);
    }
    else
        *ptr++ = udf(opcode);

    return ptr;
}
