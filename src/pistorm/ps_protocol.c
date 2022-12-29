// SPDX-License-Identifier: MIT

/*
  Original Copyright 2020 Claude Schwarz
  Code reorganized and rewritten by
  Niklas Ekström 2021 (https://github.com/niklasekstrom)
*/

#define PS_PROTOCOL_IMPL

#include <stdint.h>

#include "config.h"
#include "support.h"
#include "tlsf.h"
#include "ps_protocol.h"
#include "M68k.h"

volatile unsigned int *gpio;
volatile unsigned int *gpclk;

unsigned int gpfsel0;
unsigned int gpfsel1;
unsigned int gpfsel2;

unsigned int gpfsel0_o;
unsigned int gpfsel1_o;
unsigned int gpfsel2_o;

#define BITBANG_DELAY PISTORM_BITBANG_DELAY

#define CHIPSET_DELAY PISTORM_CHIPSET_DELAY
#define CIA_DELAY     PISTORM_CIA_DELAY

volatile uint8_t gpio_lock;

static void usleep(uint64_t delta)
{
    uint64_t hi = LE32(*(volatile uint32_t*)0xf2003008);
    uint64_t lo = LE32(*(volatile uint32_t*)0xf2003004);
    uint64_t t1, t2;

    if (hi != LE32(*(volatile uint32_t*)0xf2003008))
    {
        hi = LE32(*(volatile uint32_t*)0xf2003008);
        lo = LE32(*(volatile uint32_t*)0xf2003004);
    }

    t1 = (hi << 32) | lo;
    t1 += delta;
    t2 = 0;

    do {
        hi = LE32(*(volatile uint32_t*)0xf2003008);
        lo = LE32(*(volatile uint32_t*)0xf2003004);
        if (hi != LE32(*(volatile uint32_t*)0xf2003008))
        {
            hi = LE32(*(volatile uint32_t*)0xf2003008);
            lo = LE32(*(volatile uint32_t*)0xf2003004);
        }
        t2 = (hi << 32) | lo;
    } while (t2 < t1);
}

static inline void ticksleep(uint64_t ticks)
{
    uint64_t t0 = 0, t1 = 0;
    asm volatile("mrs %0, CNTPCT_EL0":"=r"(t0));
    t0 += ticks;
    do {
        asm volatile("mrs %0, CNTPCT_EL0":"=r"(t1));
    } while(t1 < t0);
}

static inline void ticksleep_wfe(uint64_t ticks)
{
    uint64_t t0 = 0, t1 = 0;
    asm volatile("mrs %0, CNTPCT_EL0":"=r"(t0));
    t0 += ticks;
    do {
        asm volatile("mrs %0, CNTPCT_EL0":"=r"(t1));
        asm volatile("wfe");
    } while(t1 < t0);
}

#define TXD_BIT (1 << 26)

uint32_t bitbang_delay;
 
void bitbang_putByte(uint8_t byte)
{
    if (!gpio)
        gpio = ((volatile unsigned *)BCM2708_PERI_BASE) + GPIO_ADDR / 4;

    uint64_t t0 = 0, t1 = 0;
    asm volatile("mrs %0, CNTPCT_EL0":"=r"(t0));

    *(gpio + 10) = LE32(TXD_BIT); // Start bit - 0
  
    do {
        asm volatile("mrs %0, CNTPCT_EL0":"=r"(t1));
    } while(t1 < (t0 + bitbang_delay));
  
    for (int i=0; i < 8; i++) {
        asm volatile("mrs %0, CNTPCT_EL0":"=r"(t0));

        if (byte & 1)
            *(gpio + 7) = LE32(TXD_BIT);
        else
            *(gpio + 10) = LE32(TXD_BIT);
        byte = byte >> 1;

        do {
            asm volatile("mrs %0, CNTPCT_EL0":"=r"(t1));
        } while(t1 < (t0 + bitbang_delay));
    }
    asm volatile("mrs %0, CNTPCT_EL0":"=r"(t0));

    *(gpio + 7) = LE32(TXD_BIT);  // Stop bit - 1

    do {
        asm volatile("mrs %0, CNTPCT_EL0":"=r"(t1));
    } while(t1 < (t0 + 3*bitbang_delay / 2));
}

#define FS_CLK  (1 << 26)
#define FS_DO   (1 << 27)
#define FS_CTS  (1 << 25)

void (*fs_putByte)(uint8_t);

void fastSerial_putByte_pi3(uint8_t byte)
{
    if (!gpio)
        gpio = ((volatile unsigned *)BCM2708_PERI_BASE) + GPIO_ADDR / 4;
  
    /* Wait for CTS to go high */
    //while (0 == (*(gpio + 13) & LE32(FS_CTS))) {}

    /* Start bit */
    *(gpio + 10) = LE32(FS_DO);

    /* Clock down */
    *(gpio + 10) = LE32(FS_CLK);
    //*(gpio + 10) = LE32(FS_CLK);
    /* Clock up */
    *(gpio + 7) = LE32(FS_CLK);
    //*(gpio + 7) = LE32(FS_CLK);

    for (int i=0; i < 8; i++) {
        if (byte & 1)
            *(gpio + 7) = LE32(FS_DO);
        else
            *(gpio + 10) = LE32(FS_DO);

        /* Clock down */
        *(gpio + 10) = LE32(FS_CLK);
        //*(gpio + 10) = LE32(FS_CLK);
        /* Clock up */
        *(gpio + 7) = LE32(FS_CLK);
        //*(gpio + 7) = LE32(FS_CLK);
        
        byte = byte >> 1;
    }

    /* DEST bit (0) */
    *(gpio + 10) = LE32(FS_DO);

    /* Clock down */
    *(gpio + 10) = LE32(FS_CLK);
    //*(gpio + 10) = LE32(FS_CLK);
    /* Clock up */
    *(gpio + 7) = LE32(FS_CLK);
    *(gpio + 7) = LE32(FS_CLK);

    /* Leave FS_CLK and FS_DO high */
    *(gpio + 7) = LE32(FS_CLK);
    *(gpio + 7) = LE32(FS_DO);
}

