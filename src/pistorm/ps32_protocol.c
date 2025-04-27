// SPDX-License-Identifier: MIT

/*
 * Copyright 2022 Claude Schwarz
 * Copyright 2022 Niklas Ekström
 * Copyright 2022 Michal Schulz (Emu68 adaptations)
 */

#undef PS_PROTOCOL_IMPL

#include <stdint.h>

#include "config.h"
#include "support.h"
#include "tlsf.h"
#include "ps_protocol.h"
#include "M68k.h"
#include "cache.h"

//volatile uint8_t gpio_lock;
//volatile uint32_t gpio_rdval;

extern struct M68KState *__m68k_state;

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

// REG_STATUS
#define STATUS_IS_BM        (1 << 0)
#define STATUS_RESET        (1 << 1)
#define STATUS_HALT         (1 << 2)
#define STATUS_IPL_MASK     (7 << 3)
#define STATUS_IPL_SHIFT    3
#define STATUS_TERM_NORMAL  (1 << 6)
#define STATUS_REQ_ACTIVE   (1 << 7)

// REG_CONTROL
#define CONTROL_REQ_BM      (1 << 0)
#define CONTROL_DRIVE_RESET (1 << 1)
#define CONTROL_DRIVE_HALT  (1 << 2)
#define CONTROL_DRIVE_INT2  (1 << 3)
#define CONTROL_DRIVE_INT6  (1 << 4)

typedef unsigned int uint;

#define IN      0
#define OUT     1
#define AF0     4
#define AF1     5
#define AF2     6
#define AF3     7
#define AF4     3
#define AF5     2

#define FUNC(pin, x)    (((x) & 0x7) << (((pin) % 10)*3))
#define REG(pin)        ((pin) / 10)
#define MASK(pin)       (7 << (((pin) % 10) * 3))

#define PF(f, i)        ((f) << (((i) % 10) * 3)) // Pin function.
#define GO(i)           PF(1, (i)) // GPIO output.

#define PIN_IPL0        0
#define PIN_IPL1        1
#define PIN_IPL2        2
#define PIN_TXN         3
#define PIN_KBRESET     4
#define SER_OUT_BIT     5 //debug EMU68
#define PIN_RD          6
#define PIN_WR          7
#define PIN_D(x)        (8 + x)
#define PIN_A(x)        (24 + x)
#define SER_OUT_CLK     27 //debug EMU68

#define CLEAR_BITS      (0x0fffffff & ~((1 << PIN_RD) | (1 << PIN_WR) | (1 << SER_OUT_BIT) | (1 << SER_OUT_CLK)))

// Pins for FPGA programming
#define PIN_CRESET1     6
#define PIN_CRESET2     7
#define PIN_TESTN       17
#define PIN_CCK         22
#define PIN_SS          24
#define PIN_CBUS0       14
#define PIN_CBUS1       15
#define PIN_CBUS2       18
#define PIN_CDI0        10
#define PIN_CDI1        25
#define PIN_CDI2        9
#define PIN_CDI3        8
#define PIN_CDI4        11
#define PIN_CDI5        1
#define PIN_CDI6        16
#define PIN_CDI7        13

#define SPLIT_DATA(x)   (x)
#define MERGE_DATA(x)   (x)

#define GPFSEL0_INPUT (GO(PIN_WR) | GO(PIN_RD) | GO(SER_OUT_BIT))
#define GPFSEL1_INPUT (0)
#define GPFSEL2_INPUT (GO(29) | GO(PIN_A(2)) | GO(PIN_A(1)) | GO(PIN_A(0)) | GO(SER_OUT_CLK))

#define GPFSEL0_OUTPUT (GO(PIN_D(1)) | GO(PIN_D(0)) | GO(PIN_WR) | GO(PIN_RD) | GO(SER_OUT_BIT))
#define GPFSEL1_OUTPUT (GO(PIN_D(11)) | GO(PIN_D(10)) | GO(PIN_D(9)) | GO(PIN_D(8)) | GO(PIN_D(7)) | GO(PIN_D(6)) | GO(PIN_D(5)) | GO(PIN_D(4)) | GO(PIN_D(3)) | GO(PIN_D(2)))
#define GPFSEL2_OUTPUT (GO(29) | GO(PIN_A(2)) | GO(PIN_A(1)) | GO(PIN_A(0)) | GO(PIN_D(15)) | GO(PIN_D(14)) | GO(PIN_D(13)) | GO(PIN_D(12)) | GO(SER_OUT_CLK))

#define REG_DATA_LO     0
#define REG_DATA_HI     1
#define REG_ADDR_LO     2
#define REG_ADDR_HI     3
#define REG_STATUS      4
#define REG_CONTROL     4
#define REG_VERSION     7

#define TXN_SIZE_SHIFT  8
#define TXN_RW_SHIFT    10
#define TXN_FC_SHIFT    11

#define SIZE_BYTE       0
#define SIZE_WORD       1
#define SIZE_LONG       3

#define TXN_READ        (1 << TXN_RW_SHIFT)
#define TXN_WRITE       (0 << TXN_RW_SHIFT)

#define BCM2708_PERI_BASE 0xF2000000  
#define BCM2708_PERI_SIZE 0x01000000

#define GPIO_ADDR 0x200000
#define GPCLK_ADDR 0x101000

#define CLK_PASSWD 0x5a000000
#define CLK_GP0_CTL 0x070
#define CLK_GP0_DIV 0x074

#define GPIO_PULL *(gpio + 37)      // Pull up/pull down
#define GPIO_PULLCLK0 *(gpio + 38)  // Pull up/pull down clock

#define DIRECTION_INPUT 0
#define DIRECTION_OUTPUT 1

volatile uint32_t *gpio;
volatile uint32_t *gpset;
volatile uint32_t *gpreset;
volatile uint32_t *gpread;

volatile struct
{
    uint32_t GPFSEL0;
    uint32_t GPFSEL1;
    uint32_t GPFSEL2;
    uint32_t GPFSEL3;
    uint32_t GPFSEL4;
    uint32_t GPFSEL5;
    uint32_t _pad_0;
    uint32_t GPSET0;
    uint32_t GPSET1;
    uint32_t _pad_1;
    uint32_t GPCLR0;
    uint32_t GPCLR1;
    uint32_t _pad_2;
    uint32_t GPLEV0;
    uint32_t GPLEV1;
    uint32_t _pad_3[42];
    uint32_t GPIO_PUP_PDN_CNTRL_REG0;
    uint32_t GPIO_PUP_PDN_CNTRL_REG1;
    uint32_t GPIO_PUP_PDN_CNTRL_REG2;
    uint32_t GPIO_PUP_PDN_CNTRL_REG3;
} *const GPIO = (volatile void *)(BCM2708_PERI_BASE + GPIO_ADDR);

uint32_t use_2slot = 1;

static inline void set_input() {
    *(gpio + 0) = LE32(GPFSEL0_INPUT);
    *(gpio + 1) = LE32(GPFSEL1_INPUT);
    *(gpio + 2) = LE32(GPFSEL2_INPUT);
}

static inline void set_output() {
    *(gpio + 0) = LE32(GPFSEL0_OUTPUT);
    *(gpio + 1) = LE32(GPFSEL1_OUTPUT);
    *(gpio + 2) = LE32(GPFSEL2_OUTPUT);
}

void pistorm_setup_serial()
{
    uint32_t fsel;
    
    fsel = *(gpio + REG(SER_OUT_BIT));
    fsel &= LE32(~MASK(SER_OUT_BIT));
    fsel |= LE32(FUNC(SER_OUT_BIT, OUT));
    *(gpio + REG(SER_OUT_BIT)) = fsel;

    fsel = *(gpio + REG(SER_OUT_CLK));
    fsel &= LE32(~MASK(SER_OUT_CLK));
    fsel |= LE32(FUNC(SER_OUT_CLK, OUT));
    *(gpio + REG(SER_OUT_CLK)) = fsel;
}

// Guessing PiStorm model. Activate Pull-Up on GPIO17 and GPIO24. Then read
// The value of this IO pins. Following values are expected
//
//              GPIO17   GPIO24
// PiStorm32:     Hi       Low
// PiStorm16:     Hi       Hi

uint8_t pistorm_get_model()
{
    uint8_t model = 0;
    uint32_t io;

    set_input();

    // Set GPIO17 and GPIO24 pull-ups
    GPIO->GPIO_PUP_PDN_CNTRL_REG1 &= ~LE32((3 << 2) | (3 << 16));
    GPIO->GPIO_PUP_PDN_CNTRL_REG1 |= LE32((1 << 2) | (1 << 16));

    // Read level on GPIO
    io = LE32(GPIO->GPLEV0);

    if (io & (1 << 17)) {
        model |= 1;
    }
    if (io & (1 << 24))
    {
        model |= 2;
    }

    return model;
}

static void pistorm_setup_io()
{
    gpio =    ((volatile uint32_t *)BCM2708_PERI_BASE) + GPIO_ADDR / 4;
    gpset =   ((volatile uint32_t *)gpio) + 7;// *(gpio + 7);
    gpreset = ((volatile uint32_t *)gpio) + 10;// *(gpio + 10);
    gpread =  ((volatile uint32_t *)gpio) + 13;// *(gpio + 13);
}

