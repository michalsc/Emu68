/*
    Copyright © 2019 Michal Schulz <michal.schulz@gmx.de>
    https://github.com/michalsc

    This Source Code Form is subject to the terms of the
    Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed
    with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "support.h"
#include "M68k.h"
#include "EmuFeatures.h"

uint8_t SR_GetEALength(uint16_t *insn_stream, uint8_t ea, uint8_t imm_size)
{
    uint8_t word_count = 0;
    uint8_t mode, reg;

    mode = (ea >> 3) & 7;
    reg = ea & 7;

    /* modes 0, 1, 2, 3 and 4 do not have extra words */
    if (mode > 4)
    {
        if (mode == 5)      /* 16-bit offset in next opcode */
            word_count++;
        else if (mode == 6 || (mode == 7 && reg == 3))
        {
            /* Reg- or PC-relative addressing mode */
            uint16_t brief = BE16(insn_stream[0]);

            /* Brief word is here */
            word_count++;

            if (brief & 0x100)
            {
                /* Full brief format */
                switch (brief & 3)
                {
                    case 2:
                        word_count++;       /* Word outer displacement */
                        break;
                    case 3:
                        word_count += 2;    /* Long outer displacement */
                        break;
                }

                switch (brief & 0x30)
                {
                    case 0x20:
                        word_count++;       /* Word base displacement */
                        break;
                    case 0x30:
                        word_count += 2;    /* Long base displacement */
                        break;
                }
            }
        }
        else if (mode == 7)
        {
            if (reg == 2) /* PC-relative with 16-bit offset in next opcode */
                word_count++;
            else if (reg == 0)  /* Absolute word */
                word_count++;
            else if (reg == 1)  /* Absolute long */
                word_count += 2;
            else if (reg == 4)  /* Immediate */
            {
                switch (imm_size)
                {
                    case 1:
                        word_count++;
                        break;
                    case 2:
                        word_count++;
                        break;
                    case 4:
                        word_count+=2;
                        break;
                    case 8:
                        word_count+=4;
                        break;
                    case 12:
                        word_count+=6;
                    default:
                        break;
                }
            }
        }
    }

    return word_count;
}


