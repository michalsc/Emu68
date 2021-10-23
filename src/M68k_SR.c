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

static uint8_t SR_GetEALength(uint16_t *insn_stream, uint8_t ea, uint8_t imm_size)
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
#if 0
static uint8_t SR_TestOpcodeEA(uint16_t *insn_stream, uint32_t nest_level)
{
    uint16_t next_opcode;
    uint8_t mask = 0;
    uint8_t word_count;

    /* First calculate the EA length */
    word_count = 1 + SR_GetEALength(insn_stream + 1, BE16(*insn_stream) & 0x3f, 0);

    /* Get the opcode past current 2-byte instruction */
    next_opcode = BE16(insn_stream[word_count]);

    /* Fetch correct table baset on bits 12..15 of the opcode */
    struct SRMaskEntry *e = OpcodeMap[next_opcode >> 12];

    /* Search within table until SME_END is found */
    while (e->me_Type != SME_END)
    {
        if ((next_opcode & e->me_OpcodeMask) == e->me_Opcode)
        {
            /* Don't nest. Check only the SME_MASK type */
            if (e->me_Type == SME_MASK)
                mask = e->me_SRMask;
            else if (e->me_Type == SME_FUNC && nest_level < Options.M68K_TRANSLATION_DEPTH)
                mask = e->me_TestFunction(&insn_stream[word_count], nest_level+1);
        }
        e++;
    }

    return mask;
}

static uint8_t SR_TestOpcodeMOVEA(uint16_t *insn_stream, uint32_t nest_level)
{
    uint16_t opcode;
    uint16_t next_opcode;
    uint8_t mask = 0;
    uint8_t word_count;

    opcode = BE16(insn_stream[0]);

    /* First calculate the EA length */
    switch (opcode & 0x3000)
    {
        case 0x3000:
            word_count = 1 + SR_GetEALength(insn_stream + 1, opcode & 0x3f, 2);
            break;
        case 0x2000:
            word_count = 1 + SR_GetEALength(insn_stream + 1, opcode & 0x3f, 4);
            break;
        default:
            return 0;
    }

    /* Get the opcode past current 2-byte instruction */
    next_opcode = BE16(insn_stream[word_count]);

    /* Fetch correct table baset on bits 12..15 of the opcode */
    struct SRMaskEntry *e = OpcodeMap[next_opcode >> 12];

    /* Search within table until SME_END is found */
    while (e->me_Type != SME_END)
    {
        if ((next_opcode & e->me_OpcodeMask) == e->me_Opcode)
        {
            /* Don't nest. Check only the SME_MASK type */
            if (e->me_Type == SME_MASK)
                mask = e->me_SRMask;
            else if (e->me_Type == SME_FUNC && nest_level < Options.M68K_TRANSLATION_DEPTH)
                mask = e->me_TestFunction(&insn_stream[word_count], nest_level+1);

            break;
        }
        e++;
    }

    return mask;
}

static uint8_t SR_TestOpcodeADDA(uint16_t *insn_stream, uint32_t nest_level)
{
    uint16_t opcode;
    uint16_t next_opcode;
    uint8_t mask = 0;
    uint8_t word_count;

    opcode = BE16(insn_stream[0]);

    /* First calculate the EA length */
    switch (opcode & 0x01c0)
    {
        case 0x00c0:
            word_count = 1 + SR_GetEALength(insn_stream + 1, opcode & 0x3f, 2);
            break;
        case 0x01c0:
            word_count = 1 + SR_GetEALength(insn_stream + 1, opcode & 0x3f, 4);
            break;
        default:
            return 0;
    }

    /* Get the opcode past current 2-byte instruction */
    next_opcode = BE16(insn_stream[word_count]);

    /* Fetch correct table baset on bits 12..15 of the opcode */
    struct SRMaskEntry *e = OpcodeMap[next_opcode >> 12];

    /* Search within table until SME_END is found */
    while (e->me_Type != SME_END)
    {
        if ((next_opcode & e->me_OpcodeMask) == e->me_Opcode)
        {
            /* Don't nest. Check only the SME_MASK type */
            if (e->me_Type == SME_MASK)
                mask = e->me_SRMask;
            else if (e->me_Type == SME_FUNC && nest_level < Options.M68K_TRANSLATION_DEPTH)
                mask = e->me_TestFunction(&insn_stream[word_count], nest_level+1);

            break;
        }
        e++;
    }

    return mask;
}

