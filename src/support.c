/*
    Copyright © 2019 Michal Schulz <michal.schulz@gmx.de>
    https://github.com/michalsc

    This Source Code Form is subject to the terms of the
    Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed
    with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <stdarg.h>
#include "support.h"

#define LOG2_10 3.321928094887362

static double pow10_tab[] = {
    1e00, 1e01, 1e02, 1e03, 1e04, 1e05, 1e06, 1e07, 1e08,
    1e09, 1e10, 1e11, 1e12, 1e13, 1e14, 1e15, 1e16, 1e17,
    1e18, 1e19, 1e20, 1e21, 1e22, 1e23, 1e24, 1e25, 1e26, 
    1e27, 1e28, 1e29, 1e30, 1e31
};

static double pow10_32tab[] = {
    1e00, 1e32, 1e64, 1e96, 1e128, 1e160, 1e192, 1e224, 1e256, 1e288
};

static double pow10_neg32tab[] = {
    1e-00, 1e-32, 1e-64, 1e-96, 1e-128, 1e-160, 1e-192, 1e-224, 1e-256, 1e-288, 1e-320,
};

double my_pow10(int exp)
{
    if (exp >= 0 && exp < 308) {
        return (pow10_tab[exp % 32] * pow10_32tab[exp / 32]);
    } else if (exp >= -323 && exp < 0) {
        return (pow10_neg32tab[-exp/32] / pow10_tab[(-exp) % 32]);
    }
    else return 0;
}

int my_log10(double v)
{
    const int maxp = 308;
    const int minp = -323;
    int min = minp;
    int max = maxp; 
    int mid = (max + min) / 2;
    
    do {
        double p = my_pow10(mid);
        
        /* If 10^mid == v then return mid */
        if (v == p) return mid;
        else
        {
            /* If p > v then select lower half */
            if (p > v)
            {
                max = mid;
            }
            /* Otherwise select upper half */
            else
            {
                min = mid;
            }
            mid = (max + min) / 2;
        } 
    } while ((max - min) > 1);

    return mid;
}

static int int_strlen(char *buf)
{
    int len = 0;

    if (buf)
        while(*buf++)
            len++;

    return len;
}

static void int_itoa(char *buf, char base, uintptr_t value, char zero_pad, int precision, int size_mod, char big, int alternate_form, int neg, char sign)
{
    int length = 0;

    do {
        char c = value % base;

        if (c >= 10) {
            if (big)
                c += 'A'-10;
            else
                c += 'a'-10;
        }
        else
            c += '0';

        value = value / base;
        buf[length++] = c;
    } while(value != 0);

    if (precision != 0)
    {
        while (length < precision)
            buf[length++] = '0';
    }
    else if (size_mod != 0 && zero_pad)
    {
        int sz_mod = size_mod;
        if (alternate_form)
        {
            if (base == 16) sz_mod -= 2;
            else if (base == 8) sz_mod -= 1;
        }
        if (neg)
            sz_mod -= 1;

        while (length < sz_mod)
            buf[length++] = '0';
    }
    if (alternate_form)
    {
        if (base == 8)
            buf[length++] = '0';
        if (base == 16) {
            buf[length++] = big ? 'X' : 'x';
            buf[length++] = '0';
        }
    }

    if (neg)
        buf[length++] = '-';
    else {
        if (sign == '+')
            buf[length++] = '+';
        else if (sign == ' ')
            buf[length++] = ' ';
    }

    for (int i=0; i < length/2; i++)
    {
        char tmp = buf[i];
        buf[i] = buf[length - i - 1];
        buf[length - i - 1] = tmp;
    }

    buf[length] = 0;
}