void fastSerial_putByte_pi4(uint8_t byte)
{
    if (!gpio)
        gpio = ((volatile unsigned *)BCM2708_PERI_BASE) + GPIO_ADDR / 4;
  
    /* Start bit */
    *(gpio + 10) = LE32(FS_DO);

    /* Clock down */
    *(gpio + 10) = LE32(FS_CLK);
    *(gpio + 10) = LE32(FS_CLK);
    /* Clock up */
    *(gpio + 7) = LE32(FS_CLK);
    *(gpio + 7) = LE32(FS_CLK);

    for (int i=0; i < 8; i++) {
        if (byte & 1)
            *(gpio + 7) = LE32(FS_DO);
        else
            *(gpio + 10) = LE32(FS_DO);

        /* Clock down */
        *(gpio + 10) = LE32(FS_CLK);
        *(gpio + 10) = LE32(FS_CLK);
        /* Clock up */
        *(gpio + 7) = LE32(FS_CLK);
        *(gpio + 7) = LE32(FS_CLK);
        
        byte = byte >> 1;
    }

    /* DEST bit (0) */
    *(gpio + 10) = LE32(FS_DO);

    /* Clock down */
    *(gpio + 10) = LE32(FS_CLK);
    *(gpio + 10) = LE32(FS_CLK);
    /* Clock up */
    *(gpio + 7) = LE32(FS_CLK);
    *(gpio + 7) = LE32(FS_CLK);

    /* Leave FS_CLK and FS_DO high */
    *(gpio + 7) = LE32(FS_CLK);
    *(gpio + 7) = LE32(FS_DO);
}

void fastSerial_reset()
{
    if (!gpio)
        gpio = ((volatile unsigned *)BCM2708_PERI_BASE) + GPIO_ADDR / 4;

    /* Leave FS_CLK and FS_DO high */
    *(gpio + 7) = LE32(FS_CLK);
    *(gpio + 7) = LE32(FS_DO);

    for (int i=0; i < 16; i++) {
        /* Clock down */
        *(gpio + 10) = LE32(FS_CLK);
        *(gpio + 10) = LE32(FS_CLK);
        /* Clock up */
        *(gpio + 7) = LE32(FS_CLK);
        *(gpio + 7) = LE32(FS_CLK);
    }
}

void fastSerial_putByte(uint8_t byte)
{
    static char reset_pending = 0;

    if (reset_pending)
    {
        fastSerial_reset();
        reset_pending = 0;
    }

    if (!gpio)
        gpio = ((volatile unsigned *)BCM2708_PERI_BASE) + GPIO_ADDR / 4;
  
    if (fs_putByte)
        fs_putByte(byte);
    
    if (byte == 10)
        reset_pending = 1;
}

void fastSerial_init()
{
    uint64_t tmp;

    if (!gpio)
        gpio = ((volatile unsigned *)BCM2708_PERI_BASE) + GPIO_ADDR / 4;
  
    asm volatile("mrs %0, CNTFRQ_EL0":"=r"(tmp));

    if (tmp > 20000000)
    {
        fs_putByte = fastSerial_putByte_pi4;
    }
    else
    {
        fs_putByte = fastSerial_putByte_pi3;
    }   

    fastSerial_reset();
}


static void pistorm_setup_io() {
    gpio = ((volatile unsigned *)BCM2708_PERI_BASE) + GPIO_ADDR / 4;
    gpclk = ((volatile unsigned *)BCM2708_PERI_BASE) + GPCLK_ADDR / 4;
}

static void setup_gpclk() {
    // Enable 200MHz CLK output on GPIO4, adjust divider and pll source depending
    // on pi model
    uint64_t tmp;
    asm volatile("mrs %0, CNTFRQ_EL0":"=r"(tmp));

    *(gpclk + (CLK_GP0_CTL / 4)) = LE32(CLK_PASSWD | (1 << 5));
    usleep(10);
    while ((*(gpclk + (CLK_GP0_CTL / 4))) & LE32(1 << 7));
    usleep(100);
    if (tmp > 20000000)
        *(gpclk + (CLK_GP0_DIV / 4)) =
            LE32(CLK_PASSWD | (5 << 12));  // divider , 3=200MHz on pi4
    else
        *(gpclk + (CLK_GP0_DIV / 4)) =
            LE32(CLK_PASSWD | (6 << 12));  // divider , 6=200MHz on pi3
    usleep(10);
    *(gpclk + (CLK_GP0_CTL / 4)) =
        LE32(CLK_PASSWD | 5 | (1 << 4));  // pll? 6=plld, 5=pllc
    usleep(10);
    while (((*(gpclk + (CLK_GP0_CTL / 4))) & LE32(1 << 7)) == 0);
    usleep(100);

    SET_GPIO_ALT(PIN_CLK, 0);  // gpclk0
}

