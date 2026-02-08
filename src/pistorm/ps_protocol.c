// SPDX-License-Identifier: MIT

/*
 * Copyright 2022 Claude Schwarz
 * Copyright 2022 Niklas Ekström
 * Copyright 2022-2025 Michal Schulz (Emu68 adaptations)
 */

#undef PS_PROTOCOL_IMPL

#include <stdint.h>

#include "config.h"
#include "support.h"
#include "tlsf.h"
#include "ps_protocol.h"
#include "M68k.h"
#include "cache.h"
#include "intc.h"

extern struct M68KState *__m68k_state;

static inline uint64_t get_microseconds()
{
    volatile struct
    {
        uint32_t LO;
        uint32_t HI;
    } *CLOCK = (volatile void *)0xf2003004;

    uint64_t hi = CLOCK->HI;
    uint64_t lo = CLOCK->LO;
    uint64_t t;

    if (unlikely(hi != CLOCK->HI))
    {
        hi = CLOCK->HI;
        lo = CLOCK->LO;
    }

    t = LE64(hi) | LE32(lo);
    
    return t;
}

static void usleep(uint64_t delta)
{
    volatile struct
    {
        uint32_t LO;
        uint32_t HI;
    } *CLOCK = (volatile void *)0xf2003004;

    uint64_t hi = CLOCK->HI;
    uint64_t lo = CLOCK->LO;
    uint64_t t1, t2;

    if (unlikely(hi != CLOCK->HI))
    {
        hi = CLOCK->HI;
        lo = CLOCK->LO;
    }

    t1 = LE64(hi) | LE32(lo);
    t1 += delta;
    t2 = 0;

    do
    {
        hi = CLOCK->HI;
        lo = CLOCK->LO;
        if (unlikely(hi != CLOCK->HI))
        {
            hi = CLOCK->HI;
            lo = CLOCK->LO;
        }
        t2 = LE64(hi) | LE32(lo);
    } while (t2 < t1);
}

static inline void ticksleep(uint64_t ticks)
{
    uint64_t t0 = 0, t1 = 0;
    asm volatile("mrs %0, CNTPCT_EL0" : "=r"(t0));
    t0 += ticks;
    do
    {
        asm volatile("mrs %0, CNTPCT_EL0" : "=r"(t1));
    } while (t1 < t0);
}

static inline void ticksleep_wfe(uint64_t ticks)
{
    uint64_t t0 = 0, t1 = 0;
    asm volatile("mrs %0, CNTPCT_EL0" : "=r"(t0));
    t0 += ticks;
    do
    {
        asm volatile("mrs %0, CNTPCT_EL0" : "=r"(t1));
        asm volatile("wfe");
    } while (t1 < t0);
}

// REG_STATUS
#define STATUS_IS_BM (1 << 0)
#define STATUS_RESET (1 << 1)
#define STATUS_HALT (1 << 2)
#define STATUS_IPL_MASK (7 << 3)
#define STATUS_IPL_SHIFT 3
#define STATUS_TERM_NORMAL (1 << 6)
#define STATUS_REQ_ACTIVE (1 << 7)

// REG_CONTROL
#define CONTROL_REQ_BM (1 << 0)
#define CONTROL_DRIVE_RESET (1 << 1)
#define CONTROL_DRIVE_HALT (1 << 2)
#define CONTROL_DRIVE_INT2 (1 << 3)
#define CONTROL_DRIVE_INT6 (1 << 4)

#define IN 0
#define OUT 1
#define AF0 4
#define AF1 5
#define AF2 6
#define AF3 7
#define AF4 3
#define AF5 2

#define FUNC(pin, x) (((x) & 0x7) << (((pin) % 10) * 3))
#define REG(pin) ((pin) / 10)
#define MASK(pin) (7 << (((pin) % 10) * 3))

#define PF(f, i) ((f) << (((i) % 10) * 3)) // Pin function.
#define GO(i) PF(1, (i))                   // GPIO output.

#define PIN_IPL0 0
#define PIN_IPL1 1
#define PIN_IPL2 2
#define PIN_TXN 3
#define PIN_KBRESET 4
#define SER_OUT_BIT 5 // debug EMU68
#define PIN_RD 6
#define PIN_WR 7
#define PIN_D(x) (8 + x)
#define PIN_A(x) (24 + x)
#define SER_OUT_CLK 27 // debug EMU68

// Pins for MDIO
#define PIN_RGMII_MDIO 28
#define PIN_RGMII_MDC 29

#define CLEAR_BITS_PI3  (0x0fffffff & ~((1 << PIN_RD) | (1 << PIN_WR) | (1 << SER_OUT_BIT) | (1 << SER_OUT_CLK)))
#define CLEAR_BITS_PI4  (0x0fffffff & ~((1 << PIN_RD) | (1 << PIN_WR) | (1 << SER_OUT_BIT) | (1 << SER_OUT_CLK) | (1 << PIN_RGMII_MDIO) | (1 << PIN_RGMII_MDC)))

// Pins for FPGA programming
#define PIN_CRESET1 6
#define PIN_CRESET2 7
#define PIN_TESTN 17
#define PIN_CSI 27
#define PIN_CCK 22
#define PIN_CDI0 10
#define PIN_SS 24
#define PIN_CBUS0 14
#define PIN_CBUS1 15
#define PIN_CBUS2 18

#define GPFSEL0_INPUT_PI3 (GO(PIN_WR) | GO(PIN_RD) | GO(SER_OUT_BIT))
#define GPFSEL1_INPUT_PI3 (0)
#define GPFSEL2_INPUT_PI3 (GO(29) | GO(PIN_A(2)) | GO(PIN_A(1)) | GO(PIN_A(0)) | GO(SER_OUT_CLK))

#define GPFSEL0_OUTPUT_PI3 (GO(PIN_D(1)) | GO(PIN_D(0)) | GO(PIN_WR) | GO(PIN_RD) | GO(SER_OUT_BIT))
#define GPFSEL1_OUTPUT_PI3 (GO(PIN_D(11)) | GO(PIN_D(10)) | GO(PIN_D(9)) | GO(PIN_D(8)) | GO(PIN_D(7)) | GO(PIN_D(6)) | GO(PIN_D(5)) | GO(PIN_D(4)) | GO(PIN_D(3)) | GO(PIN_D(2)))
#define GPFSEL2_OUTPUT_PI3 (GO(29) | GO(PIN_A(2)) | GO(PIN_A(1)) | GO(PIN_A(0)) | GO(PIN_D(15)) | GO(PIN_D(14)) | GO(PIN_D(13)) | GO(PIN_D(12)) | GO(SER_OUT_CLK))

#define GPFSEL0_INPUT_PI4 (GO(PIN_WR) | GO(PIN_RD) | GO(SER_OUT_BIT))
#define GPFSEL1_INPUT_PI4 (0)
#define GPFSEL2_INPUT_PI4 (GO(PIN_A(2)) | GO(PIN_A(1)) | GO(PIN_A(0)) | GO(SER_OUT_CLK) | PF(AF5, PIN_RGMII_MDC) | PF(AF5, PIN_RGMII_MDIO))

#define GPFSEL0_OUTPUT_PI4 (GO(PIN_D(1)) | GO(PIN_D(0)) | GO(PIN_WR) | GO(PIN_RD) | GO(SER_OUT_BIT))
#define GPFSEL1_OUTPUT_PI4 (GO(PIN_D(11)) | GO(PIN_D(10)) | GO(PIN_D(9)) | GO(PIN_D(8)) | GO(PIN_D(7)) | GO(PIN_D(6)) | GO(PIN_D(5)) | GO(PIN_D(4)) | GO(PIN_D(3)) | GO(PIN_D(2)))
#define GPFSEL2_OUTPUT_PI4 (GO(PIN_A(2)) | GO(PIN_A(1)) | GO(PIN_A(0)) | GO(PIN_D(15)) | GO(PIN_D(14)) | GO(PIN_D(13)) | GO(PIN_D(12)) | GO(SER_OUT_CLK) | PF(AF5, PIN_RGMII_MDC) | PF(AF5, PIN_RGMII_MDIO))

#define REG_DATA_LO 0
#define REG_DATA_HI 1
#define REG_ADDR_LO 2
#define REG_ADDR_HI 3
#define REG_STATUS 4
#define REG_CONTROL 4
#define REG_VERSION 7

#define TXN_SIZE_SHIFT 8
#define TXN_RW_SHIFT 10
#define TXN_FC_SHIFT 11

#define SIZE_BYTE 0
#define SIZE_WORD 1
#define SIZE_LONG 3

#define TXN_READ (1 << TXN_RW_SHIFT)
#define TXN_WRITE (0 << TXN_RW_SHIFT)

#define CLK_PASSWD 0x5a000000
#define CLK_GP0_CTL 0x070
#define CLK_GP0_DIV 0x074

#define DIRECTION_INPUT 0
#define DIRECTION_OUTPUT 1

#define BCM2708_PERI_BASE 0xF2000000
#define BCM2708_PERI_SIZE 0x01000000

#define GPIO_ADDR 0x200000
#define GPCLK_ADDR 0x101000

volatile struct GPIO_s
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

uint32_t INPUT[3] = {
    0, 0, 0
};

uint32_t OUTPUT[3] = {
    0, 0, 0
};

uint32_t CLEAR_BITS = 0;

uint32_t use_2slot = 1;

static int next_slot = 0;
static int slot_active[2] = {0, 0};
#define REG_SLOT 5
#define CONTROL_INC_EXEC_SLOT (1 << 5)

static inline void set_input()
{
    GPIO->GPFSEL0 = INPUT[0];
    GPIO->GPFSEL1 = INPUT[1];
    GPIO->GPFSEL2 = INPUT[2];
}

static inline void set_output()
{
    GPIO->GPFSEL0 = OUTPUT[0];
    GPIO->GPFSEL1 = OUTPUT[1];
    GPIO->GPFSEL2 = OUTPUT[2];
}

static inline uint32_t wait_txn()
{
    uint32_t data;
    while ((data = GPIO->GPLEV0) & LE32(1 << PIN_TXN)) {}
    return LE32(data);
}

static inline uint32_t read_ps_reg_ps16(uint32_t address)
{
    GPIO->GPSET0 = LE32(address << PIN_A(0));

    // Delay for Pi3, 3*7.5nS , or 3*3.5nS for
    GPIO->GPCLR0 = LE32(1 << PIN_RD);
    GPIO->GPCLR0 = LE32(1 << PIN_RD);
    GPIO->GPCLR0 = LE32(1 << PIN_RD);

    // More conservative behavior - read the GPLEV0 twice
    uint32_t data = LE32(GPIO->GPLEV0);
    //data = LE32(GPIO->GPLEV0);

    GPIO->GPSET0 = LE32(1 << PIN_RD);
    GPIO->GPCLR0 = CLEAR_BITS;

    data = (data >> PIN_D(0)) & 0xffff;

    return data;
}

