/*
    Copyright © 2019-2025 Michal Schulz <michal.schulz@gmx.de>
    https://github.com/michalsc

    This Source Code Form is subject to the terms of the
    Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed
    with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#define restrict __restrict__

#include <emu68/FPR>
#include <emu68/GPR>
#include <emu68/ppc/Opcode>

#include "cache.h"
#include "config.h"
#include "PPC.h"
#include "A64.h"

namespace Emu68::PPC::Emit {

int addi(PPCTranslatorContext* tc, uint32_t opcode)
{
    uint8_t rd = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint16_t imm = opcode & 0xffff;
    int32_t simm = (int16_t)imm;

    /* If rA == 0 then this is just load immediate */
    if (ra == 0) {
        GPR reg_rd = tc->mapGPRForWrite(rd);
        tc->emitLoadImmediate(reg_rd, (uint32_t)simm);
    } else {
        GPR arm_rd, arm_ra;

        /* Source register and target are the same */
        if (ra == rd) {
            arm_rd = tc->mapGPRForReadAndWrite(rd);
            arm_ra = GPR(arm_rd.get());
        } else {
            arm_ra = tc->mapGPRForRead(ra);
            arm_rd = tc->mapGPRForWrite(rd);
        }

        /* If negative, we handle subtraction */
        if (simm < 0) {
            simm = -simm;
            if ((simm & 0xfffff000) == 0) {
                tc->emit(sub_immed(arm_rd, arm_ra, simm & 0xfff));
            } else if ((simm & 0xffff0fff) == 0) {
                tc->emit(sub_immed_lsl12(arm_rd, arm_ra, (simm >> 12) & 0xfff));
            } else {
                GPR tmp = GPR::allocate();
                tc->emitLoadImmediate(tmp, simm);
                tc->emit(sub_reg(arm_rd, arm_ra, tmp, LSL, 0));
            }
        } else {
            if ((simm & 0xfffff000) == 0) {
                tc->emit(add_immed(arm_rd, arm_ra, simm & 0xfff));
            } else if ((simm & 0xffff0fff) == 0) {
                tc->emit(add_immed_lsl12(arm_rd, arm_ra, (simm >> 12) & 0xfff));
            } else {
                GPR tmp = GPR::allocate();
                tc->emitLoadImmediate(tmp, simm);
                tc->emit(add_reg(arm_rd, arm_ra, tmp, LSL, 0));
            }
        }
    }

    tc->advancePC(4);

    return 1;
}

int addis(PPCTranslatorContext* tc, uint32_t opcode)
{
    uint8_t rd = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint16_t imm = opcode & 0xffff;

    /* If rA == 0 then this is load upper */
    if (ra == 0) {
        GPR arm_rd = tc->mapGPRForWrite(rd);
        
        /* If next opcode is ori or addi to the same register, then it is a load immediate */
        uint32_t opcode2 = cache_read_32(ICACHE, (uint32_t)(uintptr_t)(tc->getPC() + 1));
        uint32_t expected = (rd << 21) | (rd << 16);
        /* Check if ori or addi */
        if ((opcode2 & 0xffff0000) == (expected | (24 << 26)) ||     // This is ori
            (opcode2 & 0xffff8000) == (expected | (14 << 26)))       // This is addi, must be positive, thus test bit 15 too
        {
            tc->emitLoadImmediate(arm_rd, (opcode << 16) | (opcode2 & 0xffff));
            tc->advancePC(8);
            return 2;
        }
        else
        {
            /* Regular case, just a load immediate shifted */
            tc->emit(mov_immed_u16(arm_rd, imm, 1));
        }
    } else {
        GPR arm_rd, arm_ra;

        /* Source register and target are the same */
        if (ra == rd) {
            arm_rd = tc->mapGPRForReadAndWrite(rd);
            arm_ra = GPR(arm_rd.get());
        } else {
            arm_ra = tc->mapGPRForRead(ra);
            arm_rd = tc->mapGPRForWrite(rd);
        }

        if (imm & 0xff00) {
            GPR tmp = GPR::allocate();
            tc->emit({ 
                mov_immed_u16(tmp, imm, 1),
                add_reg(arm_rd, arm_ra, tmp, LSL, 0)
            });
        }
        else {
            tc->emit(add_immed_lsl12(arm_rd, arm_ra, (imm & 0xff) << 4));
        }
    }

    tc->advancePC(4);

    return 1;
}

int cmpi(PPCTranslatorContext* tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 0x00600000) return -1;

    uint8_t cr = (opcode >> 23) & 7;
    uint8_t ra = (opcode >> 16) & 31;
    uint16_t imm = opcode & 0xffff;
    int32_t simm = (int16_t)imm;

    /* Force-load XER and CR */
#if PPC_SO_PROPAGATION
    tc->mapGPRForRead(XERn);
#endif
    tc->mapGPRForReadAndWrite(CRn);

    GPR reg_ra = tc->mapGPRForRead(ra);

    /* Is the immediate in range for CMP? */
    if ((simm & 0xfffff000) == 0) {
        tc->emit(cmp_immed(reg_ra, simm));
    }
    else if ((simm & 0xff000fff) == 0) {
        tc->emit(cmp_immed_lsl12(reg_ra, simm >> 12));
    }
    else {
        GPR tmp = GPR::allocate();

        tc->emitLoadImmediate(tmp, simm);
        tc->emit(cmp_reg(reg_ra, tmp, LSL, 0));
    }

    Emit::setCRnSigned(tc, cr);

    tc->advancePC(4);

    return 1;
}

int cmpli(PPCTranslatorContext* tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 0x00600000) return -1;

    uint8_t cr = (opcode >> 23) & 7;
    uint8_t ra = (opcode >> 16) & 31;
    uint32_t imm = opcode & 0xffff;

    /* Force-load XER and CR */
#if PPC_SO_PROPAGATION
    tc->mapGPRForRead(XERn);
