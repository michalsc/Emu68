#include "config.h"
#include "tlsf.h"
#include "support.h"
#include "cache.h"
//#include "ps_protocol.h"
#include "M68k.h"
#include <stdint.h>

union CacheLine
{
    uint128_t cl_128;
    uint64_t cl_64[2];
    uint32_t cl_32[4];
    uint16_t cl_16[8];
    uint8_t  cl_8[16];
};

#define D(x) /* x */

#define F_VALID         0x80
#define F_DIRTY0        0x01
#define F_DIRTY1        0x02
#define F_DIRTY2        0x04
#define F_DIRTY3        0x08

#if CACHE_WAY_COUNT > 32
#error CACHE_WAY_COUNT shall be less or equal 32
#endif

#define GET_SET(x)  (((x) >> 4) & (CACHE_SET_COUNT - 1))
#define GET_TAG(x)  ((x) & ~(CACHE_SET_COUNT * 16 - 1))

struct Cache
{
    union CacheLine     c_Lines[CACHE_SET_COUNT][CACHE_WAY_COUNT];
    uint32_t            c_Tags[CACHE_SET_COUNT][CACHE_WAY_COUNT];
    uint8_t             c_Flags[CACHE_SET_COUNT][CACHE_WAY_COUNT];
    uint32_t            c_WaySelect[CACHE_SET_COUNT];
};

struct Cache *IC;
struct Cache *DC;

void cache_mark_hit(struct Cache *cache, int set, int way)
{
    /* Mark the way as accessed */
    cache->c_WaySelect[set] |= 1 << way;

    /* If all ways are marked as accessed, clear them all and set the current one again */
    if (cache->c_WaySelect[set] == (0xffffffff >> (32 - CACHE_WAY_COUNT)))
    {
        cache->c_WaySelect[set] = 1 << way;
    }
}

int cache_get_way(struct Cache *cache, int set)
{
    return __builtin_ffs(~cache->c_WaySelect[set]) - 1;
}

void cache_setup()
{
    if (IC != NULL && DC != NULL) {
        kprintf("[CACHE] Cache was alraedy set up, invalidating only\n");
        
        cache_invalidate_all(ICACHE);
        cache_invalidate_all(DCACHE);
        
        return;
    }
    
    (kprintf("[CACHE] Cache setup. Cache sizeof=%lu\n", sizeof(struct Cache)));
    (kprintf("[CACHE] Way count: %d, Set count: %d\n", CACHE_WAY_COUNT, CACHE_SET_COUNT));

    IC = (struct Cache *)tlsf_malloc(tlsf, sizeof(struct Cache));
    DC = (struct Cache *)tlsf_malloc(tlsf, sizeof(struct Cache));

    for(int i=0; i < CACHE_SET_COUNT; i++)
    {
        IC->c_WaySelect[i] = 0;
        DC->c_WaySelect[i] = 0;
        for (int j=0; j < CACHE_WAY_COUNT; j++)
        {
            IC->c_Flags[i][j] = 0;
            IC->c_Tags[i][j] = 0;
        }
    }

    (kprintf("[CACHE] ICache @ %p, DCache @ %p\n", IC, DC));
}

void cache_invalidate_all(enum CacheType type)
{
    struct Cache *cache = (type == ICACHE) ? IC : DC;

    D(kprintf("[CACHE] %cCache invalidate all\n", type == ICACHE ? 'I':'D'));

    for (int i=0; i < CACHE_SET_COUNT; i++)
    {
        cache->c_WaySelect[i] = 0;
        for (int j=0; j < CACHE_WAY_COUNT; j++)
        {
            cache->c_Flags[i][j] = 0;
        }        
    }
}

void cache_flush_all(enum CacheType type)
{
    if (type == ICACHE)
        cache_invalidate_all(type);

    struct Cache *cache = (type == ICACHE) ? IC : DC;

    D(kprintf("[CACHE] %cCache flush all\n", type == ICACHE ? 'I':'D'));

    for (int set=0; set < CACHE_SET_COUNT; set++)
    {
        cache->c_WaySelect[set] = 0;
        for (int way=0; way < CACHE_WAY_COUNT; way++)
        {
            if ((cache->c_Flags[set][way] & F_VALID) && (cache->c_Flags[set][way] & (F_DIRTY0 | F_DIRTY1 | F_DIRTY2 | F_DIRTY3)) != 0)
            {
                uint32_t line_address = cache->c_Tags[set][way] + (set << 4);
                D(kprintf("[CACHE]   cache line was previously used, tag=%08x, address=%08x, flushing\n",
                    cache->c_Tags[set][way], line_address));
                
                /* Write cache back if the lines are dirty */
                if (cache->c_Flags[set][way] & F_DIRTY0)
                    *(uint32_t *)(uintptr_t)(line_address) = cache->c_Lines[set][way].cl_32[0];
                if (cache->c_Flags[set][way] & F_DIRTY1)
                    *(uint32_t *)(uintptr_t)(line_address + 4) = cache->c_Lines[set][way].cl_32[1];
                if (cache->c_Flags[set][way] & F_DIRTY2)
                    *(uint32_t *)(uintptr_t)(line_address + 8) = cache->c_Lines[set][way].cl_32[2];
                if (cache->c_Flags[set][way] & F_DIRTY3)
                    *(uint32_t *)(uintptr_t)(line_address + 12) = cache->c_Lines[set][way].cl_32[3];
            }
            cache->c_Flags[set][way] = 0;
        }        
    }
}

void cache_invalidate_line(enum CacheType type, uint32_t address)
{
    struct Cache *cache = (type == ICACHE) ? IC : DC;

    D(kprintf("[CACHE] %cCache invalidate line (%08lx)\n", type == ICACHE ? 'I':'D', address));

    const uint32_t tag = GET_TAG(address);
    const uint32_t set = GET_SET(address);

    for (unsigned i=0; i < CACHE_WAY_COUNT; i++)
    {
        if ((cache->c_Tags[set][i] == tag) &&
            (cache->c_Flags[set][i] & F_VALID))
        {
            cache->c_Flags[set][i] = 0;
            cache->c_WaySelect[set] &= ~(1 << i);
        }
    }
}