static inline void write_ps_reg(unsigned int address, unsigned int data)
{
    *gpset = LE32((SPLIT_DATA(data) << PIN_D(0)) | (address << PIN_A(0)));

    //Delay for Pi4, 2*3.5nS
    *gpreset = LE32(1 << PIN_WR); *gpreset = LE32(1 << PIN_WR); *gpreset = LE32(1 << PIN_WR);

    *gpset = LE32(1 << PIN_WR);
    *gpreset = LE32(CLEAR_BITS);
}

static inline unsigned int read_ps_reg(unsigned int address)
{
    *gpset = LE32(address << PIN_A(0));

    //Delay for Pi3, 3*7.5nS , or 3*3.5nS for
    *gpreset = LE32(1 << PIN_RD); *gpreset = LE32(1 << PIN_RD); *gpreset = LE32(1 << PIN_RD); // *gpreset = LE32(1 << PIN_RD);

    unsigned int data = LE32(*gpread);
    data = LE32(*gpread);
    /*
    data = LE32(*gpread);
    data = LE32(*gpread);
    */
    *gpset = LE32(1 << PIN_RD);
    *gpreset = LE32(CLEAR_BITS);

//    gpio_rdval = data;

    data = MERGE_DATA(data >> PIN_D(0)) & 0xffff;

    return data;
}

static inline unsigned int read_ps_reg_with_wait(unsigned int address)
{
    *gpset = LE32(address << PIN_A(0));

    //Delay for Pi3, 3*7.5nS , or 3*3.5nS for
    *gpreset = LE32(1 << PIN_RD); *gpreset = LE32(1 << PIN_RD); *gpreset = LE32(1 << PIN_RD); //*gpreset = LE32(1 << PIN_RD);

    unsigned int data;
    while ((data = *gpread) & LE32(1 << PIN_TXN)) asm volatile("yield");
    data = LE32(*gpread);
    /*
    data = LE32(*gpread);
    data = LE32(*gpread);
    */
    *gpset = LE32(1 << PIN_RD);
    *gpreset = LE32(CLEAR_BITS);

//    gpio_rdval = data;

    data = MERGE_DATA(data >> PIN_D(0)) & 0xffff;

    return data;
}


void ps_set_control(unsigned int value)
{
//    while(__atomic_test_and_set(&gpio_lock, __ATOMIC_ACQUIRE)) asm volatile("yield");

    set_output();
    write_ps_reg(REG_CONTROL, 0x8000 | (value & 0x7fff));
    set_input();

//    __atomic_clear(&gpio_lock, __ATOMIC_RELEASE);
}

void ps_clr_control(unsigned int value)
{
//    while(__atomic_test_and_set(&gpio_lock, __ATOMIC_ACQUIRE)) asm volatile("yield");

    set_output();
    write_ps_reg(REG_CONTROL, value & 0x7fff);
    set_input();

//    __atomic_clear(&gpio_lock, __ATOMIC_RELEASE);
}

unsigned int ps_read_status()
{
//    while(__atomic_test_and_set(&gpio_lock, __ATOMIC_ACQUIRE)) asm volatile("yield");
    
    unsigned int status = read_ps_reg(REG_STATUS);

//    __atomic_clear(&gpio_lock, __ATOMIC_RELEASE);

    return status;
}

static uint g_fc = 0;

void cpu_set_fc(unsigned int fc)
{
    g_fc = fc;
}

static int next_slot = 0;
static int slot_active[2] = {0, 0};
#define REG_SLOT        5
#define CONTROL_INC_EXEC_SLOT (1 << 5)

static void do_write_access_2s(unsigned int address, unsigned int data, unsigned int size)
{
    uint32_t rdval = 0;

//    while(__atomic_test_and_set(&gpio_lock, __ATOMIC_ACQUIRE)) asm volatile("yield");

    set_output();

    write_ps_reg(REG_SLOT, next_slot);
    if (slot_active[next_slot])
    {
        while ((rdval = *gpread) & LE32(1 << PIN_TXN)) asm volatile("yield");
//        gpio_rdval = LE32(rdval);
    }

    write_ps_reg(REG_DATA_LO, data & 0xffff);
    if (size == SIZE_LONG)
        write_ps_reg(REG_DATA_HI, (data >> 16) & 0xffff);

    write_ps_reg(REG_ADDR_LO, address & 0xffff);
    write_ps_reg(REG_ADDR_HI, TXN_WRITE | (g_fc << TXN_FC_SHIFT) | (size << TXN_SIZE_SHIFT) | ((address >> 16) & 0xff));

    set_input();
   
    /* For chip memory advance to next slot, for chipset access wait for completion of the transaction */
    if (address >= 0x00bf0000 && address <= 0x00dfffff)
    {
        while ((rdval = *gpread) & LE32(1 << PIN_TXN)) asm volatile("yield");
//        gpio_rdval = LE32(rdval);
        slot_active[next_slot] = 0;
    }
    else
    {
        slot_active[next_slot] = 1;
    }

//    __atomic_clear(&gpio_lock, __ATOMIC_RELEASE);

    next_slot = (next_slot + 1) & 1;

    if (address >= 0x00bf0000 && address <= 0x00dfffff) ps_read_32(0x00f80000);
}

static inline void do_write_access_64_2s(unsigned int address, uint64_t data)
{
    uint32_t rdval = 0;

//    while(__atomic_test_and_set(&gpio_lock, __ATOMIC_ACQUIRE)) asm volatile("yield");

    set_output();

    write_ps_reg(REG_SLOT, next_slot);
    if (slot_active[next_slot])
    {
        while ((rdval = *gpread) & LE32(1 << PIN_TXN)) asm volatile("yield");
//        gpio_rdval = LE32(rdval);
    }

    /* Set first long word to write */
    write_ps_reg(REG_DATA_LO, (data >> 32) & 0xffff);
    write_ps_reg(REG_DATA_HI, (data >> 48) & 0xffff);

    /* Set address and start transaction */
    write_ps_reg(REG_ADDR_LO, address & 0xffff);
    write_ps_reg(REG_ADDR_HI, TXN_WRITE | (g_fc << TXN_FC_SHIFT) | (SIZE_LONG << TXN_SIZE_SHIFT) | ((address >> 16) & 0xff));

    slot_active[next_slot] = 1;
    next_slot = (next_slot + 1) & 1;

    write_ps_reg(REG_SLOT, next_slot);
    if (slot_active[next_slot])
    {
        while ((rdval = *gpread) & LE32(1 << PIN_TXN)) asm volatile("yield");
//        gpio_rdval = LE32(rdval);
    }

    /* Advance the address, set second long word to write */
    address += 4;

    /* Set second long word to write */
    write_ps_reg(REG_DATA_LO, (data) & 0xffff);
    write_ps_reg(REG_DATA_HI, (data >> 16) & 0xffff);

    /* Set address */
    write_ps_reg(REG_ADDR_LO, address & 0xffff);

    /* Start second transaction */
    write_ps_reg(REG_ADDR_HI, TXN_WRITE | (g_fc << TXN_FC_SHIFT) | (SIZE_LONG << TXN_SIZE_SHIFT) | ((address >> 16) & 0xff));

    set_input();
   
    /* For chip memory advance to next slot, for chipset access wait for completion of the transaction */
    if (address >= 0x00bf0000 && address <= 0x00dfffff)
    {
        while ((rdval = *gpread) & LE32(1 << PIN_TXN)) asm volatile("yield");
//        gpio_rdval = LE32(rdval);
        slot_active[next_slot] = 0;
    }
    else
    {
        slot_active[next_slot] = 1;
    }

//    __atomic_clear(&gpio_lock, __ATOMIC_RELEASE);

    next_slot = (next_slot + 1) & 1;

    if (address >= 0x00bf0000 && address <= 0x00dfffff) ps_read_32(0x00f80000);
}