static uint8_t SR_TestOpcode16B(uint16_t *insn_stream, uint32_t nest_level)
{
    uint16_t next_opcode;
    uint8_t mask = 0;

    /* Get the opcode past current 2-byte instruction */
    next_opcode = BE16(insn_stream[1]);

    /* Fetch correct table baset on bits 12..15 of the opcode */
    struct SRMaskEntry *e = OpcodeMap[next_opcode >> 12];

    /* Search within table until SME_END is found */
    while (e->me_Type != SME_END)
    {
        if ((next_opcode & e->me_OpcodeMask) == e->me_Opcode)
        {
            /* Don't nest. Check only the SME_MASK type */
            if (e->me_Type == SME_MASK)
                mask = e->me_SRMask;
            else if (e->me_Type == SME_FUNC && nest_level < Options.M68K_TRANSLATION_DEPTH)
                mask = e->me_TestFunction(&insn_stream[1], nest_level+1);
            break;
        }
        e++;
    }

    return mask;
}

static uint8_t SR_TestOpcode32B(uint16_t *insn_stream, uint32_t nest_level)
{
    uint16_t next_opcode;
    uint8_t mask = 0;

    /* Get the opcode past current 4-byte instruction */
    next_opcode = BE16(insn_stream[2]);

    /* Fetch correct table baset on bits 12..15 of the opcode */
    struct SRMaskEntry *e = OpcodeMap[next_opcode >> 12];

    /* Search within table until SME_END is found */
    while (e->me_Type != SME_END)
    {
        if ((next_opcode & e->me_OpcodeMask) == e->me_Opcode)
        {
            /* Don't nest. Check only the SME_MASK type */
            if (e->me_Type == SME_MASK)
                mask = e->me_SRMask;
            else if (e->me_Type == SME_FUNC && nest_level < Options.M68K_TRANSLATION_DEPTH)
                mask = e->me_TestFunction(&insn_stream[2], nest_level+1);
            break;
        }
        e++;
    }

    return mask;
}

static uint8_t SR_TestOpcode48B(uint16_t *insn_stream, uint32_t nest_level)
{
    uint16_t next_opcode;
    uint8_t mask = 0;

    /* Get the opcode past current 4-byte instruction */
    next_opcode = BE16(insn_stream[3]);

    /* Fetch correct table baset on bits 12..15 of the opcode */
    struct SRMaskEntry *e = OpcodeMap[next_opcode >> 12];

    /* Search within table until SME_END is found */
    while (e->me_Type != SME_END)
    {
        if ((next_opcode & e->me_OpcodeMask) == e->me_Opcode)
        {
            /* Don't nest. Check only the SME_MASK type */
            if (e->me_Type == SME_MASK)
                mask = e->me_SRMask;
            else if (e->me_Type == SME_FUNC && nest_level < Options.M68K_TRANSLATION_DEPTH)
                mask = e->me_TestFunction(&insn_stream[3], nest_level+1);
            break;
        }
        e++;
    }

    return mask;
}