static inline uint32_t read_ps_reg_ps32(uint32_t address)
{
    GPIO->GPSET0 = LE32(address << PIN_A(0));

    // Delay for Pi3, 3*7.5nS , or 3*3.5nS for
    GPIO->GPCLR0 = LE32(1 << PIN_RD);
    GPIO->GPCLR0 = LE32(1 << PIN_RD);
    GPIO->GPCLR0 = LE32(1 << PIN_RD);

    uint32_t data = LE32(GPIO->GPLEV0);
    data = LE32(GPIO->GPLEV0);

    GPIO->GPSET0 = LE32(1 << PIN_RD);
    GPIO->GPCLR0 = CLEAR_BITS;

    data = (data >> PIN_D(0)) & 0xffff;

    return data;
}

static inline uint32_t read_ps_reg_with_wait_ps16(uint32_t address)
{
    GPIO->GPSET0 = LE32(address << PIN_A(0));

    // Delay for Pi3, 3*7.5nS , or 3*3.5nS for
    GPIO->GPCLR0 = LE32(1 << PIN_RD);
    GPIO->GPCLR0 = LE32(1 << PIN_RD);
    GPIO->GPCLR0 = LE32(1 << PIN_RD);

    wait_txn();

    // More conservative behavior - read the GPLEV0 twice
    uint32_t data = LE32(GPIO->GPLEV0);

    GPIO->GPSET0 = LE32(1 << PIN_RD);
    GPIO->GPCLR0 = CLEAR_BITS;

    data = (data >> PIN_D(0)) & 0xffff;

    return data;
}

static inline uint32_t read_ps_reg_with_wait_ps32(uint32_t address)
{
    GPIO->GPSET0 = LE32(address << PIN_A(0));

    // Delay for Pi3, 3*7.5nS , or 3*3.5nS for
    GPIO->GPCLR0 = LE32(1 << PIN_RD);
    GPIO->GPCLR0 = LE32(1 << PIN_RD);
    GPIO->GPCLR0 = LE32(1 << PIN_RD);

    wait_txn();

    uint32_t data = LE32(GPIO->GPLEV0);

    GPIO->GPSET0 = LE32(1 << PIN_RD);
    GPIO->GPCLR0 = CLEAR_BITS;

    data = (data >> PIN_D(0)) & 0xffff;

    return data;
}

static inline void write_ps_reg_ps16(uint32_t address, uint16_t data)
{
    GPIO->GPSET0 = LE32((data << PIN_D(0)) | (address << PIN_A(0)));

    // Delay for Pi4, 2*3.5nS
    // More conservative behavior do 4 writes instead of one here
    GPIO->GPCLR0 = LE32(1 << PIN_WR);
    GPIO->GPCLR0 = LE32(1 << PIN_WR);
    GPIO->GPCLR0 = LE32(1 << PIN_WR);
    GPIO->GPCLR0 = LE32(1 << PIN_WR);

    GPIO->GPSET0 = LE32(1 << PIN_WR);
    GPIO->GPCLR0 = CLEAR_BITS;

    // If writing to REG_ADDR_HI then issue one extra read from GPIO to
    // give firmware time to start rolling!
    if (address == REG_ADDR_HI)
    {
        (void)GPIO->GPLEV0;
    }
}

static inline void write_ps_reg_ps32(uint32_t address, uint16_t data)
{
    GPIO->GPSET0 = LE32((data << PIN_D(0)) | (address << PIN_A(0)));

    // Delay for Pi4, 2*3.5nS
    GPIO->GPCLR0 = LE32(1 << PIN_WR);
    GPIO->GPCLR0 = LE32(1 << PIN_WR);
    GPIO->GPCLR0 = LE32(1 << PIN_WR);

    GPIO->GPSET0 = LE32(1 << PIN_WR);
    GPIO->GPCLR0 = CLEAR_BITS;
}

// Pointers to functions talking with PiStorm - selected depending on detected
// PiStorm model

void (*write_ps_reg)(uint32_t address, uint16_t data);
uint32_t (*read_ps_reg)(uint32_t address);
uint32_t (*read_ps_reg_with_wait)(uint32_t address);

void ps_set_control(uint32_t value)
{
    set_output();
    write_ps_reg(REG_CONTROL, 0x8000 | (value & 0x7fff));
    set_input();
}

void ps_clr_control(uint32_t value)
{
    set_output();
    write_ps_reg(REG_CONTROL, value & 0x7fff);
    set_input();
}

void ps_efinix_setup(uint8_t pistorm_model)
{
    // set programming pins to output
    GPIO->GPFSEL0 = LE32(GO(PIN_CRESET1) | GO(PIN_CRESET2)); // GPIO 0..9
    // PiStorm16 needs only CDI0
    GPIO->GPFSEL1 = LE32(GO(PIN_CDI0) | GO(PIN_CBUS0) | GO(PIN_CBUS1) | GO(PIN_CBUS2) | GO(PIN_TESTN)); // GPIO 10..19
    // PiStorm16 needs only CCK and CSI, PiStorm32 needs only CCK and SS
    GPIO->GPFSEL2 = LE32(GO(PIN_CCK) | GO(PIN_SS) | GO(PIN_CSI));   // GPIO 20..29

    // make sure FPGA is not in reset
    GPIO->GPSET0 = LE32((1 << PIN_CRESET1) | (1 << PIN_CRESET2));

    if (pistorm_model == PISTORM_MODEL_32) {
        // set cbus (x1 spi)
        GPIO->GPSET0 = LE32((1 << PIN_CBUS0) | (1 << PIN_CBUS1) | (1 << PIN_CBUS2));

        // set other relevant pins for programming to correct level
        GPIO->GPSET0 = LE32((1 << PIN_TESTN) | (1 << PIN_CCK));
        GPIO->GPCLR0 = LE32((1 << PIN_SS));
    }
    else if (pistorm_model == PISTORM_MODEL_16) {
        // FPGA is pre-configured to 1x SPI mode, passive:
        // CBUS[2:0] = 3'b111
        // SS = b0

        // set other relevant pins for programming to correct level
        GPIO->GPSET0 = LE32((1 << PIN_CSI) | (1 << PIN_CCK));
    }
    
    // reset fpga, latching cbus and mode pins
    ps_efinix_reset();
}

void ps_efinix_write(uint8_t data_out)
{
    // thats just bitbanged 1x SPI, takes ~100mS to load
    for(uint8_t mask = 0x80; mask != 0; mask >>= 1)
    {
        GPIO->GPCLR0 = LE32(1 << PIN_CCK);
        GPIO->GPCLR0 = LE32(1 << PIN_CCK);
        GPIO->GPCLR0 = LE32(1 << PIN_CCK);
        if (data_out & mask)
            GPIO->GPSET0 = LE32(1 << PIN_CDI0);
        else
            GPIO->GPCLR0 = LE32(1 << PIN_CDI0);
        GPIO->GPSET0 = LE32(1 << PIN_CCK);
        GPIO->GPSET0 = LE32(1 << PIN_CCK);
        GPIO->GPSET0 = LE32(1 << PIN_CCK);
        GPIO->GPSET0 = LE32(1 << PIN_CCK); // to get closer to 50/50 duty cycle
    }
    GPIO->GPCLR0 = LE32(1 << PIN_CCK);
    GPIO->GPCLR0 = LE32(1 << PIN_CCK);
    GPIO->GPCLR0 = LE32(1 << PIN_CCK);
}

void ps_efinix_reset()
{
    uint32_t old_GPFSEL0 = LE32(GPIO->GPFSEL0);
    uint32_t new_GPFSEL0 = old_GPFSEL0 & ~(PF(7, PIN_CRESET1) | PF(7, PIN_CRESET2));
    
    // New GPFSEL shall have CRESET1 and CRESET2 as outputs
    new_GPFSEL0 |= GO(PIN_CRESET1) | GO(PIN_CRESET2);
    
    // Set new GPFSEL0 with enabled CRESET pins
    GPIO->GPFSEL0 = LE32(new_GPFSEL0);

    // reset fpga, latching cbus and mode pins
    GPIO->GPCLR0 = LE32((1 << PIN_CRESET1) | (1 << PIN_CRESET2));
    usleep(50000); // wait a bit for ps32-lite glitch filter RC to discharge
    GPIO->GPSET0 = LE32((1 << PIN_CRESET1) | (1 << PIN_CRESET2));

    // Restore old GPFSEL0 value
    GPIO->GPFSEL0 = LE32(old_GPFSEL0);
}

void ps_efinix_load(char *buffer, long length)
{
    long i;
    for (i = 0; i < length; ++i)
    {
        ps_efinix_write(buffer[i]);
    }

    // 1000 dummy clocks for startup of user logic
    for (i = 0; i < 1000; ++i)
    {
        ps_efinix_write(0x00);
    }
}

static uint8_t pistorm_model;
static uint8_t pistorm_model_set = 0;

void force_pistorm_model(uint8_t model)
{
    pistorm_model = model;
    pistorm_model_set = 1;
}

// Guessing PiStorm model. Activate Pull-Up on GPIO17 and GPIO24. Then read
// The value of this IO pins. Following values are expected
//
//              GPIO17   GPIO24
// PiStorm32:     Hi       Low
// PiStorm16:     Hi       Hi

uint8_t pistorm_get_model()
{
    if (pistorm_model_set)
        return pistorm_model;

    uint32_t io;

    pistorm_model = 0;

    set_input();

    ps_efinix_reset();

    /* GPIO24 and GPIO17 must be set to input */
    GPIO->GPFSEL1 &= ~LE32(PF(7, 17));
    GPIO->GPFSEL2 &= ~LE32(PF(7, 24));

    // Set GPIO17 and GPIO24 pull-ups
    GPIO->GPIO_PUP_PDN_CNTRL_REG1 &= ~LE32((3 << 2) | (3 << 16));
    GPIO->GPIO_PUP_PDN_CNTRL_REG1 |= LE32((1 << 2) | (1 << 16));

    usleep(10000);

    // Read level on GPIO
    io = GPIO->GPLEV0;

    // Reset GPIO to input again
    set_input();

    if (io & LE32(1 << 17))
    {
        pistorm_model |= 1;
    }
    if (io & LE32(1 << 24))
    {
        pistorm_model |= 2;
    }

    pistorm_model_set = 1;

    return pistorm_model;
}

#define BITBANG_DELAY PISTORM_BITBANG_DELAY

void bitbang_putByte(uint8_t byte)
{
    uint64_t t0 = 0, t1 = 0;
    asm volatile("mrs %0, CNTPCT_EL0" : "=r"(t0));

    GPIO->GPCLR0 = LE32(1 << SER_OUT_BIT); // Start bit - 0

    do
    {
        asm volatile("mrs %0, CNTPCT_EL0" : "=r"(t1));
    } while (t1 < (t0 + BITBANG_DELAY));

    for (int i = 0; i < 8; i++)
    {
        asm volatile("mrs %0, CNTPCT_EL0" : "=r"(t0));

        if (byte & 1)
            GPIO->GPSET0 = LE32(1 << SER_OUT_BIT);
        else
            GPIO->GPCLR0 = LE32(1 << SER_OUT_BIT);
        byte = byte >> 1;

        do
        {
            asm volatile("mrs %0, CNTPCT_EL0" : "=r"(t1));
        } while (t1 < (t0 + BITBANG_DELAY));
    }
    asm volatile("mrs %0, CNTPCT_EL0" : "=r"(t0));

    GPIO->GPSET0 = LE32(1 << SER_OUT_BIT); // Stop bit - 1

    do
    {
        asm volatile("mrs %0, CNTPCT_EL0" : "=r"(t1));
    } while (t1 < (t0 + 3 * BITBANG_DELAY / 2));
}