void cache_invalidate_range(enum CacheType type, uint32_t address, uint32_t len)
{
    const uint64_t start = address & 0xfffffff0;
    const uint64_t end = (address + len - 1) & 0xfffffff0;

    for (uint64_t addr = start; addr <= end; addr += 16)
    {
        cache_invalidate_line(type, addr);
    }
}

void cache_flush_line(enum CacheType type, uint32_t address)
{
    if (type == ICACHE)
        cache_invalidate_line(type, address);

    struct Cache *cache = (type == ICACHE) ? IC : DC;

    D(kprintf("[CACHE] %cCache invalidate line (%08lx)\n", type == ICACHE ? 'I':'D', address));

    const uint32_t tag = GET_TAG(address);
    const uint32_t set = GET_SET(address);

    for (unsigned i=0; i < CACHE_WAY_COUNT; i++)
    {
        if ((cache->c_Tags[set][i] == tag) &&
            (cache->c_Flags[set][i] & F_VALID))
        {
            uint32_t line_address = cache->c_Tags[set][i] + (set << 4);

            /* Write cache back if the lines are dirty */
            if (cache->c_Flags[set][i] & F_DIRTY0)
                *(uint32_t *)(uintptr_t)(line_address) = cache->c_Lines[set][i].cl_32[0];
            if (cache->c_Flags[set][i] & F_DIRTY1)
                *(uint32_t *)(uintptr_t)(line_address + 4) = cache->c_Lines[set][i].cl_32[1];
            if (cache->c_Flags[set][i] & F_DIRTY2)
                *(uint32_t *)(uintptr_t)(line_address + 8) = cache->c_Lines[set][i].cl_32[2];
            if (cache->c_Flags[set][i] & F_DIRTY3)
                *(uint32_t *)(uintptr_t)(line_address + 12) = cache->c_Lines[set][i].cl_32[3];

            cache->c_Flags[set][i] = 0;
            cache->c_WaySelect[set] &= ~(1 << i);
        }
    }
}

uint128_t cache_read_128(enum CacheType type, uint32_t address)
{
    struct Cache *cache = (type == ICACHE) ? IC : DC;
    const uint32_t tag = GET_TAG(address);
    const uint32_t set = GET_SET(address);
    int way = -1;

    if (address >= 0x01000000)
        return *(uint128_t *)(uintptr_t)address;

#if 0

    if (type == DCACHE) return 0;
    
    /* Exit if given cache is disabled in CACR */
    uint32_t cacr;
    asm volatile("mov %w0, v31.s[0]":"=r"(cacr));
    if (type == ICACHE && (cacr & CACR_IE) == 0)
        return 0;
    else if (type == DCACHE && (cacr & CACR_DE) == 0)
        return 0;

    /* Only ROM is cacheable! */
    if (!((address >= 0x00e00000 && address < 0x00e80000) || (address >= 0x00f00000 && address < 0x01000000)))
        return 0;

#endif

    D(kprintf("[CACHE] %cCache read_128(%08lx)\n", type == ICACHE ? 'I':'D', address));

    D(kprintf("[CACHE]   set = %u, ", set));
    D(kprintf("tag = %08x, ", tag));

    if ((address & 15) != 0)
    {
        uint128_t data;
        data.hi = cache_read_64(type, address);
        data.lo = cache_read_64(type, address + 8);
        return data;
    }

        /* Check the given set and find the cache way with mathing tag and falid flag set */
    for (unsigned i=0; i < CACHE_WAY_COUNT; i++)
    {
        if ((cache->c_Tags[set][i] == tag) &&
            (cache->c_Flags[set][i] & F_VALID))
        {
            way = i;
            break;
        }
    }

    D(kprintf("way = %d\n", way));

    /* There was no valid cache line matching the tag, get one */
    if (way == -1)
    {
        way = cache_get_way(cache, set);
        D(kprintf("[CACHE]   allocated way = %d\n", way));
        if (cache->c_Flags[set][way] & F_VALID)
        {
            uint32_t line_address = cache->c_Tags[set][way] + (set << 4);
            D(kprintf("[CACHE]   cache line was previously used, tag=%08x, address=%08x, flushing\n",
        	    cache->c_Tags[set][way], line_address));
            
            /* Write cache back if the lines are dirty */
            if (cache->c_Flags[set][way] & F_DIRTY0)
                *(uint32_t*)(uintptr_t)line_address = cache->c_Lines[set][way].cl_32[0];
            if (cache->c_Flags[set][way] & F_DIRTY1)
                *(uint32_t*)(uintptr_t)(line_address + 4) = cache->c_Lines[set][way].cl_32[1];
            if (cache->c_Flags[set][way] & F_DIRTY2)
                *(uint32_t*)(uintptr_t)(line_address + 8) = cache->c_Lines[set][way].cl_32[2];
            if (cache->c_Flags[set][way] & F_DIRTY3)
                *(uint32_t*)(uintptr_t)(line_address + 12) = cache->c_Lines[set][way].cl_32[3];
        }

        /* Load the cache line */
        D(kprintf("[CACHE]   loading line from address %08x\n", address & 0xfffffff0));
        cache->c_Lines[set][way].cl_128 = *(uint128_t *)(uintptr_t)(address & 0xfffffff0);

        /* Update tag, mark page as valid */
        cache->c_Flags[set][way] = F_VALID;
        cache->c_Tags[set][way] = tag;
    }

    /* Mark LRUm */
    cache_mark_hit(cache, set, way);

    uint128_t data = cache->c_Lines[set][way].cl_128;

    D(kprintf("[CACHE]   => %016lx%016lx\n", data.hi, data.lo));

    return data;
}

