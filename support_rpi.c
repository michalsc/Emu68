#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>

#include "support_rpi.h"

static int serial_up = 0;

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

#define ARM_PERIIOBASE ((uint32_t)io_base)

static inline void waitSerOUT(void *io_base)
{
    while(1)
    {
       if ((rd32le(PL011_0_BASE + PL011_FR) & PL011_FR_TXFF) == 0) break;
    }
}

static inline void putByte(void *io_base, char chr)
{
    if (serial_up)
    {
        waitSerOUT(io_base);

        if (chr == '\n')
        {
            wr32le(PL011_0_BASE + PL011_DR, '\r');
            waitSerOUT(io_base);
        }
        wr32le(PL011_0_BASE + PL011_DR, (uint8_t)chr);
        waitSerOUT(io_base);
    }
}

void kprintf_pc(putc_func putc_f, void *putc_data, const char * restrict format, ...)
{
    va_list v;
    va_start(v, format);
    vkprintf_pc(putc_f, putc_data, format, v);
    va_end(v);
}

void kprintf(const char * restrict format, ...)
{
    va_list v;
    va_start(v, format);
    vkprintf_pc(putByte, (void*)0xf2000000, format, v);
    va_end(v);
}

void vkprintf(const char * restrict format, va_list args)
{
    vkprintf_pc(putByte, (void*)0xf2000000, format, args);
}

void arm_flush_cache(uint32_t addr, uint32_t length)
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

void arm_icache_invalidate(uint32_t addr, uint32_t length)
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

void arm_dcache_invalidate(uint32_t addr, uint32_t length)
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

