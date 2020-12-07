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
    uint8_t  me_BaseLength;
    uint8_t  me_HasEA;
    uint8_t  me_Type;
    uint8_t  me_SRNeeds;
    uint8_t  me_SRSets;
    uint8_t  (*me_TestFunction)(uint16_t *stream, uint32_t nest_level);
};

#define SME_MASK    1
#define SME_FUNC    2
#define SME_END     255

static uint8_t SR_TestBranch(uint16_t *insn_stream, uint32_t nest_level);
static uint8_t SR_TestOpcode16B(uint16_t *insn_stream, uint32_t nest_level);
static uint8_t SR_TestOpcode32B(uint16_t *insn_stream, uint32_t nest_level);
static uint8_t SR_TestOpcode48B(uint16_t *insn_stream, uint32_t nest_level);
static uint8_t SR_TestOpcodeEA(uint16_t *insn_stream, uint32_t nest_level);
static uint8_t SR_TestOpcodeMOVEA(uint16_t *insn_stream, uint32_t nest_level);
static uint8_t SR_TestOpcodeADDA(uint16_t *insn_stream, uint32_t nest_level);

static struct SRMaskEntry Line0_Map[] = {
    { 0xffbf, 0x003c, 2, 0, SME_MASK, SR_C | SR_Z | SR_N | SR_V | SR_X, SR_C | SR_Z | SR_N | SR_V | SR_X , NULL }, /* ORI to CCR/SR - needs all falgs, sets all flags */
    { 0xff80, 0x0000, 2, 1, SME_MASK, 0, SR_C | SR_Z | SR_N | SR_V, NULL },                /* ORI.B / ORI.W */
    { 0xffc0, 0x0080, 3, 1, SME_MASK, 0, SR_C | SR_Z | SR_N | SR_V, NULL },                /* ORI.L */
    { 0xffbf, 0x023c, 2, 0, SME_MASK, SR_C | SR_Z | SR_N | SR_V | SR_X, SR_C | SR_Z | SR_N | SR_V | SR_X , NULL }, /* ANDI to CCR/SR - needs all falgs, sets all flags */
    { 0xf9c0, 0x00c0, 2, 1, SME_MASK, 0, SR_C | SR_Z | SR_N | SR_V, NULL },        /* CHK2/CMP2 */
    { 0xff80, 0x0200, 2, 1, SME_MASK, 0, SR_C | SR_Z | SR_N | SR_V, NULL },        /* ANDI.B / ANDI.W */
    { 0xffc0, 0x0280, 3, 1, SME_MASK, 0, SR_C | SR_Z | SR_N | SR_V, NULL },        /* ANDI.L */
    { 0xff80, 0x0400, 2, 1, SME_MASK, 0, SR_X | SR_C | SR_Z | SR_N | SR_V, NULL }, /* SUBI.B / SUBI.W */
    { 0xffc0, 0x0480, 3, 1, SME_MASK, 0, SR_X | SR_C | SR_Z | SR_N | SR_V, NULL }, /* SUBI.L */
    { 0xffc0, 0x06c0, 1, 0, SME_MASK, 0, 0, NULL },                                /* RTM/CALLM */
    { 0xff80, 0x0600, 2, 1, SME_MASK, 0, SR_X | SR_C | SR_Z | SR_N | SR_V, NULL }, /* ADDI.B / ADDI.W */
    { 0xffc0, 0x0680, 3, 1, SME_MASK, 0, SR_X | SR_C | SR_Z | SR_N | SR_V, NULL }, /* ADDI.L */
    { 0xffbf, 0x0a3c, 2, 0, SME_MASK, SR_C | SR_Z | SR_N | SR_V | SR_X, SR_C | SR_Z | SR_N | SR_V | SR_X, NULL }, /* EORI to CCR/SR - they rely on current CC! */
    { 0xff80, 0x0a00, 2, 1, SME_MASK, 0, SR_C | SR_Z | SR_N | SR_V, NULL },        /* EORI.B / EORI.W */
    { 0xffc0, 0x0a80, 3, 1, SME_MASK, 0, SR_C | SR_Z | SR_N | SR_V, NULL },        /* EORI.L */
    { 0xff80, 0x0c00, 2, 1, SME_MASK, 0, SR_C | SR_Z | SR_N | SR_V, NULL },        /* CMPI.B / CMPI.W */
    { 0xffc0, 0x0c80, 3, 1, SME_MASK, 0, SR_C | SR_Z | SR_N | SR_V, NULL },        /* CMPI.L */
    { 0xff00, 0x0800, 1, 1, SME_MASK, 0, SR_Z, NULL },                    /* BTST/BSET/BCLR/BCHG - reg */
    { 0xf9ff, 0x08fc, 3, 0, SME_MASK, 0, SR_C | SR_Z | SR_N | SR_V, NULL },        /* CAS2 */
    { 0xf9c0, 0x08c0, 2, 1, SME_MASK, 0, SR_C | SR_Z | SR_N | SR_V, NULL },        /* CAS */
    { 0xf100, 0x0100, 2, 1, SME_MASK, 0, SR_Z, 0, SR_Z, NULL },                    /* BTST/BSET/BCLR/BCHG - imm */
    { 0x0000, 0x0000, 0, 0, SME_END,  0, 0, NULL }
};

