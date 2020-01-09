/*
    Copyright © 2019 Michal Schulz <michal.schulz@gmx.de>
    https://github.com/michalsc

    This Source Code Form is subject to the terms of the
    Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed
    with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "ARM.h"
#include "M68k.h"
#include "RegisterAllocator.h"
#include "EmuFeatures.h"
#include "lists.h"
#include "tlsf.h"

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
    C_10P4096,

    C_TWO54,
    C_LN2HI,
    C_LN2LO,
    C_LG1,
    C_LG2,
    C_LG3,
    C_LG4,
    C_LG5,
    C_LG6,
    C_LG7
};

static long double const constants[128] = {
    [C_PI] =        3.14159265358979323846264338327950288, /* Official */
    [C_PI_2] =      1.57079632679489661923132169163975144,
    [C_PI_4] =      0.785398163397448309615660845819875721,
    [C_1_PI] =      0.318309886183790671537767526745028724,
    [C_2_PI] =      0.636619772367581343075535053490057448,
    [C_2_SQRTPI] =  1.12837916709551257389615890312154517,
    [C_SQRT2] =     1.41421356237309504880168872420969808,
    [C_SQRT1_2] =   0.707106781186547524400844362104849039,
    [C_0_5] =       0.5,
    [C_1_5] =       1.5,
    [C_LOG10_2] =   0.301029995663981195214, /* Official - Log10(2) */
    [C_E] =         2.71828182845904523536028747135266250,                /* Official */
    [C_LOG2E] =     1.44269504088896340735992468100189214,            /* Official */
    [C_LOG10E] =    0.434294481903251827651128918916605082,           /* Official */
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

    [C_LN2] =       0.693147180559945309417232121458176568,              /* Official */
    [C_LN10] =      2.30258509299404568401799145468436421,             /* Official */
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
    [C_10P512] =    0x1.fffffep+127f,           /* Official 1E512 - too large for double! */
    [C_10P1024] =   0x1.fffffep+127f,           /* Official 1E1024 - too large for double! */
    [C_10P2048] =   0x1.fffffep+127f,           /* Official 1E2048 - too large for double! */
    [C_10P4096] =   0x1.fffffep+127f,           /* Official 1E4096 - too large for double! */

    [C_TWO54] =     1.80143985094819840000e+16,
    [C_LN2HI] =     6.93147180369123816490e-01,
    [C_LN2LO] =     1.90821492927058770002e-10,

    [C_LG1] =       6.666666666666735130e-01,
    [C_LG2] =       3.999999999940941908e-01,
    [C_LG3] =       2.857142874366239149e-01,
    [C_LG4] =       2.222219843214978396e-01,
    [C_LG5] =       1.818357216161805012e-01,
    [C_LG6] =       1.531383769920937332e-01,
    [C_LG7] =       1.479819860511658591e-01,
};


/* Parts of this file are copied from libm implementation by Sun Microsystems */

/*
 * ====================================================
 * Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
 *
 * Developed at SunPro, a Sun Microsystems, Inc. business.
 * Permission to use, copy, modify, and distribute this
 * software is freely granted, provided that this notice
 * is preserved.
 * ====================================================
 */

double __ieee754_log(double x)
{
    double hfsq,f,s,z,R,w,t1,t2,dk;
    int32_t k,hx,i,j;
    uint32_t lx;

    union {
        uint64_t i;
        uint32_t i32[2];
        double d;
    } n;

    n.d = x;
    hx = n.i32[0];
    lx = n.i32[1];

    k=0;
    if (hx < 0x00100000) {              /* x < 2**-1022  */
        if (((hx&0x7fffffff)|lx)==0)
            return -constants[C_TWO54]/constants[C_ZERO];		/* log(+-0)=-inf */
        if (hx<0) return (x-x)/constants[C_ZERO];	/* log(-#) = NaN */
        k -= 54; x *= constants[C_TWO54]; /* subnormal number, scale up x */
        n.d = x;
        hx = n.i32[0];
    }
    if (hx >= 0x7ff00000) return x+x;
    k += (hx>>20)-1023;
    hx &= 0x000fffff;
    i = (hx+0x95f64)&0x100000;
    n.i32[0] = hx|(i^0x3ff00000);	/* normalize x or x/2 */
    x = n.d;
    k += (i>>20);
    f = x-1.0;
    if((0x000fffff&(2+hx))<3) {	/* |f| < 2**-20 */
          if(f==constants[C_ZERO]) { if(k==0) return constants[C_ZERO];  else {dk=(double)k;
                               return dk*constants[C_LN2HI]+dk*constants[C_LN2LO];}}
        R = f*f*(0.5-0.33333333333333333*f);
        if(k==0) return f-R; else {dk=(double)k;
                 return dk*constants[C_LN2HI]-((R-dk*constants[C_LN2LO])-f);}
    }
    s = f/(2.0+f);
    dk = (double)k;
    z = s*s;
	i = hx-0x6147a;
	w = z*z;
	j = 0x6b851-hx;
	t1= w*(constants[C_LG2]+w*(constants[C_LG4]+w*constants[C_LG6]));
	t2= z*(constants[C_LG1]+w*(constants[C_LG3]+w*(constants[C_LG5]+w*constants[C_LG7])));
	i |= j;
	R = t2+t1;
	if(i>0) {
	    hfsq=0.5*f*f;
	    if(k==0) return f-(hfsq-s*(hfsq+R)); else
		     return dk*constants[C_LN2HI]-((hfsq-(s*(hfsq+R)+dk*constants[C_LN2LO]))-f);
	} else {
	    if(k==0) return f-s*(f-R); else
		     return dk*constants[C_LN2HI]-((s*(f-R)-dk*constants[C_LN2LO])-f);
	}
}

/* End of Sun Microsystems part */

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
    SIZE_B = 6,
};

uint8_t FPUDataSize[] = {
    [SIZE_L] = 4,
    [SIZE_S] = 4,
    [SIZE_X] = 12,
    [SIZE_P] = 12,
    [SIZE_W] = 2,
    [SIZE_D] = 8,
    [SIZE_B] = 1
};

int FPSR_Update_Needed(uint16_t **m68k_ptr)
{
    uint16_t opcode = BE16((*m68k_ptr)[0]);
    uint16_t opcode2 = BE16((*m68k_ptr)[1]);

    /* If next opcode is not LineF then we need to update FPSR */
    if ((opcode & 0xfe00) != 0xf200)
    {
        return 1;
    }
    else
    {
        /*
            Update FPSR condition codes only if subsequent FPU instruction
            is one of following: FBcc, FDBcc, FMOVEM, FScc, FTRAPcc
        */
        if ((opcode & 0xff80) == 0xf280)    /* FBcc */
            return 1;
        if ((opcode & 0xfff8) == 0xf248 && (opcode2 & 0xffc0) == 0) /* FDBcc */
            return 1;
        if ((opcode & 0xffc0) == 0xf200 && (opcode2 & 0xc700) == 0xc000) /* FMOVEM */
            return 1;
        if ((opcode & 0xffc0) == 0xf240 && (opcode2 & 0xffc0) == 0) /* FScc */
            return 1;
        if ((opcode & 0xfff8) == 0xf278 && (opcode2 & 0xffc0) == 0) /* FTRAPcc */
            return 1;
    }

    return 0;
}