int M68K_GetLine4Length(uint16_t *insn_stream)
{
    uint16_t opcode = BE16(*insn_stream);
    int length = 1;
    int need_ea = 1;
    int opsize = 2;

    /* 0100000011xxxxxx - MOVE from SR */
    if ((opcode & 0xffc0) == 0x40c0)
    {
        need_ea = 1;
        opsize = 2;
    }
    /* 0100001011xxxxxx - MOVE from CCR */
    else if ((opcode &0xffc0) == 0x42c0)
    {
        need_ea = 1;
        opsize = 2;
    }
    /* 01000000ssxxxxxx - NEGX */
    else if ((opcode & 0xff00) == 0x4000 && (opcode & 0xc0) != 0xc0)
    {
        need_ea = 1;
        opsize = 0;
    }
    /* 01000010ssxxxxxx - CLR */
    else if ((opcode & 0xff00) == 0x4200 && (opcode & 0xc0) != 0xc0)
    {
        need_ea = 1;
        opsize = 0;
    }
    /* 0100010011xxxxxx - MOVE to CCR */
    else if ((opcode &0xffc0) == 0x44c0)
    {
        need_ea = 1;
        opsize = 2;
    }
    /* 01000100ssxxxxxx - NEG */
    else if ((opcode &0xff00) == 0x4400 && (opcode & 0xc0) != 0xc0)
    {
        need_ea = 1;
        opsize = 0;
    }
    /* 0100011011xxxxxx - MOVE to SR */
    else if ((opcode &0xffc0) == 0x46c0)
    {
        need_ea = 1;
        opsize = 2;
    }
    /* 01000110ssxxxxxx - NOT */
    else if ((opcode &0xff00) == 0x4600 && (opcode & 0xc0) != 0xc0)
    {
        need_ea = 1;
        opsize = 0;
    }
    /* 0100100xxx000xxx - EXT, EXTB */
    else if ((opcode & 0xfeb8) == 0x4880)
    {
        need_ea = 0;
    }
    /* 0100100000001xxx - LINK - 32 bit offset */
    else if ((opcode & 0xfff8) == 0x4808)
    {
        need_ea = 0;
        length = 3;
    }
    /* 0100100000xxxxxx - NBCD */
    else if ((opcode & 0xffc0) == 0x4800 && (opcode & 0x08) != 0x08)
    {
        need_ea = 1;
        opsize = 0;
    }
    /* 0100100001000xxx - SWAP */
    else if ((opcode & 0xfff8) == 0x4840)
    {
        need_ea = 0;
    }
    /* 0100100001001xxx - BKPT */
    else if ((opcode & 0xfff8) == 0x4848)
    {
        need_ea = 0;
    }
    /* 0100100001xxxxxx - PEA */
    else if ((opcode & 0xffc0) == 0x4840 && (opcode & 0x38) != 0x08)
    {
        need_ea = 1;
        opsize = 0;
    }
    /* 0100101011111100 - ILLEGAL */
    else if (opcode == 0x4afc)
    {
        need_ea = 0;
    }
    /* 0100101011xxxxxx - TAS */
    else if ((opcode & 0xffc0) == 0x4ac0)
    {
        need_ea = 1;
        opsize = 0;
    }
    /* 0100101011xxxxxx - TST */
    else if ((opcode & 0xff00) == 0x4a00 && (opcode & 0xc0) != 0xc0)
    {
        need_ea = 1;
        switch (opcode & 0x00c0)
        {
            case 0x0000:    /* Byte operation */
                opsize = 1;
                break;
            case 0x0040:    /* Short operation */
                opsize = 2;
                break;
            case 0x0080:    /* Long operation */
                opsize = 4;
                break;
        }
    }
    /* 0100110000xxxxxx - MULU, MULS, DIVU, DIVUL, DIVS, DIVSL */
    else if ((opcode & 0xff80) == 0x4c00 || (opcode == 0x83c0))
    {
        length = 2;
        opsize = 4;
        need_ea = 1;
    }
    /* 010011100100xxxx - TRAP */
    else if ((opcode & 0xfff0) == 0x4e40)
    {
        need_ea = 0;
    }
    /* 0100111001010xxx - LINK */
    else if ((opcode & 0xfff8) == 0x4e50)
    {
        need_ea = 0;
        length = 2;
    }
    /* 0100111001011xxx - UNLK */
    else if ((opcode & 0xfff8) == 0x4e58)
    {
        need_ea = 0;
    }
    /* 010011100110xxxx - MOVE USP */
    else if ((opcode & 0xfff0) == 0x4e60)
    {
        need_ea = 0;
    }
    /* 0100111001110000 - RESET */
    else if (opcode == 0x4e70)
    {
        need_ea = 0;
    }
    /* 0100111001110000 - NOP */
    else if (opcode == 0x4e71)
    {
        need_ea = 0;
    }
    /* 0100111001110010 - STOP */
    else if (opcode == 0x4e72)
    {
        need_ea = 0;
        length = 2;
    }
    /* 0100111001110011 - RTE */
    else if (opcode == 0x4e73)
    {
        need_ea = 0;
    }
    /* 0100111001110100 - RTD */
    else if (opcode == 0x4e74)
    {
        need_ea = 0;
        length = 2;
    }
    /* 0100111001110101 - RTS */
    else if (opcode == 0x4e75)
    {
        need_ea = 0;
    }
    /* 0100111001110110 - TRAPV */
    else if (opcode == 0x4e76)
    {
        need_ea = 0;
    }
    /* 0100111001110111 - RTR */
    else if (opcode == 0x4e77)
    {
        need_ea = 0;
    }
    /* 010011100111101x - MOVEC */
    else if ((opcode & 0xfffe) == 0x4e7a)
    {
        need_ea = 0;
        length = 2;
    }
    /* 0100111010xxxxxx - JSR */
    else if ((opcode & 0xffc0) == 0x4e80)
    {
        need_ea = 1;
        opsize = 0;
    }
    /* 0100111011xxxxxx - JMP */
    else if ((opcode & 0xffc0) == 0x4ec0)
    {
        need_ea = 1;
        opsize = 0;
    }
    /* 01001x001xxxxxxx - MOVEM */
    else if ((opcode & 0xfb80) == 0x4880)
    {
        need_ea = 0;
        length = 2;
    }
    /* 0100xxx111xxxxxx - LEA */
    else if ((opcode & 0xf1c0) == 0x41c0)
    {
        need_ea = 1;
        opsize = 0;
    }
    /* 0100xxx1x0xxxxxx - CHK */
    else if ((opcode & 0xf140) == 0x4100)
    {
        need_ea = 1;
        opsize = (opcode & 0x80) ? 2 : 4;
    }

    if (need_ea) {
        length += SR_GetEALength(&insn_stream[length], opcode & 0x3f, opsize);
    }

    kprintf("GetLine4Length for opcode %04x returns %d\n", opcode, 2*length);

    return length;
}