static inline void do_write_access_128_2s(unsigned int address, uint128_t data)
{
    uint32_t rdval = 0;

    //while(__atomic_test_and_set(&gpio_lock, __ATOMIC_ACQUIRE)) asm volatile("yield");

    set_output();
    write_ps_reg(REG_SLOT, next_slot);
    if (slot_active[next_slot])
    {
        while ((rdval = *gpread) & LE32(1 << PIN_TXN)) asm volatile("yield");
        //gpio_rdval = LE32(rdval);
    }

    /* Set first long word to write */
    write_ps_reg(REG_DATA_LO, (data.hi >> 32) & 0xffff);
    write_ps_reg(REG_DATA_HI, (data.hi >> 48) & 0xffff);

    /* Set address and start transaction */
    write_ps_reg(REG_ADDR_LO, address & 0xffff);
    write_ps_reg(REG_ADDR_HI, TXN_WRITE | (g_fc << TXN_FC_SHIFT) | (SIZE_LONG << TXN_SIZE_SHIFT) | ((address >> 16) & 0xff));

    slot_active[next_slot] = 1;
    next_slot = (next_slot + 1) & 1;
    write_ps_reg(REG_SLOT, next_slot);
    if (slot_active[next_slot])
    {
        while ((rdval = *gpread) & LE32(1 << PIN_TXN)) asm volatile("yield");
//        gpio_rdval = LE32(rdval);
    }

    /* Advance the address, set second long word to write */
    address += 4;

    /* Set second long word to write */
    write_ps_reg(REG_DATA_LO, (data.hi) & 0xffff);
    write_ps_reg(REG_DATA_HI, (data.hi >> 16) & 0xffff);

    /* Set address */
    write_ps_reg(REG_ADDR_LO, address & 0xffff);

    /* Start second transaction */
    write_ps_reg(REG_ADDR_HI, TXN_WRITE | (g_fc << TXN_FC_SHIFT) | (SIZE_LONG << TXN_SIZE_SHIFT) | ((address >> 16) & 0xff));

    slot_active[next_slot] = 1;
    next_slot = (next_slot + 1) & 1;
    write_ps_reg(REG_SLOT, next_slot);
    if (slot_active[next_slot])
    {
        while ((rdval = *gpread) & LE32(1 << PIN_TXN)) asm volatile("yield");
//        gpio_rdval = LE32(rdval);
    }

    address += 4;

    /* Set third long word to write */
    write_ps_reg(REG_DATA_LO, (data.lo >> 32) & 0xffff);
    write_ps_reg(REG_DATA_HI, (data.lo >> 48) & 0xffff);

    /* Set address */
    write_ps_reg(REG_ADDR_LO, address & 0xffff);

    /* Start third transaction */
    write_ps_reg(REG_ADDR_HI, TXN_WRITE | (g_fc << TXN_FC_SHIFT) | (SIZE_LONG << TXN_SIZE_SHIFT) | ((address >> 16) & 0xff));

    address += 4;

    slot_active[next_slot] = 1;
    next_slot = (next_slot + 1) & 1;
    write_ps_reg(REG_SLOT, next_slot);
    if (slot_active[next_slot])
    {
        while ((rdval = *gpread) & LE32(1 << PIN_TXN)) asm volatile("yield");
//        gpio_rdval = LE32(rdval);
    }

    /* Set second long word to write */
    write_ps_reg(REG_DATA_LO, (data.lo) & 0xffff);
    write_ps_reg(REG_DATA_HI, (data.lo >> 16) & 0xffff);

    /* Set address */
    write_ps_reg(REG_ADDR_LO, address & 0xffff);

    /* Start fourth transaction */
    write_ps_reg(REG_ADDR_HI, TXN_WRITE | (g_fc << TXN_FC_SHIFT) | (SIZE_LONG << TXN_SIZE_SHIFT) | ((address >> 16) & 0xff));

    set_input();

    /* For chip memory advance to next slot, for chipset access wait for completion of the transaction */
    if (address >= 0x00bf0000 && address <= 0x00dfffff)
    {
        while ((rdval = *gpread) & LE32(1 << PIN_TXN)) asm volatile("yield");
//        gpio_rdval = LE32(rdval);
        slot_active[next_slot] = 0;
    }
    else
    {
        slot_active[next_slot] = 1;
    }

//    __atomic_clear(&gpio_lock, __ATOMIC_RELEASE);

    next_slot = (next_slot + 1) & 1;

    if (address >= 0x00bf0000 && address <= 0x00dfffff) ps_read_32(0x00f80000);
}

static int do_read_access_2s(unsigned int address, unsigned int size)
{
    uint32_t rdval = 0;

//    while(__atomic_test_and_set(&gpio_lock, __ATOMIC_ACQUIRE)) asm volatile("yield");

    set_output();
    write_ps_reg(REG_SLOT, next_slot);
    if (slot_active[next_slot])
    {
        while ((rdval = *gpread) & LE32(1 << PIN_TXN)) asm volatile("yield");
        //gpio_rdval = LE32(rdval);
    }

    write_ps_reg(REG_ADDR_LO, address & 0xffff);
    write_ps_reg(REG_ADDR_HI, TXN_READ | (g_fc << TXN_FC_SHIFT) | (size << TXN_SIZE_SHIFT) | ((address >> 16) & 0xff));

    set_input();
    unsigned int data;

    while ((rdval = *gpread) & LE32(1 << PIN_TXN)) asm volatile("yield");
//    gpio_rdval = LE32(rdval);

    data = read_ps_reg(REG_DATA_LO);
    if (size == SIZE_BYTE)
        data &= 0xff;
    else if (size == SIZE_LONG)
        data |= read_ps_reg(REG_DATA_HI) << 16;

//    __atomic_clear(&gpio_lock, __ATOMIC_RELEASE);

    slot_active[next_slot] = 0;
    next_slot = (next_slot + 1) & 1;

    return data;
}

static inline uint64_t do_read_access_64_2s(unsigned int address)
{
    uint64_t data;
    uint32_t rdval = 0;

//    while(__atomic_test_and_set(&gpio_lock, __ATOMIC_ACQUIRE)) asm volatile("yield");

    set_output();
    write_ps_reg(REG_SLOT, next_slot);
    if (slot_active[next_slot])
    {
        while ((rdval = *gpread) & LE32(1 << PIN_TXN)) asm volatile("yield");
//        gpio_rdval = LE32(rdval);
    }

    write_ps_reg(REG_ADDR_LO, address & 0xffff);
    write_ps_reg(REG_ADDR_HI, TXN_READ | (g_fc << TXN_FC_SHIFT) | (SIZE_LONG << TXN_SIZE_SHIFT) | ((address >> 16) & 0xff));

    set_input();

    while ((rdval = *gpread) & LE32(1 << PIN_TXN)) asm volatile("yield");
//    gpio_rdval = LE32(rdval);

    address += 4;

    data = (uint64_t)read_ps_reg(REG_DATA_HI) << 48;
    data |= (uint64_t)read_ps_reg(REG_DATA_LO) << 32;
    slot_active[next_slot] = 0;
    next_slot = (next_slot + 1) & 1;

    set_output();
    write_ps_reg(REG_SLOT, next_slot);
    //if (slot_active[next_slot])
    //{
    //    while ((rdval = *gpread) & LE32(1 << PIN_TXN)) asm volatile("yield");
    //    gpio_rdval = LE32(rdval);
    //}

    write_ps_reg(REG_ADDR_LO, address & 0xffff);
    write_ps_reg(REG_ADDR_HI, TXN_READ | (g_fc << TXN_FC_SHIFT) | (SIZE_LONG << TXN_SIZE_SHIFT) | ((address >> 16) & 0xff));

    set_input();

    while ((rdval = *gpread) & LE32(1 << PIN_TXN)) asm volatile("yield");
//    gpio_rdval = LE32(rdval);

    slot_active[next_slot] = 0;
    next_slot = (next_slot + 1) & 1;

    data |= (uint64_t)read_ps_reg(REG_DATA_HI) << 16;
    data |= (uint64_t)read_ps_reg(REG_DATA_LO);

//    __atomic_clear(&gpio_lock, __ATOMIC_RELEASE);

    return data;
}