static uint8_t SR_TestBranch(uint16_t *insn_stream, uint32_t nest_level)
{
    /*
        At this point insn_stream points to the branch opcode.
        Check what's at the target of this branch
    */
    uint8_t mask = 0;
    uint16_t opcode = BE16(*insn_stream);
    int32_t bra_off = 0;

    /* Advance stream 1 word past BRA */
    insn_stream++;

    /* use 16-bit offset */
    if ((opcode & 0x00ff) == 0x00)
    {
        bra_off = (int16_t)(BE16(insn_stream[0]));
    }
    /* use 32-bit offset */
    else if ((opcode & 0x00ff) == 0xff)
    {
        bra_off = (int32_t)(BE32(*(uint32_t*)insn_stream));
    }
    else
    /* otherwise use 8-bit offset */
    {
        bra_off = (int8_t)(opcode & 0xff);
    }

    /* Advance instruction stream accordingly */
    insn_stream = (uint16_t *)((intptr_t)insn_stream + bra_off);

    /* Fetch new opcode and test it */
    uint16_t next_opcode = BE16(*insn_stream);

    /* Fetch correct table baset on bits 12..15 of the opcode */
    struct SRMaskEntry *e = OpcodeMap[next_opcode >> 12];

    /* Search within table until SME_END is found */
    while (e->me_Type != SME_END)
    {
        if ((next_opcode & e->me_OpcodeMask) == e->me_Opcode)
        {
            /* Don't nest. Check only the SME_MASK type */
            if (e->me_Type == SME_MASK)
                mask = e->me_SRMask;
            else if (e->me_Type == SME_FUNC && nest_level < Options.M68K_TRANSLATION_DEPTH)
                mask = e->me_TestFunction(insn_stream, nest_level+1);
            break;
        }
        e++;
    }

    return mask;
}

/* Get the mask of status flags changed by the instruction specified by the opcode */
uint8_t M68K_GetSRMask(uint16_t *insn_stream)
{
    uint16_t opcode = BE16(*insn_stream);
    uint8_t mask = 0;

    /* Fetch correct table baset on bits 12..15 of the opcode */
    struct SRMaskEntry *e = OpcodeMap[opcode >> 12];

    /* Search within table until SME_END is found */
    while (e->me_Type != SME_END)
    {
        if ((opcode & e->me_OpcodeMask) == e->me_Opcode)
        {
            if (e->me_Type == SME_MASK)
                mask = e->me_SRMask;
            else if (e->me_Type == SME_FUNC) {
                mask = e->me_TestFunction(insn_stream, 0);
            }
            break;
        }
        e++;
    }

    return mask;
}

#endif