/* Allocates FPU register and fetches data according to the R/M field of the FPU opcode */
uint32_t *FPU_FetchData(uint32_t *ptr, uint16_t **m68k_ptr, uint8_t *reg, uint16_t opcode,
        uint16_t opcode2, uint8_t *ext_count)
{
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

        /* The regtister was not yet assigned? assign it to a temporary reg now */
        if (*reg == 0xff)
            *reg = RA_AllocFPURegister(&ptr);

        uint8_t ea = opcode & 0x3f;
        enum FPUOpSize size = (opcode2 >> 10) & 7;
        int32_t imm_offset = 0;

        /* Case 1: mode 000 - Dn */
        if ((ea & 0x38) == 0)
        {
            uint8_t int_reg = 0xff;
            uint8_t tmp_reg;

            switch (size)
            {
                /* Single - move to single half of the reg, convert to double */
                case SIZE_S:
                    ptr = EMIT_LoadFromEffectiveAddress(ptr, 4, &int_reg, ea, *m68k_ptr, ext_count, 1, NULL);
                    *ptr++ = fmsr(*reg * 2, int_reg);
                    *ptr++ = fcvtds(*reg, *reg * 2);
                    RA_FreeARMRegister(&ptr, int_reg);
                    break;

                case SIZE_L:
                    ptr = EMIT_LoadFromEffectiveAddress(ptr, 4, &int_reg, ea, *m68k_ptr, ext_count, 1, NULL);
                    *ptr++ = fmsr(*reg * 2, int_reg);
                    *ptr++ = fsitod(*reg, *reg * 2);
                    RA_FreeARMRegister(&ptr, int_reg);
                    break;

                case SIZE_W:
                    ptr = EMIT_LoadFromEffectiveAddress(ptr, 2, &int_reg, ea, *m68k_ptr, ext_count, 1, NULL);
                    tmp_reg = RA_AllocARMRegister(&ptr);
                    *ptr++ = sxth(tmp_reg, int_reg, 0);
                    *ptr++ = fmsr(*reg * 2, tmp_reg);
                    *ptr++ = fsitod(*reg, *reg * 2);
                    RA_FreeARMRegister(&ptr, tmp_reg);
                    RA_FreeARMRegister(&ptr, int_reg);
                    break;

                case SIZE_B:
                    ptr = EMIT_LoadFromEffectiveAddress(ptr, 1, &int_reg, ea, *m68k_ptr, ext_count, 1, NULL);
                    tmp_reg = RA_AllocARMRegister(&ptr);
                    *ptr++ = sxtb(tmp_reg, int_reg, 0);
                    *ptr++ = fmsr(*reg * 2, tmp_reg);
                    *ptr++ = fsitod(*reg, *reg * 2);
                    RA_FreeARMRegister(&ptr, tmp_reg);
                    RA_FreeARMRegister(&ptr, int_reg);
                    break;

                default:
                    kprintf("[JIT] LineF: wrong argument size %d for Dn access at %08x\n", (int)size, *m68k_ptr-1);
            }
        }
        /* Case 2: mode 111:100 - immediate */
        else if (ea == 0x3c)
        {
            /* Fetch data *or* pointer to data into int_reg */
            uint8_t int_reg = 0xff;
            int not_yet_done = 0;

            switch (size)
            {
                case SIZE_S:
                    ptr = EMIT_LoadFromEffectiveAddress(ptr, 4, &int_reg, ea, *m68k_ptr, ext_count, 0, NULL);
                    *ptr++ = fmsr(*reg * 2, int_reg);
                    *ptr++ = fcvtds(*reg, *reg * 2);
                    break;
                case SIZE_L:
                    ptr = EMIT_LoadFromEffectiveAddress(ptr, 4, &int_reg, ea, *m68k_ptr, ext_count, 0, NULL);
                    *ptr++ = fmsr(*reg * 2, int_reg);
                    *ptr++ = fsitod(*reg, *reg * 2);
                    break;
                case SIZE_W:
                    ptr = EMIT_LoadFromEffectiveAddress(ptr, 2, &int_reg, ea, *m68k_ptr, ext_count, 0, NULL);
                    *ptr++ = sxth(int_reg, int_reg, 0);
                    *ptr++ = fmsr(*reg * 2, int_reg);
                    *ptr++ = fsitod(*reg, *reg * 2);
                    break;
                case SIZE_B:
                    ptr = EMIT_LoadFromEffectiveAddress(ptr, 1, &int_reg, ea, *m68k_ptr, ext_count, 0, NULL);
                    *ptr++ = sxtb(int_reg, int_reg, 0);
                    *ptr++ = fmsr(*reg * 2, int_reg);
                    *ptr++ = fsitod(*reg, *reg * 2);
                    break;
                default:
                    ptr = EMIT_LoadFromEffectiveAddress(ptr, 0, &int_reg, ea, *m68k_ptr, ext_count, 0, NULL);
                    not_yet_done = 1;
                    break;
            }

            /* if data not yet in the reg, use the address to load it into FPU register */
            if (not_yet_done)
            {
                switch(size)
                {
                    case SIZE_D:
                        if (((uint32_t)(*m68k_ptr + *ext_count)) & 0x3) {
                            uint8_t tmp1 = RA_AllocARMRegister(&ptr);
                            uint8_t tmp2 = RA_AllocARMRegister(&ptr);

                            *ptr++ = ldr_offset(int_reg, tmp1, 0);
                            *ptr++ = ldr_offset(int_reg, tmp2, 4);
                            *ptr++ = fmdrr(*reg, tmp1, tmp2);

                            RA_FreeARMRegister(&ptr, tmp2);
                            RA_FreeARMRegister(&ptr, tmp1);
                        }
                        else
                            *ptr++ = fldd(*reg, int_reg, 0);
                        *ext_count += 4;
                        break;

                    case SIZE_X:
                        {
                            kprintf("extended precision load!\n");
                            uint8_t tmp1 = RA_AllocARMRegister(&ptr);
                            uint8_t tmp2 = RA_AllocARMRegister(&ptr);
                            /* Extended format. First get the 64-bit mantissa and fit it into 52 bit fraction */
                            *ptr++ = ldr_offset(int_reg, tmp2, 4);
                            *ptr++ = ldr_offset(int_reg, tmp1, 8);

                            /* Shift right the second word of mantissa */
                            *ptr++ = lsr_immed(tmp1, tmp1, 11);
                            /* Insert first 11 bit of first word of mantissa */
                            *ptr++ = bfi(tmp1, tmp2, 21, 11);

                            /* Load lower half word of destination double type */
                            *ptr++ = fmdlr(*reg, tmp1);

                            /* Shift right upper part of mantissa, clear explicit bit */
                            *ptr++ = bic_immed(tmp2, tmp2, 0x102);
                            *ptr++ = lsr_immed(tmp2, tmp2, 11);

                            /* Get exponent, extract sign bit  */
                            *ptr++ = ldrh_offset(int_reg, tmp1, 0);
                            *ptr++ = tst_immed(tmp1, 0xc80);

                            /* Remove bias of exponent (16383) and add bias of double eponent (1023) */
                            *ptr++ = sub_immed(tmp1, tmp1, 0xc3c);

                            /* Insert exponent into upper part of first double word, insert sign */
                            *ptr++ = bfi(tmp2, tmp1, 20, 11);
                            *ptr++ = orr_cc_immed(ARM_CC_NE, tmp2, tmp2, 0x102);

                            /* Load upper half into destination double */
                            *ptr++ = fmdhr(*reg, tmp2);

                            RA_FreeARMRegister(&ptr, tmp1);
                            RA_FreeARMRegister(&ptr, tmp2);
                        }
                        *ext_count += 6;
                        break;

                    case SIZE_P:
                        kprintf("Packed load!\n");
                        *ext_count += 6;
                        break;

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
            uint8_t mode = (opcode & 0x0038) >> 3;

            if (mode == 4 || mode == 3)
                ptr = EMIT_LoadFromEffectiveAddress(ptr, 0, &int_reg, opcode & 0x3f, *m68k_ptr, ext_count, 0, NULL);
            else
                ptr = EMIT_LoadFromEffectiveAddress(ptr, 0, &int_reg, opcode & 0x3f, *m68k_ptr, ext_count, 1, &imm_offset);

            /* Pre index? Adjust base register accordingly */
            if (mode == 4) {
                uint8_t pre_sz = FPUDataSize[size];

                if ((pre_sz == 1) && ((opcode & 7) == 7))
                    pre_sz = 2;

                *ptr++ = sub_immed(int_reg, int_reg, pre_sz);

                RA_SetDirtyM68kRegister(&ptr, 8 + (opcode & 7));
            }

            switch (size)
            {
                case SIZE_X:
                    {
                        if (imm_offset < -4095 || imm_offset > 4095-8) {
                            uint8_t off = RA_AllocARMRegister(&ptr);

                            *ptr++ = movw_immed_u16(off, (imm_offset) & 0xffff);
                            imm_offset >>= 16;
                            if (imm_offset)
                                *ptr++ = movt_immed_u16(off, (imm_offset) & 0xffff);
                            *ptr++ = add_reg(off, int_reg, off, 0);

                            RA_FreeARMRegister(&ptr, int_reg);
                            int_reg = off;
                            imm_offset = 0;
                        }

                        kprintf("extended precision load from EA!\n");
                        uint8_t tmp1 = RA_AllocARMRegister(&ptr);
                        uint8_t tmp2 = RA_AllocARMRegister(&ptr);
                        /* Extended format. First get the 64-bit mantissa and fit it into 52 bit fraction */
                        *ptr++ = ldr_offset(int_reg, tmp2, imm_offset + 4);
                        *ptr++ = ldr_offset(int_reg, tmp1, imm_offset + 8);

                        /* Shift right the second word of mantissa */
                        *ptr++ = lsr_immed(tmp1, tmp1, 11);
                        /* Insert first 11 bit of first word of mantissa */
                        *ptr++ = bfi(tmp1, tmp2, 21, 11);

                        /* Load lower half word of destination double type */
                        *ptr++ = fmdlr(*reg, tmp1);

                        /* Shift right upper part of mantissa, clear explicit bit */
                        *ptr++ = bic_immed(tmp2, tmp2, 0x102);
                        *ptr++ = lsr_immed(tmp2, tmp2, 11);

                        /* Get exponent, extract sign bit  */
                        *ptr++ = ldr_offset(int_reg, tmp1, imm_offset);
                        *ptr++ = lsr_immed(tmp1, tmp1, 16);
                        *ptr++ = tst_immed(tmp1, 0xc80);

                        /* Remove bias of exponent (16383) and add bias of double eponent (1023) */
                        *ptr++ = sub_immed(tmp1, tmp1, 0xc3c);

                        /* Insert exponent into upper part of first double word, insert sign */
                        *ptr++ = bfi(tmp2, tmp1, 20, 11);
                        *ptr++ = orr_cc_immed(ARM_CC_NE, tmp2, tmp2, 0x102);

                        /* Load upper half into destination double */
                        *ptr++ = fmdhr(*reg, tmp2);

                        RA_FreeARMRegister(&ptr, tmp1);
                        RA_FreeARMRegister(&ptr, tmp2);
                    }
                    break;
                case SIZE_D:
                    {
                        uint8_t tmp1 = RA_AllocARMRegister(&ptr);
                        uint8_t tmp2 = RA_AllocARMRegister(&ptr);

                        if (Options.M68K_ALLOW_UNALIGNED_FPU) {
                            if (imm_offset < -4095 || imm_offset > 4095-4) {
                                uint8_t off = RA_AllocARMRegister(&ptr);

                                *ptr++ = movw_immed_u16(off, (imm_offset) & 0xffff);
                                imm_offset >>= 16;
                                if (imm_offset)
                                    *ptr++ = movt_immed_u16(off, (imm_offset) & 0xffff);
                                *ptr++ = add_reg(off, int_reg, off, 0);

                                RA_FreeARMRegister(&ptr, int_reg);
                                int_reg = off;
                                imm_offset = 0;
                            }

                            *ptr++ = ldr_offset(int_reg, tmp1, imm_offset);
                            *ptr++ = ldr_offset(int_reg, tmp2, imm_offset + 4);
                            *ptr++ = fmdrr(*reg, tmp1, tmp2);
                        } else {
                            if ((imm_offset & 3) || imm_offset < -1023 || imm_offset > 1023) {
                                uint8_t off = RA_AllocARMRegister(&ptr);

                                *ptr++ = movw_immed_u16(off, (imm_offset) & 0xffff);
                                imm_offset >>= 16;
                                if (imm_offset)
                                    *ptr++ = movt_immed_u16(off, (imm_offset) & 0xffff);
                                *ptr++ = add_reg(off, int_reg, off, 0);

                                RA_FreeARMRegister(&ptr, int_reg);
                                int_reg = off;
                                imm_offset = 0;
                            }
                            *ptr++ = fldd(*reg, int_reg, imm_offset >> 2);
                        }

                        RA_FreeARMRegister(&ptr, tmp2);
                        RA_FreeARMRegister(&ptr, tmp1);
                    }
                    break;
                case SIZE_S:
                    if ((imm_offset & 3) || imm_offset < -1023 || imm_offset > 1023) {
                        uint8_t off = RA_AllocARMRegister(&ptr);

                        *ptr++ = movw_immed_u16(off, (imm_offset) & 0xffff);
                        imm_offset >>= 16;
                        if (imm_offset)
                            *ptr++ = movt_immed_u16(off, (imm_offset) & 0xffff);
                        *ptr++ = add_reg(off, int_reg, off, 0);

                        RA_FreeARMRegister(&ptr, int_reg);
                        int_reg = off;
                        imm_offset = 0;
                    }
                    *ptr++ = flds(*reg * 2, int_reg, imm_offset >> 2);
                    *ptr++ = fcvtds(*reg, *reg * 2);
                    break;
                case SIZE_L:
                    if (imm_offset < -4095 || imm_offset > 4095) {
                        uint8_t off = RA_AllocARMRegister(&ptr);

                        *ptr++ = movw_immed_u16(off, (imm_offset) & 0xffff);
                        imm_offset >>= 16;
                        if (imm_offset)
                            *ptr++ = movt_immed_u16(off, (imm_offset) & 0xffff);
                        *ptr++ = add_reg(off, int_reg, off, 0);

                        RA_FreeARMRegister(&ptr, int_reg);
                        int_reg = off;
                        imm_offset = 0;
                    }

                    val_reg = RA_AllocARMRegister(&ptr);
                    *ptr++ = ldr_offset(int_reg, val_reg, imm_offset);
                    *ptr++ = fmsr(*reg * 2, val_reg);
                    *ptr++ = fsitod(*reg, *reg * 2);
                    break;
                case SIZE_W:
                    if (imm_offset < -255 || imm_offset > 255) {
                        uint8_t off = RA_AllocARMRegister(&ptr);

                        *ptr++ = movw_immed_u16(off, (imm_offset) & 0xffff);
                        imm_offset >>= 16;
                        if (imm_offset)
                            *ptr++ = movt_immed_u16(off, (imm_offset) & 0xffff);
                        *ptr++ = add_reg(off, int_reg, off, 0);

                        RA_FreeARMRegister(&ptr, int_reg);
                        int_reg = off;
                        imm_offset = 0;
                    }
                    val_reg = RA_AllocARMRegister(&ptr);
                    *ptr++ = ldrsh_offset(int_reg, val_reg, imm_offset);
                    *ptr++ = fmsr(*reg * 2, val_reg);
                    *ptr++ = fsitod(*reg, *reg * 2);
                    break;
                case SIZE_B:
                    if (imm_offset < -4095 || imm_offset > 4095) {
                        uint8_t off = RA_AllocARMRegister(&ptr);

                        *ptr++ = movw_immed_u16(off, (imm_offset) & 0xffff);
                        imm_offset >>= 16;
                        if (imm_offset)
                            *ptr++ = movt_immed_u16(off, (imm_offset) & 0xffff);
                        *ptr++ = add_reg(off, int_reg, off, 0);

                        RA_FreeARMRegister(&ptr, int_reg);
                        int_reg = off;
                        imm_offset = 0;
                    }
                    val_reg = RA_AllocARMRegister(&ptr);
                    *ptr++ = ldrsb_offset(int_reg, val_reg, imm_offset);
                    *ptr++ = fmsr(*reg * 2, int_reg);
                    *ptr++ = fsitod(*reg, *reg * 2);
                    break;
                default:
                    break;
            }

            /* Post index? Adjust base register accordingly */
            if (mode == 3) {
                uint8_t post_sz = FPUDataSize[size];

                if ((post_sz == 1) && ((opcode & 7) == 7))
                    post_sz = 2;

                *ptr++ = add_immed(int_reg, int_reg, post_sz);

                RA_SetDirtyM68kRegister(&ptr, 8 + (opcode & 7));
            }

            RA_FreeARMRegister(&ptr, int_reg);
            RA_FreeARMRegister(&ptr, val_reg);
        }
    }

    return ptr;
}

/* Allocates FPU register and fetches data according to the R/M field of the FPU opcode */
uint32_t *FPU_StoreData(uint32_t *ptr, uint16_t **m68k_ptr, uint8_t reg, uint16_t opcode,
        uint16_t opcode2, uint8_t *ext_count)
{
    /*
        Store is always to memory, R/M was set to 1, the source is FPU register, target is
        defined by EA stored in first part of the opcode. Destination identifier specifies
        the data length.

        Get EA, eventually (in case of mode 000 - Dn) perform simple data transfer.
        Otherwise get address from EA and store data here
    */

    uint8_t ea = opcode & 0x3f;
    enum FPUOpSize size = (opcode2 >> 10) & 7;
    int32_t imm_offset = 0;

    /* Case 1: mode 000 - Dn */
    if ((ea & 0x38) == 0)
    {
        uint8_t int_reg = 0xff;
        uint8_t tmp_reg = 0xff;
        uint8_t vfp_reg = RA_AllocFPURegister(&ptr);

        switch (size)
        {
            case SIZE_S:
                int_reg = RA_MapM68kRegisterForWrite(&ptr, ea & 7); // Destination for write only, discard contents
                *ptr++ = fcvtsd(vfp_reg * 2, reg);                  // Convert double to single
                *ptr++ = fmrs(int_reg, vfp_reg * 2);                // Move single to destination ARM reg
                RA_FreeARMRegister(&ptr, int_reg);
                break;

            case SIZE_L:
                int_reg = RA_MapM68kRegisterForWrite(&ptr, ea & 7); // Destination for write only, discard contents
                *ptr++ = ftosid(vfp_reg * 2, reg);                  // Convert double to signed integer
                *ptr++ = fmrs(int_reg, vfp_reg * 2);                // Move signed int to destination ARM reg
                RA_FreeARMRegister(&ptr, int_reg);
                break;

            case SIZE_W:
                int_reg = RA_MapM68kRegister(&ptr, ea & 7);
                tmp_reg = RA_AllocARMRegister(&ptr);
                *ptr++ = ftosid(vfp_reg * 2, reg);                  // Convert double to signed integer
                *ptr++ = fmrs(tmp_reg, vfp_reg * 2);                // Move signed int to temporary ARM reg
                *ptr++ = bfi(int_reg, tmp_reg, 0, 16);              // Insert lower 16 bits of temporary into target
                RA_SetDirtyM68kRegister(&ptr, ea & 7);
                RA_FreeARMRegister(&ptr, tmp_reg);
                RA_FreeARMRegister(&ptr, int_reg);
                break;

            case SIZE_B:
                int_reg = RA_MapM68kRegister(&ptr, ea & 7);
                tmp_reg = RA_AllocARMRegister(&ptr);
                *ptr++ = ftosid(vfp_reg * 2, reg);                  // Convert double to signed integer
                *ptr++ = fmrs(tmp_reg, vfp_reg * 2);                // Move signed int to temporary ARM reg
                *ptr++ = bfi(int_reg, tmp_reg, 0, 8);               // Insert lower 16 bits of temporary into target
                RA_SetDirtyM68kRegister(&ptr, ea & 7);
                RA_FreeARMRegister(&ptr, tmp_reg);
                RA_FreeARMRegister(&ptr, int_reg);
                break;

            default:
                kprintf("[JIT] LineF: wrong argument size %d for Dn access\n", (int)size);
        }

        RA_FreeFPURegister(&ptr, vfp_reg);
    }
    /* Case 2: get pointer to data (EA) and store yourself */
    else
    {
        uint8_t int_reg = 0xff;
        uint8_t val_reg = 0xff;
        uint8_t mode = (opcode & 0x0038) >> 3;
        uint8_t vfp_reg = RA_AllocFPURegister(&ptr);

        if (mode == 4 || mode == 3)
            ptr = EMIT_LoadFromEffectiveAddress(ptr, 0, &int_reg, opcode & 0x3f, *m68k_ptr, ext_count, 0, NULL);
        else
            ptr = EMIT_LoadFromEffectiveAddress(ptr, 0, &int_reg, opcode & 0x3f, *m68k_ptr, ext_count, 1, &imm_offset);

        /* Pre index? Adjust base register accordingly */
        if (mode == 4) {
            uint8_t pre_sz = FPUDataSize[size];

            if ((pre_sz == 1) && ((opcode & 7) == 7))
                pre_sz = 2;

            *ptr++ = sub_immed(int_reg, int_reg, pre_sz);

            RA_SetDirtyM68kRegister(&ptr, 8 + (opcode & 7));
        }

        switch (size)
        {
            case SIZE_X:
                {
                    kprintf("extended precision store to EA!\n");
#if 0
                    uint8_t tmp1 = RA_AllocARMRegister(&ptr);
                    uint8_t tmp2 = RA_AllocARMRegister(&ptr);
                    /* Extended format. First get the 64-bit mantissa and fit it into 52 bit fraction */
                    *ptr++ = ldr_offset(int_reg, tmp2, 4);
                    *ptr++ = ldr_offset(int_reg, tmp1, 8);

                    /* Shift right the second word of mantissa */
                    *ptr++ = lsr_immed(tmp1, tmp1, 11);
                    /* Insert first 11 bit of first word of mantissa */
                    *ptr++ = bfi(tmp1, tmp2, 21, 11);

                    /* Load lower half word of destination double type */
                    *ptr++ = fmdlr(*reg, tmp1);

                    /* Shift right upper part of mantissa, clear explicit bit */
                    *ptr++ = bic_immed(tmp2, tmp2, 0x102);
                    *ptr++ = lsr_immed(tmp2, tmp2, 11);

                    /* Get exponent, extract sign bit  */
                    *ptr++ = ldrh_offset(int_reg, tmp1, 0);
                    *ptr++ = tst_immed(tmp1, 0xc80);

                    /* Remove bias of exponent (16383) and add bias of double eponent (1023) */
                    *ptr++ = sub_immed(tmp1, tmp1, 0xc3c);

                    /* Insert exponent into upper part of first double word, insert sign */
                    *ptr++ = bfi(tmp2, tmp1, 20, 11);
                    *ptr++ = orr_cc_immed(ARM_CC_NE, tmp2, tmp2, 0x102);

                    /* Load upper half into destination double */
                    *ptr++ = fmdhr(*reg, tmp2);

                    RA_FreeARMRegister(&ptr, tmp1);
                    RA_FreeARMRegister(&ptr, tmp2);
#endif
                }
                break;
            case SIZE_D:
                {
                    uint8_t tmp1 = RA_AllocARMRegister(&ptr);
                    uint8_t tmp2 = RA_AllocARMRegister(&ptr);

                    if (Options.M68K_ALLOW_UNALIGNED_FPU) {
                        *ptr++ = fmrrd(tmp1, tmp2, reg);
                        if (imm_offset > -4096 && imm_offset < 4096-4) {
                            *ptr++ = str_offset(int_reg, tmp1, imm_offset);
                            *ptr++ = str_offset(int_reg, tmp2, imm_offset + 4);
                        } else {
                            uint8_t off = RA_AllocARMRegister(&ptr);

                            *ptr++ = movw_immed_u16(off, (imm_offset) & 0xffff);
                            imm_offset >>= 16;
                            if (imm_offset)
                                *ptr++ = movt_immed_u16(off, (imm_offset) & 0xffff);
                            *ptr++ = add_reg(off, int_reg, off, 0);

                            *ptr++ = str_offset(off, tmp1, 0);
                            *ptr++ = str_offset(off, tmp2, 4);

                            RA_FreeARMRegister(&ptr, off);
                        }
                    } else {
                        if ((imm_offset & 3) == 0 && imm_offset > -1024 && imm_offset < 1024) {
                            *ptr++ = fstd(reg, int_reg, imm_offset >> 2);
                        } else {
                            uint8_t off = RA_AllocARMRegister(&ptr);

                            *ptr++ = movw_immed_u16(off, (imm_offset) & 0xffff);
                            imm_offset >>= 16;
                            if (imm_offset)
                                *ptr++ = movt_immed_u16(off, (imm_offset) & 0xffff);
                            *ptr++ = add_reg(off, int_reg, off, 0);

                            *ptr++ = fstd(reg, off, 0);

                            RA_FreeARMRegister(&ptr, off);
                        }
                    }

                    RA_FreeARMRegister(&ptr, tmp2);
                    RA_FreeARMRegister(&ptr, tmp1);
                }
                break;
            case SIZE_S:
                *ptr++ = fcvtsd(vfp_reg * 2, reg);
                if ((imm_offset & 3) == 0 && imm_offset > -1024 && imm_offset < 1024) {
                    *ptr++ = fsts(vfp_reg * 2, int_reg, imm_offset >> 2);
                } else {
                    uint8_t off = RA_AllocARMRegister(&ptr);

                    *ptr++ = movw_immed_u16(off, (imm_offset) & 0xffff);
                    imm_offset >>= 16;
                    if (imm_offset)
                        *ptr++ = movt_immed_u16(off, (imm_offset) & 0xffff);
                    *ptr++ = add_reg(off, int_reg, off, 0);
                    *ptr++ = fsts(vfp_reg * 2, off, 0);

                    RA_FreeARMRegister(&ptr, off);
                }
                break;
            case SIZE_L:
                val_reg = RA_AllocARMRegister(&ptr);
                *ptr++ = ftosid(vfp_reg * 2, reg);
                *ptr++ = fmrs(val_reg, vfp_reg * 2);
                if (imm_offset > -4096 && imm_offset < 4096) {
                    *ptr++ = str_offset(int_reg, val_reg, imm_offset);
                } else {
                    uint8_t off = RA_AllocARMRegister(&ptr);

                    *ptr++ = movw_immed_u16(off, (imm_offset) & 0xffff);
                    imm_offset >>= 16;
                    if (imm_offset)
                        *ptr++ = movt_immed_u16(off, (imm_offset) & 0xffff);
                    *ptr++ = add_reg(off, int_reg, off, 0);
                    *ptr++ = str_offset(off, val_reg, 0);

                    RA_FreeARMRegister(&ptr, off);
                }
                break;
            case SIZE_W:
                val_reg = RA_AllocARMRegister(&ptr);
                *ptr++ = ftosid(vfp_reg * 2, reg);
                *ptr++ = fmrs(val_reg, vfp_reg * 2);
                if (imm_offset > -256 && imm_offset < 256) {
                    *ptr++ = strh_offset(int_reg, val_reg, imm_offset);
                } else {
                    uint8_t off = RA_AllocARMRegister(&ptr);

                    *ptr++ = movw_immed_u16(off, (imm_offset) & 0xffff);
                    imm_offset >>= 16;
                    if (imm_offset)
                        *ptr++ = movt_immed_u16(off, (imm_offset) & 0xffff);
                    *ptr++ = add_reg(off, int_reg, off, 0);
                    *ptr++ = strh_offset(off, val_reg, 0);

                    RA_FreeARMRegister(&ptr, off);
                }
                break;
            case SIZE_B:
                val_reg = RA_AllocARMRegister(&ptr);
                *ptr++ = ftosid(vfp_reg * 2, reg);
                *ptr++ = fmrs(val_reg, vfp_reg * 2);
                if (imm_offset > -4096 && imm_offset < 4096) {
                    *ptr++ = strb_offset(int_reg, val_reg, imm_offset);
                } else {
                    uint8_t off = RA_AllocARMRegister(&ptr);

                    *ptr++ = movw_immed_u16(off, (imm_offset) & 0xffff);
                    imm_offset >>= 16;
                    if (imm_offset)
                        *ptr++ = movt_immed_u16(off, (imm_offset) & 0xffff);
                    *ptr++ = add_reg(off, int_reg, off, 0);
                    *ptr++ = strb_offset(off, val_reg, 0);

                    RA_FreeARMRegister(&ptr, off);
                }
                break;
            default:
                break;
        }

        /* Post index? Adjust base register accordingly */
        if (mode == 3) {
            uint8_t post_sz = FPUDataSize[size];

            if ((post_sz == 1) && ((opcode & 7) == 7))
                post_sz = 2;

            *ptr++ = add_immed(int_reg, int_reg, post_sz);

            RA_SetDirtyM68kRegister(&ptr, 8 + (opcode & 7));
        }

        RA_FreeFPURegister(&ptr, vfp_reg);
        RA_FreeARMRegister(&ptr, int_reg);
        RA_FreeARMRegister(&ptr, val_reg);
    }

    return ptr;
}

/* Clean and invalidate entire data cache, code after ARMv7 architecture reference manual */
void __attribute__((naked)) clear_entire_dcache(void)
{
    asm volatile(
"   push {r0, r1, r2, r3, r4, r5, r7, r9, r10, r11, lr} \n"
"   mrc p15, 1, r0, c0, c0, 1   \n"     // Read CLIDR into R0
"   ands r3, r0, #0x07000000    \n"
"   mov r3, r3, lsr #23         \n"     // Cache level value (naturally aligned)
"   beq 5f                      \n"
"   mov r10, #0                 \n"
"1: add r2, r10, r10, lsr #1    \n"     // Work out 3 x cachelevel
"   mov r1, r0, lsr r2          \n"     // bottom 3 bits are the Cache type for this level
"   and r1, r1, #7              \n"     // get those 3 bits alone
"   cmp r1, #2                  \n"
"   blt 4f                      \n"     // no cache or only instruction cache at this level
"   mcr p15, 2, r10, c0, c0, 0  \n"     // write CSSELR from R10
"   isb                         \n"     // ISB to sync the change to the CCSIDR
"   mrc p15, 1, r1, c0, c0, 0   \n"     // read current CCSIDR to R1
"   and r2, r1, #7              \n"     // extract the line length field
"   add r2, r2, #4              \n"     // add 4 for the line length offset (log2 16 bytes)
"   ldr r4, =0x3FF              \n"
"   ands r4, r4, r1, lsr #3     \n"     // R4 is the max number on the way size (right aligned)
"   clz r5, r4                  \n"     // R5 is the bit position of the way size increment
"   mov r9, r4                  \n"     // R9 working copy of the max way size (right aligned)
"2: ldr r7, =0x00007FFF         \n"
"   ands r7, r7, r1, lsr #13    \n"     // R7 is the max number of the index size (right aligned)
"3: orr r11, r10, r9, lsl r5    \n"     // factor in the way number and cache number into R11
"   orr r11, r11, r7, lsl r2    \n"     // factor in the index number
"   mcr p15, 0, r11, c7, c14, 2 \n"     // clean and invalidate by set/way
"   subs r7, r7, #1             \n"     // decrement the index
"   bge 3b                      \n"
"   subs r9, r9, #1             \n"     // decrement the way number
"   bge 2b                      \n"
"4: add r10, r10, #2            \n"     // increment the cache number
"   cmp r3, r10                 \n"
"   bgt 1b                      \n"
"   dsb                         \n"
"5: pop {r0, r1, r2, r3, r4, r5, r7, r9, r10, r11, pc} \n"
"   .ltorg                      \n"
    );
}

/* Clean and invalidate entire data cache, code after ARMv7 architecture reference manual */
void __attribute__((naked)) invalidate_entire_dcache(void)
{
    asm volatile(
"   push {r4, r5, r7, r9, r10, r11, lr} \n"
"   mrc p15, 1, r0, c0, c0, 1   \n"     // Read CLIDR into R0
"   ands r3, r0, #0x07000000    \n"
"   mov r3, r3, lsr #23         \n"     // Cache level value (naturally aligned)
"   beq 5f                      \n"
"   mov r10, #0                 \n"
"1: add r2, r10, r10, lsr #1    \n"     // Work out 3 x cachelevel
"   mov r1, r0, lsr r2          \n"     // bottom 3 bits are the Cache type for this level
"   and r1, r1, #7              \n"     // get those 3 bits alone
"   cmp r1, #2                  \n"
"   blt 4f                      \n"     // no cache or only instruction cache at this level
"   mcr p15, 2, r10, c0, c0, 0  \n"     // write CSSELR from R10
"   isb                         \n"     // ISB to sync the change to the CCSIDR
"   mrc p15, 1, r1, c0, c0, 0   \n"     // read current CCSIDR to R1
"   and r2, r1, #7              \n"     // extract the line length field
"   add r2, r2, #4              \n"     // add 4 for the line length offset (log2 16 bytes)
"   ldr r4, =0x3FF              \n"
"   ands r4, r4, r1, lsr #3     \n"     // R4 is the max number on the way size (right aligned)
"   clz r5, r4                  \n"     // R5 is the bit position of the way size increment
"   mov r9, r4                  \n"     // R9 working copy of the max way size (right aligned)
"2: ldr r7, =0x00007FFF         \n"
"   ands r7, r7, r1, lsr #13    \n"     // R7 is the max number of the index size (right aligned)
"3: orr r11, r10, r9, lsl r5    \n"     // factor in the way number and cache number into R11
"   orr r11, r11, r7, lsl r2    \n"     // factor in the index number
"   mcr p15, 0, r11, c7, c6, 2  \n"     // invalidate by set/way
"   subs r7, r7, #1             \n"     // decrement the index
"   bge 3b                      \n"
"   subs r9, r9, #1             \n"     // decrement the way number
"   bge 2b                      \n"
"4: add r10, r10, #2            \n"     // increment the cache number
"   cmp r3, r10                 \n"
"   bgt 1b                      \n"
"   dsb                         \n"
"5: pop {r4, r5, r7, r9, r10, r11, pc} \n"
"   .ltorg                      \n"
    );
}

void __clear_cache(void *begin, void *end);

uint32_t icache_epilogue[64];

void *invalidate_instruction_cache(uintptr_t target_addr, uint16_t *pc, uint32_t *arm_pc)
{
    int i;
    uint16_t opcode = BE16(pc[0]);
    struct M68KTranslationUnit *u;
    struct Node *n, *next;
    extern struct List LRU;
    extern void *handle;
    extern uint32_t last_PC;

    kprintf("[LINEF] ICache flush... Opcode=%04x, Target=%08x, PC=%08x, ARM PC=%08x\n", opcode, target_addr, pc, arm_pc);
    kprintf("[LINEF] ARM insn: %08x\n", *arm_pc);

    for (i=0; i < 64; i++)
    {
        if (arm_pc[i] == 0xffffffff)
            break;

        icache_epilogue[i] = arm_pc[i];
    }

    kprintf("[LINEF] Copied %d instructions of epilogue\n", i);
    __clear_cache(&icache_epilogue[0], &icache_epilogue[i]);

    last_PC = 0xffffffff;

    /* Get the scope */
    switch (opcode & 0x18) {
        case 0x08:  /* Line */
            kprintf("[LINEF] Invalidating line\n");
            ForeachNodeSafe(&LRU, n, next)
            {
                u = (struct M68KTranslationUnit *)((intptr_t)n - __builtin_offsetof(struct M68KTranslationUnit, mt_LRUNode));
                kprintf("[LINEF] Unit %08x, %08x-%08x\n", u, u->mt_M68kLow, u->mt_M68kHigh);

                // If highest address of unit is lower than the begin flushed area, or lowest address of unit higher than the flushed area end
                // then skip the unit
                if ((uintptr_t)u->mt_M68kLow > ((target_addr + 16) & ~15) || (uintptr_t)u->mt_M68kHigh < (target_addr & ~15))
                    continue;

                kprintf("[LINEF] Unit match! Removing.\n");
                REMOVE(&u->mt_LRUNode);
                REMOVE(&u->mt_HashNode);
                tlsf_free(handle, u);
            }
            break;
        case 0x10:  /* Page */
            kprintf("[LINEF] Invalidating page\n");
            ForeachNodeSafe(&LRU, n, next)
            {
                u = (struct M68KTranslationUnit *)((intptr_t)n - __builtin_offsetof(struct M68KTranslationUnit, mt_LRUNode));
                kprintf("[LINEF] Unit %08x, %08x-%08x\n", u, u->mt_M68kLow, u->mt_M68kHigh);

                // If highest address of unit is lower than the begin flushed area, or lowest address of unit higher than the flushed area end
                // then skip the unit
                if ((uintptr_t)u->mt_M68kLow > ((target_addr + 4096) & ~4095) || (uintptr_t)u->mt_M68kHigh < (target_addr & ~4095))
                    continue;

                kprintf("[LINEF] Unit match! Removing.\n");
                REMOVE(&u->mt_LRUNode);
                REMOVE(&u->mt_HashNode);
                tlsf_free(handle, u);
            }
            break;
        case 0x18:  /* All */
            kprintf("[LINEF] Invalidating all\n");
            while ((n = REMHEAD(&LRU))) {
                u = (struct M68KTranslationUnit *)((intptr_t)n - __builtin_offsetof(struct M68KTranslationUnit, mt_LRUNode));
                kprintf("[LINEF] Removing unit %08x\n", u);
                REMOVE(&u->mt_HashNode);
                tlsf_free(handle, u);
            }
            break;
    }

    return &icache_epilogue[0];
}

void __attribute__((naked)) trampoline_icache_invalidate(void)
{
    asm volatile("bl invalidate_instruction_cache\n\tbx r0");
}

uint32_t *EMIT_lineF(uint32_t *ptr, uint16_t **m68k_ptr)
{
    uint16_t opcode = BE16((*m68k_ptr)[0]);
    uint16_t opcode2 = BE16((*m68k_ptr)[1]);
    uint8_t ext_count = 1;
    (*m68k_ptr)++;

    /* CINV */
    if ((opcode & 0xff20) == 0xf400)
    {
        uint8_t tmp = 0xff;
        uint8_t tmp2 = 0xff;
        ext_count = 0;

        /* Invalidating data cache? */
        if (opcode & 0x40) {
            /* Get the scope */
            switch (opcode & 0x18) {
                case 0x08:  /* Line */
                    tmp = RA_CopyFromM68kRegister(&ptr, 8 + (opcode & 7));
                    *ptr++ = bic_immed(tmp, tmp, 0x1f);
                    *ptr++ = mcr(15, 0, tmp, 7, 6, 1); /* clean and invalidate data cache line */
                    *ptr++ = mov_immed_u8(tmp, 0);
                    *ptr++ = mcr(15, 0, tmp, 7, 10, 4); /* dsb */
                    RA_FreeARMRegister(&ptr, tmp);
                    break;
                case 0x10:  /* Page */
                    tmp = RA_CopyFromM68kRegister(&ptr, 8 + (opcode & 7));
                    tmp2 = RA_AllocARMRegister(&ptr);
                    *ptr++ = bic_immed(tmp, tmp, 0x0ff);
                    *ptr++ = bic_immed(tmp, tmp, 0xc0f);
                    *ptr++ = mov_immed_u8(tmp2, 128);
                    *ptr++ = mcr(15, 0, tmp, 7, 6, 1); /* clean and invalidate data cache line */
                    *ptr++ = add_immed(tmp, tmp, 32);
                    *ptr++ = subs_immed(tmp2, tmp2, 1);
                    *ptr++ = b_cc(ARM_CC_NE, -5);
                    *ptr++ = mcr(15, 0, tmp2, 7, 10, 4); /* dsb */
                    RA_FreeARMRegister(&ptr, tmp);
                    RA_FreeARMRegister(&ptr, tmp2);
                    break;
                case 0x18:  /* All */
                    *ptr++ = push(0x0f | (1 << 12));
                    *ptr++ = ldr_offset(15, 12, 8);
                    *ptr++ = blx_cc_reg(ARM_CC_AL, 12);
                    *ptr++ = pop(0x0f | (1 << 12));
                    *ptr++ = b_cc(ARM_CC_AL, 0);
                    *ptr++ = BE32((uint32_t)invalidate_entire_dcache);
                    break;
            }
        }
        /* Invalidating instruction cache? */
        if (opcode & 0x80) {
            int8_t off = 0;
            ptr = EMIT_GetOffsetPC(ptr, &off);
            kprintf("[LINEF] Inserting icache flush\n");

            if ((opcode & 0x18) == 0x08 || (opcode & 0x18) == 0x10)
            {
                uint8_t tmp = RA_MapM68kRegister(&ptr, 8 + (opcode & 7));
                *ptr++ = push(0x0f | (1 << 12));
                if (tmp != 0)
                    *ptr++ = mov_reg(0, tmp);
            }
            else
            {
                *ptr++ = push(0x0f | (1 << 12));
            }
            if (off >= 0)
                *ptr++ = add_immed(1, REG_PC, off);
            else
                *ptr++ = sub_immed(1, REG_PC, -off);
            *ptr++ = add_immed(2, 15, 4);
            *ptr++ = ldr_offset(15, 12, 8);
            *ptr++ = blx_cc_reg(ARM_CC_AL, 12);
            *ptr++ = pop(0x0f | (1 << 12));
            *ptr++ = b_cc(ARM_CC_AL, 0);
            *ptr++ = BE32((uint32_t)trampoline_icache_invalidate);
        }

        ptr = EMIT_AdvancePC(ptr, 2 * (ext_count + 1));

        /* If instruction cache was invalidated, then break translation after this instruction! */
        if (opcode & 0x80) {
            *ptr++ = INSN_TO_LE(0xffffffff);
        }

        *ptr++ = INSN_TO_LE(0xfffffff0);
    }
    /* CPUSH */
    else if ((opcode & 0xff20) == 0xf420)
    {
        uint8_t tmp = 0xff;
        uint8_t tmp2 = 0xff;
        ext_count = 0;

        /* Flush data cache? */
        if (opcode & 0x40) {
            /* Get the scope */
            switch (opcode & 0x18) {
                case 0x08:  /* Line */
                    tmp = RA_CopyFromM68kRegister(&ptr, 8 + (opcode & 7));
                    *ptr++ = bic_immed(tmp, tmp, 0x1f);
                    *ptr++ = mcr(15, 0, tmp, 7, 14, 1); /* clean and invalidate data cache line */
                    *ptr++ = mov_immed_u8(tmp, 0);
                    *ptr++ = mcr(15, 0, tmp, 7, 10, 4); /* dsb */
                    RA_FreeARMRegister(&ptr, tmp);
                    break;
                case 0x10:  /* Page */
                    tmp = RA_CopyFromM68kRegister(&ptr, 8 + (opcode & 7));
                    tmp2 = RA_AllocARMRegister(&ptr);
                    *ptr++ = bic_immed(tmp, tmp, 0x0ff);
                    *ptr++ = bic_immed(tmp, tmp, 0xc0f);
                    *ptr++ = mov_immed_u8(tmp2, 128);
                    *ptr++ = mcr(15, 0, tmp, 7, 14, 1); /* clean and invalidate data cache line */
                    *ptr++ = add_immed(tmp, tmp, 32);
                    *ptr++ = subs_immed(tmp2, tmp2, 1);
                    *ptr++ = b_cc(ARM_CC_NE, -5);
                    *ptr++ = mcr(15, 0, tmp2, 7, 10, 4); /* dsb */
                    RA_FreeARMRegister(&ptr, tmp);
                    RA_FreeARMRegister(&ptr, tmp2);
                    break;
                case 0x18:  /* All */
                    *ptr++ = push(0x0f | (1 << 12));
                    *ptr++ = ldr_offset(15, 12, 8);
                    *ptr++ = blx_cc_reg(ARM_CC_AL, 12);
                    *ptr++ = pop(0x0f | (1 << 12));
                    *ptr++ = b_cc(ARM_CC_AL, 0);
                    *ptr++ = BE32((uint32_t)clear_entire_dcache);
                    break;
            }
        }
        /* Invalidating instruction cache? */
        if (opcode & 0x80) {
            int8_t off = 0;
            ptr = EMIT_GetOffsetPC(ptr, &off);
            kprintf("[LINEF] Inserting icache flush\n");
            if ((opcode & 0x18) == 0x08 || (opcode & 0x18) == 0x10)
            {
                uint8_t tmp = RA_MapM68kRegister(&ptr, 8 + (opcode & 7));
                *ptr++ = push(0x0f | (1 << 12));
                if (tmp != 0)
                    *ptr++ = mov_reg(0, tmp);
            }
            else
            {
                *ptr++ = push(0x0f | (1 << 12));
            }
            if (off >= 0)
                *ptr++ = add_immed(1, REG_PC, off);
            else
                *ptr++ = sub_immed(1, REG_PC, -off);
            *ptr++ = add_immed(2, 15, 4);
            *ptr++ = ldr_offset(15, 12, 8);
            *ptr++ = blx_cc_reg(ARM_CC_AL, 12);
            *ptr++ = pop(0x0f | (1 << 12));
            *ptr++ = b_cc(ARM_CC_AL, 0);
            *ptr++ = BE32((uint32_t)trampoline_icache_invalidate);
        }

        ptr = EMIT_AdvancePC(ptr, 2 * (ext_count + 1));

        /* If instruction cache was invalidated, then break translation after this instruction! */
        if (opcode & 0x80) {
            *ptr++ = INSN_TO_LE(0xffffffff);
        }

        *ptr++ = INSN_TO_LE(0xfffffff0);
    }
    /* FMOVECR reg */
    else if (opcode == 0xf200 && (opcode2 & 0xfc00) == 0x5c00)
    {
        union {
            double d;
            uint32_t u32[2];
        } u;
        uint8_t fp_dst = (opcode2 >> 7) & 7;
        uint8_t offset = opcode2 & 0x7f;

        /* Alloc destination FP register for write */
        fp_dst = RA_MapFPURegisterForWrite(&ptr, fp_dst);

        /*
            Load pointer to constants into base register, then load the value from table into
            destination VFP register, finally skip the base address (which is not an ARM INSN)
        */
        if (offset == C_10P0) {
            *ptr++ = fmov_imm(fp_dst, 112);
        }
        else if (offset == C_ZERO) {
            *ptr++ = fmov_i64(fp_dst, 0, 0xe);
        }
        else {
            u.d = constants[offset];

            *ptr++ = fldd(fp_dst, 15, 0);
            *ptr++ = b_cc(ARM_CC_AL, 1);
            *ptr++ = BE32(u.u32[0]);
            *ptr++ = BE32(u.u32[1]);
        }
        ptr = EMIT_AdvancePC(ptr, 2 * (ext_count + 1));
        (*m68k_ptr) += ext_count;

        if (FPSR_Update_Needed(m68k_ptr))
        {
            uint8_t fpsr = M68K_ModifyFPSR(&ptr);

            *ptr++ = fcmpzd(fp_dst);
            *ptr++ = fmstat();
            *ptr++ = bic_immed(fpsr, fpsr, 0x40f);
            *ptr++ = orr_cc_immed(ARM_CC_EQ, fpsr, fpsr, 0x404);
            *ptr++ = orr_cc_immed(ARM_CC_MI, fpsr, fpsr, 0x408);
            *ptr++ = orr_cc_immed(ARM_CC_VS, fpsr, fpsr, 0x401);
        }
    }
    /* FABS */
    else if ((opcode & 0xffc0) == 0xf200 && ((opcode2 & 0xa07f) == 0x0018 || (opcode2 & 0xa07b) == 0x0058))
    {
        uint8_t fp_src = 0xff;
        uint8_t fp_dst = (opcode2 >> 7) & 7;
        uint8_t precision = 0;

        if (opcode2 & 0x0040) {
            if (opcode2 & 0x0004)
                precision = 8;
            else
                precision = 4;
        }

        (void)precision;

        ptr = FPU_FetchData(ptr, m68k_ptr, &fp_src, opcode, opcode2, &ext_count);
        fp_dst = RA_MapFPURegisterForWrite(&ptr, fp_dst);

        *ptr++ = fabsd(fp_dst, fp_src);

        RA_FreeFPURegister(&ptr, fp_src);

        ptr = EMIT_AdvancePC(ptr, 2 * (ext_count + 1));
        (*m68k_ptr) += ext_count;

        if (FPSR_Update_Needed(m68k_ptr))
        {
            uint8_t fpsr = M68K_ModifyFPSR(&ptr);

            *ptr++ = fcmpzd(fp_dst);
            *ptr++ = fmstat();
            *ptr++ = bic_immed(fpsr, fpsr, 0x40f);
            *ptr++ = orr_cc_immed(ARM_CC_EQ, fpsr, fpsr, 0x404);
            *ptr++ = orr_cc_immed(ARM_CC_MI, fpsr, fpsr, 0x408);
            *ptr++ = orr_cc_immed(ARM_CC_VS, fpsr, fpsr, 0x401);
        }
    }
    /* FADD */
    else if ((opcode & 0xffc0) == 0xf200 && ((opcode2 & 0xa07f) == 0x0022 || (opcode2 & 0xa07b) == 0x0062))
    {
        uint8_t fp_src = 0xff;
        uint8_t fp_dst = (opcode2 >> 7) & 7;
        uint8_t precision = 0;

        if (opcode2 & 0x0040) {
            if (opcode2 & 0x0004)
                precision = 8;
            else
                precision = 4;
        }

        (void)precision;

        ptr = FPU_FetchData(ptr, m68k_ptr, &fp_src, opcode, opcode2, &ext_count);
        fp_dst = RA_MapFPURegister(&ptr, fp_dst);

        *ptr++ = faddd(fp_dst, fp_dst, fp_src);

        RA_SetDirtyFPURegister(&ptr, fp_dst);

        RA_FreeFPURegister(&ptr, fp_src);

        ptr = EMIT_AdvancePC(ptr, 2 * (ext_count + 1));
        (*m68k_ptr) += ext_count;

        if (FPSR_Update_Needed(m68k_ptr))
        {
            uint8_t fpsr = M68K_ModifyFPSR(&ptr);

            *ptr++ = fcmpzd(fp_dst);
            *ptr++ = fmstat();
            *ptr++ = bic_immed(fpsr, fpsr, 0x40f);
            *ptr++ = orr_cc_immed(ARM_CC_EQ, fpsr, fpsr, 0x404);
            *ptr++ = orr_cc_immed(ARM_CC_MI, fpsr, fpsr, 0x408);
            *ptr++ = orr_cc_immed(ARM_CC_VS, fpsr, fpsr, 0x401);
        }
    }
    /* FBcc */
    else if ((opcode & 0xff80) == 0xf280)
    {
        uint8_t fpsr = M68K_GetFPSR(&ptr);
        uint8_t predicate = opcode & 0x3f;
        uint8_t success_condition = 0;
        uint8_t tmp_cc = 0xff;
        uint32_t *tmpptr;

        /* Test predicate with masked signalling bit, operations are the same */
        switch (predicate & 0x0f)
        {
            case F_CC_EQ:
                *ptr++ = tst_immed(fpsr, 0x404);
                success_condition = ARM_CC_NE;
                break;
            case F_CC_NE:
                *ptr++ = tst_immed(fpsr, 0x404);
                success_condition = ARM_CC_EQ;
                break;
            case F_CC_OGT:
                *ptr++ = tst_immed(fpsr, 0x40d);
                success_condition = ARM_CC_EQ;
                break;
            case F_CC_ULE:
                *ptr++ = tst_immed(fpsr, 0x40d);
                success_condition = ARM_CC_NE;
                break;
            case F_CC_OGE: // Z == 1 || (N == 0 && NAN == 0)
                tmp_cc = RA_AllocARMRegister(&ptr);
                *ptr++ = eor_immed(tmp_cc, fpsr, 0x404);    /* Copy fpsr to temporary reg, invert Z */
                *ptr++ = tst_immed(tmp_cc, 0x404); // Test Z
                *ptr++ = tst_cc_immed(ARM_CC_NE, fpsr, 0x409); // Z == 1? Test N == 0 && NAN == 0
                success_condition = ARM_CC_EQ;
                break;
            case F_CC_ULT: // NAN == 1 || (N == 1 && Z == 0)
                tmp_cc = RA_AllocARMRegister(&ptr);
                *ptr++ = eor_immed(tmp_cc, fpsr, 0x409);    /* Copy fpsr to temporary reg, invert N and NAN */
                *ptr++ = tst_immed(tmp_cc, 0x401);  // Test NAN
                *ptr++ = tst_cc_immed(ARM_CC_NE, fpsr, 0x40c); // !NAN == 1, test !N == 0 && Z == 0
                success_condition = ARM_CC_EQ;
                break;
            case F_CC_OLT: // N == 1 && (NAN == 0 && Z == 0)
                tmp_cc = RA_AllocARMRegister(&ptr);
                *ptr++ = eor_immed(tmp_cc, fpsr, 0x408);    /* Copy fpsr to temporary reg, invert N */
                *ptr++ = tst_immed(tmp_cc, 0x40d);
                success_condition = ARM_CC_EQ;
                break;
            case F_CC_UGE: // NAN == 1 || (Z == 1 || N == 0)
                tmp_cc = RA_AllocARMRegister(&ptr);
                *ptr++ = eor_immed(tmp_cc, fpsr, 0x408);    /* Copy fpsr to temporary reg, invert N */
                *ptr++ = tst_immed(tmp_cc, 0x40d);
                success_condition = ARM_CC_NE;
                break;
            case F_CC_OLE: // Z == 1 || (N == 1 && NAN == 0)
                tmp_cc = RA_AllocARMRegister(&ptr);
                *ptr++ = eor_immed(tmp_cc, fpsr, 0x40c);    /* Copy fpsr to temporary reg, invert Z and N */
                *ptr++ = tst_immed(tmp_cc, 0x404);          /* Test if inverted Z == 0 */
                *ptr++ = tst_cc_immed(ARM_CC_NE, tmp_cc, 0x409);    /* Test if !N == 0 && NAN == 0 */
                success_condition = ARM_CC_EQ;
                break;
            case F_CC_UGT: // NAN == 1 || (N == 0 && Z == 0)
                tmp_cc = RA_AllocARMRegister(&ptr);
                *ptr++ = eor_immed(tmp_cc, fpsr, 0x401);    /* Copy fpsr to temporary reg, invert NAN */
                *ptr++ = tst_immed(tmp_cc, 0x401);          /* Test if inverted NAN == 0 */
                *ptr++ = tst_cc_immed(ARM_CC_NE, tmp_cc, 0x40c);    /* Test if !N == 0 && Z == 0 */
                success_condition = ARM_CC_EQ;
                break;
            case F_CC_OGL:
                *ptr++ = tst_immed(fpsr, 0x405);
                success_condition = ARM_CC_EQ;
                break;
            case F_CC_UEQ:
                *ptr++ = tst_immed(fpsr, 0x405);
                success_condition = ARM_CC_NE;
                break;
            case F_CC_OR:
                *ptr++ = tst_immed(fpsr, 0x401);
                success_condition = ARM_CC_EQ;
                break;
            case F_CC_UN:
                *ptr++ = tst_immed(fpsr, 0x401);
                success_condition = ARM_CC_NE;
                break;
        }
        RA_FreeARMRegister(&ptr, tmp_cc);

        int8_t local_pc_off = 2;

        ptr = EMIT_GetOffsetPC(ptr, &local_pc_off);
        ptr = EMIT_ResetOffsetPC(ptr);

        uint8_t reg = RA_AllocARMRegister(&ptr);

        intptr_t branch_target = (intptr_t)(*m68k_ptr);
        intptr_t branch_offset = 0;

        /* use 16-bit offset */
        if ((opcode & 0x0040) == 0x0000)
        {
            branch_offset = (int16_t)BE16(*(*m68k_ptr)++);
        }
        /* use 32-bit offset */
        else
        {
            uint16_t lo16, hi16;
            hi16 = BE16(*(*m68k_ptr)++);
            lo16 = BE16(*(*m68k_ptr)++);
            branch_offset = lo16 | (hi16 << 16);
        }

        branch_offset += local_pc_off;

        if (branch_offset > 0 && branch_offset < 255)
            *ptr++ = add_cc_immed(success_condition, REG_PC, REG_PC, branch_offset);
        else if (branch_offset > -256 && branch_offset < 0)
            *ptr++ = sub_cc_immed(success_condition, REG_PC, REG_PC, -branch_offset);
        else if (branch_offset != 0) {
            *ptr++ = movw_cc_immed_u16(success_condition, reg, branch_offset);
            if ((branch_offset >> 16) & 0xffff)
                *ptr++ = movt_cc_immed_u16(success_condition, reg, (branch_offset >> 16) & 0xffff);
            *ptr++ = add_cc_reg(success_condition, REG_PC, REG_PC, reg, 0);
        }

        branch_target += branch_offset - local_pc_off;

        /* Next jump to skip the condition - invert bit 0 of the condition code here! */
        tmpptr = ptr;

        *ptr++ = 0; // Here a b_cc(success_condition ^ 1, 2); will be put, but with right offset

        int16_t local_pc_off_16 = local_pc_off - 2;

        /* Adjust PC accordingly */
        if ((opcode & 0x0040) == 0x0000)
        {
            local_pc_off_16 += 4;
        }
        /* use 32-bit offset */
        else
        {
            local_pc_off_16 += 6;
        }

        if (local_pc_off_16 > 0 && local_pc_off_16 < 255)
            *ptr++ = add_immed(REG_PC, REG_PC, local_pc_off_16);
        else if (local_pc_off_16 > -256 && local_pc_off_16 < 0)
            *ptr++ = sub_immed(REG_PC, REG_PC, -local_pc_off_16);
        else if (local_pc_off_16 != 0) {
            *ptr++ = movw_immed_u16(reg, local_pc_off_16);
            if ((local_pc_off_16 >> 16) & 0xffff)
                *ptr++ = movt_immed_u16(reg, local_pc_off_16 >> 16);
            *ptr++ = add_reg(REG_PC, REG_PC, reg, 0);
        }

        /* Now we now how far we jump. put the branch in place */
        *tmpptr = b_cc(success_condition, ptr-tmpptr-2);
        *m68k_ptr = (uint16_t *)branch_target;

        RA_FreeARMRegister(&ptr, reg);
        *ptr++ = (uint32_t)tmpptr;
        *ptr++ = 1;
        *ptr++ = branch_target;
        *ptr++ = INSN_TO_LE(0xfffffffe);
    }
    /* FCMP */
    else if ((opcode & 0xffc0) == 0xf200 && (opcode2 & 0xa07f) == 0x0038)
    {
        uint8_t fp_src = 0xff;
        uint8_t fp_dst = (opcode2 >> 7) & 7;

        ptr = FPU_FetchData(ptr, m68k_ptr, &fp_src, opcode, opcode2, &ext_count);
        fp_dst = RA_MapFPURegister(&ptr, fp_dst);

        *ptr++ = fcmpd(fp_dst, fp_src);

        RA_FreeFPURegister(&ptr, fp_src);

        ptr = EMIT_AdvancePC(ptr, 2 * (ext_count + 1));
        (*m68k_ptr) += ext_count;

        if (FPSR_Update_Needed(m68k_ptr))
        {
            uint8_t fpsr = M68K_ModifyFPSR(&ptr);

            *ptr++ = fmstat();
            *ptr++ = bic_immed(fpsr, fpsr, 0x40f);
            *ptr++ = orr_cc_immed(ARM_CC_EQ, fpsr, fpsr, 0x404);
            *ptr++ = orr_cc_immed(ARM_CC_MI, fpsr, fpsr, 0x408);
            *ptr++ = orr_cc_immed(ARM_CC_VS, fpsr, fpsr, 0x401);
        }
    }
    /* FDIV */
    else if ((opcode & 0xffc0) == 0xf200 && ((opcode2 & 0xa07f) == 0x0020 || (opcode2 & 0xa07b) == 0x0060))
    {
        uint8_t fp_src = 0xff;
        uint8_t fp_dst = (opcode2 >> 7) & 7;
        uint8_t precision = 0;

        if (opcode2 & 0x0040) {
            if (opcode2 & 0x0004)
                precision = 8;
            else
                precision = 4;
        }

        (void)precision;

        ptr = FPU_FetchData(ptr, m68k_ptr, &fp_src, opcode, opcode2, &ext_count);
        fp_dst = RA_MapFPURegister(&ptr, fp_dst);

        *ptr++ = fdivd(fp_dst, fp_dst, fp_src);

        RA_SetDirtyFPURegister(&ptr, fp_dst);

        RA_FreeFPURegister(&ptr, fp_src);

        ptr = EMIT_AdvancePC(ptr, 2 * (ext_count + 1));
        (*m68k_ptr) += ext_count;

        if (FPSR_Update_Needed(m68k_ptr))
        {
            uint8_t fpsr = M68K_ModifyFPSR(&ptr);

            *ptr++ = fcmpzd(fp_dst);
            *ptr++ = fmstat();
            *ptr++ = bic_immed(fpsr, fpsr, 0x40f);
            *ptr++ = orr_cc_immed(ARM_CC_EQ, fpsr, fpsr, 0x404);
            *ptr++ = orr_cc_immed(ARM_CC_MI, fpsr, fpsr, 0x408);
            *ptr++ = orr_cc_immed(ARM_CC_VS, fpsr, fpsr, 0x401);
        }
    }
    /* FINT */
    else if ((opcode & 0xffc0) == 0xf200 && (opcode2 & 0xa07f) == 0x0001)
    {
        uint8_t fp_src = 0xff;
        uint8_t fp_dst = (opcode2 >> 7) & 7;

        ptr = FPU_FetchData(ptr, m68k_ptr, &fp_src, opcode, opcode2, &ext_count);
        fp_dst = RA_MapFPURegisterForWrite(&ptr, fp_dst);

        *ptr++ = ftosid(0, fp_src);     /* Convert double to signed integer with rounding defined by FPSCR */
        *ptr++ = fsitod(fp_dst, 0);     /* Convert signed integer back to double */

        RA_FreeFPURegister(&ptr, fp_src);

        ptr = EMIT_AdvancePC(ptr, 2 * (ext_count + 1));
        (*m68k_ptr) += ext_count;

        if (FPSR_Update_Needed(m68k_ptr))
        {
            uint8_t fpsr = M68K_ModifyFPSR(&ptr);

            *ptr++ = fcmpzd(fp_dst);
            *ptr++ = fmstat();
            *ptr++ = bic_immed(fpsr, fpsr, 0x40f);
            *ptr++ = orr_cc_immed(ARM_CC_EQ, fpsr, fpsr, 0x404);
            *ptr++ = orr_cc_immed(ARM_CC_MI, fpsr, fpsr, 0x408);
            *ptr++ = orr_cc_immed(ARM_CC_VS, fpsr, fpsr, 0x401);
        }
    }
    /* FINTRZ */
    else if ((opcode & 0xffc0) == 0xf200 && (opcode2 & 0xa07f) == 0x0003)
    {
        uint8_t fp_src = 0xff;
        uint8_t fp_dst = (opcode2 >> 7) & 7;

        ptr = FPU_FetchData(ptr, m68k_ptr, &fp_src, opcode, opcode2, &ext_count);
        fp_dst = RA_MapFPURegisterForWrite(&ptr, fp_dst);

        *ptr++ = ftosidrz(0, fp_src);     /* Convert double to signed integer with rounding defined by FPSCR */
        *ptr++ = fsitod(fp_dst, 0);     /* Convert signed integer back to double */

        RA_FreeFPURegister(&ptr, fp_src);

        ptr = EMIT_AdvancePC(ptr, 2 * (ext_count + 1));
        (*m68k_ptr) += ext_count;

        if (FPSR_Update_Needed(m68k_ptr))
        {
            uint8_t fpsr = M68K_ModifyFPSR(&ptr);

            *ptr++ = fcmpzd(fp_dst);
            *ptr++ = fmstat();
            *ptr++ = bic_immed(fpsr, fpsr, 0x40f);
            *ptr++ = orr_cc_immed(ARM_CC_EQ, fpsr, fpsr, 0x404);
            *ptr++ = orr_cc_immed(ARM_CC_MI, fpsr, fpsr, 0x408);
            *ptr++ = orr_cc_immed(ARM_CC_VS, fpsr, fpsr, 0x401);
        }
    }
    /* FLOGN */
    else if ((opcode & 0xffc0) == 0xf200 && (opcode2 & 0xa07f) == 0x0014)
    {
        uint8_t fp_src = 0xff;
        uint8_t fp_dst = (opcode2 >> 7) & 7;

        ptr = FPU_FetchData(ptr, m68k_ptr, &fp_src, opcode, opcode2, &ext_count);
        fp_dst = RA_MapFPURegisterForWrite(&ptr, fp_dst);

        *ptr++ = fcpyd(0, fp_src);

        *ptr++ = push(0x0f | (1 << 12));
        *ptr++ = ldr_offset(15, 12, 12);
        *ptr++ = blx_cc_reg(ARM_CC_AL, 12);
        *ptr++ = fcpyd(fp_dst, 0);
        *ptr++ = pop(0x0f | (1 << 12));
        *ptr++ = b_cc(ARM_CC_AL, 0);
        *ptr++ = BE32((uint32_t)__ieee754_log);

        RA_FreeFPURegister(&ptr, fp_src);

        ptr = EMIT_AdvancePC(ptr, 2 * (ext_count + 1));
        (*m68k_ptr) += ext_count;

        if (FPSR_Update_Needed(m68k_ptr))
        {
            uint8_t fpsr = M68K_ModifyFPSR(&ptr);

            *ptr++ = fcmpzd(fp_dst);
            *ptr++ = fmstat();
            *ptr++ = bic_immed(fpsr, fpsr, 0x40f);
            *ptr++ = orr_cc_immed(ARM_CC_EQ, fpsr, fpsr, 0x404);
            *ptr++ = orr_cc_immed(ARM_CC_MI, fpsr, fpsr, 0x408);
            *ptr++ = orr_cc_immed(ARM_CC_VS, fpsr, fpsr, 0x401);
        }

        *ptr++ = INSN_TO_LE(0xfffffff0);
    }
    /* FMOVE to REG */
    else if ((opcode & 0xffc0) == 0xf200 && ((opcode2 & 0xa07f) == 0x0000 || (opcode2 & 0xa07b) == 0x0040))
    {
        uint8_t fp_src = 0xff;
        uint8_t fp_dst = (opcode2 >> 7) & 7;
        uint8_t precision = 0;

        if (opcode2 & 0x0040) {
            if (opcode2 & 0x0004)
                precision = 8;
            else
                precision = 4;
        }

        (void)precision;

        ptr = FPU_FetchData(ptr, m68k_ptr, &fp_src, opcode, opcode2, &ext_count);
        fp_dst = RA_MapFPURegisterForWrite(&ptr, fp_dst);

        *ptr++ = fcpyd(fp_dst, fp_src);

        RA_FreeFPURegister(&ptr, fp_src);
        ptr = EMIT_AdvancePC(ptr, 2 * (ext_count + 1));
        (*m68k_ptr) += ext_count;

        if (FPSR_Update_Needed(m68k_ptr))
        {
            uint8_t fpsr = M68K_ModifyFPSR(&ptr);

            *ptr++ = fcmpzd(fp_dst);
            *ptr++ = fmstat();
            *ptr++ = bic_immed(fpsr, fpsr, 0x40f);
            *ptr++ = orr_cc_immed(ARM_CC_EQ, fpsr, fpsr, 0x404);
            *ptr++ = orr_cc_immed(ARM_CC_MI, fpsr, fpsr, 0x408);
            *ptr++ = orr_cc_immed(ARM_CC_VS, fpsr, fpsr, 0x401);
        }
    }
    /* FMOVE to MEM */
    else if ((opcode & 0xffc0) == 0xf200 && (opcode2 & 0xe07f) == 0x6000)
    {
        uint8_t fp_src = (opcode2 >> 7) & 7;
        fp_src = RA_MapFPURegister(&ptr, fp_src);
        ptr = FPU_StoreData(ptr, m68k_ptr, fp_src, opcode, opcode2, &ext_count);

        ptr = EMIT_AdvancePC(ptr, 2 * (ext_count + 1));
        (*m68k_ptr) += ext_count;

        if (FPSR_Update_Needed(m68k_ptr))
        {
            uint8_t fpsr = M68K_ModifyFPSR(&ptr);

            *ptr++ = fcmpzd(fp_src);
            *ptr++ = fmstat();
            *ptr++ = bic_immed(fpsr, fpsr, 0x40f);
            *ptr++ = orr_cc_immed(ARM_CC_EQ, fpsr, fpsr, 0x404);
            *ptr++ = orr_cc_immed(ARM_CC_MI, fpsr, fpsr, 0x408);
            *ptr++ = orr_cc_immed(ARM_CC_VS, fpsr, fpsr, 0x401);
        }
    }
    /* FMOVE from special */
    else if ((opcode & 0xffc0) == 0xf200 && (opcode2 & 0xe3ff) == 0xa000)
    {
        uint8_t special_reg;
        switch (opcode2 & 0x1c00)
        {
            case 0x1000:    /* FPCR */
                special_reg = M68K_GetFPCR(&ptr);
                break;
            case 0x0800:    /* FPSR */
                special_reg = M68K_GetFPSR(&ptr);
                break;
            case 0x0400:    /* FPIAR */
                special_reg = RA_AllocARMRegister(&ptr);
                *ptr++ = ldr_offset(REG_CTX, special_reg, __builtin_offsetof(struct M68KState, FPIAR));
                break;
        }

        if ((opcode & 0x38) == 0)
        {
            uint8_t reg = RA_MapM68kRegisterForWrite(&ptr, opcode & 7);
            *ptr++ = mov_reg(reg, special_reg);
        }
        else
        {
            ptr = EMIT_StoreToEffectiveAddress(ptr, 4, &special_reg, opcode & 0x3f, *m68k_ptr, &ext_count);
        }

        if ((opcode2 & 0x1c00) == 0x0400)
            RA_FreeARMRegister(&ptr, special_reg);

        ptr = EMIT_AdvancePC(ptr, 2 * (ext_count + 1));
        (*m68k_ptr) += ext_count;
    }
    /* FMOVE to special */
    else if ((opcode & 0xffc0) == 0xf200 && (opcode2 & 0xe3ff) == 0x8000)
    {
        uint8_t src = 0xff;
        uint8_t tmp = 0xff;
        uint8_t reg = 0xff;

        ptr = EMIT_LoadFromEffectiveAddress(ptr, 4, &src, opcode & 0x3f, *m68k_ptr, &ext_count, 0, NULL);

        switch (opcode2 & 0x1c00)
        {
            case 0x1000:    /* FPCR */
                tmp = RA_AllocARMRegister(&ptr);
                reg = M68K_ModifyFPCR(&ptr);
                *ptr++ = mov_reg(reg, src);
                *ptr++ = fmxr(tmp, FPSCR);
                *ptr++ = lsr_immed(src, src, 4);
                *ptr++ = tst_immed(src, 1);
                *ptr++ = eor_cc_immed(ARM_CC_NE, src, src, 2);
                *ptr++ = bfi(tmp, src, 22, 2);
                *ptr++ = fmrx(FPSCR, tmp);
                break;
            case 0x0800:    /* FPSR */
                reg = M68K_ModifyFPSR(&ptr);
                *ptr++ = mov_reg(reg, src);
                break;
            case 0x0400:    /* FPIAR */
                *ptr++ = str_offset(REG_CTX, src, __builtin_offsetof(struct M68KState, FPIAR));
                break;
        }

        RA_FreeARMRegister(&ptr, src);
        RA_FreeARMRegister(&ptr, tmp);

        ptr = EMIT_AdvancePC(ptr, 2 * (ext_count + 1));
        (*m68k_ptr) += ext_count;
    }
    /* FMOVEM */
    else if ((opcode & 0xffc0) == 0xf200 && (opcode2 & 0xc700) == 0xc000)
    {
        char dir = (opcode2 >> 13) & 1;
        uint8_t base_reg = 0xff;

        if (dir) { /* FPn to memory */
            uint8_t mode = (opcode & 0x0038) >> 3;

            if (mode == 4 || mode == 3)
                ptr = EMIT_LoadFromEffectiveAddress(ptr, 0, &base_reg, opcode & 0x3f, *m68k_ptr, &ext_count, 0, NULL);
            else
                ptr = EMIT_LoadFromEffectiveAddress(ptr, 0, &base_reg, opcode & 0x3f, *m68k_ptr, &ext_count, 1, NULL);

            /* Pre index? Note - dynamic mode not supported yet! using double mode instead of extended! */
            if (mode == 4) {
                int size = 0;
                int cnt = 0;
                for (int i=0; i < 8; i++)
                    if ((opcode2 & (1 << i)))
                        size++;
                *ptr++ = sub_immed(base_reg, base_reg, 12*size);

                for (int i=0; i < 8; i++) {
                    if ((opcode2 & (1 << i)) != 0) {
                        uint8_t fp_reg = RA_MapFPURegister(&ptr, i);
                        //*ptr++ = sub_immed(base_reg, base_reg, 12);
                        *ptr++ = fstd(fp_reg, base_reg, 3*cnt++);
                        RA_FreeFPURegister(&ptr, fp_reg);
                    }
                }
                RA_SetDirtyM68kRegister(&ptr, 8 + (opcode & 7));
            } else if (mode == 3) {
                kprintf("[JIT] Unsupported FMOVEM operation (REG to MEM postindex)\n");
            } else {
                int cnt = 0;
                for (int i=0; i < 8; i++) {
                    if ((opcode2 & (1 << i)) != 0) {
                        uint8_t fp_reg = RA_MapFPURegister(&ptr, i);
                        *ptr++ = fstd(fp_reg, base_reg, cnt*3);
                        cnt++;
                        RA_FreeFPURegister(&ptr, fp_reg);
                    }
                }
            }
        } else { /* memory to FPn */
            uint8_t mode = (opcode & 0x0038) >> 3;

            if (mode == 4 || mode == 3)
                ptr = EMIT_LoadFromEffectiveAddress(ptr, 0, &base_reg, opcode & 0x3f, *m68k_ptr, &ext_count, 0, NULL);
            else
                ptr = EMIT_LoadFromEffectiveAddress(ptr, 0, &base_reg, opcode & 0x3f, *m68k_ptr, &ext_count, 1, NULL);

            /* Post index? Note - dynamic mode not supported yet! using double mode instead of extended! */
            if (mode == 3) {
                int cnt = 0;
                for (int i=0; i < 8; i++) {
                    if ((opcode2 & (0x80 >> i)) != 0) {
                        uint8_t fp_reg = RA_MapFPURegisterForWrite(&ptr, i);
                        *ptr++ = fldd(fp_reg, base_reg, 3*cnt++);
                        //*ptr++ = add_immed(base_reg, base_reg, 12);
                        RA_FreeFPURegister(&ptr, fp_reg);
                    }
                }
                *ptr++ = add_immed(base_reg, base_reg, 12*cnt);
                RA_SetDirtyM68kRegister(&ptr, 8 + (opcode & 7));
            } else if (mode == 4) {
                kprintf("[JIT] Unsupported FMOVEM operation (REG to MEM preindex)\n");
            } else {
                int cnt = 0;
                for (int i=0; i < 8; i++) {
                    if ((opcode2 & (0x80 >> i)) != 0) {
                        uint8_t fp_reg = RA_MapFPURegisterForWrite(&ptr, i);
                        *ptr++ = fldd(fp_reg, base_reg, cnt*3);
                        cnt++;
                        RA_FreeFPURegister(&ptr, fp_reg);
                    }
                }
            }
        }

        RA_FreeARMRegister(&ptr, base_reg);

        ptr = EMIT_AdvancePC(ptr, 2 * (ext_count + 1));
        (*m68k_ptr) += ext_count;
    }
    /* FMUL */
    else if ((opcode & 0xffc0) == 0xf200 && ((opcode2 & 0xa07f) == 0x0023 || (opcode2 & 0xa07b) == 0x0063))
    {
        uint8_t fp_src = 0xff;
        uint8_t fp_dst = (opcode2 >> 7) & 7;
        uint8_t precision = 0;

        if (opcode2 & 0x0040) {
            if (opcode2 & 0x0004)
                precision = 8;
            else
                precision = 4;
        }

        (void)precision;

        ptr = FPU_FetchData(ptr, m68k_ptr, &fp_src, opcode, opcode2, &ext_count);
        fp_dst = RA_MapFPURegister(&ptr, fp_dst);

        *ptr++ = fmuld(fp_dst, fp_dst, fp_src);

        RA_SetDirtyFPURegister(&ptr, fp_dst);

        RA_FreeFPURegister(&ptr, fp_src);

        ptr = EMIT_AdvancePC(ptr, 2 * (ext_count + 1));
        (*m68k_ptr) += ext_count;

        if (FPSR_Update_Needed(m68k_ptr))
        {
            uint8_t fpsr = M68K_ModifyFPSR(&ptr);

            *ptr++ = fcmpzd(fp_dst);
            *ptr++ = fmstat();
            *ptr++ = bic_immed(fpsr, fpsr, 0x40f);
            *ptr++ = orr_cc_immed(ARM_CC_EQ, fpsr, fpsr, 0x404);
            *ptr++ = orr_cc_immed(ARM_CC_MI, fpsr, fpsr, 0x408);
            *ptr++ = orr_cc_immed(ARM_CC_VS, fpsr, fpsr, 0x401);
        }
    }
    /* FNEG */
    else if ((opcode & 0xffc0) == 0xf200 && ((opcode2 & 0xa07f) == 0x001a || (opcode2 & 0xa07b) == 0x005a))
    {
        uint8_t fp_src = 0xff;
        uint8_t fp_dst = (opcode2 >> 7) & 7;
        uint8_t precision = 0;

        if (opcode2 & 0x0040) {
            if (opcode2 & 0x0004)
                precision = 8;
            else
                precision = 4;
        }

        (void)precision;

        ptr = FPU_FetchData(ptr, m68k_ptr, &fp_src, opcode, opcode2, &ext_count);
        fp_dst = RA_MapFPURegisterForWrite(&ptr, fp_dst);

        *ptr++ = fnegd(fp_dst, fp_src);

        RA_FreeFPURegister(&ptr, fp_src);

        ptr = EMIT_AdvancePC(ptr, 2 * (ext_count + 1));
        (*m68k_ptr) += ext_count;

        if (FPSR_Update_Needed(m68k_ptr))
        {
            uint8_t fpsr = M68K_ModifyFPSR(&ptr);

            *ptr++ = fcmpzd(fp_dst);
            *ptr++ = fmstat();
            *ptr++ = bic_immed(fpsr, fpsr, 0x40f);
            *ptr++ = orr_cc_immed(ARM_CC_EQ, fpsr, fpsr, 0x404);
            *ptr++ = orr_cc_immed(ARM_CC_MI, fpsr, fpsr, 0x408);
            *ptr++ = orr_cc_immed(ARM_CC_VS, fpsr, fpsr, 0x401);
        }
    }
    /* FTST */
    else if ((opcode & 0xffc0) == 0xf200 && (opcode2 & 0xa07f) == 0x003a)
    {
        uint8_t fp_src = 0xff;

        ptr = FPU_FetchData(ptr, m68k_ptr, &fp_src, opcode, opcode2, &ext_count);

        *ptr++ = fcmpzd(fp_src);

        RA_FreeFPURegister(&ptr, fp_src);

        ptr = EMIT_AdvancePC(ptr, 2 * (ext_count + 1));
        (*m68k_ptr) += ext_count;

        if (FPSR_Update_Needed(m68k_ptr))
        {
            uint8_t fpsr = M68K_ModifyFPSR(&ptr);

            *ptr++ = fmstat();
            *ptr++ = bic_immed(fpsr, fpsr, 0x40f);
            *ptr++ = orr_cc_immed(ARM_CC_EQ, fpsr, fpsr, 0x404);
            *ptr++ = orr_cc_immed(ARM_CC_MI, fpsr, fpsr, 0x408);
            *ptr++ = orr_cc_immed(ARM_CC_VS, fpsr, fpsr, 0x401);
        }
    }
    /* FScc */
    else if ((opcode & 0xffc0) == 0xf240 && (opcode2 & 0xffc0) == 0)
    {
        uint8_t fpsr = M68K_GetFPSR(&ptr);
        uint8_t predicate = opcode2 & 0x3f;
        uint8_t success_condition = 0;
        uint8_t tmp_cc = 0xff;

        /* Test predicate with masked signalling bit, operations are the same */
        switch (predicate & 0x0f)
        {
            case F_CC_EQ:
                *ptr++ = tst_immed(fpsr, 0x404);
                success_condition = ARM_CC_NE;
                break;
            case F_CC_NE:
                *ptr++ = tst_immed(fpsr, 0x404);
                success_condition = ARM_CC_EQ;
                break;
            case F_CC_OGT:
                *ptr++ = tst_immed(fpsr, 0x40d);
                success_condition = ARM_CC_EQ;
                break;
            case F_CC_ULE:
                *ptr++ = tst_immed(fpsr, 0x40d);
                success_condition = ARM_CC_NE;
                break;
            case F_CC_OGE: // Z == 1 || (N == 0 && NAN == 0)
                tmp_cc = RA_AllocARMRegister(&ptr);
                *ptr++ = eor_immed(tmp_cc, fpsr, 0x404);    /* Copy fpsr to temporary reg, invert Z */
                *ptr++ = tst_immed(tmp_cc, 0x404); // Test Z
                *ptr++ = tst_cc_immed(ARM_CC_NE, fpsr, 0x409); // Z == 1? Test N == 0 && NAN == 0
                success_condition = ARM_CC_EQ;
                break;
            case F_CC_ULT: // NAN == 1 || (N == 1 && Z == 0)
                tmp_cc = RA_AllocARMRegister(&ptr);
                *ptr++ = eor_immed(tmp_cc, fpsr, 0x409);    /* Copy fpsr to temporary reg, invert N and NAN */
                *ptr++ = tst_immed(tmp_cc, 0x401);  // Test NAN
                *ptr++ = tst_cc_immed(ARM_CC_NE, fpsr, 0x40c); // !NAN == 1, test !N == 0 && Z == 0
                success_condition = ARM_CC_EQ;
                break;
            case F_CC_OLT: // N == 1 && (NAN == 0 && Z == 0)
                tmp_cc = RA_AllocARMRegister(&ptr);
                *ptr++ = eor_immed(tmp_cc, fpsr, 0x408);    /* Copy fpsr to temporary reg, invert N */
                *ptr++ = tst_immed(tmp_cc, 0x40d);
                success_condition = ARM_CC_EQ;
                break;
            case F_CC_UGE: // NAN == 1 || (Z == 1 || N == 0)
                tmp_cc = RA_AllocARMRegister(&ptr);
                *ptr++ = eor_immed(tmp_cc, fpsr, 0x408);    /* Copy fpsr to temporary reg, invert N */
                *ptr++ = tst_immed(tmp_cc, 0x40d);
                success_condition = ARM_CC_NE;
                break;
            case F_CC_OLE: // Z == 1 || (N == 1 && NAN == 0)
                tmp_cc = RA_AllocARMRegister(&ptr);
                *ptr++ = eor_immed(tmp_cc, fpsr, 0x40c);    /* Copy fpsr to temporary reg, invert Z and N */
                *ptr++ = tst_immed(tmp_cc, 0x404);          /* Test if inverted Z == 0 */
                *ptr++ = tst_cc_immed(ARM_CC_NE, tmp_cc, 0x409);    /* Test if !N == 0 && NAN == 0 */
                success_condition = ARM_CC_EQ;
                break;
            case F_CC_UGT: // NAN == 1 || (N == 0 && Z == 0)
                tmp_cc = RA_AllocARMRegister(&ptr);
                *ptr++ = eor_immed(tmp_cc, fpsr, 0x401);    /* Copy fpsr to temporary reg, invert NAN */
                *ptr++ = tst_immed(tmp_cc, 0x401);          /* Test if inverted NAN == 0 */
                *ptr++ = tst_cc_immed(ARM_CC_NE, tmp_cc, 0x40c);    /* Test if !N == 0 && Z == 0 */
                success_condition = ARM_CC_EQ;
                break;
            case F_CC_OGL:
                *ptr++ = tst_immed(fpsr, 0x405);
                success_condition = ARM_CC_EQ;
                break;
            case F_CC_UEQ:
                *ptr++ = tst_immed(fpsr, 0x405);
                success_condition = ARM_CC_NE;
                break;
            case F_CC_OR:
                *ptr++ = tst_immed(fpsr, 0x401);
                success_condition = ARM_CC_EQ;
                break;
            case F_CC_UN:
                *ptr++ = tst_immed(fpsr, 0x401);
                success_condition = ARM_CC_NE;
                break;
        }
        RA_FreeARMRegister(&ptr, tmp_cc);

        if ((opcode & 0x38) == 0)
        {
            /* FScc Dx case */
            uint8_t dest = RA_MapM68kRegister(&ptr, opcode & 7);
            RA_SetDirtyM68kRegister(&ptr, opcode & 7);

            *ptr++ = orr_cc_immed(success_condition, dest, dest, 0xff);
            *ptr++ = bfc_cc(success_condition ^ 1, dest, 0, 8);
        }
        else
        {
            /* Load effective address */
            uint8_t dest = 0xff;
            uint8_t tmp = RA_AllocARMRegister(&ptr);
            uint8_t mode = (opcode & 0x0038) >> 3;

            *ptr++ = mov_cc_immed_u8(success_condition ^ 1, tmp, 0);
            *ptr++ = mvn_cc_immed_u8(success_condition, tmp, 0);

            if (mode == 3)
                ptr = EMIT_LoadFromEffectiveAddress(ptr, 0, &dest, opcode & 0x3f, *m68k_ptr, &ext_count, 0, NULL);
            else
                ptr = EMIT_LoadFromEffectiveAddress(ptr, 0, &dest, opcode & 0x3f, *m68k_ptr, &ext_count, 1, NULL);

            if (mode == 3)
            {
                *ptr++ = strb_offset_postindex(dest, tmp, (opcode & 7) == 7 ? 2 : 1);
                RA_SetDirtyM68kRegister(&ptr, 8 + (opcode & 7));
            }
            else
                *ptr++ = strb_offset(dest, tmp, 0);

            RA_FreeARMRegister(&ptr, tmp);
            RA_FreeARMRegister(&ptr, dest);
        }

        ptr = EMIT_AdvancePC(ptr, 2 * (ext_count + 1));
        (*m68k_ptr) += ext_count;
    }
    /* FSQRT */
    else if ((opcode & 0xffc0) == 0xf200 && ((opcode2 & 0xa07f) == 0x0004 || (opcode2 & 0xa07b) == 0x0041))
    {
        uint8_t fp_src = 0xff;
        uint8_t fp_dst = (opcode2 >> 7) & 7;
        uint8_t precision = 0;

        if (opcode2 & 0x0040) {
            if (opcode2 & 0x0004)
                precision = 8;
            else
                precision = 4;
        }

        (void)precision;

        ptr = FPU_FetchData(ptr, m68k_ptr, &fp_src, opcode, opcode2, &ext_count);
        fp_dst = RA_MapFPURegisterForWrite(&ptr, fp_dst);

        if (Features.ARM_SUPPORTS_SQRT)
        {
            *ptr++ = fsqrtd(fp_dst, fp_src);
        }
        else
        {
            /* Missing... */
        }

        RA_FreeFPURegister(&ptr, fp_src);

        ptr = EMIT_AdvancePC(ptr, 2 * (ext_count + 1));
        (*m68k_ptr) += ext_count;

        if (FPSR_Update_Needed(m68k_ptr))
        {
            uint8_t fpsr = M68K_ModifyFPSR(&ptr);

            *ptr++ = fcmpzd(fp_dst);
            *ptr++ = fmstat();
            *ptr++ = bic_immed(fpsr, fpsr, 0x40f);
            *ptr++ = orr_cc_immed(ARM_CC_EQ, fpsr, fpsr, 0x404);
            *ptr++ = orr_cc_immed(ARM_CC_MI, fpsr, fpsr, 0x408);
            *ptr++ = orr_cc_immed(ARM_CC_VS, fpsr, fpsr, 0x401);
        }
    }
    /* FSUB */
    else if ((opcode & 0xffc0) == 0xf200 && ((opcode2 & 0xa07f) == 0x0028 || (opcode2 & 0xa07b) == 0x0068))
    {
        uint8_t fp_src = 0xff;
        uint8_t fp_dst = (opcode2 >> 7) & 7;
        uint8_t precision = 0;

        if (opcode2 & 0x0040) {
            if (opcode2 & 0x0004)
                precision = 8;
            else
                precision = 4;
        }

        (void)precision;

        ptr = FPU_FetchData(ptr, m68k_ptr, &fp_src, opcode, opcode2, &ext_count);
        fp_dst = RA_MapFPURegister(&ptr, fp_dst);

        *ptr++ = fsubd(fp_dst, fp_dst, fp_src);

        RA_SetDirtyFPURegister(&ptr, fp_dst);

        RA_FreeFPURegister(&ptr, fp_src);

        ptr = EMIT_AdvancePC(ptr, 2 * (ext_count + 1));
        (*m68k_ptr) += ext_count;

        if (FPSR_Update_Needed(m68k_ptr))
        {
            uint8_t fpsr = M68K_ModifyFPSR(&ptr);

            *ptr++ = fcmpzd(fp_dst);
            *ptr++ = fmstat();
            *ptr++ = bic_immed(fpsr, fpsr, 0x40f);
            *ptr++ = orr_cc_immed(ARM_CC_EQ, fpsr, fpsr, 0x404);
            *ptr++ = orr_cc_immed(ARM_CC_MI, fpsr, fpsr, 0x408);
            *ptr++ = orr_cc_immed(ARM_CC_VS, fpsr, fpsr, 0x401);
        }
    }
    /* FSIN */
    else if ((opcode & 0xffc0) == 0xf200 && (opcode2 & 0xa07f) == 0x000e)
    {
        uint8_t fp_dst = (opcode2 >> 7) & 7;
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

        *ptr++ = push(1 << 12);

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
        *ptr++ = ldr_offset(15, 12, (intptr_t)adr_cos - (intptr_t)ref_ptr);
        *ptr++ = blx_cc_reg(ARM_CC_AL, 12);
        *ptr++ = fnegd(fp_dst, 0);

        *exit_1 |= INSN_TO_LE(ptr - exit_1 - 2);
        *exit_2 |= INSN_TO_LE(ptr - exit_2 - 2);
        *exit_3 |= INSN_TO_LE(ptr - exit_3 - 2);

        *ptr++ = tst_immed(sign, 0x480);
        *ptr++ = fnegd_cc(ARM_CC_MI, fp_dst, fp_dst);

        *ptr++ = pop(1 << 12);

        RA_FreeFPURegister(&ptr, fp_src);
        RA_FreeFPURegister(&ptr, fp_tmp1);
        RA_FreeFPURegister(&ptr, fp_tmp2);
        RA_FreeARMRegister(&ptr, base_reg);
        RA_FreeARMRegister(&ptr, top_half);
        RA_FreeARMRegister(&ptr, sign);
        RA_FreeARMRegister(&ptr, cmp_num);
        ptr = EMIT_AdvancePC(ptr, 2 * (ext_count + 1));
        (*m68k_ptr) += ext_count;

        if (FPSR_Update_Needed(m68k_ptr))
        {
            uint8_t fpsr = M68K_ModifyFPSR(&ptr);

            *ptr++ = fcmpzd(fp_dst);
            *ptr++ = fmstat();
            *ptr++ = bic_immed(fpsr, fpsr, 0x40f);
            *ptr++ = orr_cc_immed(ARM_CC_EQ, fpsr, fpsr, 0x404);
            *ptr++ = orr_cc_immed(ARM_CC_MI, fpsr, fpsr, 0x408);
            *ptr++ = orr_cc_immed(ARM_CC_VS, fpsr, fpsr, 0x401);
        }

        *ptr++ = INSN_TO_LE(0xfffffff0);
    }
    /* FCOS */
    else if ((opcode & 0xffc0) == 0xf200 && (opcode2 & 0xa07f) == 0x001d)
    {
        uint8_t fp_dst = (opcode2 >> 7) & 7;
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

        *ptr++ = push(1 << 12);

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

        *ptr++ = pop(1 << 12);

        RA_FreeFPURegister(&ptr, fp_src);
        RA_FreeFPURegister(&ptr, fp_tmp1);
        RA_FreeFPURegister(&ptr, fp_tmp2);
        RA_FreeARMRegister(&ptr, base_reg);
        RA_FreeARMRegister(&ptr, top_half);
        RA_FreeARMRegister(&ptr, cmp_num);
        ptr = EMIT_AdvancePC(ptr, 2 * (ext_count + 1));
        (*m68k_ptr) += ext_count;

        if (FPSR_Update_Needed(m68k_ptr))
        {
            uint8_t fpsr = M68K_ModifyFPSR(&ptr);

            *ptr++ = fcmpzd(fp_dst);
            *ptr++ = fmstat();
            *ptr++ = bic_immed(fpsr, fpsr, 0x40f);
            *ptr++ = orr_cc_immed(ARM_CC_EQ, fpsr, fpsr, 0x404);
            *ptr++ = orr_cc_immed(ARM_CC_MI, fpsr, fpsr, 0x408);
            *ptr++ = orr_cc_immed(ARM_CC_VS, fpsr, fpsr, 0x401);
        }

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
