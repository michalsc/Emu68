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
#include "tlsf.h"
#include "A64.h"
#include "devicetree.h"
#include "globals.h"
#include "spinlock.h"

#define DV2P(x) /* if (is_serial_up()) { x ;} */ /* x */
#define DMAP(x) /* if (is_serial_up()) { x ;} */ /* x */

#define DESC_TYPE_MASK      3ULL
#define DESC_BLOCK          1ULL                    /* L1 1GB / L2 2MB block */
#define DESC_TABLE          3ULL                    /* L1/L2 pointer to next level */
#define DESC_PAGE           3ULL                    /* L3 4K page */
#define DESC_CONTIG         (1ULL << 52)
#define DESC_OA_MASK        0x0000fffffffff000ULL
#define DESC_UATTR_MASK     0xfff0000000000000ULL

#define SZ_4K               0x1000ULL
#define SZ_2M               0x200000ULL
#define SZ_1G               0x40000000ULL

#define L1_IDX(v)           (((v) >> 30) & 0x1ff)
#define L2_IDX(v)           (((v) >> 21) & 0x1ff)
#define L3_IDX(v)           (((v) >> 12) & 0x1ff)

#define IS_KERNEL_VA(v)     (((v) & 0xffff000000000000ULL) != 0)

/* Virtual base of physical address space at 0x0 (320GB) */
static const uintptr_t PHYS_VIRT_OFFSET = 0xffffff9000000000;

/* Number of 4K pages in the MMU pool */
#define MMU_POOL_SIZE 2048

struct mmu_page
{
    union
    {
        struct mmu_page *mp_next;
        uint64_t mp_entries[512];
    };
};

/* L1 table of the half selected by virt. TTBR may carry ASID/CnP bits, mask them off */
static inline struct mmu_page *mmu_root(uintptr_t virt)
{
    uintptr_t ttbr;
 
    if (IS_KERNEL_VA(virt))
        __asm__ volatile("mrs %0, TTBR1_EL1":"=r"(ttbr));
    else
        __asm__ volatile("mrs %0, TTBR0_EL1":"=r"(ttbr));
 
    return (struct mmu_page *)((ttbr & DESC_OA_MASK) + PHYS_VIRT_OFFSET);
}

/* Make table stores visible to the table walker before anything links to them */
static inline void mmu_publish(void)
{
    __asm__ volatile("dsb ishst" ::: "memory");
}

/* Flush all TLB entries */
static inline void mmu_tlb_flush_all(void)
{
    __asm__ volatile(
"       dsb     ishst               \n"
"       tlbi    VMALLE1IS           \n"
"       dsb     ish                 \n"
"       isb                         \n" ::: "memory");
}


/* L1 table for bottom half. Filled from startup code */
__attribute__((used, section(".mmu"))) struct mmu_page mmu_user_L1;
/* L1 table for top half */
__attribute__((used, section(".mmu"))) struct mmu_page mmu_kernel_L1;
/* One additional directory to map the 1GB kernel address space in 2MB pages here */
__attribute__((used, section(".mmu"))) struct mmu_page mmu_kernel_L2;

/* Serialize access to MMU functions */
static spinlock_t mmu_lock;

/*
    Pool of free 4K pages for translation tables. Single linked list: allocate = pop head,
    free = push head.
*/
static struct mmu_page *mmu_free_pages;

/*
    Tables unlinked during the current map/unmap. They may still be cached by the table walker
    of any core, so they go back to the pool only after the TLB invalidate in mmu_commit().
    Kept as a list with a tail pointer, so returning them is an O(1) splice.
*/
static struct mmu_page *mmu_retired_head;
static struct mmu_page *mmu_retired_tail;

static int mmu_pages_free = 0;
static int mmu_pages_used = 0;
static int mmu_pages_retired = 0;
static int mmu_pages_used_max = 0;

void mmu_print_stats(void)
{
    kprintf("[MMU] Pages free: %d, used: %d, retired: %d, max used: %d\n",
            mmu_pages_free, mmu_pages_used, mmu_pages_retired, mmu_pages_used_max);
}

static void mmu_pool_init(uintptr_t va, uintptr_t size)
{
    for (uintptr_t p = va; p < va + size; p += sizeof(struct mmu_page))
    {
        struct mmu_page *page = (struct mmu_page *)p;
        page->mp_next = mmu_free_pages;
        mmu_free_pages = page;
        mmu_pages_free++;
    }
}