static int M68K_GetLine0Length(uint16_t *insn_stream)
{
    uint16_t opcode = BE16(*insn_stream);
    int length = 1;
    int need_ea = 0;
    int opsize = 4;

    /* 00000000xxxxxxxx - ORI to CCR, ORI to SR, ORI */
    if ((opcode & 0xff00) == 0x0000 && (opcode & 0x00c0) != 0x00c0)   /* 00000000xxxxxxxx - ORI to CCR, ORI to SR, ORI */
    {
        if (
            (opcode & 0x00ff) == 0x003c ||
            (opcode & 0x00ff) == 0x007c
        ) 
        {
            length = 2;
            need_ea = 0;
        }
        else
        {
            need_ea = 1;
            switch (opcode & 0x00c0)
            {
                case 0x0000:
                    opsize = 1;
                    length = 2;
                    break;
                case 0x0040:
                    opsize = 2;
                    length = 2;
                    break;
                case 0x0080:
                    opsize = 4;
                    length = 3;
                    break;
            }
        }
    }
    /* 00000010xxxxxxxx - ANDI to CCR, ANDI to SR, ANDI */
    else if ((opcode & 0xff00) == 0x0200)   
    {
        if (
            (opcode & 0x00ff) == 0x003c ||
            (opcode & 0x00ff) == 0x007c
        )
        {
            length = 2;
            need_ea = 0;
        }
        else
        {
            need_ea = 1;
            switch (opcode & 0x00c0)
            {
                case 0x0000:
                    opsize = 1;
                    length = 2;
                    break;
                case 0x0040:
                    opsize = 2;
                    length = 2;
                    break;
                case 0x0080:
                    opsize = 4;
                    length = 3;
                    break;
            }
        }   
    }
    /* 00000100xxxxxxxx - SUBI */
    else if ((opcode & 0xff00) == 0x0400)   
    {
        need_ea = 1;
        switch (opcode & 0x00c0)
        {
            case 0x0000:
                opsize = 1;
                length = 2;
                break;
            case 0x0040:
                opsize = 2;
                length = 2;
                break;
            case 0x0080:
                opsize = 4;
                length = 3;
                break;
        }
    }
    /* 00000110xxxxxxxx - ADDI */
    else if ((opcode & 0xff00) == 0x0600 && (opcode & 0x00c0) != 0x00c0)   
    {
        need_ea = 1;
        switch (opcode & 0x00c0)
        {
            case 0x0000:
                opsize = 1;
                length = 2;
                break;
            case 0x0040:
                opsize = 2;
                length = 2;
                break;
            case 0x0080:
                opsize = 4;
                length = 3;
                break;
        }
    }
    /* 00000xx011xxxxxx - CMP2, CHK2 */
    else if ((opcode & 0xf9c0) == 0x00c0)   
    {
        length = 2;
        need_ea = 1;
        opsize = 0;
    }
    /* 00001010xxxxxxxx - EORI to CCR, EORI to SR, EORI */
    else if ((opcode & 0xff00) == 0x0a00)   
    {
        if (
            (opcode & 0x00ff) == 0x003c ||
            (opcode & 0x00ff) == 0x007c
        )
        {
            length = 2;
            need_ea = 0;
        }
        else
        {
            need_ea = 1;
            switch (opcode & 0x00c0)
            {
                case 0x0000:
                    opsize = 1;
                    length = 2;
                    break;
                case 0x0040:
                    opsize = 2;
                    length = 2;
                    break;
                case 0x0080:
                    opsize = 4;
                    length = 3;
                    break;
            }
        }   
    }
    /* 00001100xxxxxxxx - CMPI */
    else if ((opcode & 0xff00) == 0x0c00)   
    {
        need_ea = 1;
        switch (opcode & 0x00c0)
        {
            case 0x0000:
                opsize = 1;
                length = 2;
                break;
            case 0x0040:
                opsize = 2;
                length = 2;
                break;
            case 0x0080:
                opsize = 4;
                length = 3;
                break;
        }
    }
    else if ((opcode & 0xffc0) == 0x0800)   /* 0000100000xxxxxx - BTST */
    {
        length = 2;
        need_ea = 1;
        opsize = 1;
    }
    else if ((opcode & 0xffc0) == 0x0840)   /* 0000100001xxxxxx - BCHG */
    {
        length = 2;
        need_ea = 1;
        opsize = 1;
    }
    else if ((opcode & 0xffc0) == 0x0880)   /* 0000100010xxxxxx - BCLR */
    {
        length = 2;
        need_ea = 1;
        opsize = 1;
    }
    else if ((opcode & 0xffc0) == 0x08c0)   /* 0000100011xxxxxx - BSET */
    {
        length = 2;
        need_ea = 1;
        opsize = 1;
    }
    else if ((opcode & 0xff00) == 0x0e00)   /* 00001110xxxxxxxx - MOVES */
    {
        length = 2;
        need_ea = 1;
        opsize = 0;
    }
    else if ((opcode & 0xf9c0) == 0x08c0)   /* 00001xx011xxxxxx - CAS, CAS2 */
    {
        need_ea = 1;
        opsize = 0;
        if ((opcode & 0x3f) == 0x3c)
        {
            length = 3;
        }
        else
        {
            length = 2;
        }
    }
    else if ((opcode & 0xf1c0) == 0x0100)   /* 0000xxx100xxxxxx - BTST */
    {
        need_ea = 1;
        opsize = 1;
    }
    else if ((opcode & 0xf1c0) == 0x0140)   /* 0000xxx101xxxxxx - BCHG */
    {
        need_ea = 1;
        opsize = 1;
    }
    else if ((opcode & 0xf1c0) == 0x0180)   /* 0000xxx110xxxxxx - BCLR */
    {
        need_ea = 1;
        opsize = 1;
    }
    else if ((opcode & 0xf1c0) == 0x01c0)   /* 0000xxx111xxxxxx - BSET */
    {
        need_ea = 1;
        opsize = 1;
    }
    else if ((opcode & 0xf038) == 0x0008)   /* 0000xxxxxx001xxx - MOVEP */
    {
        need_ea = 0;
        length = 2;
    }

    if (need_ea)
    {
        length += SR_GetEALength(&insn_stream[length], opcode & 0x3f, opsize);
    }

    return length;
}