uint64_t cache_read_64(enum CacheType type, uint32_t address)
{
    struct Cache *cache = (type == ICACHE) ? IC : DC;
    const uint32_t tag = GET_TAG(address);
    const uint32_t set = GET_SET(address);
    int way = -1;

    if (address >= 0x01000000)
        return *(uint64_t *)(uintptr_t)address;

#if 0
    if (type == DCACHE) return 0;

    /* Exit if given cache is disabled in CACR */
    uint32_t cacr;
    asm volatile("mov %w0, v31.s[0]":"=r"(cacr));
    if (type == ICACHE && (cacr & CACR_IE) == 0)
        return 0;
    else if (type == DCACHE && (cacr & CACR_DE) == 0)
        return 0;

    /* Only ROM is cacheable! */
    if (!((address >= 0x00e00000 && address < 0x00e80000) || (address >= 0x00f00000 && address < 0x01000000)))
        return 0;
#endif

    D(kprintf("[CACHE] %cCache read_64(%08lx)\n", type == ICACHE ? 'I':'D', address));

    D(kprintf("[CACHE]   set = %u, ", set));
    D(kprintf("tag = %08x, ", tag));

    if ((address & 15) > 8)
    {
        uint64_t data = 0;
        switch (address & 15)
        {
            case 9:
                data = cache_read_64(type, address - 1) << 8;
                data |= cache_read_8(type, address + 7);
                break;
            
            case 10:
                data = cache_read_64(type, address - 2) << 16;
                data |= cache_read_16(type, address + 6);
                break;

            case 11:
                data = cache_read_64(type, address - 3) << 24;
                data |= cache_read_32(type, address + 5) >> 8;
                break;

            case 12:
                data = (uint64_t)cache_read_32(type, address) << 32;
                data |= cache_read_32(type, address + 4);
                break;
            
            case 13:
                data = (uint64_t)cache_read_16(type, address) << 40;
                data |= cache_read_64(type, address + 3) >> 24;
                break;

            case 14:
                data = (uint64_t)cache_read_16(type, address) << 48;
                data |= cache_read_64(type, address + 2) >> 16;
                break;

            case 15:
                data = (uint64_t)cache_read_8(type, address) << 56;
                data |= cache_read_64(type, address + 1) >> 8;
                break;

            default:
                break;
        }
        return data;
    }

    /* Check the given set and find the cache way with mathing tag and falid flag set */
    for (unsigned i=0; i < CACHE_WAY_COUNT; i++)
    {
        if ((cache->c_Tags[set][i] == tag) &&
            (cache->c_Flags[set][i] & F_VALID))
        {
            way = i;
            break;
        }
    }

    D(kprintf("way = %d\n", way));

    /* There was no valid cache line matching the tag, get one */
    if (way == -1)
    {
        way = cache_get_way(cache, set);
        D(kprintf("[CACHE]   allocated way = %d\n", way));
        if (cache->c_Flags[set][way] & F_VALID)
        {
            uint32_t line_address = cache->c_Tags[set][way] + (set << 4);
            D(kprintf("[CACHE]   cache line was previously used, tag=%08x, address=%08x, flushing\n",
        	    cache->c_Tags[set * CACHE_WAY_COUNT + way], line_address));
            
            /* Write cache back if the lines are dirty */
            if (cache->c_Flags[set][way] & F_DIRTY0)
                *(uint32_t*)(uintptr_t)line_address = cache->c_Lines[set][way].cl_32[0];
            if (cache->c_Flags[set][way] & F_DIRTY1)
                *(uint32_t*)(uintptr_t)(line_address + 4) = cache->c_Lines[set][way].cl_32[1];
            if (cache->c_Flags[set][way] & F_DIRTY2)
                *(uint32_t*)(uintptr_t)(line_address + 8) = cache->c_Lines[set][way].cl_32[2];
            if (cache->c_Flags[set][way] & F_DIRTY3)
                *(uint32_t*)(uintptr_t)(line_address + 12) = cache->c_Lines[set][way].cl_32[3];
        }

        /* Load the cache line */
        D(kprintf("[CACHE]   loading line from address %08x\n", address & 0xfffffff0));
        cache->c_Lines[set][way].cl_128 = *(uint128_t *)(uintptr_t)(address & 0xfffffff0);

        /* Update tag, mark page as valid */
        cache->c_Flags[set][way] = F_VALID;
        cache->c_Tags[set][way] = tag;
    }

    /* Mark LRUm */
    cache_mark_hit(cache, set, way);

    uint64_t data = *(uint64_t *)&cache->c_Lines[set][way].cl_8[address & 15];

    D(kprintf("[CACHE]   => %016lx\n", data));

    return data;
}

uint32_t cache_read_32(enum CacheType type, uint32_t address)
{
    struct Cache *cache = (type == ICACHE) ? IC : DC;
    const uint32_t tag = GET_TAG(address);
    const uint32_t set = GET_SET(address);
    int way = -1;

    if (address >= 0x01000000)
        return *(uint32_t *)(uintptr_t)address;

#if 0
    if (type == DCACHE) return 0;

    /* Exit if given cache is disabled in CACR */
    uint32_t cacr;
    asm volatile("mov %w0, v31.s[0]":"=r"(cacr));
    if (type == ICACHE && (cacr & CACR_IE) == 0)
        return 0;
    else if (type == DCACHE && (cacr & CACR_DE) == 0)
        return 0;

    /* Only ROM is cacheable! */
    if (!((address >= 0x00e00000 && address < 0x00e80000) || (address >= 0x00f00000 && address < 0x01000000)))
        return 0;
#endif

    D(kprintf("[CACHE] %cCache read_32(%08lx)\n", type == ICACHE ? 'I':'D', address));

    D(kprintf("[CACHE]   set = %u, ", set));
    D(kprintf("tag = %08x, ", tag));

    if ((address & 15) > 12)
    {
        uint32_t data;
        switch (address & 15)
        {
            case 13:
                data = cache_read_32(type, address - 1) << 8;
                data |= cache_read_8(type, address + 3);
                break;
            case 14:
                data = cache_read_16(type, address) << 16;
                data |= cache_read_16(type, address + 2);
                break;
            case 15:
                data = cache_read_8(type, address) << 24;
                data |= cache_read_32(type, address + 1) >> 8;
                break;
            default:
                break;
        }
        
        return data;
    }

    /* Check the given set and find the cache way with mathing tag and falid flag set */
    for (unsigned i=0; i < CACHE_WAY_COUNT; i++)
    {
        if ((cache->c_Tags[set][i] == tag) &&
            (cache->c_Flags[set][i] & F_VALID))
        {
            way = i;
            break;
        }
    }

    D(kprintf("way = %d\n", way));

    /* There was no valid cache line matching the tag, get one */
    if (way == -1)
    {
        way = cache_get_way(cache, set);
        D(kprintf("[CACHE]   allocated way = %d\n", way));
        if (cache->c_Flags[set][way] & F_VALID)
        {
            uint32_t line_address = cache->c_Tags[set][way] + (set << 4);
            D(kprintf("[CACHE]   cache line was previously used, tag=%08x, address=%08x, flushing\n",
        	    cache->c_Tags[set][way], line_address));
            
            /* Write cache back if the lines are dirty */
            if (cache->c_Flags[set][way] & F_DIRTY0)
                *(uint32_t*)(uintptr_t)line_address = cache->c_Lines[set][way].cl_32[0];
            if (cache->c_Flags[set][way] & F_DIRTY1)
                *(uint32_t*)(uintptr_t)(line_address + 4) = cache->c_Lines[set][way].cl_32[1];
            if (cache->c_Flags[set][way] & F_DIRTY2)
                *(uint32_t*)(uintptr_t)(line_address + 8) = cache->c_Lines[set][way].cl_32[2];
            if (cache->c_Flags[set][way] & F_DIRTY3)
                *(uint32_t*)(uintptr_t)(line_address + 12) = cache->c_Lines[set][way].cl_32[3];
        }

        /* Load the cache line */
        D(kprintf("[CACHE]   loading line from address %08x\n", address & 0xfffffff0));
        cache->c_Lines[set][way].cl_128 = *(uint128_t *)(uintptr_t)(address & 0xfffffff0);

        /* Update tag, mark page as valid */
        cache->c_Flags[set][way] = F_VALID;
        cache->c_Tags[set][way] = tag;
    }

    /* Mark LRUm */
    cache_mark_hit(cache, set, way);

    uint32_t data = *(uint32_t *)&cache->c_Lines[set][way].cl_8[address & 15];
    D(kprintf("[CACHE]   => %08x\n", data));

    return data;
}

