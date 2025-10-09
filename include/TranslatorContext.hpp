#ifndef _TRANSLATORCONTEXT_HPP
#define _TRANSLATORCONTEXT_HPP

#include <cstdint>
#include <initializer_list>

extern "C" {
#include "A64.h"
}

#undef EMIT

namespace Emu68 {

struct TranslatorContext {
    uint32_t *      tc_CodeStart;
    uint32_t *      tc_CodePtr;
    
    uint32_t * EMIT(std::initializer_list<uint32_t> list) {
        for(auto insn: list) {
            *tc_CodePtr++ = insn;
        }
        return tc_CodePtr;
    }

    uint32_t * EMIT(uint32_t insn) {
        *tc_CodePtr++ = insn;
        return tc_CodePtr;
    }

    void LoadImmediate(uint8_t rd, uint32_t immed) {
        /* Handle special cases */
        if (immed == 0) {
            EMIT(mov_reg(rd, WZR));
            return;
        }

        if (immed == 0xffffffff) {
            EMIT(mvn_reg(rd, WZR, LSL, 0));
            return;
        }

        /* Try to use bitmask immediate */
        uint32_t mask = number_to_mask(immed);

        if (mask) {
            EMIT(orr_immed(rd, WZR, (mask >> 16) & 0x3f, mask & 0x3f));
            return;
        }

        /* All shortcuts so far failed, test if it is possible to generate 32-bit immediate with one move */
        if ((immed & 0xffff) == 0) {
            EMIT(mov_immed_u16(rd, (immed >> 16) & 0xffff, 1));
        }
        else if ((immed & 0xffff) == 0xffff) {
            EMIT(movn_immed_u16(rd, (~immed >> 16) & 0xffff, 1));
        }
        else if ((immed & 0xffff0000) == 0) {
            EMIT(mov_immed_u16(rd, immed & 0xffff, 0));
        }
        else if ((immed & 0xffff0000) == 0xffff0000) {
            EMIT(movn_immed_u16(rd, ~immed & 0xffff, 0));
        }
        else {
            EMIT({
                mov_immed_u16(rd, immed & 0xffff, 0),
                movk_immed_u16(rd, (immed >> 16) & 0xffff, 1)
            });
        }
    }
};

}

#endif /* _TRANSLATORCONTEXT_HPP */