static struct mmu_page* get_4k_page()
{
    struct mmu_page *p = mmu_free_pages;

    /* Check if there is a free 4k page */
    if (!p)
    {
        kprintf("[MMU] Out of 4K pages for MMU tables!\n");
        while(1);
    }

    mmu_free_pages = p->mp_next;

    mmu_pages_free--;
    mmu_pages_used++;
    if (mmu_pages_used > mmu_pages_used_max)
        mmu_pages_used_max = mmu_pages_used;

    return p;
}

/*
    Check if the table is a static slot. In our case this can be only 
    the mmu_kernel_L2 table.
    
    Both L1 tables are root and cannot be freed. All tables **besides** mmu_kernel_L2 
    are allocated from the pool and can be freed.
*/
static inline int is_static_slot(uintptr_t virt)
{
    return IS_KERNEL_VA(virt) && L1_IDX(virt) == 0;
}

/*
    Push an unlinked table on the retired list. Overwriting entry 0 with the link is harmless:
    a kernel VA with bits [1:0] == 0 reads as an invalid descriptor, and the table is already
    unreachable from the live tree.
*/
static void retire_table(struct mmu_page *p)
{
    mmu_pages_retired++;
    p->mp_next = mmu_retired_head;
    if (mmu_retired_head == NULL)
        mmu_retired_tail = p;
    mmu_retired_head = p;
}

/*
    Commit any changes to the MMU tables. This flushes the TLB and, once flush is complete,
    returns retired tables to the free pool. This is safe because the TLB flush ensures that no core
    can be using the retired tables any more.
*/
static void mmu_commit(void)
{
    mmu_tlb_flush_all();
 
    if (mmu_retired_head)
    {
        mmu_retired_tail->mp_next = mmu_free_pages;
        mmu_free_pages = mmu_retired_head;
        mmu_retired_head = NULL;
        mmu_retired_tail = NULL;
        mmu_pages_free += mmu_pages_retired;
        mmu_pages_used -= mmu_pages_retired;
        mmu_pages_retired = 0;
    }
}

/* Convert a MMU page/directory descriptor to a mmu_page pointer */
static inline struct mmu_page *desc_to_table(uint64_t d)
{
    return (struct mmu_page *)((d & DESC_OA_MASK) + PHYS_VIRT_OFFSET);
}

/* Convert a mmu_page pointer to a MMU page/directory descriptor */
static inline uint64_t table_to_desc(struct mmu_page *p)
{
    return ((uintptr_t)p - PHYS_VIRT_OFFSET) | DESC_TABLE;
}

/* Shift and mask low and high attrs to make them or-able with the descriptor*/
static inline uint64_t make_attrs(uint32_t attr_low, uint32_t attr_high)
{
    /* attr_high is the top 16 bits of the descriptor; [51:48] are output address, not attributes */
    return ((uint64_t)attr_low & 0xffc) | (((uint64_t)attr_high << 48) & DESC_UATTR_MASK);
}

/* Break-before-make replacement of a live L2 or L3 descriptor */
static void replace_entry_bbm(uint64_t *entry, uint64_t new_desc)
{
    *entry = 0;
    mmu_tlb_flush_all();
    *entry = new_desc;
    mmu_publish();
}

/*
    The first 4GB of user space shares its L1 entries with user 4..8GB and with the top 4GB of
    kernel space. Collect all slots belonging to the L1 entry of virt.
*/
static int l1_slots(uintptr_t virt, uint64_t *slots[3])
{
    struct mmu_page *l1 = mmu_root(virt);
    int idx = L1_IDX(virt);
    int n = 0;
 
    slots[n++] = &l1->mp_entries[idx];
 
    if (!IS_KERNEL_VA(virt) && idx < 4)
    {
        slots[n++] = &l1->mp_entries[idx + 4];
        slots[n++] = &mmu_root(~0ULL)->mp_entries[508 + idx];
    }
 
    return n;
}

/* Set an L1 descriptor together with all its aliases. With bbm, the old one is broken first */
static void set_l1_entry(uintptr_t virt, uint64_t new_desc, int bbm)
{
    uint64_t *slots[3];
    int n = l1_slots(virt, slots);
 
    if (bbm)
    {
        for (int i=0; i < n; i++) {
            *slots[i] = 0;
        }
        mmu_tlb_flush_all();
    }
 
    for (int i=0; i < n; i++) {
        *slots[i] = new_desc;
    }
 
    mmu_publish();
}