#endif
    tc->mapGPRForReadAndWrite(CRn);

    GPR reg_ra = tc->mapGPRForRead(ra);

    /* Is the immediate in range for CMP? */
    if ((imm & 0xfffff000) == 0) {
        tc->emit(cmp_immed(reg_ra, imm));
    }
    else if ((imm & 0xff000fff) == 0) {
        tc->emit(cmp_immed_lsl12(reg_ra, imm >> 12));
    }
    else {
        GPR tmp = GPR::allocate();

        tc->emitLoadImmediate(tmp, imm);
        tc->emit(cmp_reg(reg_ra, tmp, LSL, 0));
    }

    Emit::setCRnUnsigned(tc, cr);

    tc->advancePC(4);

    return 1;
}

int ori(PPCTranslatorContext* tc, uint32_t opcode)
{
    uint32_t immed = opcode & 0xffff;
    uint32_t mask = number_to_mask(immed);
    uint8_t rs = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    
    uint8_t reg_rS = tc->mapGPRForRead(rs);
    uint8_t reg_rA = tc->mapGPRForReadAndWrite(ra);

    if (immed == 0) {
        if (rs == ra)
            tc->emit(nop());
        else
            tc->emit(mov_reg(reg_rA, reg_rS));
    }
    else {
        if (mask == 0) {
            GPR tmp = GPR::allocate();
            tc->emit({
                mov_immed_u16(tmp, immed, 0),
                orr_reg(reg_rA, reg_rS, tmp, LSL, 0)
            });
        } else {
            tc->emit(orr_immed(reg_rA, reg_rS, (mask >> 16) & 0x3f, mask & 0x3f));
        }
    }

    tc->advancePC(4);

    return 1;
}

int oris(PPCTranslatorContext* tc, uint32_t opcode)
{
    uint32_t immed = opcode & 0xffff;
    uint32_t mask = number_to_mask(immed << 16);
    uint8_t rs = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    
    GPR reg_rS = tc->mapGPRForRead(rs);
    GPR reg_rA = tc->mapGPRForReadAndWrite(ra);

    if (mask == 0) {
        GPR tmp = GPR::allocate();
        tc->emit({
            mov_immed_u16(tmp, immed, 1),
            orr_reg(reg_rA, reg_rS, tmp, LSL, 0)
        });
    } else {
        tc->emit(orr_immed(reg_rA, reg_rS, (mask >> 16) & 0x3f, mask & 0x3f));
    }

    tc->advancePC(4);

    return 1;
}

int andi_dot(PPCTranslatorContext* tc, uint32_t opcode)
{
    uint32_t immed = opcode & 0xffff;
    uint32_t mask = number_to_mask(immed);
    uint8_t rs = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    
    GPR reg_rS = tc->mapGPRForRead(rs);
    GPR reg_rA = tc->mapGPRForReadAndWrite(ra);

    if (mask == 0) {
        GPR tmp = GPR::allocate();
        tc->emit({
            mov_immed_u16(tmp, immed, 0),
            ands_reg(reg_rA, reg_rS, tmp, LSL, 0)
        });
    } else {
        tc->emit(ands_immed(reg_rA, reg_rS, (mask >> 16) & 0x3f, mask & 0x3f));
    }

    Emit::setCRnLogicNoMinus(tc, 0);

    tc->advancePC(4);

    return 1;
}

int andis_dot(PPCTranslatorContext* tc, uint32_t opcode)
{
    uint32_t immed = opcode & 0xffff;
    uint32_t mask = number_to_mask(immed << 16);
    uint8_t rs = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    
    GPR reg_rS = tc->mapGPRForRead(rs);
    GPR reg_rA = tc->mapGPRForReadAndWrite(ra);

    if (mask == 0) {
        GPR tmp = GPR::allocate();
        tc->emit({
            mov_immed_u16(tmp, immed, 1),
            ands_reg(reg_rA, reg_rS, tmp, LSL, 0)
        });
    } else {
        tc->emit(ands_immed(reg_rA, reg_rS, (mask >> 16) & 0x3f, mask & 0x3f));
    }

    if (immed & 0x8000)
        Emit::setCRnLogic(tc, 0);
    else
        Emit::setCRnLogicNoMinus(tc, 0);

    tc->advancePC(4);

    return 1;
}

int xori(PPCTranslatorContext* tc, uint32_t opcode)
{
    uint32_t immed = opcode & 0xffff;
    uint32_t mask = number_to_mask(immed);
    uint8_t rs = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    
    GPR reg_rS = tc->mapGPRForRead(rs);
    GPR reg_rA = tc->mapGPRForReadAndWrite(ra);

    if (mask == 0) {
        GPR tmp = GPR::allocate();
        tc->emit({
            mov_immed_u16(tmp, immed, 0),
            eor_reg(reg_rA, reg_rS, tmp, LSL, 0)
        });
    } else {
        tc->emit(eor_immed(reg_rA, reg_rS, (mask >> 16) & 0x3f, mask & 0x3f));
    }

    tc->advancePC(4);

    return 1;
}

int xoris(PPCTranslatorContext* tc, uint32_t opcode)
{
    uint32_t immed = opcode & 0xffff;
    uint32_t mask = number_to_mask(immed << 16);
    uint8_t rs = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    
    GPR reg_rS = tc->mapGPRForRead(rs);
    GPR reg_rA = tc->mapGPRForReadAndWrite(ra);

    if (mask == 0) {
        GPR tmp = GPR::allocate();
        tc->emit({
            mov_immed_u16(tmp, immed, 1),
            eor_reg(reg_rA, reg_rS, tmp, LSL, 0)
        });
    } else {
        tc->emit(eor_immed(reg_rA, reg_rS, (mask >> 16) & 0x3f, mask & 0x3f));
    }

    tc->advancePC(4);

    return 1;
}

