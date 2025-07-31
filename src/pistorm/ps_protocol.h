// SPDX-License-Identifier: MIT

/*
    Code reorganized and rewritten by 
    Niklas Ekström 2021 (https://github.com/niklasekstrom)
*/

#ifndef _PS_PROTOCOL_H
#define _PS_PROTOCOL_H

#include <stdint.h>
#include "support.h"

#ifdef PS_PROTOCOL_IMPL

#define PIN_TXN_IN_PROGRESS 0
#define PIN_IPL_ZERO 1
#define PIN_A0 2
#define PIN_A1 3
#define PIN_CLK 4
#define PIN_RESET 5
#define PIN_RD 6
#define PIN_WR 7
#define PIN_D(x) (8 + x)

#define REG_DATA 0
#define REG_ADDR_LO 1
#define REG_ADDR_HI 2
#define REG_STATUS 3

#define STATUS_BIT_INIT 1
#define STATUS_BIT_RESET 2

#define STATUS_MASK_IPL 0xe000
#define STATUS_SHIFT_IPL 13

// PERIIOBASE set equally for all RasPi targets, mapped there by Emu68
#define BCM2708_PERI_BASE 0xF2000000  
#define BCM2708_PERI_SIZE 0x01000000

#define GPIO_ADDR 0x200000 /* GPIO controller */
#define GPCLK_ADDR 0x101000

#define GPIO_BASE (BCM2708_PERI_BASE + 0x200000) /* GPIO controller */
#define GPCLK_BASE (BCM2708_PERI_BASE + 0x101000)

#define CLK_PASSWD 0x5a000000
#define CLK_GP0_CTL 0x070
#define CLK_GP0_DIV 0x074

// GPIO setup macros. Always use INP_GPIO(x) before using OUT_GPIO(x) or
// SET_GPIO_ALT(x,y)
#define INP_GPIO(g) *(gpio + ((g) / 10)) &= ~(7 << (((g) % 10) * 3))
#define OUT_GPIO(g) *(gpio + ((g) / 10)) |= (1 << (((g) % 10) * 3))
#define SET_GPIO_ALT(g, a)  \
  *(gpio + (((g) / 10))) |= \
      LE32((((a) <= 3 ? (a) + 4 : (a) == 4 ? 3 : 2) << (((g) % 10) * 3)))

#define GPIO_PULL *(gpio + 37)      // Pull up/pull down
#define GPIO_PULLCLK0 *(gpio + 38)  // Pull up/pull down clock

// Pins for MDIO
#define PIN_RGMII_MDIO  28
#define PIN_RGMII_MDC   29

#define CLEAR_BITS_PI3 0xffffec
#define CLEAR_BITS_PI4 (0xffffec & ~((1 << PIN_RGMII_MDIO) | (1 << PIN_RGMII_MDC)))

#define GPFSEL0_INPUT_PI3 0x00244240
#define GPFSEL1_INPUT_PI3 0x00000000
#define GPFSEL2_INPUT_PI3 (0x00000000 | (1 << 18) | (1 << 21) | (1 << 27))

#define GPFSEL0_OUTPUT_PI3 0x09244240
#define GPFSEL1_OUTPUT_PI3 0x09249249
#define GPFSEL2_OUTPUT_PI3 (0x00000249 | (1 << 18) | (1 << 21) | (1 << 27))

#define GPFSEL0_INPUT_PI4 0x00244240
#define GPFSEL1_INPUT_PI4 0x00000000
#define GPFSEL2_INPUT_PI4 (0x00000000 | (1 << 18) | (1 << 21) | (2 << 27) | (2 << 24))

#define GPFSEL0_OUTPUT_PI4 0x09244240
#define GPFSEL1_OUTPUT_PI4 0x09249249
#define GPFSEL2_OUTPUT_PI4 (0x00000249 | (1 << 18) | (1 << 21) | (2 << 27) | (2 << 24))

#endif

unsigned int ps_read_8(unsigned int address);
unsigned int ps_read_16(unsigned int address);
unsigned int ps_read_32(unsigned int address);
uint64_t ps_read_64(unsigned int address);
uint128_t ps_read_128(unsigned int address);

void ps_write_8(unsigned int address, unsigned int data);
void ps_write_16(unsigned int address, unsigned int data);
void ps_write_32(unsigned int address, unsigned int data);
void ps_write_64(unsigned int address, uint64_t data);
void ps_write_128(unsigned int address, uint128_t data);

void ps_buptest(unsigned int size_kb, unsigned int maxiter);

unsigned int ps_read_status_reg();
void ps_write_status_reg(unsigned int value);

void ps_setup_protocol();
void ps_reset_state_machine();
void ps_pulse_reset();

void bitbang_putByte(uint8_t byte);
void fastSerial_putByte(uint8_t byte);
void fastSerial_init();
void ps_housekeeper();
unsigned int ps_get_ipl_zero();

void wb_task();
void wb_init();
void wb_waitfree();
void wb_push(uint32_t address, uint32_t value, uint8_t size);
void ps_efinix_load(char* buffer, long length);
void ps_efinix_setup();

#endif /* _PS_PROTOCOL_H */