/*
    Make sure *entry (an L1 entry if level == 1, L2 entry if level == 2) points to a table and
    return it. An invalid entry gets a fresh, zeroed table. A block gets split into 512 blocks
    (or pages) of the next level with identical attributes.
*/
static struct mmu_page *ensure_table(uint64_t *entry, int level, uintptr_t virt)
{
    uint64_t d = *entry;
    struct mmu_page *t;
    int was_block = 0;
 
    if ((d & DESC_TYPE_MASK) == DESC_TABLE) {
        return desc_to_table(d);
    }
 
    t = get_4k_page();
 
    if ((d & DESC_TYPE_MASK) == DESC_BLOCK)
    {
        uint64_t step = (level == 1) ? SZ_2M : SZ_4K;
        uint64_t base = (d & ~(DESC_TYPE_MASK | DESC_CONTIG)) | ((level == 1) ? DESC_BLOCK : DESC_PAGE);
 
        DMAP(kprintf("[MMU] L%d entry is a block. Splitting\n", level));
 
        for (int i=0; i < 512; i++) {
            t->mp_entries[i] = base + i * step;
        }
 
        was_block = 1;
    }
    else
    {
        for (int i=0; i < 512; i++) {
            t->mp_entries[i] = 0;
        }
    }
 
    mmu_publish();
 
    if (level == 1) {
        set_l1_entry(virt, table_to_desc(t), was_block);
    } else if (was_block) {
        replace_entry_bbm(entry, table_to_desc(t));
    } else {
        *entry = table_to_desc(t);
    }
 
    return t;
}

/* Free an L2 table together with all L3 tables it points to */
static void retire_l2_tree(struct mmu_page *l2)
{
    for (int i=0; i < 512; i++)
    {
        if ((l2->mp_entries[i] & DESC_TYPE_MASK) == DESC_TABLE) {
            retire_table(desc_to_table(l2->mp_entries[i]));
        }
    }
    retire_table(l2);
}


/*
    Called on an L2 entry after its 2MB window was modified. If it points to an L3 table:
    - all 512 entries invalid -> drop the table,
    - 512 pages with physically contiguous, 2MB aligned output and identical attributes -> fold
      into a single 2MB block.
*/
static void tidy_l2_entry(uint64_t *e2)
{
    uint64_t d = *e2;
    struct mmu_page *t;
    uint64_t first;
 
    if ((d & DESC_TYPE_MASK) != DESC_TABLE) return;
 
    t = desc_to_table(d);
    first = t->mp_entries[0];
 
    if (first == 0)
    {
        for (int i=1; i < 512; i++) {
            if (t->mp_entries[i]) return;
        }
 
        DMAP(kprintf("[MMU] L3 table empty, dropping it\n"));
        *e2 = 0;
        retire_table(t);
    }
    else if ((first & DESC_TYPE_MASK) == DESC_PAGE && (first & (SZ_2M - SZ_4K)) == 0)
    {
        for (int i=1; i < 512; i++) {
            if (t->mp_entries[i] != first + i * SZ_4K) return;
        }
 
        DMAP(kprintf("[MMU] L3 table maps contiguous 2MB, merging\n"));
        replace_entry_bbm(e2, (first & ~(DESC_TYPE_MASK | DESC_CONTIG)) | DESC_BLOCK);
        retire_table(t);
    }
}

/* Same as tidy_l2_entry, one level up: drop an empty L2 table or fold it into a 1GB block */
static void tidy_l1_entry(uintptr_t virt)
{
    uint64_t d = mmu_root(virt)->mp_entries[L1_IDX(virt)];
    struct mmu_page *t;
    uint64_t first;
 
    if (is_static_slot(virt) || (d & DESC_TYPE_MASK) != DESC_TABLE) {
        return;
    }
 
    t = desc_to_table(d);
    first = t->mp_entries[0];
 
    if (first == 0)
    {
        for (int i=1; i < 512; i++) {
            if (t->mp_entries[i]) return;
        }
 
        DMAP(kprintf("[MMU] L2 table empty, dropping it\n"));
        set_l1_entry(virt, 0, 0);
        retire_table(t);
    }
    else if ((first & DESC_TYPE_MASK) == DESC_BLOCK && (first & (SZ_1G - SZ_4K)) == 0)
    {
        for (int i=1; i < 512; i++) {
            if (t->mp_entries[i] != first + i * SZ_2M) return;
        }
 
        DMAP(kprintf("[MMU] L2 table maps contiguous 1GB, merging\n"));
        set_l1_entry(virt, first & ~DESC_CONTIG, 1);
        retire_table(t);
    }
}

