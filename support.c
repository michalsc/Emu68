/*
    Copyright © 2019 Michal Schulz <michal.schulz@gmx.de>
    https://github.com/michalsc

    This Source Code Form is subject to the terms of the
    Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed
    with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <stdarg.h>
#include "support.h"

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
                    length_mod = 8;
                }
                else length_mod = 4;
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

                case 'p':
                    value = va_arg(args, uintptr_t);
                    int_itoa(tmpbuf, 16, value, 1, 2*sizeof(uintptr_t), 2*sizeof(uintptr_t), big, 1, 0, sign);
                    str = tmpbuf;
                    size_mod -= int_strlen(str);
                    do {
                        putc_f(putc_data, *str);
                    } while(*str++);
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
                    do {
                        putc_f(putc_data, *str);
                    } while(*str++);
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
                    do {
                        putc_f(putc_data, *str);
                    } while(*str++);
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
                    do {
                        putc_f(putc_data, *str);
                    } while(*str++);
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
                    do {
                        putc_f(putc_data, *str);
                    } while(*str++);
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
                        } while(*str++ && --precision);
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

#ifdef __aarch64__

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
    __asm__ __volatile__("isb sy");
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

#else

void arm_flush_cache(uintptr_t addr, uint32_t length)
{
        length = (length + 31) & ~31;
        while (length)
        {
                __asm__ __volatile__("mcr p15, 0, %0, c7, c14, 1"::"r"(addr));
                addr += 32;
                length -= 32;
        }
        __asm__ __volatile__("mcr p15, 0, %0, c7, c10, 4"::"r"(addr));
}

void arm_icache_invalidate(uintptr_t addr, uint32_t length)
{
    length = (length + 31) & ~31;
        while (length)
        {
                __asm__ __volatile__("mcr p15, 0, %0, c7, c5, 1"::"r"(addr));
                addr += 32;
                length -= 32;
        }
        __asm__ __volatile__("mcr p15, 0, %0, c7, c10, 4"::"r"(addr));
}

void arm_dcache_invalidate(uintptr_t addr, uint32_t length)
{
    length = (length + 31) & ~31;
        while (length)
        {
                __asm__ __volatile__("mcr p15, 0, %0, c7, c6, 1"::"r"(addr));
                addr += 32;
                length -= 32;
        }
        __asm__ __volatile__("mcr p15, 0, %0, c7, c10, 4"::"r"(addr));
}

#endif

size_t strlen(const char *c)
{
    size_t result = 0;
    while (*c++)
        result++;

    return result;
}

int strcmp(const char *s1, const char *s2)
{
	while (*s1 == *s2++)
		if (*s1++ == '\0')
			return (0);
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

void memset(void *ptr, uint8_t fill, size_t sz)
{
    uint8_t *p = ptr;
    if (p)
        while(sz--)
            *p++ = fill;
}

void memcpy(void *dst, const void *src, size_t sz)
{
    uint8_t *d = dst;
    const uint8_t *s = src;

    while(sz--)
	*d++ = *s++;
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

void * tlsf;
void * jit_tlsf;