int M68K_GetLineELength(uint16_t *insn_stream)
{
    uint16_t opcode = BE16(*insn_stream);
    int length = 1;
    int need_ea = 0;

    if (
        (opcode & 0xf8c0) == 0xe0c0     // memory shift/rotate
    )
    {
        length = 1;
        need_ea = 1;
    }
    else if (
        (opcode & 0xf8c0) == 0xe8c0     // bf* instructions
    )
    {
        length = 2;
        need_ea = 1;
    }
    else
    {
        length = 1;
        need_ea = 0;
    }

    if (need_ea)
    {
        length += SR_GetEALength(&insn_stream[length], opcode & 0x3f, 0);
    }

    return length;
}

int M68K_GetLine6Length(uint16_t *insn_stream)
{
    uint16_t opcode = BE16(*insn_stream);
    int length = 1;
    
    if ((opcode & 0xff) == 0) {
        length = 2;
    }
    else if ((opcode & 0xff) == 0xff) {
        length = 3;
    }

    return length;
}

int M68K_GetLine8Length(uint16_t *insn_stream)
{
    uint16_t opcode = BE16(*insn_stream);
    int length = 1;
    int need_ea = 1;
    int opsize = 2;

    if (
        (opcode & 0xf0c0) == 0x80c0     // div word size
    )
    {
        length = 1;
        opsize = 2;
        need_ea = 1;
    }
    else if (
        (opcode & 0xf1f0) == 0x8100     // sbcd
    )
    {
        length = 1;
        need_ea = 0;
    }
    else if (
        (opcode & 0xf1f0) == 0x8140 ||  // pack
        (opcode & 0xf1f0) == 0x8180     // unpk
    )
    {
        length = 2;
        need_ea = 0;
    }
    else {
        need_ea = 1;
        opsize = 1 << ((opcode >> 6) & 3);
    }

    if (need_ea) {
        length += SR_GetEALength(&insn_stream[length], opcode & 0x3f, opsize);
    }

    return length;
}

int M68K_GetLine9Length(uint16_t *insn_stream)
{
    uint16_t opcode = BE16(*insn_stream);
    int length = 1;
    int need_ea = 1;
    int opsize = 2;

    /* SUBA */
    if ((opcode & 0xf0c0) == 0x90c0)
    {
        opsize = (opcode & 0x0100) == 0x0100 ? 4 : 2;
    }
    /* SUBX */
    else if ((opcode & 0xf130) == 0x9100)
    {
        need_ea = 0;
    }
    /* SUB */
    else
    {
        opsize = 1 << ((opcode >> 6) & 3);
    }

    if (need_ea) {
        length += SR_GetEALength(&insn_stream[length], opcode & 0x3f, opsize);
    }

    return length;
}

int M68K_GetLineBLength(uint16_t *insn_stream)
{
    uint16_t opcode = BE16(*insn_stream);
    int length = 1;
    int need_ea = 1;
    int opsize = 2;

    /* 1011xxxx11xxxxxx - CMPA */
    if ((opcode & 0xf0c0) == 0xb0c0)
    {
        opsize = ((opcode >> 8) & 1) ? 4 : 2;
    }
    /* 1011xxx1xx001xxx - CMPM */
    else if ((opcode & 0xf138) == 0xb108)
    {
        need_ea = 0;
    }
    /* 1011xxx0xxxxxxxx - CMP */
    else if ((opcode & 0xf100) == 0xb000)
    {
        opsize = 1 << ((opcode >> 6) & 3);
    }
    /* 1011xxxxxxxxxxxx - EOR */
    else if ((opcode & 0xf000) == 0xb000)
    {
        opsize = 1 << ((opcode >> 6) & 3);
    }

    if (need_ea) {
        length += SR_GetEALength(&insn_stream[length], opcode & 0x3f, opsize);
    }

    return length;
}

