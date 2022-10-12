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

#undef PIN_RD
#undef PIN_D
#undef GPFSEL0_INPUT
#undef GPFSEL1_INPUT
#undef GPFSEL2_INPUT
#undef GPFSEL0_OUTPUT
#undef GPFSEL1_OUTPUT
#undef GPFSEL2_OUTPUT
#undef REG_ADDR_LO
#undef REG_ADDR_HI
#undef REG_STATUS

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
#define GO(i)           FUNC(i, OUT)

#define SER_OUT_BIT             2
#define SER_OUT_CLK             3
#define PIN_IPL_ZERO            1

#define PIN_RD                  4
#define PIN_WR                  7
#define PIN_D(x)                ((x) < 9 ? (8 + (x)) : (9 + (x)))
#define PIN_A(x)                (25 + (x))

#define SPLIT_DATA(x)   ((((x) & 0xfe00) << 1) | ((x) & 0x1ff))
#define MERGE_DATA(x)   ((((x) & 0x1fc00) >> 1) | ((x) & 0x1ff))

#define GPFSEL0_INPUT (GO(PIN_WR) | GO(PIN_RD) | GO(SER_OUT_BIT) | GO(SER_OUT_CLK))
#define GPFSEL1_INPUT (0)
#define GPFSEL2_INPUT (GO(PIN_A(2)) | GO(PIN_A(1)) | GO(PIN_A(0)))

#define GPFSEL0_OUTPUT (GO(PIN_D(1)) | GO(PIN_D(0)) | GO(PIN_WR) | GO(PIN_RD) | GO(SER_OUT_BIT) | GO(SER_OUT_CLK))
#define GPFSEL1_OUTPUT (GO(PIN_D(10)) | GO(PIN_D(9)) | GO(PIN_D(8)) | GO(PIN_D(7)) | GO(PIN_D(6)) | GO(PIN_D(5)) | GO(PIN_D(4)) | GO(PIN_D(3)) | GO(PIN_D(2)))
#define GPFSEL2_OUTPUT (GO(PIN_A(2)) | GO(PIN_A(1)) | GO(PIN_A(0)) | GO(PIN_D(15)) | GO(PIN_D(14)) | GO(PIN_D(13)) | GO(PIN_D(12)) | GO(PIN_D(11)))

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

#define REG_DATA_LO     0
#define REG_DATA_HI     1
#define REG_ADDR_LO     2
#define REG_ADDR_HI     3
#define REG_STATUS      4
#define REG_CONTROL     4

#define TXN_SIZE_SHIFT  8
#define TXN_RW_SHIFT    10
#define TXN_FC_SHIFT    11

#define SIZE_BYTE       0
#define SIZE_WORD       1
#define SIZE_LONG       3

#define TXN_READ        (1 << TXN_RW_SHIFT)
#define TXN_WRITE       (0 << TXN_RW_SHIFT)


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

void fastSerial_init()
{
    if (!gpio)
        gpio = ((volatile unsigned *)BCM2708_PERI_BASE) + GPIO_ADDR / 4;
  
    /* Leave FS_CLK and FS_DO high */
    *(gpio + 7) = LE32(1 << SER_OUT_BIT);
    *(gpio + 7) = LE32(1 << SER_OUT_CLK);

    for (int i=0; i < 16; i++) {
        /* Clock down */
        *(gpio + 10) = LE32(1 << SER_OUT_CLK);
        
        /* Clock up */
        *(gpio + 7) = LE32(1 << SER_OUT_CLK);
    }
}