static void put_2m_page(uintptr_t phys, uintptr_t virt, uint64_t attrs)
{
    struct mmu_page *l2 = ensure_table(&mmu_root(virt)->mp_entries[L1_IDX(virt)], 1, virt);
    uint64_t *e2 = &l2->mp_entries[L2_IDX(virt)];
    uint64_t new_desc = (phys & DESC_OA_MASK & ~(SZ_2M - 1)) | attrs | DESC_BLOCK;
 
    DMAP(kprintf("[MMU] put_2m_page(%p, %p, %016x)\n", phys, virt, attrs));
 
    if ((*e2 & DESC_TYPE_MASK) == DESC_TABLE)
    {
        struct mmu_page *l3 = desc_to_table(*e2);
 
        DMAP(kprintf("[MMU] L2 entry was pointing to L3 directory. Freeing it now\n"));
        replace_entry_bbm(e2, new_desc);
        retire_table(l3);
    }
    else
    {
        *e2 = new_desc;
    }
}

static void put_4k_page(uintptr_t phys, uintptr_t virt, uint64_t attrs)
{
    struct mmu_page *l2 = ensure_table(&mmu_root(virt)->mp_entries[L1_IDX(virt)], 1, virt);
    struct mmu_page *l3 = ensure_table(&l2->mp_entries[L2_IDX(virt)], 2, virt);
 
    DMAP(kprintf("[MMU] put_4k_page(%p, %p, %016x)\n", phys, virt, attrs));
 
    l3->mp_entries[L3_IDX(virt)] = (phys & DESC_OA_MASK) | attrs | DESC_PAGE;
}

/* Run the merge/drop pass over every 2MB and 1GB window touched by [virt, virt + length) */
static void tidy_range(uintptr_t virt, uintptr_t length)
{
    uintptr_t v = virt & ~(SZ_2M - 1);
    uintptr_t left = length + (virt - v);
 
    while (left)
    {
        uintptr_t step = (left > SZ_2M) ? SZ_2M : left;
        uint64_t d = mmu_root(v)->mp_entries[L1_IDX(v)];
 
        if ((d & DESC_TYPE_MASK) == DESC_TABLE) {
            tidy_l2_entry(&desc_to_table(d)->mp_entries[L2_IDX(v)]);
        }
 
        /* Last 2MB window of a 1GB region, or end of range: try the L1 entry too */
        if (step == left || ((v + SZ_2M) & (SZ_1G - 1)) == 0) {
            tidy_l1_entry(v);
        }
 
        v += SZ_2M;
        left -= step;
    }
}


