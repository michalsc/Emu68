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

struct SRMaskEntry {
    uint16_t me_OpcodeMask;
    uint16_t me_Opcode;
    uint8_t  me_Type;
    uint8_t  me_SRMask;
    uint8_t  (*me_TestFunction)(uint16_t *stream, int nest_level);
};

#define SME_MASK    1
#define SME_FUNC    2
#define SME_END     255

static uint8_t SR_TestBranch(uint16_t *insn_stream, int nest_level);
static uint8_t SR_TestOpcode16B(uint16_t *insn_stream, int nest_level);
static uint8_t SR_TestOpcode32B(uint16_t *insn_stream, int nest_level);
static uint8_t SR_TestOpcode48B(uint16_t *insn_stream, int nest_level);
static uint8_t SR_TestOpcodeEA(uint16_t *insn_stream, int nest_level);
static uint8_t SR_TestOpcodeMOVEA(uint16_t *insn_stream, int nest_level);
static uint8_t SR_TestOpcodeADDA(uint16_t *insn_stream, int nest_level);

static struct SRMaskEntry Line0_Map[] = {
    { 0xffbf, 0x003c, SME_MASK, 0, NULL },                                /* ORI to CCR/SR - they rely on current CC! */
    { 0xff00, 0x0000, SME_MASK, SR_C | SR_Z | SR_N | SR_V, NULL },        /* ORI */
    { 0xffbf, 0x023c, SME_MASK, 0, NULL },                                /* ANDI to CCR/SR - they rely on current CC! */
    { 0xf9c0, 0x00c0, SME_MASK, SR_C | SR_Z | SR_N | SR_V, NULL },        /* CHK2/CMP2 */
    { 0xff00, 0x0200, SME_MASK, SR_C | SR_Z | SR_N | SR_V, NULL },        /* ANDI */
    { 0xff00, 0x0400, SME_MASK, SR_X | SR_C | SR_Z | SR_N | SR_V, NULL }, /* SUBI */
    { 0xffc0, 0x06c0, SME_MASK, 0, NULL },                                /* RTM/CALLM */
    { 0xff00, 0x0600, SME_MASK, SR_X | SR_C | SR_Z | SR_N | SR_V, NULL }, /* ADDI */
    { 0xffbf, 0x0a3c, SME_MASK, 0, NULL },                                /* EORI to CCR/SR - they rely on current CC! */
    { 0xff00, 0x0a00, SME_MASK, SR_C | SR_Z | SR_N | SR_V, NULL },        /* EORI */
    { 0xff00, 0x0c00, SME_MASK, SR_C | SR_Z | SR_N | SR_V, NULL },        /* CMPI */
    { 0xff00, 0x0800, SME_MASK, SR_Z, NULL },                             /* BTST/BSET/BCLR/BCHG */
    { 0xf9c0, 0x08c0, SME_MASK, SR_C | SR_Z | SR_N | SR_V, NULL },        /* CAS/CAS2 */
    { 0xf100, 0x0100, SME_MASK, SR_Z, NULL },                             /* BTST/BSET/BCLR/BCHG */
    { 0x0000, 0x0000, SME_END,  0, NULL }
};

static struct SRMaskEntry Line1_Map[] = {
    { 0xc1c0, 0x0040, SME_FUNC, 0, SR_TestOpcodeMOVEA },                  /* MOVEA case - destination is An */
    { 0xc000, 0x0000, SME_MASK, SR_C | SR_V | SR_Z | SR_N, NULL },        /* All other moves change CC */
    { 0x0000, 0x0000, SME_END,  0, NULL }
};

static struct SRMaskEntry Line2_Map[sizeof(Line1_Map)/sizeof(struct SRMaskEntry)] __attribute__((alias("Line1_Map")));
static struct SRMaskEntry Line3_Map[sizeof(Line1_Map)/sizeof(struct SRMaskEntry)] __attribute__((alias("Line1_Map")));