static inline uint128_t do_read_access_128_2s(unsigned int address)
{
    uint128_t data;
    uint32_t rdval = 0;

//    while(__atomic_test_and_set(&gpio_lock, __ATOMIC_ACQUIRE)) asm volatile("yield");

    set_output();
    write_ps_reg(REG_SLOT, next_slot);
    if (slot_active[next_slot])
    {
        while ((rdval = *gpread) & LE32(1 << PIN_TXN)) asm volatile("yield");
//        gpio_rdval = LE32(rdval);
    }

    write_ps_reg(REG_ADDR_LO, address & 0xffff);
    write_ps_reg(REG_ADDR_HI, TXN_READ | (g_fc << TXN_FC_SHIFT) | (SIZE_LONG << TXN_SIZE_SHIFT) | ((address >> 16) & 0xff));

    set_input();

    while ((rdval = *gpread) & LE32(1 << PIN_TXN)) asm volatile("yield");
//    gpio_rdval = LE32(rdval);

    address += 4;
    data.hi = (uint64_t)read_ps_reg(REG_DATA_HI) << 48;
    data.hi |= (uint64_t)read_ps_reg(REG_DATA_LO) << 32;
    slot_active[next_slot] = 0;
    next_slot = (next_slot + 1) & 1;

    set_output();
    write_ps_reg(REG_SLOT, next_slot);
    //if (slot_active[next_slot])
    //{
    //    while ((rdval = *gpread) & LE32(1 << PIN_TXN)) asm volatile("yield");
    //    gpio_rdval = LE32(rdval);
    //}

    write_ps_reg(REG_ADDR_LO, address & 0xffff);
    write_ps_reg(REG_ADDR_HI, TXN_READ | (g_fc << TXN_FC_SHIFT) | (SIZE_LONG << TXN_SIZE_SHIFT) | ((address >> 16) & 0xff));

    set_input();

    while ((rdval = *gpread) & LE32(1 << PIN_TXN)) asm volatile("yield");
//    gpio_rdval = LE32(rdval);

    address += 4;
    data.hi |= (uint64_t)read_ps_reg(REG_DATA_HI) << 16;
    data.hi |= (uint64_t)read_ps_reg(REG_DATA_LO);
    slot_active[next_slot] = 0;
    next_slot = (next_slot + 1) & 1;

    set_output();
    write_ps_reg(REG_SLOT, next_slot);
    //if (slot_active[next_slot])
    //{
    //    while ((rdval = *gpread) & LE32(1 << PIN_TXN)) asm volatile("yield");
    //    gpio_rdval = LE32(rdval);
    //}

    write_ps_reg(REG_ADDR_LO, address & 0xffff);
    write_ps_reg(REG_ADDR_HI, TXN_READ | (g_fc << TXN_FC_SHIFT) | (SIZE_LONG << TXN_SIZE_SHIFT) | ((address >> 16) & 0xff));

    set_input();

    while ((rdval = *gpread) & LE32(1 << PIN_TXN)) asm volatile("yield");
//    gpio_rdval = LE32(rdval);

    address += 4;
    data.lo = (uint64_t)read_ps_reg(REG_DATA_HI) << 48;
    data.lo |= (uint64_t)read_ps_reg(REG_DATA_LO) << 32;
    slot_active[next_slot] = 0;
    next_slot = (next_slot + 1) & 1;

    set_output();
    write_ps_reg(REG_SLOT, next_slot);
    //if (slot_active[next_slot])
    //{
    //    while ((rdval = *gpread) & LE32(1 << PIN_TXN)) asm volatile("yield");
    //    gpio_rdval = LE32(rdval);
    //}

    write_ps_reg(REG_ADDR_LO, address & 0xffff);
    write_ps_reg(REG_ADDR_HI, TXN_READ | (g_fc << TXN_FC_SHIFT) | (SIZE_LONG << TXN_SIZE_SHIFT) | ((address >> 16) & 0xff));

    set_input();

    while ((rdval = *gpread) & LE32(1 << PIN_TXN)) asm volatile("yield");
//    gpio_rdval = LE32(rdval);

    data.lo |= (uint64_t)read_ps_reg(REG_DATA_HI) << 16;
    data.lo |= (uint64_t)read_ps_reg(REG_DATA_LO);

    slot_active[next_slot] = 0;
    next_slot = (next_slot + 1) & 1;

//    __atomic_clear(&gpio_lock, __ATOMIC_RELEASE);

    return data;
}

static uint write_pending = 0;

static inline void do_write_access(unsigned int address, unsigned int data, unsigned int size)
{
    uint32_t rdval = 0;

//    while(__atomic_test_and_set(&gpio_lock, __ATOMIC_ACQUIRE)) asm volatile("yield");

    set_output();

    write_ps_reg(REG_DATA_LO, data & 0xffff);
    if (size == SIZE_LONG)
        write_ps_reg(REG_DATA_HI, (data >> 16) & 0xffff);

    write_ps_reg(REG_ADDR_LO, address & 0xffff);

    if (write_pending) while ((rdval = *gpread) & LE32(1 << PIN_TXN)) {}
//    gpio_rdval = LE32(rdval);

    write_ps_reg(REG_ADDR_HI, TXN_WRITE | (g_fc << TXN_FC_SHIFT) | (size << TXN_SIZE_SHIFT) | ((address >> 16) & 0xff));

    set_input();

    if (address > 0x00200000)
    {
        while ((rdval = *gpread) & LE32(1 << PIN_TXN)) {}
//        gpio_rdval = LE32(rdval);

        write_pending = 0;
    }
    else
        write_pending = 1;
    
//    __atomic_clear(&gpio_lock, __ATOMIC_RELEASE);
}

static inline void do_write_access_64(unsigned int address, uint64_t data)
{
    uint32_t rdval = 0;

//    while(__atomic_test_and_set(&gpio_lock, __ATOMIC_ACQUIRE)) asm volatile("yield");

    set_output();

    /* Set first long word to write */
    write_ps_reg(REG_DATA_LO, (data >> 32) & 0xffff);
    write_ps_reg(REG_DATA_HI, (data >> 48) & 0xffff);

    /* Set address and start transaction */
    write_ps_reg(REG_ADDR_LO, address & 0xffff);

    if (write_pending) while ((rdval = *gpread) & LE32(1 << PIN_TXN)) {}
//    gpio_rdval = LE32(rdval);

    write_ps_reg(REG_ADDR_HI, TXN_WRITE | (g_fc << TXN_FC_SHIFT) | (SIZE_LONG << TXN_SIZE_SHIFT) | ((address >> 16) & 0xff));

    /* Advance the address, set second long word to write */
    address += 4;

    /* Wait for completion of first transfer */
    while ((rdval = *gpread) & LE32(1 << PIN_TXN)) {}
//    gpio_rdval = LE32(rdval);

    /* Set second long word to write */
    write_ps_reg(REG_DATA_LO, (data) & 0xffff);
    write_ps_reg(REG_DATA_HI, (data >> 16) & 0xffff);

    /* Set address */
    write_ps_reg(REG_ADDR_LO, address & 0xffff);
   
    /* Start second transaction */
    write_ps_reg(REG_ADDR_HI, TXN_WRITE | (g_fc << TXN_FC_SHIFT) | (SIZE_LONG << TXN_SIZE_SHIFT) | ((address >> 16) & 0xff));

    set_input();

    if (address > 0x00200000)
    {
        while ((rdval = *gpread) & LE32(1 << PIN_TXN)) {}
//        gpio_rdval = LE32(rdval);

        write_pending = 0;
    }
    else
        write_pending = 1;
    
//    __atomic_clear(&gpio_lock, __ATOMIC_RELEASE);
}

static inline void do_write_access_128(unsigned int address, uint128_t data)
{
    uint32_t rdval = 0;

//    while(__atomic_test_and_set(&gpio_lock, __ATOMIC_ACQUIRE)) asm volatile("yield");

    set_output();

    /* Set first long word to write */
    write_ps_reg(REG_DATA_LO, (data.hi >> 32) & 0xffff);
    write_ps_reg(REG_DATA_HI, (data.hi >> 48) & 0xffff);

    /* Set address and start transaction */
    write_ps_reg(REG_ADDR_LO, address & 0xffff);

    if (write_pending) while ((rdval = *gpread) & LE32(1 << PIN_TXN)) {}
//    gpio_rdval = LE32(rdval);

    write_ps_reg(REG_ADDR_HI, TXN_WRITE | (g_fc << TXN_FC_SHIFT) | (SIZE_LONG << TXN_SIZE_SHIFT) | ((address >> 16) & 0xff));

    /* Advance the address, set second long word to write */
    address += 4;

    /* Wait for completion of first transfer */
    while ((rdval = *gpread) & LE32(1 << PIN_TXN)) {}
//    gpio_rdval = LE32(rdval);

    /* Set second long word to write */
    write_ps_reg(REG_DATA_LO, (data.hi) & 0xffff);
    write_ps_reg(REG_DATA_HI, (data.hi >> 16) & 0xffff);

    /* Set address */
    write_ps_reg(REG_ADDR_LO, address & 0xffff);
    
    /* Start second transaction */
    write_ps_reg(REG_ADDR_HI, TXN_WRITE | (g_fc << TXN_FC_SHIFT) | (SIZE_LONG << TXN_SIZE_SHIFT) | ((address >> 16) & 0xff));

    address += 4;

    /* Wait for completion of second transfer */
    while ((rdval = *gpread) & LE32(1 << PIN_TXN)) {}
//    gpio_rdval = LE32(rdval);

    /* Set third long word to write */
    write_ps_reg(REG_DATA_LO, (data.lo >> 32) & 0xffff);
    write_ps_reg(REG_DATA_HI, (data.lo >> 48) & 0xffff);

    /* Set address */
    write_ps_reg(REG_ADDR_LO, address & 0xffff);
    
    /* Start third transaction */
    write_ps_reg(REG_ADDR_HI, TXN_WRITE | (g_fc << TXN_FC_SHIFT) | (SIZE_LONG << TXN_SIZE_SHIFT) | ((address >> 16) & 0xff));

    address += 4;

    /* Wait for completion of third transfer */
    while ((rdval = *gpread) & LE32(1 << PIN_TXN)) {}
//    gpio_rdval = LE32(rdval);

    /* Set second long word to write */
    write_ps_reg(REG_DATA_LO, (data.lo) & 0xffff);
    write_ps_reg(REG_DATA_HI, (data.lo >> 16) & 0xffff);

    /* Set address */
    write_ps_reg(REG_ADDR_LO, address & 0xffff);
    
    /* Start fourth transaction */
    write_ps_reg(REG_ADDR_HI, TXN_WRITE | (g_fc << TXN_FC_SHIFT) | (SIZE_LONG << TXN_SIZE_SHIFT) | ((address >> 16) & 0xff));

    set_input();

    if (address > 0x00200000)
    {
        while ((rdval = *gpread) & LE32(1 << PIN_TXN)) {}
//        gpio_rdval = LE32(rdval);

        write_pending = 0;
    }
    else
        write_pending = 1;

//    __atomic_clear(&gpio_lock, __ATOMIC_RELEASE);
}

