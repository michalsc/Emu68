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

#define NULL ((void*)0)
#define TRUE 1
#define FALSE 0

#define xstr(s) str(s)
#define str(s) #s

#if EMU68_HOST_BIG_ENDIAN
#define L16(x) ((((x) & 0xff00) >> 8) | (((x) & 0x00ff) << 8))
#define L32(x) (((L16(x)) << 16) | L16(((x) >> 16) & 0xffff))
#define L64(x) (((L32(x)) << 32) | L32(((x) >> 32) & 0xffffffff))
#else
#define L16(x) (x)
#define L32(x) (x)
#define L64(x) (x)
#endif

static inline uint64_t BE64(uint64_t x)
{
    union {
        uint64_t v;
        uint8_t u[8];
    } tmp;

    tmp.v = x;

    return ((uint64_t)(tmp.u[0]) << 56) | ((uint64_t)(tmp.u[1]) << 48) | ((uint64_t)(tmp.u[2]) << 40) | ((uint64_t)(tmp.u[3]) << 32) |
        (tmp.u[4] << 24) | (tmp.u[5] << 16) | (tmp.u[6] << 8) | (tmp.u[7]);
}

static inline uint64_t LE64(uint64_t x)
{
    union {
        uint64_t v;
        uint8_t u[8];
    } tmp;

    tmp.v = x;

    return ((uint64_t)(tmp.u[7]) << 56) | ((uint64_t)(tmp.u[6]) << 48) | ((uint64_t)(tmp.u[5]) << 40) | ((uint64_t)(tmp.u[4]) << 32) |
        (tmp.u[3] << 24) | (tmp.u[2] << 16) | (tmp.u[1] << 8) | (tmp.u[0]);
}

static inline uint32_t BE32(uint32_t x)
{
    union {
        uint32_t v;
        uint8_t u[4];
    } tmp;

    tmp.v = x;

    return (tmp.u[0] << 24) | (tmp.u[1] << 16) | (tmp.u[2] << 8) | (tmp.u[3]);
}

static inline uint32_t LE32(uint32_t x)
{
    union {
        uint32_t v;
        uint8_t u[4];
    } tmp;

    tmp.v = x;

    return (tmp.u[3] << 24) | (tmp.u[2] << 16) | (tmp.u[1] << 8) | (tmp.u[0]);
}

static inline uint16_t BE16(uint16_t x)
{
    union {
        uint16_t v;
        uint8_t u[2];
    } tmp;

    tmp.v = x;

    return (tmp.u[0] << 8) | (tmp.u[1]);
}

static inline uint16_t LE16(uint16_t x)
{
    union {
        uint16_t v;
        uint8_t u[2];
    } tmp;

    tmp.v = x;

    return (tmp.u[1] << 8) | (tmp.u[0]);
}

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
const char *remove_path(const char *in);
size_t strlen(const char *c);
int strcmp(const char *s1, const char *s2);
void *memmove(void *dst, const void *src, size_t sz);
void *memcpy(void *dst, const void *src, size_t sz);
void *memset(void *ptr, int fill, size_t sz);
char *strstr(const char *str, const char *find);
void bzero(void *ptr, size_t sz);
void platform_init();
void platform_post_init();
void setup_serial();

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