/* Check if opcode is of branch kind or may result in a */
int M68K_IsBranch(uint16_t *insn_stream)
{
    uint16_t opcode = BE16(*insn_stream);

    if (
        opcode == 0x007c            ||
        opcode == 0x027c            ||
        opcode == 0x0a7c            ||
        (opcode & 0xffc0) == 0x40c0 ||
        (opcode & 0xffc0) == 0x46c0 ||
        (opcode & 0xfff8) == 0x4848 ||
        opcode == 0x4afc            ||
        (opcode & 0xfff0) == 0x4e40 ||
        (opcode & 0xfff0) == 0x4e60 ||
        opcode == 0x4e70            ||
        opcode == 0x4e72            ||
        opcode == 0x4e73            ||
        opcode == 0x4e74            ||
        opcode == 0x4e75            ||
        opcode == 0x4e76            ||
        opcode == 0x4e77            ||
        (opcode & 0xfffe) == 0x4e7a ||
        (opcode & 0xff80) == 0x4e80 ||
        (opcode & 0xf0f8) == 0x50c8 ||
        (opcode & 0xf000) == 0x6000
    )
        return 1;
    else
        return 0;
}

int M68K_GetMoveLength(uint16_t *insn_stream)
{
    uint16_t opcode = BE16(*insn_stream);
    int size = 0;
    int length = 1;
    uint8_t ea = opcode & 0x3f;

    if ((opcode & 0x3000) == 0x1000)
        size = 1;
    else if ((opcode & 0x3000) == 0x2000)
        size = 4;
    else
        size = 2;

    length += SR_GetEALength(&insn_stream[length], ea & 0x3f, size);

    ea = (opcode >> 3) & 0x38;
    ea |= (opcode >> 9) & 0x7;

    length += SR_GetEALength(&insn_stream[length], ea, size);

    kprintf("GetMOVELength for opcode %04x returns %d\n", opcode, 2*length);

    return length;    
}