static inline int do_read_access(unsigned int address, unsigned int size)
{
    uint32_t rdval = 0;

//    while(__atomic_test_and_set(&gpio_lock, __ATOMIC_ACQUIRE)) asm volatile("yield");

    set_output();

    write_ps_reg(REG_ADDR_LO, address & 0xffff);

    if (write_pending) while ((rdval = *gpread) & LE32(1 << PIN_TXN)) {}
//    gpio_rdval = LE32(rdval);

    write_ps_reg(REG_ADDR_HI, TXN_READ | (g_fc << TXN_FC_SHIFT) | (size << TXN_SIZE_SHIFT) | ((address >> 16) & 0xff));

    set_input();
    unsigned int data;

//    while ((rdval = *gpread) & LE32(1 << PIN_TXN)) {}
//    gpio_rdval = LE32(rdval);

    data = read_ps_reg_with_wait(REG_DATA_LO);
    if (size == SIZE_BYTE)
        data &= 0xff;
    else if (size == SIZE_LONG)
        data |= read_ps_reg(REG_DATA_HI) << 16;

    write_pending = 0;

//    __atomic_clear(&gpio_lock, __ATOMIC_RELEASE);

    return data;
}

static inline uint64_t do_read_access_64(unsigned int address)
{
    uint32_t rdval = 0;
    uint64_t data;

//    while(__atomic_test_and_set(&gpio_lock, __ATOMIC_ACQUIRE)) asm volatile("yield");

    set_output();

    write_ps_reg(REG_ADDR_LO, address & 0xffff);

    if (write_pending) while ((rdval = *gpread) & LE32(1 << PIN_TXN)) {}
//    gpio_rdval = LE32(rdval);

    write_ps_reg(REG_ADDR_HI, TXN_READ | (g_fc << TXN_FC_SHIFT) | (SIZE_LONG << TXN_SIZE_SHIFT) | ((address >> 16) & 0xff));

    set_input();

//    while ((rdval = *gpread) & LE32(1 << PIN_TXN)) {}
//    gpio_rdval = LE32(rdval);

    address += 4;
    data = (uint64_t)read_ps_reg_with_wait(REG_DATA_HI) << 48;
    data |= (uint64_t)read_ps_reg(REG_DATA_LO) << 32;

    set_output();

    write_ps_reg(REG_ADDR_LO, address & 0xffff);
    write_ps_reg(REG_ADDR_HI, TXN_READ | (g_fc << TXN_FC_SHIFT) | (SIZE_LONG << TXN_SIZE_SHIFT) | ((address >> 16) & 0xff));

    set_input();

//    while ((rdval = *gpread) & LE32(1 << PIN_TXN)) {}
//    gpio_rdval = LE32(rdval);

    data |= (uint64_t)read_ps_reg_with_wait(REG_DATA_HI) << 16;
    data |= (uint64_t)read_ps_reg(REG_DATA_LO);

    write_pending = 0;

//    __atomic_clear(&gpio_lock, __ATOMIC_RELEASE);

    return data;
}

static inline uint128_t do_read_access_128(unsigned int address)
{
    uint32_t rdval = 0;
    uint128_t data;

//    while(__atomic_test_and_set(&gpio_lock, __ATOMIC_ACQUIRE)) asm volatile("yield");

    set_output();

    write_ps_reg(REG_ADDR_LO, address & 0xffff);
    
    if (write_pending) while ((rdval = *gpread) & LE32(1 << PIN_TXN)) {}
//    gpio_rdval = LE32(rdval);

    write_ps_reg(REG_ADDR_HI, TXN_READ | (g_fc << TXN_FC_SHIFT) | (SIZE_LONG << TXN_SIZE_SHIFT) | ((address >> 16) & 0xff));

    set_input();

//    while ((rdval = *gpread) & LE32(1 << PIN_TXN)) {}
//    gpio_rdval = LE32(rdval);

    address += 4;
    data.hi = (uint64_t)read_ps_reg_with_wait(REG_DATA_HI) << 48;
    data.hi |= (uint64_t)read_ps_reg(REG_DATA_LO) << 32;

    set_output();

    write_ps_reg(REG_ADDR_LO, address & 0xffff);
    write_ps_reg(REG_ADDR_HI, TXN_READ | (g_fc << TXN_FC_SHIFT) | (SIZE_LONG << TXN_SIZE_SHIFT) | ((address >> 16) & 0xff));

    set_input();

//    while ((rdval = *gpread) & LE32(1 << PIN_TXN)) {}
//    gpio_rdval = LE32(rdval);

    address += 4;
    data.hi |= (uint64_t)read_ps_reg_with_wait(REG_DATA_HI) << 16;
    data.hi |= (uint64_t)read_ps_reg(REG_DATA_LO);

    set_output();

    write_ps_reg(REG_ADDR_LO, address & 0xffff);
    write_ps_reg(REG_ADDR_HI, TXN_READ | (g_fc << TXN_FC_SHIFT) | (SIZE_LONG << TXN_SIZE_SHIFT) | ((address >> 16) & 0xff));

    set_input();

//    while ((rdval = *gpread) & LE32(1 << PIN_TXN)) {}
//    gpio_rdval = LE32(rdval);

    address += 4;
    data.lo = (uint64_t)read_ps_reg_with_wait(REG_DATA_HI) << 48;
    data.lo |= (uint64_t)read_ps_reg(REG_DATA_LO) << 32;

    set_output();

    write_ps_reg(REG_ADDR_LO, address & 0xffff);
    write_ps_reg(REG_ADDR_HI, TXN_READ | (g_fc << TXN_FC_SHIFT) | (SIZE_LONG << TXN_SIZE_SHIFT) | ((address >> 16) & 0xff));

    set_input();

//    while ((rdval = *gpread) & LE32(1 << PIN_TXN)) {}
//    gpio_rdval = LE32(rdval);

    data.lo |= (uint64_t)read_ps_reg_with_wait(REG_DATA_HI) << 16;
    data.lo |= (uint64_t)read_ps_reg(REG_DATA_LO);

    write_pending = 0;

//    __atomic_clear(&gpio_lock, __ATOMIC_RELEASE);

    return data;
}

int (*read_access)(unsigned int address, unsigned int size);
uint64_t (*read_access_64)(unsigned int address);
uint128_t (*read_access_128)(unsigned int address);
void (*write_access)(unsigned int address, unsigned int data, unsigned int size);
void (*write_access_64)(unsigned int address, uint64_t data);
void (*write_access_128)(unsigned int address, uint128_t data);

void ps_setup_protocol()
{
    if (use_2slot)
    {
        kprintf("[PS32] Setting up two-slot protocol\n");
        
        read_access = do_read_access_2s;
        read_access_64 = do_read_access_64_2s;
        read_access_128 = do_read_access_128_2s;

        write_access = do_write_access_2s;
        write_access_64 = do_write_access_64_2s;
        write_access_128 = do_write_access_128_2s;
    }
    else
    {
        kprintf("[PS32] Setting up protocol\n");

        read_access = do_read_access;
        read_access_64 = do_read_access_64;
        read_access_128 = do_read_access_128;

        write_access = do_write_access;
        write_access_64 = do_write_access_64;
        write_access_128 = do_write_access_128;
    }

//    __atomic_clear(&gpio_lock, __ATOMIC_RELEASE);

    pistorm_setup_io();
    pistorm_setup_serial();

    *gpreset = LE32(CLEAR_BITS);

    set_input();
}


unsigned int caddress;
union {
    uint32_t u32;
    uint16_t u16[2];
    uint8_t  u8[4];
} cdata;
union {
    uint32_t u32;
    uint16_t u16[2];
    uint8_t  u8[4];
} cmask;

void flush_cdata()
{
    if (cmask.u32 == 0xffffffff)
    {
        write_access(caddress, cdata.u32, SIZE_LONG);
    }
    else
    {
        if (cmask.u16[0] == 0xffff)
        {
            write_access(caddress, cdata.u16[0], SIZE_WORD);
            cmask.u16[0] = 0;
        }
        if (cmask.u16[1] == 0xffff)
        {
            write_access(caddress + 2, cdata.u16[1], SIZE_WORD);
            cmask.u16[1] = 0;
        }

        if (cmask.u32)
        {
            for (int i=0; i < 4; i++)
            {
                if (cmask.u8[i]) {
                    write_access(caddress + i, cdata.u8[i], SIZE_BYTE);
                }
            }
        }
    }

    caddress = 0xffffffff;
    cmask.u32 = 0;
}

#define SLOW_IO(address) ((address) >= 0xDFF09A && (address) < 0xDFF09E)

static inline void check_blit_active(unsigned int addr, unsigned int size) {
    if (!(__m68k_state->JIT_CONTROL2 & JC2F_BLITWAIT))
        return;

    const uint32_t bstart = 0xDFF040;   // BLTCON0
    const uint32_t bend = 0xDFF076;     // BLTADAT+2
    if (addr >= bend || addr + size <= bstart)
        return;

    const uint16_t mask = 1<<14 | 1<<9 | 1<<6; // BBUSY | DMAEN | BLTEN
    while ((read_access(0xdff002, SIZE_WORD) & mask) == mask) {
        // Dummy reads to not steal too many cycles from the blitter.
        // But don't use e.g. CIA reads as we expect the operation
        // to finish soon.
        read_access(0x00f00000, SIZE_BYTE);
        read_access(0x00f00000, SIZE_BYTE);
    }
}

