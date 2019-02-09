#include <stdint.h>

#include <stdio.h>
#include "ARM.h"
#include "M68k.h"

struct SRMaskEntry {
    uint16_t me_OpcodeMask;
    uint16_t me_Opcode;
    uint8_t  me_Type;
    uint8_t  me_SRMask;
};

#define SME_MASK    1
#define SME_END     255

static struct SRMaskEntry Line0_Map[] = {
    { 0xffbf, 0x003c, SME_MASK, 0 },                                /* ORI to CCR/SR - they rely on current CC! */
    { 0xff00, 0x0000, SME_MASK, SR_C | SR_Z | SR_N | SR_V },        /* ORI */
    { 0xffbf, 0x023c, SME_MASK, 0 },                                /* ANDI to CCR/SR - they rely on current CC! */
    { 0xf9c0, 0x00c0, SME_MASK, SR_C | SR_Z | SR_N | SR_V },        /* CHK2/CMP2 */
    { 0xff00, 0x0200, SME_MASK, SR_C | SR_Z | SR_N | SR_V },        /* ANDI */
    { 0xff00, 0x0400, SME_MASK, SR_X | SR_C | SR_Z | SR_N | SR_V }, /* SUBI */
    { 0xffc0, 0x06c0, SME_MASK, 0 },                                /* RTM/CALLM */
    { 0xff00, 0x0600, SME_MASK, SR_X | SR_C | SR_Z | SR_N | SR_V }, /* ADDI */
    { 0xffbf, 0x0a3c, SME_MASK, 0 },                                /* EORI to CCR/SR - they rely on current CC! */
    { 0xff00, 0x0a00, SME_MASK, SR_C | SR_Z | SR_N | SR_V },        /* EORI */
    { 0xff00, 0x0c00, SME_MASK, SR_C | SR_Z | SR_N | SR_V },        /* CMPI */
    { 0xff00, 0x0800, SME_MASK, SR_Z },                             /* BTST/BSET/BCLR/BCHG */
    { 0xf9c0, 0x08c0, SME_MASK, SR_C | SR_Z | SR_N | SR_V },        /* CAS/CAS2 */
    { 0xf100, 0x0100, SME_MASK, SR_Z },                             /* BTST/BSET/BCLR/BCHG */
    { 0x0000, 0x0000, SME_END,  0 }
};

static struct SRMaskEntry Line1_Map[] = {
    { 0xc040, 0x0040, SME_MASK, 0 },                                /* MOVEA case - destination is An */
    { 0xc000, 0x0000, SME_MASK, SR_C | SR_V | SR_Z | SR_N },        /* All other moves change CC */
    { 0x0000, 0x0000, SME_END,  0 }
};

static struct SRMaskEntry Line2_Map[sizeof(Line1_Map)/sizeof(struct SRMaskEntry)] __attribute__((alias("Line1_Map")));
static struct SRMaskEntry Line3_Map[sizeof(Line1_Map)/sizeof(struct SRMaskEntry)] __attribute__((alias("Line1_Map")));

