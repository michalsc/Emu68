#ifndef _SUPPORT_RPI_H
#define _SUPPORT_RPI_H

#include <stdarg.h>
#include "ARM.h"

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
void vkprintf_pc(putc_func putc_f, void *putc_data, const char * restrict format, va_list args);
void kprintf_pc(putc_func putc_f, void *putc_data, const char * restrict format, ...);
void vkprintf(const char * restrict format, va_list args);
void kprintf(const char * restrict format, ...);
void arm_flush_cache(uint32_t addr, uint32_t length);
void arm_icache_invalidate(uint32_t addr, uint32_t length);
void arm_dcache_invalidate(uint32_t addr, uint32_t length);
const char *remove_path(const char *in);
extern void * tlsf;
uint32_t set_clock_rate(uint32_t clock_id, uint32_t speed);
uint32_t get_min_clock_rate(uint32_t clock_id);
uint32_t get_max_clock_rate(uint32_t clock_id);
uint32_t get_clock_rate(uint32_t clock_id);
void setup_serial();
struct Size { uint16_t widht; uint16_t height; };
struct Size get_display_size();

struct Result32 {
    uint32_t q;
    uint32_t r;
};

struct Result64 {
    uint64_t q;
    uint64_t r;
};

struct Result32 uidiv(uint32_t n, uint32_t d);
struct Result32 sidiv(int32_t n, int32_t d);
struct Result64 uldiv(uint64_t n, uint64_t d);
struct Result64 sldiv(int64_t n, int64_t d);

#endif // _SUPPORT_RPI_H