int rlwimix(PPCTranslatorContext* tc, uint32_t opcode)
{
    uint8_t update_cr = opcode & 1;
    uint8_t rs = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t sh = (opcode >> 11) & 31;
    uint8_t mb = (opcode >> 6) & 31;
    uint8_t me = (opcode >> 1) & 31;

    GPR reg_rs = tc->mapGPRForRead(rs);
    GPR reg_ra = tc->mapGPRForWrite(ra);
    GPR tmp = GPR::allocate();
    
    /* TODO: add obvious shortcuts! */

    /* If sh is set, rotate left */
    if (sh) {
        tc->emit(ror(tmp, reg_rs, (32 - sh) & 31));
    }

    /* Mask result if me - mb is not 31 */
    if (((me - mb) & 31) != 31)
    {
        if (mb <= me)
        {
            /* mb < me - mask of type 0x0f...f0 */
            tc->emit({
                bic_immed(reg_ra, reg_ra, 1 + me - mb, 31 & (me + 1)),
                and_immed(tmp, tmp, 1 + me - mb, 31 & (me + 1)),
                orr_reg(reg_ra, reg_ra, tmp, LSL, 0)
            });
        }
        else if (me < mb)
        {
            /* mb < me - mask of type 0xf..0..f */
            tc->emit({
                bic_immed(reg_ra, reg_ra, mb - me - 1, me + 1),
                and_immed(tmp, tmp, mb - me - 1, me + 1),
                orr_reg(reg_ra, reg_ra, tmp, LSL, 0)
            });
        }
    } else if (update_cr) {
        if (!sh)
            tc->emit(adds_immed(reg_ra, reg_rs, 0));
        else
            tc->emit(adds_immed(reg_ra, tmp, 0));
    }

    if (update_cr) 
    {
        setCRnLogic(tc, 0);
    }

    tc->advancePC(4);

    return 1;
}

int rlwinmx(PPCTranslatorContext* tc, uint32_t opcode)
{
    uint8_t update_cr = opcode & 1;
    uint8_t rs = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t sh = (opcode >> 11) & 31;
    uint8_t mb = (opcode >> 6) & 31;
    uint8_t me = (opcode >> 1) & 31;

    GPR reg_rs = tc->mapGPRForRead(rs);
    GPR reg_ra = tc->mapGPRForWrite(ra);
    GPR and_src = GPR(reg_rs.get());

    /* TODO: add obvious shortcuts! */

    if (mb == 0 && me == (31 - sh))
    {
        /* shifting left, leaving zeros on right */
        tc->emit(lsl(reg_ra, reg_rs, sh));
    }
    else if (me == 31 && sh == (32 - mb))
    {
        /* shifting right, zeros on the left */
        tc->emit(lsr(reg_ra, reg_rs, 32 - sh));
    }
    else
    {
        /* If sh is set, rotate left */
        if (sh) {
            tc->emit(ror(reg_ra, reg_rs, (32 - sh) & 31));
            and_src = GPR(reg_ra.get());
        }

        /* Mask result if me - mb is not 31 */
        if (((me - mb) & 31) != 31)
        {
            if (mb <= me)
            {
                /* mb < me - mask of type 0x0f...f0 */
                tc->emit(update_cr ?
                    ands_immed(reg_ra, and_src, 1 + me - mb, 31 & (me + 1)) :
                    and_immed(reg_ra, and_src, 1 + me - mb, 31 & (me + 1))
                );
            }
            else if (me < mb)
            {
                /* mb < me - mask of type 0xf..0..f */
                tc->emit(update_cr ?
                    ands_immed(reg_ra, and_src, mb - me - 1, me + 1) :
                    and_immed(reg_ra, and_src, mb - me - 1, me + 1)
                );
            }
        } else if (update_cr) {
            if (!sh)
                tc->emit(adds_immed(reg_ra, reg_rs, 0));
            else
                tc->emit(tst_immed(reg_ra, 32, 0));
        }
    }

    if (update_cr) 
    {
        setCRnLogic(tc, 0);
    }

    tc->advancePC(4);

    return 1;
}

int rlwnmx(PPCTranslatorContext* tc, uint32_t opcode)
{
    uint8_t update_cr = opcode & 1;
    uint8_t rs = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;
    uint8_t mb = (opcode >> 6) & 31;
    uint8_t me = (opcode >> 1) & 31;

    GPR reg_rs = tc->mapGPRForRead(rs);
    GPR reg_rb = tc->mapGPRForRead(rb);
    GPR reg_ra = tc->mapGPRForWrite(ra);

    /* TODO: add obvious shortcuts! */

    tc->emit({
        neg_reg(reg_ra, reg_rb, LSL, 0),
        rorv(reg_ra, reg_rs, reg_ra)
    });

    /* Mask result if me - mb is not 31 */
    if (((me - mb) & 31) != 31)
    {
        if (mb <= me)
        {
            /* mb < me - mask of type 0x0f...f0 */
            tc->emit(update_cr ?
                ands_immed(reg_ra, reg_ra, 1 + me - mb, 31 & (me + 1)) :
                and_immed(reg_ra, reg_ra, 1 + me - mb, 31 & (me + 1))
            );
        }
        else if (me < mb)
        {
            /* mb < me - mask of type 0xf..0..f */
            tc->emit(update_cr ?
                ands_immed(reg_ra, reg_ra, mb - me - 1, me + 1) :
                and_immed(reg_ra, reg_ra, mb - me - 1, me + 1)
            );
        }
    } else if (update_cr) {
        tc->emit(cmp_immed(reg_ra, 0));
    }

    if (update_cr) 
    {
        setCRnLogic(tc, 0);
    }

    tc->advancePC(4);

    return 1;
}