static struct SRMaskEntry Line4_Map[] = {
    { 0xfdc0, 0x40c0, SME_MASK, 0, NULL },                                /* MOVE from CCR/SR */
    { 0xff00, 0x4000, SME_MASK, SR_X | SR_C | SR_Z | SR_N | SR_V, NULL }, /* NEGX */
    { 0xff00, 0x4200, SME_MASK, SR_C | SR_Z | SR_N | SR_V, NULL },        /* CLR */
    { 0xffc0, 0x44c0, SME_MASK, SR_X | SR_C | SR_Z | SR_N | SR_V, NULL }, /* MOVE to CCR */
    { 0xff00, 0x4400, SME_MASK, SR_X | SR_C | SR_Z | SR_N | SR_V, NULL }, /* NEG */
    { 0xffc0, 0x46c0, SME_MASK, SR_X | SR_C | SR_Z | SR_N | SR_V, NULL }, /* MOVE to SR */
    { 0xff00, 0x4600, SME_MASK, SR_C | SR_Z | SR_N | SR_V, NULL },        /* NOT */
    { 0xfeb8, 0x4880, SME_MASK, SR_C | SR_Z | SR_N | SR_V, NULL },        /* EXT/EXTB */
    { 0xfff8, 0x4808, SME_FUNC, 0, SR_TestOpcode48B },                    /* LINK */
    { 0xffc0, 0x4800, SME_MASK, SR_X | SR_C | SR_Z | SR_N | SR_V, NULL }, /* NBCD */
    { 0xfff8, 0x4840, SME_MASK, SR_C | SR_Z | SR_N | SR_V, NULL },        /* SWAP */
    { 0xfff8, 0x4848, SME_MASK, 0, NULL },                                /* BKPT */
    { 0xffc0, 0x4840, SME_FUNC, 0, SR_TestOpcodeEA },                     /* PEA */
    { 0xffff, 0x4afc, SME_MASK, 0, NULL },                                /* ILLEGAL */
    { 0xff00, 0x4a00, SME_MASK, SR_C | SR_Z | SR_N | SR_V, NULL },        /* TAS/TST */
    { 0xff80, 0x4c00, SME_MASK, SR_C | SR_Z | SR_N | SR_V, NULL },        /* MULU/MULS/DIVU/DIVS */
    { 0xfff0, 0x4e40, SME_MASK, 0, NULL },                                /* TRAP */
    { 0xfff8, 0x4e50, SME_FUNC, 0, SR_TestOpcode32B },                    /* LINK */
    { 0xfff8, 0x4e58, SME_FUNC, 0, SR_TestOpcode16B },                    /* UNLK */
    { 0xfff0, 0x4e60, SME_MASK, 0, NULL },                                /* MOVE USP */
    { 0xffff, 0x4e70, SME_MASK, 0, NULL },                                /* RESET */
    { 0xffff, 0x4e71, SME_FUNC, 0, SR_TestOpcode16B },                    /* NOP */
    { 0xffff, 0x4e72, SME_MASK, SR_X | SR_C | SR_Z | SR_N | SR_V, NULL }, /* STOP */
    { 0xffff, 0x4e73, SME_MASK, SR_X | SR_C | SR_Z | SR_N | SR_V, NULL }, /* RTE */
    { 0xffff, 0x4e74, SME_MASK, 0, NULL },                                /* RTD */
    { 0xffff, 0x4e75, SME_MASK, 0, NULL },                                /* RTS */
    { 0xffff, 0x4e76, SME_MASK, 0, NULL },                                /* TRAPV */
    { 0xffff, 0x4e77, SME_MASK, SR_X | SR_C | SR_Z | SR_N | SR_V, NULL }, /* RTR */
    { 0xfffe, 0x4e7a, SME_MASK, 0, NULL },                                /* MOVEC */
    { 0xffc0, 0x4e80, SME_MASK, 0, NULL },                                /* JSR */
    { 0xffc0, 0x4ec0, SME_MASK, 0, NULL },                                /* JMP */
    { 0xfb80, 0x4880, SME_FUNC, 0, SR_TestOpcode32B },                    /* MOVEM */
    { 0xf1c0, 0x41c0, SME_FUNC, 0, SR_TestOpcodeEA },                     /* LEA */
    { 0xf140, 0x4100, SME_MASK, 0, NULL },                                /* CHK */
    { 0x0000, 0x0000, SME_END,  0, NULL }
};

static struct SRMaskEntry Line5_Map[] = {
    { 0xf0c0, 0x50c0, SME_MASK, 0, NULL },                                /* TRAP/DBcc/Scc */
    { 0xf038, 0x5008, SME_FUNC, 0, SR_TestOpcode16B },                    /* SUBQ/ADDQ with An */
    { 0xf000, 0x5000, SME_MASK, SR_X | SR_C | SR_V | SR_Z | SR_N, NULL }, /* SUBQ/ADDQ */
    { 0x0000, 0x0000, SME_END,  0, NULL }
};

