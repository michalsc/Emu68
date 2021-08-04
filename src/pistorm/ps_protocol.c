// SPDX-License-Identifier: MIT

/*
  Original Copyright 2020 Claude Schwarz
  Code reorganized and rewritten by
  Niklas Ekström 2021 (https://github.com/niklasekstrom)
*/

#define PS_PROTOCOL_IMPL

#include <stdint.h>
#include "support.h"
#include "ps_protocol.h"

volatile unsigned int *gpio;
volatile unsigned int *gpclk;

unsigned int gpfsel0;
unsigned int gpfsel1;
unsigned int gpfsel2;

unsigned int gpfsel0_o;
unsigned int gpfsel1_o;
unsigned int gpfsel2_o;

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

static void pistorm_setup_io() {
  gpio = ((volatile unsigned *)BCM2708_PERI_BASE) + GPIO_ADDR / 4;
  gpclk = ((volatile unsigned *)BCM2708_PERI_BASE) + GPCLK_ADDR / 4;
}

static void setup_gpclk() {
  // Enable 200MHz CLK output on GPIO4, adjust divider and pll source depending
  // on pi model
  *(gpclk + (CLK_GP0_CTL / 4)) = LE32(CLK_PASSWD | (1 << 5));
  usleep(10);
  while ((*(gpclk + (CLK_GP0_CTL / 4))) & LE32(1 << 7))
    ;
  usleep(100);
  *(gpclk + (CLK_GP0_DIV / 4)) =
      LE32(CLK_PASSWD | (6 << 12));  // divider , 6=200MHz on pi3
  usleep(10);
  *(gpclk + (CLK_GP0_CTL / 4)) =
      LE32(CLK_PASSWD | 5 | (1 << 4));  // pll? 6=plld, 5=pllc
  usleep(10);
  while (((*(gpclk + (CLK_GP0_CTL / 4))) & LE32(1 << 7)) == 0)
    ;
  usleep(100);

  SET_GPIO_ALT(PIN_CLK, 0);  // gpclk0
}

void ps_setup_protocol() {
  pistorm_setup_io();
  setup_gpclk();

  *(gpio + 10) = LE32(0xffffec);

  *(gpio + 0) = LE32(GPFSEL0_INPUT);
  *(gpio + 1) = LE32(GPFSEL1_INPUT);
  *(gpio + 2) = LE32(GPFSEL2_INPUT);
}

void ps_write_16(unsigned int address, unsigned int data) {
  *(gpio + 0) = LE32(GPFSEL0_OUTPUT);
  *(gpio + 1) = LE32(GPFSEL1_OUTPUT);
  *(gpio + 2) = LE32(GPFSEL2_OUTPUT);

  *(gpio + 7) = LE32(((data & 0xffff) << 8) | (REG_DATA << PIN_A0));
  *(gpio + 7) = LE32(1 << PIN_WR);
  *(gpio + 10) = LE32(1 << PIN_WR);
  *(gpio + 10) = LE32(0xffffec);

  *(gpio + 7) = LE32(((address & 0xffff) << 8) | (REG_ADDR_LO << PIN_A0));
  *(gpio + 7) = LE32(1 << PIN_WR);
  *(gpio + 10) = LE32(1 << PIN_WR);
  *(gpio + 10) = LE32(0xffffec);

  *(gpio + 7) = LE32(((0x0000 | (address >> 16)) << 8) | (REG_ADDR_HI << PIN_A0));
  *(gpio + 7) = LE32(1 << PIN_WR);
  *(gpio + 10) = LE32(1 << PIN_WR);
  *(gpio + 10) = LE32(0xffffec);

  *(gpio + 0) = LE32(GPFSEL0_INPUT);
  *(gpio + 1) = LE32(GPFSEL1_INPUT);
  *(gpio + 2) = LE32(GPFSEL2_INPUT);

  while (*(gpio + 13) & LE32((1 << PIN_TXN_IN_PROGRESS))) {}
}