static struct SRMaskEntry Line4_Map[] = {
    { 0xfdc0, 0x40c0, SME_MASK, 0 },                                /* MOVE from CCR/SR */
    { 0xff00, 0x4000, SME_MASK, SR_X | SR_C | SR_Z | SR_N | SR_V }, /* NEGX */
    { 0xff00, 0x4200, SME_MASK, SR_C | SR_Z | SR_N | SR_V },        /* CLR */
    { 0xffc0, 0x44c0, SME_MASK, SR_X | SR_C | SR_Z | SR_N | SR_V }, /* MOVE to CCR */
    { 0xff00, 0x4400, SME_MASK, SR_X | SR_C | SR_Z | SR_N | SR_V }, /* NEG */
    { 0xffc0, 0x46c0, SME_MASK, SR_X | SR_C | SR_Z | SR_N | SR_V }, /* MOVE to SR */
    { 0xff00, 0x4600, SME_MASK, SR_C | SR_Z | SR_N | SR_V },        /* NOT */
    { 0xfeb8, 0x4880, SME_MASK, SR_C | SR_Z | SR_N | SR_V },        /* EXT/EXTB */
    { 0xfff8, 0x4808, SME_MASK, 0 },                                /* LINK */
    { 0xffc0, 0x4800, SME_MASK, SR_X | SR_C | SR_Z | SR_N | SR_V }, /* NBCD */
    { 0xfff8, 0x4840, SME_MASK, SR_C | SR_Z | SR_N | SR_V },        /* SWAP */
    { 0xffc0, 0x4840, SME_MASK, 0 },                                /* BKPT/PEA */
    { 0xffff, 0x4afc, SME_MASK, 0 },                                /* ILLEGAL */
    { 0xff00, 0x4a00, SME_MASK, SR_C | SR_Z | SR_N | SR_V },        /* TAS/TST */
    { 0xff80, 0x4a00, SME_MASK, SR_C | SR_Z | SR_N | SR_V },        /* MULU/MULS/DIVU/DIVS */
    { 0xfff0, 0x4e40, SME_MASK, 0 },                                /* TRAP */
    { 0xfff8, 0x4e50, SME_MASK, 0 },                                /* LINK */
    { 0xfff8, 0x4e58, SME_MASK, 0 },                                /* UNLK */
    { 0xfff0, 0x4e60, SME_MASK, 0 },                                /* MOVE USP */
    { 0xffff, 0x4e70, SME_MASK, 0 },                                /* RESET */
    { 0xffff, 0x4e71, SME_MASK, 0 },                                /* NOP */
    { 0xffff, 0x4e72, SME_MASK, SR_X | SR_C | SR_Z | SR_N | SR_V }, /* STOP */
    { 0xffff, 0x4e73, SME_MASK, SR_X | SR_C | SR_Z | SR_N | SR_V }, /* RTE */
    { 0xffff, 0x4e74, SME_MASK, 0 },                                /* RTD */
    { 0xffff, 0x4e75, SME_MASK, 0 },                                /* RTS */
    { 0xffff, 0x4e76, SME_MASK, 0 },                                /* TRAPV */
    { 0xffff, 0x4e77, SME_MASK, SR_X | SR_C | SR_Z | SR_N | SR_V }, /* RTR */
    { 0xfffe, 0x4e7a, SME_MASK, 0 },                                /* MOVEC */
    { 0xffc0, 0x4e80, SME_MASK, 0 },                                /* JSR */
    { 0xffc0, 0x4ec0, SME_MASK, 0 },                                /* JMP */
    { 0xfb80, 0x4880, SME_MASK, 0 },                                /* MOVEM */
    { 0xf1c0, 0x41c0, SME_MASK, 0 },                                /* LEA */
    { 0xf140, 0x4100, SME_MASK, 0 },                                /* CHK */
    { 0x0000, 0x0000, SME_END,  0 }
};

static struct SRMaskEntry Line5_Map[] = {
    { 0xf0c0, 0x50c0, SME_MASK, 0 },                                /* TRAP/DBcc/Scc */
    { 0xf000, 0x5000, SME_MASK, SR_X | SR_C | SR_V | SR_Z | SR_N }, /* SUBQ/ADDQ */
    { 0x0000, 0x0000, SME_END,  0 }
};

static struct SRMaskEntry Line6_Map[] = {
    { 0x0000, 0x0000, SME_END,  0 }                                 /* BRA/BSR/Bcc */
};

static struct SRMaskEntry Line7_Map[] = {
    { 0xf000, 0x7000, SME_MASK, SR_C | SR_V | SR_Z | SR_N },        /* MOVEQ */
    { 0x0000, 0x0000, SME_END,  0 }
};

static struct SRMaskEntry Line8_Map[] = {
    { 0xf1c0, 0x80c0, SME_MASK, SR_C | SR_V | SR_Z | SR_N },        /* DIVU */
    { 0xf1f0, 0x8100, SME_MASK, SR_X | SR_C | SR_V | SR_Z | SR_N }, /* SBCD */
    { 0xf1f0, 0x8140, SME_MASK, 0 },                                /* PACK */
    { 0xf1f0, 0x8180, SME_MASK, 0 },                                /* UNPK */
    { 0xf1c0, 0x81c0, SME_MASK, SR_C | SR_V | SR_Z | SR_N },        /* DIVS */
    { 0xf000, 0x8000, SME_MASK, SR_C | SR_V | SR_Z | SR_N },        /* OR */
    { 0x0000, 0x0000, SME_END,  0 }
};

static struct SRMaskEntry Line9_Map[] = {
    { 0xf0c0, 0x90c0, SME_MASK, 0 },                                /* SUBA */
    { 0xf000, 0x9000, SME_MASK, SR_X | SR_C | SR_V | SR_Z | SR_N }, /* SUB/SUBX */
    { 0x0000, 0x0000, SME_END,  0 }
};

static struct SRMaskEntry LineA_Map[] = {
    { 0x0000, 0x0000, SME_END,  0 }
};

