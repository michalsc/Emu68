#include <stdint.h>
#include <stdarg.h>
#include <stddef.h>

void  __attribute__((used,aligned(256),section(".vectors"))) __stub_vectors() {
__asm__(
"       .section .vectors               \n"

"       .org 0x100,0                    \n"
"       .globl SystemReset              \n"
"SystemReset:                           \n"
"       lis %r1, 0xffef                 \n"
"       ori %r1, %r1, 0xffe0            \n"
"       bl PPC_C_Init                   \n"
"1:     b 1b                            \n"

"       .org 0x200,0                    \n"
"       .globl MachineCheck             \n"
"MachineCheck:                          \n"
"1:     b 1b                            \n"

"       .org 0x300,0                    \n"
"       .globl DSI                      \n"
"DSI:                                   \n"
"1:     b 1b                            \n"

"       .org 0x400,0                    \n"
"       .globl ISI                      \n"
"ISI:                                   \n"
"1:     b 1b                            \n"

"       .org 0x500,0                    \n"
"       .globl ExternalInt              \n"
"ExternalInt:                           \n"
"1:     b 1b                            \n"

"       .org 0x600,0                    \n"
"       .globl Alignment                \n"
"Alignment:                             \n"
"1:     b 1b                            \n"

"       .org 0x700,0                    \n"
"       .globl Program                  \n"
"Program:                               \n"
"1:     b 1b                            \n"

);
}


/*********** SUPPORT *************/

static inline __attribute__((always_inline)) uint64_t BE64(uint64_t x)
{
    return x;
}

static inline __attribute__((always_inline)) uint64_t LE64(uint64_t x)
{
    return __builtin_bswap64(x);
}

static inline __attribute__((always_inline)) uint32_t BE32(uint32_t x)
{
    return x;
}

static inline __attribute__((always_inline)) uint32_t LE32(uint32_t x)
{
    return __builtin_bswap32(x);
}

static inline __attribute__((always_inline)) uint16_t BE16(uint16_t x)
{
    return x;
}

static inline __attribute__((always_inline)) uint16_t LE16(uint16_t x)
{
    return __builtin_bswap16(x);
}


static inline uint32_t rd32le(uint32_t iobase) {
    return LE32(*(volatile uint32_t *)(iobase));
}

static inline uint32_t rd32be(uint32_t iobase) {
    return BE32(*(volatile uint32_t *)(iobase));
}

static inline uint16_t rd16le(uint32_t iobase) {
    return LE16(*(volatile uint16_t *)(iobase));
}

static inline uint16_t rd16be(uint32_t iobase) {
    return BE16(*(volatile uint16_t *)(iobase));
}

static inline uint8_t rd8(uint32_t iobase) {
    return *(volatile uint8_t *)(iobase);
}

static inline void wr32le(uint32_t iobase, uint32_t value) {
    *(volatile uint32_t *)(iobase) = LE32(value);
}

static inline void wr32be(uint32_t iobase, uint32_t value) {
    *(volatile uint32_t *)(iobase) = BE32(value);
}

static inline void wr16le(uint32_t iobase, uint16_t value) {
    *(volatile uint16_t *)(iobase) = LE16(value);
}

static inline void wr16be(uint32_t iobase, uint16_t value) {
    *(volatile uint16_t *)(iobase) = BE16(value);
}

static inline void wr8(uint32_t iobase, uint8_t value) {
    *(volatile uint8_t *)(iobase) = value;
}

typedef void (*putc_func)(void *data, char c);


char *
strcpy(char *s1, const char *s2)
{
    char *s = s1;
    while ((*s++ = *s2++) != 0)
	;
    return (s1);
}

int
strcmp(const char *s1, const char *s2)
{
    for ( ; *s1 == *s2; s1++, s2++)
	if (*s1 == '\0')
	    return 0;
    return ((*(unsigned char *)s1 < *(unsigned char *)s2) ? -1 : +1);
}

int int_strlen(char *buf)
{
    int len = 0;

    if (buf)
        while(*buf++)
            len++;

    return len;
}


void int_itoa(char *buf, char base, uintptr_t value, char zero_pad, int precision, int size_mod, char big, int alternate_form, int neg, char sign)
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

void putByte(void *data, char c)
{
    (void)data;
    *(volatile uint8_t *)0xdeadbeef = c;
}

void kprintf(const char * restrict format, ...)
{
    va_list v;
    va_start(v, format);
    vkprintf_pc(putByte, 0, format, v);
    va_end(v);
}

asm(
"delay_loop:    \n"
"    mtctr   %r3 \n"     /* number of iterations in r3 */
"1:  bdnz+    1b \n"
"    blr");

void delay_loop(uint32_t count);

void GetBogoMIPS()
{
    uint32_t Begin_Time, End_Time;

    asm volatile("lwbrx %0, 0, %1":"=r"(Begin_Time):"r"(0xf2003004));
    delay_loop(100000000);
    asm volatile("lwbrx %0, 0, %1":"=r"(End_Time):"r"(0xf2003004));

    kprintf("100000000 loop cycles in %d us -> %d BogoMIPS\n", End_Time - Begin_Time, 100000000 / (End_Time - Begin_Time));
}

void PPC_C_Init(uint16_t *framebuffer, uint32_t fb_width, uint32_t fb_height, uint32_t pitch)
{
    uint32_t Begin_Time, End_Time;
    uint32_t Begin_Cycles, End_Cycles;
    char buf[20];

    kprintf("Hello, PPC\n");
    kprintf("Here is Emu68, %s, speaking ;)\n", "or maybe EmuPPC");
    kprintf("Testing literals:\n");
    kprintf("  %%s: %s\n", "this is a text");
    kprintf("  %%c: %c\n", 'A');
    kprintf("  %%d: %d\n", 1536);
    kprintf("  %%x: %x\n", 0xdeadbeef);


    GetBogoMIPS();

    uint32_t start, end;

    asm volatile("mfspr %0, 900":"=r"(start));

    asm volatile("lwbrx %0, 0, %1":"=r"(Begin_Time):"r"(0xf2003004));
    asm volatile("mftbl %0":"=r"(Begin_Cycles));

    const uint32_t w = 400;
    const uint32_t h = 300;
    const uint32_t start_x = (fb_width - w) / 2;
    const uint32_t start_y = (fb_height - h) / 2;
    (void)pitch;
    uint32_t c = 0;
    
    for (int i=0; i < 10000; i++)
    {
        uint16_t *ptr = framebuffer + start_y * fb_width + start_x;

        for (unsigned y = 0; y < h; y++) {
            for (unsigned x=0; x < w; x++) {
                ptr[x] = c++;
            }
            ptr += fb_width;
        }
    }
    kprintf("%d\n", c);

    asm volatile("lwbrx %0, 0, %1":"=r"(End_Time):"r"(0xf2003004));
    asm volatile("mftbl %0":"=r"(End_Cycles));

    asm volatile("mfspr %0, 900":"=r"(end));

    kprintf("Test loop time: %d us\n", End_Time - Begin_Time);
    kprintf("Test loop cycles: %d\n", End_Cycles - Begin_Cycles);
    kprintf("Test loop instructions: %u\n", end - start);
    uint32_t speed = (((end - start) / ((End_Time - Begin_Time) / 1000)) ) / 100;
    kprintf("Test loop speed: %u.%u MIPS\n", speed / 10, speed % 10);

    int_itoa(buf, 10, 1536, 0, 0, 0, 0, 0, 0, 0);
    kprintf(buf);
}

