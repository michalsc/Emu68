/*
    Copyright © 2019 Michal Schulz <michal.schulz@gmx.de>
    https://github.com/michalsc

    This Source Code Form is subject to the terms of the
    Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed
    with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <stdint.h>
#include <stdarg.h>

#include "support.h"

#define UART_RBR        0x0000
#define UART_THR        0x0000
#define UART_DLL        0x0000
#define UART_DLH        0x0004
#define UART_IER        0x0004
#define UART_IIR        0x0008
#define UART_FCR        0x0008
#define UART_LCR        0x000c
#define UART_MCR        0x0010
#define UART_LSR        0x0014
#define UART_MSR        0x0018
#define UART_SCR        0x001c
#define UART_SRBR       0x0030
#define UART_STHR       0x006c
#define UART_FAR        0x0070
#define UART_TFR        0x0074
#define UART_RFW        0x0078
#define UART_USR        0x007c
#define UART_TFL        0x0080
#define UART_RFL        0x0084
#define UART_SRR        0x0088
#define UART_SRTS       0x008c
#define UART_SBCR       0x0090
#define UART_SDMAM      0x0094
#define UART_SFE        0x0098
#define UART_SRT        0x009c
#define UART_STET       0x00a0
#define UART_HTX        0x00a4
#define UART_DMASA      0x00a8
#define UART_CPR        0x00f4
#define UART_UCV        0x00f8
#define UART_CTR        0x00fc

static int serial_up = 0;

static inline void waitSerOUT(void *io_base)
{
    while(1)
    {
       if ((rd32le((intptr_t)io_base + UART_LSR) & (1 << 5)) != 0) break;
    }
}

static inline void putByte(void *io_base, char chr)
{
    if (serial_up)
    {
        waitSerOUT(io_base);

        if (chr == '\n')
        {
            wr32le((intptr_t)io_base + UART_THR, '\r');
            waitSerOUT(io_base);
        }
        wr32le((intptr_t)io_base + UART_THR, (uint8_t)chr);
        waitSerOUT(io_base);
    }
}

void kprintf(const char * restrict format, ...)
{
    va_list v;
    va_start(v, format);
    vkprintf_pc(putByte, (void*)0xff1a0000, format, v);
    va_end(v);
}

void vkprintf(const char * restrict format, va_list args)
{
    vkprintf_pc(putByte, (void*)0xff1a0000, format, args);
}

void setup_serial()
{
    serial_up = 1;
}
