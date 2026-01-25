/*
    Copyright © 2019-2025 Michal Schulz <michal.schulz@gmx.de>
    https://github.com/michalsc

    This Source Code Form is subject to the terms of the
    Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed
    with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <emu68/LRUCache>

#define likely(x)       __builtin_expect(!!(x), 1) 
#define unlikely(x)     __builtin_expect(!!(x), 0)

namespace Emu68 {

uint32_t* LRUCache::findBlock(uint32_t address)
{
    const uint32_t set = addressToSet(address);
    struct Entry* e = &cache[set * WAY_COUNT];
    uint32_t mask = 0x80000000;
    
    for (uint32_t i = 0; i < WAY_COUNT; i++, mask >>= 1)
    {
        if (likely(e[i].ppc == address))
        {
            /* Tell CPU we are going to execute the code soon, give it time to prefetch eventually */
            asm volatile ("prfm plil1keep, [%0]"::"r"(e[i].arm));

            uint32_t current = alloc[set] & ~mask; 
            if ((current >> WAY_COUNT) == 0) { current = ~mask; }
            alloc[set] = current;

            return e[i].arm;
        }
    }

    return nullptr;
}

void LRUCache::invalidateByARMAddress(uint32_t* addr)
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

void LRUCache::invalidateByAddress(uint32_t addr)
{
    const uint32_t set = addressToSet(addr);
    struct Entry* e = &cache[set * WAY_COUNT];

    for (unsigned i = 0; i < WAY_COUNT; i++)
    {
        if (e[i].ppc == addr)
        {
            e[i].arm = nullptr;
            e[i].ppc = 0xffffffff;
            alloc[set] |= (0x80000000 >> i);
            break;
        }
    }
}

void LRUCache::invalidateAll()
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

void LRUCache::insertBlock(uint32_t address, uint32_t* entryPoint)
{
    const uint32_t set = addressToSet(address);
    struct Entry* e = &cache[set * WAY_COUNT];
    int loc = __builtin_clz(alloc[set]);
    uint32_t mask = 0x80000000 >> loc;

    // Insert new entry
    e[loc].ppc = address;
    e[loc].arm = entryPoint;

    // Touch the last used
    uint32_t current = alloc[set] & ~mask; 
    if ((current >> WAY_COUNT) == 0) { current = ~mask; }
    alloc[set] = current;
}

} // namespace Emu68