int32_t strlen(const char *c)
{
        int32_t result = 0;
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

static inline void dsb() {
    asm volatile ("mcr p15,#0,%[zero],c7,c10,#4" : : [zero] "r" (0));
}

static inline void dmb() {
    asm volatile ("mcr p15,#0,%[zero],c7,c10,#5" : : [zero] "r" (0));
}

/* status register flags */

#define MBOX_TX_FULL (1UL << 31)
#define MBOX_RX_EMPTY (1UL << 30)
#define MBOX_CHANMASK 0xF

/* VideoCore tags used. */

#define VCTAG_GET_ARM_MEMORY     0x00010005
#define VCTAG_GET_CLOCK_RATE     0x00030002

#define VCCLOCK_PIXEL            9

/*----------------------------------------------------------------------------*/

static uint32_t mbox_recv(uint32_t channel)
{
	volatile uint32_t *mbox_read = (uint32_t*)0xf200B880;
	volatile uint32_t *mbox_status = (uint32_t*)0xf200B898;
	uint32_t response, status;

	do
	{
		do
		{
			status = LE32(*mbox_status);
			dsb();
		}
		while (status & MBOX_RX_EMPTY);

		dmb();
		response = LE32(*mbox_read);
		dmb();
	}
	while ((response & MBOX_CHANMASK) != channel);

	return (response & ~MBOX_CHANMASK);
}

/*----------------------------------------------------------------------------*/

static void mbox_send(uint32_t channel, uint32_t data)
{
	volatile uint32_t *mbox_write = (uint32_t*)0xF200B8A0;
	volatile uint32_t *mbox_status = (uint32_t*)0xF200B898;
	uint32_t status;

	data &= ~MBOX_CHANMASK;
	data |= channel & MBOX_CHANMASK;

	do
	{
		status = LE32(*mbox_status);
		dsb();
	}
	while (status & MBOX_TX_FULL);

	dmb();
	*mbox_write = LE32(data);
}

uint32_t FBReq[32] __attribute__((aligned(16)));

uint32_t get_clock_rate(uint32_t clock_id)
{
    FBReq[0] = LE32(4*8);       // Length
    FBReq[1] = 0;               // Request
    FBReq[2] = LE32(0x00030002);// GetClockRate
    FBReq[3] = LE32(8);
    FBReq[4] = 0;
    FBReq[5] = LE32(clock_id);
    FBReq[6] = 0;
    FBReq[7] = 0;

    arm_flush_cache((uint32_t)FBReq, 32);
    mbox_send(8, 0x000fffff & (uint32_t)FBReq);
    mbox_recv(8);

    return FBReq[6];
}

uint32_t get_max_clock_rate(uint32_t clock_id)
{
    FBReq[0] = LE32(4*8);       // Length
    FBReq[1] = 0;               // Request
    FBReq[2] = LE32(0x00030004);// GetMaxClockRate
    FBReq[3] = LE32(8);
    FBReq[4] = 0;
    FBReq[5] = LE32(clock_id);
    FBReq[6] = 0;
    FBReq[7] = 0;

    arm_flush_cache((uint32_t)FBReq, 32);
    mbox_send(8, 0x000fffff & (uint32_t)FBReq);
    mbox_recv(8);

    return FBReq[6];
}

uint32_t get_min_clock_rate(uint32_t clock_id)
{
    FBReq[0] = LE32(4*8);       // Length
    FBReq[1] = 0;               // Request
    FBReq[2] = LE32(0x00030007);// GetClockRate
    FBReq[3] = LE32(8);
    FBReq[4] = 0;
    FBReq[5] = LE32(clock_id);
    FBReq[6] = 0;
    FBReq[7] = 0;

    arm_flush_cache((uint32_t)FBReq, 32);
    mbox_send(8, 0x000fffff & (uint32_t)FBReq);
    mbox_recv(8);

    return FBReq[6];
}

uint32_t set_clock_rate(uint32_t clock_id, uint32_t speed)
{
    FBReq[0] = LE32(4*9);       // Length
    FBReq[1] = 0;               // Request
    FBReq[2] = LE32(0x00038002);// SetClockRate
    FBReq[3] = LE32(12);
    FBReq[4] = 0;
    FBReq[5] = LE32(clock_id);
    FBReq[6] = LE32(speed);
    FBReq[7] = 0;
    FBReq[7] = 0;

    arm_flush_cache((uint32_t)FBReq, 36);
    mbox_send(8, 0x000fffff & (uint32_t)FBReq);
    mbox_recv(8);

    return FBReq[6];
}

#define PL011_ICR_FLAGS (PL011_ICR_RXIC|PL011_ICR_TXIC|PL011_ICR_RTIC|PL011_ICR_FEIC|PL011_ICR_PEIC|PL011_ICR_BEIC|PL011_ICR_OEIC|PL011_ICR_RIMIC|PL011_ICR_CTSMIC|PL011_ICR_DSRMIC|PL011_ICR_DCDMIC)

#define DEF_BAUD 115200

#define PL011_DIVCLOCK(baud, clock)     ((clock * 4) / baud)
#define PL011_BAUDINT(baud, clock)      ((PL011_DIVCLOCK(baud, clock) & 0xFFFFFFC0) >> 6)
#define PL011_BAUDFRAC(baud, clock)     ((PL011_DIVCLOCK(baud, clock) & 0x0000003F) >> 0)

#undef ARM_PERIIOBASE
#define ARM_PERIIOBASE 0xf2000000

#define GPIO_BASE                                       (ARM_PERIIOBASE + 0x200000)
#define GPFSEL1                                         (GPIO_BASE + 0x4)
#define GPPUD                                           (GPIO_BASE + 0x94)
#define GPPUDCLK0                                       (GPIO_BASE + 0x98)

void setup_serial()
{
    unsigned int        uartvar;
    unsigned const int uartbaud = 115200;
    unsigned int uartclock;
    unsigned int uartdivint;
    unsigned int uartdivfrac;

    uartclock = get_clock_rate(2);

    wr32le(PL011_0_BASE + PL011_CR, 0);

    uartvar = rd32le(GPFSEL1);
    uartvar &= ~(7<<12);                        // TX on GPIO14
    uartvar |= 4<<12;                           // alt0
    uartvar &= ~(7<<15);                        // RX on GPIO15
    uartvar |= 4<<15;                           // alt0
    wr32le(GPFSEL1, uartvar);

    /* Disable pull-ups and pull-downs on rs232 lines */
    wr32le(GPPUD, 0);

    for (uartvar = 0; uartvar < 150; uartvar++) asm volatile ("mov r0, r0\n");

    wr32le(GPPUDCLK0, (1 << 14)|(1 << 15));

    for (uartvar = 0; uartvar < 150; uartvar++) asm volatile ("mov r0, r0\n");

    wr32le(GPPUDCLK0, 0);

    wr32le(PL011_0_BASE + PL011_ICR, PL011_ICR_FLAGS);
    uartdivint = PL011_BAUDINT(uartbaud, uartclock);
    wr32le(PL011_0_BASE + PL011_IBRD, uartdivint);
    uartdivfrac = PL011_BAUDFRAC(uartbaud, uartclock);
    wr32le(PL011_0_BASE + PL011_FBRD, uartdivfrac);
    wr32le(PL011_0_BASE + PL011_LCRH, PL011_LCRH_WLEN8|PL011_LCRH_FEN);           // 8N1, Fifo enabled
    wr32le(PL011_0_BASE + PL011_CR, PL011_CR_UARTEN|PL011_CR_TXE|PL011_CR_RXE);   // enable the uart, tx and rx

    for (uartvar = 0; uartvar < 150; uartvar++) asm volatile ("mov r0, r0\n");

    serial_up = 1;
}