uint16_t cache_read_16(enum CacheType type, uint32_t address)
{
    struct Cache *cache = (type == ICACHE) ? IC : DC;
    const uint32_t tag = GET_TAG(address);
    const uint32_t set = GET_SET(address);
    int way = -1;

    if (address >= 0x01000000)
        return *(uint16_t *)(uintptr_t)address;

#if 0

    if (type == DCACHE) return 0;

    /* Exit if given cache is disabled in CACR */
    uint32_t cacr;
    asm volatile("mov %w0, v31.s[0]":"=r"(cacr));
    if (type == ICACHE && (cacr & CACR_IE) == 0)
        return 0;
    else if (type == DCACHE && (cacr & CACR_DE) == 0)
        return 0;

    /* Only ROM is cacheable! */
    if (!((address >= 0x00e00000 && address < 0x00e80000) || (address >= 0x00f00000 && address < 0x01000000)))
        return 0;
#endif

    D(kprintf("[CACHE] %cCache read_16(%08lx)\n", type == ICACHE ? 'I':'D', address));

    D(kprintf("[CACHE]   set = %u, ", set));
    D(kprintf("tag = %08x, ", tag));

    if ((address & 15) > 14)
    {
        uint16_t data = cache_read_8(type, address) << 8;
        data |= cache_read_8(type, address + 1);
        
        return data;
    }

    /* Check the given set and find the cache way with mathing tag and falid flag set */
    for (unsigned i=0; i < CACHE_WAY_COUNT; i++)
    {
        if ((cache->c_Tags[set][i] == tag) &&
            (cache->c_Flags[set][i] & F_VALID))
        {
            way = i;
            break;
        }
    }

    D(kprintf("way = %d\n", way));

    /* There was no valid cache line matching the tag, get one */
    if (way == -1)
    {
        way = cache_get_way(cache, set);
        D(kprintf("[CACHE]   allocated way = %d\n", way));
        if (cache->c_Flags[set][way] & F_VALID)
        {
            uint32_t line_address = cache->c_Tags[set][way] + (set << 4);
            D(kprintf("[CACHE]   cache line was previously used, tag=%08x, address=%08x, flushing\n",
        	    cache->c_Tags[set][way], line_address));
            
            /* Write cache back if the lines are dirty */
            if (cache->c_Flags[set][way] & F_DIRTY0)
                *(uint32_t*)(uintptr_t)line_address = cache->c_Lines[set][way].cl_32[0];
            if (cache->c_Flags[set][way] & F_DIRTY1)
                *(uint32_t*)(uintptr_t)(line_address + 4) = cache->c_Lines[set][way].cl_32[1];
            if (cache->c_Flags[set][way] & F_DIRTY2)
                *(uint32_t*)(uintptr_t)(line_address + 8) = cache->c_Lines[set][way].cl_32[2];
            if (cache->c_Flags[set][way] & F_DIRTY3)
                *(uint32_t*)(uintptr_t)(line_address + 12) = cache->c_Lines[set][way].cl_32[3];
        }

        /* Load the cache line */
        //kprintf("[CACHE]   loading line from address %08x\n", address & 0xfffffff0);
        cache->c_Lines[set][way].cl_128 = *(uint128_t *)(uintptr_t)(address & 0xfffffff0);

        /* Update tag, mark page as valid */
        cache->c_Flags[set][way] = F_VALID;
        cache->c_Tags[set][way] = tag;
    }

    /* Mark LRUm */
    cache_mark_hit(cache, set, way);

    uint16_t data = *(uint16_t *)(void *)&cache->c_Lines[set][way].cl_8[address & 15];
    D(kprintf("[CACHE]   => %04x\n", data));

    return data;
}