static void int_ftoa(char *buf, double value)
{
    int exp = 0;
    char c;

    union {
        uint64_t u64;
        double d;
    } u;

    u.d = value;

    if ((u.u64 & 0x7ff0000000000000ULL) == 0x7ff0000000000000 && (u.u64 & 0x000fffffffffffff) != 0)
    {
        *buf++ = 'N';
        *buf++ = 'a';
        *buf++ = 'N';
        *buf++ = 0;
        return;
    }

    if ((u.u64 & 0x7fffffffffffffffULL) == 0x7ff0000000000000)
    {
        if (u.u64 & 0x8000000000000000)
            *buf++ = '-';
        else
            *buf++ = ' ';
        *buf++ = 'I';
        *buf++ = 'n';
        *buf++ = 'f';
        *buf++ = 0;
        return;
    }

    if (value < 0)
    {
        *buf++ = '-';
        value = -value;
    }

    if (value == 0.0) {
        exp = 0;
        value = 0;
    }
    else
    {
        exp = my_log10(value);
        value /= my_pow10(exp);
    }

    c = (int)value;
    *buf++ = '0' + c;
    *buf++ = '.';

    for(int i=0; i < 5; i++)
    {
        value = (value - c) * 10;
        c = (int)value;
        *buf++ = '0' + c;
    }
    
    *buf++ = 'E';
    if (exp < 0)
    {
        *buf++ = '-';
        exp = -exp;
    }
    int_itoa(buf, 10, exp, 0, 0, 0, 0, 0, 0, 0);
}

