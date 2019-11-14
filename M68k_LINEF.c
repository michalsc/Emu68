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

    C_SIN_COEFF_SINGLE = 0x1a,
    C_COS_COEFF_SINGLE = 0x2a,

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

static long double const constants[128] = {
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

    /* Polynom coefficients for sin(x*Pi), x=0..0.5*/

    [C_SIN_COEFF] = -2.11100178050346585936E-5,
                    4.65963708473294521719E-4,
                    -7.37035513524020578156E-3,
                    8.21458769726032277098E-2,
                    -5.99264528627362954518E-1,
                    2.55016403985097679243,
                    -5.16771278004952168888,
                    3.14159265358979102647,
    
    /* Reduced number of polynom coefficients for sin(x*Pi), x=0..0.5 */

    [C_SIN_COEFF_SINGLE] = 
                    7.74455095806670556524E-2,
                    -5.98160819620617657839E-1,
                    2.55005088882843729408,
                    -5.1677080762924026306,
                    3.14159259939191476447,

    /* Polynom coefficients for cos(x*Pi), x=0..0.5 */

    [C_COS_COEFF] = 4.15383875943350535407E-6,
                    -1.04570624685965272291E-4,
                    1.92955784205552168426E-3,
                    -2.58068890507489103003E-2,
                    2.35330630164104256943E-1,
                    -1.33526276884550367708,
                    4.05871212641655666324,
                    -4.93480220054467742126,
                    9.99999999999999997244E-1,
    
    /* Reduced number of polynom coefficients for cos(x*Pi), x=0..0.5 */
    [C_COS_COEFF_SINGLE] =
                    2.20485796302921884119E-1,
                    -1.33223541188749370639,
                    4.058461009872062766402,
                    -4.93479497666537363458,
                    9.99999967245121125386E-1,

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

void __attribute__((naked)) PolySine(void)
{
    asm volatile(
        "   vpush {d1,d2,d3}        \n"
        "   push {r0}               \n"
        "   ldr r0,=constants       \n"
        "   vldr d1, [r0, %0]       \n"
        "   vmul.f64 d3, d0, d0     \n"
        "   vldr d2, [r0, %0+8]     \n"
        "   vfma.f64 d2, d1, d3     \n"
        "   vldr d1, [r0, %0+16]    \n"
        "   vfma.f64 d1, d2, d3     \n"
        "   vldr d2, [r0, %0+24]    \n"
        "   vfma.f64 d2, d1, d3     \n"
        "   vldr d1, [r0, %0+32]    \n"
        "   vfma.f64 d1, d2, d3     \n"
        "   vldr d2, [r0, %0+40]    \n"
        "   vfma.f64 d2, d1, d3     \n"
        "   vldr d1, [r0, %0+48]    \n"
        "   vfma.f64 d1, d2, d3     \n"
        "   vldr d2, [r0, %0+56]    \n"
        "   vfma.f64 d2, d1, d3     \n"
        "   vmul.f64 d0, d2, d0     \n"
        "   pop {r0}                \n"
        "   vpop {d1,d2,d3}         \n"
        "   bx lr                   \n"
        "   .ltorg                  \n"::"i"(C_SIN_COEFF*8)
    );
}

void __attribute__((naked)) PolySineSingle(void)
{
    asm volatile(
        "   vpush {d1,d2,d3}        \n"
        "   push {r0}               \n"
        "   ldr r0,=constants       \n"
        "   vldr d1, [r0, %0]       \n"
        "   vmul.f64 d3, d0, d0     \n"
        "   vldr d2, [r0, %0+8]     \n"
        "   vfma.f64 d2, d1, d3     \n"
        "   vldr d1, [r0, %0+16]    \n"
        "   vfma.f64 d1, d2, d3     \n"
        "   vldr d2, [r0, %0+24]    \n"
        "   vfma.f64 d2, d1, d3     \n"
        "   vldr d1, [r0, %0+32]    \n"
        "   vfma.f64 d1, d2, d3     \n"
        "   vmul.f64 d0, d1, d0     \n"
        "   pop {r0}                \n"
        "   vpop {d1,d2,d3}         \n"
        "   bx lr                   \n"
        "   .ltorg                  \n"::"i"(C_SIN_COEFF_SINGLE*8)
    );
}

void __attribute__((naked)) PolyCosine(void)
{
    asm volatile(
        "   vpush {d1,d2}           \n"
        "   push {r0}               \n"
        "   ldr r0,=constants       \n"
        "   vmul.f64 d2, d0, d0     \n"
        "   vldr d0, [r0, %0]       \n"
        "   vldr d1, [r0, %0+8]     \n"
        "   vfma.f64 d1, d0, d2     \n"
        "   vldr d0, [r0, %0+16]    \n"
        "   vfma.f64 d0, d1, d2     \n"
        "   vldr d1, [r0, %0+24]    \n"
        "   vfma.f64 d1, d0, d2     \n"
        "   vldr d0, [r0, %0+32]    \n"
        "   vfma.f64 d0, d1, d2     \n"
        "   vldr d1, [r0, %0+40]    \n"
        "   vfma.f64 d1, d0, d2     \n"
        "   vldr d0, [r0, %0+48]    \n"
        "   vfma.f64 d0, d1, d2     \n"
        "   vldr d1, [r0, %0+56]    \n"
        "   vfma.f64 d1, d0, d2     \n"
        "   vldr d0, [r0, %0+64]    \n"
        "   vfma.f64 d0, d1, d2     \n"
        "   pop {r0}                \n"
        "   vpop {d1,d2}            \n"
        "   bx lr                   \n"
        "   .ltorg                  \n"::"i"(C_COS_COEFF*8)
    );
}

void __attribute__((naked)) PolyCosineSingle(void)
{
    asm volatile(
        "   vpush {d1,d2}           \n"
        "   push {r0}               \n"
        "   ldr r0,=constants       \n"
        "   vmul.f64 d2, d0, d0     \n"
        "   vldr d0, [r0, %0]       \n"
        "   vldr d1, [r0, %0+8]     \n"
        "   vfma.f64 d1, d0, d2     \n"
        "   vldr d0, [r0, %0+16]    \n"
        "   vfma.f64 d0, d1, d2     \n"
        "   vldr d1, [r0, %0+24]    \n"
        "   vfma.f64 d1, d0, d2     \n"
        "   vldr d0, [r0, %0+32]    \n"
        "   vfma.f64 d0, d1, d2     \n"
        "   pop {r0}                \n"
        "   vpop {d1,d2}            \n"
        "   bx lr                   \n"
        "   .ltorg                  \n"::"i"(C_COS_COEFF_SINGLE*8)
    );
}

enum FPUOpSize {
    SIZE_L = 0,
    SIZE_S = 1,
    SIZE_X = 2,
    SIZE_P = 3,
    SIZE_W = 4,
    SIZE_D = 5,
    SIZE_B = 6
};

/* Allocates FPU register and fetches data according to the R/M field of the FPU opcode */
uint32_t *FPU_FetchData(uint32_t *ptr, uint16_t **m68k_ptr, uint8_t *reg, uint16_t opcode, 
        uint16_t opcode2, uint8_t *ext_count)
{
    printf("[JIT] FPU_FetchData()\n");

    /* IF R/M is zero, then source identifier is FPU reg number. */
    if ((opcode2 & 0x4000) == 0)
    {
        *reg = RA_MapFPURegister(&ptr, (opcode2 >> 10) & 7);
    }
    else
    {
        /* 
            R/M was set to 1, the source is defined by EA stored in first part of the
            opcode. Source identifier specifies the data length.

            Get EA, eventually (in case of mode 000 - Dn) perform simple data transfer.
            Otherwise get address from EA and fetch data here
        */
        *reg = RA_AllocFPURegister(&ptr);
        uint8_t ea = opcode & 0x3f;
        enum FPUOpSize size = (opcode2 >> 10) & 7;

        /* Case 1: mode 000 - Dn */
        if ((ea & 0x38) == 0)
        {
            uint8_t int_reg = 0xff;
            uint8_t tmp_reg;

            switch (size)
            {
                /* Single - move to single half of the reg, convert to double */
                case SIZE_S:
                    ptr = EMIT_LoadFromEffectiveAddress(ptr, 4, &int_reg, ea, *m68k_ptr, ext_count, 1);
                    *ptr++ = fmsr(*reg * 2, int_reg);
                    *ptr++ = fcvtds(*reg, *reg * 2);
                    RA_FreeARMRegister(&ptr, int_reg);
                    break;

                case SIZE_L:
                    ptr = EMIT_LoadFromEffectiveAddress(ptr, 4, &int_reg, ea, *m68k_ptr, ext_count, 1);
                    *ptr++ = fmsr(*reg * 2, int_reg);
                    *ptr++ = fsitod(*reg, *reg * 2);
                    RA_FreeARMRegister(&ptr, int_reg);
                    break;

                case SIZE_W:
                    ptr = EMIT_LoadFromEffectiveAddress(ptr, 2, &int_reg, ea, *m68k_ptr, ext_count, 1);
                    tmp_reg = RA_AllocARMRegister(&ptr);
                    *ptr++ = sxth(tmp_reg, int_reg, 0);
                    *ptr++ = fmsr(*reg * 2, tmp_reg);
                    *ptr++ = fsitod(*reg, *reg * 2);
                    RA_FreeARMRegister(&ptr, tmp_reg);
                    RA_FreeARMRegister(&ptr, int_reg);
                    break;

                case SIZE_B:
                    ptr = EMIT_LoadFromEffectiveAddress(ptr, 1, &int_reg, ea, *m68k_ptr, ext_count, 1);
                    tmp_reg = RA_AllocARMRegister(&ptr);
                    *ptr++ = sxtb(tmp_reg, int_reg, 0);
                    *ptr++ = fmsr(*reg * 2, tmp_reg);
                    *ptr++ = fsitod(*reg, *reg * 2);
                    RA_FreeARMRegister(&ptr, tmp_reg);
                    RA_FreeARMRegister(&ptr, int_reg);
                    break;

                default:
                    printf("[JIT] LineF: wrong argument size %d for Dn access\n", (int)size);
            }
        }
        /* Case 2: mode 111:100 - immediate */
        else if (ea == 0x3c)
        {
            /* Step 1: Fetch data *or* pointer to data into int_reg */
            uint8_t int_reg = 0xff;
            int not_yet_done = 0;

            switch (size)
            {
                case SIZE_S:
                    ptr = EMIT_LoadFromEffectiveAddress(ptr, 4, &int_reg, ea, *m68k_ptr, ext_count, 0);
                    *ptr++ = fmsr(*reg * 2, int_reg);
                    *ptr++ = fcvtds(*reg, *reg * 2);
                    break;
                case SIZE_L:
                    ptr = EMIT_LoadFromEffectiveAddress(ptr, 4, &int_reg, ea, *m68k_ptr, ext_count, 0);
                    *ptr++ = fmsr(*reg * 2, int_reg);
                    *ptr++ = fsitod(*reg, *reg * 2);
                    break;
                case SIZE_W:
                    ptr = EMIT_LoadFromEffectiveAddress(ptr, 2, &int_reg, ea, *m68k_ptr, ext_count, 0);
                    *ptr++ = sxth(int_reg, int_reg, 0);
                    *ptr++ = fmsr(*reg * 2, int_reg);
                    *ptr++ = fsitod(*reg, *reg * 2);
                    break;
                case SIZE_B:
                    ptr = EMIT_LoadFromEffectiveAddress(ptr, 1, &int_reg, ea, *m68k_ptr, ext_count, 0);
                    *ptr++ = sxtb(int_reg, int_reg, 0);
                    *ptr++ = fmsr(*reg * 2, int_reg);
                    *ptr++ = fsitod(*reg, *reg * 2);
                    break;
                default:
                    ptr = EMIT_LoadFromEffectiveAddress(ptr, 0, &int_reg, ea, *m68k_ptr, ext_count, 0);
                    not_yet_done = 1;
                    break;
            }

            /* Step 2: if data not yet in the reg, use the address to load it into FPU register */
            if (not_yet_done)
            {
                switch(size)
                {
                    case SIZE_D:
                        *ptr++ = fldd(*reg, int_reg, 0);
                        *ext_count += 4;
                        break;
                    
                    case SIZE_X:
                        *ext_count += 6;
                        break;

                    case SIZE_P:
                        *ext_count += 6;
                    default:
                        break;
                }
            }

            RA_FreeARMRegister(&ptr, int_reg);
        }
        /* Case 3: get pointer to data (EA) and fetch yourself */
        else
        {
            uint8_t int_reg = 0xff;
            uint8_t val_reg = 0xff;
            ptr = EMIT_LoadFromEffectiveAddress(ptr, 0, &int_reg, ea, *m68k_ptr, ext_count, 1);



            RA_FreeARMRegister(&ptr, int_reg);
            RA_FreeARMRegister(&ptr, val_reg);
        }
    }

    return ptr;
}

uint32_t *EMIT_lineF(uint32_t *ptr, uint16_t **m68k_ptr)
{
    uint16_t opcode = BE16((*m68k_ptr)[0]);
    uint16_t opcode2 = BE16((*m68k_ptr)[1]);
    uint8_t ext_count = 1;
    (*m68k_ptr)++;

    /* FABS */
    if ((opcode & 0xffc0) == 0xf200 && (opcode2 & 0xa07f) == 0x0018)
    {
        uint8_t fp_src = 0xff;
        uint8_t fp_dst = (opcode2 >> 7) & 7;

        ptr = FPU_FetchData(ptr, m68k_ptr, &fp_src, opcode, opcode2, &ext_count);
        fp_dst = RA_MapFPURegisterForWrite(&ptr, fp_dst);

        *ptr++ = fabsd(fp_dst, fp_src);

        RA_FreeFPURegister(&ptr, fp_src);

        ptr = EMIT_AdvancePC(ptr, 2 * (ext_count + 1));
        (*m68k_ptr) += ext_count;
    }
    /* FADD */
    else if ((opcode & 0xffc0) == 0xf200 && (opcode2 & 0xa07f) == 0x0022)
    {
        uint8_t fp_src = 0xff;
        uint8_t fp_dst = (opcode2 >> 7) & 7;

        ptr = FPU_FetchData(ptr, m68k_ptr, &fp_src, opcode, opcode2, &ext_count);
        fp_dst = RA_MapFPURegisterForWrite(&ptr, fp_dst);

        *ptr++ = faddd(fp_dst, fp_dst, fp_src);

        RA_FreeFPURegister(&ptr, fp_src);

        ptr = EMIT_AdvancePC(ptr, 2 * (ext_count + 1));
        (*m68k_ptr) += ext_count;
    }
    /* FDIV */
    else if ((opcode & 0xffc0) == 0xf200 && (opcode2 & 0xa07f) == 0x0020)
    {
        uint8_t fp_src = 0xff;
        uint8_t fp_dst = (opcode2 >> 7) & 7;

        ptr = FPU_FetchData(ptr, m68k_ptr, &fp_src, opcode, opcode2, &ext_count);
        fp_dst = RA_MapFPURegisterForWrite(&ptr, fp_dst);

        *ptr++ = fdivd(fp_dst, fp_dst, fp_src);

        RA_FreeFPURegister(&ptr, fp_src);

        ptr = EMIT_AdvancePC(ptr, 2 * (ext_count + 1));
        (*m68k_ptr) += ext_count;
    }
    /* FMUL */
    else if ((opcode & 0xffc0) == 0xf200 && (opcode2 & 0xa07f) == 0x0023)
    {
        uint8_t fp_src = 0xff;
        uint8_t fp_dst = (opcode2 >> 7) & 7;

        ptr = FPU_FetchData(ptr, m68k_ptr, &fp_src, opcode, opcode2, &ext_count);
        fp_dst = RA_MapFPURegisterForWrite(&ptr, fp_dst);

        *ptr++ = fmuld(fp_dst, fp_dst, fp_src);

        RA_FreeFPURegister(&ptr, fp_src);

        ptr = EMIT_AdvancePC(ptr, 2 * (ext_count + 1));
        (*m68k_ptr) += ext_count;
    }
    /* FNEG */
    else if ((opcode & 0xffc0) == 0xf200 && (opcode2 & 0xa07f) == 0x001a)
    {
        uint8_t fp_src = 0xff;
        uint8_t fp_dst = (opcode2 >> 7) & 7;

        ptr = FPU_FetchData(ptr, m68k_ptr, &fp_src, opcode, opcode2, &ext_count);
        fp_dst = RA_MapFPURegisterForWrite(&ptr, fp_dst);

        *ptr++ = fnegd(fp_dst, fp_src);

        RA_FreeFPURegister(&ptr, fp_src);

        ptr = EMIT_AdvancePC(ptr, 2 * (ext_count + 1));
        (*m68k_ptr) += ext_count;
    }
    /* FSUB */
    else if ((opcode & 0xffc0) == 0xf200 && (opcode2 & 0xa07f) == 0x0028)
    {
        uint8_t fp_src = 0xff;
        uint8_t fp_dst = (opcode2 >> 7) & 7;

        ptr = FPU_FetchData(ptr, m68k_ptr, &fp_src, opcode, opcode2, &ext_count);
        fp_dst = RA_MapFPURegisterForWrite(&ptr, fp_dst);

        *ptr++ = fsubd(fp_dst, fp_dst, fp_src);

        RA_FreeFPURegister(&ptr, fp_src);

        ptr = EMIT_AdvancePC(ptr, 2 * (ext_count + 1));
        (*m68k_ptr) += ext_count;
    }
    /* FMOVECR reg */
    else if (opcode == 0xf200 && (opcode2 & 0xfc00) == 0x5c00)
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
    /* FSIN */
    else if ((opcode & 0xffc0) == 0xf200 && (opcode2 & 0xa07f) == 0x000e)
    {
        uint8_t fp_dst = 0xff;
        uint8_t fp_src = 0xff;
        uint8_t base_reg = RA_AllocARMRegister(&ptr);
        uint8_t top_half = RA_AllocARMRegister(&ptr);
        uint8_t sign    = RA_AllocARMRegister(&ptr);
        uint8_t cmp_num = RA_AllocARMRegister(&ptr);
        uint8_t fp_tmp1 = RA_AllocFPURegister(&ptr);
        uint8_t fp_tmp2 = RA_AllocFPURegister(&ptr);
        uint32_t *tmp_ptr;
        uint32_t *ref_ptr;
        uint32_t *exit_1;
        uint32_t *exit_2;
        uint32_t *exit_3;
        uint32_t *adr_sin;
        uint32_t *adr_cos;
        uint32_t *adr_trim;

        /* Fetch source */
        ptr = FPU_FetchData(ptr, m68k_ptr, &fp_src, opcode, opcode2, &ext_count);

        /* Alloc destination FP register for write */
        fp_dst = RA_MapFPURegisterForWrite(&ptr, fp_dst);

        /*
            Load pointer to constants into base register, then load the value from table into
            destination VFP register, finally skip the base address (which is not an ARM INSN)
        */
        *ptr++ = ldr_offset(15, base_reg, 0);
        *ptr++ = b_cc(ARM_CC_AL, 3);
        *ptr++ = BE32((uint32_t)(&constants[0]));
        adr_sin = ptr;
        *ptr++ = BE32((uint32_t)PolySine);
        adr_cos = ptr;
        *ptr++ = BE32((uint32_t)PolyCosine);
        adr_trim = ptr;
        *ptr++ = BE32((uint32_t)&TrimDoubleRange);

        *ptr++ = movw_immed_u16(cmp_num, 0);

        /* Get sign (topmost bit) to separate arm register */
        *ptr++ = INSN_TO_LE(0xee300b10 | (sign << 12) | (fp_src << 16));

        /* sin(x)=-sin(-x) -> tmp1 = |x| */
        *ptr++ = fabsd(fp_tmp1, fp_src);

        /* Divide x by Pi -> result into d0 */
        *ptr++ = fldd(fp_tmp2, base_reg, C_1_PI * 2);
        *ptr++ = fmuld(0, fp_tmp1, fp_tmp2);

        /* Trim range to 0..2 */
        /* Keep r0-r3,lr and ip safe on the stack. Exclude reg_quot and reg_rem in case they were allocated in r0..r4 range */
        *ptr++ = push((1 << base_reg) | 0x0f | (1 << 12));
        ref_ptr = ptr+2;
        *ptr++ = ldr_offset(15, 12, (intptr_t)adr_trim - (intptr_t)ref_ptr);
        *ptr++ = blx_cc_reg(ARM_CC_AL, 12);
        *ptr++ = pop((1 << base_reg) | 0x0f | (1 << 12));

        /*
            At this point d0 contains range trimmed to 0..2 corresponding to 0..2Pi.
            Load top half of the d0 register into ARM register
        */
        *ptr++ = INSN_TO_LE(0xee300b10 | (top_half << 12));

        /* Range 1: 0..0.5Pi - sin(x) uses sin(x) table */
        *ptr++ = movt_immed_u16(cmp_num, 0x3fe0);
        *ptr++ = cmp_reg(top_half, cmp_num);
        tmp_ptr = ptr;
        *ptr++ = b_cc(ARM_CC_GT, 0);

        ref_ptr = ptr+2;
        *ptr++ = ldr_offset(15, 12, (intptr_t)adr_sin - (intptr_t)ref_ptr);
        *ptr++ = blx_cc_reg(ARM_CC_AL, 12);
        *ptr++ = fcpyd(fp_dst, 0);
        exit_1 = ptr;
        *ptr++ = b_cc(ARM_CC_AL, 0);
        *tmp_ptr |= INSN_TO_LE(ptr - tmp_ptr - 2);

        /* Range 2: 0.5Pi..1, sin(x) = cos(x-0.5Pi) */
        *ptr++ = movt_immed_u16(cmp_num, 0x3ff0);
        *ptr++ = cmp_reg(top_half, cmp_num);
        tmp_ptr = ptr;
        *ptr++ = b_cc(ARM_CC_GT, 0);
        *ptr++ = fldd(fp_tmp1, base_reg, C_0_5 * 2);
        *ptr++ = fsubd(0, 0, fp_tmp1);

        ref_ptr = ptr+2;
        *ptr++ = ldr_offset(15, 12, (intptr_t)adr_cos - (intptr_t)ref_ptr);
        *ptr++ = blx_cc_reg(ARM_CC_AL, 12);
        *ptr++ = fcpyd(fp_dst, 0);
        exit_2 = ptr;
        *ptr++ = b_cc(ARM_CC_AL, 0);
        *tmp_ptr |= INSN_TO_LE(ptr - tmp_ptr - 2);

        /* Range 3: 1..1.5Pi, sin(x) = -sin(x-Pi) */
        *ptr++ = movt_immed_u16(cmp_num, 0x3ff8);
        *ptr++ = cmp_reg(top_half, cmp_num);
        tmp_ptr = ptr;
        *ptr++ = b_cc(ARM_CC_GT, 0);
        *ptr++ = fldd(fp_tmp1, base_reg, C_10P0 * 2);
        *ptr++ = fsubd(0, 0, fp_tmp1);

        ref_ptr = ptr+2;
        *ptr++ = ldr_offset(15, 12, (intptr_t)adr_sin - (intptr_t)ref_ptr);
        *ptr++ = blx_cc_reg(ARM_CC_AL, 12);
        *ptr++ = fnegd(fp_dst, 0);
        exit_3 = ptr;
        *ptr++ = b_cc(ARM_CC_AL, 0);
        *tmp_ptr |= INSN_TO_LE(ptr - tmp_ptr - 2);

        /* Range 4: 1.5Pi..2Pi, sin(x) = -cos(x-1.5Pi) */
        *ptr++ = fldd(fp_tmp1, base_reg, C_1_5 * 2);
        *ptr++ = fsubd(0, 0, fp_tmp1);

        ref_ptr = ptr+2;
        *ptr++ = ldr_offset(15, 12, (intptr_t)adr_sin - (intptr_t)ref_ptr);
        *ptr++ = blx_cc_reg(ARM_CC_AL, 12);
        *ptr++ = fnegd(fp_dst, 0);

        *exit_1 |= INSN_TO_LE(ptr - exit_1 - 2);
        *exit_2 |= INSN_TO_LE(ptr - exit_2 - 2);
        *exit_3 |= INSN_TO_LE(ptr - exit_3 - 2);

        *ptr++ = tst_immed(sign, 0xf80);
        *ptr++ = fnegd_cc(ARM_CC_MI, fp_dst, fp_dst);

        RA_FreeFPURegister(&ptr, fp_src);
        RA_FreeFPURegister(&ptr, fp_tmp1);
        RA_FreeFPURegister(&ptr, fp_tmp2);
        RA_FreeARMRegister(&ptr, base_reg);
        RA_FreeARMRegister(&ptr, top_half);
        RA_FreeARMRegister(&ptr, sign);
        RA_FreeARMRegister(&ptr, cmp_num);
        ptr = EMIT_AdvancePC(ptr, 2 * (ext_count + 1));
        (*m68k_ptr) += ext_count;
        *ptr++ = INSN_TO_LE(0xfffffff0);
    }
    /* FCOS */
    else if ((opcode & 0xffc0) == 0xf200 && (opcode2 & 0xa07f) == 0x001d)
    {
        uint8_t fp_dst = 0xff;
        uint8_t fp_src = 0xff;
        uint8_t base_reg = RA_AllocARMRegister(&ptr);
        uint8_t top_half = RA_AllocARMRegister(&ptr);
        uint8_t cmp_num = RA_AllocARMRegister(&ptr);
        uint8_t fp_tmp1 = RA_AllocFPURegister(&ptr);
        uint8_t fp_tmp2 = RA_AllocFPURegister(&ptr);
        uint32_t *tmp_ptr;
        uint32_t *ref_ptr;
        uint32_t *exit_1;
        uint32_t *exit_2;
        uint32_t *exit_3;
        uint32_t *adr_sin;
        uint32_t *adr_cos;
        uint32_t *adr_trim;

        /* Fetch source */
        ptr = FPU_FetchData(ptr, m68k_ptr, &fp_src, opcode, opcode2, &ext_count);

        /* Alloc destination FP register for write */
        fp_dst = RA_MapFPURegisterForWrite(&ptr, fp_dst);

        /*
            Load pointer to constants into base register, then load the value from table into
            destination VFP register, finally skip the base address (which is not an ARM INSN)
        */
        *ptr++ = ldr_offset(15, base_reg, 0);
        *ptr++ = b_cc(ARM_CC_AL, 3);
        *ptr++ = BE32((uint32_t)(&constants[0]));
        adr_sin = ptr;
        *ptr++ = BE32((uint32_t)PolySine);
        adr_cos = ptr;
        *ptr++ = BE32((uint32_t)PolyCosine);
        adr_trim = ptr;
        *ptr++ = BE32((uint32_t)&TrimDoubleRange);

        *ptr++ = movw_immed_u16(cmp_num, 0);

        /* cos(x)=cos(-x) -> tmp1 = |x| */
        *ptr++ = fabsd(fp_tmp1, fp_src);

        /* Divide x by Pi -> result into d0 */
        *ptr++ = fldd(fp_tmp2, base_reg, C_1_PI * 2);
        *ptr++ = fmuld(0, fp_tmp1, fp_tmp2);

        /* Trim range to 0..2 */
        /* Keep r0-r3,lr and ip safe on the stack. Exclude reg_quot and reg_rem in case they were allocated in r0..r4 range */
        *ptr++ = push((1 << base_reg) | 0x0f | (1 << 12));
        ref_ptr = ptr+2;
        *ptr++ = ldr_offset(15, 12, (intptr_t)adr_trim - (intptr_t)ref_ptr);
        *ptr++ = blx_cc_reg(ARM_CC_AL, 12);
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

        ref_ptr = ptr+2;
        *ptr++ = ldr_offset(15, 12, (intptr_t)adr_cos - (intptr_t)ref_ptr);
        *ptr++ = blx_cc_reg(ARM_CC_AL, 12);
        *ptr++ = fcpyd(fp_dst, 0);
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

        ref_ptr = ptr+2;
        *ptr++ = ldr_offset(15, 12, (intptr_t)adr_sin - (intptr_t)ref_ptr);
        *ptr++ = blx_cc_reg(ARM_CC_AL, 12);
        *ptr++ = fnegd(fp_dst, 0);
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

        ref_ptr = ptr+2;
        *ptr++ = ldr_offset(15, 12, (intptr_t)adr_cos - (intptr_t)ref_ptr);
        *ptr++ = blx_cc_reg(ARM_CC_AL, 12);
        *ptr++ = fnegd(fp_dst, 0);
        exit_3 = ptr;
        *ptr++ = b_cc(ARM_CC_AL, 0);
        *tmp_ptr |= INSN_TO_LE(ptr - tmp_ptr - 2);

        /* Range 4: 1.5Pi..2Pi, cos(x) = sin(x-1.5Pi) */
        *ptr++ = fldd(fp_tmp1, base_reg, C_1_5 * 2);
        *ptr++ = fsubd(0, 0, fp_tmp1);

        ref_ptr = ptr+2;
        *ptr++ = ldr_offset(15, 12, (intptr_t)adr_sin - (intptr_t)ref_ptr);
        *ptr++ = blx_cc_reg(ARM_CC_AL, 12);
        *ptr++ = fcpyd(fp_dst, 0);

        *exit_1 |= INSN_TO_LE(ptr - exit_1 - 2);
        *exit_2 |= INSN_TO_LE(ptr - exit_2 - 2);
        *exit_3 |= INSN_TO_LE(ptr - exit_3 - 2);

        RA_FreeFPURegister(&ptr, fp_src);
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
