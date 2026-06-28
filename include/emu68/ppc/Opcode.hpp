/*
    Copyright © 2019-2025 Michal Schulz <michal.schulz@gmx.de>
    https://github.com/michalsc

    This Source Code Form is subject to the terms of the
    Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed
    with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include <cstdint>

namespace Emu68::PPC {

class Opcode {
    uint32_t opcode;

public:
    Opcode(uint32_t o) : opcode(o) {}

    bool illegal(uint32_t mask) const { return (opcode & mask) != 0; }

    uint32_t u32() const { return opcode; }
    int32_t i32() const { return (int32_t)opcode; }
    constexpr uint32_t u32(int s, int e) const {
        return (opcode >> (31 - e)) & ((1 << (1 + e - s)) - 1);
    }
    constexpr int32_t i32(int s, int e) const {
        return (int32_t)((opcode >> (31 - e)) & ((1 << (1 + e - s)) - 1));
    }
    constexpr uint16_t u16(int s, int e) const {
        return (uint16_t)((opcode >> (31 - e)) & ((1 << (1 + e - s)) - 1));
    }
    constexpr int16_t i16(int s, int e) const {
        return (int16_t)((opcode >> (31 - e)) & ((1 << (1 + e - s)) - 1));
    }
    constexpr uint8_t u8(int s, int e) const {
        return (uint8_t)((opcode >> (31 - e)) & ((1 << (1 + e - s)) - 1));
    }
    constexpr int8_t i8(int s, int e) const {
        return (int8_t)((opcode >> (31 - e)) & ((1 << (1 + e - s)) - 1));
    }
};

} // namespace Emu68::PPC