void ps_setup_protocol() {
    uint64_t clock;
    uint64_t delay;

    /* Setup bitbang RS232 delay based on the RS232 speed and CPU tick frequency */
    asm volatile("mrs %0, CNTFRQ_EL0":"=r"(clock));
    delay = (clock + PISTORM_BITBANG_SPEED / 2) / PISTORM_BITBANG_SPEED;
    bitbang_delay = delay;

    pistorm_setup_io();
    setup_gpclk();

    *(gpio + 10) = LE32(0xffffec);

    *(gpio + 0) = LE32(GPFSEL0_INPUT);
    *(gpio + 1) = LE32(GPFSEL1_INPUT);
    *(gpio + 2) = LE32(GPFSEL2_INPUT);

    *(gpio + 7) = LE32(TXD_BIT);
}

static void ps_write_8_int(unsigned int address, unsigned int data);

static void ps_write_16_int(unsigned int address, unsigned int data)
{
    uint64_t tmp;
    asm volatile("mrs %0, CNTFRQ_EL0":"=r"(tmp));

//    if (address > 0xffffff)
//        return;

    address &= 0xffffff;

    if (address & 1)
    {
        ps_write_8_int(address, data >> 8);
        ps_write_8_int(address + 1, data & 0xff);
    }
    else
    {
        *(gpio + 0) = LE32(GPFSEL0_OUTPUT);
        *(gpio + 1) = LE32(GPFSEL1_OUTPUT);
        *(gpio + 2) = LE32(GPFSEL2_OUTPUT);

        *(gpio + 7) = LE32(((data & 0xffff) << 8) | (REG_DATA << PIN_A0));
        if (tmp > 20000000)
        {
            *(gpio + 7) = LE32(1 << PIN_WR);
            *(gpio + 7) = LE32(1 << PIN_WR);
            *(gpio + 7) = LE32(1 << PIN_WR);
            *(gpio + 10) = LE32(1 << PIN_WR);
            *(gpio + 10) = LE32(1 << PIN_WR);
        }
        else
        {
            *(gpio + 7) = LE32(1 << PIN_WR);
            *(gpio + 10) = LE32(1 << PIN_WR);
        }
        *(gpio + 10) = LE32(0xffffec);

        *(gpio + 7) = LE32(((address & 0xffff) << 8) | (REG_ADDR_LO << PIN_A0));
        if (tmp > 20000000)
        {
            *(gpio + 7) = LE32(1 << PIN_WR);
            *(gpio + 7) = LE32(1 << PIN_WR);
            *(gpio + 7) = LE32(1 << PIN_WR);
            *(gpio + 10) = LE32(1 << PIN_WR);
            *(gpio + 10) = LE32(1 << PIN_WR);
        }
        else
        {
            *(gpio + 7) = LE32(1 << PIN_WR);
            *(gpio + 10) = LE32(1 << PIN_WR);
        }
        *(gpio + 10) = LE32(0xffffec);

        *(gpio + 7) = LE32(((0x0000 | ((address >> 16) & 0x00ff)) << 8) | (REG_ADDR_HI << PIN_A0));
        if (tmp > 20000000)
        {
            *(gpio + 7) = LE32(1 << PIN_WR);
            *(gpio + 7) = LE32(1 << PIN_WR);
            *(gpio + 7) = LE32(1 << PIN_WR);
            *(gpio + 10) = LE32(1 << PIN_WR);
            *(gpio + 10) = LE32(1 << PIN_WR);
        }
        else
        {
            *(gpio + 7) = LE32(1 << PIN_WR);
            *(gpio + 10) = LE32(1 << PIN_WR);
        }
        *(gpio + 10) = LE32(0xffffec);

        *(gpio + 0) = LE32(GPFSEL0_INPUT);
        *(gpio + 1) = LE32(GPFSEL1_INPUT);
        *(gpio + 2) = LE32(GPFSEL2_INPUT);

        while (*(gpio + 13) & LE32((1 << PIN_TXN_IN_PROGRESS))) {}
    }
}

static void ps_write_8_int(unsigned int address, unsigned int data)
{
    uint64_t tmp;
    asm volatile("mrs %0, CNTFRQ_EL0":"=r"(tmp));

//    if (address > 0xffffff)
//        return;

    address &= 0xffffff;

    if ((address & 1) == 0)
        data = data << 8; // (data & 0xff) | (data << 8);  // EVEN, A0=0,UDS
    else
        data = data & 0xff;  // ODD , A0=1,LDS

    *(gpio + 0) = LE32(GPFSEL0_OUTPUT);
    *(gpio + 1) = LE32(GPFSEL1_OUTPUT);
    *(gpio + 2) = LE32(GPFSEL2_OUTPUT);

    *(gpio + 7) = LE32(((data & 0xffff) << 8) | (REG_DATA << PIN_A0));
    if (tmp > 20000000)
    {
        *(gpio + 7) = LE32(1 << PIN_WR);
        *(gpio + 7) = LE32(1 << PIN_WR);
        *(gpio + 7) = LE32(1 << PIN_WR);
        *(gpio + 10) = LE32(1 << PIN_WR);
        *(gpio + 10) = LE32(1 << PIN_WR);
    }
    else
    {
        *(gpio + 7) = LE32(1 << PIN_WR);
        *(gpio + 10) = LE32(1 << PIN_WR);
    }
    *(gpio + 10) = LE32(0xffffec);

    *(gpio + 7) = LE32(((address & 0xffff) << 8) | (REG_ADDR_LO << PIN_A0));
    if (tmp > 20000000)
    {
        *(gpio + 7) = LE32(1 << PIN_WR);
        *(gpio + 7) = LE32(1 << PIN_WR);
        *(gpio + 7) = LE32(1 << PIN_WR);
        *(gpio + 10) = LE32(1 << PIN_WR);
        *(gpio + 10) = LE32(1 << PIN_WR);
    }
    else
    {
        *(gpio + 7) = LE32(1 << PIN_WR);
        *(gpio + 10) = LE32(1 << PIN_WR);
    }
    *(gpio + 10) = LE32(0xffffec);

    *(gpio + 7) = LE32(((0x0100 | ((address >> 16) & 0x00ff)) << 8) | (REG_ADDR_HI << PIN_A0));
    if (tmp > 20000000)
    {
        *(gpio + 7) = LE32(1 << PIN_WR);
        *(gpio + 7) = LE32(1 << PIN_WR);
        *(gpio + 7) = LE32(1 << PIN_WR);
        *(gpio + 10) = LE32(1 << PIN_WR);
        *(gpio + 10) = LE32(1 << PIN_WR);
    }
    else
    {
        *(gpio + 7) = LE32(1 << PIN_WR);
        *(gpio + 10) = LE32(1 << PIN_WR);
    }
    *(gpio + 10) = LE32(0xffffec);

    *(gpio + 0) = LE32(GPFSEL0_INPUT);
    *(gpio + 1) = LE32(GPFSEL1_INPUT);
    *(gpio + 2) = LE32(GPFSEL2_INPUT);

    while (*(gpio + 13) & LE32((1 << PIN_TXN_IN_PROGRESS))) {}
}