int M68K_GetLineCLength(uint16_t *insn_stream)
{
    uint16_t opcode = BE16(*insn_stream);
    int length = 1;
    int need_ea = 1;
    int opsize = 2;

    /* 1100xxx011xxxxxx - MULU */
    if ((opcode & 0xf1c0) == 0xc0c0)
    {
        opsize = 2;
        need_ea = 1;
    }
    /* 1100xxx10000xxxx - ABCD */
    else if ((opcode & 0xf1f0) == 0xc100)
    {
        need_ea = 0;
    }
    /* 1100xxx111xxxxxx - MULS */
    else if ((opcode & 0xf1c0) == 0xc1c0)
    {
        opsize = 2;
        need_ea = 1;
    }
    /* 1100xxx1xx00xxxx - EXG */
    else if ((opcode & 0xf130) == 0xc100)
    {
        need_ea = 0;
    }
    /* 1100xxxxxxxxxxxx - AND */
    else if ((opcode & 0xf000) == 0xc000)
    {
        opsize = 1 << ((opcode >> 6) & 3);
    }

    if (need_ea) {
        length += SR_GetEALength(&insn_stream[length], opcode & 0x3f, opsize);
    }

    return length;
}

int M68K_GetLineDLength(uint16_t *insn_stream)
{
    uint16_t opcode = BE16(*insn_stream);
    int length = 1;
    int need_ea = 1;
    int opsize = 2;

    /* ADDA */
    if ((opcode & 0xf0c0) == 0xd0c0)
    {
        opsize = (opcode & 0x0100) == 0x0100 ? 4 : 2;
    }
    /* ADDX */
    else if ((opcode & 0xf130) == 0xd100)
    {
        need_ea = 0;
    }
    /* ADD */
    else
    {
        opsize = 1 << ((opcode >> 6) & 3);
    }

    if (need_ea) {
        length += SR_GetEALength(&insn_stream[length], opcode & 0x3f, opsize);
    }

    return length;
}