void vkprintf_pc(putc_func putc_f, void *putc_data, const char * restrict format, va_list args)
{
    char tmpbuf[32];

    while(*format)
    {
        char c;
        char alternate_form = 0;
        int size_mod = 0;
        int length_mod = 0;
        int precision = 0;
        char zero_pad = 0;
        char *str;
        char sign = 0;
        char leftalign = 0;
        uintptr_t value = 0;
        intptr_t ivalue = 0;

        char big = 0;

        c = *format++;

        if (c != '%')
        {
            putc_f(putc_data, c);
        }
        else
        {
            c = *format++;

            if (c == '#') {
                alternate_form = 1;
                c = *format++;
            }

            if (c == '-') {
                leftalign = 1;
                c = *format++;
            }

            if (c == ' ' || c == '+') {
                sign = c;
                c = *format++;
            }

            if (c == '0') {
                zero_pad = 1;
                c = *format++;
            }

            while(c >= '0' && c <= '9') {
                size_mod = size_mod * 10;
                size_mod = size_mod + c - '0';
                c = *format++;
            }

            if (c == '.') {
                c = *format++;
                while(c >= '0' && c <= '9') {
                    precision = precision * 10;
                    precision = precision + c - '0';
                    c = *format++;
                }
            }

            big = 0;

            if (c == 'h')
            {
                c = *format++;
                if (c == 'h')
                {
                    c = *format++;
                    length_mod = 1;
                }
                else length_mod = 2;
            }
            else if (c == 'l')
            {
                c = *format++;
                if (c == 'l')
                {
                    c = *format++;
                }
                length_mod = 8;
            }
            else if (c == 'j')
            {
                c = *format++;
                length_mod = 9;
            }
            else if (c == 't')
            {
                c = *format++;
                length_mod = 10;
            }
            else if (c == 'z')
            {
                c = *format++;
                length_mod = 11;
            }

            switch (c) {
                case 0:
                    return;

                case '%':
                    putc_f(putc_data, '%');
                    break;

                case 'f':
                    int_ftoa(tmpbuf, va_arg(args, double));
                    str = tmpbuf;
                    size_mod -= int_strlen(str);
                    while (*str) {
                        putc_f(putc_data, *str++);
                    }
                    break;

                case 'p':
                    value = va_arg(args, uintptr_t);
                    int_itoa(tmpbuf, 16, value, 1, 2*sizeof(uintptr_t), 2*sizeof(uintptr_t), big, 1, 0, sign);
                    str = tmpbuf;
                    size_mod -= int_strlen(str);
                    while (*str) {
                        putc_f(putc_data, *str++);
                    }
                    break;

                case 'X':
                    big = 1;
                    /* fallthrough */
                case 'x':
                    switch (length_mod) {
                        case 8:
                            value = va_arg(args, uint64_t);
                            break;
                        case 9:
                            value = va_arg(args, uintmax_t);
                            break;
                        case 10:
                            value = va_arg(args, uintptr_t);
                            break;
                        case 11:
                            value = va_arg(args, size_t);
                            break;
                        default:
                            value = va_arg(args, unsigned int);
                            break;
                    }
                    int_itoa(tmpbuf, 16, value, zero_pad, precision, size_mod, big, alternate_form, 0, sign);
                    str = tmpbuf;
                    size_mod -= int_strlen(str);
                    if (!leftalign)
                        while(size_mod-- > 0)
                            putc_f(putc_data, ' ');
                    while(*str) {
                        putc_f(putc_data, *str++);
                    }
                    if (leftalign)
                        while(size_mod-- > 0)
                            putc_f(putc_data, ' ');
                    break;

                case 'u':
                    switch (length_mod) {
                        case 8:
                            value = va_arg(args, uint64_t);
                            break;
                        case 9:
                            value = va_arg(args, uintmax_t);
                            break;
                        case 10:
                            value = va_arg(args, uintptr_t);
                            break;
                        case 11:
                            value = va_arg(args, size_t);
                            break;
                        default:
                            value = va_arg(args, unsigned int);
                            break;
                    }
                    int_itoa(tmpbuf, 10, value, zero_pad, precision, size_mod, 0, alternate_form, 0, sign);
                    str = tmpbuf;
                    size_mod -= int_strlen(str);
                    if (!leftalign)
                        while(size_mod-- > 0)
                            putc_f(putc_data, ' ');
                    while(*str) {
                        putc_f(putc_data, *str++);
                    }
                    if (leftalign)
                        while(size_mod-- > 0)
                            putc_f(putc_data, ' ');
                    break;

                case 'd':
                case 'i':
                    switch (length_mod) {
                        case 8:
                            ivalue = va_arg(args, int64_t);
                            break;
                        case 9:
                            ivalue = va_arg(args, intmax_t);
                            break;
                        case 10:
                            ivalue = va_arg(args, intptr_t);
                            break;
                        case 11:
                            ivalue = va_arg(args, size_t);
                            break;
                        default:
                            ivalue = va_arg(args, int);
                            break;
                    }
                    if (ivalue < 0)
                        int_itoa(tmpbuf, 10, -ivalue, zero_pad, precision, size_mod, 0, alternate_form, 1, sign);
                    else
                        int_itoa(tmpbuf, 10, ivalue, zero_pad, precision, size_mod, 0, alternate_form, 0, sign);
                    str = tmpbuf;
                    size_mod -= int_strlen(str);
                    if (!leftalign)
                        while(size_mod-- > 0)
                            putc_f(putc_data, ' ');
                    while(*str) {
                        putc_f(putc_data, *str++);
                    }
                    if (leftalign)
                        while(size_mod-- > 0)
                            putc_f(putc_data, ' ');
                    break;

                case 'o':
                    switch (length_mod) {
                        case 8:
                            value = va_arg(args, uint64_t);
                            break;
                        case 9:
                            value = va_arg(args, uintmax_t);
                            break;
                        case 10:
                            value = va_arg(args, uintptr_t);
                            break;
                        case 11:
                            value = va_arg(args, size_t);
                            break;
                        default:
                            value = va_arg(args, uint32_t);
                            break;
                    }
                    int_itoa(tmpbuf, 8, value, zero_pad, precision, size_mod, 0, alternate_form, 0, sign);
                    str = tmpbuf;
                    size_mod -= int_strlen(str);
                    if (!leftalign)
                        while(size_mod-- > 0)
                            putc_f(putc_data, ' ');
                    while(*str) {
                        putc_f(putc_data, *str++);
                    }
                    if (leftalign)
                        while(size_mod-- > 0)
                            putc_f(putc_data, ' ');
                    break;

                case 'c':
                    putc_f(putc_data, va_arg(args, int));
                    break;

                case 's':
                    {
                        str = va_arg(args, char *);
                        do {
                            if (*str == 0)
                                break;
                            else
                                putc_f(putc_data, *str);
                            size_mod--;
                        } while(*str++ && --precision);
                        while (size_mod-- > 0)
                            putc_f(putc_data, ' ');
                    }
                    break;

                default:
                    putc_f(putc_data, c);
                    break;
            }
        }
    }
}

