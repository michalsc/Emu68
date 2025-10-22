#include <cpp/LRUCache>

#define likely(x)       __builtin_expect(!!(x), 1) 
#define unlikely(x)     __builtin_expect(!!(x), 0)

namespace Emu68 {

uint32_t *LRUCache::FindBlock(uint32_t address)
{
    const uint32_t set = ADDR_2_SET(address);
    struct Entry *e = &cache[set * WAY_COUNT];
    uint32_t mask = 0x80000000;
    
    for (int i=0; i < EMU68_LRU_WAY_COUNT; i++, mask >>= 1)
    {
        if (likely(e[i].ppc == address))
        {
            /* Tell CPU we are going to execute the code soon, give it time to prefetch eventually */
            asm volatile ("prfm plil1keep, [%0]"::"r"(e[i].arm));

            uint32_t current = alloc[set] & ~mask; 
            if (current == 0) current = ~mask;
            alloc[set] = current;

            return e[i].arm;
        }
    }

    return nullptr;
}


void LRUCache::InvalidateByARMAddress(uint32_t *addr)
{
    for (unsigned i = 0; i < SET_COUNT * WAY_COUNT; i++)
    {
        if (cache[i].arm == addr)
        {
            cache[i].arm = nullptr;
            cache[i].ppc = 0xffffffff;
            break;
        }
    }
}

void LRUCache::InvalidateByAddress(uint32_t addr)
{
    const uint32_t set = ADDR_2_SET(addr);
    struct Entry *e = &cache[set * WAY_COUNT];

    for (unsigned i = 0; i < WAY_COUNT; i++)
    {
        if (e[i].ppc == addr)
        {
            e[i].arm= nullptr;
            e[i].ppc = 0xffffffff;
            alloc[set] |= (0x80000000 >> i);
            break;
        }
    }
}

void LRUCache::InvalidateAll()
{
    for (unsigned i = 0; i < SET_COUNT * WAY_COUNT; i++)
    {
        cache[i].ppc = 0xffffffff;
        cache[i].arm = nullptr;
    }

    for (unsigned i = 0; i < SET_COUNT; i++)
    {
        alloc[i] = 0xffffffff;
    }
}

void LRUCache::InsertBlock(uint32_t address, uint32_t *entryPoint)
{
    const uint32_t set = ADDR_2_SET(address);
    struct Entry *e = &cache[set * WAY_COUNT];
    int loc = __builtin_clz(alloc[set]);
    uint32_t mask = 0x80000000 >> loc;

    // Insert new entry
    e[loc].ppc = address;
    e[loc].arm = entryPoint;

    // Touch the last used
    uint32_t current = alloc[set] & ~mask; 
    if (current == 0) current = ~mask;
    alloc[set] = current;
}


} // namespace Emu68