int orx(PPCTranslatorContext* tc, uint32_t opcode)
{
    uint8_t update_cr = opcode & 1;
    uint8_t rs = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;

    if (rs == 27 && ra == 27 && rb == 27) {
        tc->emit(yield());
        tc->advancePC(4);
        return 1;
    }

    GPR reg_rs = tc->mapGPRForRead(rs);
    GPR reg_rb = tc->mapGPRForRead(rb);
    GPR reg_ra = tc->mapGPRForWrite(ra);

    if (rb == rs) {
        tc->emit(mov_reg(reg_ra, reg_rs));
    } else {
        tc->emit(orr_reg(reg_ra, reg_rs, reg_rb, LSL, 0));
    }

    if (update_cr) {
        tc->emit(cmp_immed(reg_ra, 0));
        setCRnLogic(tc, 0);
    }

    tc->advancePC(4);

    return 1;
}

int cntlzwx(PPCTranslatorContext* tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 0x0000f800) return -1;

    uint8_t update_cr = opcode & 1;
    uint8_t rs = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;

    GPR reg_rs = tc->mapGPRForRead(rs);
    GPR reg_ra = tc->mapGPRForWrite(ra);

    tc->emit(clz(reg_ra, reg_rs));

    if (update_cr) {
        tc->emit(cmp_immed(reg_ra, 0));
        setCRnLogic(tc, 0);
    }

    tc->advancePC(4);

    return 1;
}

int mulhwux(PPCTranslatorContext* tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & (0x80000000 >> 21)) return -1;

    uint8_t update_cr = opcode & 1;
    uint8_t rd = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;

    GPR reg_ra = tc->mapGPRForRead(ra);
    GPR reg_rb = tc->mapGPRForRead(rb);
    GPR reg_rd = tc->mapGPRForWrite(rd);

    tc->emit({
        umull(reg_rd, reg_ra, reg_rb),
        lsr64(reg_rd, reg_rd, 32)
    });

    if (update_cr) {
        tc->emit(cmp_immed(reg_rd, 0));
        setCRnSigned(tc, 0);
    }

    tc->advancePC(4);

    return 1;
}

int mulhwx(PPCTranslatorContext* tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & (0x80000000 >> 21)) return -1;

    uint8_t update_cr = opcode & 1;
    uint8_t rd = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;

    GPR reg_ra = tc->mapGPRForRead(ra);
    GPR reg_rb = tc->mapGPRForRead(rb);
    GPR reg_rd = tc->mapGPRForWrite(rd);

    tc->emit({
        smull(reg_rd, reg_ra, reg_rb),
        lsr64(reg_rd, reg_rd, 32)
    });

    if (update_cr) {
        tc->emit(cmp_immed(reg_rd, 0));
        setCRnSigned(tc, 0);
    }

    tc->advancePC(4);

    return 1;
}

int andx(PPCTranslatorContext* tc, uint32_t opcode)
{
    uint8_t update_cr = opcode & 1;
    uint8_t rs = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;

    GPR reg_rs = tc->mapGPRForRead(rs);
    GPR reg_rb = tc->mapGPRForRead(rb);
    GPR reg_ra = tc->mapGPRForWrite(ra);

    if (update_cr) {
        tc->emit(ands_reg(reg_ra, reg_rs, reg_rb, LSL, 0));
        setCRnLogic(tc, 0);
    } else {
        tc->emit(and_reg(reg_ra, reg_rs, reg_rb, LSL, 0));
    }

    tc->advancePC(4);

    return 1;
}

int norx(PPCTranslatorContext* tc, uint32_t opcode)
{
    uint8_t update_cr = opcode & 1;
    uint8_t rs = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;

    GPR reg_rs = tc->mapGPRForRead(rs);
    GPR reg_rb = tc->mapGPRForRead(rb);
    GPR reg_ra = tc->mapGPRForWrite(ra);

    if (rs == rb) {
        tc->emit(mvn_reg(reg_ra, reg_rb, LSL, 0));
    } else {
        tc->emit({ 
            orr_reg(reg_ra, reg_rs, reg_rb, LSL, 0),
            mvn_reg(reg_ra, reg_ra, LSL, 0)
        });
    }
    
    if (update_cr) {
        tc->emit(cmp_immed(reg_ra, 0));
        setCRnLogic(tc, 0);
    }

    tc->advancePC(4);

    return 1;
}

int andcx(PPCTranslatorContext* tc, uint32_t opcode)
{
    uint8_t update_cr = opcode & 1;
    uint8_t rs = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;

    GPR reg_rs = tc->mapGPRForRead(rs);
    GPR reg_rb = tc->mapGPRForRead(rb);
    GPR reg_ra = tc->mapGPRForWrite(ra);

    if (update_cr) {
        tc->emit(bics_reg(reg_ra, reg_rs, reg_rb, LSL, 0));
        setCRnLogic(tc, 0);
    } else {
        tc->emit(bic_reg(reg_ra, reg_rs, reg_rb, LSL, 0));
    }

    tc->advancePC(4);

    return 1;
}

int eqvx(PPCTranslatorContext* tc, uint32_t opcode)
{
    uint8_t update_cr = opcode & 1;
    uint8_t rs = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;

    GPR reg_rs = tc->mapGPRForRead(rs);
    GPR reg_rb = tc->mapGPRForRead(rb);
    GPR reg_ra = tc->mapGPRForWrite(ra);

    if (rb == rs) {
        tc->emit(mvn_reg(reg_ra, WZR, LSL, 0));
    } else {
        tc->emit(eon_reg(reg_ra, reg_rs, reg_rb, LSL, 0));
    }

    if (update_cr) {
        tc->emit(cmp_immed(reg_ra, 0));
        setCRnLogic(tc, 0);
    }

    tc->advancePC(4);

    return 1;
}