static void ps_write_32_int(unsigned int address, unsigned int value)
{
    if (address & 1)
    {
        ps_write_8_int(address, value >> 24);
        ps_write_16_int(address + 1, value >> 8);
        ps_write_8_int(address + 3, value & 0xff);
    }
    else
    {
        ps_write_16_int(address, value >> 16);
        ps_write_16_int(address + 2, value);
    }
}

unsigned int ps_read_16_int(unsigned int address)
{
    uint64_t tmp;
    asm volatile("mrs %0, CNTFRQ_EL0":"=r"(tmp));

#if PISTORM_WRITE_BUFFER
    wb_waitfree();
#endif

address &= 0xffffff;

//    if (address > 0xffffff)
//        return 0xffff;

    if (address & 1)
    {
        unsigned int value;

        value = ps_read_8(address) << 8;
        value |= ps_read_8(address + 1);

        return value;
    }
    else
    {
        *(gpio + 0) = LE32(GPFSEL0_OUTPUT);
        *(gpio + 1) = LE32(GPFSEL1_OUTPUT);
        *(gpio + 2) = LE32(GPFSEL2_OUTPUT);

        *(gpio + 7) = LE32(((address & 0xffff) << 8) | (REG_ADDR_LO << PIN_A0));
        if (tmp > 20000000)
        {
            *(gpio + 7) = LE32(1 << PIN_WR);
            *(gpio + 7) = LE32(1 << PIN_WR);
            *(gpio + 7) = LE32(1 << PIN_WR);
            *(gpio + 10) = LE32(1 << PIN_WR);
            *(gpio + 10) = LE32(1 << PIN_WR);
        }
        else
        {
            *(gpio + 7) = LE32(1 << PIN_WR);
            *(gpio + 10) = LE32(1 << PIN_WR);
        }
        *(gpio + 10) = LE32(0xffffec);

        *(gpio + 7) = LE32(((0x0200 | ((address >> 16) & 0x00ff)) << 8) | (REG_ADDR_HI << PIN_A0));
        if (tmp > 20000000)
        {
            *(gpio + 7) = LE32(1 << PIN_WR);
            *(gpio + 7) = LE32(1 << PIN_WR);
            *(gpio + 7) = LE32(1 << PIN_WR);
            *(gpio + 10) = LE32(1 << PIN_WR);
            *(gpio + 10) = LE32(1 << PIN_WR);
        }
        else
        {
            *(gpio + 7) = LE32(1 << PIN_WR);
            *(gpio + 10) = LE32(1 << PIN_WR);
        }
        *(gpio + 10) = LE32(0xffffec);

        *(gpio + 0) = LE32(GPFSEL0_INPUT);
        *(gpio + 1) = LE32(GPFSEL1_INPUT);
        *(gpio + 2) = LE32(GPFSEL2_INPUT);

        *(gpio + 7) = LE32(REG_DATA << PIN_A0);
        *(gpio + 7) = LE32(1 << PIN_RD);
        if (tmp > 20000000)
        {
            *(gpio + 7) = LE32(1 << PIN_RD);
            *(gpio + 7) = LE32(1 << PIN_RD);
            *(gpio + 7) = LE32(1 << PIN_RD);
        }

        while (*(gpio + 13) & LE32(1 << PIN_TXN_IN_PROGRESS)) {}
        unsigned int value = LE32(*(gpio + 13));

        *(gpio + 10) = LE32(0xffffec);
        return (value >> 8) & 0xffff;
    }
}