int M68K_GetLineFLength(uint16_t *insn_stream)
{
    uint16_t opcode = BE16(insn_stream[0]);
    uint16_t opcode2 = BE16(insn_stream[1]);
    int length = 1;
    int need_ea = 0;
    int opsize = 0;

    /* MOVE16 (Ax)+, (Ay)+ */
    if ((opcode & 0xfff8) == 0xf620 && (opcode2 & 0x8fff) == 0x8000)
    {
        length = 2;
    }
    /* MOVE16 other variations */
    else if ((opcode & 0xffe0) == 0xf600)
    {
        length = 3;
    }
    /* CINV */
    else if ((opcode & 0xff20) == 0xf400)
    {
        length = 1;
    }
    /* CPUSH */
    else if ((opcode & 0xff20) == 0xf420)
    {
        length = 1;
    }
    /* FMOVECR reg */
    else if (opcode == 0xf200 && (opcode2 & 0xfc00) == 0x5c00)
    {
        length = 2;
    }
    /* FABS */
    else if ((opcode & 0xffc0) == 0xf200 && ((opcode2 & 0xa07f) == 0x0018 || (opcode2 & 0xa07b) == 0x0058))
    {
        length = 2;
        if (opcode2 & 0x4000)
        {
            need_ea = 1;
            switch ((opcode2 >> 10) & 7)
            {
                case 0:
                case 1:
                    opsize = 2;
                    break;
                
                case 2:
                    opsize = 6;
                    break;
                
                case 3:     // Packed!!
                    opsize = 6;
                    break;
                
                case 4:
                case 6:
                    opsize = 1;
                    break;

                case 5:
                    opsize = 4;
                    break;
            }
        }
    }
    /* FADD */
    else if ((opcode & 0xffc0) == 0xf200 && ((opcode2 & 0xa07f) == 0x0022 || (opcode2 & 0xa07b) == 0x0062))
    {
        length = 2;
        if (opcode2 & 0x4000)
        {
            need_ea = 1;
            switch ((opcode2 >> 10) & 7)
            {
                case 0:
                case 1:
                    opsize = 2;
                    break;
                
                case 2:
                    opsize = 6;
                    break;
                
                case 3:     // Packed!!
                    opsize = 6;
                    break;
                
                case 4:
                case 6:
                    opsize = 1;
                    break;

                case 5:
                    opsize = 4;
                    break;
            }
        }
    }
    /* FBcc */
    else if ((opcode & 0xff80) == 0xf280)
    {
        if (opcode & (1 << 6))
        {
            length = 3;
        }
        else
        {
            length = 2;
        }
    }
    /* FCMP */
    else if ((opcode & 0xffc0) == 0xf200 && (opcode2 & 0xa07f) == 0x0038)
    {
        length = 2;
        if (opcode2 & 0x4000)
        {
            need_ea = 1;
            switch ((opcode2 >> 10) & 7)
            {
                case 0:
                case 1:
                    opsize = 2;
                    break;
                
                case 2:
                    opsize = 6;
                    break;
                
                case 3:     // Packed!!
                    opsize = 6;
                    break;
                
                case 4:
                case 6:
                    opsize = 1;
                    break;

                case 5:
                    opsize = 4;
                    break;
            }
        }
    }
    /* FDIV */
    else if ((opcode & 0xffc0) == 0xf200 && ((opcode2 & 0xa07f) == 0x0020 || (opcode2 & 0xa07b) == 0x0060))
    {
        length = 2;
        if (opcode2 & 0x4000)
        {
            need_ea = 1;
            switch ((opcode2 >> 10) & 7)
            {
                case 0:
                case 1:
                    opsize = 2;
                    break;
                
                case 2:
                    opsize = 6;
                    break;
                
                case 3:     // Packed!!
                    opsize = 6;
                    break;
                
                case 4:
                case 6:
                    opsize = 1;
                    break;

                case 5:
                    opsize = 4;
                    break;
            }
        }
    }
    /* FINT */
    else if ((opcode & 0xffc0) == 0xf200 && (opcode2 & 0xa07f) == 0x0001)
    {
        length = 2;
        if (opcode2 & 0x4000)
        {
            need_ea = 1;
            switch ((opcode2 >> 10) & 7)
            {
                case 0:
                case 1:
                    opsize = 2;
                    break;
                
                case 2:
                    opsize = 6;
                    break;
                
                case 3:     // Packed!!
                    opsize = 6;
                    break;
                
                case 4:
                case 6:
                    opsize = 1;
                    break;

                case 5:
                    opsize = 4;
                    break;
            }
        }
    }
    /* FINTRZ */
    else if ((opcode & 0xffc0) == 0xf200 && (opcode2 & 0xa07f) == 0x0003)
    {
        length = 2;
        if (opcode2 & 0x4000)
        {
            need_ea = 1;
            switch ((opcode2 >> 10) & 7)
            {
                case 0:
                case 1:
                    opsize = 2;
                    break;
                
                case 2:
                    opsize = 6;
                    break;
                
                case 3:     // Packed!!
                    opsize = 6;
                    break;
                
                case 4:
                case 6:
                    opsize = 1;
                    break;

                case 5:
                    opsize = 4;
                    break;
            }
        }
    }
    /* FLOGN */
    else if ((opcode & 0xffc0) == 0xf200 && (opcode2 & 0xa07f) == 0x0014)
    {
        length = 2;
        if (opcode2 & 0x4000)
        {
            need_ea = 1;
            switch ((opcode2 >> 10) & 7)
            {
                case 0:
                case 1:
                    opsize = 2;
                    break;
                
                case 2:
                    opsize = 6;
                    break;
                
                case 3:     // Packed!!
                    opsize = 6;
                    break;
                
                case 4:
                case 6:
                    opsize = 1;
                    break;

                case 5:
                    opsize = 4;
                    break;
            }
        }
    }
    /* FMOVE to REG */
    else if ((opcode & 0xffc0) == 0xf200 && ((opcode2 & 0xa07f) == 0x0000 || (opcode2 & 0xa07b) == 0x0040))
    {
        length = 2;
        if (opcode2 & 0x4000)
        {
            need_ea = 1;
            switch ((opcode2 >> 10) & 7)
            {
                case 0:
                case 1:
                    opsize = 2;
                    break;
                
                case 2:
                    opsize = 6;
                    break;
                
                case 3:     // Packed!!
                    opsize = 6;
                    break;
                
                case 4:
                case 6:
                    opsize = 1;
                    break;

                case 5:
                    opsize = 4;
                    break;
            }
        }
    }
    /* FMOVE to MEM */
    else if ((opcode & 0xffc0) == 0xf200 && (opcode2 & 0xe07f) == 0x6000)
    {
        length = 2;
        need_ea = 1;
        switch ((opcode2 >> 10) & 7)
        {
            case 0:
            case 1:
                opsize = 2;
                break;
            
            case 2:
                opsize = 6;
                break;
            
            case 3:     // Packed!!
            case 7:
                opsize = 6;
                break;
            
            case 4:
            case 6:
                opsize = 1;
                break;

            case 5:
                opsize = 4;
                break;
        }
    }
    /* FMOVE from special */
    else if ((opcode & 0xffc0) == 0xf200 && (opcode2 & 0xe3ff) == 0xa000)
    {
        length = 2;
        need_ea = 1;
        opsize = 2;
    }
    /* FMOVE to special */
    else if ((opcode & 0xffc0) == 0xf200 && (opcode2 & 0xe3ff) == 0x8000)
    {
        length = 2;
        need_ea = 1;
        opsize = 2;
    }
    /* FMOVEM */
    else if ((opcode & 0xffc0) == 0xf200 && (opcode2 & 0xc700) == 0xc000)
    {
        length = 2;
        need_ea = 1;
    }
    /* FMUL */
    else if ((opcode & 0xffc0) == 0xf200 && ((opcode2 & 0xa07f) == 0x0023 || (opcode2 & 0xa07b) == 0x0063))
    {
        length = 2;
        if (opcode2 & 0x4000)
        {
            need_ea = 1;
            switch ((opcode2 >> 10) & 7)
            {
                case 0:
                case 1:
                    opsize = 2;
                    break;
                
                case 2:
                    opsize = 6;
                    break;
                
                case 3:     // Packed!!
                    opsize = 6;
                    break;
                
                case 4:
                case 6:
                    opsize = 1;
                    break;

                case 5:
                    opsize = 4;
                    break;
            }
        }
    }
    /* FNEG */
    else if ((opcode & 0xffc0) == 0xf200 && ((opcode2 & 0xa07f) == 0x001a || (opcode2 & 0xa07b) == 0x005a))
    {
        length = 2;
        if (opcode2 & 0x4000)
        {
            need_ea = 1;
            switch ((opcode2 >> 10) & 7)
            {
                case 0:
                case 1:
                    opsize = 2;
                    break;
                
                case 2:
                    opsize = 6;
                    break;
                
                case 3:     // Packed!!
                    opsize = 6;
                    break;
                
                case 4:
                case 6:
                    opsize = 1;
                    break;

                case 5:
                    opsize = 4;
                    break;
            }
        }
    }
    /* FTST */
    else if ((opcode & 0xffc0) == 0xf200 && (opcode2 & 0xa07f) == 0x003a)
    {
        length = 2;
        if (opcode2 & 0x4000)
        {
            need_ea = 1;
            switch ((opcode2 >> 10) & 7)
            {
                case 0:
                case 1:
                    opsize = 2;
                    break;
                
                case 2:
                    opsize = 6;
                    break;
                
                case 3:     // Packed!!
                    opsize = 6;
                    break;
                
                case 4:
                case 6:
                    opsize = 1;
                    break;

                case 5:
                    opsize = 4;
                    break;
            }
        }
    }
    /* FScc */
    else if ((opcode & 0xffc0) == 0xf240 && (opcode2 & 0xffc0) == 0)
    {
        need_ea = 1;
        opsize = 1;
        length = 2;
    }
    /* FSQRT */
    else if ((opcode & 0xffc0) == 0xf200 && ((opcode2 & 0xa07f) == 0x0004 || (opcode2 & 0xa07b) == 0x0041))
    {
        length = 2;
        if (opcode2 & 0x4000)
        {
            need_ea = 1;
            switch ((opcode2 >> 10) & 7)
            {
                case 0:
                case 1:
                    opsize = 2;
                    break;
                
                case 2:
                    opsize = 6;
                    break;
                
                case 3:     // Packed!!
                    opsize = 6;
                    break;
                
                case 4:
                case 6:
                    opsize = 1;
                    break;

                case 5:
                    opsize = 4;
                    break;
            }
        }
    }
    /* FSUB */
    else if ((opcode & 0xffc0) == 0xf200 && ((opcode2 & 0xa07f) == 0x0028 || (opcode2 & 0xa07b) == 0x0068))
    {
                length = 2;
        if (opcode2 & 0x4000)
        {
            need_ea = 1;
            switch ((opcode2 >> 10) & 7)
            {
                case 0:
                case 1:
                    opsize = 2;
                    break;
                
                case 2:
                    opsize = 6;
                    break;
                
                case 3:     // Packed!!
                    opsize = 6;
                    break;
                
                case 4:
                case 6:
                    opsize = 1;
                    break;

                case 5:
                    opsize = 4;
                    break;
            }
        }
    }
    /* FSIN */
    else if ((opcode & 0xffc0) == 0xf200 && (opcode2 & 0xa07f) == 0x000e)
    {
                length = 2;
        if (opcode2 & 0x4000)
        {
            need_ea = 1;
            switch ((opcode2 >> 10) & 7)
            {
                case 0:
                case 1:
                    opsize = 2;
                    break;
                
                case 2:
                    opsize = 6;
                    break;
                
                case 3:     // Packed!!
                    opsize = 6;
                    break;
                
                case 4:
                case 6:
                    opsize = 1;
                    break;

                case 5:
                    opsize = 4;
                    break;
            }
        }
    }
    /* FCOS */
    else if ((opcode & 0xffc0) == 0xf200 && (opcode2 & 0xa07f) == 0x001d)
    {
                length = 2;
        if (opcode2 & 0x4000)
        {
            need_ea = 1;
            switch ((opcode2 >> 10) & 7)
            {
                case 0:
                case 1:
                    opsize = 2;
                    break;
                
                case 2:
                    opsize = 6;
                    break;
                
                case 3:     // Packed!!
                    opsize = 6;
                    break;
                
                case 4:
                case 6:
                    opsize = 1;
                    break;

                case 5:
                    opsize = 4;
                    break;
            }
        }
    }
    /* FNOP */
    else if (opcode == 0xf280 && opcode2 == 0)
    {
        length = 2;
    }
    else
    {
        length = 2;
    }


    if (need_ea) {
        length += SR_GetEALength(&insn_stream[length], opcode & 0x3f, opsize * 2);
    }

    kprintf("GetLineFLength for opcode %04x returns %d\n", opcode, 2*length);

    return length;
}