void fastSerial_putByte(uint8_t byte)
{
    if (!gpio)
        gpio = ((volatile unsigned *)BCM2708_PERI_BASE) + GPIO_ADDR / 4;
  
    /* Wait for CTS to go high */
    //while (0 == (*(gpio + 13) & LE32(FS_CTS))) {}

    /* Start bit */
    *(gpio + 10) = LE32(1 << SER_OUT_BIT);
    *(gpio + 10) = LE32(1 << SER_OUT_BIT);

    /* Clock down */
    *(gpio + 10) = LE32(1 << SER_OUT_CLK);
    *(gpio + 10) = LE32(1 << SER_OUT_CLK);

    /* Clock up */
    *(gpio + 7) = LE32(1 << SER_OUT_CLK);
    *(gpio + 7) = LE32(1 << SER_OUT_CLK);

    for (int i=0; i < 8; i++) {
        if (byte & 1) {
            *(gpio + 7) = LE32(1 << SER_OUT_BIT);
            *(gpio + 7) = LE32(1 << SER_OUT_BIT);
        }
        else {
            *(gpio + 10) = LE32(1 << SER_OUT_BIT);
            *(gpio + 10) = LE32(1 << SER_OUT_BIT);
        }

        /* Clock down */
        *(gpio + 10) = LE32(1 << SER_OUT_CLK);
        *(gpio + 10) = LE32(1 << SER_OUT_CLK);

        /* Clock up */
        *(gpio + 7) = LE32(1 << SER_OUT_CLK);
        *(gpio + 7) = LE32(1 << SER_OUT_CLK);
        
        byte = byte >> 1;
    }

    /* DEST bit (0) */
    *(gpio + 10) = LE32(1 << SER_OUT_BIT);
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

static void pistorm_setup_io() {
    gpio = ((volatile unsigned *)BCM2708_PERI_BASE) + GPIO_ADDR / 4;
    gpclk = ((volatile unsigned *)BCM2708_PERI_BASE) + GPCLK_ADDR / 4;
}

void set_input()
{
    *(gpio + 0) = LE32(GPFSEL0_INPUT);
    *(gpio + 1) = LE32(GPFSEL1_INPUT);
    *(gpio + 2) = LE32(GPFSEL2_INPUT);
}

void set_output()
{
    *(gpio + 0) = LE32(GPFSEL0_OUTPUT);
    *(gpio + 1) = LE32(GPFSEL1_OUTPUT);
    *(gpio + 2) = LE32(GPFSEL2_OUTPUT);
}

void ps_setup_protocol()
{
    pistorm_setup_io();
    pistorm_setup_serial();

    usleep(100);

    set_input();
}

void ps_reset_state_machine()
{
}

unsigned int ps_get_ipl_zero()
{
    unsigned int value = *(gpio + 13);
    return value & LE32(1 << PIN_IPL_ZERO);
}

static void write_ps_reg(unsigned int address, unsigned int data) {
  *(gpio + 7) = LE32((SPLIT_DATA(data) << PIN_D(0)) | (address << PIN_A(0)));
  *(gpio + 7) = LE32(1 << PIN_WR);
  *(gpio + 7) = LE32(1 << PIN_WR);
  *(gpio + 7) = LE32(1 << PIN_WR);

  *(gpio + 10) = LE32(0x0fffffff & ~((1 << SER_OUT_BIT) | (1 << SER_OUT_CLK)));
}

static unsigned int read_ps_reg(unsigned int address) {
  *(gpio + 7) = LE32((address << PIN_A(0)) | (1 << PIN_RD));

  unsigned int data = LE32(*(gpio + 13));

  *(gpio + 10) = LE32(0x0fffffff & ~((1 << SER_OUT_BIT) | (1 << SER_OUT_CLK)));

  data = MERGE_DATA(data >> PIN_D(0)) & 0xffff;
  return data;
}

unsigned int get_status()
{
    return read_ps_reg(REG_STATUS);
}

unsigned int get_ipl()
{
    return (read_ps_reg(REG_STATUS) >> STATUS_IPL_SHIFT) & 7;
}

void ps_set_control(unsigned int value) {
  set_output();
  write_ps_reg(REG_CONTROL, 0x8000 | (value & 0x7fff));
  set_input();
}

void ps_clr_control(unsigned int value) {
  set_output();
  write_ps_reg(REG_CONTROL, value & 0x7fff);
  set_input();
}

unsigned int ps_read_status() {
  return read_ps_reg(REG_STATUS);
}

static unsigned int g_fc = 0;

void cpu_set_fc(unsigned int fc) {
  g_fc = fc;
}

static void do_write_access(unsigned int address, unsigned int data, unsigned int size) {
  //kprintf("[PS32] Starting write, address=%08x, data=%08x, size=%d\n", address, data, size);

  set_output();

  write_ps_reg(REG_DATA_LO, data & 0xffff);
  if (size == SIZE_LONG)
    write_ps_reg(REG_DATA_HI, (data >> 16) & 0xffff);

  write_ps_reg(REG_ADDR_LO, address & 0xffff);
  write_ps_reg(REG_ADDR_HI, TXN_WRITE | (g_fc << TXN_FC_SHIFT) | (size << TXN_SIZE_SHIFT) | ((address >> 16) & 0xff));

  set_input();

  *(gpio + 7) = LE32((REG_STATUS << PIN_A(0)) | (1 << PIN_RD));

  while (1) {
    data = LE32(*(gpio + 13));
    data = MERGE_DATA(data >> PIN_D(0)) & 0xffff;
    if (!(data & STATUS_REQ_ACTIVE))
      break;
  }

  *(gpio + 10) = LE32(0x0fffffff & ~((1 << SER_OUT_BIT) | (1 << SER_OUT_CLK)));

  // TODO: Add proper handling of unnormal termination.
  if (!(data & STATUS_TERM_NORMAL)) {
    // Should look at status bits to see if RESET/HALT are asserted.
    // Could also read address to see which bus cycle failed.
    kprintf("[PS32] Write access did not terminate normally, status = %04x\n", data);
    while(1) asm volatile("wfi");
  }

  // TODO: Should look at IPL at this point,
  // and if it has changed then should handle it right away.

  //kprintf("[PS32] Write completed\n");
}

static int do_read_access(unsigned int address, unsigned int size) {
  //kprintf("[PS32] Starting read, address=%08x, size=%d\n", address, size);

    if (address & 0xff000000) {
        return 0xffffffff;
    }
    if ((address & 0x00ff0000) == 0x00f00000) {
        return 0xffffffff;
    }
        

  if (address == 0xf00000)
    usleep(1000);

  set_output();

  write_ps_reg(REG_ADDR_LO, address & 0xffff);
  write_ps_reg(REG_ADDR_HI, TXN_READ | (g_fc << TXN_FC_SHIFT) | (size << TXN_SIZE_SHIFT) | ((address >> 16) & 0xff));

  set_input();

  *(gpio + 7) = LE32((REG_STATUS << PIN_A(0)) | (1 << PIN_RD));

  unsigned int data;

  while (1) {
    data = LE32(*(gpio + 13));
    data = MERGE_DATA(data >> PIN_D(0)) & 0xffff;
    if (!(data & STATUS_REQ_ACTIVE))
      break;
  }

  *(gpio + 10) = LE32(0x0fffffff & ~((1 << SER_OUT_BIT) | (1 << SER_OUT_CLK)));

  // TODO: Add proper handling of unnormal termination.
  if (!(data & STATUS_TERM_NORMAL)) {
    kprintf("[PS32] Read access did not terminate normally, status = %04x\n", data);
    while(1) asm volatile("wfi");
  }

  // TODO: Should look at IPL at this point,
  // and if it has changed then should handle it right away.

  data = read_ps_reg(REG_DATA_LO);
  if (size == SIZE_BYTE)
    data &= 0xff;
  else if (size == SIZE_LONG)
    data |= read_ps_reg(REG_DATA_HI) << 16;

  //kprintf("[PS32] Read completed with data=%08x\n", data);
  return data;
}


void ps_write_8(unsigned int address, unsigned int data) {
  do_write_access(address, data, SIZE_BYTE);
}

void ps_write_16(unsigned int address, unsigned int data) {
  do_write_access(address, data, SIZE_WORD);
}

void ps_write_32(unsigned int address, unsigned int data) {
  do_write_access(address, data, SIZE_LONG);
}

unsigned int ps_read_8(unsigned int address) {
  return do_read_access(address, SIZE_BYTE);
}

unsigned int ps_read_16(unsigned int address) {
  return do_read_access(address, SIZE_WORD);
}

unsigned int ps_read_32(unsigned int address) {
  return do_read_access(address, SIZE_LONG);
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
  usleep(100000);

  kprintf("[PS32] Clear DRIVE_RESET\n");
  ps_clr_control(CONTROL_DRIVE_RESET);

    overlay = 1;
    board = &__boards_start;
    board_idx = 0;
}

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

    kprintf("[HKEEP] PIN = %08x\n", LE32(*(gpio + 13)));

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