uintptr_t mmu_virt2phys(uintptr_t addr)
{
    uintptr_t phys = 0;
    uint64_t *tbl = mmu_root(addr)->mp_entries;
    int idx_l1, idx_l2, idx_l3;
    uint64_t tmp;
 
    DV2P(kprintf("[MMU] virt2phys(%p)\n", addr));
    DV2P(kprintf("[MMU] L1 table: %p\n", tbl));
 
    idx_l1 = L1_IDX(addr);
    idx_l2 = L2_IDX(addr);
    idx_l3 = L3_IDX(addr);
 
    DV2P(kprintf("[MMU] idx_l1 = %d, idx_l2 = %d, idx_l3 = %d\n", idx_l1, idx_l2, idx_l3));
 
    tmp = tbl[idx_l1];
    DV2P(kprintf("[MMU] item in L1 table: %016x\n", tmp));
    if (tmp & 1)
    {
        DV2P(kprintf("[MMU] is valid\n"));
        if (tmp & 2) {
            tbl = desc_to_table(tmp)->mp_entries;
            DV2P(kprintf("[MMU] L2 table at %p\n", tbl));
 
            tmp = tbl[idx_l2];
            DV2P(kprintf("[MMU] item in L2 table: %016x\n", tmp));
            if (tmp & 1)
            {
                DV2P(kprintf("[MMU] is valid\n"));
                if (tmp & 2)
                {
                    tbl = desc_to_table(tmp)->mp_entries;
                    DV2P(kprintf("[MMU] L3 table at %p\n", tbl));
 
                    tmp = tbl[idx_l3];
                    DV2P(kprintf("[MMU] item in L3 table: %016x\n", tmp));
                    if ((tmp & 3) == 3)
                    {
                        DV2P(kprintf("[MMU] is valid 4K page\n"));
                        phys = (tmp & 0xfffffffff000) + (addr & 0xfff);
                    }
                    else {
                        DV2P(kprintf("[MMU] invalid!\n"));
                        return -1;
                    }
                }
                else {
                    DV2P(kprintf("[MMU] 2MB page!\n"));
                    phys = (tmp & 0xffffffe00000) + (addr & 0x1fffff);
                }
            }
            else
            {
                DV2P(kprintf("[MMU] invalid!\n"));
                return -1;
            }
        } else {
            DV2P(kprintf("[MMU] 1GB page!\n"));
            phys = (tmp & 0xffffc0000000) + (addr & 0x3fffffff);
        }
    }
    else {
        DV2P(kprintf("[MMU] invalid!\n"));
        return -1;
    }
 
    DV2P(kprintf("[MMU] returning %p\n", phys));
 
    return phys;
}

struct MemoryBlock *sys_memory;