static struct SRMaskEntry Line1_Map[] = {
    { 0xc1c0, 0x0040, 1, 2, SME_FUNC, 0, 0, SR_TestOpcodeMOVEA },                  /* MOVEA case - destination is An */
    { 0xc000, 0x0000, 1, 2, SME_MASK, 0, SR_C | SR_V | SR_Z | SR_N, NULL },        /* All other moves change CC */
    { 0x0000, 0x0000, 0, 0, SME_END,  0, 0, NULL }
};

static struct SRMaskEntry Line2_Map[sizeof(Line1_Map)/sizeof(struct SRMaskEntry)] __attribute__((alias("Line1_Map")));
static struct SRMaskEntry Line3_Map[sizeof(Line1_Map)/sizeof(struct SRMaskEntry)] __attribute__((alias("Line1_Map")));

static struct SRMaskEntry Line4_Map[] = {
    { 0xfdc0, 0x40c0, 1, 1, SME_MASK, SR_X | SR_C | SR_Z | SR_N | SR_V, 0, NULL }, /* MOVE from CCR/SR */
    { 0xff00, 0x4000, 1, 1, SME_MASK, SR_X, SR_X | SR_C | SR_Z | SR_N | SR_V, NULL }, /* NEGX */
    { 0xff00, 0x4200, 1, 1, SME_MASK, 0, SR_C | SR_Z | SR_N | SR_V, NULL },        /* CLR */
    { 0xffc0, 0x44c0, 1, 1, SME_MASK, 0, SR_X | SR_C | SR_Z | SR_N | SR_V, 0, NULL }, /* MOVE to CCR */
    { 0xff00, 0x4400, 1, 1, SME_MASK, 0, SR_X | SR_C | SR_Z | SR_N | SR_V, NULL }, /* NEG */
    { 0xffc0, 0x46c0, 1, 1, SME_MASK, 0, SR_X | SR_C | SR_Z | SR_N | SR_V, NULL }, /* MOVE to SR */
    { 0xff00, 0x4600, 1, 1, SME_MASK, 0, SR_C | SR_Z | SR_N | SR_V, NULL },        /* NOT */
    { 0xfeb8, 0x4880, 1, 0, SME_MASK, 0, SR_C | SR_Z | SR_N | SR_V, NULL },        /* EXT/EXTB */
    { 0xfff8, 0x4808, 3, 0, SME_FUNC, 0, 0, SR_TestOpcode48B },                    /* LINK */
    { 0xffc0, 0x4800, 1, 1, SME_MASK, SR_X, SR_X | SR_C | SR_Z | SR_N | SR_V, NULL }, /* NBCD */
    { 0xfff8, 0x4840, 1, 0, SME_MASK, 0, SR_C | SR_Z | SR_N | SR_V, NULL },        /* SWAP */
    { 0xfff8, 0x4848, 1, 0, SME_MASK, SR_X | SR_C | SR_Z | SR_N | SR_V, 0, NULL }, /* BKPT */
    { 0xffc0, 0x4840, 1, 1, SME_FUNC, 0, 0, SR_TestOpcodeEA },                     /* PEA */
    { 0xffff, 0x4afc, 1, 0, SME_MASK, SR_X | SR_C | SR_Z | SR_N | SR_V, 0, NULL }, /* ILLEGAL */
    { 0xff00, 0x4a00, 1, 1, SME_MASK, 0, SR_C | SR_Z | SR_N | SR_V, NULL },        /* TAS/TST */
    { 0xffc0, 0x4c00, 1, 1, SME_MASK, 0, SR_C | SR_Z | SR_N | SR_V, NULL },        /* MULU/MULS */
    { 0xffc0, 0x4c40, 2, 1, SME_MASK, 0, SR_C | SR_Z | SR_N | SR_V, NULL },        /* DIVU/DIVS */
    { 0xfff0, 0x4e40, 1, 0, SME_MASK, SR_X | SR_C | SR_Z | SR_N | SR_V, 0, NULL }, /* TRAP */
    { 0xfff8, 0x4e50, 2, 0, SME_FUNC, 0, 0, SR_TestOpcode32B },                    /* LINK */
    { 0xfff8, 0x4e58, 1, 0, SME_FUNC, 0, 0, SR_TestOpcode16B },                    /* UNLK */
    { 0xfff0, 0x4e60, 1, 0, SME_MASK, SR_X | SR_C | SR_Z | SR_N | SR_V, 0, NULL }, /* MOVE USP */
    { 0xffff, 0x4e70, 1, 0, SME_MASK, SR_X | SR_C | SR_Z | SR_N | SR_V, 0, NULL }, /* RESET */
    { 0xffff, 0x4e71, 1, 0, SME_FUNC, 0, 0, SR_TestOpcode16B },                    /* NOP */
    { 0xffff, 0x4e72, 2, 0, SME_MASK, SR_X | SR_C | SR_Z | SR_N | SR_V, SR_X | SR_C | SR_Z | SR_N | SR_V, NULL }, /* STOP */
    { 0xffff, 0x4e73, 1, 0, SME_MASK, 0, SR_X | SR_C | SR_Z | SR_N | SR_V, NULL }, /* RTE */
    { 0xffff, 0x4e74, 2, 0, SME_MASK, 0, 0, NULL },                                /* RTD */
    { 0xffff, 0x4e75, 1, 0, SME_MASK, 0, 0, NULL },                                /* RTS */
    { 0xffff, 0x4e76, 1, 0, SME_MASK, SR_V, 0, NULL },                             /* TRAPV */
    { 0xffff, 0x4e77, 1, 0, SME_MASK, 0, SR_X | SR_C | SR_Z | SR_N | SR_V, NULL }, /* RTR */
    { 0xfffe, 0x4e7a, 2, 0, SME_MASK, SR_X | SR_C | SR_Z | SR_N | SR_V, 0, NULL }, /* MOVEC */
    { 0xffc0, 0x4e80, 1, 1, SME_MASK, 0, 0, NULL },                                /* JSR */
    { 0xffc0, 0x4ec0, 1, 1, SME_MASK, 0, 0, NULL },                                /* JMP */
    { 0xfb80, 0x4880, 2, 1, SME_FUNC, 0, 0, SR_TestOpcode32B },                    /* MOVEM */
    { 0xf1c0, 0x41c0, 1, 1, SME_FUNC, 0, 0, SR_TestOpcodeEA },                     /* LEA */
    { 0xf140, 0x4100, 1, 1, SME_MASK, 0, SR_C | SR_Z | SR_N | SR_V, NULL },        /* CHK */
    { 0x0000, 0x0000, 0, 0, SME_END,  0, 0, NULL }
};