/* Get number of 16-bit words this instruction occupies */
int M68K_GetINSNLength(uint16_t *insn_stream)
{
    uint16_t opcode = *insn_stream;
    int length = 0;

    switch(opcode & 0xf000)
    {
        case 0x0000:
            length = M68K_GetLine0Length(insn_stream);
            break;
        case 0x1000: /* Fallthrough */
        case 0x2000: /* Fallthrough */
        case 0x3000:
            length = M68K_GetMoveLength(insn_stream);
            break;
        case 0x4000:
            length = M68K_GetLine4Length(insn_stream);
            break;
        case 0x5000:
            length = M68K_GetLine5Length(insn_stream);
            break;
        case 0x6000:
            length = M68K_GetLine6Length(insn_stream);
            break;
        case 0x7000:
            length = 1;
            break;
        case 0x8000:
            length = M68K_GetLine8Length(insn_stream);
            break;
        case 0x9000:
            length = M68K_GetLine9Length(insn_stream);
            break;
        case 0xa000:
            length = 1;
            break;
        case 0xb000:
            length = M68K_GetLineBLength(insn_stream);
            break;
        case 0xc000:
            length = M68K_GetLineCLength(insn_stream);
            break;
        case 0xd000:
            length = M68K_GetLineDLength(insn_stream);
            break;
        case 0xe000:
            length = M68K_GetLineELength(insn_stream);
            break;
        case 0xf000:
            length = M68K_GetLineFLength(insn_stream);
            break;
        default:
            break;
    }

//    kprintf(" = %d\n", length);

    return length;
}