uint8_t cache_read_8(enum CacheType type, uint32_t address)
{
    struct Cache *cache = (type == ICACHE) ? IC : DC;
    const uint32_t tag = GET_TAG(address);
    const uint32_t set = GET_SET(address);
    int way = -1;

    if (address >= 0x01000000)
        return *(uint8_t *)(uintptr_t)address;

#if 0

    if (type == DCACHE) return 0;

    /* Exit if given cache is disabled in CACR */
    uint32_t cacr;
    asm volatile("mov %w0, v31.s[0]":"=r"(cacr));
    if (type == ICACHE && (cacr & CACR_IE) == 0)
        return 0;
    else if (type == DCACHE && (cacr & CACR_DE) == 0)
        return 0;

    /* Only ROM is cacheable! */
    if (!((address >= 0x00e00000 && address < 0x00e80000) || (address >= 0x00f00000 && address < 0x01000000)))
        return 0;
#endif

    D(kprintf("[CACHE] %cCache read_8(%08lx)\n", type == ICACHE ? 'I':'D', address));

    D(kprintf("[CACHE]   set = %u, ", set));
    D(kprintf("tag = %08x, ", tag));

    /* Check the given set and find the cache way with mathing tag and falid flag set */
    for (unsigned i=0; i < CACHE_WAY_COUNT; i++)
    {
        if ((cache->c_Tags[set][i] == tag) &&
            (cache->c_Flags[set][i] & F_VALID))
        {
            way = i;
            break;
        }
    }

    D(kprintf("way = %d\n", way));

    /* There was no valid cache line matching the tag, get one */
    if (way == -1)
    {
        way = cache_get_way(cache, set);
        D(kprintf("[CACHE]   allocated way = %d\n", way));
        if (cache->c_Flags[set][way] & F_VALID)
        {
            uint32_t line_address = cache->c_Tags[set][way] + (set << 4);
            D(kprintf("[CACHE]   cache line was previously used, tag=%08x, address=%08x, flushing\n",
        	    cache->c_Tags[set][way], line_address));
            
            /* Write cache back if the lines are dirty */
            if (cache->c_Flags[set][way] & F_DIRTY0)
                *(uint32_t*)(uintptr_t)line_address = cache->c_Lines[set][way].cl_32[0];
            if (cache->c_Flags[set][way] & F_DIRTY1)
                *(uint32_t*)(uintptr_t)(line_address + 4) = cache->c_Lines[set][way].cl_32[1];
            if (cache->c_Flags[set][way] & F_DIRTY2)
                *(uint32_t*)(uintptr_t)(line_address + 8) = cache->c_Lines[set][way].cl_32[2];
            if (cache->c_Flags[set][way] & F_DIRTY3)
                *(uint32_t*)(uintptr_t)(line_address + 12) = cache->c_Lines[set][way].cl_32[3];
        }

        /* Load the cache line */
        D(kprintf("[CACHE]   loading line from address %08x\n", address & 0xfffffff0));
        cache->c_Lines[set][way].cl_128 = *(uint128_t *)(uintptr_t)(address & 0xfffffff0);

        /* Update tag, mark page as valid */
        cache->c_Flags[set][way] = F_VALID;
        cache->c_Tags[set][way] = tag;
    }

    /* Mark LRUm */
    cache_mark_hit(cache, set, way);

    uint8_t data = cache->c_Lines[set][way].cl_8[address & 15];

    D(kprintf("[CACHE]   => %02x\n", data));

    return data;
}

int cache_write_128(enum CacheType type, uint32_t address, uint128_t data, uint8_t write_back)
{
    struct Cache *cache = (type == ICACHE) ? IC : DC;
    const uint32_t tag = GET_TAG(address);
    const uint32_t set = GET_SET(address);
    int way = -1;

#if 0
    return 0;

    /* Exit if given cache is disabled in CACR */
    uint32_t cacr;
    asm volatile("mov %w0, v31.s[0]":"=r"(cacr));
    if (type == ICACHE)
        return 0;
    else if (type == DCACHE && (cacr & CACR_DE) == 0)
        return 0;

    /* Only ROM is cacheable! */
    if (!((address >= 0x00e00000 && address < 0x00e80000) || (address >= 0x00f00000 && address < 0x01000000)))
        return 0;
#endif

    D(kprintf("[CACHE] %cCache write_128(%08lx, %016lx%016lx, %x)\n", type == ICACHE ? 'I':'D', address, data.hi, data.lo, write_back));

    D(kprintf("[CACHE]   set = %u, ", set));
    D(kprintf("tag = %08x, ", tag));

    if ((address & 15) != 0)
    {
        D(kprintf("[CACHE] Accessed data spans over two cache lines, aborting\n"));
        return 0;
    }

    /* Check the given set and find the cache way with mathing tag and falid flag set */
    for (unsigned i=0; i < CACHE_WAY_COUNT; i++)
    {
        if ((cache->c_Tags[set][i] == tag) &&
            (cache->c_Flags[set][i] & F_VALID))
        {
            way = i;
            break;
        }
    }

    D(kprintf("way = %d\n", way));

    /* There was no valid cache line matching the tag */
    if (way == -1)
    {
        /* Write-through cache does not load cache line, performs direct write instead */
        if (write_back == 0)
            return 0;

        /* Write-back cache loads whole cache line and updates the value */
        way = cache_get_way(cache, set);
        D(kprintf("[CACHE]   allocated way = %d\n", way));
        if (cache->c_Flags[set][way] & F_VALID)
        {
            uint32_t line_address = cache->c_Tags[set][way] + (set << 4);
            D(kprintf("[CACHE]   cache line was previously used, tag=%08x, address=%08x, flushing\n",
        	    cache->c_Tags[set][way], line_address));
            
            /* Write cache back if the lines are dirty */
            if (cache->c_Flags[set][way] & F_DIRTY0)
                *(uint32_t*)(uintptr_t)line_address = cache->c_Lines[set][way].cl_32[0];
            if (cache->c_Flags[set][way] & F_DIRTY1)
                *(uint32_t*)(uintptr_t)(line_address + 4) = cache->c_Lines[set][way].cl_32[1];
            if (cache->c_Flags[set][way] & F_DIRTY2)
                *(uint32_t*)(uintptr_t)(line_address + 8) = cache->c_Lines[set][way].cl_32[2];
            if (cache->c_Flags[set][way] & F_DIRTY3)
                *(uint32_t*)(uintptr_t)(line_address + 12) = cache->c_Lines[set][way].cl_32[3];
        }

        /* No need to load cache line here, it gets overwritten and marked dirty immediately */

        /* Update tag, mark page as valid */
        cache->c_Flags[set][way] = F_VALID;
        cache->c_Tags[set][way] = tag;
    }

    /* Mark LRUm */
    cache_mark_hit(cache, set, way);

    cache->c_Lines[set][way].cl_128 = data;

    if (write_back)
    {
        /* Write-back cache marks the portion of cache line as dirty */
        cache->c_Flags[set][way] |= F_DIRTY0 | F_DIRTY1 | F_DIRTY2 | F_DIRTY3;
    }
    else
    {
        /* Write-through cache performs direct write to memory, cache line remains not-dirty */
        *(uint128_t *)(uintptr_t)address = data;
    }

    return 1;
}