static struct SRMaskEntry Line5_Map[] = {
    { 0xf0f8, 0x50c8, 2, 0, SME_MASK, SR_C | SR_Z | SR_N | SR_V, 0, NULL },        /* DBcc */
    { 0xf0ff, 0x50fc, 1, 0, SME_MASK, SR_C | SR_Z | SR_N | SR_V, 0, NULL },        /* TRAPcc */
    { 0xf0ff, 0x50fa, 2, 0, SME_MASK, SR_C | SR_Z | SR_N | SR_V, 0, NULL },        /* TRAPcc.W */
    { 0xf0ff, 0x50fb, 2, 0, SME_MASK, SR_C | SR_Z | SR_N | SR_V, 0, NULL },        /* TRAPcc.L */
    { 0xf0c0, 0x50c0, 1, 1, SME_MASK, SR_C | SR_Z | SR_N | SR_V, 0, NULL },        /* Scc */
    { 0xf038, 0x5008, 1, 1, SME_FUNC, 0, 0, SR_TestOpcode16B },                    /* SUBQ/ADDQ with An */
    { 0xf000, 0x5000, 1, 1, SME_MASK, 0, SR_X | SR_C | SR_V | SR_Z | SR_N, NULL }, /* SUBQ/ADDQ */
    { 0x0000, 0x0000, 0, 0, SME_END,  0, 0, NULL }
};

