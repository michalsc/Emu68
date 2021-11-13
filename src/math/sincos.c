/* origin: FreeBSD /usr/src/lib/msun/src/s_sin.c */
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

#define _GNU_SOURCE
#include "libm.h"

struct double2 sincos(double x)
{
    struct double2 res;
	double y[2], s, c;
	uint32_t ix;
	unsigned n;

	GET_HIGH_WORD(ix, x);
	ix &= 0x7fffffff;

	/* |x| ~< pi/4 */
	if (ix <= 0x3fe921fb) {
		/* if |x| < 2**-27 * sqrt(2) */
		if (ix < 0x3e46a09e) {
			/* raise inexact if x!=0 and underflow if subnormal */
			FORCE_EVAL(ix < 0x00100000 ? x/0x1p120f : x+0x1p120f);
			res.d[0] = x;
			res.d[1] = 1.0;
			return res;
		}
		res.d[0] = __sin(x, 0.0, 0);
		res.d[1] = __cos(x, 0.0);
		return res;
	}

	/* sincos(Inf or NaN) is NaN */
	if (ix >= 0x7ff00000) {
		res.d[0] = res.d[1] = x - x;
		return res;
	}

	/* argument reduction needed */
	n = __rem_pio2(x, y);
	s = __sin(y[0], y[1], 1);
	c = __cos(y[0], y[1]);
	switch (n&3) {
	case 0:
		res.d[0] = s;
		res.d[1] = c;
		break;
	case 1:
		res.d[0] = c;
		res.d[1] = -s;
		break;
	case 2:
		res.d[0] = -s;
		res.d[1] = -c;
		break;
	case 3:
	default:
		res.d[0] = -c;
		res.d[1] = s;
		break;
	}

    return res;
}