void kprintf_pc(putc_func putc_f, void *putc_data, const char * restrict format, ...)
{
    va_list v;
    va_start(v, format);
    vkprintf_pc(putc_f, putc_data, format, v);
    va_end(v);
}

void arm_flush_cache(uintptr_t addr, uint32_t length)
{
    int line_size = 0;
    uintptr_t top_addr = addr + length;

    asm volatile("mrs %0, CTR_EL0":"=r"(line_size));

    line_size = (line_size >> 16) & 15;
    line_size = 4 << line_size;

    __asm__ __volatile__("dsb sy");
    addr = addr & ~(line_size - 1);
    while (addr < top_addr)
    {
            __asm__ __volatile__("dc civac, %0"::"r"(addr));
            addr += line_size;
    }
    __asm__ __volatile__("dsb sy");
}

void arm_icache_invalidate(uintptr_t addr, uint32_t length)
{
    int line_size = 0;
    uintptr_t top_addr = addr + length;

    asm volatile("mrs %0, CTR_EL0":"=r"(line_size));

    line_size = line_size & 15;
    line_size = 4 << line_size;

    addr = addr & ~(line_size - 1);

    __asm__ __volatile__("dsb sy");
    while (addr < top_addr)
    {
            __asm__ __volatile__("ic ivau, %0"::"r"(addr));
            addr += line_size;
    }
    __asm__ __volatile__("dsb ish; isb sy");
}

void arm_dcache_invalidate(uintptr_t addr, uint32_t length)
{
    int line_size = 0;
    uintptr_t top_addr = addr + length;

    asm volatile("mrs %0, CTR_EL0":"=r"(line_size));

    line_size = (line_size >> 16) & 15;
    line_size = 4 << line_size;

    addr = addr & ~(line_size - 1);

    __asm__ __volatile__("dsb sy");
    while (addr < top_addr)
    {
            __asm__ __volatile__("dc ivac, %0"::"r"(addr));
            addr += line_size;
    }
    __asm__ __volatile__("dsb sy");
}

void putc_s(void *data, char c)
{
    char **ppchr = data;
    char *pchr = *ppchr;
    *pchr++ = c;
    *pchr = 0;
    *ppchr = pchr;
}

void sprintf(char *buf, const char * restrict format, ...)
{
    va_list v;
    va_start(v, format);
    vkprintf_pc(putc_s, &buf, format, v);
    va_end(v);
}

void __sprintf_chk(char *buf, int flag, size_t strlen, const char *format, ...)
{
    (void)flag;
    (void)strlen;
    va_list v;
    va_start(v, format);
    vkprintf_pc(putc_s, &buf, format, v);
    va_end(v);
}

size_t strlen(const char *c)
{
    size_t result = 0;
    while (*c++)
        result++;

    return result;
}

char * strncpy(char * dst, const char * src, size_t len)
{
    size_t slen = strlen(src);
    if (slen > len)
        slen = len;
    bzero(dst, len);
    memcpy(dst, src, slen);
    return dst;
}

char * strcpy(char * dst, const char * src)
{
    memcpy(dst, src, strlen(src) + 1);
    return dst;
}

char *__strcpy_chk(char *dst, const char *src, int destlen)
{
    int len = strlen(src) + 1;
    if (len > destlen) len = destlen;
    memcpy(dst, src, len);
    return dst;
}