int M68K_GetLine5Length(uint16_t *insn_stream)
{
    uint16_t opcode = BE16(*insn_stream);
    int length = 1;
    int need_ea = 1;
    int opsize = 2;

    /* Scc/TRAPcc/DBcc */
    if ((opcode & 0xf0c0) == 0x50c0)
    {
        /* DBcc */
        if ((opcode & 0x38) == 0x08)
        {
            length = 2;
            need_ea = 0;
        }
        /* TRAPcc */
        else if ((opcode & 0x38) == 0x38)
        {
            need_ea = 0;
            switch (opcode & 7)
            {
                case 4:
                    length = 1;
                    break;
                case 2:
                    length = 2;
                    break;
                case 3:
                    length = 3;
                    break;
            }
        }
        /* Scc */
        else
        {
            need_ea = 1;
            opsize = 1;
        }   
    }
    /* SUBQ */
    else if ((opcode & 0xf100) == 0x5100)
    {
        need_ea = 1;
        switch ((opcode >> 6) & 3)
        {
            case 0:
                opsize = 1;
                break;
            case 1:
                opsize = 2;
                break;
            case 2:
                opsize = 4;
                break;
        }
    }
    /* ADDQ */
    else if ((opcode & 0xf100) == 0x5000)
    {
        need_ea = 1;
        switch ((opcode >> 6) & 3)
        {
            case 0:
                opsize = 1;
                break;
            case 1:
                opsize = 2;
                break;
            case 2:
                opsize = 4;
                break;
        }
    }  

    if (need_ea) {
        length += SR_GetEALength(&insn_stream[length], opcode & 0x3f, opsize);
    }

    return length;
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

static SR_Check SRCheck[] = {
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
    NULL,
    GetSR_LineB,
    GetSR_LineC,
    GetSR_LineD,
    GetSR_LineE,
    NULL,
};

/* Get the mask of status flags changed by the instruction specified by the opcode */
uint8_t M68K_GetSRMask(uint16_t *insn_stream)
{
    uint16_t opcode = BE16(*insn_stream);
    int scan_depth = 0;
    const int max_scan_depth = 20;
    uint8_t mask = 0;
    uint8_t needed = 0;
    int found = 0;

    D(kprintf("[JIT] GetSRMask, opcode %04x @ %08x, ", opcode, insn_stream));

    if (SRCheck[opcode >> 12] != NULL) {
        uint32_t flags = SRCheck[opcode >> 12](opcode);
        mask = flags & 0xffff;
        needed = flags >> 16;
        found = 1;
    }
    else {
        found = 0;
        mask = 0;
        needed = SR_CCR;
    }

    if (!found) {
        D(kprintf("opcode not found!\n"));
        return 0;
    }

return mask;

    D(kprintf(" SRNeeds = %x, SRSets = %x\n", needed, mask));

    /*
        Check as long as there are still some flags to be set by the opcode and the depth
        of scan is not exceeded
    */
    while(mask && scan_depth < max_scan_depth)
    {
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
                uint8_t condition = (opcode >> 9) & 7;
                // List of masks which the condition code needs by itself
                const uint8_t masks[] = {
                    0,                  // T, F
                    SR_C | SR_Z,        // HI, LS
                    SR_C,               // CC, CS
                    SR_Z,               // NE, EQ
                    SR_V,               // VC, VS
                    SR_N,               // MI, PL
                    SR_N | SR_V,        // GE, LT
                    SR_N | SR_V | SR_Z  // GT, LE
                };

                // Mark the flags which conditional jump needs by itself
                needed |= mask & masks[condition];

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
                scan_depth = max_scan_depth - 1 - (max_scan_depth - scan_depth) / 2;
                int scan_depth_tmp = scan_depth;

                while(mask1 && scan_depth < max_scan_depth)
                {
                    scan_depth++;
                    uint16_t sets;
                    uint16_t needs;

                    /* If instruction is a branch break the scan */
                    if (M68K_IsBranch(insn_stream))
                        break;

                    /* Get opcode */
                    opcode = BE16(*insn_stream);

                    D(kprintf("[JIT]   %02d.1: opcode=%04x @ %08x ", scan_depth, opcode, insn_stream));

                    if (SRCheck[opcode >> 12] != NULL) {
                        uint32_t flags = SRCheck[opcode >> 12](opcode);
                        sets = flags;
                        needs = flags >> 16;
                        found = 1;
                    }
                    else {
                        sets = 0;
                        needs = SR_CCR;
                        found = 0;
                    }

                    if (mask1 & needs) {
                        needed1 |= (mask1 & needs);
                    }

                    /* Clear flags which this instruction sets */
                    mask1 = mask1 & ~sets;

                    if (!found)
                    {
                        D(kprintf("opcode not found!\n"));
                        break;
                    }

                    /* Advance to subsequent instruction */
                    insn_stream += M68K_GetINSNLength(insn_stream);
                }

                scan_depth = scan_depth_tmp;

                while(mask2 && scan_depth < max_scan_depth)
                {
                    scan_depth++;
                    uint16_t sets;
                    uint16_t needs;

                    /* If instruction is a branch break the scan */
                    if (M68K_IsBranch(insn_stream_2))
                        break;

                    /* Get opcode */
                    opcode = BE16(*insn_stream_2);

                    D(kprintf("[JIT]   %02d.2: opcode=%04x @ %08x ", scan_depth, opcode, insn_stream_2));

                    if (SRCheck[opcode >> 12] != NULL) {
                        uint32_t flags = SRCheck[opcode >> 12](opcode);
                        sets = flags;
                        needs = flags >> 16;
                        found = 1;
                    }
                    else {
                        sets = 0;
                        needs = SR_CCR;
                        found = 0;
                    }

                    if (mask2 & needs) {
                        needed2 |= (mask2 & needs);
                    }

                    /* Clear flags which this instruction sets */
                    mask2 = mask2 & ~sets;

                    if (!found)
                    {
                        D(kprintf("opcode not found!\n"));
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

        uint16_t sets;
        uint16_t needs;

        if (SRCheck[opcode >> 12] != NULL) {
            uint32_t flags = SRCheck[opcode >> 12](opcode);
            sets = flags;
            needs = flags >> 16;
            found = 1;
        }
        else {
            sets = 0;
            needs = SR_CCR;
            found = 0;
        }

        if (mask & needs) {
            needed |= (mask & needs);
        }

        /* Clear flags which this instruction sets */
        mask = mask & ~sets;

        if (!found)
        {
            D(kprintf("opcode not found!\n"));
            break;
        }
    }

    return mask | needed;
}
