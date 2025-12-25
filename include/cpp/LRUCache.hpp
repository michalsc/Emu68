#ifndef _CPP_LRUCACHE_H
#define _CPP_LRUCACHE_H

#include "config.h"
#include <cstdint>

namespace Emu68 {

constexpr uint32_t WAY_COUNT = EMU68_LRU_WAY_COUNT;
constexpr uint32_t SET_COUNT = EMU68_LRU_SET_COUNT;

struct Entry {
    uintptr_t ppc;
    uint32_t *arm;
};

class LRUCache {
    struct Entry cache[WAY_COUNT * SET_COUNT] __attribute__((aligned(64)));
    uint32_t alloc[SET_COUNT] __attribute__((aligned(64)));

    uint32_t ADDR_2_SET(uint32_t addr) const { return (((addr) >> 2) % SET_COUNT); }

public:
    uint32_t *findBlock(uint32_t address);
    void invalidateByARMAddress(uint32_t *addr);
    void invalidateByAddress(uint32_t addr);
    void invalidateAll();
    void insertBlock(uint32_t address, uint32_t *entryPoint);
    uintptr_t cacheLoc() const { return (uintptr_t)&cache; }
    uintptr_t allocLoc() const { return (uintptr_t)&alloc; }
};



}

#endif /* _CPP_LRUCACHE_H */
