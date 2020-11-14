/*
    SysInfo routine and surrounding C support code for RasPi
*/
#include <stdint.h>
#include <stdarg.h>
#include <stddef.h>

#define HZ 1000000

extern void SI_BusTest(unsigned long number_of_runs asm("d0"));
extern void SI_Start();
extern void SI_Start_Nr(unsigned long number_of_runs asm("d2"));

void _main(int n);

extern uint16_t *framebuffer;
extern uint32_t pitch;
void put_char(uint8_t c);

int __start(uint32_t p asm("d0"), uint16_t *fb asm("a0"))
{
  framebuffer = fb;
  pitch = p;

  _main(1000);
}

uint16_t *framebuffer = (void*)0;
uint32_t pitch = 0;

extern const uint32_t topaz8_charloc[];
extern const uint8_t topaz8_chardata[];

uint32_t text_x = 0;
uint32_t text_y = 0;
const int modulo = 192;

void put_char(uint8_t c)
{
    if (framebuffer && pitch)
    {
    uint16_t *pos_in_image = (uint16_t*)((uint32_t)framebuffer + (text_y * 16 + 5)* pitch);
    pos_in_image += 4 + text_x * 8;

    if (c == 10) {
	text_x = 0;
	text_y++;
    }
    else if (c >= 32) {
        uint32_t loc = (topaz8_charloc[c - 32] >> 16) >> 3;
        const uint8_t *data = &topaz8_chardata[loc];

        for (int y = 0; y < 16; y++) {
            const uint8_t byte = *data;

            for (int x=0; x < 8; x++) {
                if (byte & (0x80 >> x)) {
                    pos_in_image[x] = 0;
                }
            }

            if (y & 1)
                data += modulo;
            pos_in_image += pitch / 2;
        }
    text_x++;
    }

    }
}

/*********** SUPPORT *************/

static inline __attribute__((always_inline)) uint64_t BE64(uint64_t x)
{
    union {
        uint64_t v;
        uint8_t u[8];
    } tmp;

    tmp.v = x;

    return ((uint64_t)(tmp.u[0]) << 56) | ((uint64_t)(tmp.u[1]) << 48) | ((uint64_t)(tmp.u[2]) << 40) | ((uint64_t)(tmp.u[3]) << 32) |
        (tmp.u[4] << 24) | (tmp.u[5] << 16) | (tmp.u[6] << 8) | (tmp.u[7]);
}

static inline __attribute__((always_inline)) uint64_t LE64(uint64_t x)
{
    union {
        uint64_t v;
        uint8_t u[8];
    } tmp;

    tmp.v = x;

    return ((uint64_t)(tmp.u[7]) << 56) | ((uint64_t)(tmp.u[6]) << 48) | ((uint64_t)(tmp.u[5]) << 40) | ((uint64_t)(tmp.u[4]) << 32) |
        (tmp.u[3] << 24) | (tmp.u[2] << 16) | (tmp.u[1] << 8) | (tmp.u[0]);
}

static inline __attribute__((always_inline)) uint32_t BE32(uint32_t x)
{
    union {
        uint32_t v;
        uint8_t u[4];
    } tmp;

    tmp.v = x;

    return (tmp.u[0] << 24) | (tmp.u[1] << 16) | (tmp.u[2] << 8) | (tmp.u[3]);
}

static inline __attribute__((always_inline)) uint32_t LE32(uint32_t x)
{
    union {
        uint32_t v;
        uint8_t u[4];
    } tmp;

    tmp.v = x;

    return (tmp.u[3] << 24) | (tmp.u[2] << 16) | (tmp.u[1] << 8) | (tmp.u[0]);
}

static inline __attribute__((always_inline)) uint16_t BE16(uint16_t x)
{
    union {
        uint16_t v;
        uint8_t u[2];
    } tmp;

    tmp.v = x;

    return (tmp.u[0] << 8) | (tmp.u[1]);
}

static inline __attribute__((always_inline)) uint16_t LE16(uint16_t x)
{
    union {
        uint16_t v;
        uint8_t u[2];
    } tmp;

    tmp.v = x;

    return (tmp.u[1] << 8) | (tmp.u[0]);
}


