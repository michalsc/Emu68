/*
    Copyright © 2020 Michal Schulz <michal.schulz@gmx.de>
    https://github.com/michalsc

    This Source Code Form is subject to the terms of the
    Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed
    with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <stdint.h>
#include "support.h"
#include "devicetree.h"

struct mmu_page
{
    union
    {
        struct mmu_page *mp_next;
        uint64_t mp_entries[512];
    };
};

static struct mmu_page *mmu_free_pages;

void *get_4k_page()
{
    struct mmu_page *p = NULL;

    /* Check if there is a free 4k page */
    if (!mmu_free_pages)
    {
        of_node_t *e = dt_find_node("/memory");
        if (e)
        {
            of_property_t *p = dt_find_property(e, "reg");
            uint32_t *range = p->op_value;
            int size_cells = dt_get_property_value_u32(e, "#size-cells", 1, TRUE);
            int address_cells = dt_get_property_value_u32(e, "#address-cells", 1, TRUE);
            int addr_pos = address_cells - 1;
            int size_pos = address_cells + size_cells - 1;

            range[size_pos] = BE32(BE32(range[size_pos])-(1 << 21));
            uintptr_t mmu_ploc = BE32(range[addr_pos]) + BE32(range[size_pos]);

            mmu_ploc |= 0xffffffff00000000;

            mmu_free_pages = (void *)mmu_ploc;
            mmu_free_pages->mp_next = NULL;

            for (int i=0; i < 511; i++)
            {
                struct mmu_page *last = mmu_free_pages;
                mmu_free_pages = mmu_free_pages + 1;
                mmu_free_pages->mp_next = last;
            }
        }
    }

    if (mmu_free_pages)
    {
        /* Get pointer to next free 4k page */
        p = mmu_free_pages;

        /* Update pointer to free pages */
        mmu_free_pages = p->mp_next;
    }

    return p;
}

void free_4k_page(void *page)
{
    struct mmu_page *p = page;

    p->mp_next = mmu_free_pages;
    mmu_free_pages = p;
}
