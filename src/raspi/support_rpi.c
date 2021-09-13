/*
    Copyright © 2019 Michal Schulz <michal.schulz@gmx.de>
    https://github.com/michalsc

    This Source Code Form is subject to the terms of the
    Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed
    with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <stdint.h>
#include <stdarg.h>

#include "support_rpi.h"
#include "mmu.h"
#include "tlsf.h"

#ifdef PISTORM
#include "ps_protocol.h"
#endif

#ifdef __aarch64__

#else

asm(
"       .globl __aeabi_uidiv                \n"
"       .type _start,%function              \n"
"__aeabi_uidiv:                             \n"
"       push {%lr}                          \n"
"       sub %sp, %sp, #8                    \n"
"       push {%r0, %r1}                     \n"
"       pop  {%r1, %r2}                     \n"
"       mov %r0, %sp                        \n"
"       bl uidiv                            \n"
"       pop {%r0}                           \n"
"       add %sp, %sp, #4                    \n"
"       pop {%pc}                           \n"


"       .globl __aeabi_uidivmod             \n"
"       .type _start,%function              \n"
"__aeabi_uidivmod:                          \n"
"       push {%lr}                          \n"
"       sub %sp, %sp, #8                    \n"
"       push {%r0, %r1}                     \n"
"       pop  {%r1, %r2}                     \n"
"       mov %r0, %sp                        \n"
"       bl uidiv                            \n"
"       pop {%r0}                           \n"
"       pop {%r1}                           \n"
"       pop {%pc}                           \n"
);

#endif

static int serial_up = 0;

#define ARM_PERIIOBASE ((intptr_t)io_base)

#ifdef PISTORM

uint8_t *q_buffer;
volatile uint64_t q_head;
volatile uint64_t q_tail;
#define Q_SIZE (8*1024*1024)

void q_push(uint8_t data)
{
    while(q_tail + Q_SIZE <= q_head)
        asm volatile("yield");
    
    q_buffer[q_head & (Q_SIZE - 1)] = data;
    __sync_add_and_fetch(&q_head, 1);
    asm volatile("sev");
}

uint8_t q_pop()
{
    while (q_tail == q_head) {
        asm volatile("wfe");
    }

    uint8_t data = q_buffer[q_tail & (Q_SIZE - 1)];
    __sync_add_and_fetch(&q_tail, 1);
    return data;
}

int redirect = 0;

static inline void putByte(void *io_base, char chr)
{
    (void)io_base;

    if (redirect)
    {
        if (chr == '\n')
            q_push('\r');
        q_push(chr);
    }
    else
    {
        if (chr == '\n')
            bitbang_putByte('\r');
        bitbang_putByte(chr);
    }
}

void serial_writer()
{
    redirect = 1;

    while(1) {
        bitbang_putByte(q_pop());
    }
}

#else

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

#endif

#undef ARM_PERIIOBASE
#define ARM_PERIIOBASE 0xf2000000

volatile unsigned char print_lock = 0;

void kprintf(const char * restrict format, ...)
{
    va_list v;
    va_start(v, format);

    while(__atomic_test_and_set(&print_lock, __ATOMIC_ACQUIRE)) asm volatile("yield");

    vkprintf_pc(putByte, (void*)ARM_PERIIOBASE, format, v);

    __atomic_clear(&print_lock, __ATOMIC_RELEASE);

    va_end(v);
}

void vkprintf(const char * restrict format, va_list args)
{
    while(__atomic_test_and_set(&print_lock, __ATOMIC_ACQUIRE)) asm volatile("yield");

    vkprintf_pc(putByte, (void*)ARM_PERIIOBASE, format, args);

    __atomic_clear(&print_lock, __ATOMIC_RELEASE);
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
	volatile uint32_t *mbox_read = (uint32_t*)(ARM_PERIIOBASE + 0xB880);
	volatile uint32_t *mbox_status = (uint32_t*)(ARM_PERIIOBASE + 0xB898);
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
	volatile uint32_t *mbox_write = (uint32_t*)(ARM_PERIIOBASE + 0xB8A0);
	volatile uint32_t *mbox_status = (uint32_t*)(ARM_PERIIOBASE + 0xB898);
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

//uint32_t FBReq[128] __attribute__((aligned(16)));
uint32_t *FBReq = (uint32_t *)0xffffff9000001000;

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

    arm_flush_cache((intptr_t)FBReq, 32);
    mbox_send(8, mmu_virt2phys((intptr_t)FBReq));
    mbox_recv(8);

    return LE32(FBReq[6]);
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

    arm_flush_cache((intptr_t)FBReq, 32);
    mbox_send(8, mmu_virt2phys((intptr_t)FBReq));
    mbox_recv(8);

    return LE32(FBReq[6]);
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

    arm_flush_cache((intptr_t)FBReq, 32);
    mbox_send(8, mmu_virt2phys((intptr_t)FBReq));
    mbox_recv(8);

    return LE32(FBReq[6]);
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
    FBReq[8] = 0;

    arm_flush_cache((intptr_t)FBReq, 36);
    mbox_send(8, mmu_virt2phys((intptr_t)FBReq));
    mbox_recv(8);

    return LE32(FBReq[6]);
}

void get_vc_memory(void **base, uint32_t *size)
{
    FBReq[0] = LE32(4*8);
    FBReq[1] = 0;
    FBReq[2] = LE32(0x00010006);
    FBReq[3] = LE32(8);
    FBReq[4] = 0;
    FBReq[5] = 0;
    FBReq[6] = 0;
    FBReq[7] = 0;

    arm_flush_cache((intptr_t)FBReq, 32);
    mbox_send(8, mmu_virt2phys((intptr_t)FBReq));
    mbox_recv(8);

    if (base) {
        *base = (void *)(intptr_t)LE32(FBReq[5]);
    }

    if (size) {
        *size = LE32(FBReq[6]);
    }
}

struct Size get_display_size()
{
    struct Size sz;

    FBReq[0] = LE32(4*8);
    FBReq[1] = 0;
    FBReq[2] = LE32(0x00040003);
    FBReq[3] = LE32(8);
    FBReq[4] = 0;
    FBReq[5] = 0;
    FBReq[6] = 0;
    FBReq[7] = 0;

    arm_flush_cache((intptr_t)FBReq, 32);
    mbox_send(8, mmu_virt2phys((intptr_t)FBReq));
    mbox_recv(8);

    sz.width = LE32(FBReq[5]);
    sz.height = LE32(FBReq[6]);

    return sz;
}

void init_display(struct Size dimensions, void **framebuffer, uint32_t *pitch)
{
    int c = 1;
    int pos_buffer_base = 0;
//    int pos_buffer_size = 0;
    int pos_buffer_pitch = 0;

    FBReq[c++] = 0;                 // Request
    FBReq[c++] = LE32(0x48003);     // SET_RESOLUTION
    FBReq[c++] = LE32(8);
    FBReq[c++] = 0;
    FBReq[c++] = LE32(dimensions.width);
    FBReq[c++] = LE32(dimensions.height);

    FBReq[c++] = LE32(0x48004);          // Virtual resolution: duplicate physical size...
    FBReq[c++] = LE32(8);
    FBReq[c++] = 0;
    FBReq[c++] = LE32(dimensions.width);
    FBReq[c++] = LE32(dimensions.height);

    FBReq[c++] = LE32(0x48005);   // Set depth
    FBReq[c++] = LE32(4);
    FBReq[c++] = LE32(0);
    FBReq[c++] = LE32(16);

    FBReq[c++] = LE32(0x40001); // Allocate buffer
    FBReq[c++] = LE32(8);
    FBReq[c++] = LE32(0);
    pos_buffer_base = c;
    FBReq[c++] = LE32(64);
//    pos_buffer_size = c;
    FBReq[c++] = LE32(0);

    FBReq[c++] = LE32(0x40008); // Get pitch
    FBReq[c++] = LE32(4);
    FBReq[c++] = LE32(0);
    pos_buffer_pitch = c;
    FBReq[c++] = LE32(0);

    FBReq[c++] = 0;

    FBReq[0] = LE32(c << 2);

    arm_flush_cache((intptr_t)FBReq, c * 4);
    mbox_send(8, mmu_virt2phys((intptr_t)FBReq));
    mbox_recv(8);

    uint32_t _base = LE32(FBReq[pos_buffer_base]);
    uint32_t _pitch = LE32(FBReq[pos_buffer_pitch]);

    if ((_base & 0xc0000000) == 0x40000000)
    {
        // Cached buffer
    }
    else if ((_base & 0xc0000000) == 0xc0000000)
    {
        // Uncached buffer
    }
    _base &= ~0xc0000000;

    if (framebuffer)
        *framebuffer = (void*)(intptr_t)_base;

    if (pitch)
        *pitch = _pitch;
}

#define PL011_ICR_FLAGS (PL011_ICR_RXIC|PL011_ICR_TXIC|PL011_ICR_RTIC|PL011_ICR_FEIC|PL011_ICR_PEIC|PL011_ICR_BEIC|PL011_ICR_OEIC|PL011_ICR_RIMIC|PL011_ICR_CTSMIC|PL011_ICR_DSRMIC|PL011_ICR_DCDMIC)

#define DEF_BAUD 115200

#define PL011_DIVCLOCK(baud, clock)     ((clock * 4) / baud)
#define PL011_BAUDINT(baud, clock)      ((PL011_DIVCLOCK(baud, clock) & 0xFFFFFFC0) >> 6)
#define PL011_BAUDFRAC(baud, clock)     ((PL011_DIVCLOCK(baud, clock) & 0x0000003F) >> 0)

#define GPIO_BASE                                       (ARM_PERIIOBASE + 0x200000)
#define GPFSEL1                                         (GPIO_BASE + 0x4)
#define GPPUD                                           (GPIO_BASE + 0x94)
#define GPPUDCLK0                                       (GPIO_BASE + 0x98)

#ifdef PISTORM

void setup_serial()
{
    serial_up = 1;

    q_buffer = tlsf_malloc(tlsf, Q_SIZE);
    q_head = 0;
    q_tail = 0;
}

#else

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

    for (uartvar = 0; uartvar < 150; uartvar++) asm volatile ("nop");

    wr32le(GPPUDCLK0, (1 << 14)|(1 << 15));

    for (uartvar = 0; uartvar < 150; uartvar++) asm volatile ("nop");

    wr32le(GPPUDCLK0, 0);

    wr32le(PL011_0_BASE + PL011_ICR, PL011_ICR_FLAGS);
    uartdivint = PL011_BAUDINT(uartbaud, uartclock);
    wr32le(PL011_0_BASE + PL011_IBRD, uartdivint);
    uartdivfrac = PL011_BAUDFRAC(uartbaud, uartclock);
    wr32le(PL011_0_BASE + PL011_FBRD, uartdivfrac);
    wr32le(PL011_0_BASE + PL011_LCRH, PL011_LCRH_WLEN8|PL011_LCRH_FEN);           // 8N1, Fifo enabled
    wr32le(PL011_0_BASE + PL011_CR, PL011_CR_UARTEN|PL011_CR_TXE|PL011_CR_RXE);   // enable the uart, tx and rx

    for (uartvar = 0; uartvar < 150; uartvar++) asm volatile ("nop");

    serial_up = 1;
}

#endif