unsigned int ps_read_8_int(unsigned int address)
{
    uint64_t tmp;
    asm volatile("mrs %0, CNTFRQ_EL0":"=r"(tmp));

#if PISTORM_WRITE_BUFFER
    wb_waitfree();
#endif

    address &= 0xffffff;

//    if (address > 0xffffff)
//        return 0xff;

    *(gpio + 0) = LE32(GPFSEL0_OUTPUT);
    *(gpio + 1) = LE32(GPFSEL1_OUTPUT);
    *(gpio + 2) = LE32(GPFSEL2_OUTPUT);

    *(gpio + 7) = LE32(((address & 0xffff) << 8) | (REG_ADDR_LO << PIN_A0));
    if (tmp > 20000000)
    {
        *(gpio + 7) = LE32(1 << PIN_WR);
        *(gpio + 7) = LE32(1 << PIN_WR);
        *(gpio + 7) = LE32(1 << PIN_WR);
        *(gpio + 10) = LE32(1 << PIN_WR);
        *(gpio + 10) = LE32(1 << PIN_WR);
    }
    else
    {
        *(gpio + 7) = LE32(1 << PIN_WR);
        *(gpio + 10) = LE32(1 << PIN_WR);
    }
    *(gpio + 10) = LE32(0xffffec);

    *(gpio + 7) = LE32(((0x0300 | ((address >> 16) & 0x00ff)) << 8) | (REG_ADDR_HI << PIN_A0));
    if (tmp > 20000000)
    {
        *(gpio + 7) = LE32(1 << PIN_WR);
        *(gpio + 7) = LE32(1 << PIN_WR);
        *(gpio + 7) = LE32(1 << PIN_WR);
        *(gpio + 10) = LE32(1 << PIN_WR);
        *(gpio + 10) = LE32(1 << PIN_WR);
    }
    else
    {
        *(gpio + 7) = LE32(1 << PIN_WR);
        *(gpio + 10) = LE32(1 << PIN_WR);
    }
    *(gpio + 10) = LE32(0xffffec);

    *(gpio + 0) = LE32(GPFSEL0_INPUT);
    *(gpio + 1) = LE32(GPFSEL1_INPUT);
    *(gpio + 2) = LE32(GPFSEL2_INPUT);

    *(gpio + 7) = LE32(REG_DATA << PIN_A0);
    *(gpio + 7) = LE32(1 << PIN_RD);
    if (tmp > 20000000)
    {
        *(gpio + 7) = LE32(1 << PIN_RD);
        *(gpio + 7) = LE32(1 << PIN_RD);
        *(gpio + 7) = LE32(1 << PIN_RD);
    }

    while (*(gpio + 13) & LE32(1 << PIN_TXN_IN_PROGRESS)) {}
    unsigned int value = LE32(*(gpio + 13));

    *(gpio + 10) = LE32(0xffffec);

    value = (value >> 8) & 0xffff;

    if ((address & 1) == 0)
        return (value >> 8) & 0xff;  // EVEN, A0=0,UDS
    else
        return value & 0xff;  // ODD , A0=1,LDS
}

unsigned int ps_read_32_int(unsigned int address)
{
#if PISTORM_WRITE_BUFFER
    wb_waitfree();
#endif

    if (address & 1)
    {
        unsigned int value;
        value = ps_read_8(address) << 24;
        value |= ps_read_16(address + 1) << 8;
        value |= ps_read_8(address + 3);
        return value;
    }
    else
    {
        unsigned int a = ps_read_16(address);
        unsigned int b = ps_read_16(address + 2);
        return (a << 16) | b;
    }
}

void ps_write_status_reg(unsigned int value)
{
    uint64_t tmp;
    asm volatile("mrs %0, CNTFRQ_EL0":"=r"(tmp));

    *(gpio + 0) = LE32(GPFSEL0_OUTPUT);
    *(gpio + 1) = LE32(GPFSEL1_OUTPUT);
    *(gpio + 2) = LE32(GPFSEL2_OUTPUT);

    *(gpio + 7) = LE32(((value & 0xffff) << 8) | (REG_STATUS << PIN_A0));

    *(gpio + 7) = LE32(1 << PIN_WR);
    *(gpio + 7) = LE32(1 << PIN_WR);  // delay
    if (tmp > 20000000)
    {
        *(gpio + 7) = LE32(1 << PIN_WR);
        *(gpio + 7) = LE32(1 << PIN_WR);
    }
    *(gpio + 10) = LE32(1 << PIN_WR);
    if (tmp > 20000000)
    {
        *(gpio + 10) = LE32(1 << PIN_WR);
        *(gpio + 10) = LE32(1 << PIN_WR);
    }
    *(gpio + 10) = LE32(0xffffec);

    *(gpio + 0) = LE32(GPFSEL0_INPUT);
    *(gpio + 1) = LE32(GPFSEL1_INPUT);
    *(gpio + 2) = LE32(GPFSEL2_INPUT);
}

unsigned int ps_read_status_reg()
{
    uint64_t tmp;
    asm volatile("mrs %0, CNTFRQ_EL0":"=r"(tmp));

    *(gpio + 7) = LE32(REG_STATUS << PIN_A0);
    *(gpio + 7) = LE32(1 << PIN_RD);
    *(gpio + 7) = LE32(1 << PIN_RD);
    *(gpio + 7) = LE32(1 << PIN_RD);
    *(gpio + 7) = LE32(1 << PIN_RD);
    if (tmp > 20000000)
    {
        *(gpio + 7) = LE32(1 << PIN_RD);
        *(gpio + 7) = LE32(1 << PIN_RD);
        *(gpio + 7) = LE32(1 << PIN_RD);
        *(gpio + 7) = LE32(1 << PIN_RD);
    }
    unsigned int value = LE32(*(gpio + 13));

    *(gpio + 10) = LE32(0xffffec);

    return (value >> 8) & 0xffff;
}

void ps_reset_state_machine()
{
    ps_write_status_reg(STATUS_BIT_INIT);
    usleep(1500);
    ps_write_status_reg(0);
    usleep(100);
}

#include <boards.h>
extern struct ExpansionBoard **board;
extern struct ExpansionBoard *__boards_start;
extern int board_idx;
extern uint32_t overlay;