int strcmp(const char *s1, const char *s2)
{
	while (*s1 == *s2++)
		if (*s1++ == '\0')
			return (0);
	return (*(const unsigned char *)s1 - *(const unsigned char *)(s2 - 1));
}

int strncmp(const char *s1, const char *s2, size_t n)
{
    if (n == 0) {
        return 0;
    }
	while (*s1 == *s2++) {
        if (--n == 0)
            return 0;
		if (*s1++ == '\0')
			return 0;
    }
	return (*(const unsigned char *)s1 - *(const unsigned char *)(s2 - 1));
}

const char *remove_path(const char *in)
{
    const char *p = &in[strlen(in)-1];
    while (p > in && p[-1] != '/' && p[-1] != ':') p--;
    return p;
}

int raise(int sig)
{
    kprintf("[BOOT] called raise(%d)\n", sig);
    (void)sig;
    return 0;
}

void bzero(void *ptr, size_t sz)
{
    char *p = ptr;
    if (p)
        while(sz--)
            *p++ = 0;
}

void *memset(void *ptr, int fill, size_t sz)
{
    uint8_t *p = ptr;
    if (p)
        while(sz--)
            *p++ = fill;
    return ptr;
}

void *memcpy(void *dst, const void *src, size_t sz)
{
    uint8_t *d = dst;
    const uint8_t *s = src;

    while(sz--)
	*d++ = *s++;

    return dst;
}

void *memmove(void *dst, const void *src, size_t sz)
{
    uint8_t *d = dst;
    const uint8_t *s = src;

    if (d > s)
    {
	d += sz;
	s += sz;

	while(sz--)
	    *--d = *--s;
    }
    else
	while(sz--)
	    *d++ = *s++;

    return dst;
}

char * strstr(const char *s, const char *find)
{
    char c, sc;
	size_t len;

	if ((c = *find++) != '\0') {
		len = strlen(find);
		do {
			do {
				if ((sc = *s++) == '\0')
					return (NULL);
			} while (sc != c);
		} while (strncmp(s, find, len) != 0);
		s--;
	}
	return ((char *)s);
}

void * tlsf;
void * jit_tlsf;

static const int32_t table[] = {
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,
16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,
32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,
48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,
64,
'a','b','c','d','e','f','g','h','i','j','k','l','m',
'n','o','p','q','r','s','t','u','v','w','x','y','z',
91,92,93,94,95,96,
'a','b','c','d','e','f','g','h','i','j','k','l','m',
'n','o','p','q','r','s','t','u','v','w','x','y','z',
123,124,125,126,127,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
};

static const int32_t *const ptable = table+128;

const int32_t **__ctype_tolower_loc(void)
{
    return (void *)&ptable;
}

int tolower(int c)
{
    if (c >= -128 && c < 256)
        return ptable[c];
    else return 0;
}

/*	$NetBSD: qsort.c,v 1.23 2017/05/19 19:48:19 christos Exp $	*/
/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

static inline char	*med3(char *, char *, char *,
    int (*)(const void *, const void *));
static inline void	 swapfunc(char *, char *, size_t, int);

#define min(a, b)	(a) < (b) ? a : b

/*
 * Qsort routine from Bentley & McIlroy's "Engineering a Sort Function".
 */
#define swapcode(TYPE, parmi, parmj, n) { 		\
	size_t i = (n) / sizeof (TYPE); 		\
	TYPE *pi = (TYPE *)(void *)(parmi); 		\
	TYPE *pj = (TYPE *)(void *)(parmj); 		\
	do { 						\
		TYPE	t = *pi;			\
		*pi++ = *pj;				\
		*pj++ = t;				\
        } while (--i > 0);				\
}

#define SWAPINIT(a, es) swaptype = ((char *)a - (char *)0) % sizeof(long) || \
	es % sizeof(long) ? 2 : es == sizeof(long)? 0 : 1;

static inline void
swapfunc(char *a, char *b, size_t n, int swaptype)
{

	if (swaptype <= 1) 
		swapcode(long, a, b, n)
	else
		swapcode(char, a, b, n)
}

