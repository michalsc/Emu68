/*
    Copyright © 2019 Michal Schulz <michal.schulz@gmx.de>
    https://github.com/michalsc

    This Source Code Form is subject to the terms of the
    Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed
    with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#ifndef _EMU68_CODEGEN_BASE_H
#define _EMU68_CODEGEN_BASE_H

#include <stdint.h>
#include <emu68/CodeGenerator.h>

namespace emu68 {

template< typename arch >
uint8_t CodeGenerator<arch>::GetEALength(const uint16_t *insn_stream, uint8_t imm_size)
{
    uint16_t opcode;
    uint8_t word_count = 1;
    uint8_t mode, reg;

    opcode = BE16(insn_stream[0]);
    mode = (opcode >> 3) & 7;
    reg = (opcode) & 7;

    /* modes 0, 1, 2, 3 and 4 do not have extra words */
    if (mode > 4) {
        if (mode == 5) {     /* 16-bit offset in next opcode */
            word_count++;
        } else if (mode == 6 || (mode == 7 && reg == 3)) {
            /* Reg- or PC-relative addressing mode */
            uint16_t brief = BE16(insn_stream[1]);
            /* Brief word is here */
            word_count++;
            switch (brief & 3) {
                case 2:
                    word_count++;       /* Word outer displacement */
                    break;
                case 3:
                    word_count += 2;    /* Long outer displacement */
                    break;
            }
            switch (brief & 0x30) {
                case 0x20:
                    word_count++;       /* Word base displacement */
                    break;
                case 0x30:
                    word_count += 2;    /* Long base displacement */
                    break;
            }
        } else if (mode == 7) {
            if (reg == 2) { /* PC-relative with 16-bit offset in next opcode */
                word_count++;
            } else if (reg == 0) { /* Absolute word */
                word_count++;
            } else if (reg == 1) { /* Absolute long */
                word_count += 2;
            } else if (reg == 4) { /* Immediate */
                switch (imm_size) {
                    case 1: /* passthrough */
                    case 2:
                        word_count++;
                        break;
                    case 4:
                        word_count+= 2;
                        break;
                    default:
                        break;
                }
            }
        }
    }
    return word_count;
}


template< typename arch >
void CodeGenerator<arch>::Compile()
{
    EmitPrologue();



    EmitEpilogue();
}


}

#endif /* _EMU68_CODEGEN_BASE_H */