int xorx(PPCTranslatorContext* tc, uint32_t opcode)
{
    uint8_t update_cr = opcode & 1;
    uint8_t rs = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;

    GPR reg_rs = tc->mapGPRForRead(rs);
    GPR reg_rb = tc->mapGPRForRead(rb);
    GPR reg_ra = tc->mapGPRForWrite(ra);

    tc->emit(eor_reg(reg_ra, reg_rs, reg_rb, LSL, 0));

    if (update_cr) {
        tc->emit(cmp_immed(reg_ra, 0));
        setCRnLogic(tc, 0);
    }

    tc->advancePC(4);

    return 1;
}

int orcx(PPCTranslatorContext* tc, uint32_t opcode)
{
    uint8_t update_cr = opcode & 1;
    uint8_t rs = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;

    GPR reg_rs = tc->mapGPRForRead(rs);
    GPR reg_rb = tc->mapGPRForRead(rb);
    GPR reg_ra = tc->mapGPRForWrite(ra);

    tc->emit(orn_reg(reg_ra, reg_rs, reg_rb, LSL, 0));

    if (update_cr) {
        tc->emit(cmp_immed(reg_ra, 0));
        setCRnLogic(tc, 0);
    }

    tc->advancePC(4);

    return 1;
}

int nandx(PPCTranslatorContext* tc, uint32_t opcode)
{
    uint8_t update_cr = opcode & 1;
    uint8_t rs = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;

    GPR reg_rs = tc->mapGPRForRead(rs);
    GPR reg_rb = tc->mapGPRForRead(rb);
    GPR reg_ra = tc->mapGPRForWrite(ra);

    tc->emit({ 
        and_reg(reg_ra, reg_rs, reg_rb, LSL, 0),
        mvn_reg(reg_ra, reg_ra, LSL, 0)
    });

    if (update_cr) {
        tc->emit(cmp_immed(reg_ra, 0));
        setCRnLogic(tc, 0);
    }

    tc->advancePC(4);

    return 1;
}

int mulli(PPCTranslatorContext* tc, uint32_t opcode)
{
    uint8_t rd = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint16_t immed = opcode & 0xffff;

    GPR reg_ra = tc->mapGPRForRead(ra);
    GPR reg_rd = tc->mapGPRForWrite(rd);
    GPR tmp = GPR::allocate();

    if (immed & 0x8000) {
        tc->emit(movn_immed_u16(tmp, ~immed & 0xffff, 0));
    } else {
        tc->emit(mov_immed_u16(tmp, immed, 0));
    }

    tc->emit(mul(reg_rd, reg_ra, tmp));

    tc->advancePC(4);

    return 1;
}

int addx(PPCTranslatorContext* tc, uint32_t opcode)
{
    uint8_t rd = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;
    uint8_t oe = (opcode >> 10) & 1;
    uint8_t rc = opcode & 1;

    GPR reg_ra = tc->mapGPRForRead(ra);
    GPR reg_rb = tc->mapGPRForRead(rb);
    GPR reg_rd = tc->mapGPRForWrite(rd);

    if (oe || rc) {
        tc->emit(adds_reg(reg_rd, reg_ra, reg_rb, LSL, 0));
    } else {
        tc->emit(add_reg(reg_rd, reg_ra, reg_rb, LSL, 0));
    }

    if (oe) {
        GPR reg_xer = tc->mapGPRForReadAndWrite(XERn);
        GPR tmp = GPR::allocate();

        tc->emit({
            orr_immed(reg_xer, reg_xer, 2, 2),
            bic_immed(tmp, reg_xer, 1, 2),
            csel(reg_xer, reg_xer, tmp, A64_CC_VS)
        });
    }

    if (rc) {
        setCRnSigned(tc, 0);
    }

    tc->advancePC(4);

    return 1;
}

int addic(PPCTranslatorContext* tc, uint32_t opcode)
{
    uint8_t rd = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint16_t imm = (opcode & 0xffff);

    GPR reg_ra = tc->mapGPRForRead(ra);
    GPR reg_rd = tc->mapGPRForWrite(rd);
    GPR tmp = GPR::allocate();
    GPR reg_xer = tc->mapGPRForReadAndWrite(XERn);

    if (imm & 0x8000) {
        tc->emit(movn_immed_u16(tmp, ~imm & 0xffff, 0));
    } else {
        tc->emit(mov_immed_u16(tmp, imm, 0));
    }

    tc->emit({ 
        bic_immed(reg_xer, reg_xer, 1, 3),      // Clear CA flag in xer
        adds_reg(reg_rd, tmp, reg_ra, LSL, 0),
        orr_immed(tmp, reg_xer, 1, 3),          // Set CA flag from xer into tmp
        csel(reg_xer, tmp, reg_xer, A64_CC_CS)  // Select CA set or clear in XER depending on A64 C flag
    });

    tc->advancePC(4);

    return 1;
}

int addic_dot(PPCTranslatorContext* tc, uint32_t opcode)
{
    uint8_t rd = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint16_t imm = (opcode & 0xffff);

    GPR reg_ra = tc->mapGPRForRead(ra);
    GPR reg_rd = tc->mapGPRForWrite(rd);
    GPR tmp = GPR::allocate();
    GPR reg_xer = tc->mapGPRForReadAndWrite(XERn);

    if (imm & 0x8000) {
        tc->emit(movn_immed_u16(tmp, ~imm & 0xffff, 0));
    } else {
        tc->emit(mov_immed_u16(tmp, imm, 0));
    }

    tc->emit({ 
        bic_immed(reg_xer, reg_xer, 1, 3),      // Clear CA flag in xer
        adds_reg(reg_rd, tmp, reg_ra, LSL, 0),
        orr_immed(tmp, reg_xer, 1, 3),          // Set CA flag from xer into tmp
        csel(reg_xer, tmp, reg_xer, A64_CC_CS)  // Select CA set or clear in XER depending on A64 C flag
    });

    setCRnSigned(tc, 0);

    tc->advancePC(4);

    return 1;
}

