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

static int serial_up = 0;

static inline void waitSerOUT(void *io_base)
{
    (void)io_base;
    while(1)
    {
//       if ((rd32le((intptr_t)io_base + UART_LSR) & (1 << 5)) != 0) break;
    }
}

static inline void putByte(void *io_base, char chr)
{
    (void)io_base;
    (void)chr;
    if (serial_up)
    {
/*
        waitSerOUT(io_base);

        if (chr == '\n')
        {
            wr32le((intptr_t)io_base + UART_THR, '\r');
            waitSerOUT(io_base);
        }
        wr32le((intptr_t)io_base + UART_THR, (uint8_t)chr);
        waitSerOUT(io_base);
*/
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
/*
    uint32_t tmp;

    tmp = rd32le(0xff1a0000 + UART_LCR);

    while(rd32le(0xff1a0000 + UART_USR) & 1);
    wr32le(0xff1a0000 + UART_LCR, tmp | 0x80);
    wr32le(0xff1a0000 + UART_DLL, 13);
    wr32le(0xff1a0000 + UART_DLH, 0);
    wr32le(0xff1a0000 + UART_LCR, tmp);

    serial_up = 1;
*/
}