#define D(x) /* x */

typedef uint32_t (*SR_Check)(uint16_t opcode);

static uint32_t GetSR_def(uint16_t opcode)
{
    (void)opcode;
    return (SR_CCR << 16) | SR_CCR;
}

static SR_Check SRCheck[16] = {
    GetSR_Line0,
    GetSR_Line1,
    GetSR_Line2,
    GetSR_Line3,
    GetSR_Line4,
    GetSR_Line5,
    GetSR_Line6,
    GetSR_Line7,
    GetSR_Line8,
    GetSR_Line9,
    GetSR_def,
    GetSR_LineB,
    GetSR_LineC,
    GetSR_LineD,
    GetSR_LineE,
    GetSR_def
};

/* Get the mask of status flags changed by the instruction specified by the opcode */
uint8_t M68K_GetSRMask(uint16_t *insn_stream)
{
    uint16_t opcode = BE16(*insn_stream);
    int scan_depth = 0;
    const int max_scan_depth = 20;
    uint8_t mask = 0;
    uint8_t needed = 0;
    uint8_t tmp_sets = 0;
    uint8_t tmp_needs = 0;

    D(kprintf("[JIT] GetSRMask, opcode %04x @ %08x, ", opcode, insn_stream));

    uint32_t flags = SRCheck[opcode >> 12](opcode);
    mask = flags & 0x1f;
    tmp_sets = flags & 0x1f;
    tmp_needs = (flags >> 16) & 0x1f;

    D(kprintf(" SRNeeds = %x, SRSets = %x\n", tmp_needs, tmp_sets));

    /*
        Check as long as there are still some flags to be set by the opcode and the depth
        of scan is not exceeded
    */
    while(mask != 0 && scan_depth < max_scan_depth)
    {
        #if 1
        if (
            //(opcode >> 12) == 0 ||
            //(opcode >> 12) == 1 ||
            //(opcode >> 12) == 2 ||
            //(opcode >> 12) == 3 ||
            (opcode >> 12) == 4 ||
            //(opcode >> 12) == 5 ||
            //(opcode >> 12) == 6 ||
            //(opcode >> 12) == 7 ||
            //(opcode >> 12) == 8 ||
            //(opcode >> 12) == 9 ||
            //(opcode >> 12) == 11 ||
            //(opcode >> 12) == 12 ||
            //(opcode >> 12) == 13 ||
            //(opcode >> 12) == 14 ||
            0
        )   
            return mask | needed;
        #endif
        
        /* Increase scan depth level */
        scan_depth++;

        /* If instruction is a branch break the scan */
        if (M68K_IsBranch(insn_stream))
        {
            /* Check if BRA/BSR and follow if possible */
            if ((opcode & 0xfe00) == 0x6000)
            {
                int32_t branch_offset = (int8_t)(opcode & 0xff);

                if ((opcode & 0xff) == 0) {
                    branch_offset = (int16_t)BE16(insn_stream[1]);
                } else if ((opcode & 0xff) == 0xff) {
                    uint16_t lo16, hi16;
                    hi16 = BE16(insn_stream[1]);
                    lo16 = BE16(insn_stream[2]);
                    branch_offset = lo16 | (hi16 << 16);
                }

                insn_stream = insn_stream + 1 + (branch_offset >> 1);

                D(kprintf("[JIT]   %02d: PC-relative jump by %d bytes to %08x\n", scan_depth, branch_offset, insn_stream));
            }
            /* Check if JMP/JSR and follow if possible */
            else if ((opcode & 0xffbe) == 0x4eb8)
            {
                if (opcode & 1) {
                    uint16_t lo16, hi16;
                    hi16 = BE16(insn_stream[1]);
                    lo16 = BE16(insn_stream[2]);
                    insn_stream = (uint16_t*)(uintptr_t)(lo16 | (hi16 << 16));
                } else {
                    insn_stream = (uint16_t*)(uintptr_t)((uint32_t)BE16(insn_stream[1]));
                }

                D(kprintf("[JIT]   %02d: Absolute jump to %08x\n", scan_depth, insn_stream));
            }
            else if ((opcode & 0xf000) == 0x6000)
            {
                int32_t branch_offset = (int8_t)(opcode & 0xff);
                uint16_t *insn_stream_2 = insn_stream + 1;

                // Mark the flags which conditional jump needs by itself
                needed |= mask & (SRCheck[opcode >> 12](opcode) >> 16);

                if ((opcode & 0xff) == 0) {
                    branch_offset = (int16_t)BE16(insn_stream[1]);
                    insn_stream_2++;
                } else if ((opcode & 0xff) == 0xff) {
                    uint16_t lo16, hi16;
                    hi16 = BE16(insn_stream[1]);
                    lo16 = BE16(insn_stream[2]);
                    branch_offset = lo16 | (hi16 << 16);
                    insn_stream_2+=2;
                }

                insn_stream = insn_stream + 1 + (branch_offset >> 1);

                D(kprintf("[JIT]   %02d: Splitting into two paths %08x and %08x\n", scan_depth, insn_stream, insn_stream_2));

                uint8_t mask1 = mask;
                uint8_t mask2 = mask;
                uint8_t needed1 = needed;
                uint8_t needed2 = needed;
                int scan_depth_tmp = scan_depth;

                while(mask1 && scan_depth < max_scan_depth)
                {
                    scan_depth++;

                    /* If instruction is a branch break the scan */
                    if (M68K_IsBranch(insn_stream))
                        break;

                    /* Get opcode */
                    opcode = BE16(*insn_stream);

                    D(kprintf("[JIT]   %02d.1: opcode=%04x @ %08x ", scan_depth, opcode, insn_stream));

                    uint32_t flags = SRCheck[opcode >> 12](opcode);
                    tmp_sets = flags & 0x1f;
                    tmp_needs = (flags >> 16) & 0x1f;

                    D(kprintf(" SRNeeds = %x, SRSets = %x\n", tmp_needs, tmp_sets));

                    /* If instruction *needs* one of flags from current opcode, break the check and return mask */
                    if (mask1 & tmp_needs) {
                        needed1 |= (mask1 & tmp_needs);
                    }

                    /* Clear flags which this instruction sets */
                    mask1 = mask1 & ~tmp_sets;

                    if (tmp_needs == SR_CCR)
                    {
                        break;
                    }

                    /* Advance to subsequent instruction */
                    insn_stream += M68K_GetINSNLength(insn_stream);
                }

                scan_depth = scan_depth_tmp;

                while(mask2 && scan_depth < max_scan_depth)
                {
                    scan_depth++;

                    /* If instruction is a branch break the scan */
                    if (M68K_IsBranch(insn_stream_2))
                        break;

                    /* Get opcode */
                    opcode = BE16(*insn_stream_2);

                    D(kprintf("[JIT]   %02d.2: opcode=%04x @ %08x ", scan_depth, opcode, insn_stream_2));

                    uint32_t flags = SRCheck[opcode >> 12](opcode);
                    tmp_sets = flags & 0x1f;
                    tmp_needs = (flags >> 16) & 0x1f;

                    D(kprintf(" SRNeeds = %x, SRSets = %x\n", tmp_needs, tmp_sets));

                    /* If instruction *needs* one of flags from current opcode, break the check and return mask */
                    if (mask2 & tmp_needs) {
                        needed2 |= (mask2 & tmp_needs);
                    }

                    /* Clear flags which this instruction sets */
                    mask2 = mask2 & ~tmp_sets;

                    if (tmp_needs == SR_CCR)
                    {
                        break;
                    }

                    /* Advance to subsequent instruction */
                    insn_stream_2 += M68K_GetINSNLength(insn_stream_2);
                }

                D(kprintf("[JIT]   joining masks %x and %x to %x\n", mask1 | needed1, mask2 | needed2, mask1 | needed1 | mask2 | needed2));

                return mask1 | needed1 | mask2 | needed2;
            }
            else 
            {
                D(kprintf("[JIT]   %02d: check breaks on branch\n", scan_depth));
                break;
            }
        }
        else          
        {
            /* Advance to subsequent instruction */
            insn_stream += M68K_GetINSNLength(insn_stream);
        }
        
        /* Get opcode */
        opcode = BE16(*insn_stream);
        D(kprintf("[JIT]   %02d: opcode=%04x @ %08x ", scan_depth, opcode, insn_stream));

        uint32_t flags = SRCheck[opcode >> 12](opcode);
        tmp_sets = flags & 0x1f;
        tmp_needs = (flags >> 16) & 0x1f;

        D(kprintf(" SRNeeds = %x, SRSets = %x\n", tmp_needs, tmp_sets));

        /* If instruction *needs* one of flags from current opcode, break the check and return mask */
        if (mask & tmp_needs) {
            needed |= (mask & tmp_needs);
        }

        /* Clear flags which this instruction sets */
        mask = mask & ~tmp_sets;

        if (tmp_needs == SR_CCR) {
            break;
        }
    }

    D(kprintf("[JIT] GetSRMask returns %x\n", mask | needed));

    return mask | needed;
}