void ps_write_8(unsigned int address, unsigned int data) {
    write_access(address, data, SIZE_BYTE);
    if (SLOW_IO(address))
    {
        read_access(0x00f00000, SIZE_BYTE);
    }
    cache_invalidate_range(ICACHE, address, 1);
}

void ps_write_16(unsigned int address, unsigned int data) {
    check_blit_active(address, 2);
    write_access(address, data, SIZE_WORD);
    if (SLOW_IO(address))
    {
        read_access(0x00f00000, SIZE_BYTE);
    }
    cache_invalidate_range(ICACHE, address, 2);
}

void ps_write_32(unsigned int address, unsigned int data) {
    check_blit_active(address, 4);
    write_access(address, data, SIZE_LONG);
    if (SLOW_IO(address))
    {
        read_access(0x00f00000, SIZE_BYTE);
    }
    cache_invalidate_range(ICACHE, address, 4);
}

void ps_write_64(unsigned int address, uint64_t data) {
    check_blit_active(address, 8);
    write_access_64(address, data);
    if (SLOW_IO(address))
    {
        read_access(0x00f00000, SIZE_BYTE);
    }
    cache_invalidate_range(ICACHE, address, 8);
}

void ps_write_128(unsigned int address, uint128_t data) {
    check_blit_active(address, 16);
    write_access_128(address, data);
    if (SLOW_IO(address))
    {
        read_access(0x00f00000, SIZE_BYTE);
    }
    cache_invalidate_range(ICACHE, address, 16);
}

unsigned int ps_read_8(unsigned int address) {
    return read_access(address, SIZE_BYTE);
}

unsigned int ps_read_16(unsigned int address) {
    return read_access(address, SIZE_WORD);
}

unsigned int ps_read_32(unsigned int address) {
    return read_access(address, SIZE_LONG);
}

uint64_t ps_read_64(unsigned int address) {
    return read_access_64(address);
}

uint128_t ps_read_128(unsigned int address) {
    return read_access_128(address);
}

void ps_reset_state_machine() {
}

#include <boards.h>
extern struct ExpansionBoard **board;
extern struct ExpansionBoard *__boards_start;
extern int board_idx;
extern uint32_t overlay;

void ps_pulse_reset()
{
    kprintf("[PS32] Set REQUEST_BM\n");
    ps_set_control(CONTROL_REQ_BM);
    usleep(100000);

    kprintf("[PS32] Set DRIVE_RESET\n");
    ps_set_control(CONTROL_DRIVE_RESET);
    usleep(150000);

    kprintf("[PS32] Clear DRIVE_RESET\n");
    ps_clr_control(CONTROL_DRIVE_RESET);

    if (use_2slot)
        ps_set_control(CONTROL_INC_EXEC_SLOT);

    overlay = 1;
    board = &__boards_start;
    board_idx = 0;
}



void ps_efinix_setup()
{
    //set programming pins to output
    *(gpio + 0) = LE32(GO(PIN_CRESET1) | GO(PIN_CRESET2) | GO(PIN_CDI2) | GO(PIN_CDI3) | GO(PIN_CDI5));//GPIO 0..9
    *(gpio + 1) = LE32(GO(PIN_CDI0) | GO(PIN_CDI4) | GO(PIN_CDI7) | GO(PIN_CDI6) | GO(PIN_CBUS0) | GO(PIN_CBUS1) | GO(PIN_CBUS2) | GO(PIN_TESTN));//GPIO 10..19
    *(gpio + 2) = LE32(GO(PIN_CCK) | GO(PIN_CDI1) | GO(PIN_SS)); //GPIO 20..29

    //make sure FPGA is not in reset
    *(gpio + 7) = LE32((1 << PIN_CRESET1) | (1 << PIN_CRESET2));

    /*
    valid cbus options are
    x1 SPI => CBUS 3'b111
    x2 SPI => CBUS 3'b110
    x4 SPI => CBUS 3'b101
    x8 SPI => CBUS 3'b100
    */
    //set cbus (x1 spi)
    *(gpio + 7) = LE32((1 << PIN_CBUS0) | (1 << PIN_CBUS1)| (1 << PIN_CBUS2));

    //set other relevant pins for programming to correct level
    *(gpio + 7) = LE32((1 << PIN_TESTN) | (1 << PIN_CCK));
    *(gpio + 10) = LE32((1 << PIN_SS));

    //reset fpga, latching cbus and mode pins
    *(gpio + 10) = LE32((1 << PIN_CRESET1) | (1 << PIN_CRESET2));
    usleep(50000); //wait a bit for ps32-lite glitch filter RC to discharge
    *(gpio + 7) = LE32((1 << PIN_CRESET1) | (1 << PIN_CRESET2));
}

void ps_efinix_write(unsigned char data_out)
{
    //thats just bitbanged 1x SPI, takes ~100mS to load
    unsigned char loop, mask;
    for (loop=0,mask=0x80;loop<8;loop++, mask=mask>>1)
    {
        *(gpio + 10) = LE32(1 << PIN_CCK);
        *(gpio + 10) = LE32(1 << PIN_CCK);
        *(gpio + 10) = LE32(1 << PIN_CCK);
        if (data_out & mask) *(gpio + 7) = LE32(1 << PIN_CDI0);
        else *(gpio + 10) = LE32(1 << PIN_CDI0);
        *(gpio + 7) = LE32(1 << PIN_CCK);
        *(gpio + 7) = LE32(1 << PIN_CCK);
        *(gpio + 7) = LE32(1 << PIN_CCK);
        *(gpio + 7) = LE32(1 << PIN_CCK); //to get closer to 50/50 duty cycle
    }
    *(gpio + 10) = LE32(1 << PIN_CCK);
    *(gpio + 10) = LE32(1 << PIN_CCK);
    *(gpio + 10) = LE32(1 << PIN_CCK);
}

void ps_efinix_load(char* buffer, long length)
{
    long i;
    for (i = 0; i < length; ++i)
    {
        ps_efinix_write(buffer[i]);
    }

    //1000 dummy clocks for startup of user logic
    for (i = 0; i < 1000; ++i)
    {
        ps_efinix_write(0x00);
    }
}

#define BITBANG_DELAY PISTORM_BITBANG_DELAY

void bitbang_putByte(uint8_t byte)
{
    if (!gpio)
        gpio = ((volatile unsigned *)BCM2708_PERI_BASE) + GPIO_ADDR / 4;

    uint64_t t0 = 0, t1 = 0;
    asm volatile("mrs %0, CNTPCT_EL0":"=r"(t0));

    *(gpio + 10) = LE32(1 << SER_OUT_BIT); // Start bit - 0
  
    do {
        asm volatile("mrs %0, CNTPCT_EL0":"=r"(t1));
    } while(t1 < (t0 + BITBANG_DELAY));
  
    for (int i=0; i < 8; i++) {
        asm volatile("mrs %0, CNTPCT_EL0":"=r"(t0));

        if (byte & 1)
            *(gpio + 7) = LE32(1 << SER_OUT_BIT);
        else
            *(gpio + 10) = LE32(1 << SER_OUT_BIT);
        byte = byte >> 1;

        do {
            asm volatile("mrs %0, CNTPCT_EL0":"=r"(t1));
        } while(t1 < (t0 + BITBANG_DELAY));
    }
    asm volatile("mrs %0, CNTPCT_EL0":"=r"(t0));

    *(gpio + 7) = LE32(1 << SER_OUT_BIT);  // Stop bit - 1

    do {
        asm volatile("mrs %0, CNTPCT_EL0":"=r"(t1));
    } while(t1 < (t0 + 3*BITBANG_DELAY / 2));
}

void (*fs_putByte)(uint8_t);

void fastSerial_putByte_pi3(uint8_t byte)
{
    if (!gpio)
        gpio = ((volatile unsigned *)BCM2708_PERI_BASE) + GPIO_ADDR / 4;
  
    /* Start bit */
    *(gpio + 10) = LE32(1 << SER_OUT_BIT);

    /* Clock down */
    *(gpio + 10) = LE32(1 << SER_OUT_CLK);
    
    /* Clock up */
    *(gpio + 7) = LE32(1 << SER_OUT_CLK);

    for (int i=0; i < 8; i++) {
        if (byte & 1)
            *(gpio + 7) = LE32(1 << SER_OUT_BIT);
        else
            *(gpio + 10) = LE32(1 << SER_OUT_BIT);

        /* Clock down */
        *(gpio + 10) = LE32(1 << SER_OUT_CLK);
        
        /* Clock up */
        *(gpio + 7) = LE32(1 << SER_OUT_CLK);
        
        byte = byte >> 1;
    }

    /* DEST bit (0) */
    *(gpio + 10) = LE32(1 << SER_OUT_BIT);

    /* Clock down */
    *(gpio + 10) = LE32(1 << SER_OUT_CLK);

    /* Clock up */
    *(gpio + 7) = LE32(1 << SER_OUT_CLK);
    *(gpio + 7) = LE32(1 << SER_OUT_CLK);

    /* Leave FS_CLK and FS_DO high */
    *(gpio + 7) = LE32(1 << SER_OUT_CLK);
    *(gpio + 7) = LE32(1 << SER_OUT_BIT);
}