void ps_pulse_reset()
{
    ps_write_status_reg(0);
    usleep(30000);
    ps_write_status_reg(STATUS_BIT_RESET);
    
    overlay = 1;
    board = &__boards_start;
    board_idx = 0;
}

unsigned int ps_get_ipl_zero()
{
    unsigned int value = (*(gpio + 13));
    return value & LE32(1 << PIN_IPL_ZERO);
}

#define INT2_ENABLED 1

#define PM_RSTC         ((volatile unsigned int*)(0xf2000000 + 0x0010001c))
#define PM_RSTS         ((volatile unsigned int*)(0xf2000000 + 0x00100020))
#define PM_WDOG         ((volatile unsigned int*)(0xf2000000 + 0x00100024))
#define PM_WDOG_MAGIC   0x5a000000
#define PM_RSTC_FULLRST 0x00000020

volatile int housekeeper_enabled = 0;
extern struct M68KState *__m68k_state;

void ps_housekeeper() 
{
    if (!gpio)
        gpio = ((volatile unsigned *)BCM2708_PERI_BASE) + GPIO_ADDR / 4;
  
    extern uint64_t arm_cnt;
    uint64_t t0;
    uint64_t last_arm_cnt = arm_cnt;

    asm volatile("mrs %0, CNTPCT_EL0":"=r"(t0));
    asm volatile("mrs %0, PMCCNTR_EL0":"=r"(last_arm_cnt));

    kprintf("[HKEEP] Housekeeper activated\n");
    kprintf("[HKEEP] Please note we are burning the cpu with busyloops now\n");

    /* Configure timer-based event stream */
    /* Enable timer regs from EL0, enable event stream on posedge, monitor 2th bit */
    /* This gives a frequency of 2.4MHz for a 19.2MHz timer */
    asm volatile("msr CNTKCTL_EL1, %0"::"r"(3 | (1 << 2) | (3 << 8) | (2 << 4)));

    for(;;) {
        if (housekeeper_enabled)
        {
            uint32_t pin = LE32(*(gpio + 13));
            __m68k_state->INT.IPL = (pin & (1 << PIN_IPL_ZERO)) ? 0 : 1;

            asm volatile("":::"memory");

            if (__m68k_state->INT.IPL)
                asm volatile("sev":::"memory");

            if ((pin & (1 << PIN_RESET)) == 0) {
                kprintf("[HKEEP] Houskeeper will reset RasPi now...\n");

                unsigned int r;
                // trigger a restart by instructing the GPU to boot from partition 0
                r = LE32(*PM_RSTS); r &= ~0xfffffaaa;
                *PM_RSTS = LE32(PM_WDOG_MAGIC | r);   // boot from partition 0
                *PM_WDOG = LE32(PM_WDOG_MAGIC | 10);
                *PM_RSTC = LE32(PM_WDOG_MAGIC | PM_RSTC_FULLRST);

                while(1);
            }

            /*
              Wait for event. It can happen that the CPU is flooded with them for some reason, but
              nevertheless, thanks for the event stream set up above, they will appear at 1.2MHz in worst case
            */
            asm volatile("wfe");
        }
    }
}

#if PISTORM_WRITE_BUFFER

#define WRITEBUFFER_SIZE  PISTORM_WRITE_BUFFER_SIZE

struct WriteRequest {
    uint32_t  wr_addr;
    uint32_t  wr_value;
    uint8_t   wr_size;
};

struct WriteRequest *wr_buffer;
volatile uint32_t wr_head;
volatile uint32_t wr_tail;
volatile unsigned char bus_lock = 0;

void wb_push(uint32_t address, uint32_t value, uint8_t size)
{
    while(wr_tail + WRITEBUFFER_SIZE <= wr_head)
        asm volatile("yield");
    
    wr_buffer[wr_head & (WRITEBUFFER_SIZE - 1)].wr_addr = address;
    wr_buffer[wr_head & (WRITEBUFFER_SIZE - 1)].wr_value = value;
    wr_buffer[wr_head & (WRITEBUFFER_SIZE - 1)].wr_size = size;

    asm volatile("dmb sy":::"memory");

    __sync_add_and_fetch(&wr_head, 1);
    
    asm volatile("sev");
}

struct WriteRequest wb_pop()
{
    while (wr_tail == wr_head) {
        asm volatile("wfe");
    }

    struct WriteRequest data = wr_buffer[wr_tail & (WRITEBUFFER_SIZE - 1)];

    __sync_add_and_fetch(&wr_tail, 1);
    
    return data;
}

struct WriteRequest wb_peek()
{
    while (wr_tail == wr_head) {
        asm volatile("wfe");
    }

    struct WriteRequest data = wr_buffer[wr_tail & (WRITEBUFFER_SIZE - 1)];

    return data;
}

void wb_wait()
{
    while (wr_tail == wr_head) {
        asm volatile("wfe");
    }
}

void wb_waitfree()
{
    while (wr_tail != wr_head)
        asm volatile("yield");
}
#endif

void wb_init()
{
#if PISTORM_WRITE_BUFFER
    wr_buffer = tlsf_malloc(tlsf, sizeof(struct WriteRequest) * WRITEBUFFER_SIZE);
    wr_head = wr_tail = 0;
    bus_lock = 0;
#endif
}