int cache_write_64(enum CacheType type, uint32_t address, uint64_t data, uint8_t write_back)
{
    struct Cache *cache = (type == ICACHE) ? IC : DC;
    const uint32_t tag = GET_TAG(address);
    const uint32_t set = GET_SET(address);
    int way = -1;

#if 0
    return 0;

    /* Exit if given cache is disabled in CACR */
    uint32_t cacr;
    asm volatile("mov %w0, v31.s[0]":"=r"(cacr));
    if (type == ICACHE)
        return 0;
    else if (type == DCACHE && (cacr & CACR_DE) == 0)
        return 0;

    /* Only ROM is cacheable! */
    if (!((address >= 0x00e00000 && address < 0x00e80000) || (address >= 0x00f00000 && address < 0x01000000)))
        return 0;
#endif

    D(kprintf("[CACHE] %cCache write_64(%08lx, %016lx, %x)\n", type == ICACHE ? 'I':'D', address, data, write_back));

    D(kprintf("[CACHE]   set = %u, ", set));
    D(kprintf("tag = %08x, ", tag));

    if ((address & 15) > 8)
    {
        D(kprintf("[CACHE] Accessed data spans over two cache lines, aborting\n"));
        return 0;
    }

    /* Check the given set and find the cache way with mathing tag and falid flag set */
    for (unsigned i=0; i < CACHE_WAY_COUNT; i++)
    {
        if ((cache->c_Tags[set][i] == tag) &&
            (cache->c_Flags[set][i] & F_VALID))
        {
            way = i;
            break;
        }
    }

    D(kprintf("way = %d\n", way));

    /* There was no valid cache line matching the tag */
    if (way == -1)
    {
        /* Write-through cache does not load cache line, performs direct write instead */
        if (write_back == 0)
            return 0;

        /* Write-back cache loads whole cache line and updates the value */
        way = cache_get_way(cache, set);
        D(kprintf("[CACHE]   allocated way = %d\n", way));
        if (cache->c_Flags[set][way] & F_VALID)
        {
            uint32_t line_address = cache->c_Tags[set][way] + (set << 4);
            D(kprintf("[CACHE]   cache line was previously used, tag=%08x, address=%08x, flushing\n",
        	    cache->c_Tags[set][way], line_address));
            
            /* Write cache back if the lines are dirty */
            if (cache->c_Flags[set][way] & F_DIRTY0)
                *(uint32_t*)(uintptr_t)line_address = cache->c_Lines[set][way].cl_32[0];
            if (cache->c_Flags[set][way] & F_DIRTY1)
                *(uint32_t*)(uintptr_t)(line_address + 4) = cache->c_Lines[set][way].cl_32[1];
            if (cache->c_Flags[set][way] & F_DIRTY2)
                *(uint32_t*)(uintptr_t)(line_address + 8) = cache->c_Lines[set][way].cl_32[2];
            if (cache->c_Flags[set][way] & F_DIRTY3)
                *(uint32_t*)(uintptr_t)(line_address + 12) = cache->c_Lines[set][way].cl_32[3];
        }

        /* Load the cache line */
        D(kprintf("[CACHE]   loading line from address %08x\n", address & 0xfffffff0));
        cache->c_Lines[set][way].cl_128 = *(uint128_t *)(uintptr_t)(address & 0xfffffff0);

        /* Update tag, mark page as valid */
        cache->c_Flags[set][way] = F_VALID;
        cache->c_Tags[set][way] = tag;
    }

    /* Mark LRUm */
    cache_mark_hit(cache, set, way);

    (*(uint64_t *)(void*)&cache->c_Lines[set][way].cl_8[address & 15]) = data;

    if (write_back)
    {
        /* Write-back cache marks the portion of cache line as dirty */
        switch((address >> 2) & 3)
        {
            case 0:
                cache->c_Flags[set][way] |= F_DIRTY0 | F_DIRTY1;
                if ((((address + 7) >> 2) & 3) == 2)
                    cache->c_Flags[set][way] |= F_DIRTY2;
                break;
            case 1:
                cache->c_Flags[set][way] |= F_DIRTY1 | F_DIRTY2;
                if ((((address + 7) >> 2) & 3) == 3)
                    cache->c_Flags[set][way] |= F_DIRTY3;
                break;
            case 2:
                cache->c_Flags[set][way] |= F_DIRTY2 | F_DIRTY3;
                break;
        }
    }
    else
    {
        /* Write-through cache performs direct write to memory, cache line remains not-dirty */
        *(uint64_t *)(uintptr_t)address = data;
    }

    return 1;
}

