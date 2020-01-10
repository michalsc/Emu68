/*
    Copyright © 2019 Michal Schulz <michal.schulz@gmx.de>
    https://github.com/michalsc

    This Source Code Form is subject to the terms of the
    Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed
    with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <stdarg.h>
#include <stdint.h>
#include "A64.h"
#include "config.h"
#include "support.h"
#include "tlsf.h"
#include "devicetree.h"
#include "M68k.h"
#include "HunkLoader.h"
#include "DuffCopy.h"
#include "EmuLogo.h"
#include "EmuFeatures.h"
#include "RegisterAllocator.h"

extern uint64_t mmu_user_L1[512];
extern uint64_t mmu_user_L2[4*512];

void platform_init()
{
    /*
        Prepare mapping for peripherals. Use and update the data from device tree here
        All peripherals are mapped in the lower 4G address space so that they can be
        accessed from m68k.
    */
    uint32_t start_map = 0xf80 / 2;

    uint32_t addr_bus = 0xf8000000;
    uint32_t addr_cpu = 0xf8000000;
    uint32_t addr_len = 0x08000000;

    if (addr_len < 0x00200000)
        addr_len = 0x00200000;

    kprintf("bus: %08x, cpu: %08x, len: %08x\n", addr_bus, addr_cpu, addr_len);

    /* Prepare mapping - device type */
    for (unsigned i=0; i < (addr_len >> 21); i++)
    {
        /* Strongly-ordered device, uncached, 16MB region */
        mmu_user_L2[start_map + i] = ((i << 21) + addr_cpu) | 0x745;
    }
}