void wb_task()
{
#if PISTORM_WRITE_BUFFER
    kprintf("[WBACK] Write buffer activated\n");

    while(1) {
        struct WriteRequest req = wb_peek();

        while(__atomic_test_and_set(&bus_lock, __ATOMIC_ACQUIRE)) { asm volatile("yield"); }

        switch (req.wr_size) {
            case 1:
                ps_write_8_int(req.wr_addr, req.wr_value);
                break;
            case 2:
                ps_write_16_int(req.wr_addr, req.wr_value);
                break;
            case 4:
                ps_write_32_int(req.wr_addr, req.wr_value);
                break;
        }
#if CIA_DELAY
        if (req.wr_addr >= 0xbf0000 && req.wr_addr <= 0xbfffff) {
            ticksleep(CIA_DELAY);
        }
#endif
#if CHIPSET_DELAY
        if (req.wr_addr >= 0xa00000) {
            ticksleep(CHIPSET_DELAY);
        }
#endif
        __atomic_clear(&bus_lock, __ATOMIC_RELEASE);

        wb_pop();
    }
#else
    while(1) asm volatile("wfi");
#endif
}

void ps_write_8(unsigned int address, unsigned int data)
{
#if PISTORM_WRITE_BUFFER
    if (address < 0xa00000)
    {
        wb_push(address, data, 1);
    }
    else {
        wb_push(address, data, 1);
        wb_waitfree();
    }
#else
    ps_write_8_int(address, data);
#if CIA_DELAY
    if (address >= 0xbf0000 && address <= 0xbfffff) {
        ticksleep(CIA_DELAY);
    }
#endif
#if CHIPSET_DELAY
    if (address >= 0xa00000) {
        ticksleep(CHIPSET_DELAY);
    }
#endif
#endif
}

void ps_write_16(unsigned int address, unsigned int data)
{
#if PISTORM_WRITE_BUFFER
    if (address < 0xa00000)
    {
        wb_push(address, data, 2);
    }
    else {
        wb_push(address, data, 2);
        wb_waitfree();
    }
#else
    ps_write_16_int(address, data);
#if CIA_DELAY
    if (address >= 0xbf0000 && address <= 0xbfffff) {
        ticksleep(CIA_DELAY);
    }
#endif
#if CHIPSET_DELAY
    if (address >= 0xa00000) {
        ticksleep(CHIPSET_DELAY);
    }
#endif
#endif
}

void ps_write_32(unsigned int address, unsigned int data)
{
#if PISTORM_WRITE_BUFFER
    if (address < 0xa00000)
    {
        wb_push(address, data, 4);
    }
    else {
        wb_push(address, data, 4);
        wb_waitfree();
    }
#else
    ps_write_32_int(address, data);
#if CIA_DELAY
    if (address >= 0xbf0000 && address <= 0xbfffff) {
        ticksleep(CIA_DELAY);
    }
#endif
#if CHIPSET_DELAY
    if (address >= 0xa00000) {
        ticksleep(CHIPSET_DELAY);
    }
#endif
#endif
}

unsigned int ps_read_8(unsigned int address)
{
    int val = ps_read_8_int(address);

#if CIA_DELAY
    if (address >= 0xbf0000 && address <= 0xbfffff) {
        ticksleep(CIA_DELAY);
    }
#endif
#if CHIPSET_DELAY
    if (address >= 0xa00000) {
        ticksleep(CHIPSET_DELAY);
    }
#endif
    return val;  
}

unsigned int ps_read_16(unsigned int address)
{
    int val = ps_read_16_int(address);
#if CIA_DELAY
    if (address >= 0xbf0000 && address <= 0xbfffff) {
        ticksleep(CIA_DELAY);
    }
#endif
#if CHIPSET_DELAY
    if (address >= 0xa00000) {
        ticksleep(CHIPSET_DELAY);
    }
#endif
    return val;
}

unsigned int ps_read_32(unsigned int address)
{
    int val = ps_read_32_int(address);
#if CIA_DELAY
    if (address >= 0xbf0000 && address <= 0xbfffff) {
        ticksleep(CIA_DELAY);
    }
#endif
#if CHIPSET_DELAY
    if (address >= 0xa00000) {
        ticksleep(CHIPSET_DELAY);
    }
#endif
    return val;
}

void put_char(uint8_t c);

static void __putc(void *data, char c)
{
    (void)data;
    put_char(c);
}

static uint32_t _seed;
uint32_t rnd() {
    _seed = (_seed * 1103515245) + 12345;
    return _seed;
}

/* BupTest by beeanyew, ported to Emu68 */

