/*
    Copyright © 2019-2025 Michal Schulz <michal.schulz@gmx.de>
    https://github.com/michalsc

    This Source Code Form is subject to the terms of the
    Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed
    with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include <cstdint>

#include "config.h"

namespace Emu68 {

#if WAY_COUNT >= 32
#error "EMU68_LRU_WAY_COUNT too high"
#endif

constexpr uint32_t WAY_COUNT = EMU68_LRU_WAY_COUNT;
constexpr uint32_t SET_COUNT = EMU68_LRU_SET_COUNT;

struct Entry {
    uintptr_t ppc;
    uint32_t* arm;
};

class LRUCache {
    struct Entry cache[WAY_COUNT * SET_COUNT] __attribute__((aligned(64)));
    uint32_t alloc[SET_COUNT] __attribute__((aligned(64)));

    uint32_t addressToSet(uint32_t addr) const { return (((addr) >> 2) % SET_COUNT); }

public:
    uint32_t* findBlock(uint32_t address);
    void invalidateByARMAddress(uint32_t* addr);
    void invalidateByAddress(uint32_t addr);
    void invalidateAll();
    void insertBlock(uint32_t address, uint32_t* entryPoint);
    uintptr_t cacheLoc() const { return (uintptr_t)&cache; }
    uintptr_t allocLoc() const { return (uintptr_t)&alloc; }
};

} // namespace Emu68
