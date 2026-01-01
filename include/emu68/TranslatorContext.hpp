/*
    Copyright © 2019-2025 Michal Schulz <michal.schulz@gmx.de>
    https://github.com/michalsc

    This Source Code Form is subject to the terms of the
    Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed
    with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include <cstdint>
#include <concepts>
#include <iterator>

// A64.h is not entirely c++ compliant
extern "C" {
#include "A64.h"
}

#undef EMIT

namespace Emu68 {

struct TranslatorContext {
    uint32_t* tc_CodeStart;
    uint32_t* tc_CodePtr;
    uint32_t  tc_InsnCount;
    bool tc_SupervisorChecked;
    bool tc_FPUChecked;

    TranslatorContext() : tc_CodeStart(nullptr), tc_CodePtr(nullptr),
                          tc_InsnCount(0), tc_SupervisorChecked(false) {}
    TranslatorContext(uint32_t* start) : tc_CodeStart(start), tc_CodePtr(start),
                                        tc_InsnCount(0), tc_SupervisorChecked(false) {}

    virtual uint8_t allocARMRegister() = 0;
    virtual uint8_t allocFPRegister() = 0;
    virtual void freeARMRegister(uint8_t arm_reg) = 0;
    virtual void freeFPRegister(uint8_t fp_reg) = 0;

    void emitStop() { emit(INSN_TO_LE(MARKER_STOP)); }
    void emitBreak() { emit(INSN_TO_LE(MARKER_BREAK)); }

    TranslatorContext& emit(uint32_t insn) {
        *tc_CodePtr++ = insn;
        return *this;
    }

    TranslatorContext& emit(std::initializer_list<uint32_t> list) {
        for(uint32_t insn : list) {
            *tc_CodePtr++ = insn;
        }
        return *this;
    }

    template<typename Iterator>
    TranslatorContext& emit(Iterator begin, Iterator end) {
        while(begin != end) {
            *tc_CodePtr++ = *begin++;
        }
        return *this;
    }

    template<typename Container>
    requires (!std::same_as<std::decay_t<Container>, uint32_t> &&
              !std::same_as<std::decay_t<Container>, std::initializer_list<uint32_t>>)
    TranslatorContext& emit(const Container& container) {
        return emit(std::begin(container), std::end(container));
    }

    TranslatorContext& operator<<(uint32_t insn) {
        return emit(insn);
    }

    TranslatorContext& operator<<(std::initializer_list<uint32_t> list) {
        return emit(list);
    }

    template<typename Container>
    requires (!std::same_as<std::decay_t<Container>, uint32_t> &&
              !std::same_as<std::decay_t<Container>, std::initializer_list<uint32_t>>)
    TranslatorContext& operator<<(const Container& container) {
        return emit(container);
    }

    void emitLoadImmediate(uint8_t rd, uint32_t immed) {
        /* Handle special cases */
        if (immed == 0) {
            emit(mov_reg(rd, WZR));
            return;
        }

        if (immed == 0xffffffff) {
            emit(mvn_reg(rd, WZR, LSL, 0));
            return;
        }

        /* Try to use bitmask immediate */
        uint32_t mask = number_to_mask(immed);

        if (mask) {
            emit(orr_immed(rd, WZR, (mask >> 16) & 0x3f, mask & 0x3f));
            return;
        }

        /* All shortcuts so far failed, test if it is possible to generate 32-bit immediate with one move */
        if ((immed & 0xffff) == 0) {
            emit(mov_immed_u16(rd, (immed >> 16) & 0xffff, 1));
        }
        else if ((immed & 0xffff) == 0xffff) {
            emit(movn_immed_u16(rd, (~immed >> 16) & 0xffff, 1));
        }
        else if ((immed & 0xffff0000) == 0) {
            emit(mov_immed_u16(rd, immed & 0xffff, 0));
        }
        else if ((immed & 0xffff0000) == 0xffff0000) {
            emit(movn_immed_u16(rd, ~immed & 0xffff, 0));
        }
        else {
            emit({
                mov_immed_u16(rd, immed & 0xffff, 0),
                movk_immed_u16(rd, (immed >> 16) & 0xffff, 1)
            });
        }
    }
};

} // namespace Emu68