int cache_write_32(enum CacheType type, uint32_t address, uint32_t data, uint8_t write_back)
{
    struct Cache *cache = (type == ICACHE) ? IC : DC;
    const uint32_t tag = GET_TAG(address);
    const uint32_t set = GET_SET(address);
    int way = -1;

#if 0
    return 0;

    /* Exit if given cache is disabled in CACR */
    uint32_t cacr;
    asm volatile("mov %w0, v31.s[0]":"=r"(cacr));
    if (type == ICACHE)
        return 0;
    else if (type == DCACHE && (cacr & CACR_DE) == 0)
        return 0;

    /* Only ROM is cacheable! */
    if (!((address >= 0x00e00000 && address < 0x00e80000) || (address >= 0x00f00000 && address < 0x01000000)))
        return 0;
#endif

    D(kprintf("[CACHE] %cCache write_32(%08lx, %08x, %x)\n", type == ICACHE ? 'I':'D', address, data, write_back));

    D(kprintf("[CACHE]   set = %u, ", set));
    D(kprintf("tag = %08x, ", tag));

    if ((address & 15) > 12)
    {
        D(kprintf("[CACHE] Accessed data spans over two cache lines, aborting\n"));
        return 0;
    }

    /* Check the given set and find the cache way with mathing tag and falid flag set */
    for (unsigned i=0; i < CACHE_WAY_COUNT; i++)
    {
        if ((cache->c_Tags[set][i] == tag) &&
            (cache->c_Flags[set][i] & F_VALID))
        {
            way = i;
            break;
        }
    }

    D(kprintf("way = %d\n", way));

    /* There was no valid cache line matching the tag */
    if (way == -1)
    {
        /* Write-through cache does not load cache line, performs direct write instead */
        if (write_back == 0)
            return 0;

        /* Write-back cache loads whole cache line and updates the value */
        way = cache_get_way(cache, set);
        D(kprintf("[CACHE]   allocated way = %d\n", way));
        if (cache->c_Flags[set][way] & F_VALID)
        {
            uint32_t line_address = cache->c_Tags[set][way] + (set << 4);
            D(kprintf("[CACHE]   cache line was previously used, tag=%08x, address=%08x, flushing\n",
        	    cache->c_Tags[set][way], line_address));
            
            /* Write cache back if the lines are dirty */
            if (cache->c_Flags[set][way] & F_DIRTY0)
                *(uint32_t*)(uintptr_t)line_address = cache->c_Lines[set][way].cl_32[0];
            if (cache->c_Flags[set][way] & F_DIRTY1)
                *(uint32_t*)(uintptr_t)(line_address + 4) = cache->c_Lines[set][way].cl_32[1];
            if (cache->c_Flags[set][way] & F_DIRTY2)
                *(uint32_t*)(uintptr_t)(line_address + 8) = cache->c_Lines[set][way].cl_32[2];
            if (cache->c_Flags[set][way] & F_DIRTY3)
                *(uint32_t*)(uintptr_t)(line_address + 12) = cache->c_Lines[set][way].cl_32[3];
        }

        /* Load the cache line */
        D(kprintf("[CACHE]   loading line from address %08x\n", address & 0xfffffff0));
        cache->c_Lines[set][way].cl_128 = *(uint128_t *)(uintptr_t)(address & 0xfffffff0);

        /* Update tag, mark page as valid */
        cache->c_Flags[set][way] = F_VALID;
        cache->c_Tags[set][way] = tag;
    }

    /* Mark LRUm */
    cache_mark_hit(cache, set, way);

    (*(uint32_t *)(void*)&cache->c_Lines[set][way].cl_8[address & 15]) = data;

    if (write_back)
    {
        /* Write-back cache marks the portion of cache line as dirty */
        switch((address >> 2) & 3)
        {
            case 0:
                cache->c_Flags[set][way] |= F_DIRTY0;
                if ((((address + 3) >> 2) & 3) == 1)
                    cache->c_Flags[set][way] |= F_DIRTY1;
                break;
            case 1:
                cache->c_Flags[set][way] |= F_DIRTY1;
                if ((((address + 3) >> 2) & 3) == 2)
                    cache->c_Flags[set][way] |= F_DIRTY2;
                break;
            case 2:
                cache->c_Flags[set][way] |= F_DIRTY2;
                if ((((address + 3) >> 2) & 3) == 3)
                    cache->c_Flags[set][way] |= F_DIRTY3;
                break;
            case 3:
                cache->c_Flags[set][way] |= F_DIRTY3;
                break;
        }
    }
    else
    {
        /* Write-through cache performs direct write to memory, cache line remains not-dirty */
        *(uint32_t *)(uintptr_t)address = data;
    }

    return 1;
}


int cache_write_16(enum CacheType type, uint32_t address, uint16_t data, uint8_t write_back)
{
    struct Cache *cache = (type == ICACHE) ? IC : DC;
    const uint32_t tag = GET_TAG(address);
    const uint32_t set = GET_SET(address);
    int way = -1;

#if 0
    return 0;

    /* Exit if given cache is disabled in CACR */
    uint32_t cacr;
    asm volatile("mov %w0, v31.s[0]":"=r"(cacr));
    if (type == ICACHE)
        return 0;
    else if (type == DCACHE && (cacr & CACR_DE) == 0)
        return 0;

    /* Only ROM is cacheable! */
    if (!((address >= 0x00e00000 && address < 0x00e80000) || (address >= 0x00f00000 && address < 0x01000000)))
        return 0;
#endif

    D(kprintf("[CACHE] %cCache write_16(%08lx, %04x, %x)\n", type == ICACHE ? 'I':'D', address, data, write_back));

    D(kprintf("[CACHE]   set = %u, ", set));
    D(kprintf("tag = %08x, ", tag));

    if ((address & 15) > 14)
    {
        D(kprintf("[CACHE] Accessed data spans over two cache lines, aborting\n"));
        return 0;
    }

    /* Check the given set and find the cache way with mathing tag and falid flag set */
    for (unsigned i=0; i < CACHE_WAY_COUNT; i++)
    {
        if ((cache->c_Tags[set][i] == tag) &&
            (cache->c_Flags[set][i] & F_VALID))
        {
            way = i;
            break;
        }
    }

    D(kprintf("way = %d\n", way));

    /* There was no valid cache line matching the tag */
    if (way == -1)
    {
        /* Write-through cache does not load cache line, performs direct write instead */
        if (write_back == 0)
            return 0;

        /* Write-back cache loads whole cache line and updates the value */
        way = cache_get_way(cache, set);
        D(kprintf("[CACHE]   allocated way = %d\n", way));
        if (cache->c_Flags[set][way] & F_VALID)
        {
            uint32_t line_address = cache->c_Tags[set][way] + (set << 4);
            D(kprintf("[CACHE]   cache line was previously used, tag=%08x, address=%08x, flushing\n",
        	    cache->c_Tags[set][way], line_address));
            
            /* Write cache back if the lines are dirty */
            if (cache->c_Flags[set][way] & F_DIRTY0)
                *(uint32_t*)(uintptr_t)line_address = cache->c_Lines[set][way].cl_32[0];
            if (cache->c_Flags[set][way] & F_DIRTY1)
                *(uint32_t*)(uintptr_t)(line_address + 4) = cache->c_Lines[set][way].cl_32[1];
            if (cache->c_Flags[set][way] & F_DIRTY2)
                *(uint32_t*)(uintptr_t)(line_address + 8) = cache->c_Lines[set][way].cl_32[2];
            if (cache->c_Flags[set][way] & F_DIRTY3)
                *(uint32_t*)(uintptr_t)(line_address + 12) = cache->c_Lines[set][way].cl_32[3];
        }

        /* Load the cache line */
        D(kprintf("[CACHE]   loading line from address %08x\n", address & 0xfffffff0));
        cache->c_Lines[set][way].cl_128 = *(uint128_t *)(uintptr_t)(address & 0xfffffff0);

        /* Update tag, mark page as valid */
        cache->c_Flags[set][way] = F_VALID;
        cache->c_Tags[set][way] = tag;
    }

    /* Mark LRUm */
    cache_mark_hit(cache, set, way);

    (*(uint16_t *)(void*)&cache->c_Lines[set][way].cl_8[address & 15]) = data;

    if (write_back)
    {
        /* Write-back cache marks the portion of cache line as dirty */
        switch((address >> 2) & 3)
        {
            case 0:
                cache->c_Flags[set][way] |= F_DIRTY0;
                if ((((address + 1) >> 2) & 3) == 1)
                    cache->c_Flags[set][way] |= F_DIRTY1;
                break;
            case 1:
                cache->c_Flags[set][way] |= F_DIRTY1;
                if ((((address + 1) >> 2) & 3) == 2)
                    cache->c_Flags[set][way] |= F_DIRTY2;
                break;
            case 2:
                cache->c_Flags[set][way] |= F_DIRTY2;
                if ((((address + 1) >> 2) & 3) == 3)
                    cache->c_Flags[set][way] |= F_DIRTY3;
                break;
            case 3:
                cache->c_Flags[set][way] |= F_DIRTY3;
                break;
        }
    }
    else
    {
        /* Write-through cache performs direct write to memory, cache line remains not-dirty */
        *(uint16_t*)(uintptr_t)address = data;
    }

    return 1;
}