int subfx(PPCTranslatorContext* tc, uint32_t opcode)
{
    uint8_t rd = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;
    uint8_t oe = (opcode >> 10) & 1;
    uint8_t rc = opcode & 1;

    GPR reg_ra = tc->mapGPRForRead(ra);
    GPR reg_rb = tc->mapGPRForRead(rb);
    GPR reg_rd = tc->mapGPRForWrite(rd);

    if (oe || rc) {
        tc->emit(subs_reg(reg_rd, reg_rb, reg_ra, LSL, 0));
    } else {
        tc->emit(sub_reg(reg_rd, reg_rb, reg_ra, LSL, 0));
    }

    if (oe) {
        GPR reg_xer = tc->mapGPRForReadAndWrite(XERn);
        GPR tmp = GPR::allocate();

        tc->emit({
            orr_immed(reg_xer, reg_xer, 2, 2),
            bic_immed(tmp, reg_xer, 1, 2),
            csel(reg_xer, reg_xer, tmp, A64_CC_VS)
        });
    }

    if (rc) {
        setCRnSigned(tc, 0);
    }

    tc->advancePC(4);

    return 1;
}

int subfic(PPCTranslatorContext* tc, uint32_t opcode)
{
    uint8_t rd = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint16_t imm = (opcode & 0xffff);

    GPR reg_ra = tc->mapGPRForRead(ra);
    GPR reg_rd = tc->mapGPRForWrite(rd);
    GPR tmp = GPR::allocate();
    GPR reg_xer = tc->mapGPRForReadAndWrite(XERn);

    if (imm & 0x8000) {
        tc->emit(movn_immed_u16(tmp, ~imm & 0xffff, 0));
    } else {
        tc->emit(mov_immed_u16(tmp, imm, 0));
    }

    tc->emit({ 
        bic_immed(reg_xer, reg_xer, 1, 3),      // Clear CA flag in xer
        subs_reg(reg_rd, tmp, reg_ra, LSL, 0),
        orr_immed(tmp, reg_xer, 1, 3),          // Set CA flag from xer into tmp
        csel(reg_xer, tmp, reg_xer, A64_CC_CS)  // Select CA set or clear in XER depending on A64 C flag
    });

    tc->advancePC(4);

    return 1;
}

int subfcx(PPCTranslatorContext* tc, uint32_t opcode)
{
    uint8_t rd = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;
    uint8_t oe = (opcode >> 10) & 1;
    uint8_t rc = opcode & 1;

    GPR reg_ra = tc->mapGPRForRead(ra);
    GPR reg_rb = tc->mapGPRForRead(rb);
    GPR reg_rd = tc->mapGPRForWrite(rd);
    GPR tmp = GPR::allocate();
    GPR reg_xer = tc->mapGPRForReadAndWrite(XERn);
    // TODO: Verify if flag set correctly

    tc->emit({ 
        bic_immed(reg_xer, reg_xer, 1, 3),      // Clear CA flag in xer
        subs_reg(reg_rd, reg_rb, reg_ra, LSL, 0),
        orr_immed(tmp, reg_xer, 1, 3),          // Set CA flag from xer into tmp
        csel(reg_xer, tmp, reg_xer, A64_CC_CS)  // Select CA set or clear in XER depending on A64 C flag
    });

    if (oe) {
        tc->emit({
            orr_immed(reg_xer, reg_xer, 2, 2),
            bic_immed(tmp, reg_xer, 1, 2),
            csel(reg_xer, reg_xer, tmp, A64_CC_VS)
        });
    }

    if (rc) {
        setCRnSigned(tc, 0);
    }

    tc->advancePC(4);

    return 1;
}

int addcx(PPCTranslatorContext* tc, uint32_t opcode)
{
    uint8_t rd = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;
    uint8_t oe = (opcode >> 10) & 1;
    uint8_t rc = opcode & 1;

    GPR reg_ra = tc->mapGPRForRead(ra);
    GPR reg_rb = tc->mapGPRForRead(rb);
    GPR reg_rd = tc->mapGPRForWrite(rd);
    GPR tmp = GPR::allocate();
    GPR reg_xer = tc->mapGPRForReadAndWrite(XERn);
    // TODO: Verify if flag set correctly

    tc->emit({ 
        bic_immed(reg_xer, reg_xer, 1, 3),      // Clear CA flag in xer
        adds_reg(reg_rd, reg_rb, reg_ra, LSL, 0),
        orr_immed(tmp, reg_xer, 1, 3),          // Set CA flag from xer into tmp
        csel(reg_xer, tmp, reg_xer, A64_CC_CS)  // Select CA set or clear in XER depending on A64 C flag
    });

    if (oe) {
        tc->emit({
            orr_immed(reg_xer, reg_xer, 2, 2),
            bic_immed(tmp, reg_xer, 1, 2),
            csel(reg_xer, reg_xer, tmp, A64_CC_VS)
        });
    }

    if (rc) {
        setCRnSigned(tc, 0);
    }

    tc->advancePC(4);

    return 1;
}

int addex(PPCTranslatorContext* tc, uint32_t opcode)
{
    uint8_t rd = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;
    uint8_t oe = (opcode >> 10) & 1;
    uint8_t rc = opcode & 1;

    GPR reg_ra = tc->mapGPRForRead(ra);
    GPR reg_rb = tc->mapGPRForRead(rb);
    GPR reg_rd = tc->mapGPRForWrite(rd);
    GPR tmp = GPR::allocate();
    GPR reg_xer = tc->mapGPRForReadAndWrite(XERn);
    // TODO: Verify if flag set correctly

    tc->emit({ 
        ubfx(tmp, reg_xer, 29, 1),                 // Extract CA field into tmp
        subs_immed(WZR, tmp, 1),                    // Subtract 1, this will set carry bit
        bic_immed(reg_xer, reg_xer, 1, 3),          // Clear CA flag in xer
        adcs(reg_rd, reg_ra, reg_rb),               // Add with carry
        orr_immed(tmp, reg_xer, 1, 3),              // Set CA flag from xer into tmp
        csel(reg_xer, tmp, reg_xer, A64_CC_CS)      // Select CA set or clear in XER depending on A64 C flag
    });

    if (oe) {
        tc->emit({
            orr_immed(reg_xer, reg_xer, 2, 2),
            bic_immed(tmp, reg_xer, 1, 2),
            csel(reg_xer, reg_xer, tmp, A64_CC_VS)
        });
    }

    if (rc) {
        setCRnSigned(tc, 0);
    }

    tc->advancePC(4);

    return 1;
}