static struct SRMaskEntry LineB_Map[] = {
    { 0xf000, 0xb000, SME_MASK, SR_C | SR_V | SR_Z | SR_N },        /* CMP/CMPM/CMPA/EOR */
    { 0x0000, 0x0000, SME_END,  0 }
};

static struct SRMaskEntry LineC_Map[] = {
    { 0xf1c0, 0xc0c0, SME_MASK, SR_C | SR_V | SR_Z | SR_N },        /* MULU */
    { 0xf1f0, 0xc100, SME_MASK, SR_X | SR_C | SR_V | SR_Z | SR_N }, /* ABCD */
    { 0xf1c0, 0xc1c0, SME_MASK, SR_C | SR_V | SR_Z | SR_N },        /* MULS */
    { 0xf1f0, 0xc140, SME_MASK, 0 },                                /* EXG Dx,Dy / EXG Ax,Ay */
    { 0xf1f0, 0xc180, SME_MASK, 0 },                                /* EXG Dx,Ay */
    { 0xf000, 0xc000, SME_MASK, SR_C | SR_V | SR_Z | SR_N },        /* AND */
    { 0x0000, 0x0000, SME_END,  0 }
};

static struct SRMaskEntry LineD_Map[] = {
    { 0xf0c0, 0xd0c0, SME_MASK, 0 },                                /* ADDA */
    { 0xf000, 0xd000, SME_MASK, SR_X | SR_C | SR_V | SR_Z | SR_N }, /* ADD/ADDX */
    { 0x0000, 0x0000, SME_END,  0 }
};

static struct SRMaskEntry LineE_Map[] = {
    { 0xfec0, 0xe0c0, SME_MASK, SR_X | SR_C | SR_V | SR_Z | SR_N }, /* ASL/ASR */
    { 0xfec0, 0xe2c0, SME_MASK, SR_X | SR_C | SR_V | SR_Z | SR_N }, /* LSL/LSR */
    { 0xfec0, 0xe4c0, SME_MASK, SR_X | SR_C | SR_V | SR_Z | SR_N }, /* ROXL/ROXR */
    { 0xfec0, 0xe6c0, SME_MASK, SR_C | SR_V | SR_Z | SR_N },        /* ROL/ROR */
    { 0xffc0, 0xe8c0, SME_MASK, SR_C | SR_V | SR_Z | SR_N },        /* BFTST */
    { 0xffc0, 0xe9c0, SME_MASK, SR_C | SR_V | SR_Z | SR_N },        /* BFEXTU */
    { 0xffc0, 0xeac0, SME_MASK, SR_C | SR_V | SR_Z | SR_N },        /* BFCHG */
    { 0xffc0, 0xebc0, SME_MASK, SR_C | SR_V | SR_Z | SR_N },        /* BFEXTS */
    { 0xffc0, 0xecc0, SME_MASK, SR_C | SR_V | SR_Z | SR_N },        /* BFCLR */
    { 0xffc0, 0xedc0, SME_MASK, SR_C | SR_V | SR_Z | SR_N },        /* BFFFO */
    { 0xffc0, 0xeec0, SME_MASK, SR_C | SR_V | SR_Z | SR_N },        /* BFSET */
    { 0xffc0, 0xefc0, SME_MASK, SR_C | SR_V | SR_Z | SR_N },        /* BFINS */
    { 0xf018, 0xe000, SME_MASK, SR_X | SR_C | SR_V | SR_Z | SR_N }, /* ASL/ASR */
    { 0xf018, 0xe008, SME_MASK, SR_X | SR_C | SR_V | SR_Z | SR_N }, /* LSL/LSR */
    { 0xf018, 0xe010, SME_MASK, SR_X | SR_C | SR_V | SR_Z | SR_N }, /* ROXL/ROXR */
    { 0xf018, 0xe018, SME_MASK, SR_C | SR_V | SR_Z | SR_N },        /* ROL/ROR */
    { 0x0000, 0x0000, SME_END,  0 }
};

static struct SRMaskEntry LineF_Map[] = {
    { 0x0000, 0x0000, SME_END,  0 }
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

/* Get the mask of status flags changed by the instruction specified by the opcode */
uint8_t M68K_GetSRMask(uint16_t opcode)
{
    uint8_t mask = 0;

    /* Fetch correct table baset on bits 12..15 of the opcode */
    struct SRMaskEntry *e = OpcodeMap[opcode >> 12];

    /* Search within table until SME_END is found */
    while (e->me_Type != SME_END)
    {
        if ((opcode & e->me_OpcodeMask) == e->me_Opcode)
        {
            mask = e->me_SRMask;
            break;
        }
        e++;
    }

    return mask;
}
