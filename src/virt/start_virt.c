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
#include "mmu.h"
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

    mmu_map(0x09000000, 0x09000000, 0x00001000, 
        MMU_ACCESS | MMU_NS | MMU_ALLOW_EL0 | MMU_ATTR(1), 0);
    mmu_map(0x09000000, 0xf2201000, 0x00001000,
        MMU_ACCESS | MMU_NS | MMU_ALLOW_EL0 | MMU_ATTR(1), 0);
}

void platform_post_init()
{

}