void (*fs_putByte)(uint8_t);

void fastSerial_putByte_pi3(uint8_t byte)
{
    /* Start bit */
    GPIO->GPCLR0 = LE32(1 << SER_OUT_BIT);

    /* Clock down */
    GPIO->GPCLR0 = LE32(1 << SER_OUT_CLK);

    /* Clock up */
    GPIO->GPSET0 = LE32(1 << SER_OUT_CLK);

    for (int i = 0; i < 8; i++)
    {
        if (byte & 1)
            GPIO->GPSET0 = LE32(1 << SER_OUT_BIT);
        else
            GPIO->GPCLR0 = LE32(1 << SER_OUT_BIT);

        /* Clock down */
        GPIO->GPCLR0 = LE32(1 << SER_OUT_CLK);

        /* Clock up */
        GPIO->GPSET0 = LE32(1 << SER_OUT_CLK);

        byte = byte >> 1;
    }

    /* DEST bit (0) */
    GPIO->GPCLR0 = LE32(1 << SER_OUT_BIT);

    /* Clock down */
    GPIO->GPCLR0 = LE32(1 << SER_OUT_CLK);

    /* Clock up */
    GPIO->GPSET0 = LE32(1 << SER_OUT_CLK);
    GPIO->GPSET0 = LE32(1 << SER_OUT_CLK);

    /* Leave FS_CLK and FS_DO high */
    GPIO->GPSET0 = LE32(1 << SER_OUT_CLK);
    GPIO->GPSET0 = LE32(1 << SER_OUT_BIT);
}

void fastSerial_putByte_pi4(uint8_t byte)
{
    /* Start bit */
    GPIO->GPCLR0 = LE32(1 << SER_OUT_BIT);

    /* Clock down */
    GPIO->GPCLR0 = LE32(1 << SER_OUT_CLK);
    GPIO->GPCLR0 = LE32(1 << SER_OUT_CLK);
    GPIO->GPCLR0 = LE32(1 << SER_OUT_CLK);
    /* Clock up */
    GPIO->GPSET0 = LE32(1 << SER_OUT_CLK);
    GPIO->GPSET0 = LE32(1 << SER_OUT_CLK);
    GPIO->GPSET0 = LE32(1 << SER_OUT_CLK);

    for (int i = 0; i < 8; i++)
    {
        if (byte & 1)
            GPIO->GPSET0 = LE32(1 << SER_OUT_BIT);
        else
            GPIO->GPCLR0 = LE32(1 << SER_OUT_BIT);

        /* Clock down */
        GPIO->GPCLR0 = LE32(1 << SER_OUT_CLK);
        GPIO->GPCLR0 = LE32(1 << SER_OUT_CLK);
        GPIO->GPCLR0 = LE32(1 << SER_OUT_CLK);
        /* Clock up */
        GPIO->GPSET0 = LE32(1 << SER_OUT_CLK);
        GPIO->GPSET0 = LE32(1 << SER_OUT_CLK);
        GPIO->GPSET0 = LE32(1 << SER_OUT_CLK);

        byte = byte >> 1;
    }

    /* DEST bit (0) */
    GPIO->GPCLR0 = LE32(1 << SER_OUT_BIT);

    /* Clock down */
    GPIO->GPCLR0 = LE32(1 << SER_OUT_CLK);
    GPIO->GPCLR0 = LE32(1 << SER_OUT_CLK);
    GPIO->GPCLR0 = LE32(1 << SER_OUT_CLK);
    /* Clock up */
    GPIO->GPSET0 = LE32(1 << SER_OUT_CLK);
    GPIO->GPSET0 = LE32(1 << SER_OUT_CLK);
    GPIO->GPSET0 = LE32(1 << SER_OUT_CLK);

    /* Leave FS_CLK and FS_DO high */
    GPIO->GPSET0 = LE32(1 << SER_OUT_CLK);
    GPIO->GPSET0 = LE32(1 << SER_OUT_BIT);
}