int cache_write_8(enum CacheType type, uint32_t address, uint8_t data, uint8_t write_back)
{
    struct Cache *cache = (type == ICACHE) ? IC : DC;
    const uint32_t tag = GET_TAG(address);
    const uint32_t set = GET_SET(address);
    int way = -1;

#if 0
    return 0;

    /* Exit if given cache is disabled in CACR */
    uint32_t cacr;
    asm volatile("mov %w0, v31.s[0]":"=r"(cacr));
    if (type == ICACHE)
        return 0;
    else if (type == DCACHE && (cacr & CACR_DE) == 0)
        return 0;

    /* Only ROM is cacheable! */
    if (!((address >= 0x00e00000 && address < 0x00e80000) || (address >= 0x00f00000 && address < 0x01000000)))
        return 0;
#endif

    D(kprintf("[CACHE] %cCache write_8(%08lx, %02x, %x)\n", type == ICACHE ? 'I':'D', address, data, write_back));

    D(kprintf("[CACHE]   set = %u, ", set));
    D(kprintf("tag = %08x, ", tag));

    /* Check the given set and find the cache way with mathing tag and falid flag set */
    for (unsigned i=0; i < CACHE_WAY_COUNT; i++)
    {
        if ((cache->c_Tags[set][i] == tag) &&
            (cache->c_Flags[set][i] & F_VALID))
        {
            way = i;
            break;
        }
    }

    D(kprintf("way = %d\n", way));

    /* There was no valid cache line matching the tag */
    if (way == -1)
    {
        /* Write-through cache does not load cache line, performs direct write instead */
        if (write_back == 0)
            return 0;

        /* Write-back cache loads whole cache line and updates the value */
        way = cache_get_way(cache, set);
        D(kprintf("[CACHE]   allocated way = %d\n", way));
        if (cache->c_Flags[set][way] & F_VALID)
        {
            uint32_t line_address = cache->c_Tags[set][way] + (set << 4);
            D(kprintf("[CACHE]   cache line was previously used, tag=%08x, address=%08x, flushing\n",
        	    cache->c_Tags[set][way], line_address));
            
            /* Write cache back if the lines are dirty */
            if (cache->c_Flags[set][way] & F_DIRTY0)
                *(uint32_t*)(uintptr_t)line_address = cache->c_Lines[set][way].cl_32[0];
            if (cache->c_Flags[set][way] & F_DIRTY1)
                *(uint32_t*)(uintptr_t)(line_address + 4) = cache->c_Lines[set][way].cl_32[1];
            if (cache->c_Flags[set][way] & F_DIRTY2)
                *(uint32_t*)(uintptr_t)(line_address + 8) = cache->c_Lines[set][way].cl_32[2];
            if (cache->c_Flags[set][way] & F_DIRTY3)
                *(uint32_t*)(uintptr_t)(line_address + 12) = cache->c_Lines[set][way].cl_32[3];
        }

        /* Load the cache line */
        D(kprintf("[CACHE]   loading line from address %08x\n", address & 0xfffffff0));
        cache->c_Lines[set][way].cl_128 = *(uint128_t *)(uintptr_t)(address & 0xfffffff0);

        /* Update tag, mark page as valid */
        cache->c_Flags[set][way] = F_VALID;
        cache->c_Tags[set][way] = tag;
    }

    /* Mark LRUm */
    cache_mark_hit(cache, set, way);

    cache->c_Lines[set][way].cl_8[address & 15] = data;

    if (write_back)
    {
        /* Write-back cache marks the portion of cache line as dirty */
        switch((address >> 2) & 3)
        {
            case 0:
                cache->c_Flags[set][way] |= F_DIRTY0;
                break;
            case 1:
                cache->c_Flags[set][way] |= F_DIRTY1;
                break;
            case 2:
                cache->c_Flags[set][way] |= F_DIRTY2;
                break;
            case 3:
                cache->c_Flags[set][way] |= F_DIRTY3;
                break;
        }
    }
    else
    {
        /* Write-through cache performs direct write to memory, cache line remains not-dirty */
        *(uint8_t *)(uintptr_t)address = data;
    }

    return 1;
}