static struct SRMaskEntry Line6_Map[] = {
    { 0xfe00, 0x6000, SME_FUNC,  0, 0, SR_TestBranch },                      /* BRA/BSR */
    { 0xf000, 0x6000, SME_MASK,  SR_C | SR_Z | SR_N | SR_V, 0, NULL },       /* Bcc */
    { 0x0000, 0x0000, SME_END,  0, 0, NULL }
};

static struct SRMaskEntry Line7_Map[] = {
    { 0xf000, 0x7000, 1, 0, SME_MASK, 0, SR_C | SR_V | SR_Z | SR_N, NULL },        /* MOVEQ */
    { 0x0000, 0x0000, 0, 0, SME_END,  0, 0, NULL }
};

static struct SRMaskEntry Line8_Map[] = {
    { 0xf1c0, 0x80c0, 1, 1, SME_MASK, 0, SR_C | SR_V | SR_Z | SR_N, NULL },        /* DIVU */
    { 0xf1f0, 0x8100, 1, 0, SME_MASK, SR_X, SR_X | SR_C | SR_V | SR_Z | SR_N, NULL }, /* SBCD */
    { 0xf1f0, 0x8140, 2, 0, SME_MASK, 0, 0, NULL },                                /* PACK */
    { 0xf1f0, 0x8180, 2, 0, SME_MASK, 0, 0, NULL },                                /* UNPK */
    { 0xf1c0, 0x81c0, 1, 1, SME_MASK, 0, SR_C | SR_V | SR_Z | SR_N, NULL },        /* DIVS */
    { 0xf000, 0x8000, 1, 1, SME_MASK, 0, SR_C | SR_V | SR_Z | SR_N, NULL },        /* OR */
    { 0x0000, 0x0000, 0, 0, SME_END,  0, 0, NULL }
};

static struct SRMaskEntry Line9_Map[] = {
    { 0xf0c0, 0x90c0, 1, 1, SME_FUNC, 0, 0, SR_TestOpcodeADDA },                   /* SUBA */
    { 0xf130, 0x9100, 1, 0, SME_MASK, SR_X, SR_X | SR_C | SR_V | SR_Z | SR_N, NULL }, /* SUBX */
    { 0xf000, 0x9000, 1, 1, SME_MASK, 0, SR_X | SR_C | SR_V | SR_Z | SR_N, NULL }, /* SUB */
    { 0x0000, 0x0000, 0, 0, SME_END,  0, NULL }
};

static struct SRMaskEntry LineA_Map[] = {
    { 0x0000, 0x0000, 0, 0, SME_END,  0, 0, NULL }
};

static struct SRMaskEntry LineB_Map[] = {
    { 0xf000, 0xb000, SME_MASK, 0, SR_C | SR_V | SR_Z | SR_N, NULL },        /* CMP/CMPM/CMPA/EOR */
    { 0x0000, 0x0000, 0, 0, SME_END,  0, NULL }
};

static struct SRMaskEntry LineC_Map[] = {
    { 0xf1c0, 0xc0c0, SME_MASK, 0, SR_C | SR_V | SR_Z | SR_N, NULL },        /* MULU */
    { 0xf1f0, 0xc100, SME_MASK, 0, SR_X | SR_C | SR_V | SR_Z | SR_N, NULL }, /* ABCD */
    { 0xf1c0, 0xc1c0, SME_MASK, 0, SR_C | SR_V | SR_Z | SR_N, NULL },        /* MULS */
    { 0xf1f0, 0xc140, SME_FUNC, 0, 0, SR_TestOpcode16B },                    /* EXG Dx,Dy / EXG Ax,Ay */
    { 0xf1f0, 0xc180, SME_FUNC, 0, 0, SR_TestOpcode16B },                    /* EXG Dx,Ay */
    { 0xf000, 0xc000, SME_MASK, 0, SR_C | SR_V | SR_Z | SR_N, NULL },        /* AND */
    { 0x0000, 0x0000, 0, 0, SME_END,  0, 0, NULL }
};