void ps_buptest(unsigned int test_size, unsigned int maxiter)
{
    // Initialize RNG
    uint64_t tmp;
    asm volatile("mrs %0, CNTPCT_EL0":"=r"(tmp));

    _seed = tmp;

    kprintf_pc(__putc, NULL, "BUPTest with size %dK requested through commandline\n", test_size);

    test_size *= 1024;
    uint32_t frac = test_size / 16;

    uint8_t *garbage = tlsf_malloc(tlsf, test_size);

    ps_write_8(0xbfe201, 0x0101);       //CIA OVL
    ps_write_8(0xbfe001, 0x0000);       //CIA OVL LOW

    for (unsigned int iter = 0; iter < maxiter; iter++) {
        kprintf_pc(__putc, NULL, "Iteration %d...\n", iter + 1);

        // Fill the garbage buffer and chip ram with random data
        kprintf_pc(__putc, NULL, "  Writing BYTE garbage data to Chip...            ");
        for (uint32_t i = 0; i < test_size; i++) {
            uint8_t val = 0;
            val = rnd();
            garbage[i] = val;
            ps_write_8(i, val);

            if ((i % (frac * 2)) == 0)
                kprintf_pc(__putc, NULL, "*");
        }

        for (uint32_t i = 0; i < test_size; i++) {
            uint32_t c = ps_read_8(i);
            if (c != garbage[i]) {
                kprintf_pc(__putc, NULL, "\n    READ8: Garbege data mismatch at $%.6X: %.2X should be %.2X.\n", i, c, garbage[i]);
                while(1);
            }

            if ((i % (frac * 4)) == 0)
                kprintf_pc(__putc, NULL, "*");
        }

        for (uint32_t i = 0; i < (test_size) - 2; i += 2) {
            uint32_t c = BE16(ps_read_16(i));
            if (c != *((uint16_t *)&garbage[i])) {
                kprintf_pc(__putc, NULL, "\n    READ16_EVEN: Garbege data mismatch at $%.6X: %.4X should be %.4X.\n", i, c, *((uint16_t *)&garbage[i]));
                while(1);
            }

            if ((i % (frac * 4)) == 0)
                kprintf_pc(__putc, NULL, "*");
        }

        for (uint32_t i = 1; i < (test_size) - 2; i += 2) {
            uint32_t c = BE16((ps_read_8(i) << 8) | ps_read_8(i + 1));
            if (c != *((uint16_t *)&garbage[i])) {
                kprintf_pc(__putc, NULL, "\n    READ16_ODD: Garbege data mismatch at $%.6X: %.4X should be %.4X.\n", i, c, *((uint16_t *)&garbage[i]));
                while(1);
            }

            if ((i % (frac * 4)) == 1)
                kprintf_pc(__putc, NULL, "*");
        }
        
        for (uint32_t i = 0; i < (test_size) - 4; i += 2) {
            uint32_t c = BE32(ps_read_32(i));
            if (c != *((uint32_t *)&garbage[i])) {
                kprintf_pc(__putc, NULL, "\n    READ32_EVEN: Garbege data mismatch at $%.6X: %.8X should be %.8X.\n", i, c, *((uint32_t *)&garbage[i]));
                while(1);
            }
            
            if ((i % (frac * 4)) == 0)
                kprintf_pc(__putc, NULL, "*");
        }

        for (uint32_t i = 1; i < (test_size) - 4; i += 2) {
            uint32_t c = ps_read_8(i) << 24;
            c |= (BE16(ps_read_16(i + 1)) << 8);
            c |= ps_read_8(i + 3);
            if (c != *((uint32_t *)&garbage[i])) {
                kprintf_pc(__putc, NULL, "\n    READ32_ODD: Garbege data mismatch at $%.6X: %.8X should be %.8X.\n", i, c, *((uint32_t *)&garbage[i]));
                while(1);
            }

            if ((i % (frac * 4)) == 1)
                kprintf_pc(__putc, NULL, "*");
        }

        for (uint32_t i = 0; i < test_size; i++) {
            ps_write_8(i, (uint32_t)0x0);

            if ((i % (frac * 8)) == 0)
                kprintf_pc(__putc, NULL, "*");
        }

        kprintf_pc(__putc, NULL, "\n  Writing WORD garbage data to Chip, unaligned... ");
        for (uint32_t i = 1; i < (test_size) - 2; i += 2) {
            uint16_t v = *((uint16_t *)&garbage[i]);
            ps_write_8(i + 1, (v & 0x00FF));
            ps_write_8(i, (v >> 8));

            if ((i % (frac * 2)) == 1)
                kprintf_pc(__putc, NULL, "*");
        }

        for (uint32_t i = 1; i < (test_size) - 2; i += 2) {
            uint32_t c = BE16((ps_read_8(i) << 8) | ps_read_8(i + 1));
            if (c != *((uint16_t *)&garbage[i])) {
                kprintf_pc(__putc, NULL, "\n    READ16_ODD: Garbege data mismatch at $%.6X: %.4X should be %.4X.\n", i, c, *((uint16_t *)&garbage[i]));
                while(1);
            }

            if ((i % (frac * 2)) == 1)
                kprintf_pc(__putc, NULL, "*");
        }

        for (uint32_t i = 0; i < test_size; i++) {
            ps_write_8(i, (uint32_t)0x0);
        }

        kprintf_pc(__putc, NULL, "\n  Writing LONG garbage data to Chip, unaligned... ");
        for (uint32_t i = 1; i < (test_size) - 4; i += 4) {
            uint32_t v = *((uint32_t *)&garbage[i]);
            ps_write_8(i , v & 0x0000FF);
            ps_write_16(i + 1, BE16(((v & 0x00FFFF00) >> 8)));
            ps_write_8(i + 3 , (v & 0xFF000000) >> 24);

            if ((i % (frac * 2)) == 1)
                kprintf_pc(__putc, NULL, "*");
        }

        for (uint32_t i = 1; i < (test_size) - 4; i += 4) {
            uint32_t c = ps_read_8(i);
            c |= (BE16(ps_read_16(i + 1)) << 8);
            c |= (ps_read_8(i + 3) << 24);
            if (c != *((uint32_t *)&garbage[i])) {
                kprintf_pc(__putc, NULL, "\n    READ32_ODD: Garbege data mismatch at $%.6X: %.8X should be %.8X.\n", i, c, *((uint32_t *)&garbage[i]));
                while(1);
            }

            if ((i % (frac * 2)) == 1)
                kprintf_pc(__putc, NULL, "*");
        }

        kprintf_pc(__putc, NULL, "\n");
    }


    kprintf_pc(__putc, NULL, "All done. BUPTest completed.\n");

    ps_pulse_reset();

    tlsf_free(tlsf, garbage);
}