void ps_write_8(unsigned int address, unsigned int data) {
  if ((address & 1) == 0)
    data = data + (data << 8);  // EVEN, A0=0,UDS
  else
    data = data & 0xff;  // ODD , A0=1,LDS

  *(gpio + 0) = LE32(GPFSEL0_OUTPUT);
  *(gpio + 1) = LE32(GPFSEL1_OUTPUT);
  *(gpio + 2) = LE32(GPFSEL2_OUTPUT);

  *(gpio + 7) = LE32(((data & 0xffff) << 8) | (REG_DATA << PIN_A0));
  *(gpio + 7) = LE32(1 << PIN_WR);
  *(gpio + 10) = LE32(1 << PIN_WR);
  *(gpio + 10) = LE32(0xffffec);

  *(gpio + 7) = LE32(((address & 0xffff) << 8) | (REG_ADDR_LO << PIN_A0));
  *(gpio + 7) = LE32(1 << PIN_WR);
  *(gpio + 10) = LE32(1 << PIN_WR);
  *(gpio + 10) = LE32(0xffffec);

  *(gpio + 7) = LE32(((0x0100 | (address >> 16)) << 8) | (REG_ADDR_HI << PIN_A0));
  *(gpio + 7) = LE32(1 << PIN_WR);
  *(gpio + 10) = LE32(1 << PIN_WR);
  *(gpio + 10) = LE32(0xffffec);

  *(gpio + 0) = LE32(GPFSEL0_INPUT);
  *(gpio + 1) = LE32(GPFSEL1_INPUT);
  *(gpio + 2) = LE32(GPFSEL2_INPUT);

  while (*(gpio + 13) & LE32((1 << PIN_TXN_IN_PROGRESS))) {}
}

void ps_write_32(unsigned int address, unsigned int value) {
  ps_write_16(address, value >> 16);
  ps_write_16(address + 2, value);
}

unsigned int ps_read_16(unsigned int address) {
  *(gpio + 0) = LE32(GPFSEL0_OUTPUT);
  *(gpio + 1) = LE32(GPFSEL1_OUTPUT);
  *(gpio + 2) = LE32(GPFSEL2_OUTPUT);

  *(gpio + 7) = LE32(((address & 0xffff) << 8) | (REG_ADDR_LO << PIN_A0));
  *(gpio + 7) = LE32(1 << PIN_WR);
  *(gpio + 10) = LE32(1 << PIN_WR);
  *(gpio + 10) = LE32(0xffffec);

  *(gpio + 7) = LE32(((0x0200 | (address >> 16)) << 8) | (REG_ADDR_HI << PIN_A0));
  *(gpio + 7) = LE32(1 << PIN_WR);
  *(gpio + 10) = LE32(1 << PIN_WR);
  *(gpio + 10) = LE32(0xffffec);

  *(gpio + 0) = LE32(GPFSEL0_INPUT);
  *(gpio + 1) = LE32(GPFSEL1_INPUT);
  *(gpio + 2) = LE32(GPFSEL2_INPUT);

  *(gpio + 7) = LE32(REG_DATA << PIN_A0);
  *(gpio + 7) = LE32(1 << PIN_RD);

  while (*(gpio + 13) & LE32(1 << PIN_TXN_IN_PROGRESS)) {}
  unsigned int value = LE32(*(gpio + 13));

  *(gpio + 10) = LE32(0xffffec);

  return (value >> 8) & 0xffff;
}

