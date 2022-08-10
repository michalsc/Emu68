/*
    Copyright © 2019 Michal Schulz <michal.schulz@gmx.de>
    https://github.com/michalsc

    This Source Code Form is subject to the terms of the
    Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed
    with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#ifndef _SUPPORT_H
#define _SUPPORT_H

#include <stdint.h>
#include <stdarg.h>

#ifdef __aarch64__
#include "A64.h"
#else
#include "ARM.h"
#endif

#ifdef NULL
#undef NULL
#endif
#define NULL ((void*)0)
#define TRUE 1
#define FALSE 0

#define xstr(s) str(s)
#define str(s) #s

#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__

#define L16(x) ((((x) & 0xff00) >> 8) | (((x) & 0x00ff) << 8))
#define L32(x) (((L16(x)) << 16) | L16(((x) >> 16) & 0xffff))
#define L64(x) (((L32(x)) << 32) | L32(((x) >> 32) & 0xffffffff))

static inline uint64_t BE64(uint64_t x) { return x; }
static inline uint32_t BE32(uint32_t x) { return x; }
static inline uint16_t BE16(uint16_t x) { return x; }

static inline uint64_t LE64(uint64_t x) { return __builtin_bswap64(x); }
static inline uint32_t LE32(uint32_t x) { return __builtin_bswap32(x); }
static inline uint16_t LE16(uint16_t x) { return __builtin_bswap16(x); }

#else

#define L16(x) (x)
#define L32(x) (x)
#define L64(x) (x)

static inline uint64_t BE64(uint64_t x) { return __builtin_bswap64(x); }
static inline uint32_t BE32(uint32_t x) { return __builtin_bswap32(x); }
static inline uint16_t BE16(uint16_t x) { return __builtin_bswap16(x); }

static inline uint64_t LE64(uint64_t x) { return x; }
static inline uint32_t LE32(uint32_t x) { return x; }
static inline uint16_t LE16(uint16_t x) { return x; }

#endif

typedef unsigned long size_t;

static inline uint32_t rd32le(uintptr_t iobase) {
    return LE32(*(volatile uint32_t *)(iobase));
}

static inline uint32_t rd32be(uintptr_t iobase) {
    return BE32(*(volatile uint32_t *)(iobase));
}

static inline uint16_t rd16le(uintptr_t iobase) {
    return LE16(*(volatile uint16_t *)(iobase));
}

static inline uint16_t rd16be(uintptr_t iobase) {
    return BE16(*(volatile uint16_t *)(iobase));
}

static inline uint8_t rd8(uintptr_t iobase) {
    return *(volatile uint8_t *)(iobase);
}

static inline void wr32le(uintptr_t iobase, uint32_t value) {
    *(volatile uint32_t *)(iobase) = LE32(value);
}

static inline void wr32be(uintptr_t iobase, uint32_t value) {
    *(volatile uint32_t *)(iobase) = BE32(value);
}

static inline void wr16le(uintptr_t iobase, uint16_t value) {
    *(volatile uint16_t *)(iobase) = LE16(value);
}

static inline void wr16be(uintptr_t iobase, uint16_t value) {
    *(volatile uint16_t *)(iobase) = BE16(value);
}

static inline void wr8(uintptr_t iobase, uint8_t value) {
    *(volatile uint8_t *)(iobase) = value;
}

#ifdef __aarch64__
static inline void dsb() {
    asm volatile ("dsb sy");
}

static inline void dmb() {
    asm volatile ("dmb sy");
}
#else
static inline void dsb() {
    asm volatile ("mcr p15,#0,%[zero],c7,c10,#4" : : [zero] "r" (0));
}

static inline void dmb() {
    asm volatile ("mcr p15,#0,%[zero],c7,c10,#5" : : [zero] "r" (0));
}
#endif

typedef void (*putc_func)(void *data, char c);
void vkprintf_pc(putc_func putc_f, void *putc_data, const char * format, va_list args);
void kprintf_pc(putc_func putc_f, void *putc_data, const char * format, ...);
void vkprintf(const char * format, va_list args);
void kprintf(const char * format, ...);
void arm_flush_cache(uintptr_t addr, uint32_t length);
void arm_icache_invalidate(uintptr_t addr, uint32_t length);
void arm_dcache_invalidate(uintptr_t addr, uint32_t length);
void clear_entire_dcache();
const char *remove_path(const char *in);
size_t strlen(const char *c);
int strcmp(const char *s1, const char *s2);
int strncmp(const char *s1, const char *s2, size_t n);
void *memmove(void *dst, const void *src, size_t sz);
void *memcpy(void *dst, const void *src, size_t sz);
void *memset(void *ptr, int fill, size_t sz);
char *strstr(const char *str, const char *find);
void bzero(void *ptr, size_t sz);
void platform_init();
void platform_post_init();
void setup_serial();
const char * find_token(const char * string, const char * token);

extern void * tlsf;
extern void * jit_tlsf;

struct MemoryBlock {
    uintptr_t mb_Base;
    uintptr_t mb_Size;
};

extern struct MemoryBlock *sys_memory;

struct Result32 {
    uint32_t q;
    uint32_t r;
};

struct Result64 {
    uint64_t q;
    uint64_t r;
};

struct BuildID {
    uint32_t bid_NameLen;
    uint32_t bid_DescLen;
    uint32_t bid_Type;
    uint8_t bid_Data[1];
};

struct Result32 uidiv(uint32_t n, uint32_t d);
struct Result32 sidiv(int32_t n, int32_t d);
struct Result64 uldiv(uint64_t n, uint64_t d);
struct Result64 sldiv(int64_t n, int64_t d);

#ifdef RASPI
#include "support_rpi.h"
#endif

#endif /* _SUPPORT_H */
