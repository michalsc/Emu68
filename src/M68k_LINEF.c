/*
    Copyright © 2019 Michal Schulz <michal.schulz@gmx.de>
    https://github.com/michalsc

    This Source Code Form is subject to the terms of the
    Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed
    with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "support.h"
#include "M68k.h"
#include "RegisterAllocator.h"
#include "EmuFeatures.h"
#include "lists.h"
#include "tlsf.h"
#include "math/libm.h"

enum {
    C_PI = 0,
    C_PI_2,
    C_PI_4,
    C_1_PI,
    C_2_PI,
    C_2_SQRTPI,
    C_1_2PI,
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

static double const __attribute__((used)) constants[128] = {
    [C_PI] =        3.14159265358979323846264338327950288, /* Official */
    [C_PI_2] =      1.57079632679489661923132169163975144,
    [C_PI_4] =      0.785398163397448309615660845819875721,
    [C_1_PI] =      0.318309886183790671537767526745028724,
    [C_2_PI] =      0.636619772367581343075535053490057448,
    [C_2_SQRTPI] =  1.12837916709551257389615890312154517,
    [C_1_2PI] =     0.1591549430918953357688837633725143620,
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

typedef union 
{
    uint8_t  c[12];
    uint32_t i[3];
} packed_t;

double my_pow10(int exp);
int my_log10(double v);

double PackedToDouble(packed_t value)
{
    double ret = 0.0;
    int exp = 0;
    uint64_t integer = 0;

    for (int i=4; i < 12; i++)
    {
        integer = integer * 10 + (value.c[i] >> 4);
        integer = integer * 10 + (value.c[i] & 0x0f);
    }

    ret = (double)(value.c[3] & 0x0f) + (double)integer / 1e16;
    exp = 100 * (value.c[0] & 0x0f) + 10 * (value.c[1] >> 4) + (value.c[1] & 0x0f);

    if (value.c[0] & 0x80)
        ret = -ret;
    if (value.c[0] & 0x40)
        exp = -exp;

    ret = ret * exp10(exp);

    return ret;
}

packed_t DoubleToPacked(double value, int k)
{
    k = ((int8_t)k << 1) >> 1;

    int exp = 0;
    packed_t ret;
    uint8_t c;

    ret.i[0] = 0;
    ret.i[1] = 0;
    ret.i[2] = 0;

    int prec = k > 0 ? k - 1 : 4 - k;

    if (value < 0)
    {
        value = -value;
        ret.c[0] |= 0x80;
    }

    if (prec > 16)
        prec = 16;

    exp = my_log10(value);
    value /= my_pow10(exp);

    c = (int)value;
    ret.c[3] = c;

    for(int i=0; i < prec; i++)
    {
        value = (value - c) * 10;
        c = (int)value;
        if (i & 1)
            ret.c[4 + (i >> 1)] |= c;
        else
            ret.c[4 + (i >> 1)] |= c << 4;
    }

    if (exp < 0) {
        exp = -exp;
        ret.c[0] |= 0x40;
    }

    ret.c[1] = exp % 10;
    exp /= 10;
    ret.c[1] |= (exp % 10) << 4;
    exp /= 10;
    ret.c[0] |= exp % 10;
    exp /= 10;
    
    if (exp != 0) {
        ret.c[2] |= (exp) << 4;
    }

    return ret;
}

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

#ifdef __aarch64__

void PolySine(void);
void  __attribute__((used)) stub_PolySine(void)
{
    asm volatile(
        "   .align 4                \n"
        "   .globl PolySine         \n"
        "PolySine:                  \n"
        "   stp d1, d2, [sp, #-16]! \n"
        "   str d3, [sp, #-8]!      \n"
        "   str x0, [sp, #-8]!      \n"
        "   ldr x0,=constants       \n"
        "   ldp d1, d2, [x0, %0]    \n"
        "   fmul d3, d0, d0         \n"
        "   fmadd d2, d1, d3, d2    \n"
        "   ldr d1, [x0, %0+16]     \n"
        "   fmadd d1, d2, d3, d1    \n"
        "   ldr d2, [x0, %0+24]     \n"
        "   fmadd d2, d1, d3, d2    \n"
        "   ldr d1, [x0, %0+32]     \n"
        "   fmadd d1, d2, d3, d1    \n"
        "   ldr d2, [x0, %0+40]     \n"
        "   fmadd d2, d1, d3, d2    \n"
        "   ldr d1, [x0, %0+48]     \n"
        "   fmadd d1, d2, d3, d1    \n"
        "   ldr d2, [x0, %0+56]     \n"
        "   fmadd d2, d1, d3, d2    \n"
        "   fmul d0, d2, d0         \n"
        "   ldr x0, [sp], 8         \n"
        "   ldr d3, [sp], 8         \n"
        "   ldp d1, d2, [sp], 16    \n"
        "   ret                     \n"
        "   .ltorg                  \n"::"i"(C_SIN_COEFF*8)
    );
}

void PolySineSingle(void);
void  __attribute__((used)) stub_PolySineSingle(void)
{
    asm volatile(
        "   .align 4                \n"
        "   .globl PolySineSingle   \n"
        "PolySineSingle:            \n"
        "   stp d1, d2, [sp, #-16]! \n"
        "   str d3, [sp, #-8]!      \n"
        "   str x0, [sp, #-8]!      \n"
        "   ldr x0,=constants       \n"
        "   ldp d1, d2, [x0, %0]    \n"
        "   fmul d3, d0, d0         \n"
        "   fmadd d2, d1, d3, d2    \n"
        "   ldr d1, [x0, %0+16]     \n"
        "   fmadd d1, d2, d3, d1    \n"
        "   ldr d2, [x0, %0+24]     \n"
        "   fmadd d2, d1, d3, d2    \n"
        "   ldr d1, [x0, %0+32]     \n"
        "   fmadd d1, d2, d3, d1    \n"
        "   fmul d0, d1, d0         \n"
        "   ldr x0, [sp], 8         \n"
        "   ldr d3, [sp], 8         \n"
        "   ldp d1, d2, [sp], 16    \n"
        "   ret                     \n"
        "   .ltorg                  \n"::"i"(C_SIN_COEFF_SINGLE*8)
    );
}

void PolyCosine(void);
void  __attribute__((used)) stub_PolyCosine(void)
{
    asm volatile(
        "   .align 4                \n"
        "   .globl PolyConsine      \n"
        "PolyCosine:                \n"
        "   stp d1, d2, [sp, #-16]! \n"
        "   str x0, [sp, #-8]!      \n"
        "   ldr x0,=constants       \n"
        "   fmul d2, d0, d0         \n"
        "   ldp d0, d1, [x0, %0]    \n"
        "   fmadd d1, d0, d2, d1    \n"
        "   ldr d0, [x0, %0+16]     \n"
        "   fmadd d0, d1, d2, d0    \n"
        "   ldr d1, [x0, %0+24]     \n"
        "   fmadd d1, d0, d2, d1    \n"
        "   ldr d0, [x0, %0+32]     \n"
        "   fmadd d0, d1, d2, d0    \n"
        "   ldr d1, [x0, %0+40]     \n"
        "   fmadd d1, d0, d2, d1    \n"
        "   ldr d0, [x0, %0+48]     \n"
        "   fmadd d0, d1, d2, d0    \n"
        "   ldr d1, [x0, %0+56]     \n"
        "   fmadd d1, d0, d2, d1    \n"
        "   ldr d0, [x0, %0+64]     \n"
        "   fmadd d0, d1, d2, d0    \n"
        "   ldr x0, [sp], 8         \n"
        "   ldp d1, d2, [sp], 16    \n"
        "   ret                     \n"
        "   .ltorg                  \n"::"i"(C_COS_COEFF*8)
    );
}

void PolyCosineSingle(void);
void  __attribute__((used)) stub_PolyCosineSingle(void)
{
    asm volatile(
        "   .align 4                \n"
        "   .globl PolyCosineSingle \n"
        "PolyCosineSingle:          \n"
        "   stp d1, d2, [sp, #-16]! \n"
        "   str x0, [sp, #-8]!      \n"
        "   ldr x0,=constants       \n"
        "   fmul d2, d0, d0         \n"
        "   ldp d0, d1, [x0, %0]    \n"
        "   fmadd d1, d0, d2, d1    \n"
        "   ldr d0, [x0, %0+16]     \n"
        "   fmadd d0, d1, d2, d0    \n"
        "   ldr d1, [x0, %0+24]     \n"
        "   fmadd d1, d0, d2, d1    \n"
        "   ldr d0, [x0, %0+32]     \n"
        "   fmadd d0, d1, d2, d0    \n"
        "   ldr x0, [sp], 8         \n"
        "   ldp d1, d2, [sp], 16    \n"
        "   ret                     \n"
        "   .ltorg                  \n"::"i"(C_COS_COEFF_SINGLE*8)
    );
}


#else