void mmu_init()
{
    /*
        At this stage the user space memory is not set up yet, but the kernel runs in high address
        space, so it is safe to adjust the MMU tables for lower region. Here, only peripherals are
        mapped, but the rest will come very soon.
    */

    uintptr_t mmu_ploc = 0;

    of_node_t *e = dt_find_node("/memory");

    if (e)
    {
        of_property_t *p = dt_find_property(e, "reg");
        uint32_t *range = p->op_value;
        int size_cells = dt_get_property_value_u32(e, "#size-cells", 1, TRUE);
        int address_cells = dt_get_property_value_u32(e, "#address-cells", 1, TRUE);
        int block_size = 4 * (size_cells + address_cells);
        int block_count = p->op_length / block_size;
        bool unicam_set = false;
        bool vc4_set = false;
        bool cma_set = false;

        sys_memory = tlsf_malloc(tlsf, (1 + block_count) * sizeof(struct MemoryBlock));
        for (int block=0; block < block_count; block++)
        {
            of_property_t *p = NULL;
            int last_block = (block == block_count - 1);
            uintptr_t addr = 0;
            uintptr_t size = 0;
            int update_needed = 0;

            for (int i=0; i < address_cells; i++)
            {
                addr = (addr << 32) | BE32(range[i]);
            }
            for (int i=0; i < size_cells; i++)
            {
                size = (size << 32) | BE32(range[i + address_cells]);
            }

            /* 
                If this is the last block, borrow 2MB from there for initial MMU page pool, then take 
                memory from there for m68k and PPC jit, too.
            */
            if (last_block) {
                of_node_t *e = dt_find_node("/emu68");

                size -= MMU_POOL_SIZE * sizeof(struct mmu_page);
                update_needed = 1;
                mmu_ploc = addr + size;

                uint32_t m68k_jit_size = dt_get_property_value_u32(e, "m68k-jit-size", 0, FALSE) << 20;
                size -= m68k_jit_size;
                globals.m68k_jit_phys_base = (void *)(addr + size);

                if (dt_find_property(e, "enable-ppc"))
                {
                    uint32_t ppc_jit_size = dt_get_property_value_u32(e, "ppc-jit-size", 0, FALSE) << 20;
                    size -= ppc_jit_size;
                    globals.ppc_jit_phys_base = (void *)(addr + size);
                }
            }

            /* 
                If /emu68/vc4-mem property exists, check if its base address is 0. If so, 
                then we can assign it a base address here, provided the memory block ends below 1GB,
                and adjust the size of the memory block accordingly. 
            */
            if (!vc4_set && (p = dt_find_property(dt_find_node("/emu68"), "vc4-mem"))) {
                uint32_t *reg = p->op_value;
                if (reg[1] != 0 && reg[0] == 0) {
                    if (addr + size <= 0x40000000) {
                        size -= (reg[1] + 0x00200000);
                        reg[0] = addr + size + (2 << 20);
                        update_needed = 1;
                        vc4_set = true;
                    }
                }
            }

            /*
                If /emu68/cma node exists, check its reg property. If base address is set to 0,
                assign some memory to it now, provided the memory block ends below 1GB (some 
                Pi peripherals might need it)
            */

            if (!cma_set && (p = dt_find_property(dt_find_node("/emu68/cma"), "reg"))) {
                uint32_t *reg = p->op_value;
                if (reg[1] != 0 && reg[0] == 0) {
                    if (addr + size <= 0x40000000) {
                        size -= (reg[1] + 0x1fffff) & ~0x1fffff;
                        size -= 2 << 20;
                        reg[0] = addr + size + (2 << 20);
                        update_needed = 1;
                        cma_set = true;
                    }
                }
            }

            /* 
                If /emu68/unicam-mem property exists, check if its base address is 0. If so,
                then we can assign it a base address here, provided the memory block ends below 1GB,
                and adjust the size of the memory block accordingly. 
            */ 
            if (!unicam_set && (p = dt_find_property(dt_find_node("/emu68"), "unicam-mem"))) {
                uint32_t *reg = p->op_value;
                if (reg[1] != 0 && reg[0] == 0) {
                    if (addr + size <= 0x40000000) {
                        size -= (reg[1] + 0x1fffff) & ~0x1fffff;
                        size -= 2 << 20;
                        reg[0] = addr + size + (2 << 20);
                        update_needed = 1;
                        unicam_set = true;
                    }
                }
            }

#ifdef PISTORM_ANY_MODEL
            // Adjust base and size of the memory block
            if (addr < 0x01000000) {
                size -= 0x01000000 - addr;
                addr = 0x01000000;

                update_needed = 1;
            }
#endif
            if (update_needed)
            {
                // Put the block back into device tree, this time in updated form
                uint32_t *storage = &range[address_cells];
                uintptr_t tmp = addr;
                for (int i=0; i < address_cells; i++)
                {
                    *--storage = (uint32_t)tmp;
                    tmp >>= 32;
                }

                storage = &range[address_cells + size_cells];
                tmp = size;
                for (int i=0; i < size_cells; i++)
                {
                    *--storage = (uint32_t)tmp;
                    tmp >>= 32;
                }

            }

            sys_memory[block].mb_Base = addr;
            sys_memory[block].mb_Size = size;
            
            range += block_size / 4;
        }

        sys_memory[block_count].mb_Base = 0;
        sys_memory[block_count].mb_Size = 0;
    }

    spinlock_init(&mmu_lock);

    mmu_user_L1.mp_entries[0] = 0;
    mmu_user_L1.mp_entries[1] = 0;
    mmu_user_L1.mp_entries[2] = 0;
    mmu_user_L1.mp_entries[3] = 0;

    /* Initialize the MMU page pool */
    mmu_pool_init(mmu_ploc + PHYS_VIRT_OFFSET, MMU_POOL_SIZE * sizeof(struct mmu_page));

    arm_flush_cache((intptr_t)&mmu_user_L1, sizeof(mmu_user_L1));
    arm_flush_cache((intptr_t)&mmu_kernel_L1, sizeof(mmu_kernel_L1));
    arm_flush_cache((intptr_t)&mmu_kernel_L2, sizeof(mmu_kernel_L2));

    mmu_tlb_flush_all();
}

void mirror_page(uintptr_t virt)
{
    int idx_l1 = (virt >> 30) & 0x1ff;

    /* For 0..4GB create a shadow in the 4..8GB and -4..0GB areas */
    if (0 == (virt  & 0xffff000000000000))
    {
        if (idx_l1 < 4) {
            struct mmu_page *tbl_kernel;
            struct mmu_page *tbl;

            /* Update user space area */
            __asm__ volatile("mrs %0, TTBR0_EL1":"=r"(tbl));
            tbl = (struct mmu_page *)((uintptr_t)tbl + PHYS_VIRT_OFFSET);
            tbl->mp_entries[idx_l1 + 4] = tbl->mp_entries[idx_l1];
            
            /* Now fetch kernel table and update the topmost region, too */
            __asm__ volatile("mrs %0, TTBR1_EL1":"=r"(tbl_kernel));
            tbl_kernel = (struct mmu_page *)((uintptr_t)tbl_kernel + PHYS_VIRT_OFFSET);
            tbl_kernel->mp_entries[508 + idx_l1] = tbl->mp_entries[idx_l1];
        }
    }
}