int cmp(PPCTranslatorContext* tc, uint32_t opcode)
{
    /* sanity check */
    if (opcode & 0x00600001) return -1;

    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;
    uint8_t cr = (opcode >> 23) & 7;

    GPR reg_ra = tc->mapGPRForRead(ra);
    GPR reg_rb = tc->mapGPRForRead(rb);

    /* Force-load XER and CR */
#if PPC_SO_PROPAGATION
    tc->mapGPRForRead(XERn);
#endif
    tc->mapGPRForReadAndWrite(CRn);

    tc->emit(cmp_reg(reg_ra, reg_rb, LSL, 0));

    setCRnSigned(tc, cr);

    tc->advancePC(4);

    return 1;
}

int cmpl(PPCTranslatorContext* tc, uint32_t opcode)
{
    /* sanity check */
    if (opcode & 0x00600001) return -1;

    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;
    uint8_t cr = (opcode >> 23) & 7;

    /* Force-load XER and CR */
#if PPC_SO_PROPAGATION
    tc->mapGPRForRead(XERn);
#endif
    tc->mapGPRForReadAndWrite(CRn);

    GPR reg_ra = tc->mapGPRForRead(ra);
    uint8_t reg_rb = tc->mapGPRForRead(rb);

    tc->emit(cmp_reg(reg_ra, reg_rb, LSL, 0));

    setCRnUnsigned(tc, cr);

    tc->advancePC(4);

    return 1;
}

int negx(PPCTranslatorContext* tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 0x0000f800) return -1;

    uint8_t rd = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    
    uint8_t oe = (opcode >> 10) & 1;
    uint8_t rc = opcode & 1;

    GPR reg_ra = tc->mapGPRForRead(ra);
    GPR reg_rd = tc->mapGPRForWrite(rd);

    if (oe || rc) {
        tc->emit(negs_reg(reg_rd, reg_ra, LSL, 0));
    } else {
        tc->emit(neg_reg(reg_rd, reg_ra, LSL, 0));
    }

    if (oe) {
        GPR reg_xer = tc->mapGPRForReadAndWrite(XERn);
        GPR tmp = GPR::allocate();

        tc->emit({
            orr_immed(reg_xer, reg_xer, 2, 2),
            bic_immed(tmp, reg_xer, 1, 2),
            csel(reg_xer, reg_xer, tmp, A64_CC_VS)
        });
    }

    if (rc) {
        setCRnSigned(tc, 0);
    }

    tc->advancePC(4);

    return 1;
}

int divwux(PPCTranslatorContext* tc, uint32_t opcode)
{
    uint8_t rd = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;
    
    uint8_t oe = (opcode >> 10) & 1;
    uint8_t rc = opcode & 1;

    GPR reg_ra = tc->mapGPRForRead(ra);
    GPR reg_rb = tc->mapGPRForRead(rb);
    GPR reg_rd = tc->mapGPRForWrite(rd);

    if (oe) {
        GPR tmp = GPR::allocate();
        GPR reg_xer = tc->mapGPRForReadAndWrite(XERn);

        tc->emit({
            cmp_immed(reg_rb, 0),
            orr_immed(reg_xer, reg_xer, 2, 2),
            bic_immed(tmp, reg_xer, 1, 2),
            csel(reg_xer, reg_xer, tmp, A64_CC_EQ)
        });
    }

    tc->emit(udiv(reg_rd, reg_ra, reg_rb));

    if (rc) {
        tc->emit(cmp_immed(reg_rd, 0));
        setCRnSigned(tc, 0);
    }

    tc->advancePC(4);

    return 1;
}

int divwx(PPCTranslatorContext* tc, uint32_t opcode)
{
    uint8_t rd = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;
    
    uint8_t oe = (opcode >> 10) & 1;
    uint8_t rc = opcode & 1;

    GPR reg_ra = tc->mapGPRForRead(ra);
    GPR reg_rb = tc->mapGPRForRead(rb);
    GPR reg_rd = tc->mapGPRForWrite(rd);

    if (oe) {
        GPR tmp = GPR::allocate();
        GPR reg_xer = tc->mapGPRForReadAndWrite(XERn);

        // TODO: set XER OV if 0x80000000 / -1 is attempted

        tc->emit({
            cmp_immed(reg_rb, 0),
            orr_immed(reg_xer, reg_xer, 2, 2),
            bic_immed(tmp, reg_xer, 1, 2),
            csel(reg_xer, reg_xer, tmp, A64_CC_EQ)
        });
    }

    tc->emit(sdiv(reg_rd, reg_ra, reg_rb));

    if (rc) {
        tc->emit(cmp_immed(reg_rd, 0));
        setCRnSigned(tc, 0);
    }

    tc->advancePC(4);

    return 1;
}