void PolySine(void);
void  __attribute__((used)) stub_PolySine(void)
{
    asm volatile(
        "   .align 4                \n"
        "PolySine:                  \n"
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

void PolySineSingle(void);
void  __attribute__((used)) stub_PolySineSingle(void)
{
    asm volatile(
        "   .align 4                \n"
        "PolySineSingle:            \n"
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

void PolyCosine(void);
void  __attribute__((used)) stub_PolyCosine(void)
{
    asm volatile(
        "   .align 4                \n"
        "PolyCosine:                \n"
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

void PolyCosineSingle(void);
void  __attribute__((used)) stub_PolyCosineSingle(void)
{
    asm volatile(
        "   .align 4                \n"
        "PolyCosineSingle:          \n"
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

#endif

enum FPUOpSize {
    SIZE_L = 0,
    SIZE_S = 1,
    SIZE_X = 2,
    SIZE_P = 3,
    SIZE_W = 4,
    SIZE_D = 5,
    SIZE_B = 6,
    SIZE_Pdyn = 7,
};

uint8_t FPUDataSize[] = {
    [SIZE_L] = 4,
    [SIZE_S] = 4,
    [SIZE_X] = 12,
    [SIZE_P] = 12,
    [SIZE_Pdyn] = 12,
    [SIZE_W] = 2,
    [SIZE_D] = 8,
    [SIZE_B] = 1
};

int FPSR_Update_Needed(uint16_t **m68k_ptr)
{
    uint16_t *ptr = *m68k_ptr;

#if 1
    int cnt = 0;

    while((BE16(*ptr) & 0xfe00) != 0xf200)
    {
        if (cnt++ > 15)
            return 1;
        if (M68K_IsBranch(ptr))
            return 1;
        
        int len = M68K_GetINSNLength(ptr);
        if (len <= 0)
            return 1;
        ptr += len;
    }
#else
    if ((BE16(*ptr) & 0xfe00) != 0xf200)
        return 1;
#endif

    uint16_t opcode = BE16(ptr[0]);
    uint16_t opcode2 = BE16(ptr[1]);

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
    if ((opcode & 0xffc0) == 0xf200 && (opcode2 & 0xc3ff) == 0x8000) /* FMOVEM special */
        return 1;
    if ((opcode & 0xffc0) == 0xf240 && (opcode2 & 0xffc0) == 0) /* FScc */
        return 1;
    if ((opcode & 0xfff8) == 0xf278 && (opcode2 & 0xffc0) == 0) /* FTRAPcc */
        return 1;
    if ((opcode & 0xffc0) == 0xf200 && (opcode2 & 0xe000) == 0x6000) /* FMOVE to MEM */
        return 1;
    if (opcode == 0xf280 && opcode2 == 0x0000) /* FNOP */
        return 1;
    if ((opcode & 0xffc0) == 0xf340) /* FRESTORE */
        return 1;
    if ((opcode & 0xffc0) == 0xf300) /* FSAVE */
        return 1;

    return 0;
}

/* Allocates FPU register and fetches data according to the R/M field of the FPU opcode */
uint32_t *FPU_FetchData(uint32_t *ptr, uint16_t **m68k_ptr, uint8_t *reg, uint16_t opcode,
        uint16_t opcode2, uint8_t *ext_count)
{
    union {
        uint64_t u64;
        uint32_t u32[2];
    } u;

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
                    *ptr++ = fmsr(*reg, int_reg);
                    *ptr++ = fcvtds(*reg, *reg);
                    RA_FreeARMRegister(&ptr, int_reg);
                    break;

                case SIZE_L:
                    ptr = EMIT_LoadFromEffectiveAddress(ptr, 4, &int_reg, ea, *m68k_ptr, ext_count, 1, NULL);
                    *ptr++ = scvtf_32toD(*reg, int_reg);
                    RA_FreeARMRegister(&ptr, int_reg);
                    break;

                case SIZE_W:
                    ptr = EMIT_LoadFromEffectiveAddress(ptr, 2, &int_reg, ea, *m68k_ptr, ext_count, 1, NULL);
                    tmp_reg = RA_AllocARMRegister(&ptr);
                    *ptr++ = sxth(tmp_reg, int_reg);
                    *ptr++ = scvtf_32toD(*reg, tmp_reg);
                    RA_FreeARMRegister(&ptr, tmp_reg);
                    RA_FreeARMRegister(&ptr, int_reg);
                    break;

                case SIZE_B:
                    ptr = EMIT_LoadFromEffectiveAddress(ptr, 1, &int_reg, ea, *m68k_ptr, ext_count, 1, NULL);
                    tmp_reg = RA_AllocARMRegister(&ptr);
                    *ptr++ = sxtb(tmp_reg, int_reg);
                    *ptr++ = scvtf_32toD(*reg, tmp_reg);
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
                {
                    int8_t off = 4;
                    ptr = EMIT_GetOffsetPC(ptr, &off);
                    *ptr++ = flds(*reg, REG_PC, off);
                    *ptr++ = fcvtds(*reg, *reg);
                    *ext_count += 2;
                    break;
                }

                case SIZE_L:
                    ptr = EMIT_LoadFromEffectiveAddress(ptr, 4, &int_reg, ea, *m68k_ptr, ext_count, 0, NULL);
                    *ptr++ = scvtf_32toD(*reg, int_reg);
                    break;
                
                case SIZE_W:
                {
                    int_reg = RA_AllocARMRegister(&ptr);
                    int16_t imm = (int16_t)BE16((*m68k_ptr)[1]);
                    *ptr++ = movw_immed_u16(int_reg, imm & 0xffff);
                    if (imm < 0)
                        *ptr++ = movt_immed_u16(int_reg, 0xffff);
                    *ptr++ = scvtf_32toD(*reg, int_reg);
                    *ext_count += 1;
                    break;
                }

                case SIZE_B:
                {
                    int_reg = RA_AllocARMRegister(&ptr);
                    int8_t imm = (int8_t)BE16((*m68k_ptr)[1]);
                    *ptr++ = mov_immed_s8(int_reg, imm);
                    *ptr++ = scvtf_32toD(*reg, int_reg);
                    *ext_count += 1;
                    break;
                }

                case SIZE_D:
                {
                    int8_t off = 4;
                    ptr = EMIT_GetOffsetPC(ptr, &off);
                    *ptr++ = fldd(*reg, REG_PC, off);
                    *ext_count += 4;
                    break;
                }

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
                        *ptr++ = fldd(*reg, int_reg, 0);
                        *ext_count += 4;
                        break;

                    case SIZE_X:
                        ptr = EMIT_Load96bitFP(ptr, *reg, int_reg, 0);
                        *ext_count += 6;
                        break;

                    case SIZE_P:
                        u.u64 = (uintptr_t)PackedToDouble;

                        ptr = EMIT_SaveRegFrame(ptr, (RA_GetTempAllocMask() | REG_PROTECT | 7));

                        *ptr++ = ldr64_offset(int_reg, 0, 0);
                        *ptr++ = ldr64_offset(int_reg, 1, 8);
                        *ptr++ = adr(30, 20);
                        *ptr++ = ldr64_pcrel(2, 2);
                        *ptr++ = br(2);

                        *ptr++ = u.u32[0];
                        *ptr++ = u.u32[1];

                        *ptr++ = fcpyd(*reg, 0);

                        ptr = EMIT_RestoreRegFrame(ptr, (RA_GetTempAllocMask() | REG_PROTECT | 7));
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
            int8_t pre_sz = 0;
            int8_t post_sz = 0;

            if (mode == 4 || mode == 3)
                ptr = EMIT_LoadFromEffectiveAddress(ptr, 0, &int_reg, opcode & 0x3f, *m68k_ptr, ext_count, 0, NULL);
            else
                ptr = EMIT_LoadFromEffectiveAddress(ptr, 0, &int_reg, opcode & 0x3f, *m68k_ptr, ext_count, 1, &imm_offset);

            /* Pre index? Adjust base register accordingly */
            if (mode == 4) {
                pre_sz = FPUDataSize[size];

                if ((pre_sz == 1) && ((opcode & 7) == 7))
                    pre_sz = 2;
                
                pre_sz = -pre_sz;
            }
            /* Post index? Adjust base register accordingly */
            else if (mode == 3) {
                post_sz = FPUDataSize[size];

                if ((post_sz == 1) && ((opcode & 7) == 7))
                    post_sz = 2;
            }

            switch (size)
            {
                case SIZE_P:
                    {
                        if (pre_sz)
                        {
                            *ptr++ = sub_immed(int_reg, int_reg, -pre_sz);
                        }
                        if (imm_offset < -255 || imm_offset > 251) {
                            uint8_t off = RA_AllocARMRegister(&ptr);

                            if (imm_offset > -4096 && imm_offset < 0)
                            {
                                *ptr++ = sub_immed(off, int_reg, -imm_offset);
                            }
                            else if (imm_offset >= 0 && imm_offset < 4096)
                            {
                                *ptr++ = add_immed(off, int_reg, imm_offset);
                            }
                            else
                            {
                                *ptr++ = movw_immed_u16(off, (imm_offset) & 0xffff);
                                imm_offset >>= 16;
                                if (imm_offset)
                                    *ptr++ = movt_immed_u16(off, (imm_offset) & 0xffff);
                                *ptr++ = add_reg(off, int_reg, off, LSL, 0);
                            }
                            RA_FreeARMRegister(&ptr, int_reg);
                            int_reg = off;
                            imm_offset = 0;
                        }

                        u.u64 = (uintptr_t)PackedToDouble;

                        ptr = EMIT_SaveRegFrame(ptr, (RA_GetTempAllocMask() | REG_PROTECT | 7));

                        *ptr++ = ldur64_offset(int_reg, 0, imm_offset);
                        *ptr++ = ldur64_offset(int_reg, 1, imm_offset + 8);
                        *ptr++ = adr(30, 20);
                        *ptr++ = ldr64_pcrel(2, 2);
                        *ptr++ = br(2);

                        *ptr++ = u.u32[0];
                        *ptr++ = u.u32[1];

                        *ptr++ = fcpyd(*reg, 0);

                        ptr = EMIT_RestoreRegFrame(ptr, (RA_GetTempAllocMask() | REG_PROTECT | 7));

                        if (post_sz)
                        {
                            *ptr++ = add_immed(int_reg, int_reg, post_sz);
                        }
                    }
                    break;

                case SIZE_X:
                    {
                        if (pre_sz)
                        {
                            *ptr++ = sub_immed(int_reg, int_reg, -pre_sz);
                        }
                        if (imm_offset < -255 || imm_offset > 251) {
                            uint8_t off = RA_AllocARMRegister(&ptr);

                            if (imm_offset > -4096 && imm_offset < 0)
                            {
                                *ptr++ = sub_immed(off, int_reg, -imm_offset);
                            }
                            else if (imm_offset >= 0 && imm_offset < 4096)
                            {
                                *ptr++ = add_immed(off, int_reg, imm_offset);
                            }
                            else
                            {
                                *ptr++ = movw_immed_u16(off, (imm_offset) & 0xffff);
                                imm_offset >>= 16;
                                if (imm_offset)
                                    *ptr++ = movt_immed_u16(off, (imm_offset) & 0xffff);
                                *ptr++ = add_reg(off, int_reg, off, LSL, 0);
                            }
                            RA_FreeARMRegister(&ptr, int_reg);
                            int_reg = off;
                            imm_offset = 0;
                        }

                        ptr = EMIT_Load96bitFP(ptr, *reg, int_reg, imm_offset);

                        if (post_sz)
                        {
                            *ptr++ = add_immed(int_reg, int_reg, post_sz);
                        }
                    }
                    break;
                case SIZE_D:
                    {
                        if (pre_sz)
                        {
                            *ptr++ = fldd_preindex(*reg, int_reg, pre_sz);
                        }
                        else if (post_sz)
                        {
                            *ptr++ = fldd_postindex(*reg, int_reg, post_sz);
                        }
                        else if (imm_offset >= -255 && imm_offset <= 255)
                        {
                            *ptr++ = fldd(*reg, int_reg, imm_offset);
                        }
                        else if (imm_offset >= 0 && imm_offset < 32760 && !(imm_offset & 7))
                        {
                            *ptr++ = fldd_pimm(*reg, int_reg, imm_offset >> 3);
                        }
                        else
                        {
                            uint8_t off = RA_AllocARMRegister(&ptr);
                            if (imm_offset > -4096 && imm_offset < 0)
                            {
                                *ptr++ = sub_immed(off, int_reg, -imm_offset);
                            }
                            else if (imm_offset >= 0 && imm_offset < 4096)
                            {
                                *ptr++ = add_immed(off, int_reg, imm_offset);
                            }
                            else
                            {
                                *ptr++ = movw_immed_u16(off, (imm_offset) & 0xffff);
                                imm_offset >>= 16;
                                if (imm_offset)
                                    *ptr++ = movt_immed_u16(off, (imm_offset) & 0xffff);
                                *ptr++ = add_reg(off, int_reg, off, LSL, 0);
                            }
                            *ptr++ = fldd(*reg, off, 0);
                            RA_FreeARMRegister(&ptr, off);
                        }
                    }
                    break;
                case SIZE_S:
                    if (pre_sz)
                    {
                        *ptr++ = flds_preindex(*reg, int_reg, pre_sz);
                    }
                    else if (post_sz)
                    {
                        *ptr++ = flds_postindex(*reg, int_reg, post_sz);
                    }
                    else if (imm_offset >= -255 && imm_offset <= 255)
                    {
                        *ptr++ = flds(*reg, int_reg, imm_offset);
                    }
                    else if (imm_offset >= 0 && imm_offset < 16380 && !(imm_offset & 3))
                    {
                        *ptr++ = flds_pimm(*reg, int_reg, imm_offset >> 2);
                    }
                    else
                    {
                        uint8_t off = RA_AllocARMRegister(&ptr);
                        if (imm_offset > -4096 && imm_offset < 0)
                        {
                            *ptr++ = sub_immed(off, int_reg, -imm_offset);
                        }
                        else if (imm_offset >= 0 && imm_offset < 4096)
                        {
                            *ptr++ = add_immed(off, int_reg, imm_offset);
                        }
                        else
                        {
                            *ptr++ = movw_immed_u16(off, (imm_offset) & 0xffff);
                            imm_offset >>= 16;
                            if (imm_offset)
                                *ptr++ = movt_immed_u16(off, (imm_offset) & 0xffff);
                            *ptr++ = add_reg(off, int_reg, off, LSL, 0);
                        }
                        *ptr++ = flds(*reg, off, 0);
                        RA_FreeARMRegister(&ptr, off);
                    }
                    *ptr++ = fcvtds(*reg, *reg);
                    break;
                case SIZE_L:
                    val_reg = RA_AllocARMRegister(&ptr);

                    if (pre_sz)
                    {
                        *ptr++ = ldr_offset_preindex(int_reg, val_reg, pre_sz);
                    }
                    else if (post_sz)
                    {
                        *ptr++ = ldr_offset_postindex(int_reg, val_reg, pre_sz);
                    }
                    else if (imm_offset >= -255 && imm_offset <= 255)
                    {
                        *ptr++ = ldur_offset(int_reg, val_reg, imm_offset);
                    }
                    else if (imm_offset >= 0 && imm_offset < 16380 && !(imm_offset & 3))
                    {
                        *ptr++ = ldr_offset(int_reg, val_reg, imm_offset);
                    }
                    else
                    {
                        uint8_t off = RA_AllocARMRegister(&ptr);
                        if (imm_offset > -4096 && imm_offset < 0)
                        {
                            *ptr++ = sub_immed(off, int_reg, -imm_offset);
                        }
                        else if (imm_offset >= 0 && imm_offset < 4096)
                        {
                            *ptr++ = add_immed(off, int_reg, imm_offset);
                        }
                        else
                        {
                            *ptr++ = movw_immed_u16(off, (imm_offset) & 0xffff);
                            imm_offset >>= 16;
                            if (imm_offset)
                                *ptr++ = movt_immed_u16(off, (imm_offset) & 0xffff);
                            *ptr++ = add_reg(off, int_reg, off, LSL, 0);
                        }
                        *ptr++ = ldr_offset(off, val_reg, 0);
                        RA_FreeARMRegister(&ptr, off);
                    }
                    *ptr++ = scvtf_32toD(*reg, val_reg);
                    break;
                case SIZE_W:
                    val_reg = RA_AllocARMRegister(&ptr);

                    if (pre_sz)
                    {
                        *ptr++ = ldrh_offset_preindex(int_reg, val_reg, pre_sz);
                    }
                    else if (post_sz)
                    {
                        *ptr++ = ldrh_offset_postindex(int_reg, val_reg, pre_sz);
                    }
                    else if (imm_offset >= -255 && imm_offset <= 255)
                    {
                        *ptr++ = ldurh_offset(int_reg, val_reg, imm_offset);
                    }
                    else if (imm_offset >= 0 && imm_offset < 8190 && !(imm_offset & 1))
                    {
                        *ptr++ = ldrh_offset(int_reg, val_reg, imm_offset);
                    }
                    else
                    {
                        uint8_t off = RA_AllocARMRegister(&ptr);
                        if (imm_offset > -4096 && imm_offset < 0)
                        {
                            *ptr++ = sub_immed(off, int_reg, -imm_offset);
                        }
                        else if (imm_offset >= 0 && imm_offset < 4096)
                        {
                            *ptr++ = add_immed(off, int_reg, imm_offset);
                        }
                        else
                        {
                            *ptr++ = movw_immed_u16(off, (imm_offset) & 0xffff);
                            imm_offset >>= 16;
                            if (imm_offset)
                                *ptr++ = movt_immed_u16(off, (imm_offset) & 0xffff);
                            *ptr++ = add_reg(off, int_reg, off, LSL, 0);
                        }
                        *ptr++ = ldrh_offset(off, val_reg, 0);
                        RA_FreeARMRegister(&ptr, off);
                    }
                    *ptr++ = sxth(val_reg, val_reg);
                    *ptr++ = scvtf_32toD(*reg, val_reg);
                    break;
                case SIZE_B:
                    val_reg = RA_AllocARMRegister(&ptr);

                    if (pre_sz)
                    {
                        *ptr++ = ldrb_offset_preindex(int_reg, val_reg, pre_sz);
                    }
                    else if (post_sz)
                    {
                        *ptr++ = ldrb_offset_postindex(int_reg, val_reg, pre_sz);
                    }
                    else if (imm_offset >= -255 && imm_offset <= 255)
                    {
                        *ptr++ = ldurb_offset(int_reg, val_reg, imm_offset);
                    }
                    else if (imm_offset >= 0 && imm_offset < 4096)
                    {
                        *ptr++ = ldrb_offset(int_reg, val_reg, imm_offset);
                    }
                    else
                    {
                        uint8_t off = RA_AllocARMRegister(&ptr);
                        if (imm_offset > -4096 && imm_offset < 0)
                        {
                            *ptr++ = sub_immed(off, int_reg, -imm_offset);
                        }
                        else if (imm_offset >= 0 && imm_offset < 4096)
                        {
                            *ptr++ = add_immed(off, int_reg, imm_offset);
                        }
                        else
                        {
                            *ptr++ = movw_immed_u16(off, (imm_offset) & 0xffff);
                            imm_offset >>= 16;
                            if (imm_offset)
                                *ptr++ = movt_immed_u16(off, (imm_offset) & 0xffff);
                            *ptr++ = add_reg(off, int_reg, off, LSL, 0);
                        }
                        *ptr++ = ldrb_offset(off, val_reg, 0);
                        RA_FreeARMRegister(&ptr, off);
                    }
                    *ptr++ = sxtb(val_reg, val_reg);
                    *ptr++ = scvtf_32toD(*reg, val_reg);
                    break;
                default:
                    break;
            }

            if ((mode == 4) || (mode == 3))
            {
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
                *ptr++ = fcvtsd(vfp_reg, reg);                  // Convert double to single
                *ptr++ = fmrs(int_reg, vfp_reg);                // Move single to destination ARM reg
                RA_FreeARMRegister(&ptr, int_reg);
                break;

            case SIZE_L:
                int_reg = RA_MapM68kRegisterForWrite(&ptr, ea & 7); // Destination for write only, discard contents
                // No rounding mode specified? Round to zero?
                *ptr++ = fcvtzs_Dto32(int_reg, reg);
                RA_FreeARMRegister(&ptr, int_reg);
                break;

            case SIZE_W:
                int_reg = RA_MapM68kRegister(&ptr, ea & 7);
                tmp_reg = RA_AllocARMRegister(&ptr);
                *ptr++ = fcvtzs_Dto32(tmp_reg, reg);
                *ptr++ = bfi(int_reg, tmp_reg, 0, 16);
                RA_SetDirtyM68kRegister(&ptr, ea & 7);
                RA_FreeARMRegister(&ptr, tmp_reg);
                RA_FreeARMRegister(&ptr, int_reg);
                break;

            case SIZE_B:
                int_reg = RA_MapM68kRegister(&ptr, ea & 7);
                tmp_reg = RA_AllocARMRegister(&ptr);
                *ptr++ = fcvtzs_Dto32(tmp_reg, reg);
                *ptr++ = bfi(int_reg, tmp_reg, 0, 8);
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
        int8_t pre_sz = 0;
        int8_t post_sz = 0;
        int8_t k = 0;
        uint8_t tmp64 = RA_AllocARMRegister(&ptr);
        uint8_t tmp32 = RA_AllocARMRegister(&ptr);
        union {
            uint64_t u64;
            uint32_t u32[2];
        } u;

        if (mode == 4 || mode == 3)
            ptr = EMIT_LoadFromEffectiveAddress(ptr, 0, &int_reg, opcode & 0x3f, *m68k_ptr, ext_count, 0, NULL);
        else
            ptr = EMIT_LoadFromEffectiveAddress(ptr, 0, &int_reg, opcode & 0x3f, *m68k_ptr, ext_count, 1, &imm_offset);

        /* Pre index? Adjust base register accordingly */
        if (mode == 4) {
            pre_sz = FPUDataSize[size];

            if ((pre_sz == 1) && ((opcode & 7) == 7))
                pre_sz = 2;
            
            pre_sz = -pre_sz;
        }
        /* Post index? Adjust base register accordingly */
        else if (mode == 3) {
            post_sz = FPUDataSize[size];

            if ((post_sz == 1) && ((opcode & 7) == 7))
                post_sz = 2;
        }

        switch (size)
        {
            case SIZE_P:
                u.u64 = (uintptr_t)DoubleToPacked;
                k = opcode2 & 0x7f;

                if (pre_sz)
                {
                    *ptr++ = sub_immed(int_reg, int_reg, -pre_sz);
                }

                if (reg != 0) {
                    *ptr++ = fcpyd(0, reg);
                }

                if (imm_offset >= -255 && imm_offset <= 251)
                {
                    ptr = EMIT_SaveRegFrame(ptr, (RA_GetTempAllocMask() | REG_PROTECT | 3 | (1 << 19)));

                    *ptr++ = mov_reg(19, int_reg);
                    *ptr++ = mov_immed_s8(0, k);
                    *ptr++ = adr(30, 20);
                    *ptr++ = ldr64_pcrel(1, 2);
                    *ptr++ = br(1);

                    *ptr++ = u.u32[0];
                    *ptr++ = u.u32[1];

                    *ptr++ = ror64(1, 1, 32);
                    *ptr++ = stur64_offset(19, 0, imm_offset);
                    *ptr++ = stur_offset(19, 1, imm_offset + 8);

                    ptr = EMIT_RestoreRegFrame(ptr, (RA_GetTempAllocMask() | REG_PROTECT | 3 | (1 << 19)));
                }
                else
                {
                    uint8_t off = 19;
                    
                    ptr = EMIT_SaveRegFrame(ptr, (RA_GetTempAllocMask() | REG_PROTECT | 3 | (1 << 19)));

                    if (imm_offset > -4096 && imm_offset < 0)
                    {
                        *ptr++ = sub_immed(off, int_reg, -imm_offset);
                    }
                    else if (imm_offset >= 0 && imm_offset < 4096)
                    {
                        *ptr++ = add_immed(off, int_reg, imm_offset);
                    }
                    else
                    {
                        *ptr++ = movw_immed_u16(off, (imm_offset) & 0xffff);
                        imm_offset >>= 16;
                        if (imm_offset)
                            *ptr++ = movt_immed_u16(off, (imm_offset) & 0xffff);
                        *ptr++ = add_reg(off, int_reg, off, LSL, 0);
                    }

                    *ptr++ = mov_immed_s8(0, k);
                    *ptr++ = adr(30, 20);
                    *ptr++ = ldr64_pcrel(1, 2);
                    *ptr++ = br(1);

                    *ptr++ = u.u32[0];
                    *ptr++ = u.u32[1];

                    *ptr++ = ror64(1, 1, 32);
                    *ptr++ = stur64_offset(19, 0, 0);
                    *ptr++ = stur_offset(19, 1, 8);

                    ptr = EMIT_RestoreRegFrame(ptr, (RA_GetTempAllocMask() | REG_PROTECT | 3 | (1 << 19)));
                }

                if (post_sz)
                {
                    *ptr++ = add_immed(int_reg, int_reg, post_sz);
                }
                break;
            
            case SIZE_Pdyn:
                u.u64 = (uintptr_t)DoubleToPacked;
                k = RA_MapM68kRegister(&ptr, (opcode2 >> 4) & 7);

                if (pre_sz)
                {
                    *ptr++ = sub_immed(int_reg, int_reg, -pre_sz);
                }

                if (reg != 0) {
                    *ptr++ = fcpyd(0, reg);
                }

                if (imm_offset >= -255 && imm_offset <= 251)
                {
                    ptr = EMIT_SaveRegFrame(ptr, (RA_GetTempAllocMask() | REG_PROTECT | 3 | (1 << 19)));

                    *ptr++ = mov_reg(0, k);
                    *ptr++ = mov_reg(19, int_reg);
                    *ptr++ = adr(30, 20);
                    *ptr++ = ldr64_pcrel(1, 2);
                    *ptr++ = br(1);

                    *ptr++ = u.u32[0];
                    *ptr++ = u.u32[1];

                    *ptr++ = ror64(1, 1, 32);
                    *ptr++ = stur64_offset(19, 0, imm_offset);
                    *ptr++ = stur_offset(19, 1, imm_offset + 8);

                    ptr = EMIT_RestoreRegFrame(ptr, (RA_GetTempAllocMask() | REG_PROTECT | 3 | (1 << 19)));
                }
                else
                {
                    uint8_t off = 19;
                    
                    ptr = EMIT_SaveRegFrame(ptr, (RA_GetTempAllocMask() | REG_PROTECT | 3 | (1 << 19)));

                    *ptr++ = mov_reg(0, k);

                    if (imm_offset > -4096 && imm_offset < 0)
                    {
                        *ptr++ = sub_immed(off, int_reg, -imm_offset);
                    }
                    else if (imm_offset >= 0 && imm_offset < 4096)
                    {
                        *ptr++ = add_immed(off, int_reg, imm_offset);
                    }
                    else
                    {
                        *ptr++ = movw_immed_u16(off, (imm_offset) & 0xffff);
                        imm_offset >>= 16;
                        if (imm_offset)
                            *ptr++ = movt_immed_u16(off, (imm_offset) & 0xffff);
                        *ptr++ = add_reg(off, int_reg, off, LSL, 0);
                    }

                    *ptr++ = adr(30, 20);
                    *ptr++ = ldr64_pcrel(1, 2);
                    *ptr++ = br(1);

                    *ptr++ = u.u32[0];
                    *ptr++ = u.u32[1];

                    *ptr++ = ror64(1, 1, 32);
                    *ptr++ = stur64_offset(19, 0, 0);
                    *ptr++ = stur_offset(19, 1, 8);

                    ptr = EMIT_RestoreRegFrame(ptr, (RA_GetTempAllocMask() | REG_PROTECT | 3 | (1 << 19)));
                }

                if (post_sz)
                {
                    *ptr++ = add_immed(int_reg, int_reg, post_sz);
                }
                break;

            case SIZE_X:
                {
                    if (pre_sz)
                    {
                        *ptr++ = sub_immed(int_reg, int_reg, -pre_sz);
                    }
                    if (imm_offset >= -255 && imm_offset <= 251)
                    {
                        ptr = EMIT_Store96bitFP(ptr, reg, int_reg, imm_offset);
                    }
                    else
                    {
                        uint8_t off = RA_AllocARMRegister(&ptr);
                        if (imm_offset > -4096 && imm_offset < 0)
                        {
                            *ptr++ = sub_immed(off, int_reg, -imm_offset);
                        }
                        else if (imm_offset >= 0 && imm_offset < 4096)
                        {
                            *ptr++ = add_immed(off, int_reg, imm_offset);
                        }
                        else
                        {
                            *ptr++ = movw_immed_u16(off, (imm_offset) & 0xffff);
                            imm_offset >>= 16;
                            if (imm_offset)
                                *ptr++ = movt_immed_u16(off, (imm_offset) & 0xffff);
                            *ptr++ = add_reg(off, int_reg, off, LSL, 0);
                        }

                        ptr = EMIT_Store96bitFP(ptr, reg, off, 0);
                        RA_FreeARMRegister(&ptr, off);
                    }
                    if (post_sz)
                    {
                        *ptr++ = add_immed(int_reg, int_reg, post_sz);
                    }
                }
                break;
            case SIZE_D:
                {
                    if (pre_sz)
                    {
                        *ptr++ = fstd_preindex(reg, int_reg, pre_sz);
                    }
                    else if (post_sz)
                    {
                        *ptr++ = fstd_postindex(reg, int_reg, post_sz);
                    }
                    else if (imm_offset >= -255 && imm_offset <= 255)
                    {
                        *ptr++ = fstd(reg, int_reg, imm_offset);
                    }
                    else if (imm_offset >= 0 && imm_offset < 32760 && !(imm_offset & 7))
                    {
                        *ptr++ = fstd_pimm(reg, int_reg, imm_offset >> 3);
                    }
                    else
                    {
                        uint8_t off = RA_AllocARMRegister(&ptr);
                        if (imm_offset > -4096 && imm_offset < 0)
                        {
                            *ptr++ = sub_immed(off, int_reg, -imm_offset);
                        }
                        else if (imm_offset >= 0 && imm_offset < 4096)
                        {
                            *ptr++ = add_immed(off, int_reg, imm_offset);
                        }
                        else
                        {
                            *ptr++ = movw_immed_u16(off, (imm_offset) & 0xffff);
                            imm_offset >>= 16;
                            if (imm_offset)
                                *ptr++ = movt_immed_u16(off, (imm_offset) & 0xffff);
                            *ptr++ = add_reg(off, int_reg, off, LSL, 0);
                        }
                        *ptr++ = fstd(reg, off, 0);
                        RA_FreeARMRegister(&ptr, off);
                    }
                }
                break;
            case SIZE_S:
                *ptr++ = fcvtsd(vfp_reg, reg);
                if (pre_sz)
                {
                    *ptr++ = fsts_preindex(vfp_reg, int_reg, pre_sz);
                }
                else if (post_sz)
                {
                    *ptr++ = fsts_postindex(vfp_reg, int_reg, post_sz);
                }
                else if (imm_offset >= -255 && imm_offset <= 255)
                {
                    *ptr++ = fsts(vfp_reg, int_reg, imm_offset);
                }
                else if (imm_offset >= 0 && imm_offset < 16380 && !(imm_offset & 3))
                {
                    *ptr++ = fsts_pimm(vfp_reg, int_reg, imm_offset >> 2);
                }
                else
                {
                    uint8_t off = RA_AllocARMRegister(&ptr);
                    if (imm_offset > -4096 && imm_offset < 0)
                    {
                        *ptr++ = sub_immed(off, int_reg, -imm_offset);
                    }
                    else if (imm_offset >= 0 && imm_offset < 4096)
                    {
                        *ptr++ = add_immed(off, int_reg, imm_offset);
                    }
                    else
                    {
                        *ptr++ = movw_immed_u16(off, (imm_offset) & 0xffff);
                        imm_offset >>= 16;
                        if (imm_offset)
                            *ptr++ = movt_immed_u16(off, (imm_offset) & 0xffff);
                        *ptr++ = add_reg(off, int_reg, off, LSL, 0);
                    }
                    *ptr++ = fsts(vfp_reg, off, 0);
                    RA_FreeARMRegister(&ptr, off);
                }
                break;
            case SIZE_L:
                val_reg = RA_AllocARMRegister(&ptr);
                *ptr++ = fcvtzs_Dto32(val_reg, reg);

                if (pre_sz)
                {
                    *ptr++ = str_offset_preindex(int_reg, val_reg, pre_sz);
                }
                else if (post_sz)
                {
                    *ptr++ = str_offset_postindex(int_reg, val_reg, post_sz);
                }
                else if (imm_offset >= -255 && imm_offset <= 255)
                {
                    *ptr++ = stur_offset(int_reg, val_reg, imm_offset);
                }
                else if (imm_offset >= 0 && imm_offset < 16380 && !(imm_offset & 3))
                {
                    *ptr++ = str_offset(int_reg, val_reg, imm_offset);
                }
                else
                {
                    uint8_t off = RA_AllocARMRegister(&ptr);
                    if (imm_offset > -4096 && imm_offset < 0)
                    {
                        *ptr++ = sub_immed(off, int_reg, -imm_offset);
                    }
                    else if (imm_offset >= 0 && imm_offset < 4096)
                    {
                        *ptr++ = add_immed(off, int_reg, imm_offset);
                    }
                    else
                    {
                        *ptr++ = movw_immed_u16(off, (imm_offset) & 0xffff);
                        imm_offset >>= 16;
                        if (imm_offset)
                            *ptr++ = movt_immed_u16(off, (imm_offset) & 0xffff);
                        *ptr++ = add_reg(off, int_reg, off, LSL, 0);
                    }
                    *ptr++ = str_offset(off, val_reg, 0);
                    RA_FreeARMRegister(&ptr, off);
                }
                break;
            case SIZE_W:
                val_reg = RA_AllocARMRegister(&ptr);
                *ptr++ = fcvtzs_Dto32(val_reg, reg);

                if (pre_sz)
                {
                    *ptr++ = strh_offset_preindex(int_reg, val_reg, pre_sz);
                }
                else if (post_sz)
                {
                    *ptr++ = strh_offset_postindex(int_reg, val_reg, post_sz);
                }
                else if (imm_offset >= -255 && imm_offset <= 255)
                {
                    *ptr++ = sturh_offset(int_reg, val_reg, imm_offset);
                }
                else if (imm_offset >= 0 && imm_offset < 8190 && !(imm_offset & 1))
                {
                    *ptr++ = strh_offset(int_reg, val_reg, imm_offset);
                }
                else
                {
                    uint8_t off = RA_AllocARMRegister(&ptr);
                    if (imm_offset > -4096 && imm_offset < 0)
                    {
                        *ptr++ = sub_immed(off, int_reg, -imm_offset);
                    }
                    else if (imm_offset >= 0 && imm_offset < 4096)
                    {
                        *ptr++ = add_immed(off, int_reg, imm_offset);
                    }
                    else
                    {
                        *ptr++ = movw_immed_u16(off, (imm_offset) & 0xffff);
                        imm_offset >>= 16;
                        if (imm_offset)
                            *ptr++ = movt_immed_u16(off, (imm_offset) & 0xffff);
                        *ptr++ = add_reg(off, int_reg, off, LSL, 0);
                    }
                    *ptr++ = strh_offset(off, val_reg, 0);
                    RA_FreeARMRegister(&ptr, off);
                }
                break;
            case SIZE_B:
                val_reg = RA_AllocARMRegister(&ptr);
                *ptr++ = fcvtzs_Dto32(val_reg, reg);

                if (pre_sz)
                {
                    *ptr++ = strb_offset_preindex(int_reg, val_reg, pre_sz);
                }
                else if (post_sz)
                {
                    *ptr++ = strb_offset_postindex(int_reg, val_reg, post_sz);
                }
                else if (imm_offset >= -255 && imm_offset <= 255)
                {
                    *ptr++ = sturb_offset(int_reg, val_reg, imm_offset);
                }
                else if (imm_offset >= 0 && imm_offset < 4096)
                {
                    *ptr++ = strb_offset(int_reg, val_reg, imm_offset);
                }
                else
                {
                    uint8_t off = RA_AllocARMRegister(&ptr);
                    if (imm_offset > -4096 && imm_offset < 0)
                    {
                        *ptr++ = sub_immed(off, int_reg, -imm_offset);
                    }
                    else if (imm_offset >= 0 && imm_offset < 4096)
                    {
                        *ptr++ = add_immed(off, int_reg, imm_offset);
                    }
                    else
                    {
                        *ptr++ = movw_immed_u16(off, (imm_offset) & 0xffff);
                        imm_offset >>= 16;
                        if (imm_offset)
                            *ptr++ = movt_immed_u16(off, (imm_offset) & 0xffff);
                        *ptr++ = add_reg(off, int_reg, off, LSL, 0);
                    }
                    *ptr++ = strb_offset(off, val_reg, 0);
                    RA_FreeARMRegister(&ptr, off);
                }
                break;
            default:
                break;
        }

        if ((mode == 4) || (mode == 3))
        {
            RA_SetDirtyM68kRegister(&ptr, 8 + (opcode & 7));
        }

        RA_FreeARMRegister(&ptr, tmp32);
        RA_FreeARMRegister(&ptr, tmp64);
        RA_FreeFPURegister(&ptr, vfp_reg);
        RA_FreeARMRegister(&ptr, int_reg);
        RA_FreeARMRegister(&ptr, val_reg);
    }

    return ptr;
}

#ifdef __aarch64__
void clear_entire_dcache(void);
/* Clean and invalidate entire data cache, code after ARMv8 architecture reference manual */
void  __attribute__((used)) __clear_entire_dcache(void)
{
    asm volatile(
"       .globl clear_entire_dcache      \n"
"clear_entire_dcache:                   \n"
"       stp     x0, x1, [sp, #-112]!    \n"
"       stp     x2, x3, [sp, #16]       \n"
"       stp     x4, x5, [sp, #2*16]     \n"
"       stp     x7, x8, [sp, #3*16]     \n"
"       stp     x9, x10, [sp, #4*16]    \n"
"       stp     x11, x16, [sp, #5*16]   \n"
"       str     x17, [sp, #6*16]        \n"
"       mrs     x0, CLIDR_EL1           \n"
"       and     w3, w0, #0x07000000     \n" // Get 2 x Level of Coherence
"       lsr     w3, w3, #23             \n"
"       cbz     w3, 5f                  \n"
"       mov     w10, #0                 \n" // W10 = 2 x cache level
"       mov     w8, #1                  \n" // W8 = constant 0b1
"1:     add     w2, w10, w10, lsr #1    \n" // Calculate 3 x cache level
"       lsr     w1, w0, w2              \n" // extract 3-bit cache type for this level
"       and     w1, w1, #0x7            \n"
"       cmp     w1, #2                  \n"
"       b.lt    4f                      \n" // No data or unified cache at this level
"       msr     CSSELR_EL1, x10         \n" // Select this cache level
"       isb                             \n" // Synchronize change of CSSELR
"       mrs     x1, CCSIDR_EL1          \n" // Read CCSIDR
"       and     w2, w1, #7              \n" // W2 = log2(linelen)-4
"       add     w2, w2, #4              \n" // W2 = log2(linelen)
"       ubfx    w4, w1, #3, #10         \n" // W4 = max way number, right aligned
"       clz     w5, w4                  \n" // W5 = 32-log2(ways), bit position of way in DC operand
"       lsl     w9, w4, w5              \n" // W9 = max way number, aligned to position in DC operand
"       lsl     w16, w8, w5             \n" // W16 = amount to decrement way number per iteration
"2:     ubfx    w7, w1, #13, #15        \n" // W7 = max set number, right aligned
"       lsl     w7, w7, w2              \n" // W7 = max set number, aligned to position in DC operand
"       lsl     w17, w8, w2             \n" // W17 = amount to decrement set number per iteration
"3:     orr     w11, w10, w9            \n" // W11 = combine way number and cache number ...
"       orr     w11, w11, w7            \n" // ... and set number for DC operand
"       dc      CISW, x11               \n" // Do data cache clean and invalidate by set and way
"       subs    w7, w7, w17             \n" // Decrement set number
"       b.ge    3b                      \n"
"       subs    x9, x9, x16             \n" // Decrement way number
"       b.ge    2b                      \n"
"4:     add     w10, w10, #2            \n" // Increment 2 x cache level
"       cmp     w3, w10                 \n"
"       dsb     sy                      \n" // Ensure completion of previous cache maintenance instruction
"       b.gt    1b                      \n"
"5:                                     \n"
"       ldp     x2, x3, [sp, #16]       \n"
"       ldp     x4, x5, [sp, #2*16]     \n"
"       ldp     x7, x8, [sp, #3*16]     \n"
"       ldp     x9, x10, [sp, #4*16]    \n"
"       ldp     x11, x16, [sp, #5*16]   \n"
"       ldr     x17, [sp, #6*16]        \n"
"       ldp     x0, x1, [sp], #112      \n"
"       ret                             \n"
"       .ltorg                          \n"
    );
}

void invalidate_entire_dcache(void);
/* Invalidate entire data cache, code after ARMv8 architecture reference manual */
void __attribute__((used)) __invalidate_entire_dcache(void)
{
    asm volatile(
"       .globl  invalidate_entire_dcache\n"
"invalidate_entire_dcache:              \n"
"       stp     x0, x1, [sp, #-112]!    \n"
"       stp     x2, x3, [sp, #16]       \n"
"       stp     x4, x5, [sp, #2*16]     \n"
"       stp     x7, x8, [sp, #3*16]     \n"
"       stp     x9, x10, [sp, #4*16]    \n"
"       stp     x11, x16, [sp, #5*16]   \n"
"       str     x17, [sp, #6*16]        \n"
"       mrs     x0, CLIDR_EL1           \n"
"       and     w3, w0, #0x07000000     \n" // Get 2 x Level of Coherence
"       lsr     w3, w3, #23             \n"
"       cbz     w3, 5f                  \n"
"       mov     w10, #0                 \n" // W10 = 2 x cache level
"       mov     w8, #1                  \n" // W8 = constant 0b1
"1:     add     w2, w10, w10, lsr #1    \n" // Calculate 3 x cache level
"       lsr     w1, w0, w2              \n" // extract 3-bit cache type for this level
"       and     w1, w1, #0x7            \n"
"       cmp     w1, #2                  \n"
"       b.lt    4f                      \n" // No data or unified cache at this level
"       msr     CSSELR_EL1, x10         \n" // Select this cache level
"       isb                             \n" // Synchronize change of CSSELR
"       mrs     x1, CCSIDR_EL1          \n" // Read CCSIDR
"       and     w2, w1, #7              \n" // W2 = log2(linelen)-4
"       add     w2, w2, #4              \n" // W2 = log2(linelen)
"       ubfx    w4, w1, #3, #10         \n" // W4 = max way number, right aligned
"       clz     w5, w4                  \n" // W5 = 32-log2(ways), bit position of way in DC operand
"       lsl     w9, w4, w5              \n" // W9 = max way number, aligned to position in DC operand
"       lsl     w16, w8, w5             \n" // W16 = amount to decrement way number per iteration
"2:     ubfx    w7, w1, #13, #15        \n" // W7 = max set number, right aligned
"       lsl     w7, w7, w2              \n" // W7 = max set number, aligned to position in DC operand
"       lsl     w17, w8, w2             \n" // W17 = amount to decrement set number per iteration
"3:     orr     w11, w10, w9            \n" // W11 = combine way number and cache number ...
"       orr     w11, w11, w7            \n" // ... and set number for DC operand
"       dc      ISW, x11                \n" // Do data cache invalidate by set and way
"       subs    w7, w7, w17             \n" // Decrement set number
"       b.ge    3b                      \n"
"       subs    x9, x9, x16             \n" // Decrement way number
"       b.ge    2b                      \n"
"4:     add     w10, w10, #2            \n" // Increment 2 x cache level
"       cmp     w3, w10                 \n"
"       dsb     sy                      \n" // Ensure completion of previous cache maintenance instruction
"       b.gt    1b                      \n"
"5:                                     \n"
"       ldp     x2, x3, [sp, #16]       \n"
"       ldp     x4, x5, [sp, #2*16]     \n"
"       ldp     x7, x8, [sp, #3*16]     \n"
"       ldp     x9, x10, [sp, #4*16]    \n"
"       ldp     x11, x16, [sp, #5*16]   \n"
"       ldr     x17, [sp, #6*16]        \n"
"       ldp     x0, x1, [sp], #112      \n"
"       ret                             \n"
"       .ltorg                          \n"
    );
}

#else
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
#endif

void __clear_cache(void *begin, void *end);

#define MAX_EPILOGUE_LENGTH 256
uint32_t icache_epilogue[MAX_EPILOGUE_LENGTH];

void *invalidate_instruction_cache(uintptr_t target_addr, uint16_t *pc, uint32_t *arm_pc)
{
    int i;
    uint16_t opcode = BE16(pc[0]);
    struct M68KTranslationUnit *u;
    struct Node *n, *next;
    extern struct List LRU;
    extern void *jit_tlsf;
    extern struct M68KState *__m68k_state;
    #ifndef __aarch64__
    extern uint32_t last_PC;
    #endif

    (void)jit_tlsf;

    //kprintf("[LINEF] ICache flush... Opcode=%04x, Target=%08x, PC=%08x, ARM PC=%p\n", opcode, target_addr, pc, arm_pc);
    // kprintf("[LINEF] ARM insn: %08x\n", *arm_pc);

    for (i=0; i < MAX_EPILOGUE_LENGTH; i++)
    {
        if (arm_pc[i] == 0xffffffff)
            break;

        icache_epilogue[i] = arm_pc[i];
    }

    //kprintf("[LINEF] Copied %d instructions of epilogue\n", i);
    __clear_cache(&icache_epilogue[0], &icache_epilogue[i]);

    #ifdef __aarch64__
    asm volatile("msr tpidr_el1,%0"::"r"(0xffffffff));
    #else
    last_PC = 0xffffffff;
    #endif

    /* Get the scope */
    switch (opcode & 0x18) {
        case 0x08:  /* Line */
            // kprintf("[LINEF] Invalidating line\n");
            ForeachNodeSafe(&LRU, n, next)
            {
                u = (struct M68KTranslationUnit *)((intptr_t)n - __builtin_offsetof(struct M68KTranslationUnit, mt_LRUNode));

                // If highest address of unit is lower than the begin flushed area, or lowest address of unit higher than the flushed area end
                // then skip the unit
                if ((uintptr_t)u->mt_M68kLow > ((target_addr + 16) & ~15) || (uintptr_t)u->mt_M68kHigh < (target_addr & ~15))
                    continue;

                if (__m68k_state->JIT_CONTROL & JCCF_SOFT)
                {
                    // Weak cflush. Generate invalid entry address instead of flushing. Fault handler will
                    // verify block checksum and eventually discard it
                    uintptr_t e = (uintptr_t)u->mt_ARMEntryPoint;
                    e &= 0x00ffffffffffffffULL;
                    e |= 0xaa00000000000000ULL;
                    u->mt_ARMEntryPoint = (void*)e;
                }
                else
                {
                    // kprintf("[LINEF] Unit %p, %08x-%08x match! Removing.\n", u, u->mt_M68kLow, u->mt_M68kHigh);
                    REMOVE(&u->mt_LRUNode);
                    REMOVE(&u->mt_HashNode);
                    tlsf_free(jit_tlsf, u);

                    __m68k_state->JIT_UNIT_COUNT--;
                    __m68k_state->JIT_CACHE_FREE = tlsf_get_free_size(jit_tlsf);
                }
            }
            break;
        case 0x10:  /* Page */
            // kprintf("[LINEF] Invalidating page\n");
            ForeachNodeSafe(&LRU, n, next)
            {
                u = (struct M68KTranslationUnit *)((intptr_t)n - __builtin_offsetof(struct M68KTranslationUnit, mt_LRUNode));

                // If highest address of unit is lower than the begin flushed area, or lowest address of unit higher than the flushed area end
                // then skip the unit
                if ((uintptr_t)u->mt_M68kLow > ((target_addr + 4096) & ~4095) || (uintptr_t)u->mt_M68kHigh < (target_addr & ~4095))
                    continue;

                // kprintf("[LINEF] Unit %p, %08x-%08x match! Removing.\n", u, u->mt_M68kLow, u->mt_M68kHigh);

                if (__m68k_state->JIT_CONTROL & JCCF_SOFT)
                {
                    // Weak cflush. Generate invalid entry address instead of flushing. Fault handler will
                    // verify block checksum and eventually discard it
                    uintptr_t e = (uintptr_t)u->mt_ARMEntryPoint;
                    e &= 0x00ffffffffffffffULL;
                    e |= 0xaa00000000000000ULL;
                    u->mt_ARMEntryPoint = (void*)e;
                }
                else
                {
                    REMOVE(&u->mt_LRUNode);
                    REMOVE(&u->mt_HashNode);
                    tlsf_free(jit_tlsf, u);

                    __m68k_state->JIT_UNIT_COUNT--;
                    __m68k_state->JIT_CACHE_FREE = tlsf_get_free_size(jit_tlsf);
                }
            }
            break;
        case 0x18:  /* All */
            // kprintf("[LINEF] Invalidating all\n");            
            if (__m68k_state->JIT_CONTROL & JCCF_SOFT)
            {
                if (__m68k_state->JIT_UNIT_COUNT < __m68k_state->JIT_SOFTFLUSH_THRESH)
                {
                    ForeachNode(&LRU, n)
                    {
                        uintptr_t uptr = ((uintptr_t)n - __builtin_offsetof(struct M68KTranslationUnit, mt_LRUNode));
                        uptr += __builtin_offsetof(struct M68KTranslationUnit, mt_ARMEntryPoint);

                        // Weak cflush. Generate invalid entry address instead of flushing. Fault handler will
                        // verify block checksum and eventually discard it
                        *(uint8_t *)uptr = 0xaa;
                    }
                }
                else
                {
                    while ((n = REMHEAD(&LRU))) {
                        u = (struct M68KTranslationUnit *)((intptr_t)n - __builtin_offsetof(struct M68KTranslationUnit, mt_LRUNode));
                        // kprintf("[LINEF] Removing unit %p\n", u);                
                        REMOVE(&u->mt_HashNode);
                        tlsf_free(jit_tlsf, u);
                        
                        __m68k_state->JIT_UNIT_COUNT--;
                        __m68k_state->JIT_CACHE_FREE = tlsf_get_free_size(jit_tlsf);
                    }
                }
            }
            else
            {
                while ((n = REMHEAD(&LRU))) {
                    u = (struct M68KTranslationUnit *)((intptr_t)n - __builtin_offsetof(struct M68KTranslationUnit, mt_LRUNode));
                    // kprintf("[LINEF] Removing unit %p\n", u);                
                    REMOVE(&u->mt_HashNode);
                    tlsf_free(jit_tlsf, u);

                    __m68k_state->JIT_UNIT_COUNT--;
                    __m68k_state->JIT_CACHE_FREE = tlsf_get_free_size(jit_tlsf);
                }
            }
            break;
    }

    return &icache_epilogue[0];
}

#ifdef __aarch64__
void trampoline_icache_invalidate(void);
void __attribute__((used)) __trampoline_icache_invalidate(void)
{
    asm volatile(".globl trampoline_icache_invalidate\ntrampoline_icache_invalidate: bl invalidate_instruction_cache\n\tbr x0");
}
#else
void __attribute__((naked)) trampoline_icache_invalidate(void)
{
    asm volatile("bl invalidate_instruction_cache\n\tbx r0");
}
#endif

uint32_t *EMIT_FPU(uint32_t *ptr, uint16_t **m68k_ptr, uint16_t *insn_consumed)
{
    uint16_t opcode = BE16((*m68k_ptr)[0]);
    uint16_t opcode2 = BE16((*m68k_ptr)[1]);
    uint8_t ext_count = 1;
    (*m68k_ptr)++;
    *insn_consumed = 1;

    /* FMOVECR reg */
    if (opcode == 0xf200 && (opcode2 & 0xfc00) == 0x5c00)
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
            *ptr++ = fmov_1(fp_dst);
        }
        else if (offset == C_ZERO) {
            *ptr++ = fmov_0(fp_dst);
        }
        else {
            u.d = constants[offset];
            *ptr++ = fldd_pcrel(fp_dst, 2);
            *ptr++ = b(3);
            *ptr++ = u.u32[0];
            *ptr++ = u.u32[1];
        }
        ptr = EMIT_AdvancePC(ptr, 2 * (ext_count + 1));
        (*m68k_ptr) += ext_count;

        if (FPSR_Update_Needed(m68k_ptr))
        {
            uint8_t fpsr = RA_ModifyFPSR(&ptr);

            if (offset == C_ZERO)
            {
                *ptr++ = bic_immed(fpsr, fpsr, 4, 32 - FPSRB_NAN);
                *ptr++ = orr_immed(fpsr, fpsr, 1, 32 - FPSRB_Z);
            }
            else if (offset < C_ZERO || offset >= C_LN2)
            {
                *ptr++ = bic_immed(fpsr, fpsr, 4, 32 - FPSRB_NAN);
            }
            else
            {
                *ptr++ = fcmpzd(fp_dst);
                ptr = EMIT_GetFPUFlags(ptr, fpsr);
            }
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
            uint8_t fpsr = RA_ModifyFPSR(&ptr);

            *ptr++ = fcmpzd(fp_dst);
            ptr = EMIT_GetFPUFlags(ptr, fpsr);
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
            uint8_t fpsr = RA_ModifyFPSR(&ptr);

            *ptr++ = fcmpzd(fp_dst);
            ptr = EMIT_GetFPUFlags(ptr, fpsr);
        }
    }
    /* FBcc */
    else if ((opcode & 0xff80) == 0xf280)
    {
        uint8_t fpsr = RA_GetFPSR(&ptr);
        uint8_t predicate = opcode & 0x3f;
        uint8_t success_condition = 0;
        uint8_t tmp_cc = 0xff;
        uint32_t *tmpptr;

        /* Test predicate with masked signalling bit, operations are the same */
        switch (predicate & 0x0f)
        {
            case F_CC_EQ:
                *ptr++ = tst_immed(fpsr, 1, 31 & (32 - FPSRB_Z));
                success_condition = A64_CC_NE;
                break;
            case F_CC_NE:
                *ptr++ = tst_immed(fpsr, 1, 31 & (32 - FPSRB_Z));
                success_condition = A64_CC_EQ;
                break;
            case F_CC_OGT:
                tmp_cc = RA_AllocARMRegister(&ptr);
                *ptr++ = mov_immed_u16(tmp_cc, (FPSR_Z | FPSR_N | FPSR_NAN) >> 16, 1);
                *ptr++ = tst_reg(fpsr, tmp_cc, LSL, 0);
                success_condition = ARM_CC_EQ;
                break;
            case F_CC_ULE:
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
                *ptr++ = mvn_reg(tmp_cc, tmp_cc, LSL, 0); //eor_immed(tmp_cc, tmp_cc, 1, 31 & (32 - FPSRB_N)); // !N -> N
                *ptr++ = tst_immed(tmp_cc, 1, 31 & (32 - FPSRB_N));
                success_condition = A64_CC_NE;
                break;
            case F_CC_ULT: // NAN == 1 || (N == 1 && Z == 0)
                tmp_cc = RA_AllocARMRegister(&ptr);
                *ptr++ = tst_immed(fpsr, 1, 31 & (32 - FPSRB_NAN));
                *ptr++ = b_cc(A64_CC_NE, 4);
                *ptr++ = eor_immed(tmp_cc, fpsr, 1, 31 & (32 - FPSRB_Z)); // Invert Z
                *ptr++ = and_reg(tmp_cc, fpsr, tmp_cc, LSL, 1); // !Z & N -> N
                *ptr++ = tst_immed(tmp_cc, 1, 31 & (32 - FPSRB_N));
                success_condition = A64_CC_NE;
                break;
            case F_CC_OLT: // N == 1 && (NAN == 0 && Z == 0)
                tmp_cc = RA_AllocARMRegister(&ptr);
                *ptr++ = orr_reg(tmp_cc, fpsr, fpsr, LSL, 2); // NAN | Z -> Z
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

        uint8_t pc_yes = RA_AllocARMRegister(&ptr);
        uint8_t pc_no = RA_AllocARMRegister(&ptr);

        if (branch_offset > 0 && branch_offset < 4096)
            *ptr++ = add_immed(pc_yes, REG_PC, branch_offset);
        else if (branch_offset > -4096 && branch_offset < 0)
            *ptr++ = sub_immed(pc_yes, REG_PC, -branch_offset);
        else if (branch_offset != 0) {
            *ptr++ = movw_immed_u16(reg, branch_offset);
            if ((branch_offset >> 16) & 0xffff)
                *ptr++ = movt_immed_u16(reg, (branch_offset >> 16) & 0xffff);
            *ptr++ = add_reg(pc_yes, REG_PC, reg, LSL, 0);
        }
        else { *ptr++ = mov_reg(pc_yes, REG_PC); }

        branch_target += branch_offset - local_pc_off;

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
            *ptr++ = add_immed(pc_no, REG_PC, local_pc_off_16);
        else if (local_pc_off_16 > -256 && local_pc_off_16 < 0)
            *ptr++ = sub_immed(pc_no, REG_PC, -local_pc_off_16);
        else if (local_pc_off_16 != 0) {
            *ptr++ = movw_immed_u16(reg, local_pc_off_16);
            if ((local_pc_off_16 >> 16) & 0xffff)
                *ptr++ = movt_immed_u16(reg, local_pc_off_16 >> 16);
            *ptr++ = add_reg(pc_no, REG_PC, reg, LSL, 0);
        }
        *ptr++ = csel(REG_PC, pc_yes, pc_no, success_condition);
        RA_FreeARMRegister(&ptr, pc_yes);
        RA_FreeARMRegister(&ptr, pc_no);
        tmpptr = ptr;
#if EMU68_DEF_BRANCH_AUTO
        if(
            branch_target < (intptr_t)*m68k_ptr &&
            ((intptr_t)*m68k_ptr - branch_target) < EMU68_DEF_BRANCH_AUTO_RANGE
        )
            *ptr++ = b_cc(success_condition, 1);
        else
            *ptr++ = b_cc(success_condition^1, 1);
#else
#if EMU68_DEF_BRANCH_TAKEN
        *ptr++ = b_cc(success_condition, 1);
#else
        *ptr++ = b_cc(success_condition^1, 1);
#endif
#endif

#if EMU68_DEF_BRANCH_AUTO
        if(
            branch_target < (intptr_t)*m68k_ptr &&
            ((intptr_t)*m68k_ptr - branch_target) < EMU68_DEF_BRANCH_AUTO_RANGE
        )
            *m68k_ptr = (uint16_t *)branch_target;
#else
#if EMU68_DEF_BRANCH_TAKEN
        *m68k_ptr = (uint16_t *)branch_target;
#endif
#endif
        RA_FreeARMRegister(&ptr, reg);
        *ptr++ = (uint32_t)(uintptr_t)tmpptr;
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
            uint8_t fpsr = RA_ModifyFPSR(&ptr);
            ptr = EMIT_GetFPUFlags(ptr, fpsr);
        }
    }
    /* FDIV */
    else if ((opcode & 0xffc0) == 0xf200 && ((opcode2 & 0xa07f) == 0x0020 || (opcode2 & 0xa07b) == 0x0060))
    {
        uint8_t fp_src = 0xff;
        uint8_t fp_dst = (opcode2 >> 7) & 7;

        ptr = FPU_FetchData(ptr, m68k_ptr, &fp_src, opcode, opcode2, &ext_count);
        fp_dst = RA_MapFPURegister(&ptr, fp_dst);

        *ptr++ = fdivd(fp_dst, fp_dst, fp_src);

        RA_SetDirtyFPURegister(&ptr, fp_dst);

        RA_FreeFPURegister(&ptr, fp_src);

        ptr = EMIT_AdvancePC(ptr, 2 * (ext_count + 1));
        (*m68k_ptr) += ext_count;

        if (FPSR_Update_Needed(m68k_ptr))
        {
            uint8_t fpsr = RA_ModifyFPSR(&ptr);

            *ptr++ = fcmpzd(fp_dst);
            ptr = EMIT_GetFPUFlags(ptr, fpsr);
        }
    }
    /* FSGLDIV */
    else if ((opcode & 0xffc0) == 0xf200 && ((opcode2 & 0xa07f) == 0x0024))
    {
        uint8_t fp_src = 0xff;
        uint8_t fp_dst = (opcode2 >> 7) & 7;

        ptr = FPU_FetchData(ptr, m68k_ptr, &fp_src, opcode, opcode2, &ext_count);
        fp_dst = RA_MapFPURegister(&ptr, fp_dst);

        *ptr++ = fdivd(fp_dst, fp_dst, fp_src);
        *ptr++ = fcvtsd(fp_dst, fp_dst);
        *ptr++ = fcvtds(fp_dst, fp_dst);

        RA_SetDirtyFPURegister(&ptr, fp_dst);

        RA_FreeFPURegister(&ptr, fp_src);

        ptr = EMIT_AdvancePC(ptr, 2 * (ext_count + 1));
        (*m68k_ptr) += ext_count;

        if (FPSR_Update_Needed(m68k_ptr))
        {
            uint8_t fpsr = RA_ModifyFPSR(&ptr);

            *ptr++ = fcmpzd(fp_dst);
            ptr = EMIT_GetFPUFlags(ptr, fpsr);
        }
    }
    /* FSINCOS */
    else if ((opcode & 0xffc0) == 0xf200 && ((opcode2 & 0xa078) == 0x0030))
    {
        uint8_t fp_src = 0;
        uint8_t fp_dst_sin = (opcode2 >> 7) & 7;
        uint8_t fp_dst_cos = opcode2 & 7;

        ptr = FPU_FetchData(ptr, m68k_ptr, &fp_src, opcode, opcode2, &ext_count);
        fp_dst_sin = RA_MapFPURegisterForWrite(&ptr, fp_dst_sin);
        fp_dst_cos = RA_MapFPURegisterForWrite(&ptr, fp_dst_cos);

        union {
            uint64_t u64;
            uint32_t u32[2];
        } u;

        u.u64 = (uintptr_t)sincos;

        if (fp_src != 0) {
            *ptr++ = fcpyd(0, fp_src);
        }

        ptr = EMIT_SaveRegFrame(ptr, RA_GetTempAllocMask() | REG_PROTECT);

        *ptr++ = adr(30, 20);
        *ptr++ = ldr64_pcrel(0, 2);
        *ptr++ = br(0);

        *ptr++ = u.u32[0];
        *ptr++ = u.u32[1];

        *ptr++ = fcpyd(fp_dst_cos, 1);
        *ptr++ = fcpyd(fp_dst_sin, 0);

        ptr = EMIT_RestoreRegFrame(ptr, RA_GetTempAllocMask() | REG_PROTECT);

        RA_FreeFPURegister(&ptr, fp_src);

        ptr = EMIT_AdvancePC(ptr, 2 * (ext_count + 1));
        (*m68k_ptr) += ext_count;

        if (FPSR_Update_Needed(m68k_ptr))
        {
            uint8_t fpsr = RA_ModifyFPSR(&ptr);

            *ptr++ = fcmpzd(fp_dst_sin);
            ptr = EMIT_GetFPUFlags(ptr, fpsr);
        }

        *ptr++ = INSN_TO_LE(0xfffffff0);
    }
    /* FINT */
    else if ((opcode & 0xffc0) == 0xf200 && (opcode2 & 0xa07f) == 0x0001)
    {
        uint8_t fp_src = 0xff;
        uint8_t fp_dst = (opcode2 >> 7) & 7;

        ptr = FPU_FetchData(ptr, m68k_ptr, &fp_src, opcode, opcode2, &ext_count);
        fp_dst = RA_MapFPURegisterForWrite(&ptr, fp_dst);

        *ptr++ = frint64x(fp_dst, fp_src);

        RA_FreeFPURegister(&ptr, fp_src);

        ptr = EMIT_AdvancePC(ptr, 2 * (ext_count + 1));
        (*m68k_ptr) += ext_count;

        if (FPSR_Update_Needed(m68k_ptr))
        {
            uint8_t fpsr = RA_ModifyFPSR(&ptr);

            *ptr++ = fcmpzd(fp_dst);
            ptr = EMIT_GetFPUFlags(ptr, fpsr);
        }
    }
    /* FGETEXP */
    else if ((opcode & 0xffc0) == 0xf200 && (opcode2 & 0xa07f) == 0x001e)
    {
        uint8_t fp_src = 0xff;
        uint8_t fp_dst = (opcode2 >> 7) & 7;
        uint8_t tmp = RA_AllocARMRegister(&ptr);

        ptr = FPU_FetchData(ptr, m68k_ptr, &fp_src, opcode, opcode2, &ext_count);
        fp_dst = RA_MapFPURegisterForWrite(&ptr, fp_dst);

        *ptr++ = mov_simd_to_reg(tmp, fp_src, TS_D, 0);
        *ptr++ = ror64(tmp, tmp, 52);
        *ptr++ = and_immed(tmp, tmp, 11, 0);
        *ptr++ = sub_immed(tmp, tmp, 0x3ff);
        *ptr++ = scvtf_32toD(fp_dst, tmp);

        RA_FreeFPURegister(&ptr, fp_src);
        RA_FreeARMRegister(&ptr, tmp);

        ptr = EMIT_AdvancePC(ptr, 2 * (ext_count + 1));
        (*m68k_ptr) += ext_count;

        if (FPSR_Update_Needed(m68k_ptr))
        {
            uint8_t fpsr = RA_ModifyFPSR(&ptr);

            *ptr++ = fcmpzd(fp_dst);
            ptr = EMIT_GetFPUFlags(ptr, fpsr);
        }
    }
    /* FGETMAN */
    else if ((opcode & 0xffc0) == 0xf200 && (opcode2 & 0xa07f) == 0x001f)
    {
        uint8_t fp_src = 0xff;
        uint8_t fp_dst = (opcode2 >> 7) & 7;
        uint8_t tmp = RA_AllocARMRegister(&ptr);

        ptr = FPU_FetchData(ptr, m68k_ptr, &fp_src, opcode, opcode2, &ext_count);
        fp_dst = RA_MapFPURegisterForWrite(&ptr, fp_dst);

        *ptr++ = mov_simd_to_reg(tmp, fp_src, TS_D, 0);
        *ptr++ = bic64_immed(tmp, tmp, 11, 12, 1);
        *ptr++ = orr64_immed(tmp, tmp, 10, 12, 1);
        *ptr++ = mov_reg_to_simd(fp_dst, TS_D, 0, tmp);

        RA_FreeFPURegister(&ptr, fp_src);
        RA_FreeARMRegister(&ptr, tmp);

        ptr = EMIT_AdvancePC(ptr, 2 * (ext_count + 1));
        (*m68k_ptr) += ext_count;

        if (FPSR_Update_Needed(m68k_ptr))
        {
            uint8_t fpsr = RA_ModifyFPSR(&ptr);

            *ptr++ = fcmpzd(fp_dst);
            ptr = EMIT_GetFPUFlags(ptr, fpsr);
        }
    }
    /* FINTRZ */
    else if ((opcode & 0xffc0) == 0xf200 && (opcode2 & 0xa07f) == 0x0003)
    {
        uint8_t fp_src = 0xff;
        uint8_t fp_dst = (opcode2 >> 7) & 7;

        ptr = FPU_FetchData(ptr, m68k_ptr, &fp_src, opcode, opcode2, &ext_count);
        fp_dst = RA_MapFPURegisterForWrite(&ptr, fp_dst);

        *ptr++ = frint64z(fp_dst, fp_src);

        RA_FreeFPURegister(&ptr, fp_src);

        ptr = EMIT_AdvancePC(ptr, 2 * (ext_count + 1));
        (*m68k_ptr) += ext_count;

        if (FPSR_Update_Needed(m68k_ptr))
        {
            uint8_t fpsr = RA_ModifyFPSR(&ptr);

            *ptr++ = fcmpzd(fp_dst);
            ptr = EMIT_GetFPUFlags(ptr, fpsr);
        }
    }
    /* FSCALE */
    else if ((opcode & 0xffc0) == 0xf200 && (opcode2 & 0xa07f) == 0x0026)
    {
        uint8_t int_src = 0xff;
        uint8_t fp_src = 0xff;
        uint8_t fp_dst = (opcode2 >> 7) & 7;

        switch ((opcode2 >> 10) & 7)
        {
            case 0:
                fp_src = RA_AllocFPURegister(&ptr);
                ptr = EMIT_LoadFromEffectiveAddress(ptr, 4, &int_src, opcode & 0x3f, *m68k_ptr, &ext_count, 0, NULL);
                break;
            case 4:
                fp_src = RA_AllocFPURegister(&ptr);
                ptr = EMIT_LoadFromEffectiveAddress(ptr, 2, &int_src, opcode & 0x3f, *m68k_ptr, &ext_count, 0, NULL);
                break;
            case 6:
                fp_src = RA_AllocFPURegister(&ptr);
                ptr = EMIT_LoadFromEffectiveAddress(ptr, 1, &int_src, opcode & 0x3f, *m68k_ptr, &ext_count, 0, NULL);
                break;
            default:
                int_src = RA_AllocARMRegister(&ptr);
                ptr = FPU_FetchData(ptr, m68k_ptr, &fp_src, opcode, opcode2, &ext_count);
                *ptr++ = fcvtzs_Dto32(int_src, fp_src);
                break;
        }
      
        fp_dst = RA_MapFPURegisterForWrite(&ptr, fp_dst);

        *ptr++ = add_immed(int_src, int_src, 0x3ff);
        *ptr++ = lsl64(int_src, int_src, 52);
        *ptr++ = bic64_immed(int_src, int_src, 1, 1, 1);
        *ptr++ = mov_reg_to_simd(fp_src, TS_D, 0, int_src);
        *ptr++ = fmuld(fp_dst, fp_dst, fp_src);

        RA_FreeARMRegister(&ptr, int_src);
        RA_FreeFPURegister(&ptr, fp_src);

        ptr = EMIT_AdvancePC(ptr, 2 * (ext_count + 1));
        (*m68k_ptr) += ext_count;

        if (FPSR_Update_Needed(m68k_ptr))
        {
            uint8_t fpsr = RA_ModifyFPSR(&ptr);

            *ptr++ = fcmpzd(fp_dst);
            ptr = EMIT_GetFPUFlags(ptr, fpsr);
        }
    }
    /* FLOGN */
    else if ((opcode & 0xffc0) == 0xf200 && (opcode2 & 0xa07f) == 0x0014)
    {
        uint8_t fp_src = 0;
        uint8_t fp_dst = (opcode2 >> 7) & 7;

        ptr = FPU_FetchData(ptr, m68k_ptr, &fp_src, opcode, opcode2, &ext_count);
        fp_dst = RA_MapFPURegisterForWrite(&ptr, fp_dst);

        union {
            uint64_t u64;
            uint32_t u32[2];
        } u;

        u.u64 = (uintptr_t)log;

        if (fp_src != 0) {
            *ptr++ = fcpyd(0, fp_src);
        }

        ptr = EMIT_SaveRegFrame(ptr, RA_GetTempAllocMask() | REG_PROTECT);

        *ptr++ = adr(30, 20);
        *ptr++ = ldr64_pcrel(0, 2);
        *ptr++ = br(0);

        *ptr++ = u.u32[0];
        *ptr++ = u.u32[1];

        *ptr++ = fcpyd(fp_dst, 0);

        ptr = EMIT_RestoreRegFrame(ptr, RA_GetTempAllocMask() | REG_PROTECT);

        RA_FreeFPURegister(&ptr, fp_src);

        ptr = EMIT_AdvancePC(ptr, 2 * (ext_count + 1));
        (*m68k_ptr) += ext_count;

        if (FPSR_Update_Needed(m68k_ptr))
        {
            uint8_t fpsr = RA_ModifyFPSR(&ptr);

            *ptr++ = fcmpzd(fp_dst);
            ptr = EMIT_GetFPUFlags(ptr, fpsr);
        }

        *ptr++ = INSN_TO_LE(0xfffffff0);
    }
    /* FLOGNP1 */
    else if ((opcode & 0xffc0) == 0xf200 && (opcode2 & 0xa07f) == 0x0014)
    {
        uint8_t fp_src = 0;
        uint8_t fp_dst = (opcode2 >> 7) & 7;

        ptr = FPU_FetchData(ptr, m68k_ptr, &fp_src, opcode, opcode2, &ext_count);
        fp_dst = RA_MapFPURegisterForWrite(&ptr, fp_dst);

        union {
            uint64_t u64;
            uint32_t u32[2];
        } u;

        u.u64 = (uintptr_t)log1p;

        if (fp_src != 0) {
            *ptr++ = fcpyd(0, fp_src);
        }

        ptr = EMIT_SaveRegFrame(ptr, RA_GetTempAllocMask() | REG_PROTECT);

        *ptr++ = adr(30, 20);
        *ptr++ = ldr64_pcrel(0, 2);
        *ptr++ = br(0);

        *ptr++ = u.u32[0];
        *ptr++ = u.u32[1];

        *ptr++ = fcpyd(fp_dst, 0);

        ptr = EMIT_RestoreRegFrame(ptr, RA_GetTempAllocMask() | REG_PROTECT);

        RA_FreeFPURegister(&ptr, fp_src);

        ptr = EMIT_AdvancePC(ptr, 2 * (ext_count + 1));
        (*m68k_ptr) += ext_count;

        if (FPSR_Update_Needed(m68k_ptr))
        {
            uint8_t fpsr = RA_ModifyFPSR(&ptr);

            *ptr++ = fcmpzd(fp_dst);
            ptr = EMIT_GetFPUFlags(ptr, fpsr);
        }

        *ptr++ = INSN_TO_LE(0xfffffff0);
    }
    /* FLOG10 */
    else if ((opcode & 0xffc0) == 0xf200 && (opcode2 & 0xa07f) == 0x0015)
    {
        uint8_t fp_src = 0;
        uint8_t fp_dst = (opcode2 >> 7) & 7;

        ptr = FPU_FetchData(ptr, m68k_ptr, &fp_src, opcode, opcode2, &ext_count);
        fp_dst = RA_MapFPURegisterForWrite(&ptr, fp_dst);

        union {
            uint64_t u64;
            uint32_t u32[2];
        } u;

        u.u64 = (uintptr_t)log10;

        if (fp_src != 0) {
            *ptr++ = fcpyd(0, fp_src);
        }

        ptr = EMIT_SaveRegFrame(ptr, RA_GetTempAllocMask() | REG_PROTECT);

        *ptr++ = adr(30, 20);
        *ptr++ = ldr64_pcrel(0, 2);
        *ptr++ = br(0);

        *ptr++ = u.u32[0];
        *ptr++ = u.u32[1];

        *ptr++ = fcpyd(fp_dst, 0);

        ptr = EMIT_RestoreRegFrame(ptr, RA_GetTempAllocMask() | REG_PROTECT);

        RA_FreeFPURegister(&ptr, fp_src);

        ptr = EMIT_AdvancePC(ptr, 2 * (ext_count + 1));
        (*m68k_ptr) += ext_count;

        if (FPSR_Update_Needed(m68k_ptr))
        {
            uint8_t fpsr = RA_ModifyFPSR(&ptr);

            *ptr++ = fcmpzd(fp_dst);
            ptr = EMIT_GetFPUFlags(ptr, fpsr);
        }

        *ptr++ = INSN_TO_LE(0xfffffff0);
    }
    /* FLOG2 */
    else if ((opcode & 0xffc0) == 0xf200 && (opcode2 & 0xa07f) == 0x0016)
    {
        uint8_t fp_src = 0;
        uint8_t fp_dst = (opcode2 >> 7) & 7;

        ptr = FPU_FetchData(ptr, m68k_ptr, &fp_src, opcode, opcode2, &ext_count);
        fp_dst = RA_MapFPURegisterForWrite(&ptr, fp_dst);

        union {
            uint64_t u64;
            uint32_t u32[2];
        } u;

        u.u64 = (uintptr_t)log2;

        if (fp_src != 0) {
            *ptr++ = fcpyd(0, fp_src);
        }

        ptr = EMIT_SaveRegFrame(ptr, RA_GetTempAllocMask() | REG_PROTECT);

        *ptr++ = adr(30, 20);
        *ptr++ = ldr64_pcrel(0, 2);
        *ptr++ = br(0);

        *ptr++ = u.u32[0];
        *ptr++ = u.u32[1];

        *ptr++ = fcpyd(fp_dst, 0);

        ptr = EMIT_RestoreRegFrame(ptr, RA_GetTempAllocMask() | REG_PROTECT);

        RA_FreeFPURegister(&ptr, fp_src);

        ptr = EMIT_AdvancePC(ptr, 2 * (ext_count + 1));
        (*m68k_ptr) += ext_count;

        if (FPSR_Update_Needed(m68k_ptr))
        {
            uint8_t fpsr = RA_ModifyFPSR(&ptr);

            *ptr++ = fcmpzd(fp_dst);
            ptr = EMIT_GetFPUFlags(ptr, fpsr);
        }

        *ptr++ = INSN_TO_LE(0xfffffff0);
    }
    /* FETOX */
    else if ((opcode & 0xffc0) == 0xf200 && (opcode2 & 0xa07f) == 0x0010)
    {
        uint8_t fp_src = 0;
        uint8_t fp_dst = (opcode2 >> 7) & 7;

        ptr = FPU_FetchData(ptr, m68k_ptr, &fp_src, opcode, opcode2, &ext_count);
        fp_dst = RA_MapFPURegisterForWrite(&ptr, fp_dst);

        union {
            uint64_t u64;
            uint32_t u32[2];
        } u;

        u.u64 = (uintptr_t)exp;

        if (fp_src != 0) {
            *ptr++ = fcpyd(0, fp_src);
        }

        ptr = EMIT_SaveRegFrame(ptr, RA_GetTempAllocMask() | REG_PROTECT);

        *ptr++ = adr(30, 20);
        *ptr++ = ldr64_pcrel(0, 2);
        *ptr++ = br(0);

        *ptr++ = u.u32[0];
        *ptr++ = u.u32[1];

        *ptr++ = fcpyd(fp_dst, 0);

        ptr = EMIT_RestoreRegFrame(ptr, RA_GetTempAllocMask() | REG_PROTECT);

        RA_FreeFPURegister(&ptr, fp_src);

        ptr = EMIT_AdvancePC(ptr, 2 * (ext_count + 1));
        (*m68k_ptr) += ext_count;

        if (FPSR_Update_Needed(m68k_ptr))
        {
            uint8_t fpsr = RA_ModifyFPSR(&ptr);

            *ptr++ = fcmpzd(fp_dst);
            ptr = EMIT_GetFPUFlags(ptr, fpsr);
        }

        *ptr++ = INSN_TO_LE(0xfffffff0);
    }
    /* FETOXM1 */
    else if ((opcode & 0xffc0) == 0xf200 && (opcode2 & 0xa07f) == 0x0008)
    {
        uint8_t fp_src = 0;
        uint8_t fp_dst = (opcode2 >> 7) & 7;

        ptr = FPU_FetchData(ptr, m68k_ptr, &fp_src, opcode, opcode2, &ext_count);
        fp_dst = RA_MapFPURegisterForWrite(&ptr, fp_dst);

        union {
            uint64_t u64;
            uint32_t u32[2];
        } u;

        u.u64 = (uintptr_t)expm1;

        if (fp_src != 0) {
            *ptr++ = fcpyd(0, fp_src);
        }

        ptr = EMIT_SaveRegFrame(ptr, RA_GetTempAllocMask() | REG_PROTECT);

        *ptr++ = adr(30, 20);
        *ptr++ = ldr64_pcrel(0, 2);
        *ptr++ = br(0);

        *ptr++ = u.u32[0];
        *ptr++ = u.u32[1];

        *ptr++ = fcpyd(fp_dst, 0);

        ptr = EMIT_RestoreRegFrame(ptr, RA_GetTempAllocMask() | REG_PROTECT);

        RA_FreeFPURegister(&ptr, fp_src);

        ptr = EMIT_AdvancePC(ptr, 2 * (ext_count + 1));
        (*m68k_ptr) += ext_count;

        if (FPSR_Update_Needed(m68k_ptr))
        {
            uint8_t fpsr = RA_ModifyFPSR(&ptr);

            *ptr++ = fcmpzd(fp_dst);
            ptr = EMIT_GetFPUFlags(ptr, fpsr);
        }

        *ptr++ = INSN_TO_LE(0xfffffff0);
    }
    /* FSINH */
    else if ((opcode & 0xffc0) == 0xf200 && (opcode2 & 0xa07f) == 0x0002)
    {
        uint8_t fp_src = 0;
        uint8_t fp_dst = (opcode2 >> 7) & 7;

        ptr = FPU_FetchData(ptr, m68k_ptr, &fp_src, opcode, opcode2, &ext_count);
        fp_dst = RA_MapFPURegisterForWrite(&ptr, fp_dst);

        union {
            uint64_t u64;
            uint32_t u32[2];
        } u;

        u.u64 = (uintptr_t)sinh;

        if (fp_src != 0) {
            *ptr++ = fcpyd(0, fp_src);
        }

        ptr = EMIT_SaveRegFrame(ptr, RA_GetTempAllocMask() | REG_PROTECT);

        *ptr++ = adr(30, 20);
        *ptr++ = ldr64_pcrel(0, 2);
        *ptr++ = br(0);

        *ptr++ = u.u32[0];
        *ptr++ = u.u32[1];

        *ptr++ = fcpyd(fp_dst, 0);

        ptr = EMIT_RestoreRegFrame(ptr, RA_GetTempAllocMask() | REG_PROTECT);

        RA_FreeFPURegister(&ptr, fp_src);

        ptr = EMIT_AdvancePC(ptr, 2 * (ext_count + 1));
        (*m68k_ptr) += ext_count;

        if (FPSR_Update_Needed(m68k_ptr))
        {
            uint8_t fpsr = RA_ModifyFPSR(&ptr);

            *ptr++ = fcmpzd(fp_dst);
            ptr = EMIT_GetFPUFlags(ptr, fpsr);
        }

        *ptr++ = INSN_TO_LE(0xfffffff0);
    }
    /* FCOSH */
    else if ((opcode & 0xffc0) == 0xf200 && (opcode2 & 0xa07f) == 0x0019)
    {
        uint8_t fp_src = 0;
        uint8_t fp_dst = (opcode2 >> 7) & 7;

        ptr = FPU_FetchData(ptr, m68k_ptr, &fp_src, opcode, opcode2, &ext_count);
        fp_dst = RA_MapFPURegisterForWrite(&ptr, fp_dst);

        union {
            uint64_t u64;
            uint32_t u32[2];
        } u;

        u.u64 = (uintptr_t)cosh;

        if (fp_src != 0) {
            *ptr++ = fcpyd(0, fp_src);
        }

        ptr = EMIT_SaveRegFrame(ptr, RA_GetTempAllocMask() | REG_PROTECT);

        *ptr++ = adr(30, 20);
        *ptr++ = ldr64_pcrel(0, 2);
        *ptr++ = br(0);

        *ptr++ = u.u32[0];
        *ptr++ = u.u32[1];

        *ptr++ = fcpyd(fp_dst, 0);

        ptr = EMIT_RestoreRegFrame(ptr, RA_GetTempAllocMask() | REG_PROTECT);

        RA_FreeFPURegister(&ptr, fp_src);

        ptr = EMIT_AdvancePC(ptr, 2 * (ext_count + 1));
        (*m68k_ptr) += ext_count;

        if (FPSR_Update_Needed(m68k_ptr))
        {
            uint8_t fpsr = RA_ModifyFPSR(&ptr);

            *ptr++ = fcmpzd(fp_dst);
            ptr = EMIT_GetFPUFlags(ptr, fpsr);
        }

        *ptr++ = INSN_TO_LE(0xfffffff0);
    }
    /* FATAN */
    else if ((opcode & 0xffc0) == 0xf200 && (opcode2 & 0xa07f) == 0x000a)
    {
        uint8_t fp_src = 0;
        uint8_t fp_dst = (opcode2 >> 7) & 7;

        ptr = FPU_FetchData(ptr, m68k_ptr, &fp_src, opcode, opcode2, &ext_count);
        fp_dst = RA_MapFPURegisterForWrite(&ptr, fp_dst);

        union {
            uint64_t u64;
            uint32_t u32[2];
        } u;

        u.u64 = (uintptr_t)atan;

        if (fp_src != 0) {
            *ptr++ = fcpyd(0, fp_src);
        }

        ptr = EMIT_SaveRegFrame(ptr, RA_GetTempAllocMask() | REG_PROTECT);

        *ptr++ = adr(30, 20);
        *ptr++ = ldr64_pcrel(0, 2);
        *ptr++ = br(0);

        *ptr++ = u.u32[0];
        *ptr++ = u.u32[1];

        *ptr++ = fcpyd(fp_dst, 0);

        ptr = EMIT_RestoreRegFrame(ptr, RA_GetTempAllocMask() | REG_PROTECT);

        RA_FreeFPURegister(&ptr, fp_src);

        ptr = EMIT_AdvancePC(ptr, 2 * (ext_count + 1));
        (*m68k_ptr) += ext_count;

        if (FPSR_Update_Needed(m68k_ptr))
        {
            uint8_t fpsr = RA_ModifyFPSR(&ptr);

            *ptr++ = fcmpzd(fp_dst);
            ptr = EMIT_GetFPUFlags(ptr, fpsr);
        }

        *ptr++ = INSN_TO_LE(0xfffffff0);
    }
    /* FATANH */
    else if ((opcode & 0xffc0) == 0xf200 && (opcode2 & 0xa07f) == 0x000d)
    {
        uint8_t fp_src = 0;
        uint8_t fp_dst = (opcode2 >> 7) & 7;

        ptr = FPU_FetchData(ptr, m68k_ptr, &fp_src, opcode, opcode2, &ext_count);
        fp_dst = RA_MapFPURegisterForWrite(&ptr, fp_dst);

        union {
            uint64_t u64;
            uint32_t u32[2];
        } u;

        u.u64 = (uintptr_t)atanh;

        if (fp_src != 0) {
            *ptr++ = fcpyd(0, fp_src);
        }

        ptr = EMIT_SaveRegFrame(ptr, RA_GetTempAllocMask() | REG_PROTECT);

        *ptr++ = adr(30, 20);
        *ptr++ = ldr64_pcrel(0, 2);
        *ptr++ = br(0);

        *ptr++ = u.u32[0];
        *ptr++ = u.u32[1];

        *ptr++ = fcpyd(fp_dst, 0);

        ptr = EMIT_RestoreRegFrame(ptr, RA_GetTempAllocMask() | REG_PROTECT);

        RA_FreeFPURegister(&ptr, fp_src);

        ptr = EMIT_AdvancePC(ptr, 2 * (ext_count + 1));
        (*m68k_ptr) += ext_count;

        if (FPSR_Update_Needed(m68k_ptr))
        {
            uint8_t fpsr = RA_ModifyFPSR(&ptr);

            *ptr++ = fcmpzd(fp_dst);
            ptr = EMIT_GetFPUFlags(ptr, fpsr);
        }

        *ptr++ = INSN_TO_LE(0xfffffff0);
    }
    /* FACOS */
    else if ((opcode & 0xffc0) == 0xf200 && (opcode2 & 0xa07f) == 0x001c)
    {
        uint8_t fp_src = 0;
        uint8_t fp_dst = (opcode2 >> 7) & 7;

        ptr = FPU_FetchData(ptr, m68k_ptr, &fp_src, opcode, opcode2, &ext_count);
        fp_dst = RA_MapFPURegisterForWrite(&ptr, fp_dst);

        union {
            uint64_t u64;
            uint32_t u32[2];
        } u;

        u.u64 = (uintptr_t)acos;

        if (fp_src != 0) {
            *ptr++ = fcpyd(0, fp_src);
        }

        ptr = EMIT_SaveRegFrame(ptr, RA_GetTempAllocMask() | REG_PROTECT);

        *ptr++ = adr(30, 20);
        *ptr++ = ldr64_pcrel(0, 2);
        *ptr++ = br(0);

        *ptr++ = u.u32[0];
        *ptr++ = u.u32[1];

        *ptr++ = fcpyd(fp_dst, 0);

        ptr = EMIT_RestoreRegFrame(ptr, RA_GetTempAllocMask() | REG_PROTECT);

        RA_FreeFPURegister(&ptr, fp_src);

        ptr = EMIT_AdvancePC(ptr, 2 * (ext_count + 1));
        (*m68k_ptr) += ext_count;

        if (FPSR_Update_Needed(m68k_ptr))
        {
            uint8_t fpsr = RA_ModifyFPSR(&ptr);

            *ptr++ = fcmpzd(fp_dst);
            ptr = EMIT_GetFPUFlags(ptr, fpsr);
        }

        *ptr++ = INSN_TO_LE(0xfffffff0);
    }
    /* FASIN */
    else if ((opcode & 0xffc0) == 0xf200 && (opcode2 & 0xa07f) == 0x000c)
    {
        uint8_t fp_src = 0;
        uint8_t fp_dst = (opcode2 >> 7) & 7;

        ptr = FPU_FetchData(ptr, m68k_ptr, &fp_src, opcode, opcode2, &ext_count);
        fp_dst = RA_MapFPURegisterForWrite(&ptr, fp_dst);

        union {
            uint64_t u64;
            uint32_t u32[2];
        } u;

        u.u64 = (uintptr_t)asin;

        if (fp_src != 0) {
            *ptr++ = fcpyd(0, fp_src);
        }

        ptr = EMIT_SaveRegFrame(ptr, RA_GetTempAllocMask() | REG_PROTECT);

        *ptr++ = adr(30, 20);
        *ptr++ = ldr64_pcrel(0, 2);
        *ptr++ = br(0);

        *ptr++ = u.u32[0];
        *ptr++ = u.u32[1];

        *ptr++ = fcpyd(fp_dst, 0);

        ptr = EMIT_RestoreRegFrame(ptr, RA_GetTempAllocMask() | REG_PROTECT);

        RA_FreeFPURegister(&ptr, fp_src);

        ptr = EMIT_AdvancePC(ptr, 2 * (ext_count + 1));
        (*m68k_ptr) += ext_count;

        if (FPSR_Update_Needed(m68k_ptr))
        {
            uint8_t fpsr = RA_ModifyFPSR(&ptr);

            *ptr++ = fcmpzd(fp_dst);
            ptr = EMIT_GetFPUFlags(ptr, fpsr);
        }

        *ptr++ = INSN_TO_LE(0xfffffff0);
    }
    /* FTAN */
    else if ((opcode & 0xffc0) == 0xf200 && (opcode2 & 0xa07f) == 0x000f)
    {
        uint8_t fp_src = 0;
        uint8_t fp_dst = (opcode2 >> 7) & 7;

        ptr = FPU_FetchData(ptr, m68k_ptr, &fp_src, opcode, opcode2, &ext_count);
        fp_dst = RA_MapFPURegisterForWrite(&ptr, fp_dst);

        union {
            uint64_t u64;
            uint32_t u32[2];
        } u;

        u.u64 = (uintptr_t)tan;

        if (fp_src != 0) {
            *ptr++ = fcpyd(0, fp_src);
        }

        ptr = EMIT_SaveRegFrame(ptr, RA_GetTempAllocMask() | REG_PROTECT);

        *ptr++ = adr(30, 20);
        *ptr++ = ldr64_pcrel(0, 2);
        *ptr++ = br(0);

        *ptr++ = u.u32[0];
        *ptr++ = u.u32[1];

        *ptr++ = fcpyd(fp_dst, 0);

        ptr = EMIT_RestoreRegFrame(ptr, RA_GetTempAllocMask() | REG_PROTECT);

        RA_FreeFPURegister(&ptr, fp_src);

        ptr = EMIT_AdvancePC(ptr, 2 * (ext_count + 1));
        (*m68k_ptr) += ext_count;

        if (FPSR_Update_Needed(m68k_ptr))
        {
            uint8_t fpsr = RA_ModifyFPSR(&ptr);

            *ptr++ = fcmpzd(fp_dst);
            ptr = EMIT_GetFPUFlags(ptr, fpsr);
        }

        *ptr++ = INSN_TO_LE(0xfffffff0);
    }
    /* FTANH */
    else if ((opcode & 0xffc0) == 0xf200 && (opcode2 & 0xa07f) == 0x0009)
    {
        uint8_t fp_src = 0;
        uint8_t fp_dst = (opcode2 >> 7) & 7;

        ptr = FPU_FetchData(ptr, m68k_ptr, &fp_src, opcode, opcode2, &ext_count);
        fp_dst = RA_MapFPURegisterForWrite(&ptr, fp_dst);

        union {
            uint64_t u64;
            uint32_t u32[2];
        } u;

        u.u64 = (uintptr_t)tanh;

        if (fp_src != 0) {
            *ptr++ = fcpyd(0, fp_src);
        }

        ptr = EMIT_SaveRegFrame(ptr, RA_GetTempAllocMask() | REG_PROTECT);

        *ptr++ = adr(30, 20);
        *ptr++ = ldr64_pcrel(0, 2);
        *ptr++ = br(0);

        *ptr++ = u.u32[0];
        *ptr++ = u.u32[1];

        *ptr++ = fcpyd(fp_dst, 0);

        ptr = EMIT_RestoreRegFrame(ptr, RA_GetTempAllocMask() | REG_PROTECT);

        RA_FreeFPURegister(&ptr, fp_src);

        ptr = EMIT_AdvancePC(ptr, 2 * (ext_count + 1));
        (*m68k_ptr) += ext_count;

        if (FPSR_Update_Needed(m68k_ptr))
        {
            uint8_t fpsr = RA_ModifyFPSR(&ptr);

            *ptr++ = fcmpzd(fp_dst);
            ptr = EMIT_GetFPUFlags(ptr, fpsr);
        }

        *ptr++ = INSN_TO_LE(0xfffffff0);
    }
    /* FTENTOX */
    else if ((opcode & 0xffc0) == 0xf200 && (opcode2 & 0xa07f) == 0x0012)
    {
        uint8_t fp_src = 0;
        uint8_t fp_dst = (opcode2 >> 7) & 7;

        ptr = FPU_FetchData(ptr, m68k_ptr, &fp_src, opcode, opcode2, &ext_count);
        fp_dst = RA_MapFPURegisterForWrite(&ptr, fp_dst);

        union {
            uint64_t u64;
            uint32_t u32[2];
        } u;

        u.u64 = (uintptr_t)exp10;

        if (fp_src != 0) {
            *ptr++ = fcpyd(0, fp_src);
        }

        ptr = EMIT_SaveRegFrame(ptr, RA_GetTempAllocMask() | REG_PROTECT);
        
        *ptr++ = adr(30, 20);
        *ptr++ = ldr64_pcrel(0, 2);
        *ptr++ = br(0);

        *ptr++ = u.u32[0];
        *ptr++ = u.u32[1];

        *ptr++ = fcpyd(fp_dst, 0);

        ptr = EMIT_RestoreRegFrame(ptr, RA_GetTempAllocMask() | REG_PROTECT);

        RA_FreeFPURegister(&ptr, fp_src);

        ptr = EMIT_AdvancePC(ptr, 2 * (ext_count + 1));
        (*m68k_ptr) += ext_count;

        if (FPSR_Update_Needed(m68k_ptr))
        {
            uint8_t fpsr = RA_ModifyFPSR(&ptr);

            *ptr++ = fcmpzd(fp_dst);
            ptr = EMIT_GetFPUFlags(ptr, fpsr);
        }

        *ptr++ = INSN_TO_LE(0xfffffff0);
    }
    /* FTWOTOX */
    else if ((opcode & 0xffc0) == 0xf200 && (opcode2 & 0xa07f) == 0x0011)
    {
        uint8_t fp_src = 0;
        uint8_t fp_dst = (opcode2 >> 7) & 7;

        ptr = FPU_FetchData(ptr, m68k_ptr, &fp_src, opcode, opcode2, &ext_count);
        fp_dst = RA_MapFPURegisterForWrite(&ptr, fp_dst);

        union {
            uint64_t u64;
            uint32_t u32[2];
        } u;

        u.u64 = (uintptr_t)exp2;

        if (fp_src != 0) {
            *ptr++ = fcpyd(0, fp_src);
        }

        ptr = EMIT_SaveRegFrame(ptr, RA_GetTempAllocMask() | REG_PROTECT);

        *ptr++ = adr(30, 20);
        *ptr++ = ldr64_pcrel(0, 2);
        *ptr++ = br(0);

        *ptr++ = u.u32[0];
        *ptr++ = u.u32[1];

        *ptr++ = fcpyd(fp_dst, 0);

        ptr = EMIT_RestoreRegFrame(ptr, RA_GetTempAllocMask() | REG_PROTECT);

        RA_FreeFPURegister(&ptr, fp_src);

        ptr = EMIT_AdvancePC(ptr, 2 * (ext_count + 1));
        (*m68k_ptr) += ext_count;

        if (FPSR_Update_Needed(m68k_ptr))
        {
            uint8_t fpsr = RA_ModifyFPSR(&ptr);

            *ptr++ = fcmpzd(fp_dst);
            ptr = EMIT_GetFPUFlags(ptr, fpsr);
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

        /* Was such a construct really necessary */
#if 0
        ptr = FPU_FetchData(ptr, m68k_ptr, &fp_src, opcode, opcode2, &ext_count);
        fp_dst = RA_MapFPURegisterForWrite(&ptr, fp_dst);

        *ptr++ = fcpyd(fp_dst, fp_src);
#else
        if ((opcode2 & 0x4000) == 0)
        {
            fp_src = RA_MapFPURegister(&ptr, (opcode2 >> 10) & 7);
            fp_dst = RA_MapFPURegisterForWrite(&ptr, fp_dst);
            *ptr++ = fcpyd(fp_dst, fp_src);
        }
        else
        {
            fp_dst = RA_MapFPURegisterForWrite(&ptr, fp_dst);
            ptr = FPU_FetchData(ptr, m68k_ptr, &fp_dst, opcode, opcode2, &ext_count);
        }
#endif

        RA_FreeFPURegister(&ptr, fp_src);
        ptr = EMIT_AdvancePC(ptr, 2 * (ext_count + 1));
        (*m68k_ptr) += ext_count;

        if (FPSR_Update_Needed(m68k_ptr))
        {
            uint8_t fpsr = RA_ModifyFPSR(&ptr);

            *ptr++ = fcmpzd(fp_dst);
            ptr = EMIT_GetFPUFlags(ptr, fpsr);
        }
    }
    /* FMOVE to MEM */
    else if ((opcode & 0xffc0) == 0xf200 && ((opcode2 & 0xe07f) == 0x6000 || (opcode2 & 0xfc00) == 0x6c00 || (opcode2 & 0xfc0f) == 0x7c00))
    {
        uint8_t fp_src = (opcode2 >> 7) & 7;
        fp_src = RA_MapFPURegister(&ptr, fp_src);
        ptr = FPU_StoreData(ptr, m68k_ptr, fp_src, opcode, opcode2, &ext_count);

        ptr = EMIT_AdvancePC(ptr, 2 * (ext_count + 1));
        (*m68k_ptr) += ext_count;
    }
    /* FMOVE from special */
    else if ((opcode & 0xffc0) == 0xf200 && (opcode2 & 0xe3ff) == 0xa000)
    {
        uint8_t reg_CTX = RA_GetCTX(&ptr);
        uint8_t reg = 0xff;
        uint8_t dst = 0xff;

        // Handle move to Dn
        if ((opcode & 0x38) == 0)
        {
            uint8_t dst = RA_MapM68kRegisterForWrite(&ptr, opcode & 7);

            switch (opcode2 & 0x1c00)
            {
                case 0x1000:    /* FPCR */
                    reg = RA_GetFPCR(&ptr);
                    *ptr++ = mov_reg(dst, reg);
                    reg = 0xff;
                    break;
                case 0x0800:    /* FPSR */
                    reg = RA_GetFPSR(&ptr);
                    *ptr++ = mov_reg(dst, reg);
                    break;
            }
        }
        // Handle move from An
        else if ((opcode & 0x38) == 0x8)
        {
            uint8_t dst = RA_MapM68kRegisterForWrite(&ptr, 8 + (opcode & 7));

            switch (opcode2 & 0x1c00)
            {
                case 0x0400:    /* FPIAR */
                    *ptr++ = ldr_offset(reg_CTX, dst, __builtin_offsetof(struct M68KState, FPIAR));
                    break;
            }
        }
        // Handle all other cases
        else
        {
            int regnum = 0;
            int offset = 0;

            if ((opcode & 0x38) == 0x20 || (opcode & 0x38) == 0x18) {
                ptr = EMIT_LoadFromEffectiveAddress(ptr, 0, &dst, opcode & 0x3f, *m68k_ptr, &ext_count, 0, NULL);
                RA_SetDirtyM68kRegister(&ptr, 8 + (opcode & 7));
            }
            else
                ptr = EMIT_LoadFromEffectiveAddress(ptr, 0, &dst, opcode & 0x3f, *m68k_ptr, &ext_count, 1, NULL);

            if (opcode2 & 0x0400) regnum++;
            if (opcode2 & 0x0800) regnum++;
            if (opcode2 & 0x1000) regnum++;

            // In predecrement mode reserve whole space first
            if ((opcode & 0x38) == 0x20)
            {
                *ptr++ = sub_immed(dst, dst, 4 * regnum);
            }

            if (opcode2 & 0x1000)
            {
                reg = RA_GetFPCR(&ptr);
                
                *ptr++ = str_offset(dst, reg, offset);
                offset += 4;
            }

            if (opcode2 & 0x0800)
            {
                reg = RA_GetFPSR(&ptr);
                *ptr++ = str_offset(dst, reg, offset);
                offset += 4;
            }

            if (opcode2 & 0x0400)
            {
                reg = RA_AllocARMRegister(&ptr);
                *ptr++ = ldr_offset(reg_CTX, reg, __builtin_offsetof(struct M68KState, FPIAR));
                *ptr++ = str_offset(dst, reg, offset);
                RA_FreeARMRegister(&ptr, reg);
                reg = 0xff;
            }

            // In postincrement mode adjust the value now
            if ((opcode & 0x38) == 0x18)
            {
                *ptr++ = add_immed(dst, dst, 4 * regnum);
            }

            RA_FreeARMRegister(&ptr, dst);
        }

        ptr = EMIT_AdvancePC(ptr, 2 * (ext_count + 1));
        (*m68k_ptr) += ext_count;
    }
    /* FMOVE to special */
    else if ((opcode & 0xffc0) == 0xf200 && (opcode2 & 0xe3ff) == 0x8000)
    {
        uint8_t src = 0xff;
        uint8_t tmp = 0xff;
        uint8_t reg = 0xff;
        uint8_t reg_CTX = RA_GetCTX(&ptr);

        // Handle move from Dn
        if ((opcode & 0x38) == 0)
        {
            ptr = EMIT_LoadFromEffectiveAddress(ptr, 4, &src, opcode & 0x3f, *m68k_ptr, &ext_count, 0, NULL);
            switch (opcode2 & 0x1c00)
            {
                case 0x1000:    /* FPCR */
                    tmp = RA_AllocARMRegister(&ptr);
                    reg = RA_ModifyFPCR(&ptr);
                    *ptr++ = mov_reg(reg, src);
                    {
                        uint8_t round = RA_AllocARMRegister(&ptr);

                        *ptr++ = get_fpcr(src);
                        *ptr++ = ubfx(round, reg, 4, 2);
                        *ptr++ = neg_reg(round, round, LSL, 0);
                        *ptr++ = add_immed(round, round, 4);
                        *ptr++ = bfi(src, round, 22, 2);
                        *ptr++ = set_fpcr(src);

                        RA_FreeARMRegister(&ptr, round);
                    }
                    break;
                case 0x0800:    /* FPSR */
                    reg = RA_ModifyFPSR(&ptr);
                    *ptr++ = mov_reg(reg, src);
                    break;
            }
        }
        // Handle move from An
        else if ((opcode & 0x38) == 0x8)
        {
            ptr = EMIT_LoadFromEffectiveAddress(ptr, 4, &src, opcode & 0x3f, *m68k_ptr, &ext_count, 0, NULL);
            switch (opcode2 & 0x1c00)
            {
                case 0x0400:    /* FPIAR */
                    *ptr++ = str_offset(reg_CTX, src, __builtin_offsetof(struct M68KState, FPIAR));
                    break;
            }
        }
        // Handle all other cases
        else
        {
            tmp = RA_AllocARMRegister(&ptr);
            int regnum = 0;
            int offset = 0;
            if ((opcode & 0x38) == 0x20 || (opcode & 0x38) == 0x18) {
                ptr = EMIT_LoadFromEffectiveAddress(ptr, 0, &src, opcode & 0x3f, *m68k_ptr, &ext_count, 0, NULL);
                RA_SetDirtyM68kRegister(&ptr, 8 + (opcode & 7));
            }
            else
                ptr = EMIT_LoadFromEffectiveAddress(ptr, 0, &src, opcode & 0x3f, *m68k_ptr, &ext_count, 1, NULL);

            if (opcode2 & 0x0400) regnum++;
            if (opcode2 & 0x0800) regnum++;
            if (opcode2 & 0x1000) regnum++;

            // In predecrement mode reserve whole space first
            if ((opcode & 0x38) == 0x20)
            {
                *ptr++ = sub_immed(src, src, 4 * regnum);
            }

            if (opcode2 & 0x1000)
            {
                uint8_t round = RA_AllocARMRegister(&ptr);
                reg = RA_ModifyFPCR(&ptr);
                
                *ptr++ = ldr_offset(src, tmp, offset);
                *ptr++ = mov_reg(reg, tmp);

                *ptr++ = get_fpcr(tmp);
                *ptr++ = ubfx(round, reg, 4, 2);
                *ptr++ = neg_reg(round, round, LSL, 0);
                *ptr++ = add_immed(round, round, 4);
                *ptr++ = bfi(tmp, round, 22, 2);
                *ptr++ = set_fpcr(tmp);

                RA_FreeARMRegister(&ptr, round);

                offset += 4;
            }

            if (opcode2 & 0x0800)
            {
                reg = RA_ModifyFPSR(&ptr);
                *ptr++ = ldr_offset(src, tmp, offset);
                *ptr++ = mov_reg(reg, tmp);
                offset += 4;
            }

            if (opcode2 & 0x0400)
            {
                *ptr++ = ldr_offset(src, tmp, offset);
                *ptr++ = str_offset(reg_CTX, tmp, __builtin_offsetof(struct M68KState, FPIAR));
            }

            // In postincrement mode adjust the value now
            if ((opcode & 0x38) == 0x18)
            {
                *ptr++ = add_immed(src, src, 4 * regnum);
            }
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
#ifdef __aarch64__
                        ptr = EMIT_Store96bitFP(ptr, fp_reg, base_reg, 12*cnt++);
                        //*ptr++ = fstd(fp_reg, base_reg, 12*cnt++);
#else
                        *ptr++ = fstd(fp_reg, base_reg, 3*cnt++);
#endif
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
#ifdef __aarch64__
                        ptr = EMIT_Store96bitFP(ptr, fp_reg, base_reg, 12*cnt);
                        //*ptr++ = fstd(fp_reg, base_reg, cnt*12);
#else
                        *ptr++ = fstd(fp_reg, base_reg, cnt*3);
#endif
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
#ifdef __aarch64__
                        ptr = EMIT_Load96bitFP(ptr, fp_reg, base_reg, 12*cnt++);
                        //*ptr++ = fldd(fp_reg, base_reg, 12*cnt++);
#else
                        *ptr++ = fldd(fp_reg, base_reg, 3*cnt++);
#endif
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
#ifdef __aarch64__
                        ptr = EMIT_Load96bitFP(ptr, fp_reg, base_reg, 12*cnt);
                        //*ptr++ = fldd(fp_reg, base_reg, cnt*12);
#else
                        *ptr++ = fldd(fp_reg, base_reg, cnt*3);
#endif
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
            uint8_t fpsr = RA_ModifyFPSR(&ptr);

            *ptr++ = fcmpzd(fp_dst);
            ptr = EMIT_GetFPUFlags(ptr, fpsr);
        }
    }
    /* FSGLMUL */
    else if ((opcode & 0xffc0) == 0xf200 && ((opcode2 & 0xa07f) == 0x0027))
    {
        uint8_t fp_src = 0xff;
        uint8_t fp_dst = (opcode2 >> 7) & 7;

        ptr = FPU_FetchData(ptr, m68k_ptr, &fp_src, opcode, opcode2, &ext_count);
        fp_dst = RA_MapFPURegister(&ptr, fp_dst);

        *ptr++ = fmuld(fp_dst, fp_dst, fp_src);
        *ptr++ = fcvtsd(fp_dst, fp_dst);
        *ptr++ = fcvtds(fp_dst, fp_dst);

        RA_SetDirtyFPURegister(&ptr, fp_dst);

        RA_FreeFPURegister(&ptr, fp_src);

        ptr = EMIT_AdvancePC(ptr, 2 * (ext_count + 1));
        (*m68k_ptr) += ext_count;

        if (FPSR_Update_Needed(m68k_ptr))
        {
            uint8_t fpsr = RA_ModifyFPSR(&ptr);

            *ptr++ = fcmpzd(fp_dst);
            ptr = EMIT_GetFPUFlags(ptr, fpsr);
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
            uint8_t fpsr = RA_ModifyFPSR(&ptr);

            *ptr++ = fcmpzd(fp_dst);
            ptr = EMIT_GetFPUFlags(ptr, fpsr);
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
            uint8_t fpsr = RA_ModifyFPSR(&ptr);
            ptr = EMIT_GetFPUFlags(ptr, fpsr);
        }
    }
    /* FScc */
    else if ((opcode & 0xffc0) == 0xf240 && (opcode2 & 0xffc0) == 0)
    {
        uint8_t fpsr = RA_GetFPSR(&ptr);
        uint8_t predicate = opcode2 & 0x3f;
        uint8_t success_condition = 0;
        uint8_t tmp_cc = 0xff;

        /* Test predicate with masked signalling bit, operations are the same */
        switch (predicate & 0x0f)
        {
            case F_CC_EQ:
#ifdef __aarch64__
                *ptr++ = tst_immed(fpsr, 1, 31 & (32 - FPSRB_Z));
                success_condition = A64_CC_NE;
#else
                *ptr++ = tst_immed(fpsr, 0x404);
                success_condition = ARM_CC_NE;
#endif
                break;
            case F_CC_NE:
#ifdef __aarch64__
                *ptr++ = tst_immed(fpsr, 1, 31 & (32 - FPSRB_Z));
                success_condition = A64_CC_EQ;
#else
                *ptr++ = tst_immed(fpsr, 0x404);
                success_condition = ARM_CC_EQ;
#endif
                break;
            case F_CC_OGT:
#ifdef __aarch64__
                tmp_cc = RA_AllocARMRegister(&ptr);
                *ptr++ = mov_immed_u16(tmp_cc, (FPSR_Z | FPSR_N | FPSR_NAN) >> 16, 1);
                *ptr++ = tst_reg(fpsr, tmp_cc, LSL, 0);
                success_condition = ARM_CC_EQ;
#else
                *ptr++ = tst_immed(fpsr, 0x40d);
                success_condition = ARM_CC_EQ;
#endif
                break;
            case F_CC_ULE:
#ifdef __aarch64__
                tmp_cc = RA_AllocARMRegister(&ptr);
                *ptr++ = mov_immed_u16(tmp_cc, (FPSR_Z | FPSR_N | FPSR_NAN) >> 16, 1);
                *ptr++ = tst_reg(fpsr, tmp_cc, LSL, 0);
                success_condition = ARM_CC_NE;
#else
                *ptr++ = tst_immed(fpsr, 0x40d);
                success_condition = ARM_CC_NE;
#endif
                break;
            case F_CC_OGE: // Z == 1 || (N == 0 && NAN == 0)
#ifdef __aarch64__
                tmp_cc = RA_AllocARMRegister(&ptr);
                *ptr++ = tst_immed(fpsr, 1, 31 & (32 - FPSRB_Z));
                *ptr++ = b_cc(A64_CC_NE, 4);
                *ptr++ = orr_reg(tmp_cc, fpsr, fpsr, LSL, 3); // N | NAN -> N (== 0 only if N=0 && NAN=0)
                *ptr++ = eor_immed(tmp_cc, tmp_cc, 1, 31 & (32 - FPSRB_N)); // !N -> N
                *ptr++ = tst_immed(tmp_cc, 1, 31 & (32 - FPSRB_N));
                success_condition = A64_CC_NE;
#else
                tmp_cc = RA_AllocARMRegister(&ptr);
                *ptr++ = eor_immed(tmp_cc, fpsr, 0x404);    /* Copy fpsr to temporary reg, invert Z */
                *ptr++ = tst_immed(tmp_cc, 0x404); // Test Z
                *ptr++ = tst_cc_immed(ARM_CC_NE, fpsr, 0x409); // Z == 1? Test N == 0 && NAN == 0
                success_condition = ARM_CC_EQ;
#endif
                break;
            case F_CC_ULT: // NAN == 1 || (N == 1 && Z == 0)
#ifdef __aarch64__
                tmp_cc = RA_AllocARMRegister(&ptr);
                *ptr++ = tst_immed(fpsr, 1, 31 & (32 - FPSRB_NAN));
                *ptr++ = b_cc(A64_CC_NE, 4);
                *ptr++ = eor_immed(tmp_cc, fpsr, 1, 31 & (32 - FPSRB_Z)); // Invert Z
                *ptr++ = and_reg(tmp_cc, fpsr, tmp_cc, LSL, 1); // !Z & N -> N
                *ptr++ = tst_immed(tmp_cc, 1, 31 & (32 - FPSRB_N));
                success_condition = A64_CC_NE;
#else
                tmp_cc = RA_AllocARMRegister(&ptr);
                *ptr++ = eor_immed(tmp_cc, fpsr, 0x409);    /* Copy fpsr to temporary reg, invert N and NAN */
                *ptr++ = tst_immed(tmp_cc, 0x401);  // Test NAN
                *ptr++ = tst_cc_immed(ARM_CC_NE, fpsr, 0x40c); // !NAN == 1, test !N == 0 && Z == 0
                success_condition = ARM_CC_EQ;
#endif
                break;
            case F_CC_OLT: // N == 1 && (NAN == 0 && Z == 0)
#ifdef __aarch64__
                tmp_cc = RA_AllocARMRegister(&ptr);
                *ptr++ = orr_reg(tmp_cc, fpsr, fpsr, LSL, 2); // NAN | Z -> Z
                *ptr++ = eor_immed(tmp_cc, tmp_cc, 1, 31 & (32 - FPSRB_N)); // Invert N
                *ptr++ = tst_immed(tmp_cc, 2, 31 & (32 - FPSRB_Z)); // Test N==0 && Z == 0
                success_condition = A64_CC_EQ;
#else
                tmp_cc = RA_AllocARMRegister(&ptr);
                *ptr++ = eor_immed(tmp_cc, fpsr, 0x408);    /* Copy fpsr to temporary reg, invert N */
                *ptr++ = tst_immed(tmp_cc, 0x40d);
                success_condition = ARM_CC_EQ;
#endif
                break;
            case F_CC_UGE: // NAN == 1 || (Z == 1 || N == 0)
#ifdef __aarch64__
                tmp_cc = RA_AllocARMRegister(&ptr);
                *ptr++ = eor_immed(tmp_cc, fpsr, 1, 31 & (32 - FPSRB_N));
                *ptr++ = bic_immed(tmp_cc, tmp_cc, 1, 31 & (32 - FPSRB_I));
                *ptr++ = tst_immed(tmp_cc, 4, 31 & (32 - FPSRB_NAN));
                success_condition = A64_CC_NE;
#else
                tmp_cc = RA_AllocARMRegister(&ptr);
                *ptr++ = eor_immed(tmp_cc, fpsr, 0x408);    /* Copy fpsr to temporary reg, invert N */
                *ptr++ = tst_immed(tmp_cc, 0x40d);
                success_condition = ARM_CC_NE;
#endif
                break;
            case F_CC_OLE: // Z == 1 || (N == 1 && NAN == 0)
#ifdef __aarch64__
                tmp_cc = RA_AllocARMRegister(&ptr);
                *ptr++ = tst_immed(fpsr, 1, 31 & (32 - FPSRB_Z));
                *ptr++ = b_cc(A64_CC_NE, 4);
                *ptr++ = eor_immed(tmp_cc, fpsr, 1, 31 & (32 - FPSRB_NAN)); // Invert NAN
                *ptr++ = and_reg(tmp_cc, tmp_cc, tmp_cc, LSL, 3);   // !NAN & N -> N
                *ptr++ = tst_immed(tmp_cc, 1, 31 & (32 - FPSRB_N));
                success_condition = A64_CC_NE;
#else
                tmp_cc = RA_AllocARMRegister(&ptr);
                *ptr++ = eor_immed(tmp_cc, fpsr, 0x40c);    /* Copy fpsr to temporary reg, invert Z and N */
                *ptr++ = tst_immed(tmp_cc, 0x404);          /* Test if inverted Z == 0 */
                *ptr++ = tst_cc_immed(ARM_CC_NE, tmp_cc, 0x409);    /* Test if !N == 0 && NAN == 0 */
                success_condition = ARM_CC_EQ;
#endif
                break;
            case F_CC_UGT: // NAN == 1 || (N == 0 && Z == 0)
#ifdef __aarch64__
                tmp_cc = RA_AllocARMRegister(&ptr);
                *ptr++ = tst_immed(fpsr, 1, 31 & (32 - FPSRB_NAN));
                *ptr++ = b_cc(A64_CC_NE, 4);
                *ptr++ = orr_reg(tmp_cc, fpsr, fpsr, LSR, 1);
                *ptr++ = eor_immed(tmp_cc, tmp_cc, 1, 31 & (32 - FPSRB_Z));
                *ptr++ = tst_immed(tmp_cc, 1, 31 & (32 - FPSRB_Z));
                success_condition = A64_CC_NE;
#else
                tmp_cc = RA_AllocARMRegister(&ptr);
                *ptr++ = eor_immed(tmp_cc, fpsr, 0x401);    /* Copy fpsr to temporary reg, invert NAN */
                *ptr++ = tst_immed(tmp_cc, 0x401);          /* Test if inverted NAN == 0 */
                *ptr++ = tst_cc_immed(ARM_CC_NE, tmp_cc, 0x40c);    /* Test if !N == 0 && Z == 0 */
                success_condition = ARM_CC_EQ;
#endif
                break;
            case F_CC_OGL:
#ifdef __aarch64__
                tmp_cc = RA_AllocARMRegister(&ptr);
                *ptr++ = mov_immed_u16(tmp_cc, (FPSR_Z | FPSR_NAN) >> 16, 1);
                *ptr++ = tst_reg(fpsr, tmp_cc, LSL, 0);
                success_condition = A64_CC_EQ;
#else
                *ptr++ = tst_immed(fpsr, 0x405);
                success_condition = ARM_CC_EQ;
#endif
                break;
            case F_CC_UEQ:
#ifdef __aarch64__
                tmp_cc = RA_AllocARMRegister(&ptr);
                *ptr++ = mov_immed_u16(tmp_cc, (FPSR_Z | FPSR_NAN) >> 16, 1);
                *ptr++ = tst_reg(fpsr, tmp_cc, LSL, 0);
                success_condition = A64_CC_NE;
#else
                *ptr++ = tst_immed(fpsr, 0x405);
                success_condition = ARM_CC_NE;
#endif
                break;
            case F_CC_OR:
#ifdef __aarch64__
                *ptr++ = tst_immed(fpsr, 1, 31 & (32 - FPSRB_NAN));
                success_condition = A64_CC_EQ;
#else
                *ptr++ = tst_immed(fpsr, 0x401);
                success_condition = ARM_CC_EQ;
#endif
                break;
            case F_CC_UN:
#ifdef __aarch64__
                *ptr++ = tst_immed(fpsr, 1, 31 & (32 - FPSRB_NAN));
                success_condition = A64_CC_NE;
#else
                *ptr++ = tst_immed(fpsr, 0x401);
                success_condition = ARM_CC_NE;
#endif
                break;
        }
        RA_FreeARMRegister(&ptr, tmp_cc);

        if ((opcode & 0x38) == 0)
        {
            /* FScc Dx case */
            uint8_t dest = RA_MapM68kRegister(&ptr, opcode & 7);
            RA_SetDirtyM68kRegister(&ptr, opcode & 7);

#ifdef __aarch64__
            uint8_t tmp = RA_AllocARMRegister(&ptr);
            *ptr++ = csetm(tmp, success_condition);
            *ptr++ = bfi(dest, tmp, 0, 8);
            RA_FreeARMRegister(&ptr, tmp);
#else
            *ptr++ = orr_cc_immed(success_condition, dest, dest, 0xff);
            *ptr++ = bfc_cc(success_condition ^ 1, dest, 0, 8);
#endif
        }
        else
        {
            /* Load effective address */
            uint8_t tmp = RA_AllocARMRegister(&ptr);

            *ptr++ = csetm(tmp, success_condition);

            ptr = EMIT_StoreToEffectiveAddress(ptr, 1, &tmp, opcode & 0x3f, *m68k_ptr, &ext_count);

            RA_FreeARMRegister(&ptr, tmp);
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

        *ptr++ = fsqrtd(fp_dst, fp_src);

        RA_FreeFPURegister(&ptr, fp_src);

        ptr = EMIT_AdvancePC(ptr, 2 * (ext_count + 1));
        (*m68k_ptr) += ext_count;

        if (FPSR_Update_Needed(m68k_ptr))
        {
            uint8_t fpsr = RA_ModifyFPSR(&ptr);

            *ptr++ = fcmpzd(fp_dst);
            ptr = EMIT_GetFPUFlags(ptr, fpsr);
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
            uint8_t fpsr = RA_ModifyFPSR(&ptr);

            *ptr++ = fcmpzd(fp_dst);
            ptr = EMIT_GetFPUFlags(ptr, fpsr);
        }
    }
    /* FSIN */
    else if ((opcode & 0xffc0) == 0xf200 && (opcode2 & 0xa07f) == 0x000e)
    {
        uint8_t fp_src = 0;
        uint8_t fp_dst = (opcode2 >> 7) & 7;

        ptr = FPU_FetchData(ptr, m68k_ptr, &fp_src, opcode, opcode2, &ext_count);
        fp_dst = RA_MapFPURegisterForWrite(&ptr, fp_dst);

        union {
            uint64_t u64;
            uint32_t u32[2];
        } u;

        u.u64 = (uintptr_t)sin;

        if (fp_src != 0) {
            *ptr++ = fcpyd(0, fp_src);
        }

        ptr = EMIT_SaveRegFrame(ptr, RA_GetTempAllocMask() | REG_PROTECT);

        *ptr++ = adr(30, 20);
        *ptr++ = ldr64_pcrel(0, 2);
        *ptr++ = br(0);

        *ptr++ = u.u32[0];
        *ptr++ = u.u32[1];

        *ptr++ = fcpyd(fp_dst, 0);

        ptr = EMIT_RestoreRegFrame(ptr, RA_GetTempAllocMask() | REG_PROTECT);
        
        RA_FreeFPURegister(&ptr, fp_src);

        ptr = EMIT_AdvancePC(ptr, 2 * (ext_count + 1));
        (*m68k_ptr) += ext_count;

        if (FPSR_Update_Needed(m68k_ptr))
        {
            uint8_t fpsr = RA_ModifyFPSR(&ptr);

            *ptr++ = fcmpzd(fp_dst);
            ptr = EMIT_GetFPUFlags(ptr, fpsr);
        }

        *ptr++ = INSN_TO_LE(0xfffffff0);
    }
    /* FCOS */
    else if ((opcode & 0xffc0) == 0xf200 && (opcode2 & 0xa07f) == 0x001d)
    {
        uint8_t fp_src = 0;
        uint8_t fp_dst = (opcode2 >> 7) & 7;

        ptr = FPU_FetchData(ptr, m68k_ptr, &fp_src, opcode, opcode2, &ext_count);
        fp_dst = RA_MapFPURegisterForWrite(&ptr, fp_dst);

        union {
            uint64_t u64;
            uint32_t u32[2];
        } u;

        u.u64 = (uintptr_t)cos;

        if (fp_src != 0) {
            *ptr++ = fcpyd(0, fp_src);
        }

        ptr = EMIT_SaveRegFrame(ptr, RA_GetTempAllocMask() | REG_PROTECT);

        *ptr++ = adr(30, 20);
        *ptr++ = ldr64_pcrel(0, 2);
        *ptr++ = br(0);

        *ptr++ = u.u32[0];
        *ptr++ = u.u32[1];

        *ptr++ = fcpyd(fp_dst, 0);

        ptr = EMIT_RestoreRegFrame(ptr, RA_GetTempAllocMask() | REG_PROTECT);

        RA_FreeFPURegister(&ptr, fp_src);

        ptr = EMIT_AdvancePC(ptr, 2 * (ext_count + 1));
        (*m68k_ptr) += ext_count;

        if (FPSR_Update_Needed(m68k_ptr))
        {
            uint8_t fpsr = RA_ModifyFPSR(&ptr);

            *ptr++ = fcmpzd(fp_dst);
            ptr = EMIT_GetFPUFlags(ptr, fpsr);
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
    /* FRESTORE */
    else if ((opcode & ~0x3f) == 0xf340 && 
             (opcode & 0x30) != 0x00 && 
             (opcode & 0x38) != 0x20 &&
             (opcode & 0x3f) <= 0x3b)
    {
        uint8_t tmp = -1;
        ext_count = 0;
        uint32_t *tmp_ptr;
        uint8_t fpcr = RA_ModifyFPCR(&ptr);
        uint8_t fpsr = RA_ModifyFPSR(&ptr);
        uint8_t reg_CTX= RA_GetCTX(&ptr);

        ptr = EMIT_LoadFromEffectiveAddress(ptr, 4, &tmp, opcode & 0x3f, *m68k_ptr, &ext_count, 0, NULL);

        // If Postincrement mode, eventually skip rest of the frame if IDLE was fetched
        if ((opcode & 0x38) == 0x18)
        {
            uint8_t An = RA_MapM68kRegister(&ptr, 8 + (opcode & 7));
            uint8_t tmp2 = RA_AllocARMRegister(&ptr);
            *ptr++ = tst_immed(tmp, 8, 8);
            *ptr++ = b_cc(A64_CC_EQ, 5);
            *ptr++ = ubfx(tmp2, tmp, 16, 8);
            *ptr++ = cmp_immed(tmp2, 0x18);
            *ptr++ = b_cc(A64_CC_NE, 2);
            *ptr++ = add_immed(An, An, 28 - 4);
            RA_FreeARMRegister(&ptr, tmp2);
        }

        // In case of NULL frame, reset FPU to vanilla state
        *ptr++ = tst_immed(tmp, 8, 8);
        tmp_ptr = ptr;
        *ptr++ = b_cc(A64_CC_NE, 0);

        *ptr++ = fmov_0(8);
        *ptr++ = fmov_0(9);
        *ptr++ = fmov_0(10);
        *ptr++ = fmov_0(11);
        *ptr++ = fmov_0(12);
        *ptr++ = fmov_0(13);
        *ptr++ = fmov_0(14);
        *ptr++ = fmov_0(15);
        *ptr++ = mov_immed_u16(fpcr, 0, 0);
        *ptr++ = mov_immed_u16(fpsr, 0, 0);

        *ptr++ = get_fpcr(tmp);
        *ptr++ = bic_immed(tmp, tmp, 2, 32 - 22);
        *ptr++ = set_fpcr(tmp);
        *ptr++ = str_offset(reg_CTX, 31, __builtin_offsetof(struct M68KState, FPIAR));

        *tmp_ptr = b_cc(A64_CC_NE, ptr - tmp_ptr);

        RA_FreeARMRegister(&ptr, tmp);

        ptr = EMIT_AdvancePC(ptr, 2 * (ext_count + 1));
        (*m68k_ptr) += ext_count;
        ptr = EMIT_FlushPC(ptr);
    }
    /* FSAVE */
    else if ((opcode & ~0x3f) == 0xf300 && 
             (opcode & 0x30) != 0x00 && 
             (opcode & 0x38) != 0x18 &&
             (opcode & 0x3f) <= 0x39)
    {
        uint8_t tmp = RA_AllocARMRegister(&ptr);
        
        ext_count = 0;

        *ptr++ = mov_immed_u16(tmp, 0x4100, 1);
        ptr = EMIT_StoreToEffectiveAddress(ptr, 4, &tmp, opcode & 0x3f, *m68k_ptr, &ext_count);

        RA_FreeARMRegister(&ptr, tmp);

        ptr = EMIT_AdvancePC(ptr, 2 * (ext_count + 1));
        (*m68k_ptr) += ext_count;
    }
    else
    {
        ptr = EMIT_FlushPC(ptr);
        ptr = EMIT_InjectDebugString(ptr, "[JIT] opcode %04x:%04x at %08x not implemented\n", opcode, opcode2, *m68k_ptr - 1);
        ptr = EMIT_Exception(ptr, VECTOR_LINE_F, 0);
        *ptr++ = INSN_TO_LE(0xffffffff);
    }

    return ptr;
}

int DisableFPU = 0;

uint32_t *EMIT_lineF(uint32_t *ptr, uint16_t **m68k_ptr, uint16_t *insn_consumed)
{
    uint16_t opcode = BE16((*m68k_ptr)[0]);
    uint16_t opcode2 = BE16((*m68k_ptr)[1]);

    /* Check destination coprocessor - if it is FPU go to separate function */
    if (DisableFPU == 0 && (opcode & 0x0e00) == 0x0200)
    {
        return EMIT_FPU(ptr, m68k_ptr, insn_consumed);
    }
    /* PFLUSHA - ignore */
    else if ((opcode & 0xffe0) == 0xf500)
    {
        *ptr++ = nop();
        (*m68k_ptr)+=1;
        *insn_consumed = 1;
        ptr = EMIT_AdvancePC(ptr, 2);
    }
    /* MOVE16 (Ax)+, (Ay)+ */
    else if ((opcode & 0xfff8) == 0xf620) // && (opcode2 & 0x8fff) == 0x8000) <- don't test! Real m68k ignores that bit!
    {
        uint8_t aligned_src = RA_AllocARMRegister(&ptr);
        uint8_t aligned_dst = RA_AllocARMRegister(&ptr);
        uint8_t buf1 = RA_AllocARMRegister(&ptr);
        uint8_t buf2 = RA_AllocARMRegister(&ptr);
#ifndef __aarch64__
        uint8_t buf3 = RA_AllocARMRegister(&ptr);
        uint8_t buf4 = RA_AllocARMRegister(&ptr);
#endif
        uint8_t src = RA_MapM68kRegister(&ptr, 8 + (opcode & 7));
        uint8_t dst = RA_MapM68kRegister(&ptr, 8 + ((opcode2 >> 12) & 7));

#ifdef __aarch64__
        *ptr++ = bic_immed(aligned_src, src, 4, 0);
        *ptr++ = bic_immed(aligned_dst, dst, 4, 0);
        *ptr++ = ldp64(aligned_src, buf1, buf2, 0);
        *ptr++ = stp64(aligned_dst, buf1, buf2, 0);
#else
        *ptr++ = bic_immed(aligned_src, src, 0x0f);
        *ptr++ = bic_immed(aligned_dst, dst, 0x0f);
        *ptr++ = ldm(aligned_src, (1 << buf1) | (1 << buf2) | (1 << buf3) | (1 << buf4));
        *ptr++ = stm(aligned_dst, (1 << buf1) | (1 << buf2) | (1 << buf3) | (1 << buf4));
#endif
        *ptr++ = add_immed(src, src, 16);
        // Update dst only if it is not the same as src!
        if (dst != src) {
            *ptr++ = add_immed(dst, dst, 16);
        }

        RA_SetDirtyM68kRegister(&ptr, 8 + (opcode & 7));
        RA_SetDirtyM68kRegister(&ptr, 8 + ((opcode2 >> 12) & 7));

        RA_FreeARMRegister(&ptr, aligned_src);
        RA_FreeARMRegister(&ptr, aligned_dst);
        RA_FreeARMRegister(&ptr, buf1);
        RA_FreeARMRegister(&ptr, buf2);
#ifndef __aarch64__
        RA_FreeARMRegister(&ptr, buf3);
        RA_FreeARMRegister(&ptr, buf4);
#endif
        (*m68k_ptr)+=2;
        *insn_consumed = 1;
        ptr = EMIT_AdvancePC(ptr, 4);
    }
    /* MOVE16 other variations */
    else if ((opcode & 0xffe0) == 0xf600)
    {
        uint8_t aligned_reg = RA_AllocARMRegister(&ptr);
        uint8_t aligned_mem = RA_AllocARMRegister(&ptr);
        uint8_t buf1 = RA_AllocARMRegister(&ptr);
        uint8_t buf2 = RA_AllocARMRegister(&ptr);
#ifndef __aarch64__
        uint8_t buf3 = RA_AllocARMRegister(&ptr);
        uint8_t buf4 = RA_AllocARMRegister(&ptr);
#endif
        uint8_t reg = RA_MapM68kRegister(&ptr, 8 + (opcode & 7));
        uint32_t mem = (BE16((*m68k_ptr)[1]) << 16) | BE16((*m68k_ptr)[2]);

        /* Align memory pointer */
        mem &= 0xfffffff0;
        *ptr++ = movw_immed_u16(aligned_mem, mem & 0xffff);
        if (mem & 0xffff0000)
            *ptr++ = movt_immed_u16(aligned_mem, mem >> 16);
#ifdef __aarch64__
        *ptr++ = bic_immed(aligned_reg, reg, 4, 0);
        if (opcode & 8) {
            *ptr++ = ldp64(aligned_mem, buf1, buf2, 0);
            *ptr++ = stp64(aligned_reg, buf1, buf2, 0);
        }
        else {
            *ptr++ = ldp64(aligned_reg, buf1, buf2, 0);
            *ptr++ = stp64(aligned_mem, buf1, buf2, 0);
        }
#else
        *ptr++ = bic_immed(aligned_reg, reg, 0x0f);
        if (opcode & 8) {
            *ptr++ = ldm(aligned_mem, (1 << buf1) | (1 << buf2) | (1 << buf3) | (1 << buf4));
            *ptr++ = stm(aligned_reg, (1 << buf1) | (1 << buf2) | (1 << buf3) | (1 << buf4));
        }
        else {
            *ptr++ = ldm(aligned_reg, (1 << buf1) | (1 << buf2) | (1 << buf3) | (1 << buf4));
            *ptr++ = stm(aligned_mem, (1 << buf1) | (1 << buf2) | (1 << buf3) | (1 << buf4));
        }
#endif
        if (!(opcode & 0x10))
        {
            *ptr++ = add_immed(reg, reg, 16);
            RA_SetDirtyM68kRegister(&ptr, 8 + (opcode & 7));
        }

        RA_FreeARMRegister(&ptr, aligned_reg);
        RA_FreeARMRegister(&ptr, aligned_mem);
        RA_FreeARMRegister(&ptr, buf1);
        RA_FreeARMRegister(&ptr, buf2);
#ifndef __aarch64__
        RA_FreeARMRegister(&ptr, buf3);
        RA_FreeARMRegister(&ptr, buf4);
#endif
        (*m68k_ptr)+=3;
        *insn_consumed = 1;
        ptr = EMIT_AdvancePC(ptr, 6);
    }
    /* CINV */
    else if ((opcode & 0xff20) == 0xf400 && (opcode & 0x0018) != 0)
    {
        uint8_t tmp = 0xff;
        uint8_t tmp2 = 0xff;
        uint8_t tmp3 = 0xff;
        uint8_t tmp4 = 0xff;

        ptr = EMIT_FlushPC(ptr);

        /* Invalidating data cache? */
        if (opcode & 0x40) {
            /* Get the scope */
            switch (opcode & 0x18) {
                case 0x08:  /* Line */
                    tmp = RA_CopyFromM68kRegister(&ptr, 8 + (opcode & 7));
#ifdef __aarch64__
                    tmp2 = RA_AllocARMRegister(&ptr);
                    tmp3 = RA_AllocARMRegister(&ptr);
                    *ptr++ = mov_immed_u8(tmp3, 4);
                    *ptr++ = mrs(tmp2, 3, 3, 0, 0, 1); // Get CTR_EL0
                    *ptr++ = ubfx(tmp2, tmp2, 16, 4);
                    *ptr++ = lslv(tmp2, tmp3, tmp2);
                    *ptr++ = sub_immed(tmp2, tmp2, 1);
                    *ptr++ = dsb_sy();
                    *ptr++ = and_reg(tmp, tmp, tmp2, LSL, 0);
                    *ptr++ = dc_ivac(tmp);
                    *ptr++ = dsb_sy();
                    RA_FreeARMRegister(&ptr, tmp2);
                    RA_FreeARMRegister(&ptr, tmp3);
#else
                    *ptr++ = bic_immed(tmp, tmp, 0x1f);
                    *ptr++ = mcr(15, 0, tmp, 7, 6, 1); /* clean and invalidate data cache line */
                    *ptr++ = mov_immed_u8(tmp, 0);
                    *ptr++ = mcr(15, 0, tmp, 7, 10, 4); /* dsb */
#endif
                    RA_FreeARMRegister(&ptr, tmp);
                    break;
                case 0x10:  /* Page */
                    tmp = RA_CopyFromM68kRegister(&ptr, 8 + (opcode & 7));
                    tmp2 = RA_AllocARMRegister(&ptr);
#ifdef __aarch64__
                    tmp3 = RA_AllocARMRegister(&ptr);
                    tmp4 = RA_AllocARMRegister(&ptr);
                    *ptr++ = mrs(tmp3, 3, 3, 0, 0, 1); // Get CTR_EL0
                    *ptr++ = ubfx(tmp3, tmp3, 16, 4);
                    *ptr++ = mov_immed_u16(tmp2, 1024, 0);
                    *ptr++ = lsrv(tmp2, tmp2, tmp3);
                    *ptr++ = bic_immed(tmp, tmp, 12, 0);
                    *ptr++ = mov_immed_u8(tmp4, 4);
                    *ptr++ = lslv(tmp4, tmp4, tmp3);
                    *ptr++ = dc_ivac(tmp);
                    *ptr++ = add_reg(tmp, tmp, tmp4, LSL, 0);
                    *ptr++ = subs_immed(tmp2, tmp2, 1);
                    *ptr++ = b_cc(A64_CC_NE, -3);
                    *ptr++ = dsb_sy();
                    RA_FreeARMRegister(&ptr, tmp3);
                    RA_FreeARMRegister(&ptr, tmp4);
#else
                    *ptr++ = bic_immed(tmp, tmp, 0x0ff);
                    *ptr++ = bic_immed(tmp, tmp, 0xc0f);
                    *ptr++ = mov_immed_u8(tmp2, 128);
                    *ptr++ = mcr(15, 0, tmp, 7, 6, 1); /* clean and invalidate data cache line */
                    *ptr++ = add_immed(tmp, tmp, 32);
                    *ptr++ = subs_immed(tmp2, tmp2, 1);
                    *ptr++ = b_cc(ARM_CC_NE, -5);
                    *ptr++ = mcr(15, 0, tmp2, 7, 10, 4); /* dsb */
#endif
                    RA_FreeARMRegister(&ptr, tmp);
                    RA_FreeARMRegister(&ptr, tmp2);
                    break;
                case 0x18:  /* All */
#ifdef __aarch64__
                    {
                        union {
                            uint64_t u64;
                            uint32_t u32[2];
                        } u;

                        u.u64 = (uintptr_t)invalidate_entire_dcache;

                        *ptr++ = stp64_preindex(31, 0, 30, -16);
                        *ptr++ = ldr64_pcrel(0, 4);
                        *ptr++ = blr(0);
                        *ptr++ = ldp64_postindex(31, 0, 30, 16);
                        *ptr++ = b(3);
                        *ptr++ = u.u32[0];
                        *ptr++ = u.u32[1];
                    }
#else
                    *ptr++ = push(0x0f | (1 << 12));
                    *ptr++ = ldr_offset(15, 12, 8);
                    *ptr++ = blx_cc_reg(ARM_CC_AL, 12);
                    *ptr++ = pop(0x0f | (1 << 12));
                    *ptr++ = b_cc(ARM_CC_AL, 0);
                    *ptr++ = BE32((uint32_t)invalidate_entire_dcache);
#endif
                    break;
            }
        }
        /* Invalidating instruction cache? */
        if (opcode & 0x80) {
            int8_t off = 0;
            ptr = EMIT_GetOffsetPC(ptr, &off);
#ifdef __aarch64__
            union {
                uint64_t u64;
                uint32_t u32[2];
            } u;
            u.u64 = (uintptr_t)trampoline_icache_invalidate;

            *ptr++ = stp64_preindex(31, 0, 1, -176);
            for (int i=2; i < 20; i+=2)
                *ptr++ = stp64(31, i, i + 1, i * 8);
            *ptr++ = stp64(31, 29, 30, 160);
            if ((opcode & 0x18) == 0x08 || (opcode & 0x18) == 0x10)
            {
                uint8_t tmp = RA_MapM68kRegister(&ptr, 8 + (opcode & 7));
                *ptr++ = mov_reg(0, tmp);
            }
            if (off >= 0)
                *ptr++ = add_immed(1, REG_PC, off);
            else
                *ptr++ = sub_immed(1, REG_PC, -off);

            *ptr++ = adr(2, 4*6);
            *ptr++ = ldr64_pcrel(3, 3);
            *ptr++ = br(3);
            *ptr++ = b(3);
            *ptr++ = u.u32[0];
            *ptr++ = u.u32[1];

            for (int i=2; i < 20; i+=2)
                *ptr++ = ldp64(31, i, i + 1, i * 8);
            *ptr++ = ldp64(31, 29, 30, 160);
            *ptr++ = ldp64_postindex(31, 0, 1, 176);
#else
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
#endif
        }

        (*m68k_ptr)++;
        *insn_consumed = 1;

        *ptr++ = add_immed(REG_PC, REG_PC, 2);

        /* Cache flushing is context synchronizing. Stop translating code here */
        *ptr++ = INSN_TO_LE(0xffffffff);
        *ptr++ = INSN_TO_LE(0xfffffff0);
    }
    /* CPUSH */
    else if ((opcode & 0xff20) == 0xf420 && (opcode & 0x0018) != 0)
    {
        uint8_t tmp = 0xff;
        uint8_t tmp2 = 0xff;
        uint8_t tmp3 = 0xff;
        uint8_t tmp4 = 0xff;

        ptr = EMIT_FlushPC(ptr);

        /* Flush data cache? */
        if (opcode & 0x40) {
            /* Get the scope */
            switch (opcode & 0x18) {
                case 0x08:  /* Line */
                    tmp = RA_CopyFromM68kRegister(&ptr, 8 + (opcode & 7));
#ifdef __aarch64__
                    tmp2 = RA_AllocARMRegister(&ptr);
                    tmp3 = RA_AllocARMRegister(&ptr);
                    *ptr++ = mov_immed_u8(tmp3, 4);
                    *ptr++ = mrs(tmp2, 3, 3, 0, 0, 1); // Get CTR_EL0
                    *ptr++ = ubfx(tmp2, tmp2, 16, 4);
                    *ptr++ = lslv(tmp2, tmp3, tmp2);
                    *ptr++ = sub_immed(tmp2, tmp2, 1);
                    *ptr++ = dsb_sy();
                    *ptr++ = and_reg(tmp, tmp, tmp2, LSL, 0);
                    *ptr++ = dc_civac(tmp);
                    *ptr++ = dsb_sy();
                    RA_FreeARMRegister(&ptr, tmp2);
                    RA_FreeARMRegister(&ptr, tmp3);
#else
                    *ptr++ = bic_immed(tmp, tmp, 0x1f);
                    *ptr++ = mcr(15, 0, tmp, 7, 14, 1); /* clean and invalidate data cache line */
                    *ptr++ = mov_immed_u8(tmp, 0);
                    *ptr++ = mcr(15, 0, tmp, 7, 10, 4); /* dsb */
#endif
                    RA_FreeARMRegister(&ptr, tmp);
                    break;
                case 0x10:  /* Page */
                    tmp = RA_CopyFromM68kRegister(&ptr, 8 + (opcode & 7));
                    tmp2 = RA_AllocARMRegister(&ptr);
#ifdef __aarch64__
                    tmp3 = RA_AllocARMRegister(&ptr);
                    tmp4 = RA_AllocARMRegister(&ptr);
                    *ptr++ = mrs(tmp3, 3, 3, 0, 0, 1); // Get CTR_EL0
                    *ptr++ = ubfx(tmp3, tmp3, 16, 4);
                    *ptr++ = mov_immed_u16(tmp2, 1024, 0);
                    *ptr++ = lsrv(tmp2, tmp2, tmp3);
                    *ptr++ = bic_immed(tmp, tmp, 12, 0);
                    *ptr++ = mov_immed_u8(tmp4, 4);
                    *ptr++ = lslv(tmp4, tmp4, tmp3);
                    *ptr++ = dc_civac(tmp);
                    *ptr++ = add_reg(tmp, tmp, tmp4, LSL, 0);
                    *ptr++ = subs_immed(tmp2, tmp2, 1);
                    *ptr++ = b_cc(A64_CC_NE, -3);
                    *ptr++ = dsb_sy();
                    RA_FreeARMRegister(&ptr, tmp3);
                    RA_FreeARMRegister(&ptr, tmp4);
#else
                    *ptr++ = bic_immed(tmp, tmp, 0x0ff);
                    *ptr++ = bic_immed(tmp, tmp, 0xc0f);
                    *ptr++ = mov_immed_u8(tmp2, 128);
                    *ptr++ = mcr(15, 0, tmp, 7, 14, 1); /* clean and invalidate data cache line */
                    *ptr++ = add_immed(tmp, tmp, 32);
                    *ptr++ = subs_immed(tmp2, tmp2, 1);
                    *ptr++ = b_cc(ARM_CC_NE, -5);
                    *ptr++ = mcr(15, 0, tmp2, 7, 10, 4); /* dsb */
#endif
                    RA_FreeARMRegister(&ptr, tmp);
                    RA_FreeARMRegister(&ptr, tmp2);
                    break;
                case 0x18:  /* All */
#ifdef __aarch64__
                    {
                        union {
                            uint64_t u64;
                            uint32_t u32[2];
                        } u;

                        u.u64 = (uintptr_t)clear_entire_dcache;

                        *ptr++ = stp64_preindex(31, 0, 30, -16);
                        *ptr++ = ldr64_pcrel(0, 4);
                        *ptr++ = blr(0);
                        *ptr++ = ldp64_postindex(31, 0, 30, 16);
                        *ptr++ = b(3);
                        *ptr++ = u.u32[0];
                        *ptr++ = u.u32[1];
                    }
#else
                    *ptr++ = push(0x0f | (1 << 12));
                    *ptr++ = ldr_offset(15, 12, 8);
                    *ptr++ = blx_cc_reg(ARM_CC_AL, 12);
                    *ptr++ = pop(0x0f | (1 << 12));
                    *ptr++ = b_cc(ARM_CC_AL, 0);
                    *ptr++ = BE32((uint32_t)clear_entire_dcache);
#endif
                    break;
            }
        }
        /* Invalidating instruction cache? */
        if (opcode & 0x80) {
            int8_t off = 0;
            ptr = EMIT_GetOffsetPC(ptr, &off);
#ifdef __aarch64__
            union {
                uint64_t u64;
                uint32_t u32[2];
            } u;
            u.u64 = (uintptr_t)trampoline_icache_invalidate;

            *ptr++ = stp64_preindex(31, 0, 1, -176);
            for (int i=2; i < 20; i+=2)
                *ptr++ = stp64(31, i, i + 1, i * 8);
            *ptr++ = stp64(31, 29, 30, 160);
            if ((opcode & 0x18) == 0x08 || (opcode & 0x18) == 0x10)
            {
                uint8_t tmp = RA_MapM68kRegister(&ptr, 8 + (opcode & 7));
                *ptr++ = mov_reg(0, tmp);
            }
            if (off >= 0)
                *ptr++ = add_immed(1, REG_PC, off);
            else
                *ptr++ = sub_immed(1, REG_PC, -off);

            *ptr++ = adr(2, 4*6);
            *ptr++ = ldr64_pcrel(3, 3);
            *ptr++ = br(3);
            *ptr++ = b(3);
            *ptr++ = u.u32[0];
            *ptr++ = u.u32[1];

            for (int i=2; i < 20; i+=2)
                *ptr++ = ldp64(31, i, i + 1, i * 8);
            *ptr++ = ldp64(31, 29, 30, 160);
            *ptr++ = ldp64_postindex(31, 0, 1, 176);
#else
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
#endif
        }

        (*m68k_ptr)++;
        *insn_consumed = 1;

        *ptr++ = add_immed(REG_PC, REG_PC, 2);

        /* Cache is context synchronizing. Break up here! */
        *ptr++ = INSN_TO_LE(0xffffffff);
        *ptr++ = INSN_TO_LE(0xfffffff0);
    }
    else
    {
        ptr = EMIT_FlushPC(ptr);
        ptr = EMIT_InjectDebugString(ptr, "[JIT] opcode %04x at %08x not implemented\n", opcode, *m68k_ptr - 1);
        *ptr++ = svc(0x103);
        *ptr++ = (uint32_t)(uintptr_t)(*m68k_ptr - 8);
        *ptr++ = 48;
        ptr = EMIT_Exception(ptr, VECTOR_LINE_F, 0);
        *ptr++ = INSN_TO_LE(0xffffffff);
    }

    return ptr;
}
