/*
    Copyright © 2020 Michal Schulz <michal.schulz@gmx.de>
    https://github.com/michalsc

    This Source Code Form is subject to the terms of the
    Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed
    with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <stdint.h>
#include "mmu.h"
#include "support.h"
#include "devicetree.h"

#define DV2P(x) /* x */
#define DMAP(x) /* x */

struct mmu_page
{
    union
    {
        struct mmu_page *mp_next;
        uint64_t mp_entries[512];
    };
};

/* L1 table for bottom half. Filled from startup code */
__attribute__((used, section(".mmu"))) struct mmu_page mmu_user_L1;

/* L1 table for top half */
__attribute__((used, section(".mmu"))) struct mmu_page mmu_kernel_L1;
/* One additional directory to map the 1GB kernel address space in 2MB pages here */
__attribute__((used, section(".mmu"))) struct mmu_page mmu_kernel_L2;

static struct mmu_page *mmu_free_pages;

static void *get_4k_page()
{
    struct mmu_page *p = NULL;

    /* Check if there is a free 4k page */
    if (!mmu_free_pages)
    {
        /* No more 4K pages to use? Grab topmost 2MB of RAM */
        of_node_t *e = dt_find_node("/memory");
        if (e)
        {
            of_property_t *p = dt_find_property(e, "reg");
            uint32_t *range = p->op_value;
            int size_cells = dt_get_property_value_u32(e, "#size-cells", 1, TRUE);
            int address_cells = dt_get_property_value_u32(e, "#address-cells", 1, TRUE);
            int addr_pos = address_cells - 1;
            int size_pos = address_cells + size_cells - 1;

            /* Decrease the size of memory block by 2MB */
            range[size_pos] = BE32(BE32(range[size_pos])-(1 << 21));
            uintptr_t mmu_ploc = BE32(range[addr_pos]) + BE32(range[size_pos]);

            /*
                Perform OR of the range address with 0xffffffff << 32, that way it will
                be 1:1 mapped to VA in an uncached region
            */
            mmu_ploc |= 0xffffffff00000000;

            /* Chain the new 512 4K pages in our page pool */
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

    /* Now try to grab new 4K page */
    if (mmu_free_pages)
    {
        /* Get pointer to next free 4k page */
        p = mmu_free_pages;

        /* Update pointer to free pages */
        mmu_free_pages = p->mp_next;
    }

    return p;
}

static void free_4k_page(void *page)
{
    struct mmu_page *p = page;

    /* Put the 4K page back to the pool */
    p->mp_next = mmu_free_pages;
    mmu_free_pages = p;
}

uintptr_t mmu_virt2phys(uintptr_t addr)
{
    uintptr_t phys = 0;
    uint64_t *tbl = NULL;
    int idx_l1, idx_l2, idx_l3;
    uint64_t tmp;

    DV2P(kprintf("virt2phys(%p)\n", addr));

    if (addr & 0xffff000000000000) {
        DV2P(kprintf("selecting kernel tables\n"));
        asm volatile("mrs %0, TTBR1_EL1":"=r"(tbl));
        tbl = (uint64_t *)((uintptr_t)tbl | 0xffffffff00000000);
    } else {
        DV2P(kprintf("selecting user tables\n"));
        asm volatile("mrs %0, TTBR0_EL1":"=r"(tbl));
        tbl = (uint64_t *)((uintptr_t)tbl | 0xffffffff00000000);
    }

    DV2P(kprintf("L1 table: %p\n", tbl));

    idx_l1 = (addr >> 30) & 0x1ff;
    idx_l2 = (addr >> 21) & 0x1ff;
    idx_l3 = (addr >> 12) & 0x1ff;

    DV2P(kprintf("idx_l1 = %d, idx_l2 = %d, idx_l3 = %d\n", idx_l1, idx_l2, idx_l3));

    tmp = tbl[idx_l1];
    DV2P(kprintf("item in L1 table: %016x\n", tmp));
    if (tmp & 1)
    {
        DV2P(kprintf("is valid\n"));
        if (tmp & 2) {
            tbl = (uint64_t *)((tmp & 0x0000fffffffff000) | 0xffffffff00000000);
            DV2P(kprintf("L2 table at %p\n", tbl));

            tmp = tbl[idx_l2];
            DV2P(kprintf("item in L2 table: %016x\n", tmp));
            if (tmp & 1)
            {
                DV2P(kprintf("is valid\n"));
                if (tmp & 2)
                {
                    tbl = (uint64_t *)((tmp & 0x0000fffffffff000) | 0xffffffff00000000);
                    DV2P(kprintf("L3 table at %p\n", tbl));

                    tmp = tbl[idx_l3];
                    DV2P(kprintf("item in L3 table: %016x\n", tmp));
                    if ((tmp & 3) == 3)
                    {
                        DV2P(kprintf("is valid 4K page\n"));
                        phys = (tmp & 0xfffffffff000) + (addr & 0xfff);
                    }
                    else {
                        DV2P(kprintf("invalid!\n"));
                        return -1;
                    }
                }
                else {
                    DV2P(kprintf("2MB page!\n"));
                    phys = (tmp & 0xffffffe00000) + (addr & 0x1fffff);
                }
            }
            else
            {
                DV2P(kprintf("invalid!\n"));
                return -1;
            }
        } else {
            DV2P(kprintf("1GB page!\n"));
            phys = (tmp & 0xffffc0000000) + (addr & 0x3fffffff);
        }
    }
    else {
        DV2P(kprintf("invalid!\n"));
        return -1;
    }

    DV2P(kprintf("returning %p\n", phys));

    return phys;
}

void mmu_init()
{
    /*
        At this stage the user space memory is not set up yet, but the kernel runs in high address
        space, so it is safe to adjust the MMU tables for lower region. Here, only peripherals are
        mapped, but the rest will come very soon.
    */

    mmu_user_L1.mp_entries[0] = 0;
    mmu_user_L1.mp_entries[1] = 0;
    mmu_user_L1.mp_entries[2] = 0;
    mmu_user_L1.mp_entries[3] = 0;

    arm_flush_cache((intptr_t)&mmu_user_L1, sizeof(mmu_user_L1));
    arm_flush_cache((intptr_t)&mmu_kernel_L1, sizeof(mmu_kernel_L1));
    arm_flush_cache((intptr_t)&mmu_kernel_L2, sizeof(mmu_kernel_L2));
}

void put_2m_page(uintptr_t phys, uintptr_t virt, uint32_t attr_low, uint32_t attr_high)
{
    struct mmu_page *tbl;
    int idx_l2, idx_l1;

    if (virt & 0xffff000000000000) {
        asm volatile("mrs %0, TTBR1_EL1":"=r"(tbl));
        tbl = (struct mmu_page *)((uintptr_t)tbl | 0xffffffff00000000);
    } else {
        asm volatile("mrs %0, TTBR0_EL1":"=r"(tbl));
        tbl = (struct mmu_page *)((uintptr_t)tbl | 0xffffffff00000000);
    }

    DMAP(kprintf("put_2m_page(%p, %p, %03x, %03x)\n", phys, virt, attr_low, attr_high));

    idx_l1 = (virt >> 30) & 0x1ff;
    idx_l2 = (virt >> 21) & 0x1ff;

    uint64_t tbl_2 = tbl->mp_entries[idx_l1];
    struct mmu_page *p = NULL;

    if ((tbl_2 & 3) == 0)
    {
        DMAP(kprintf("L1 is empty. Creating L2 directory\n"));

        p = get_4k_page();

        for (int i=0; i < 512; i++)
            p->mp_entries[i] = 0;

        tbl->mp_entries[idx_l1] = 3 | ((uintptr_t)p & 0xffffffff);

    }
    else if ((tbl_2 & 3) == 1)
    {
        DMAP(kprintf("L1 is a 1GB page. Changing to L2 directory\n"));

        p = get_4k_page();

        for (int i=0; i < 512; i++)
            p->mp_entries[i] = (tbl_2 & 0xc0000fff) + (i << 21);

        tbl->mp_entries[idx_l1] = 3 | ((uintptr_t)p & 0xffffffff);
    }
    else
    {
        DMAP(kprintf("L1 is a link to L2 directory. All ok\n"));

        p = (struct mmu_page *)((tbl_2 & 0xfffff000) | 0xffffffff00000000);
    }

    if ((p->mp_entries[idx_l2] & 3) == 3)
    {
        struct mmu_page *l3 = (struct mmu_page *)(0xffffffff00000000 | ((p->mp_entries[idx_l2] & 0xfffff000)));
        DMAP(kprintf("L2 entry was pointing to L3 directory. Freeing it now \n"));
        free_4k_page(l3);
    }

    p->mp_entries[idx_l2] = phys & 0x0000ffffffe00000;
    p->mp_entries[idx_l2] |= attr_low | MMU_PAGE;
    p->mp_entries[idx_l2] |= ((uint64_t)attr_high) << 52;

    DMAP(kprintf("L2[%d] = %016x\n", idx_l2, p->mp_entries[idx_l2]));
}

void put_4k_page(uintptr_t phys, uintptr_t virt, uint32_t attr_low, uint32_t attr_high)
{
    struct mmu_page *tbl;
    int idx_l3, idx_l2, idx_l1;

    if (virt & 0xffff000000000000) {
        asm volatile("mrs %0, TTBR1_EL1":"=r"(tbl));
        tbl = (struct mmu_page *)((uintptr_t)tbl | 0xffffffff00000000);
    } else {
        asm volatile("mrs %0, TTBR0_EL1":"=r"(tbl));
        tbl = (struct mmu_page *)((uintptr_t)tbl | 0xffffffff00000000);
    }

    DMAP(kprintf("put_4k_page(%p, %p, %03x, %03x)\n", phys, virt, attr_low, attr_high));

    idx_l1 = (virt >> 30) & 0x1ff;
    idx_l2 = (virt >> 21) & 0x1ff;
    idx_l3 = (virt >> 12) & 0x1ff;

    uint64_t tbl_2 = tbl->mp_entries[idx_l1];
    struct mmu_page *p = NULL;

    if ((tbl_2 & 3) == 0)
    {
        DMAP(kprintf("L1 is empty. Creating L2 directory\n"));

        p = get_4k_page();

        for (int i=0; i < 512; i++)
            p->mp_entries[i] = 0;

        tbl->mp_entries[idx_l1] = 3 | ((uintptr_t)p & 0xffffffff);

    }
    else if ((tbl_2 & 3) == 1)
    {
        DMAP(kprintf("L1 is a 1GB page. Changing to L2 directory\n"));

        p = get_4k_page();

        for (int i=0; i < 512; i++)
            p->mp_entries[i] = (tbl_2 & 0xc0000fff) + (i << 21);

        tbl->mp_entries[idx_l1] = 3 | ((uintptr_t)p & 0xffffffff);
    }
    else
    {
        DMAP(kprintf("L1 is a link to L2 directory. All ok\n"));

        p = (struct mmu_page *)((tbl_2 & 0xfffff000) | 0xffffffff00000000);
    }

    tbl = p;

    DMAP(kprintf("L2[%d] = %016x\n", idx_l2, p->mp_entries[idx_l2]));
    uint64_t tbl_3 = tbl->mp_entries[idx_l2];

    if ((tbl_3 & 3) == 0)
    {
        DMAP(kprintf("L2 is empty. Creating L3 directory\n"));

        p = get_4k_page();

        for (int i=0; i < 512; i++)
            p->mp_entries[i] = 0;

        tbl->mp_entries[idx_l2] = 3 | ((uintptr_t)p & 0xffffffff);
    }
    else if ((tbl_3 & 3) == 1)
    {
        DMAP(kprintf("L2 is a 2MB page. Changing to L3 directory\n"));

        p = get_4k_page();

        for (int i=0; i < 512; i++)
            p->mp_entries[i] = (tbl_3 & 0xffe00fff) + (i << 12);

        tbl->mp_entries[idx_l2] = 3 | ((uintptr_t)p & 0xffffffff);
    }
    else
    {
        DMAP(kprintf("L2 is a link to L3 directory. All ok\n"));

        p = (struct mmu_page *)((tbl_3 & 0xfffff000) | 0xffffffff00000000);
    }

    p->mp_entries[idx_l3] = phys & 0x0000fffffffff000;
    p->mp_entries[idx_l3] |= attr_low | MMU_PAGE;
    p->mp_entries[idx_l3] |= ((uint64_t)attr_high) << 48;

    DMAP(kprintf("L3[%d] = %016x\n", idx_l3, p->mp_entries[idx_l3]));
}

void mmu_map(uintptr_t phys, uintptr_t virt, uintptr_t length, uint32_t attr_low, uint32_t attr_high)
{
    DMAP(kprintf("mmu_map(%p, %p, %x, %04x00000000%04x)\n", phys, virt, length, attr_high, attr_low));

    /* Align virt up to 2M boundary with 4K pages */
    while ((virt & 0x1fffff) && (length >= 4096))
    {
        put_4k_page(phys, virt, attr_low, attr_high);
        phys += 4096;
        virt += 4096;
        length -= 4096;
    }

    /* Now check if phys is sill aligned to 2M boundary. If not, continue using 4K pages */
    if ((phys & 0x1fffff) == 0)
    {
        /* Phys was aligned. Continue pushing 2M pages */
        while (length >= 2*1024*1024)
        {
            put_2m_page(phys, virt, attr_low, attr_high);
            phys += 2*1024*1024;
            virt += 2*1024*1024;
            length -= 2*1024*1024;
        }
    }

    /* Put the rest using 4K pages */
    while (length >= 4096)
    {
        put_4k_page(phys, virt, attr_low, attr_high);
        phys += 4096;
        virt += 4096;
        length -= 4096;
    }
}

void mmu_unmap(uintptr_t virt, uintptr_t length)
{
    (void)virt;
    (void)length;
    DMAP(kprintf("mmu_unmap(%p, %x)\n", virt, length));

}