static struct SRMaskEntry Line6_Map[] = {
    { 0xfe00, 0x6000, SME_FUNC,  0, SR_TestBranch },                      /* BRA/BSR */
    { 0xf000, 0x6000, SME_MASK,  0, NULL },                               /* Bcc */
    { 0x0000, 0x0000, SME_END,  0, NULL }
};

static struct SRMaskEntry Line7_Map[] = {
    { 0xf000, 0x7000, SME_MASK, SR_C | SR_V | SR_Z | SR_N, NULL },        /* MOVEQ */
    { 0x0000, 0x0000, SME_END,  0, NULL }
};

static struct SRMaskEntry Line8_Map[] = {
    { 0xf1c0, 0x80c0, SME_MASK, SR_C | SR_V | SR_Z | SR_N, NULL },        /* DIVU */
    { 0xf1f0, 0x8100, SME_MASK, SR_X | SR_C | SR_V | SR_Z | SR_N, NULL }, /* SBCD */
    { 0xf1f0, 0x8140, SME_MASK, 0, NULL },                                /* PACK */
    { 0xf1f0, 0x8180, SME_MASK, 0, NULL },                                /* UNPK */
    { 0xf1c0, 0x81c0, SME_MASK, SR_C | SR_V | SR_Z | SR_N, NULL },        /* DIVS */
    { 0xf000, 0x8000, SME_MASK, SR_C | SR_V | SR_Z | SR_N, NULL },        /* OR */
    { 0x0000, 0x0000, SME_END,  0, NULL }
};

static struct SRMaskEntry Line9_Map[] = {
    { 0xf0c0, 0x90c0, SME_FUNC, 0, SR_TestOpcodeADDA },                   /* SUBA */
    { 0xf130, 0x9100, SME_MASK, SR_C | SR_V | SR_Z | SR_N, NULL },        /* SUBX */
    { 0xf000, 0x9000, SME_MASK, SR_X | SR_C | SR_V | SR_Z | SR_N, NULL }, /* SUB */
    { 0x0000, 0x0000, SME_END,  0, NULL }
};

static struct SRMaskEntry LineA_Map[] = {
    { 0x0000, 0x0000, SME_END,  0, NULL }
};

static struct SRMaskEntry LineB_Map[] = {
    { 0xf000, 0xb000, SME_MASK, SR_C | SR_V | SR_Z | SR_N, NULL },        /* CMP/CMPM/CMPA/EOR */
    { 0x0000, 0x0000, SME_END,  0, NULL }
};

static struct SRMaskEntry LineC_Map[] = {
    { 0xf1c0, 0xc0c0, SME_MASK, SR_C | SR_V | SR_Z | SR_N, NULL },        /* MULU */
    { 0xf1f0, 0xc100, SME_MASK, SR_X | SR_C | SR_V | SR_Z | SR_N, NULL }, /* ABCD */
    { 0xf1c0, 0xc1c0, SME_MASK, SR_C | SR_V | SR_Z | SR_N, NULL },        /* MULS */
    { 0xf1f0, 0xc140, SME_FUNC, 0, SR_TestOpcode16B },                    /* EXG Dx,Dy / EXG Ax,Ay */
    { 0xf1f0, 0xc180, SME_FUNC, 0, SR_TestOpcode16B },                    /* EXG Dx,Ay */
    { 0xf000, 0xc000, SME_MASK, SR_C | SR_V | SR_Z | SR_N, NULL },        /* AND */
    { 0x0000, 0x0000, SME_END,  0, NULL }
};

static struct SRMaskEntry LineD_Map[] = {
    { 0xf0c0, 0xd0c0, SME_FUNC, 0, SR_TestOpcodeADDA },                   /* ADDA */
    { 0xf130, 0xd100, SME_MASK, SR_C | SR_V | SR_Z | SR_N, NULL },        /* ADDX - reqires X and modifies X! */
    { 0xf000, 0xd000, SME_MASK, SR_X | SR_C | SR_V | SR_Z | SR_N, NULL }, /* ADD */
    { 0x0000, 0x0000, SME_END,  0, NULL }
};