#define swap(a, b)						\
	if (swaptype == 0) {					\
		long t = *(long *)(void *)(a);			\
		*(long *)(void *)(a) = *(long *)(void *)(b);	\
		*(long *)(void *)(b) = t;			\
	} else							\
		swapfunc(a, b, es, swaptype)

#define vecswap(a, b, n) if ((n) > 0) swapfunc((a), (b), (size_t)(n), swaptype)

static inline char *
med3(char *a, char *b, char *c,
    int (*cmp)(const void *, const void *))
{

	return cmp(a, b) < 0 ?
	       (cmp(b, c) < 0 ? b : (cmp(a, c) < 0 ? c : a ))
              :(cmp(b, c) > 0 ? b : (cmp(a, c) < 0 ? a : c ));
}

void
qsort(void *a, size_t n, size_t es,
    int (*cmp)(const void *, const void *))
{
	char *pa, *pb, *pc, *pd, *pl, *pm, *pn;
	size_t d, r, s;
	int swaptype, cmp_result;

loop:	SWAPINIT(a, es);
	if (n < 7) {
		for (pm = (char *) a + es; pm < (char *) a + n * es; pm += es)
			for (pl = pm; pl > (char *) a && cmp(pl - es, pl) > 0;
			     pl -= es)
				swap(pl, pl - es);
		return;
	}
	pm = (char *) a + (n / 2) * es;
	if (n > 7) {
		pl = (char *) a;
		pn = (char *) a + (n - 1) * es;
		if (n > 40) {
			d = (n / 8) * es;
			pl = med3(pl, pl + d, pl + 2 * d, cmp);
			pm = med3(pm - d, pm, pm + d, cmp);
			pn = med3(pn - 2 * d, pn - d, pn, cmp);
		}
		pm = med3(pl, pm, pn, cmp);
	}
	swap(a, pm);
	pa = pb = (char *) a + es;

	pc = pd = (char *) a + (n - 1) * es;
	for (;;) {
		while (pb <= pc && (cmp_result = cmp(pb, a)) <= 0) {
			if (cmp_result == 0) {
				swap(pa, pb);
				pa += es;
			}
			pb += es;
		}
		while (pb <= pc && (cmp_result = cmp(pc, a)) >= 0) {
			if (cmp_result == 0) {
				swap(pc, pd);
				pd -= es;
			}
			pc -= es;
		}
		if (pb > pc)
			break;
		swap(pb, pc);
		pb += es;
		pc -= es;
	}

	pn = (char *) a + n * es;
	r = min(pa - (char *) a, pb - pa);
	vecswap(a, pb - r, r);
	r = min((size_t)(pd - pc), pn - pd - es);
	vecswap(pb, pn - r, r);
	/*
	 * To save stack space we sort the smaller side of the partition first
	 * using recursion and eliminate tail recursion for the larger side.
	 */
	r = pb - pa;
	s = pd - pc;
	if (r < s) {
		/* Recurse for 1st side, iterate for 2nd side. */
		if (s > es) {
			if (r > es)
				qsort(a, r / es, es, cmp);
			a = pn - s;
			n = s / es;
			goto loop;
		}
	} else {
		/* Recurse for 2nd side, iterate for 1st side. */
		if (r > es) {
			if (s > es)
				qsort(pn - s, s / es, es, cmp);
			n = r / es;
			goto loop;
		}
	}
}

int abs(int a)
{
    if (a < 0) return -a;
    else return a;
}

char *strcat(char *s1, const char *s2)
{
    strcpy(s1 + strlen(s1), s2);
    return s1;
}

char *__strcat_chk(char *s1, const char *s2, int destlen)
{
    (void)destlen;
    strcpy(s1 + strlen(s1), s2);
    return s1;
}

void __attribute__((noreturn)) __assert_fail()
{
    kprintf("[ERR] Assertion failed!\n");
    while (1)
        asm volatile("wfi");
}