unsigned int ps_read_8(unsigned int address) {
  *(gpio + 0) = LE32(GPFSEL0_OUTPUT);
  *(gpio + 1) = LE32(GPFSEL1_OUTPUT);
  *(gpio + 2) = LE32(GPFSEL2_OUTPUT);

  *(gpio + 7) = LE32(((address & 0xffff) << 8) | (REG_ADDR_LO << PIN_A0));
  *(gpio + 7) = LE32(1 << PIN_WR);
  *(gpio + 10) = LE32(1 << PIN_WR);
  *(gpio + 10) = LE32(0xffffec);

  *(gpio + 7) = LE32(((0x0300 | (address >> 16)) << 8) | (REG_ADDR_HI << PIN_A0));
  *(gpio + 7) = LE32(1 << PIN_WR);
  *(gpio + 10) = LE32(1 << PIN_WR);
  *(gpio + 10) = LE32(0xffffec);

  *(gpio + 0) = LE32(GPFSEL0_INPUT);
  *(gpio + 1) = LE32(GPFSEL1_INPUT);
  *(gpio + 2) = LE32(GPFSEL2_INPUT);

  *(gpio + 7) = LE32(REG_DATA << PIN_A0);
  *(gpio + 7) = LE32(1 << PIN_RD);

  while (*(gpio + 13) & LE32(1 << PIN_TXN_IN_PROGRESS)) {}
  unsigned int value = LE32(*(gpio + 13));

  *(gpio + 10) = LE32(0xffffec);

  value = (value >> 8) & 0xffff;

  if ((address & 1) == 0)
    return (value >> 8) & 0xff;  // EVEN, A0=0,UDS
  else
    return value & 0xff;  // ODD , A0=1,LDS
}

unsigned int ps_read_32(unsigned int address) {
  unsigned int a = ps_read_16(address);
  unsigned int b = ps_read_16(address + 2);
  return (a << 16) | b;
}

void ps_write_status_reg(unsigned int value) {
  *(gpio + 0) = LE32(GPFSEL0_OUTPUT);
  *(gpio + 1) = LE32(GPFSEL1_OUTPUT);
  *(gpio + 2) = LE32(GPFSEL2_OUTPUT);

  *(gpio + 7) = LE32(((value & 0xffff) << 8) | (REG_STATUS << PIN_A0));

  *(gpio + 7) = LE32(1 << PIN_WR);
  *(gpio + 7) = LE32(1 << PIN_WR);  // delay
  *(gpio + 10) = LE32(1 << PIN_WR);
  *(gpio + 10) = LE32(0xffffec);

  *(gpio + 0) = LE32(GPFSEL0_INPUT);
  *(gpio + 1) = LE32(GPFSEL1_INPUT);
  *(gpio + 2) = LE32(GPFSEL2_INPUT);
}

unsigned int ps_read_status_reg() {
  *(gpio + 7) = LE32(REG_STATUS << PIN_A0);
  *(gpio + 7) = LE32(1 << PIN_RD);
  *(gpio + 7) = LE32(1 << PIN_RD);
  *(gpio + 7) = LE32(1 << PIN_RD);
  *(gpio + 7) = LE32(1 << PIN_RD);

  unsigned int value = LE32(*(gpio + 13));

  *(gpio + 10) = LE32(0xffffec);

  return (value >> 8) & 0xffff;
}

void ps_reset_state_machine() {
  ps_write_status_reg(STATUS_BIT_INIT);
  usleep(1500);
  ps_write_status_reg(0);
  usleep(100);
}

void ps_pulse_reset() {
  ps_write_status_reg(0);
  usleep(100000);
  ps_write_status_reg(STATUS_BIT_RESET);
}

unsigned int ps_get_ipl_zero() {
  unsigned int value = (*(gpio + 13));
  return value & LE32(1 << PIN_IPL_ZERO);
}

#define INT2_ENABLED 1

void ps_update_irq() {
  unsigned int ipl = 0;

  if (!ps_get_ipl_zero()) {
    unsigned int status = ps_read_status_reg();
    ipl = (status & 0xe000) >> 13;
  }

  /*if (ipl < 2 && INT2_ENABLED && emu_int2_req()) {
    ipl = 2;
  }*/

  //m68k_set_irq(ipl);
}