void fastSerial_putByte_pi4(uint8_t byte)
{
    if (!gpio)
        gpio = ((volatile unsigned *)BCM2708_PERI_BASE) + GPIO_ADDR / 4;
  
    /* Start bit */
    *(gpio + 10) = LE32(1 << SER_OUT_BIT);

    /* Clock down */
    *(gpio + 10) = LE32(1 << SER_OUT_CLK);
    *(gpio + 10) = LE32(1 << SER_OUT_CLK);
    *(gpio + 10) = LE32(1 << SER_OUT_CLK);
    /* Clock up */
    *(gpio + 7) = LE32(1 << SER_OUT_CLK);
    *(gpio + 7) = LE32(1 << SER_OUT_CLK);
    *(gpio + 7) = LE32(1 << SER_OUT_CLK);

    for (int i=0; i < 8; i++) {
        if (byte & 1)
            *(gpio + 7) = LE32(1 << SER_OUT_BIT);
        else
            *(gpio + 10) = LE32(1 << SER_OUT_BIT);

        /* Clock down */
        *(gpio + 10) = LE32(1 << SER_OUT_CLK);
        *(gpio + 10) = LE32(1 << SER_OUT_CLK);
        *(gpio + 10) = LE32(1 << SER_OUT_CLK);
        /* Clock up */
        *(gpio + 7) = LE32(1 << SER_OUT_CLK);
        *(gpio + 7) = LE32(1 << SER_OUT_CLK);
        *(gpio + 7) = LE32(1 << SER_OUT_CLK);
        
        byte = byte >> 1;
    }

    /* DEST bit (0) */
    *(gpio + 10) = LE32(1 << SER_OUT_BIT);

    /* Clock down */
    *(gpio + 10) = LE32(1 << SER_OUT_CLK);
    *(gpio + 10) = LE32(1 << SER_OUT_CLK);
    *(gpio + 10) = LE32(1 << SER_OUT_CLK);
    /* Clock up */
    *(gpio + 7) = LE32(1 << SER_OUT_CLK);
    *(gpio + 7) = LE32(1 << SER_OUT_CLK);
    *(gpio + 7) = LE32(1 << SER_OUT_CLK);

    /* Leave FS_CLK and FS_DO high */
    *(gpio + 7) = LE32(1 << SER_OUT_CLK);
    *(gpio + 7) = LE32(1 << SER_OUT_BIT);
}