static struct SRMaskEntry LineE_Map[] = {
    { 0xfec0, 0xe0c0, SME_MASK, SR_X | SR_C | SR_V | SR_Z | SR_N, NULL }, /* ASL/ASR */
    { 0xfec0, 0xe2c0, SME_MASK, SR_X | SR_C | SR_V | SR_Z | SR_N, NULL }, /* LSL/LSR */
    { 0xfec0, 0xe4c0, SME_MASK, SR_C | SR_V | SR_Z | SR_N, NULL },        /* ROXL/ROXR */
    { 0xfec0, 0xe6c0, SME_MASK, SR_C | SR_V | SR_Z | SR_N, NULL },        /* ROL/ROR */
    { 0xffc0, 0xe8c0, SME_MASK, SR_C | SR_V | SR_Z | SR_N, NULL },        /* BFTST */
    { 0xffc0, 0xe9c0, SME_MASK, SR_C | SR_V | SR_Z | SR_N, NULL },        /* BFEXTU */
    { 0xffc0, 0xeac0, SME_MASK, SR_C | SR_V | SR_Z | SR_N, NULL },        /* BFCHG */
    { 0xffc0, 0xebc0, SME_MASK, SR_C | SR_V | SR_Z | SR_N, NULL },        /* BFEXTS */
    { 0xffc0, 0xecc0, SME_MASK, SR_C | SR_V | SR_Z | SR_N, NULL },        /* BFCLR */
    { 0xffc0, 0xedc0, SME_MASK, SR_C | SR_V | SR_Z | SR_N, NULL },        /* BFFFO */
    { 0xffc0, 0xeec0, SME_MASK, SR_C | SR_V | SR_Z | SR_N, NULL },        /* BFSET */
    { 0xffc0, 0xefc0, SME_MASK, SR_C | SR_V | SR_Z | SR_N, NULL },        /* BFINS */
    { 0xf018, 0xe000, SME_MASK, SR_X | SR_C | SR_V | SR_Z | SR_N, NULL }, /* ASL/ASR */
    { 0xf018, 0xe008, SME_MASK, SR_X | SR_C | SR_V | SR_Z | SR_N, NULL }, /* LSL/LSR */
    { 0xf018, 0xe010, SME_MASK, SR_C | SR_V | SR_Z | SR_N, NULL },        /* ROXL/ROXR */
    { 0xf018, 0xe018, SME_MASK, SR_C | SR_V | SR_Z | SR_N, NULL },        /* ROL/ROR */
    { 0x0000, 0x0000, SME_END,  0, NULL }
};

static struct SRMaskEntry LineF_Map[] = {
    { 0x0000, 0x0000, SME_END, 0, NULL }
};

static struct SRMaskEntry *OpcodeMap[16] = {
    Line0_Map,
    Line1_Map,
    Line2_Map,
    Line3_Map,
    Line4_Map,
    Line5_Map,
    Line6_Map,
    Line7_Map,
    Line8_Map,
    Line9_Map,
    LineA_Map,
    LineB_Map,
    LineC_Map,
    LineD_Map,
    LineE_Map,
    LineF_Map
};

static uint8_t SR_GetEALength(uint16_t *insn_stream, uint8_t imm_size)
{
    uint16_t opcode;
    uint8_t word_count = 1;
    uint8_t mode, reg;

    opcode = BE16(insn_stream[0]);
    mode = (opcode >> 3) & 7;
    reg = (opcode) & 7;

    /* modes 0, 1, 2, 3 and 4 do not have extra words */
    if (mode > 4)
    {
        if (mode == 5)      /* 16-bit offset in next opcode */
            word_count++;
        else if (mode == 6 || (mode == 7 && reg == 3))
        {
            /* Reg- or PC-relative addressing mode */
            uint16_t brief = BE16(insn_stream[1]);

            /* Brief word is here */
            word_count++;

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

static uint8_t SR_TestOpcodeEA(uint16_t *insn_stream, int nest_level)
{
    uint16_t next_opcode;
    uint8_t mask = 0;
    uint8_t word_count;

    /* First calculate the EA length */
    word_count = SR_GetEALength(insn_stream, 0);

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

static uint8_t SR_TestOpcodeMOVEA(uint16_t *insn_stream, int nest_level)
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
            word_count = SR_GetEALength(insn_stream, 2);
            break;
        case 0x2000:
            word_count = SR_GetEALength(insn_stream, 4);
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

static uint8_t SR_TestOpcodeADDA(uint16_t *insn_stream, int nest_level)
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
            word_count = SR_GetEALength(insn_stream, 2);
            break;
        case 0x01c0:
            word_count = SR_GetEALength(insn_stream, 4);
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

static uint8_t SR_TestOpcode16B(uint16_t *insn_stream, int nest_level)
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

static uint8_t SR_TestOpcode32B(uint16_t *insn_stream, int nest_level)
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

static uint8_t SR_TestOpcode48B(uint16_t *insn_stream, int nest_level)
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

static uint8_t SR_TestBranch(uint16_t *insn_stream, int nest_level)
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
