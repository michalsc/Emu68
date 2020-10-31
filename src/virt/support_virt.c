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

#define UARTDR      0x000
#define UARTRSR     0x004
#define UARTECR     0x004
#define UARTFR      0x018
#define UARTILPR    0x020
#define UARTIBRD    0x024
#define UARTFBRD    0x028

#define UARTFR_CTS  0x001
#define UARTFR_DSR  0x002
#define UARTFR_DCD  0x004
#define UARTFR_BUSY 0x008
#define UARTFR_RXFE 0x010
#define UARTFR_TXFF 0x020
#define UARTFR_RXFF 0x040
#define UARTFR_TXFE 0x080
#define UARTFR_RI   0x100

static inline void waitSerOUT(void *io_base)
{
    (void)io_base;
    while(1)
    {
       if ((rd32le((intptr_t)io_base + UARTFR) & UARTFR_TXFF) == 0) break;
    }
}

static inline void putByte(void *io_base, char chr)
{
    if (serial_up)
    {
        waitSerOUT(io_base);

        if (chr == '\n')
        {
            wr32le((intptr_t)io_base + UARTDR, '\r');
            waitSerOUT(io_base);
        }
        wr32le((intptr_t)io_base + UARTDR, (uint8_t)chr);
        waitSerOUT(io_base);
    }
}

void kprintf(const char * restrict format, ...)
{
    va_list v;
    va_start(v, format);
    vkprintf_pc(putByte, (void*)0x09000000, format, v);
    va_end(v);
}

void vkprintf(const char * restrict format, va_list args)
{
    vkprintf_pc(putByte, (void*)0x09000000, format, args);
}

void setup_serial()
{
    serial_up = 1;
/*
	of_node_t *aliases = dt_find_node("/aliases");
    if (aliases)
    {
        of_property_t *seralias = dt_find_property(aliases, "serial1");
        if (seralias)
        {
            of_node_t *serial = dt_find_node(seralias->op_value);

            if (serial)
            {
                ULONG *reg = dt_find_property(serial, "reg")->op_value;
                IPTR phys_base = reg[0];
                ULONG *ranges = dt_find_property(serial->on_parent, "ranges")->op_value;
                phys_base -= ranges[0];
                phys_base += ranges[1];
                
                pl011_base = (void *)phys_base;
            }
        }
    }
*/
}
