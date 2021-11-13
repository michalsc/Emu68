#ifndef __LIBM_H
#define __LIBM_H

#include <stdint.h>

/* Get the more significant 32 bit int from a double.  */

typedef union
{ 
  double value;
  struct
  { 
    uint32_t msw;
    uint32_t lsw;
  } parts;
  struct
  { 
    uint64_t w;
  } xparts;
} ieee_double_shape_type;

#define GET_HIGH_WORD(i,d)                                      \
do {                                                            \
  ieee_double_shape_type gh_u;                                  \
  gh_u.value = (d);                                             \
  (i) = gh_u.parts.msw;                                         \
} while (0)

/* Get the less significant 32 bit int from a double.  */

#define GET_LOW_WORD(i,d)                                       \
do {                                                            \
  ieee_double_shape_type gl_u;                                  \
  gl_u.value = (d);                                             \
  (i) = gl_u.parts.lsw;                                         \
} while (0)

/* Get two 32 bit ints from a double.  */
#define EXTRACT_WORDS(hi,lo,d)                    \
do {                                              \
  union {double f; uint64_t i;} __u;              \
  __u.f = (d);                                    \
  (hi) = __u.i >> 32;                             \
  (lo) = (uint32_t)__u.i;                         \
} while (0)

/* Set a double from two 32 bit ints.  */

#define INSERT_WORDS(d,ix0,ix1)                                 \
do {                                                            \
  ieee_double_shape_type iw_u;                                  \
  iw_u.parts.msw = (ix0);                                       \
  iw_u.parts.lsw = (ix1);                                       \
  (d) = iw_u.value;                                             \
} while (0)

#define STRICT_ASSIGN(type, lval, rval) ((lval) = (rval))

/* Set the more significant 32 bits of a double from an int.  */

#define SET_HIGH_WORD(d,v)                                      \
do {                                                            \
  ieee_double_shape_type sh_u;                                  \
  sh_u.value = (d);                                             \
  sh_u.parts.msw = (v);                                         \
  (d) = sh_u.value;                                             \
} while (0)

/* Set the less significant 32 bits of a double from an int.  */

#define SET_LOW_WORD(d,v)                                       \
do {                                                            \
  ieee_double_shape_type sl_u;                                  \
  sl_u.value = (d);                                             \
  sl_u.parts.lsw = (v);                                         \
  (d) = sl_u.value;                                             \
} while (0)

#define FORCE_EVAL(x) do {                      \
	if (sizeof(x) == sizeof(float)) {           \
		volatile float __x;                     \
		__x = (x);                              \
        (void)__x;                              \
	} else if (sizeof(x) == sizeof(double)) {   \
		volatile double __x;                    \
		__x = (x);                              \
        (void)__x;                              \
	} else {                                    \
		volatile long double __x;               \
		__x = (x);                              \
        (void)__x;                              \
	}                                           \
} while(0)


static __inline unsigned __FLOAT_BITS(float __f)
{
        union {float __f; unsigned __i;} __u;
        __u.__f = __f;
        return __u.__i;
}
static __inline unsigned long long __DOUBLE_BITS(double __f)
{
        union {double __f; unsigned long long __i;} __u;
        __u.__f = __f;
        return __u.__i;
}

static inline double sqrt(double x)
{
    asm volatile("fsqrt %d0, %d0":"+w"(x):"0"(x));
    return x;
}

static inline double fabs(double x)
{
    asm volatile("fabs %d0, %d0":"+w"(x):"0"(x));
    return x;
}

#define isnan(x) ( \
        sizeof(x) == sizeof(float) ? (__FLOAT_BITS(x) & 0x7fffffff) > 0x7f800000 : \
        sizeof(x) == sizeof(double) ? (__DOUBLE_BITS(x) & -1ULL>>1) > 0x7ffULL<<52 : 1)

struct double2 {
    double d[2];
};

double __sin(double x, double y, int iy);
double __cos(double x, double y);
double __expo2(double x);
double __tan(double x, double y, int odd);
int __rem_pio2(double x, double *y);
int __rem_pio2_large(double *x, double *y, int e0, int nx, int prec);
double scalbn(double x, int n);
double floor(double x);
double tan(double x);
double tanh(double x);
double exp(double x);
double exp10(double x);
double exp2(double x);
double expm1(double x);
double cosh(double x);
double log(double x);
double atan(double x);
double sinh(double x);
double asin(double x);
double acos(double x);
double atanh(double x);
double log1p(double x);
double log10(double x);
double log2(double x);
double modf(double x, double *iptr);
struct double2 sincos(double x);

#endif /* __LIBM_H */