#define PL011_0_BASE              (ARM_PERIIOBASE + 0x201000)
#define PRIMECELLID_PL011       0x011

#define PL011_DR                 (0x00)
#define PL011_RSRECR             (0x04)
#define PL011_FR                 (0x18)
#define PL011_ILPR               (0x20)
#define PL011_IBRD               (0x24)
#define PL011_FBRD               (0x28)
#define PL011_LCRH               (0x2C)
#define PL011_CR                 (0x30)
#define PL011_IFLS               (0x34)
#define PL011_IMSC               (0x38)
#define PL011_RIS                (0x3C)
#define PL011_MIS                (0x40)
#define PL011_ICR                (0x44)
#define PL011_DMACR              (0x48)
#define PL011_ITCR               (0x80)
#define PL011_ITIP               (0x84)
#define PL011_ITOP               (0x88)
#define PL011_TDR                (0x8C)

#define PL011_FR_CTS             (1 << 0)
#define PL011_FR_DSR             (1 << 1)
#define PL011_FR_DCD             (1 << 2)
#define PL011_FR_BUSY            (1 << 3)
#define PL011_FR_RXFE            (1 << 4)
#define PL011_FR_TXFF            (1 << 5)
#define PL011_FR_RXFF            (1 << 6)
#define PL011_FR_TXFE            (1 << 7)

#define PL011_LCRH_BRK           (1 << 0)
#define PL011_LCRH_PEN           (1 << 1)
#define PL011_LCRH_EPS           (1 << 2)
#define PL011_LCRH_STP2          (1 << 3)
#define PL011_LCRH_FEN           (1 << 4)
#define PL011_LCRH_WLEN5         (0 << 5)
#define PL011_LCRH_WLEN6         (1 << 5)
#define PL011_LCRH_WLEN7         (2 << 5)
#define PL011_LCRH_WLEN8         (3 << 5)
#define PL011_LCRH_SPS           (1 << 7)

#define PL011_CR_UARTEN          (1 << 0)
#define PL011_CR_SIREN           (1 << 1)
#define PL011_CR_SIRLP           (1 << 2)
#define PL011_CR_LBE             (1 << 7)
#define PL011_CR_TXE             (1 << 8)
#define PL011_CR_RXE             (1 << 9)
#define PL011_CR_RTSEN           (1 << 14)
#define PL011_CR_CTSEN           (1 << 15)

#define PL011_ICR_RIMIC          (1 << 0)
#define PL011_ICR_CTSMIC         (1 << 1)
#define PL011_ICR_DSRMIC         (1 << 2)
#define PL011_ICR_DCDMIC         (1 << 3)
#define PL011_ICR_RXIC           (1 << 4)
#define PL011_ICR_TXIC           (1 << 5)
#define PL011_ICR_RTIC           (1 << 6)
#define PL011_ICR_FEIC           (1 << 7)
#define PL011_ICR_PEIC           (1 << 8)
#define PL011_ICR_BEIC           (1 << 9)
#define PL011_ICR_OEIC           (1 << 10)

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

#define ARM_PERIIOBASE ((uint32_t)io_base)

void waitSerOUT(void *io_base)
{
    while(1)
    {
       if ((rd32le(PL011_0_BASE + PL011_FR) & PL011_FR_TXFF) == 0) break;
    }
}