void fastSerial_reset()
{
    if (!gpio)
        gpio = ((volatile unsigned *)BCM2708_PERI_BASE) + GPIO_ADDR / 4;

    /* Leave FS_CLK and FS_DO high */
    *(gpio + 7) = LE32(1 << SER_OUT_CLK);
    *(gpio + 7) = LE32(1 << SER_OUT_BIT);

    for (int i=0; i < 16; i++) {
        /* Clock down */
        *(gpio + 10) = LE32(1 << SER_OUT_CLK);
        *(gpio + 10) = LE32(1 << SER_OUT_CLK);
        *(gpio + 10) = LE32(1 << SER_OUT_CLK);
        /* Clock up */
        *(gpio + 7) = LE32(1 << SER_OUT_CLK);
        *(gpio + 7) = LE32(1 << SER_OUT_CLK);
        *(gpio + 7) = LE32(1 << SER_OUT_CLK);
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

    pistorm_setup_serial();

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

#define PM_RSTC         ((volatile unsigned int*)(0xf2000000 + 0x0010001c))
#define PM_RSTS         ((volatile unsigned int*)(0xf2000000 + 0x00100020))
#define PM_WDOG         ((volatile unsigned int*)(0xf2000000 + 0x00100024))
#define PM_WDOG_MAGIC   0x5a000000
#define PM_RSTC_FULLRST 0x00000020

volatile int housekeeper_enabled = 0;

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
    uint64_t tmp;
    asm volatile("mrs %0, CNTFRQ_EL0":"=r"(tmp));

    if (tmp > 20000000)
    {
        asm volatile("msr CNTKCTL_EL1, %0"::"r"(3 | (1 << 2) | (3 << 8) | (3 << 4)));
    }
    else
    {
        asm volatile("msr CNTKCTL_EL1, %0"::"r"(3 | (1 << 2) | (3 << 8) | (2 << 4)));
    }

    uint8_t pin_prev = LE32(*gpread);
    
    for(;;) {
        if (housekeeper_enabled)
        {
            //if (!__atomic_test_and_set(&gpio_lock, __ATOMIC_ACQUIRE))
            //{
            //    gpio_rdval = LE32(*gpread);
            //    __atomic_clear(&gpio_lock, __ATOMIC_RELEASE);
            //}

            uint32_t pin = LE32(*gpread);

            // Reall 680x0 CPU filters IPL lines in order to avoid false interrupts if
            // there is a clock skew between three IPL bits. We need to do the same.
            // Update IPL if and only if two subsequent IPL reads are the same.
            if ((pin & 7) == (pin_prev & 7))
            {
                __m68k_state->INT.IPL = ~pin & 7;

                asm volatile("":::"memory");

                if (__m68k_state->INT.IPL)
                    asm volatile("sev":::"memory");
            }

            pin_prev = pin;

            if ((pin & (1 << PIN_KBRESET)) == 0) {
                kprintf("[HKEEP] Houskeeper will reset RasPi now...\n");

                ps_set_control(CONTROL_REQ_BM);
                usleep(100000);

                ps_set_control(CONTROL_DRIVE_RESET);
                usleep(150000);

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

// Writeback buffer not implemented on PS32...

volatile unsigned char bus_lock = 0;

void wb_init()
{

}

void wb_task()
{

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


/* Membench */

void ps_memtest(unsigned int test_size)
{
    ps_write_8(0xbfe201, 0x0101);       //CIA OVL
    ps_write_8(0xbfe001, 0x0000);       //CIA OVL LOW

    int num_iter = 1;
    uint64_t clkspeed;
    uint64_t t0, t1;
    uint32_t ns;
    double result;

    asm volatile("mrs %0, CNTFRQ_EL0":"=r"(clkspeed));

    kprintf_pc(__putc, NULL, "MemBench with size %dK requested through commandline\n", test_size);
    test_size <<= 10;

    num_iter = 1;
    do {    
        asm volatile("mrs %0, CNTPCT_EL0":"=r"(t0));

        for (int iter = 0; iter < num_iter; iter++)
        {
            for (unsigned int addr = 0x1000; addr < test_size + 0x1000; addr++)
            {
                (void)ps_read_8(addr);
            }
        }

        asm volatile("mrs %0, CNTPCT_EL0":"=r"(t1));
        num_iter <<= 1;
    } while((t1 - t0) < clkspeed);

    result = (double)(t1 - t0) / (double)clkspeed;
    result = (double)test_size * (double)(num_iter >> 1) / result;
    ns = 1E9 / result;

    kprintf_pc(__putc, NULL, "  READ BYTE:  %5ld KB/s   %5ld ns\n", (unsigned int)result / 1024, ns);

    num_iter = 1;
    do {    
        asm volatile("mrs %0, CNTPCT_EL0":"=r"(t0));

        for (int iter = 0; iter < num_iter; iter++)
        {
            for (unsigned int addr = 0x1000; addr < test_size + 0x1000; addr+=2)
            {
                (void)ps_read_16(addr);
            }
        }

        asm volatile("mrs %0, CNTPCT_EL0":"=r"(t1));
        num_iter <<= 1;
    } while((t1 - t0) < clkspeed);

    result = (double)(t1 - t0) / (double)clkspeed;
    result = (double)test_size * (double)(num_iter >> 1) / result;
    ns = 2E9 / result;

    kprintf_pc(__putc, NULL, "  READ WORD:  %5ld KB/s   %5ld ns\n", (unsigned int)result / 1024, ns);

    num_iter = 1;
    do {    
        asm volatile("mrs %0, CNTPCT_EL0":"=r"(t0));

        for (int iter = 0; iter < num_iter; iter++)
        {
            for (unsigned int addr = 0x1000; addr < test_size + 0x1000; addr+=4)
            {
                (void)ps_read_32(addr);
            }
        }

        asm volatile("mrs %0, CNTPCT_EL0":"=r"(t1));
        num_iter <<= 1;
    } while((t1 - t0) < clkspeed);

    result = (double)(t1 - t0) / (double)clkspeed;
    result = (double)test_size * (double)(num_iter >> 1) / result;
    ns = 4E9 / result;

    kprintf_pc(__putc, NULL, "  READ LONG:  %5ld KB/s   %5ld ns\n", (unsigned int)result / 1024, ns);


    num_iter = 1;
    do {    
        asm volatile("mrs %0, CNTPCT_EL0":"=r"(t0));

        for (int iter = 0; iter < num_iter; iter++)
        {
            for (unsigned int addr = 0; addr < test_size; addr++)
            {
                (void)ps_write_8(addr, 0x00);
            }
        }

        asm volatile("mrs %0, CNTPCT_EL0":"=r"(t1));
        num_iter <<= 1;
    } while((t1 - t0) < clkspeed);

    result = (double)(t1 - t0) / (double)clkspeed;
    result = (double)test_size * (double)(num_iter >> 1) / result;
    ns = 1E9 / result;

    kprintf_pc(__putc, NULL, "  WRITE BYTE: %5ld KB/s   %5ld ns\n", (unsigned int)result / 1024, ns);

    num_iter = 1;
    do {    
        asm volatile("mrs %0, CNTPCT_EL0":"=r"(t0));

        for (int iter = 0; iter < num_iter; iter++)
        {
            for (unsigned int addr = 0; addr < test_size; addr+=2)
            {
                (void)ps_write_16(addr, 0);
            }
        }

        asm volatile("mrs %0, CNTPCT_EL0":"=r"(t1));
        num_iter <<= 1;
    } while((t1 - t0) < clkspeed);

    result = (double)(t1 - t0) / (double)clkspeed;
    result = (double)test_size * (double)(num_iter >> 1) / result;
    ns = 2E9 / result;

    kprintf_pc(__putc, NULL, "  WRITE WORD: %5ld KB/s   %5ld ns\n", (unsigned int)result / 1024, ns);

    num_iter = 1;
    do {    
        asm volatile("mrs %0, CNTPCT_EL0":"=r"(t0));

        for (int iter = 0; iter < num_iter; iter++)
        {
            for (unsigned int addr = 0; addr < test_size; addr+=4)
            {
                (void)ps_write_32(addr, 0);
            }
        }

        asm volatile("mrs %0, CNTPCT_EL0":"=r"(t1));
        num_iter <<= 1;
    } while((t1 - t0) < clkspeed);

    result = (double)(t1 - t0) / (double)clkspeed;
    result = (double)test_size * (double)(num_iter >> 1) / result;
    ns = 4E9 / result;

    kprintf_pc(__putc, NULL, "  WRITE LONG: %5ld KB/s   %5ld ns\n", (unsigned int)result / 1024, ns);
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

    write_access(0xbfe201, 0x0101, SIZE_BYTE);       //CIA OVL
    write_access(0xbfe001, 0x0000, SIZE_BYTE);       //CIA OVL LOW

    for (unsigned int iter = 0; iter < maxiter; iter++) {
        kprintf_pc(__putc, NULL, "Iteration %d...\n", iter + 1);

        // Fill the garbage buffer and chip ram with random data
        kprintf_pc(__putc, NULL, "  Writing BYTE garbage data to Chip...            ");
        for (uint32_t i = 0; i < test_size; i++) {
            uint8_t val = 0;
            val = rnd();
            garbage[i] = val;
            write_access(i, val, SIZE_BYTE);

            if ((i % (frac * 2)) == 0)
                kprintf_pc(__putc, NULL, "*");
        }

        for (uint32_t i = 0; i < test_size; i++) {
            uint32_t c = read_access(i, SIZE_BYTE);
            if (c != garbage[i]) {
                kprintf_pc(__putc, NULL, "\n    READ8: Garbege data mismatch at $%.6X: %.2X should be %.2X.\n", i, c, garbage[i]);
                while(1);
            }

            if ((i % (frac * 4)) == 0)
                kprintf_pc(__putc, NULL, "*");
        }

        for (uint32_t i = 0; i < (test_size) - 2; i += 2) {
            uint32_t c = BE16(read_access(i, SIZE_WORD));
            if (c != *((uint16_t *)&garbage[i])) {
                kprintf_pc(__putc, NULL, "\n    READ16_EVEN: Garbege data mismatch at $%.6X: %.4X should be %.4X.\n", i, c, *((uint16_t *)&garbage[i]));
                while(1);
            }

            if ((i % (frac * 4)) == 0)
                kprintf_pc(__putc, NULL, "*");
        }

        for (uint32_t i = 1; i < (test_size) - 2; i += 2) {
            uint32_t c = BE16(read_access(i, SIZE_WORD));
            if (c != *((uint16_t *)&garbage[i])) {
                kprintf_pc(__putc, NULL, "\n    READ16_ODD: Garbege data mismatch at $%.6X: %.4X should be %.4X.\n", i, c, *((uint16_t *)&garbage[i]));
                while(1);
            }

            if ((i % (frac * 4)) == 1)
                kprintf_pc(__putc, NULL, "*");
        }
        
        for (uint32_t i = 0; i < (test_size) - 4; i += 2) {
            uint32_t c = BE32(read_access(i, SIZE_LONG));
            if (c != *((uint32_t *)&garbage[i])) {
                kprintf_pc(__putc, NULL, "\n    READ32_EVEN: Garbege data mismatch at $%.6X: %.8X should be %.8X.\n", i, c, *((uint32_t *)&garbage[i]));
                while(1);
            }
            
            if ((i % (frac * 4)) == 0)
                kprintf_pc(__putc, NULL, "*");
        }

        for (uint32_t i = 1; i < (test_size) - 4; i += 2) {
            uint32_t c = BE32(read_access(i, SIZE_LONG));
            if (c != *((uint32_t *)&garbage[i])) {
                kprintf_pc(__putc, NULL, "\n    READ32_ODD: Garbege data mismatch at $%.6X: %.8X should be %.8X.\n", i, c, *((uint32_t *)&garbage[i]));
                while(1);
            }

            if ((i % (frac * 4)) == 1)
                kprintf_pc(__putc, NULL, "*");
        }

        for (uint32_t i = 0; i < test_size; i++) {
            write_access(i, (uint32_t)0x0, SIZE_BYTE);

            if ((i % (frac * 8)) == 0)
                kprintf_pc(__putc, NULL, "*");
        }

        kprintf_pc(__putc, NULL, "\n  Writing WORD garbage data to Chip, unaligned... ");
        for (uint32_t i = 1; i < (test_size) - 2; i += 2) {
            uint16_t v = *((uint16_t *)&garbage[i]);
            write_access(i + 1, (v & 0x00FF), SIZE_BYTE);
            write_access(i, (v >> 8), SIZE_BYTE);

            if ((i % (frac * 2)) == 1)
                kprintf_pc(__putc, NULL, "*");
        }

        for (uint32_t i = 1; i < (test_size) - 2; i += 2) {
            uint32_t c = BE16((read_access(i, SIZE_BYTE) << 8) | read_access(i + 1, SIZE_BYTE));
            if (c != *((uint16_t *)&garbage[i])) {
                kprintf_pc(__putc, NULL, "\n    READ16_ODD: Garbege data mismatch at $%.6X: %.4X should be %.4X.\n", i, c, *((uint16_t *)&garbage[i]));
                while(1);
            }

            if ((i % (frac * 2)) == 1)
                kprintf_pc(__putc, NULL, "*");
        }

        for (uint32_t i = 0; i < test_size; i++) {
            write_access(i, (uint32_t)0x0, SIZE_BYTE);
        }

        kprintf_pc(__putc, NULL, "\n  Writing LONG garbage data to Chip, unaligned... ");
        for (uint32_t i = 1; i < (test_size) - 4; i += 4) {
            uint32_t v = *((uint32_t *)&garbage[i]);
            write_access(i , v & 0x0000FF, SIZE_BYTE);
            write_access(i + 1, BE16(((v & 0x00FFFF00) >> 8)), SIZE_WORD);
            write_access(i + 3 , (v & 0xFF000000) >> 24, SIZE_BYTE);

            if ((i % (frac * 2)) == 1)
                kprintf_pc(__putc, NULL, "*");
        }

        for (uint32_t i = 1; i < (test_size) - 4; i += 4) {
            uint32_t c = read_access(i, SIZE_BYTE);
            c |= (BE16(read_access(i + 1, SIZE_WORD)) << 8);
            c |= (read_access(i + 3, SIZE_BYTE) << 24);
            if (c != *((uint32_t *)&garbage[i])) {
                kprintf_pc(__putc, NULL, "\n    READ32_ODD: Garbege data mismatch at $%.6X: %.8X should be %.8X.\n", i, c, *((uint32_t *)&garbage[i]));
                while(1);
            }

            if ((i % (frac * 2)) == 1)
                kprintf_pc(__putc, NULL, "*");
        }

        kprintf_pc(__putc, NULL, "\n  Writing QUAD garbage data to Chip... ");
        for (uint32_t i = 0; i < (test_size) - 8; i += 8) {
            uint64_t v = *((uint64_t *)&garbage[i]);
            write_access_64(i , v);

            if ((i % (frac * 2)) == 1)
                kprintf_pc(__putc, NULL, "*");
        }

        for (uint32_t i = 1; i < (test_size) - 16; i += 8) {
            uint64_t c = read_access_64(i);
            if (c != *((uint64_t *)&garbage[i])) {
                kprintf_pc(__putc, NULL, "\n    READ64_ODD: Garbege data mismatch at $%.6X: %.16X should be %.16X.\n", i, c, *((uint64_t *)&garbage[i]));
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