static struct SRMaskEntry LineD_Map[] = {
    { 0xf0c0, 0xd0c0, 1, 1, SME_FUNC, 0, 0, SR_TestOpcodeADDA },                   /* ADDA */
    { 0xf130, 0xd100, 1, 0, SME_MASK, SR_X, SR_X | SR_C | SR_V | SR_Z | SR_N, NULL },        /* ADDX - reqires X and modifies X! */
    { 0xf000, 0xd000, 1, 1, SME_MASK, 0, SR_X | SR_C | SR_V | SR_Z | SR_N, NULL }, /* ADD */
    { 0x0000, 0x0000, 0, 0, SME_END,  0, NULL }
};

static struct SRMaskEntry LineE_Map[] = {
    { 0xfec0, 0xe0c0, SME_MASK, 0, SR_X | SR_C | SR_V | SR_Z | SR_N, NULL }, /* ASL/ASR */
    { 0xfec0, 0xe2c0, SME_MASK, 0, SR_X | SR_C | SR_V | SR_Z | SR_N, NULL }, /* LSL/LSR */
    { 0xfec0, 0xe4c0, SME_MASK, SR_X, SR_X | SR_C | SR_V | SR_Z | SR_N, NULL },        /* ROXL/ROXR */
    { 0xfec0, 0xe6c0, SME_MASK, 0, SR_C | SR_V | SR_Z | SR_N, NULL },        /* ROL/ROR */
    { 0xffc0, 0xe8c0, SME_MASK, 0, SR_C | SR_V | SR_Z | SR_N, NULL },        /* BFTST */
    { 0xffc0, 0xe9c0, SME_MASK, 0, SR_C | SR_V | SR_Z | SR_N, NULL },        /* BFEXTU */
    { 0xffc0, 0xeac0, SME_MASK, 0, SR_C | SR_V | SR_Z | SR_N, NULL },        /* BFCHG */
    { 0xffc0, 0xebc0, SME_MASK, 0, SR_C | SR_V | SR_Z | SR_N, NULL },        /* BFEXTS */
    { 0xffc0, 0xecc0, SME_MASK, 0, SR_C | SR_V | SR_Z | SR_N, NULL },        /* BFCLR */
    { 0xffc0, 0xedc0, SME_MASK, 0, SR_C | SR_V | SR_Z | SR_N, NULL },        /* BFFFO */
    { 0xffc0, 0xeec0, SME_MASK, 0, SR_C | SR_V | SR_Z | SR_N, NULL },        /* BFSET */
    { 0xffc0, 0xefc0, SME_MASK, 0, SR_C | SR_V | SR_Z | SR_N, NULL },        /* BFINS */
    { 0xf018, 0xe000, SME_MASK, SR_X, SR_X | SR_C | SR_V | SR_Z | SR_N, NULL }, /* ASL/ASR */
    { 0xf018, 0xe008, SME_MASK, SR_X, SR_X | SR_C | SR_V | SR_Z | SR_N, NULL }, /* LSL/LSR */
    { 0xf018, 0xe010, SME_MASK, SR_X, SR_C | SR_V | SR_Z | SR_N, NULL },        /* ROXL/ROXR */
    { 0xf018, 0xe018, SME_MASK, 0, SR_C | SR_V | SR_Z | SR_N, NULL },        /* ROL/ROR */
    { 0x0000, 0x0000, 0, 0, SME_END,  0, 0, NULL }
};

static struct SRMaskEntry LineF_Map[] = {
    { 0x0000, 0x0000, 0, 0, SME_END, 0, 0, NULL }
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
        (opcode & 0xf000) == 0x5000 ||
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

/* Get number of 16-bit words this instruction occupies */
int M68K_GetINSNLength(uint16_t *insn_stream)
{
    uint16_t opcode = *insn_stream;
    int length = 0;

//    kprintf("[SR] M68K_GetINSNLength() addr=%08x opcode=%04x", insn_stream, BE16(*insn_stream));

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
        default:
            break;
    }

//    kprintf(" = %d\n", length);

    return length;
}