void putByte(void *io_base, char chr)
{
    waitSerOUT(io_base);

    if (chr == '\n')
    {
        wr32le(PL011_0_BASE + PL011_DR, '\r');
        waitSerOUT(io_base);
    }
    wr32le(PL011_0_BASE + PL011_DR, (uint8_t)chr);
    put_char(chr);
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

void vkprintf(const char * restrict format, va_list args)
{
    vkprintf_pc(putByte, (void*)0xf2000000, format, args);
}

/* variables for time measurement: */

#define Too_Small_Time (2*HZ)

uint32_t       Begin_Time,
                End_Time,
                User_Time;
uint32_t	Number_Of_Runs;
uint32_t    Clock_Frequency;
void _main (int n)
{
  asm volatile("movec #0xc00,%0":"=r"(Clock_Frequency));
  Clock_Frequency = (Clock_Frequency + HZ / 2) / HZ;
  Number_Of_Runs = 2000000;
  kprintf("[SysInfo] Running BUSTEST (%d loops)\n", Number_Of_Runs);
  kprintf("[SysInfo] Execution starts\n");
  asm volatile("movec #0xc01,%0":"=r"(Begin_Time));
  SI_BusTest(Number_Of_Runs);
  asm volatile("movec #0xc01,%0":"=r"(End_Time));
  kprintf("[SysInfo] Execution ends\n");
  User_Time = End_Time - Begin_Time;
  kprintf("[SysInfo] Begin time: %d\n", Begin_Time / Clock_Frequency);
  kprintf("[SysInfo] End time: %d\n", End_Time / Clock_Frequency);
  kprintf("[SysInfo] User time: %d\n", User_Time / Clock_Frequency);

    double Microseconds;
    double Megabytes_Per_Second;


  {
    Microseconds = ((double)User_Time / (double)Number_Of_Runs) / (double)Clock_Frequency;
    Megabytes_Per_Second = (double)Clock_Frequency * (double)Number_Of_Runs * 128e6 / (double)User_Time;
    Megabytes_Per_Second = Megabytes_Per_Second / (1024 * 1024);
  }

  kprintf("[SysInfo] Starting SysInfo like Dhrystone benchmark.\n[SysInfo] Performing %d loops\n", n);
  Number_Of_Runs = n;
  kprintf("[SysInfo] Execution starts\n");
  do {
    Number_Of_Runs = Number_Of_Runs << 2;

    asm volatile("movec #0xc01,%0":"=r"(Begin_Time));
    SI_Start_Nr(Number_Of_Runs);
    asm volatile("movec #0xc01,%0":"=r"(End_Time));

    User_Time = End_Time - Begin_Time;
  } while (User_Time/Clock_Frequency < Too_Small_Time);

  kprintf("[SysInfo] Execution ends, final loop count was %d\n", Number_Of_Runs);


  kprintf("[SysInfo] Begin time: %d\n", Begin_Time / Clock_Frequency);
  kprintf("[SysInfo] End time: %d\n", End_Time / Clock_Frequency);
  kprintf("[SysInfo] User time: %d\n", User_Time / Clock_Frequency);

    kprintf ("[SysInfo] Microseconds for one run through BUSTEST:   ");
    kprintf ("%d.%03d \n", (uint32_t)(Microseconds), ((uint32_t)(Microseconds * 1000.0)) % 1000);

    kprintf ("[SysInfo] Memory performance in BUSTEST:              ");
    kprintf ("%d MiB/s\n", (uint32_t)Megabytes_Per_Second);


  {
    double Microseconds = ((double)User_Time / (double)Number_Of_Runs) / (double)Clock_Frequency;
    double Dhrystones_Per_Second = (double)Clock_Frequency * 1e6 * (double)Number_Of_Runs / (double)User_Time;
    double MIPS = Dhrystones_Per_Second / 958.0;

    kprintf ("[SysInfo] Microseconds for one run through Dhrystone: ");
    kprintf ("%d.%03d \n", (uint32_t)(Microseconds), ((uint32_t)(Microseconds * 1000.0)) % 1000);
    kprintf ("[SysInfo] SysInfo Dhrystones:                         ");
    kprintf ("%d \n", (uint32_t)Dhrystones_Per_Second);
    kprintf ("[SysInfo] SysInfo MIPS      :                         ");
    kprintf ("%d.%02d \n", (uint32_t)MIPS, ((uint32_t)(MIPS * 10.0) % 10));
    if (User_Time < Too_Small_Time)
    {
        kprintf ("[SysInfo]   Measured time too small to obtain meaningful results\n");
        kprintf ("[SysInfo]   Please increase number of runs\n");
    }


  }
}