int mullwx(PPCTranslatorContext* tc, uint32_t opcode)
{
    uint8_t rd = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;
    
    uint8_t oe = (opcode >> 10) & 1;
    uint8_t rc = opcode & 1;

    GPR reg_ra = tc->mapGPRForRead(ra);
    GPR reg_rb = tc->mapGPRForRead(rb);
    GPR reg_rd = tc->mapGPRForWrite(rd);

    tc->emit(smull(reg_rd, reg_ra, reg_rb));

    if (oe) {
        GPR tmp = GPR::allocate();
        GPR reg_xer = tc->mapGPRForReadAndWrite(XERn);

        tc->emit({
            sxtw64(tmp, reg_rd),
            cmp_reg(tmp, reg_rd, LSL, 0),
            orr_immed(reg_xer, reg_xer, 2, 2),
            bic_immed(tmp, reg_xer, 1, 2),
            csel(reg_xer, reg_xer, tmp, A64_CC_NE)
        });
    }

    if (rc) {
        tc->emit(cmp_immed(reg_rd, 0));
        setCRnSigned(tc, 0);
    }

    tc->advancePC(4);

    return 1;
}

int srwx(PPCTranslatorContext* tc, uint32_t opcode)
{
    uint8_t rs = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;
    
    uint8_t rc = opcode & 1;

    GPR reg_rb = tc->mapGPRForRead(rb);
    GPR reg_rs = tc->mapGPRForRead(rs);
    GPR reg_ra = tc->mapGPRForWrite(ra);

    tc->emit(lsrv(reg_ra, reg_rs, reg_rb));

    if (rc) {
        tc->emit(cmp_immed(reg_ra, 0));
        setCRnSigned(tc, 0);
    }

    tc->advancePC(4);

    return 1;
}

int slwx(PPCTranslatorContext* tc, uint32_t opcode)
{
    uint8_t rs = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;
    
    uint8_t rc = opcode & 1;

    GPR reg_rs = tc->mapGPRForRead(rs);
    GPR reg_rb = tc->mapGPRForRead(rb);
    GPR reg_ra = tc->mapGPRForWrite(ra);

    tc->emit(lslv(reg_ra, reg_rs, reg_rb));

    if (rc) {
        tc->emit(cmp_immed(reg_ra, 0));
        setCRnSigned(tc, 0);
    }

    tc->advancePC(4);

    return 1;
}

int srawix(PPCTranslatorContext* tc, uint32_t opcode)
{
    uint8_t rs = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t sh = (opcode >> 11) & 31;
    
    uint8_t rc = opcode & 1;

    GPR reg_rs = tc->mapGPRForRead(rs);
    GPR reg_ra = tc->mapGPRForWrite(ra);

    GPR tmp = GPR::allocate();
    GPR reg_xer = tc->mapGPRForReadAndWrite(XERn);
    if (sh != 0) {
        tc->emit({
            /* Test if source is signed */
            tst_immed(reg_rs, 1, 1),
            /* If source is signed, put reg_rs to tmp, otherwise zero it */
            csel(tmp, reg_rs, XZR, A64_CC_EQ),
            /* Check the mask - if any bit is 1, set CA to 1 */
            tst_immed(tmp, sh, 0),
            orr_immed(reg_xer, reg_xer, 1, 3),
            bic_immed(tmp, reg_xer, 1, 3),
            csel(reg_xer, reg_xer, tmp, A64_CC_NE)
        });
    } else {
        tc->emit(bic_immed(reg_xer, reg_xer, 1, 3));
    }
    
    tc->emit(asr(reg_ra, reg_rs, sh));

    if (rc) {
        tc->emit(cmp_immed(reg_ra, 0));
        setCRnSigned(tc, 0);
    }

    tc->advancePC(4);

    return 1;
}

int srawx(PPCTranslatorContext* tc, uint32_t opcode)
{
    uint8_t rs = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;
    
    uint8_t rc = opcode & 1;

    GPR reg_rs = tc->mapGPRForRead(rs);
    GPR reg_rb = tc->mapGPRForRead(rb);
    GPR reg_ra = tc->mapGPRForWrite(ra);

    GPR tmp = GPR::allocate();
    GPR mask = GPR::allocate();
    GPR reg_xer = tc->mapGPRForReadAndWrite(XERn);

    tc->emit({
        /* Test if source is signed */
        tst_immed(reg_rs, 1, 1),
        /* If source is signed, put reg_rs to tmp, otherwise zero it */
        csel(tmp, reg_rs, XZR, A64_CC_EQ),
        /* Check the mask - if any bit is 1, set CA to 1 */
        mov_immed_u16(mask, 1, 0),
        lslv64(mask, mask, reg_rb),
        sub64_immed(mask, mask, 1),
        tst_reg(tmp, mask, LSL, 0),
        orr_immed(reg_xer, reg_xer, 1, 3),
        bic_immed(tmp, reg_xer, 1, 3),
        csel(reg_xer, reg_xer, tmp, A64_CC_NE)
    });
    
    tc->emit(asrv(reg_ra, reg_rs, reg_rb));

    if (rc) {
        tc->emit(cmp_immed(reg_ra, 0));
        setCRnSigned(tc, 0);
    }

    tc->advancePC(4);

    return 1;
}

int extsbx(PPCTranslatorContext* tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 0x0000f800) return -1;

    uint8_t rs = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    
    uint8_t rc = opcode & 1;

    GPR reg_rs = tc->mapGPRForRead(rs);
    GPR reg_ra = tc->mapGPRForWrite(ra);
    
    tc->emit(sxtb(reg_ra, reg_rs));

    if (rc) {
        tc->emit(cmp_immed(reg_ra, 0));
        setCRnLogic(tc, 0);
    }

    tc->advancePC(4);

    return 1;
}

int extshx(PPCTranslatorContext* tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 0x0000f800) return -1;

    uint8_t rs = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    
    uint8_t rc = opcode & 1;

    GPR reg_rs = tc->mapGPRForRead(rs);
    GPR reg_ra = tc->mapGPRForWrite(ra);
    
    tc->emit(sxth(reg_ra, reg_rs));

    if (rc) {
        tc->emit(cmp_immed(reg_ra, 0));
        setCRnLogic(tc, 0);
    }

    tc->advancePC(4);

    return 1;
}

} // Emu68::PPC::Emit