void fastSerial_reset()
{
    /* Leave FS_CLK and FS_DO high */
    GPIO->GPSET0 = LE32(1 << SER_OUT_CLK);
    GPIO->GPSET0 = LE32(1 << SER_OUT_BIT);

    for (int i = 0; i < 16; i++)
    {
        /* Clock down */
        GPIO->GPCLR0 = LE32(1 << SER_OUT_CLK);
        GPIO->GPCLR0 = LE32(1 << SER_OUT_CLK);
        GPIO->GPCLR0 = LE32(1 << SER_OUT_CLK);
        /* Clock up */
        GPIO->GPSET0 = LE32(1 << SER_OUT_CLK);
        GPIO->GPSET0 = LE32(1 << SER_OUT_CLK);
        GPIO->GPSET0 = LE32(1 << SER_OUT_CLK);
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

    if (fs_putByte)
        fs_putByte(byte);

    if (byte == 10)
        reset_pending = 1;
}

void pistorm_setup_serial()
{
    uint32_t fsel;

    switch (REG(SER_OUT_BIT))
    {
        case 0:
            fsel = GPIO->GPFSEL0;
            fsel &= LE32(~MASK(SER_OUT_BIT));
            fsel |= LE32(FUNC(SER_OUT_BIT, OUT));
            GPIO->GPFSEL0 = fsel;
            break;
        case 1:
            fsel = GPIO->GPFSEL1;
            fsel &= LE32(~MASK(SER_OUT_BIT));
            fsel |= LE32(FUNC(SER_OUT_BIT, OUT));
            GPIO->GPFSEL1 = fsel;
            break;
        case 2:
            fsel = GPIO->GPFSEL2;
            fsel &= LE32(~MASK(SER_OUT_BIT));
            fsel |= LE32(FUNC(SER_OUT_BIT, OUT));
            GPIO->GPFSEL2 = fsel;
            break;
    }

    switch (REG(SER_OUT_CLK))
    {
        case 0:
            fsel = GPIO->GPFSEL0;
            fsel &= LE32(~MASK(SER_OUT_CLK));
            fsel |= LE32(FUNC(SER_OUT_CLK, OUT));
            GPIO->GPFSEL0 = fsel;
            break;
        case 1:
            fsel = GPIO->GPFSEL1;
            fsel &= LE32(~MASK(SER_OUT_CLK));
            fsel |= LE32(FUNC(SER_OUT_CLK, OUT));
            GPIO->GPFSEL1 = fsel;
            break;
        case 2:
            fsel = GPIO->GPFSEL2;
            fsel &= LE32(~MASK(SER_OUT_CLK));
            fsel |= LE32(FUNC(SER_OUT_CLK, OUT));
            GPIO->GPFSEL2 = fsel;
            break;
    }
}

static void pistorm_setup_io()
{
    uint64_t tmp;
    pistorm_setup_serial();

    asm volatile("mrs %0, CNTFRQ_EL0" : "=r"(tmp));

    if (tmp > 20000000)
    {
        CLEAR_BITS = LE32(CLEAR_BITS_PI4);
        
        OUTPUT[0] = LE32(GPFSEL0_OUTPUT_PI4);
        OUTPUT[1] = LE32(GPFSEL1_OUTPUT_PI4);
        OUTPUT[2] = LE32(GPFSEL2_OUTPUT_PI4);
        
        INPUT[0] = LE32(GPFSEL0_INPUT_PI4);
        INPUT[1] = LE32(GPFSEL1_INPUT_PI4);
        INPUT[2] = LE32(GPFSEL2_INPUT_PI4);
    }
    else
    {
        CLEAR_BITS = LE32(CLEAR_BITS_PI3);
        
        OUTPUT[0] = LE32(GPFSEL0_OUTPUT_PI3);
        OUTPUT[1] = LE32(GPFSEL1_OUTPUT_PI3);
        OUTPUT[2] = LE32(GPFSEL2_OUTPUT_PI3);
        
        INPUT[0] = LE32(GPFSEL0_INPUT_PI3);
        INPUT[1] = LE32(GPFSEL1_INPUT_PI3);
        INPUT[2] = LE32(GPFSEL2_INPUT_PI3);
    }
}

void fastSerial_init()
{
    uint64_t tmp;

    pistorm_setup_serial();

    asm volatile("mrs %0, CNTFRQ_EL0" : "=r"(tmp));

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

static int write_pending = 0;
static uint8_t g_fc = 0;

#define SLOW_IO(address) ((address) >= 0xDFF09A && (address) < 0xDFF09E)

static inline void check_blit_active(unsigned int addr, unsigned int size) {
    if (__m68k_state == NULL || !(__m68k_state->JIT_CONTROL2 & JC2F_BLITWAIT))
        return;

    const uint32_t bstart = 0xDFF040;   // BLTCON0
    const uint32_t bend = 0xDFF076;     // BLTADAT+2
    if (addr >= bend || addr + size <= bstart)
        return;

    const uint16_t mask = 1<<14 | 1<<9 | 1<<6; // BBUSY | DMAEN | BLTEN
    while ((ps_read_16_int(0xdff002) & mask) == mask) {
        // Dummy reads to not steal too many cycles from the blitter.
        // But don't use e.g. CIA reads as we expect the operation
        // to finish soon.
        ps_read_8_int(0x00f00000);
        ps_read_8_int(0x00f00000);
    }
}

/* PiStorm32 */

static void ps32_do_write_access_2s(unsigned int address, unsigned int data, unsigned int size)
{
    set_output();

    write_ps_reg_ps32(REG_SLOT, next_slot);
    if (slot_active[next_slot])
    {
        wait_txn();
    }

    write_ps_reg_ps32(REG_DATA_LO, data & 0xffff);
    if (size == SIZE_LONG)
        write_ps_reg_ps32(REG_DATA_HI, (data >> 16) & 0xffff);

    write_ps_reg_ps32(REG_ADDR_LO, address & 0xffff);
    write_ps_reg_ps32(REG_ADDR_HI, TXN_WRITE | (g_fc << TXN_FC_SHIFT) | (size << TXN_SIZE_SHIFT) | ((address >> 16) & 0xff));

    set_input();
   
    /* For chip memory advance to next slot, for chipset access wait for completion of the transaction */
    if (address >= 0x00bf0000 && address <= 0x00dfffff)
    {
        wait_txn();
        slot_active[next_slot] = 0;
    }
    else
    {
        slot_active[next_slot] = 1;
    }

    next_slot = (next_slot + 1) & 1;

    if (address >= 0x00dff000 && address <= 0x00dfffff) ps_read_32(0x00f80000);
}

static inline void ps32_do_write_access_64_2s(unsigned int address, uint64_t data)
{
    set_output();

    write_ps_reg_ps32(REG_SLOT, next_slot);
    if (slot_active[next_slot])
    {
        wait_txn();
    }

    /* Set first long word to write */
    write_ps_reg_ps32(REG_DATA_LO, (data >> 32) & 0xffff);
    write_ps_reg_ps32(REG_DATA_HI, (data >> 48) & 0xffff);

    /* Set address and start transaction */
    write_ps_reg_ps32(REG_ADDR_LO, address & 0xffff);
    write_ps_reg_ps32(REG_ADDR_HI, TXN_WRITE | (g_fc << TXN_FC_SHIFT) | (SIZE_LONG << TXN_SIZE_SHIFT) | ((address >> 16) & 0xff));

    slot_active[next_slot] = 1;
    next_slot = (next_slot + 1) & 1;

    write_ps_reg_ps32(REG_SLOT, next_slot);
    if (slot_active[next_slot])
    {
        wait_txn();
    }

    /* Advance the address, set second long word to write */
    address += 4;

    /* Set second long word to write */
    write_ps_reg_ps32(REG_DATA_LO, (data) & 0xffff);
    write_ps_reg_ps32(REG_DATA_HI, (data >> 16) & 0xffff);

    /* Set address */
    write_ps_reg_ps32(REG_ADDR_LO, address & 0xffff);

    /* Start second transaction */
    write_ps_reg_ps32(REG_ADDR_HI, TXN_WRITE | (g_fc << TXN_FC_SHIFT) | (SIZE_LONG << TXN_SIZE_SHIFT) | ((address >> 16) & 0xff));

    set_input();
   
    /* For chip memory advance to next slot, for chipset access wait for completion of the transaction */
    if (address >= 0x00bf0000 && address <= 0x00dfffff)
    {
        wait_txn();
        slot_active[next_slot] = 0;
    }
    else
    {
        slot_active[next_slot] = 1;
    }

    next_slot = (next_slot + 1) & 1;

    if (address >= 0x00dff000 && address <= 0x00dfffff) ps_read_32(0x00f80000);
}

static inline void ps32_do_write_access_128_2s(unsigned int address, uint128_t data)
{
    set_output();
    write_ps_reg_ps32(REG_SLOT, next_slot);
    if (slot_active[next_slot])
    {
        wait_txn();
    }

    /* Set first long word to write */
    write_ps_reg_ps32(REG_DATA_LO, (data.hi >> 32) & 0xffff);
    write_ps_reg_ps32(REG_DATA_HI, (data.hi >> 48) & 0xffff);

    /* Set address and start transaction */
    write_ps_reg_ps32(REG_ADDR_LO, address & 0xffff);
    write_ps_reg_ps32(REG_ADDR_HI, TXN_WRITE | (g_fc << TXN_FC_SHIFT) | (SIZE_LONG << TXN_SIZE_SHIFT) | ((address >> 16) & 0xff));

    slot_active[next_slot] = 1;
    next_slot = (next_slot + 1) & 1;
    write_ps_reg_ps32(REG_SLOT, next_slot);
    if (slot_active[next_slot])
    {
        wait_txn();
    }

    /* Advance the address, set second long word to write */
    address += 4;

    /* Set second long word to write */
    write_ps_reg_ps32(REG_DATA_LO, (data.hi) & 0xffff);
    write_ps_reg_ps32(REG_DATA_HI, (data.hi >> 16) & 0xffff);

    /* Set address */
    write_ps_reg_ps32(REG_ADDR_LO, address & 0xffff);

    /* Start second transaction */
    write_ps_reg_ps32(REG_ADDR_HI, TXN_WRITE | (g_fc << TXN_FC_SHIFT) | (SIZE_LONG << TXN_SIZE_SHIFT) | ((address >> 16) & 0xff));

    slot_active[next_slot] = 1;
    next_slot = (next_slot + 1) & 1;
    write_ps_reg_ps32(REG_SLOT, next_slot);
    if (slot_active[next_slot])
    {
        wait_txn();
    }

    address += 4;

    /* Set third long word to write */
    write_ps_reg_ps32(REG_DATA_LO, (data.lo >> 32) & 0xffff);
    write_ps_reg_ps32(REG_DATA_HI, (data.lo >> 48) & 0xffff);

    /* Set address */
    write_ps_reg_ps32(REG_ADDR_LO, address & 0xffff);

    /* Start third transaction */
    write_ps_reg_ps32(REG_ADDR_HI, TXN_WRITE | (g_fc << TXN_FC_SHIFT) | (SIZE_LONG << TXN_SIZE_SHIFT) | ((address >> 16) & 0xff));

    address += 4;

    slot_active[next_slot] = 1;
    next_slot = (next_slot + 1) & 1;
    write_ps_reg_ps32(REG_SLOT, next_slot);
    if (slot_active[next_slot])
    {
        wait_txn();
    }

    /* Set second long word to write */
    write_ps_reg_ps32(REG_DATA_LO, (data.lo) & 0xffff);
    write_ps_reg_ps32(REG_DATA_HI, (data.lo >> 16) & 0xffff);

    /* Set address */
    write_ps_reg_ps32(REG_ADDR_LO, address & 0xffff);

    /* Start fourth transaction */
    write_ps_reg_ps32(REG_ADDR_HI, TXN_WRITE | (g_fc << TXN_FC_SHIFT) | (SIZE_LONG << TXN_SIZE_SHIFT) | ((address >> 16) & 0xff));

    set_input();

    /* For chip memory advance to next slot, for chipset access wait for completion of the transaction */
    if (address >= 0x00bf0000 && address <= 0x00dfffff)
    {
        wait_txn();
        slot_active[next_slot] = 0;
    }
    else
    {
        slot_active[next_slot] = 1;
    }

    next_slot = (next_slot + 1) & 1;

    if (address >= 0x00dff000 && address <= 0x00dfffff) ps_read_32(0x00f80000);
}

static int ps32_do_read_access_2s(unsigned int address, unsigned int size)
{
    set_output();
    write_ps_reg_ps32(REG_SLOT, next_slot);
    if (slot_active[next_slot])
    {
        wait_txn();
    }

    write_ps_reg_ps32(REG_ADDR_LO, address & 0xffff);
    write_ps_reg_ps32(REG_ADDR_HI, TXN_READ | (g_fc << TXN_FC_SHIFT) | (size << TXN_SIZE_SHIFT) | ((address >> 16) & 0xff));

    set_input();
    unsigned int data;

    wait_txn();

    data = read_ps_reg_ps32(REG_DATA_LO);
    if (size == SIZE_BYTE)
        data &= 0xff;
    else if (size == SIZE_LONG)
        data |= read_ps_reg_ps32(REG_DATA_HI) << 16;

    slot_active[next_slot] = 0;
    next_slot = (next_slot + 1) & 1;

    return data;
}

static inline uint64_t ps32_do_read_access_64_2s(unsigned int address)
{
    uint64_t data;

    set_output();
    write_ps_reg_ps32(REG_SLOT, next_slot);
    if (slot_active[next_slot])
    {
        wait_txn();
    }

    write_ps_reg_ps32(REG_ADDR_LO, address & 0xffff);
    write_ps_reg_ps32(REG_ADDR_HI, TXN_READ | (g_fc << TXN_FC_SHIFT) | (SIZE_LONG << TXN_SIZE_SHIFT) | ((address >> 16) & 0xff));

    set_input();

    wait_txn();

    address += 4;

    data = (uint64_t)read_ps_reg_ps32(REG_DATA_HI) << 48;
    data |= (uint64_t)read_ps_reg_ps32(REG_DATA_LO) << 32;
    slot_active[next_slot] = 0;
    next_slot = (next_slot + 1) & 1;

    set_output();
    write_ps_reg_ps32(REG_SLOT, next_slot);

    write_ps_reg_ps32(REG_ADDR_LO, address & 0xffff);
    write_ps_reg_ps32(REG_ADDR_HI, TXN_READ | (g_fc << TXN_FC_SHIFT) | (SIZE_LONG << TXN_SIZE_SHIFT) | ((address >> 16) & 0xff));

    set_input();

    wait_txn();

    slot_active[next_slot] = 0;
    next_slot = (next_slot + 1) & 1;

    data |= (uint64_t)read_ps_reg_ps32(REG_DATA_HI) << 16;
    data |= (uint64_t)read_ps_reg_ps32(REG_DATA_LO);

    return data;
}

static inline uint128_t ps32_do_read_access_128_2s(unsigned int address)
{
    uint128_t data;

    set_output();
    write_ps_reg_ps32(REG_SLOT, next_slot);
    if (slot_active[next_slot])
    {
        wait_txn();
    }

    write_ps_reg_ps32(REG_ADDR_LO, address & 0xffff);
    write_ps_reg_ps32(REG_ADDR_HI, TXN_READ | (g_fc << TXN_FC_SHIFT) | (SIZE_LONG << TXN_SIZE_SHIFT) | ((address >> 16) & 0xff));

    set_input();

    wait_txn();

    address += 4;
    data.hi = (uint64_t)read_ps_reg_ps32(REG_DATA_HI) << 48;
    data.hi |= (uint64_t)read_ps_reg_ps32(REG_DATA_LO) << 32;
    slot_active[next_slot] = 0;
    next_slot = (next_slot + 1) & 1;

    set_output();
    write_ps_reg_ps32(REG_SLOT, next_slot);

    write_ps_reg_ps32(REG_ADDR_LO, address & 0xffff);
    write_ps_reg_ps32(REG_ADDR_HI, TXN_READ | (g_fc << TXN_FC_SHIFT) | (SIZE_LONG << TXN_SIZE_SHIFT) | ((address >> 16) & 0xff));

    set_input();

    wait_txn();

    address += 4;
    data.hi |= (uint64_t)read_ps_reg_ps32(REG_DATA_HI) << 16;
    data.hi |= (uint64_t)read_ps_reg_ps32(REG_DATA_LO);
    slot_active[next_slot] = 0;
    next_slot = (next_slot + 1) & 1;

    set_output();
    write_ps_reg_ps32(REG_SLOT, next_slot);

    write_ps_reg_ps32(REG_ADDR_LO, address & 0xffff);
    write_ps_reg_ps32(REG_ADDR_HI, TXN_READ | (g_fc << TXN_FC_SHIFT) | (SIZE_LONG << TXN_SIZE_SHIFT) | ((address >> 16) & 0xff));

    set_input();

    wait_txn();

    address += 4;
    data.lo = (uint64_t)read_ps_reg_ps32(REG_DATA_HI) << 48;
    data.lo |= (uint64_t)read_ps_reg_ps32(REG_DATA_LO) << 32;
    slot_active[next_slot] = 0;
    next_slot = (next_slot + 1) & 1;

    set_output();
    write_ps_reg_ps32(REG_SLOT, next_slot);

    write_ps_reg_ps32(REG_ADDR_LO, address & 0xffff);
    write_ps_reg_ps32(REG_ADDR_HI, TXN_READ | (g_fc << TXN_FC_SHIFT) | (SIZE_LONG << TXN_SIZE_SHIFT) | ((address >> 16) & 0xff));

    set_input();

    wait_txn();

    data.lo |= (uint64_t)read_ps_reg_ps32(REG_DATA_HI) << 16;
    data.lo |= (uint64_t)read_ps_reg_ps32(REG_DATA_LO);

    slot_active[next_slot] = 0;
    next_slot = (next_slot + 1) & 1;

    return data;
}

static inline void ps32_do_write_access(unsigned int address, unsigned int data, unsigned int size)
{
    set_output();

    write_ps_reg_ps32(REG_DATA_LO, data & 0xffff);
    if (size == SIZE_LONG)
        write_ps_reg_ps32(REG_DATA_HI, (data >> 16) & 0xffff);

    write_ps_reg_ps32(REG_ADDR_LO, address & 0xffff);

    if (write_pending) wait_txn();

    write_ps_reg_ps32(REG_ADDR_HI, TXN_WRITE | (g_fc << TXN_FC_SHIFT) | (size << TXN_SIZE_SHIFT) | ((address >> 16) & 0xff));

    set_input();

    if (address >= 0x00bf0000 && address <= 0x00dfffff)
    {
        wait_txn();

        write_pending = 0;
    }
    else
        write_pending = 1;

    if (address >= 0x00dff000 && address <= 0x00dfffff) ps_read_32(0x00f80000);
}

static inline void ps32_do_write_access_64(unsigned int address, uint64_t data)
{
    set_output();

    /* Set first long word to write */
    write_ps_reg_ps32(REG_DATA_LO, (data >> 32) & 0xffff);
    write_ps_reg_ps32(REG_DATA_HI, (data >> 48) & 0xffff);

    /* Set address and start transaction */
    write_ps_reg_ps32(REG_ADDR_LO, address & 0xffff);

    if (write_pending) wait_txn();

    write_ps_reg_ps32(REG_ADDR_HI, TXN_WRITE | (g_fc << TXN_FC_SHIFT) | (SIZE_LONG << TXN_SIZE_SHIFT) | ((address >> 16) & 0xff));

    /* Advance the address, set second long word to write */
    address += 4;

    /* Wait for completion of first transfer */
    wait_txn();

    /* Set second long word to write */
    write_ps_reg_ps32(REG_DATA_LO, (data) & 0xffff);
    write_ps_reg_ps32(REG_DATA_HI, (data >> 16) & 0xffff);

    /* Set address */
    write_ps_reg_ps32(REG_ADDR_LO, address & 0xffff);
   
    /* Start second transaction */
    write_ps_reg_ps32(REG_ADDR_HI, TXN_WRITE | (g_fc << TXN_FC_SHIFT) | (SIZE_LONG << TXN_SIZE_SHIFT) | ((address >> 16) & 0xff));

    set_input();

    if (address >= 0x00bf0000 && address <= 0x00dfffff)
    {
        wait_txn();

        write_pending = 0;
    }
    else
        write_pending = 1;
    
    if (address >= 0x00dff000 && address <= 0x00dfffff) ps_read_32(0x00f80000);
}

static inline void ps32_do_write_access_128(unsigned int address, uint128_t data)
{
    set_output();

    /* Set first long word to write */
    write_ps_reg_ps32(REG_DATA_LO, (data.hi >> 32) & 0xffff);
    write_ps_reg_ps32(REG_DATA_HI, (data.hi >> 48) & 0xffff);

    /* Set address and start transaction */
    write_ps_reg_ps32(REG_ADDR_LO, address & 0xffff);

    if (write_pending) wait_txn();

    write_ps_reg_ps32(REG_ADDR_HI, TXN_WRITE | (g_fc << TXN_FC_SHIFT) | (SIZE_LONG << TXN_SIZE_SHIFT) | ((address >> 16) & 0xff));

    /* Advance the address, set second long word to write */
    address += 4;

    /* Wait for completion of first transfer */
    wait_txn();

    /* Set second long word to write */
    write_ps_reg_ps32(REG_DATA_LO, (data.hi) & 0xffff);
    write_ps_reg_ps32(REG_DATA_HI, (data.hi >> 16) & 0xffff);

    /* Set address */
    write_ps_reg_ps32(REG_ADDR_LO, address & 0xffff);
    
    /* Start second transaction */
    write_ps_reg_ps32(REG_ADDR_HI, TXN_WRITE | (g_fc << TXN_FC_SHIFT) | (SIZE_LONG << TXN_SIZE_SHIFT) | ((address >> 16) & 0xff));

    address += 4;

    /* Wait for completion of second transfer */
    wait_txn();

    /* Set third long word to write */
    write_ps_reg_ps32(REG_DATA_LO, (data.lo >> 32) & 0xffff);
    write_ps_reg_ps32(REG_DATA_HI, (data.lo >> 48) & 0xffff);

    /* Set address */
    write_ps_reg_ps32(REG_ADDR_LO, address & 0xffff);
    
    /* Start third transaction */
    write_ps_reg_ps32(REG_ADDR_HI, TXN_WRITE | (g_fc << TXN_FC_SHIFT) | (SIZE_LONG << TXN_SIZE_SHIFT) | ((address >> 16) & 0xff));

    address += 4;

    /* Wait for completion of third transfer */
    wait_txn();

    /* Set second long word to write */
    write_ps_reg_ps32(REG_DATA_LO, (data.lo) & 0xffff);
    write_ps_reg_ps32(REG_DATA_HI, (data.lo >> 16) & 0xffff);

    /* Set address */
    write_ps_reg_ps32(REG_ADDR_LO, address & 0xffff);
    
    /* Start fourth transaction */
    write_ps_reg_ps32(REG_ADDR_HI, TXN_WRITE | (g_fc << TXN_FC_SHIFT) | (SIZE_LONG << TXN_SIZE_SHIFT) | ((address >> 16) & 0xff));

    set_input();

    if (address >= 0x00bf0000 && address <= 0x00dfffff)
    {
        wait_txn();

        write_pending = 0;
    }
    else
        write_pending = 1;
    
    if (address >= 0x00dff000 && address <= 0x00dfffff) ps_read_32(0x00f80000);
}

static inline int ps32_do_read_access(unsigned int address, unsigned int size)
{
    set_output();

    write_ps_reg_ps32(REG_ADDR_LO, address & 0xffff);

    if (write_pending) wait_txn();

    write_ps_reg_ps32(REG_ADDR_HI, TXN_READ | (g_fc << TXN_FC_SHIFT) | (size << TXN_SIZE_SHIFT) | ((address >> 16) & 0xff));

    set_input();
    unsigned int data;

    data = read_ps_reg_with_wait_ps32(REG_DATA_LO);
    if (size == SIZE_BYTE)
        data &= 0xff;
    else if (size == SIZE_LONG)
        data |= read_ps_reg_ps32(REG_DATA_HI) << 16;

    write_pending = 0;

    return data;
}

static inline uint64_t ps32_do_read_access_64(unsigned int address)
{
    uint64_t data;

    set_output();

    write_ps_reg_ps32(REG_ADDR_LO, address & 0xffff);

    if (write_pending) wait_txn();

    write_ps_reg_ps32(REG_ADDR_HI, TXN_READ | (g_fc << TXN_FC_SHIFT) | (SIZE_LONG << TXN_SIZE_SHIFT) | ((address >> 16) & 0xff));

    set_input();

    address += 4;
    data = (uint64_t)read_ps_reg_with_wait_ps32(REG_DATA_HI) << 48;
    data |= (uint64_t)read_ps_reg_ps32(REG_DATA_LO) << 32;

    set_output();

    write_ps_reg_ps32(REG_ADDR_LO, address & 0xffff);
    write_ps_reg_ps32(REG_ADDR_HI, TXN_READ | (g_fc << TXN_FC_SHIFT) | (SIZE_LONG << TXN_SIZE_SHIFT) | ((address >> 16) & 0xff));

    set_input();

    data |= (uint64_t)read_ps_reg_with_wait_ps32(REG_DATA_HI) << 16;
    data |= (uint64_t)read_ps_reg_ps32(REG_DATA_LO);

    write_pending = 0;

    return data;
}

static inline uint128_t ps32_do_read_access_128(unsigned int address)
{
    uint128_t data;

    set_output();

    write_ps_reg_ps32(REG_ADDR_LO, address & 0xffff);
    
    if (write_pending) wait_txn();

    write_ps_reg_ps32(REG_ADDR_HI, TXN_READ | (g_fc << TXN_FC_SHIFT) | (SIZE_LONG << TXN_SIZE_SHIFT) | ((address >> 16) & 0xff));

    set_input();

    address += 4;
    data.hi = (uint64_t)read_ps_reg_with_wait_ps32(REG_DATA_HI) << 48;
    data.hi |= (uint64_t)read_ps_reg_ps32(REG_DATA_LO) << 32;

    set_output();

    write_ps_reg_ps32(REG_ADDR_LO, address & 0xffff);
    write_ps_reg_ps32(REG_ADDR_HI, TXN_READ | (g_fc << TXN_FC_SHIFT) | (SIZE_LONG << TXN_SIZE_SHIFT) | ((address >> 16) & 0xff));

    set_input();

    address += 4;
    data.hi |= (uint64_t)read_ps_reg_with_wait_ps32(REG_DATA_HI) << 16;
    data.hi |= (uint64_t)read_ps_reg_ps32(REG_DATA_LO);

    set_output();

    write_ps_reg_ps32(REG_ADDR_LO, address & 0xffff);
    write_ps_reg_ps32(REG_ADDR_HI, TXN_READ | (g_fc << TXN_FC_SHIFT) | (SIZE_LONG << TXN_SIZE_SHIFT) | ((address >> 16) & 0xff));

    set_input();

    address += 4;
    data.lo = (uint64_t)read_ps_reg_with_wait_ps32(REG_DATA_HI) << 48;
    data.lo |= (uint64_t)read_ps_reg_ps32(REG_DATA_LO) << 32;

    set_output();

    write_ps_reg_ps32(REG_ADDR_LO, address & 0xffff);
    write_ps_reg_ps32(REG_ADDR_HI, TXN_READ | (g_fc << TXN_FC_SHIFT) | (SIZE_LONG << TXN_SIZE_SHIFT) | ((address >> 16) & 0xff));

    set_input();

    data.lo |= (uint64_t)read_ps_reg_with_wait_ps32(REG_DATA_HI) << 16;
    data.lo |= (uint64_t)read_ps_reg_ps32(REG_DATA_LO);

    write_pending = 0;

    return data;
}

int (*ps32_read_access)(unsigned int address, unsigned int size);
uint64_t (*ps32_read_access_64)(unsigned int address);
uint128_t (*ps32_read_access_128)(unsigned int address);
void (*ps32_write_access)(unsigned int address, unsigned int data, unsigned int size);
void (*ps32_write_access_64)(unsigned int address, uint64_t data);
void (*ps32_write_access_128)(unsigned int address, uint128_t data);

void ps32_write_8_int(unsigned int address, unsigned int data) {
    ps32_write_access(address, data, SIZE_BYTE);
}

void ps32_write_8(unsigned int address, unsigned int data) {
    ps32_write_8_int(address, data);
    if (SLOW_IO(address))
    {
        ps32_read_access(0x00f00000, SIZE_BYTE);
    }
    cache_invalidate_range(ICACHE, address, 1);
}

void ps32_write_16_int(unsigned int address, unsigned int data) {
    ps32_write_access(address, data, SIZE_WORD);
}

void ps32_write_16(unsigned int address, unsigned int data) {
    check_blit_active(address, 2);
    ps32_write_16_int(address, data);
    if (SLOW_IO(address))
    {
        ps32_read_access(0x00f00000, SIZE_BYTE);
    }
    cache_invalidate_range(ICACHE, address, 2);
}

void ps32_write_32_int(unsigned int address, unsigned int data) {
    ps32_write_access(address, data, SIZE_LONG);
}

void ps32_write_32(unsigned int address, unsigned int data) {
    check_blit_active(address, 4);
    ps32_write_32_int(address, data);
    if (SLOW_IO(address))
    {
        ps32_read_access(0x00f00000, SIZE_BYTE);
    }
    cache_invalidate_range(ICACHE, address, 4);
}

void ps32_write_64_int(unsigned int address, uint64_t data) {
    ps32_write_access_64(address, data);
}

void ps32_write_64(unsigned int address, uint64_t data) {
    check_blit_active(address, 8);
    ps32_write_64_int(address, data);
    if (SLOW_IO(address))
    {
        ps32_read_access(0x00f00000, SIZE_BYTE);
    }
    cache_invalidate_range(ICACHE, address, 8);
}

void ps32_write_128_int(unsigned int address, uint128_t data) {
    ps32_write_access_128(address, data);
}

void ps32_write_128(unsigned int address, uint128_t data) {
    check_blit_active(address, 16);
    ps32_write_128_int(address, data);
    if (SLOW_IO(address))
    {
        ps32_read_access(0x00f00000, SIZE_BYTE);
    }
    cache_invalidate_range(ICACHE, address, 16);
}

unsigned int ps32_read_8(unsigned int address) {
    return ps32_read_access(address, SIZE_BYTE);
}

unsigned int ps32_read_16(unsigned int address) {
    return ps32_read_access(address, SIZE_WORD);
}

unsigned int ps32_read_32(unsigned int address) {
    return ps32_read_access(address, SIZE_LONG);
}

uint64_t ps32_read_64(unsigned int address) {
    return ps32_read_access_64(address);
}

uint128_t ps32_read_128(unsigned int address) {
    return ps32_read_access_128(address);
}


/* PiStorm16 */

static inline int ps16_read_access(unsigned int address, unsigned int size)
{
    set_output();

    write_ps_reg_ps16(REG_ADDR_LO, address & 0xffff);

    if (write_pending)
    {
        wait_txn();
    }

    write_ps_reg_ps16(REG_ADDR_HI, TXN_READ | (g_fc << TXN_FC_SHIFT) | (size << TXN_SIZE_SHIFT) | ((address >> 16) & 0xff));

    set_input();
    unsigned int data = 0;

    if (size == SIZE_LONG)
    {
        // First half of data is HI word. When reading this register TXN will show status of this word,
        // not the entire transfer
        data = read_ps_reg_with_wait_ps16(REG_DATA_HI) << 16;
        // Second half of data will be fetched once whole transfer is completed
        data |= read_ps_reg_with_wait_ps16(REG_DATA_LO);
    }
    else
    {
        data = read_ps_reg_with_wait_ps16(REG_DATA_LO);
    }

    write_pending = 0;

    return data;
}

static inline void ps16_write_access(unsigned int address, unsigned int data, unsigned int size)
{
    set_output();

    write_ps_reg_ps16(REG_DATA_LO, data & 0xffff);
    if (size == SIZE_LONG)
        write_ps_reg_ps16(REG_DATA_HI, (data >> 16) & 0xffff);

    write_ps_reg_ps16(REG_ADDR_LO, address & 0xffff);

    if (write_pending) {
        wait_txn();
    }

    write_ps_reg_ps16(REG_ADDR_HI, TXN_WRITE | (g_fc << TXN_FC_SHIFT) | (size << TXN_SIZE_SHIFT) | ((address >> 16) & 0xff));

    set_input();

    if (address >= 0x00bf0000 && address <= 0x00dfffff)
    {
        wait_txn();

        write_pending = 0;
    }
    else
        write_pending = 1;
    
    if (address >= 0x00dff000 && address <= 0x00dfffff) ps16_read_access(0x00f80000, SIZE_WORD);
}

unsigned int ps16_read_8_int(unsigned int address)
{
    unsigned int data = ps16_read_access(address, SIZE_BYTE);
    return (address & 1) ? data & 0xff : data >> 8;
}

unsigned int ps16_read_8(unsigned int address)
{
    return ps16_read_8_int(address);
}

unsigned int ps16_read_16_int(unsigned int address)
{
    if (unlikely(address & 1))
        return (ps16_read_8_int(address) << 8) | ps16_read_8_int(address + 1);
    else
        return ps16_read_access(address, SIZE_WORD);
}

unsigned int ps16_read_16(unsigned int address)
{
    return ps16_read_16_int(address);
}

unsigned int ps16_read_32_int(unsigned int address)
{
    unsigned int data = 0;
    if (unlikely(address & 1))
    {
        data = ps16_read_8_int(address) << 24;
        data |= ps16_read_16_int(address + 1) << 8;
        data |= ps16_read_8_int(address + 3);
    }
    else
    {
        data = ps16_read_access(address, SIZE_LONG);
    }
    return data;
}

unsigned int ps16_read_32(unsigned int address)
{
    return ps16_read_32_int(address);
}

uint64_t ps16_read_64_int(unsigned int address)
{
    uint64_t data = 0;
    if (unlikely(address & 1))
    {
        data = (uint64_t)ps16_read_8(address) << 56;
        data |= (uint64_t)ps16_read_32_int(address + 1) << 24;
        data |= ps16_read_16_int(address + 5) << 8;
        data |= ps16_read_8(address + 7);
    }
    else
    {
        data |= (uint64_t)ps16_read_32_int(address) << 32;
        data |= ps16_read_32_int(address + 4);
    }

    return data;
}

uint64_t ps16_read_64(unsigned int address)
{
    return ps16_read_64_int(address);
}

uint128_t ps16_read_128_int(unsigned int address)
{
    uint128_t data;

    if (unlikely(address & 1))
    {
        uint16_t d;
        data.hi = (uint64_t)ps16_read_8(address) << 56;
        data.hi |= (uint64_t)ps16_read_16(address + 1) << 40;
        data.hi |= (uint64_t)ps16_read_16(address + 3) << 24;
        data.hi |= ps16_read_16(address + 5) << 8;
        d = ps16_read_16(address + 7);
        data.hi |= (d >> 8);
        data.lo = (uint64_t)d << 56;
        data.lo |= (uint64_t)ps16_read_16(address + 9) << 40;
        data.lo |= (uint64_t)ps16_read_16(address + 11) << 24;
        data.lo |= ps16_read_16(address + 13) << 8;
        data.lo |= ps16_read_8(address + 15);
    }
    else
    {
        data.hi = ps16_read_64_int(address);
        data.lo = ps16_read_64_int(address + 8);
    }

    return data;
}

uint128_t ps16_read_128(unsigned int address)
{
    return ps16_read_128_int(address);
}


void ps16_write_8_int(unsigned int address, unsigned int data)
{
    uint16_t d = data & 0xff;
    d |= d << 8;
    ps16_write_access(address, d, SIZE_BYTE);
}

void ps16_write_8(unsigned int address, unsigned int data)
{
    ps16_write_8_int(address, data);

    if (SLOW_IO(address))
    {
        ps16_read_access(0x00f00000, SIZE_BYTE);
    }
    cache_invalidate_range(ICACHE, address, 1);
}

void ps16_write_16_int(unsigned int address, unsigned int data)
{
    if (unlikely(address & 1)) {
        ps16_write_8_int(address, data >> 8);
        ps16_write_8_int(address + 1, data);
    }
    else {
        ps16_write_access(address, data, SIZE_WORD);
    }
}

void ps16_write_16(unsigned int address, unsigned int data)
{
    check_blit_active(address, 2);
    
    ps16_write_16_int(address, data);

    if (SLOW_IO(address))
    {
        ps16_read_access(0x00f00000, SIZE_BYTE);
    }
    cache_invalidate_range(ICACHE, address, 2);
}


void ps16_write_32_int(unsigned int address, unsigned int data)
{
    if (unlikely(address & 1)) {
        ps16_write_8_int(address, data >> 24);
        ps16_write_16_int(address + 1, data >> 8);
        ps16_write_8_int(address + 3, data);
    }
    else {
        ps16_write_access(address, data, SIZE_LONG);
    }
}

void ps16_write_32(unsigned int address, unsigned int data)
{
    check_blit_active(address, 4);
    
    ps16_write_32_int(address, data);

    if (SLOW_IO(address))
    {
        ps16_read_access(0x00f00000, SIZE_BYTE);
    }
    cache_invalidate_range(ICACHE, address, 4);
}

void ps16_write_64_int(unsigned int address, uint64_t data)
{
    if (unlikely(address & 1)) {
        ps16_write_8_int(address, data >> 56);
        ps16_write_32_int(address + 1, data >> 24);
        ps16_write_16_int(address + 5, data >> 8);
        ps16_write_8_int(address + 7, data);
    }
    else {
        ps16_write_32_int(address, data >> 32);
        ps16_write_32_int(address + 4, data);
    }
}

void ps16_write_64(unsigned int address, uint64_t data)
{
    check_blit_active(address, 8);

    ps16_write_64_int(address, data);

    if (SLOW_IO(address))
    {
        ps16_read_access(0x00f00000, SIZE_BYTE);
    }
    cache_invalidate_range(ICACHE, address, 8);
}

void ps16_write_128_int(unsigned int address, uint128_t data)
{
    if (unlikely(address & 1)) {
        ps16_write_8_int(address, data.hi >> 56);
        ps16_write_32_int(address + 1, data.hi >> 24);
        ps16_write_16_int(address + 5, data.hi >> 8);
        ps16_write_16_int(address + 7, data.hi << 8 | (data.lo >> 56));
        ps16_write_32_int(address + 9, data.lo >> 24);
        ps16_write_16_int(address + 13, data.lo >> 8);
        ps16_write_8_int(address + 15, data.lo);
    }
    else {
        ps16_write_64_int(address, data.hi);
        ps16_write_64_int(address + 8, data.lo);
    }
}

void ps16_write_128(unsigned int address, uint128_t data)
{
    check_blit_active(address, 16);

    ps16_write_128_int(address, data);

    if (SLOW_IO(address))
    {
        ps16_read_access(0x00f00000, SIZE_BYTE);
    }
    cache_invalidate_range(ICACHE, address, 16);
}

static uint8_t pistorm_model;

unsigned int (*ps_read_8)(unsigned int address);
unsigned int (*ps_read_16)(unsigned int address);
unsigned int (*ps_read_32)(unsigned int address);
uint64_t (*ps_read_64)(unsigned int address);
uint128_t (*ps_read_128)(unsigned int address);

void (*ps_write_8)(unsigned int address, unsigned int data);
void (*ps_write_16)(unsigned int address, unsigned int data);
void (*ps_write_32)(unsigned int address, unsigned int data);
void (*ps_write_64)(unsigned int address, uint64_t data);
void (*ps_write_128)(unsigned int address, uint128_t data);

unsigned int (*ps_read_8_int)(unsigned int address);
unsigned int (*ps_read_16_int)(unsigned int address);
unsigned int (*ps_read_32_int)(unsigned int address);
uint64_t (*ps_read_64_int)(unsigned int address);

void (*ps_write_8_int)(unsigned int address, unsigned int data);
void (*ps_write_16_int)(unsigned int address, unsigned int data);
void (*ps_write_32_int)(unsigned int address, unsigned int data);
void (*ps_write_64_int)(unsigned int address, uint64_t data);

void ps_setup_protocol()
{
    pistorm_setup_io();
    pistorm_setup_serial();

    GPIO->GPCLR0 = CLEAR_BITS;

    set_input();

    pistorm_model = pistorm_get_model();

    switch (pistorm_model)
    {
        case PISTORM_MODEL_32:
            write_ps_reg = write_ps_reg_ps32;
            read_ps_reg = read_ps_reg_ps32;
            read_ps_reg_with_wait = read_ps_reg_with_wait_ps32;

            if (use_2slot) {
                ps32_read_access = ps32_do_read_access_2s;
                ps32_read_access_64 = ps32_do_read_access_64_2s;
                ps32_read_access_128 = ps32_do_read_access_128_2s;

                ps32_write_access = ps32_do_write_access_2s;
                ps32_write_access_64 = ps32_do_write_access_64_2s;
                ps32_write_access_128 = ps32_do_write_access_128_2s;
            }
            else {
                ps32_read_access = ps32_do_read_access;
                ps32_read_access_64 = ps32_do_read_access_64;
                ps32_read_access_128 = ps32_do_read_access_128;

                ps32_write_access = ps32_do_write_access;
                ps32_write_access_64 = ps32_do_write_access_64;
                ps32_write_access_128 = ps32_do_write_access_128;
            }
            
            ps_read_8_int = ps32_read_8;
            ps_read_16_int = ps32_read_16;
            ps_read_32_int = ps32_read_32;
            ps_read_64_int = ps32_read_64;

            ps_read_8 = ps32_read_8;
            ps_read_16 = ps32_read_16;
            ps_read_32 = ps32_read_32;
            ps_read_64 = ps32_read_64;
            ps_read_128 = ps32_read_128;

            ps_write_8_int = ps32_write_8_int;
            ps_write_16_int = ps32_write_16_int;
            ps_write_32_int = ps32_write_32_int;
            ps_write_64_int = ps32_write_64_int;

            ps_write_8 = ps32_write_8;
            ps_write_16 = ps32_write_16;
            ps_write_32 = ps32_write_32;
            ps_write_64 = ps32_write_64;
            ps_write_128 = ps32_write_128;

            break;

        case PISTORM_MODEL_16:
            write_ps_reg = write_ps_reg_ps16;
            read_ps_reg = read_ps_reg_ps16;
            read_ps_reg_with_wait = read_ps_reg_with_wait_ps16;

            ps_read_8_int = ps16_read_8_int;
            ps_read_16_int = ps16_read_16_int;
            ps_read_32_int = ps16_read_32_int;
            ps_read_64_int = ps16_read_64_int;

            ps_write_8_int = ps16_write_8_int;
            ps_write_16_int = ps16_write_16_int;
            ps_write_32_int = ps16_write_32_int;
            ps_write_64_int = ps16_write_64_int;

            ps_read_8 = ps16_read_8;
            ps_read_16 = ps16_read_16;
            ps_read_32 = ps16_read_32;
            ps_read_64 = ps16_read_64;
            ps_read_128 = ps16_read_128;

            ps_write_8 = ps16_write_8;
            ps_write_16 = ps16_write_16;
            ps_write_32 = ps16_write_32;
            ps_write_64 = ps16_write_64;
            ps_write_128 = ps16_write_128;

            break;
    }
}

void wb_init()
{
}

void wb_task()
{
}

void ps_reset_state_machine()
{
}

#include <boards.h>
extern struct ExpansionBoard **board;
extern struct ExpansionBoard *__boards_start;
extern int board_idx;
extern uint32_t overlay;

volatile int housekeeper_enabled = 0;
volatile int ignore_reset = 0;

#define PM_RSTC ((volatile unsigned int *)(0xf2000000 + 0x0010001c))
#define PM_RSTS ((volatile unsigned int *)(0xf2000000 + 0x00100020))
#define PM_WDOG ((volatile unsigned int *)(0xf2000000 + 0x00100024))
#define PM_WDOG_MAGIC 0x5a000000
#define PM_RSTC_FULLRST 0x00000020

void pi_reset()
{
    ps_set_control(CONTROL_REQ_BM);
    usleep(100000);

    ps_set_control(CONTROL_DRIVE_RESET);
    usleep(150000);

    unsigned int r;
    // trigger a restart by instructing the GPU to boot from partition 0
    r = LE32(*PM_RSTS);
    r &= 0xfffffaaa; //bits 0,2,4,6,8,10 are boot partition
    *PM_RSTS = LE32(PM_WDOG_MAGIC | r); // boot from partition 0
    
    *PM_WDOG = LE32(PM_WDOG_MAGIC | 10);

    r = LE32(*PM_RSTC);
    r &= 0xffffffcf;
    r |= PM_WDOG_MAGIC | PM_RSTC_FULLRST;
    *PM_RSTC = LE32(r);

    while (1)
        ;
}

void ps_pulse_reset()
{
    ignore_reset = 1;

    if (pistorm_model == PISTORM_MODEL_16) {
        kprintf("[PS] Set DTACK delay\n");
        ps_set_control(23 << 8);
    }

    //platform_report_stealth();

    kprintf("[PS] Set DRIVE_RESET\n");
    ps_set_control(CONTROL_DRIVE_RESET);
    usleep(150000);

    kprintf("[PS] Clear DRIVE_RESET\n");
    ps_clr_control(CONTROL_DRIVE_RESET);

    #if 1
    const uint64_t t0 = get_microseconds();
    uint64_t t = 0;
    while ((GPIO->GPLEV0 & LE32(1 << PIN_KBRESET)) == 0)
    {
        t = get_microseconds() - t0;
        
        /* 
            If reset was active longer that 3 seconds, enter stealth mode - just wait here until
            RESET is pressed for more than 3 seconds again - is this the case, then reset everything
            and get back to Emu68
        */
        if (t > 3000000)
        {
            /* Configure timer-based event stream */
            /* Enable timer regs from EL0, enable event stream on posedge, monitor 2th bit */
            /* This gives a frequency of 2.4MHz for a 19.2MHz timer */
            uint64_t freq;
            asm volatile("mrs %0, CNTFRQ_EL0" : "=r"(freq));

            if (freq > 20000000)
            {
                asm volatile("msr CNTKCTL_EL1, %0" ::"r"(3 | (1 << 2) | (3 << 8) | (4 << 4)));
            }
            else
            {
                asm volatile("msr CNTKCTL_EL1, %0" ::"r"(3 | (1 << 2) | (3 << 8) | (2 << 4)));
            }
            
            platform_report_stealth();

            kprintf("[PS] Entering STEALTH mode\n");

            /* Wait first until RESET is released*/
            while ((GPIO->GPLEV0 & LE32(1 << PIN_KBRESET)) == 0) asm volatile("wfe");

            ps_clr_control(CONTROL_REQ_BM);
            usleep(100000);

            kprintf("[PS] RESET released\n");

            int do_reset = 0;

            do {
                /* Wait first until RESET is pressed */
                while ((GPIO->GPLEV0 & LE32(1 << PIN_KBRESET)) != 0) asm volatile("wfe");
                
                kprintf("[PS] RESET pressed\n");

                const uint64_t t0 = get_microseconds();
                uint64_t t = 0;

                while ((GPIO->GPLEV0 & LE32(1 << PIN_KBRESET)) == 0)
                {
                    t = get_microseconds() - t0;
        
                    /* 
                        If reset was active longer that 3 seconds, enter stealth mode - just wait here until
                        RESET is pressed for more than 3 seconds again - is this the case, then reset everything
                        and get back to Emu68
                    */
                    if (t > 3000000)
                    {
                        do_reset = 1;
                        break;
                    }

                    asm volatile("wfe");
                }
                kprintf("[PS] RESET was active for %lld microseconds\n", t);
            } while(do_reset == 0);

            kprintf("[PS] leaving stealth mode now\n");

            kprintf("[PS] Resetting RasPi now...\n");
            pi_reset();
        }
    }
    
    kprintf("[PS] RESET was active for %lld microseconds\n", t);
#endif
    
    kprintf("[PS] Set REQUEST_BM\n");
    ps_set_control(CONTROL_REQ_BM);
    usleep(100000);

    kprintf("[PS] Set DRIVE_RESET\n");
    ps_set_control(CONTROL_DRIVE_RESET);
    usleep(150000);

    kprintf("[PS] Clear DRIVE_RESET\n");
    ps_clr_control(CONTROL_DRIVE_RESET);

    if (pistorm_model == PISTORM_MODEL_32 && use_2slot) {
        ps_set_control(CONTROL_INC_EXEC_SLOT);
        slot_active[0] = 0;
        slot_active[1] = 0;
        next_slot = 0;
    }

    kprintf("[PS] RESET done\n");

    overlay = 1;
    board = &__boards_start;
    board_idx = 0;

    ignore_reset = 0;
}


void ps_housekeeper()
{
    extern uint64_t arm_cnt;
    uint64_t t0;
    uint64_t last_arm_cnt = arm_cnt;

    asm volatile("mrs %0, CNTPCT_EL0" : "=r"(t0));
    asm volatile("mrs %0, PMCCNTR_EL0" : "=r"(last_arm_cnt));

    kprintf("[HKEEP] Housekeeper activated\n");
    kprintf("[HKEEP] Please note we are burning the cpu with busyloops now\n");

    /* Configure timer-based event stream */
    /* Enable timer regs from EL0, enable event stream on posedge, monitor 2th bit */
    /* This gives a frequency of 2.4MHz for a 19.2MHz timer */
    uint64_t freq;
    asm volatile("mrs %0, CNTFRQ_EL0" : "=r"(freq));

    if (freq > 20000000)
    {
        asm volatile("msr CNTKCTL_EL1, %0" ::"r"(3 | (1 << 2) | (3 << 8) | (3 << 4)));
    }
    else
    {
        asm volatile("msr CNTKCTL_EL1, %0" ::"r"(3 | (1 << 2) | (3 << 8) | (2 << 4)));
    }

    uint8_t pin_prev = LE32(GPIO->GPLEV0);

    for (;;)
    {
        /*
              Wait for event. It can happen that the CPU is flooded with them for some reason, but
              nevertheless, thanks for the event stream set up above, they will appear at 1.2MHz in worst case
        */
        asm volatile("wfe");

        if (housekeeper_enabled)
        {
            uint32_t pin = LE32(GPIO->GPLEV0);
#if 0
            static uint64_t last_stats_shown = 0;
            uint64_t now;

            asm volatile("mrs %0, CNTPCT_EL0" : "=r"(now));
            if (now - last_stats_shown >= (freq)) {
                last_stats_shown = now;
                double used = 100.0 * (1.0 - (double)tlsf_get_free_size(jit_tlsf) / (double)tlsf_get_total_size(jit_tlsf));
                uint64_t percent = 100 * used;
                kprintf("[JIT] JIT cache free %dkB, %d.%02d%% used, unit count %d\n", tlsf_get_free_size(jit_tlsf) / 1024, percent / 100, percent % 100, __m68k_state->JIT_UNIT_COUNT);
#if 0
                int max_idx = 0;
                static struct {
                    uint32_t item_count;
                    uint32_t list_count;
                } stats[EMU68_HASHSIZE]; 

                for (int i=0; i < EMU68_HASHSIZE; i++) {
                    stats[i].item_count = 0;
                    stats[i].list_count = 0;
                }

                for (int i=0; i < EMU68_HASHSIZE; i++) {
                    extern struct List ICache[EMU68_HASHSIZE];
                    unsigned cnt = 0;
                    struct List *l = &ICache[i];
                    struct Node *n;
                    ForeachNode(l, n) {
                        cnt++;
                    }
                    if (cnt == 0) continue;

                    int stats_idx = -1;
                    for (int j=0; j < max_idx; j++) {
                        if (stats[j].item_count == cnt) {
                            stats_idx = j;
                            break;
                        }
                    }
                    if (stats_idx == -1) {
                        stats_idx = max_idx++;
                    }
                    stats[stats_idx].item_count = cnt;
                    stats[stats_idx].list_count++;
                }
                kprintf("[JIT] JIT usage stats:\n");
                for (int i=0; i < max_idx; i++) {
                    kprintf("[JIT]   %10d lists have %10d items in bucket\n", stats[i].list_count, stats[i].item_count);
                }
#endif
            }
#endif
            // Reall 680x0 CPU filters IPL lines in order to avoid false interrupts if
            // there is a clock skew between three IPL bits. We need to do the same.
            // Update IPL if and only if two subsequent IPL reads are the same.
            if ((pin & 7) == (pin_prev & 7))
            {
                __m68k_state->INTF.IPL = ~pin & 7;

                asm volatile("" ::: "memory");

                if (__m68k_state->INTF.IPL)
                    asm volatile("sev" ::: "memory");
            }

            pin_prev = pin;

            if ((pin & (1 << PIN_KBRESET)) == 0 && ignore_reset == 0)
            {
                kprintf("[HKEEP] Houskeeper will reset RasPi now...\n");
                pi_reset();
            }
        }
    }
}

/* Send reset from Emu68 to the mainbard. Set a flag to let housekeeper ignore it */
void ps_send_reset()
{
    if(gic_available())
        gic_local_disable();

    ignore_reset = 1;

    kprintf("[PS] CPU sending RESET\n");
    ps_set_control(CONTROL_DRIVE_RESET);
    usleep(150000);

    kprintf("[PS] CPU clearing \n");
    ps_clr_control(CONTROL_DRIVE_RESET);

    kprintf("[PS] RESET done\n");

    overlay = 1;
    board = &__boards_start;
    board_idx = 0;

    usleep(10000);

    ignore_reset = 0;
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

void ps_memtest(unsigned int test_size)
{
    ps_write_8_int(0xbfe201, 0x0101);       //CIA OVL
    ps_write_8_int(0xbfe001, 0x0000);       //CIA OVL LOW

    int num_iter = 1;
    uint64_t clkspeed;
    uint64_t t0, t1;
    uint32_t ns;
    double result;

    asm volatile("mrs %0, CNTFRQ_EL0":"=r"(clkspeed));

    kprintf_pc(__putc, NULL, "MemBench with size %dK requested through commandline\n", test_size / 1024);

    num_iter = 1;

    do {
        asm volatile("mrs %0, CNTPCT_EL0":"=r"(t0));

        for (int iter = 0; iter < num_iter; iter++)
        {
            for (unsigned int addr = 0x1000; addr < test_size + 0x1000; addr++)
            {
                (void)ps_read_8_int(addr);
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
                (void)ps_read_16_int(addr);
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
                (void)ps_read_32_int(addr);
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
                (void)ps_write_8_int(addr, 0x00);
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
                (void)ps_write_16_int(addr, 0);
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
                (void)ps_write_32_int(addr, 0);
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

void ps_buptest(unsigned int test_size, unsigned int maxiter)
{
    // Initialize RNG
    uint64_t tmp;
    asm volatile("mrs %0, CNTPCT_EL0":"=r"(tmp));

    _seed = tmp;

    kprintf_pc(__putc, NULL, "BUPTest with size %dK requested through commandline\n", test_size / 1024);

    uint32_t frac = test_size / 16;

    uint8_t *garbage = tlsf_malloc(tlsf, test_size);

    ps_write_8_int(0xbfe201, 0x0101);       //CIA OVL
    ps_write_8_int(0xbfe001, 0x0000);       //CIA OVL LOW

    for (unsigned int iter = 0; iter < maxiter; iter++) {
        kprintf_pc(__putc, NULL, "Iteration %d...\n", iter + 1);

        // Fill the garbage buffer and chip ram with random data
        kprintf_pc(__putc, NULL, "  Writing BYTE garbage data to Chip...            ");

        for (uint32_t i = 0; i < test_size; i++) {
            uint8_t val = 0;
            val = rnd();
            garbage[i] = val;
            ps_write_8_int(i, val);

            if ((i % (frac * 2)) == 0)
                kprintf_pc(__putc, NULL, "*");
        }

        for (uint32_t i = 0; i < test_size; i++) {
            uint32_t c = ps_read_8_int(i);
            if (c != garbage[i]) {
                kprintf_pc(__putc, NULL, "\n    READ8: Garbege data mismatch at $%.6X: %.2X should be %.2X.\n", i, c, garbage[i]);
                while(1);
            }

            if ((i % (frac * 4)) == 0)
                kprintf_pc(__putc, NULL, "*");
        }

        for (uint32_t i = 0; i < (test_size) - 2; i += 2) {
            uint32_t c = BE16(ps_read_16_int(i));
            if (c != *((uint16_t *)&garbage[i])) {
                kprintf_pc(__putc, NULL, "\n    READ16_EVEN: Garbege data mismatch at $%.6X: %.4X should be %.4X.\n", i, c, *((uint16_t *)&garbage[i]));
                while(1);
            }

            if ((i % (frac * 4)) == 0)
                kprintf_pc(__putc, NULL, "*");
        }

        for (uint32_t i = 1; i < (test_size) - 2; i += 2) {
            uint32_t c = BE16(ps_read_16_int(i));
            if (c != *((uint16_t *)&garbage[i])) {
                kprintf_pc(__putc, NULL, "\n    READ16_ODD: Garbege data mismatch at $%.6X: %.4X should be %.4X.\n", i, c, *((uint16_t *)&garbage[i]));
                while(1);
            }

            if ((i % (frac * 4)) == 1)
                kprintf_pc(__putc, NULL, "*");
        }
        
        for (uint32_t i = 0; i < (test_size) - 4; i += 2) {
            uint32_t c = BE32(ps_read_32_int(i));
            if (c != *((uint32_t *)&garbage[i])) {
                kprintf_pc(__putc, NULL, "\n    READ32_EVEN: Garbege data mismatch at $%.6X: %.8X should be %.8X.\n", i, c, *((uint32_t *)&garbage[i]));
                while(1);
            }
            
            if ((i % (frac * 4)) == 0)
                kprintf_pc(__putc, NULL, "*");
        }

        for (uint32_t i = 1; i < (test_size) - 4; i += 2) {
            uint32_t c = BE32(ps_read_32_int(i));
            if (c != *((uint32_t *)&garbage[i])) {
                kprintf_pc(__putc, NULL, "\n    READ32_ODD: Garbege data mismatch at $%.6X: %.8X should be %.8X.\n", i, c, *((uint32_t *)&garbage[i]));
                while(1);
            }

            if ((i % (frac * 4)) == 1)
                kprintf_pc(__putc, NULL, "*");
        }

        for (uint32_t i = 0; i < test_size; i++) {
            ps_write_8_int(i, (uint32_t)0x0);

            if ((i % (frac * 8)) == 0)
                kprintf_pc(__putc, NULL, "*");
        }

        kprintf_pc(__putc, NULL, "\n  Writing WORD garbage data to Chip, unaligned... ");
        for (uint32_t i = 1; i < (test_size) - 2; i += 2) {
            uint16_t v = *((uint16_t *)&garbage[i]);
            ps_write_8_int(i + 1, (v & 0x00FF));
            ps_write_8_int(i, (v >> 8));

            if ((i % (frac * 2)) == 1)
                kprintf_pc(__putc, NULL, "*");
        }

        for (uint32_t i = 1; i < (test_size) - 2; i += 2) {
            uint32_t c = BE16((ps_read_8_int(i) << 8) | ps_read_8_int(i + 1));
            if (c != *((uint16_t *)&garbage[i])) {
                kprintf_pc(__putc, NULL, "\n    READ16_ODD: Garbege data mismatch at $%.6X: %.4X should be %.4X.\n", i, c, *((uint16_t *)&garbage[i]));
                while(1);
            }

            if ((i % (frac * 2)) == 1)
                kprintf_pc(__putc, NULL, "*");
        }

        for (uint32_t i = 0; i < test_size; i++) {
            ps_write_8_int(i, (uint32_t)0x0);
        }

        kprintf_pc(__putc, NULL, "\n  Writing LONG garbage data to Chip, unaligned... ");
        for (uint32_t i = 1; i < (test_size) - 4; i += 4) {
            uint32_t v = *((uint32_t *)&garbage[i]);
            ps_write_8_int(i , v & 0x0000FF);
            ps_write_16_int(i + 1, BE16(((v & 0x00FFFF00) >> 8)));
            ps_write_8_int(i + 3 , (v & 0xFF000000) >> 24);

            if ((i % (frac * 2)) == 1)
                kprintf_pc(__putc, NULL, "*");
        }

        for (uint32_t i = 1; i < (test_size) - 4; i += 4) {
            uint32_t c = ps_read_8_int(i);
            c |= (BE16(ps_read_16_int(i + 1)) << 8);
            c |= (ps_read_8_int(i + 3) << 24);
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
            ps_write_64_int(i , v);

            if ((i % (frac * 2)) == 1)
                kprintf_pc(__putc, NULL, "*");
        }

        for (uint32_t i = 1; i < (test_size) - 16; i += 8) {
            uint64_t c = ps_read_64_int(i);
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