void mmu_map(uintptr_t phys, uintptr_t virt, uintptr_t length, uint32_t attr_low, uint32_t attr_high)
{
    uint64_t attrs = make_attrs(attr_low, attr_high);
    uintptr_t virt_start = virt;
    uintptr_t mapped;
 
    DMAP(kprintf("[MMU] mmu_map(%p, %p, %x, %04x00000000%04x)\n", phys, virt, length, attr_high, attr_low));
 
    /* Partial pages are not mapped, same as before. Callers pass page aligned ranges */
    length &= ~(SZ_4K - 1);
    mapped = length;
 
    spinlock_acquire(&mmu_lock);
 
    /* Align virt up to 2M boundary with 4K pages */
    while ((virt & (SZ_2M - 1)) && length)
    {
        put_4k_page(phys, virt, attrs);
        phys += SZ_4K;
        virt += SZ_4K;
        length -= SZ_4K;
    }
 
    /* Now check if phys is sill aligned to 2M boundary. If not, continue using 4K pages */
    if ((phys & (SZ_2M - 1)) == 0)
    {
        while (length >= SZ_2M)
        {
            put_2m_page(phys, virt, attrs);
            phys += SZ_2M;
            virt += SZ_2M;
            length -= SZ_2M;
        }
    }
 
    /* Put the rest using 4K pages */
    while (length)
    {
        put_4k_page(phys, virt, attrs);
        phys += SZ_4K;
        virt += SZ_4K;
        length -= SZ_4K;
    }
 
    if (mapped)
        tidy_range(virt_start, mapped);
 
    mmu_commit();
 
    spinlock_release(&mmu_lock);
}
 
void mmu_unmap(uintptr_t virt, uintptr_t length)
{
    uintptr_t virt_start, left;
 
    DMAP(kprintf("[MMU] mmu_unmap(%p, %x)\n", virt, length));
 
    /* Unmap every page touched by the range */
    left = ((virt + length + SZ_4K - 1) & ~(SZ_4K - 1)) - (virt & ~(SZ_4K - 1));
    virt &= ~(SZ_4K - 1);
    virt_start = virt;
    length = left;
 
    if (!left)
        return;
 
    spinlock_acquire(&mmu_lock);
 
    while (left)
    {
        uint64_t *e1 = &mmu_root(virt)->mp_entries[L1_IDX(virt)];
        uintptr_t span1 = SZ_1G - (virt & (SZ_1G - 1));
 
        if (span1 > left)
            span1 = left;
 
        if ((*e1 & 1) == 0)
        {
            /* Nothing mapped here */
        }
        else if (span1 == SZ_1G && !is_static_slot(virt))
        {
            uint64_t old = *e1;
 
            set_l1_entry(virt, 0, 0);
            if ((old & DESC_TYPE_MASK) == DESC_TABLE)
                retire_l2_tree(desc_to_table(old));
        }
        else
        {
            struct mmu_page *l2 = ensure_table(e1, 1, virt);
            uintptr_t v = virt;
            uintptr_t l = span1;
 
            while (l)
            {
                uint64_t *e2 = &l2->mp_entries[L2_IDX(v)];
                uintptr_t span2 = SZ_2M - (v & (SZ_2M - 1));
 
                if (span2 > l)
                    span2 = l;
 
                if ((*e2 & 1) == 0)
                {
                    /* Nothing mapped here */
                }
                else if (span2 == SZ_2M)
                {
                    uint64_t old = *e2;
 
                    *e2 = 0;
                    if ((old & DESC_TYPE_MASK) == DESC_TABLE)
                        retire_table(desc_to_table(old));
                }
                else
                {
                    struct mmu_page *l3 = ensure_table(e2, 2, v);
 
                    for (uintptr_t x = v; x < v + span2; x += SZ_4K)
                        l3->mp_entries[L3_IDX(x)] = 0;
                }
 
                v += span2;
                l -= span2;
            }
        }
 
        virt += span1;
        left -= span1;
    }
 
    tidy_range(virt_start, length);
 
    mmu_commit();
 
    spinlock_release(&mmu_lock);
}
