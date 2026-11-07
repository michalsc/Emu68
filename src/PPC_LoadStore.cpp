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

#include "config.h"
#include "PPC.h"
#include "A64.h"

namespace Emu68::PPC::Emit {

int stb(PPCTranslatorContext *tc, uint32_t opcode)
{
    uint8_t rs = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    int16_t d = opcode & 0xffff;

    GPR reg = tc->mapGPRForRead(rs);

    /* If Ra is 0, then address is the displacement, only */
    if (ra == 0) {
        GPR ea = GPR::allocate();

        if (d < 0) {
            tc->emit( movn_immed_u16(ea, ~d & 0xffff, 0));
        } else {
            tc->emit( mov_immed_u16(ea, d, 0));
        }

        tc->emit(strb_offset(ea, reg, 0));
    } else {
        GPR base = tc->mapGPRForRead(ra);

        /* Ra is a register, check if displacement can be used for store directly */
        if (d >= 0 && d <= 0xfff) {
            tc->emit( strb_offset(base, reg, d));
        } else if (d >= -256 && d < 255) {
            tc->emit( sturb_offset(base, reg, d));
        }
        else {
            GPR ea = GPR::allocate();

            if (d < 0) {
                tc->emit(movn_immed_u16(ea, ~d & 0xffff, 0));
            }
            else {
                tc->emit(mov_immed_u16(ea, d, 0));
            }
            
            tc->emit(strb_regoffset(base, reg, ea, SXTW));
        }
    }

    tc->advancePC(4);

    return 1;
}

int stbu(PPCTranslatorContext *tc, uint32_t opcode)
{
    uint8_t rs = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    int16_t d = opcode & 0xffff;

    /* It is illegal to have Ra = 0 */
    if (ra == 0) {
        return -1;
    }

    GPR reg = tc->mapGPRForRead(rs);
    GPR base = tc->mapGPRForReadAndWrite(ra);

    /* Ra is a register, check if displacement can be used for store directly */
    if (d >= -256 && d <= 255) {
        tc->emit( strb_offset_preindex(base, reg, d));
    }
    else if (d >= 0 && d <= 0xfff) {
        tc->emit({ 
            strb_offset(base, reg, d),
            add_immed(base, base, d)
        });
    }
    else {
        GPR ea = GPR::allocate();

        if (d < 0) {
            tc->emit( movn_immed_u16(ea, ~d & 0xffff, 0));
        }
        else {
            tc->emit( mov_immed_u16(ea, d, 0));
        }
        
        tc->emit({ 
            strb_regoffset(base, reg, ea, SXTW),
            add_reg(base, base, ea, LSL, 0)
        });
    }

    tc->advancePC(4);

    return 1;
}

int sth(PPCTranslatorContext *tc, uint32_t opcode)
{
    uint8_t rs = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    int16_t d = opcode & 0xffff;

    GPR reg = tc->mapGPRForRead(rs);

    /* If Ra is 0, then address is the displacement, only */
    if (ra == 0) {
        GPR ea = GPR::allocate();
        if (d < 0) {
            tc->emit( movn_immed_u16(ea, ~d & 0xffff, 0));
        } else {
            tc->emit( mov_immed_u16(ea, d, 0));
        }

        tc->emit( strh_offset(ea, reg, 0));
    } else {
        /* Ra is a register, check if displacement can be used for store directly */
        if (d >= 0 && d <= 0x1ffe && (d & 1) == 0) {
            GPR base = tc->mapGPRForRead(ra);
            tc->emit( strh_offset(base, reg, d));
        }
        else if (d >= -256 && d <= 255) {
            GPR base = tc->mapGPRForRead(ra);
            tc->emit( sturh_offset(base, reg, d));
        }
        else {
            GPR ea = GPR::allocate();
            GPR base = tc->mapGPRForRead(ra);

            if (d < 0) {
                tc->emit( movn_immed_u16(ea, ~d & 0xffff, 0));
            }
            else {
                tc->emit( mov_immed_u16(ea, d, 0));
            }
            
            tc->emit( strh_regoffset(base, reg, ea, SXTW, 0));
        }
    }

    tc->advancePC(4);

    return 1;
}

int stbx(PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 1) return -1;

    uint8_t rs = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;

    GPR reg_rd = tc->mapGPRForRead(rs);
    GPR reg_rb = tc->mapGPRForRead(rb);
    
    /* If Ra is 0, then address is the displacement, only */
    if (ra == 0) {
        tc->emit( strb_offset(reg_rb, reg_rd, 0));
    } else {
        GPR reg_ra = tc->mapGPRForRead(ra);
        tc->emit( strb_regoffset(reg_ra, reg_rd, reg_rb, SXTW));
    }

    tc->advancePC(4);

    return 1;
}

int stbux(PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 1) return -1;

    uint8_t rs = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;

    if (ra == 0 || ra == rs) return -1;

    GPR reg_rd = tc->mapGPRForRead(rs);
    GPR reg_rb = tc->mapGPRForRead(rb);
    GPR reg_ra = tc->mapGPRForReadAndWrite(ra);

    tc->emit({ 
        strb_regoffset(reg_ra, reg_rd, reg_rb, SXTW),
        add_reg(reg_ra, reg_ra, reg_rb, LSL, 0)
    });

    tc->advancePC(4);

    return 1;
}

int sthx(PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 1) return -1;

    uint8_t rs = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;

    GPR reg_rd = tc->mapGPRForRead(rs);
    GPR reg_rb = tc->mapGPRForRead(rb);
    
    /* If Ra is 0, then address is the displacement, only */
    if (ra == 0) {
        tc->emit( strh_offset(reg_rb, reg_rd, 0));
    } else {
        GPR reg_ra = tc->mapGPRForRead(ra);
        tc->emit( strh_regoffset(reg_ra, reg_rd, reg_rb, SXTW, 0));
    }

    tc->advancePC(4);

    return 1;
}

int sthux(PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 1) return -1;

    uint8_t rs = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;

    if (ra == 0 || ra == rs) return -1;

    GPR reg_rd = tc->mapGPRForRead(rs);
    GPR reg_rb = tc->mapGPRForRead(rb);
    GPR reg_ra = tc->mapGPRForReadAndWrite(ra);

    tc->emit({ 
        strh_regoffset(reg_ra, reg_rd, reg_rb, SXTW, 0),
        add_reg(reg_ra, reg_ra, reg_rb, LSL, 0)
    });

    tc->advancePC(4);

    return 1;
}

int stwx(PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 1) return -1;

    uint8_t rs = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;

    GPR reg_rd = tc->mapGPRForRead(rs);
    GPR reg_rb = tc->mapGPRForRead(rb);
    
    /* If Ra is 0, then address is the displacement, only */
    if (ra == 0) {
        tc->emit( str_offset(reg_rb, reg_rd, 0));
    } else {
        GPR reg_ra = tc->mapGPRForRead(ra);
        tc->emit( str_regoffset(reg_ra, reg_rd, reg_rb, SXTW, 0));
    }

    tc->advancePC(4);
    
    return 1;
}

int stwux(PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 1) return -1;

    uint8_t rs = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;

    if (ra == 0 || ra == rs) return -1;

    GPR reg_rd = tc->mapGPRForRead(rs);
    GPR reg_rb = tc->mapGPRForRead(rb);
    GPR reg_ra = tc->mapGPRForReadAndWrite(ra);

    tc->emit({ 
        str_regoffset(reg_ra, reg_rd, reg_rb, SXTW, 0),
        add_reg(reg_ra, reg_ra, reg_rb, LSL, 0)
    });

    tc->advancePC(4);

    return 1;
}

/* reg = d, sign extended to 32 bits */
static void loadDisplacement(PPCTranslatorContext *tc, uint8_t reg, int16_t d)
{
    if (d < 0) {
        tc->emit( movn_immed_u16(reg, ~d & 0xffff, 0));
    } else {
        tc->emit( mov_immed_u16(reg, d, 0));
    }
}

/* reg = base + d, reg may be base */
static void addDisplacement(PPCTranslatorContext *tc, uint8_t reg, uint8_t base, int16_t d)
{
    if (d >= 0 && d < 4096) {
        tc->emit( add_immed(reg, base, d));
    }
    else if (d < 0 && d > -4096) {
        tc->emit( sub_immed(reg, base, -d));
    }
    else {
        GPR tmp = GPR::allocate();

        loadDisplacement(tc, tmp, d);
        tc->emit( add_reg(reg, base, tmp, LSL, 0));
    }
}

/* lmw and stmw, registers rS to r31 go in pairs, the last one alone if the count is odd */
static int loadStoreMultiple(PPCTranslatorContext *tc, Opcode opcode, bool store)
{
    uint8_t rs = opcode.u8(6, 10);
    uint8_t ra = opcode.u8(11, 15);
    int16_t d = opcode.i16(16, 31);

    /* rA in the range of loaded registers is an invalid form */
    if (!store && ra != 0 && ra >= rs) return -1;

    GPR ea = GPR::allocate();

    /* If Ra is 0, then address is the displacement, only */
    if (ra == 0) {
        loadDisplacement(tc, ea, d);
    } else {
        GPR base = tc->mapGPRForRead(ra);
        addDisplacement(tc, ea, base, d);
    }

    for (int r = rs; r < 32; r += 2) {
        int offset = 4 * (r - rs);

        if (r < 31) {
            GPR reg1 = store ? tc->mapGPRForRead(r) : tc->mapGPRForWrite(r);
            GPR reg2 = store ? tc->mapGPRForRead(r + 1) : tc->mapGPRForWrite(r + 1);
            tc->emit(store ? stp(ea, reg1, reg2, offset) : ldp(ea, reg1, reg2, offset));
        } else {
            GPR reg = store ? tc->mapGPRForRead(r) : tc->mapGPRForWrite(r);
            tc->emit(store ? str_offset(ea, reg, offset) : ldr_offset(ea, reg, offset));
        }
    }

    tc->advancePC(4);

    return 1;
}

int lmw(PPCTranslatorContext *tc, uint32_t opcode)
{
    return loadStoreMultiple(tc, opcode, false);
}

int stmw(PPCTranslatorContext *tc, uint32_t opcode)
{
    return loadStoreMultiple(tc, opcode, true);
}

int stw(PPCTranslatorContext *tc, uint32_t opcode)
{
    uint8_t rs = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    int16_t d = opcode & 0xffff;

    GPR reg = tc->mapGPRForRead(rs);

    /* If Ra is 0, then address is the displacement, only */
    if (ra == 0) {
        GPR ea = GPR::allocate();
        if (d < 0) {
            tc->emit( movn_immed_u16(ea, ~d & 0xffff, 0));
        } else {
            tc->emit( mov_immed_u16(ea, d, 0));
        }

        tc->emit( str_offset(ea, reg, 0));
    } else {
        /* Ra is a register, check if displacement can be used for store directly */
        if (d >= 0 && d <= 0x3ffc && (d & 3) == 0) {
            GPR base = tc->mapGPRForRead(ra);
            tc->emit( str_offset(base, reg, d));
        }
        else if (d >= -256 && d <= 255) {
            GPR base = tc->mapGPRForRead(ra);
            tc->emit( stur_offset(base, reg, d));
        }
        else {
            GPR ea = GPR::allocate();
            GPR base = tc->mapGPRForRead(ra);

            if (d < 0) {
                tc->emit( movn_immed_u16(ea, ~d & 0xffff, 0));
            }
            else {
                tc->emit( mov_immed_u16(ea, d, 0));
            }
            
            tc->emit( str_regoffset(base, reg, ea, SXTW, 0));
        }
    }

    tc->advancePC(4);

    return 1;
}

int stwu(PPCTranslatorContext *tc, uint32_t opcode)
{
    uint8_t rs = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    int16_t d = opcode & 0xffff;

    /* It is illegal to have Ra = 0 */
    if (ra == 0) {
        return -1;
    }

    GPR reg = tc->mapGPRForRead(rs);
    GPR base = tc->mapGPRForReadAndWrite(ra);

    /* Ra is a register, check if displacement can be used for store directly */
    if (d >= -256 && d <= 255) {
        tc->emit( str_offset_preindex(base, reg, d));
    }
    else if (d >= 0 && d <= 0xfff && (d & 3) == 0) {
        tc->emit({ 
            str_offset(base, reg, d),
            add_immed(base, base, d)
        });
    }
    else {
        GPR ea = GPR::allocate();

        if (d < 0) {
            tc->emit( movn_immed_u16(ea, ~d & 0xffff, 0));
        }
        else {
            tc->emit( mov_immed_u16(ea, d, 0));
        }
        
        tc->emit({ 
            str_regoffset(base, reg, ea, SXTW, 0),
            add_reg(base, base, ea, LSL, 0)
        });
    }

    tc->advancePC(4);

    return 1;
}

int sthu(PPCTranslatorContext *tc, uint32_t opcode)
{
    uint8_t rs = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    int16_t d = opcode & 0xffff;

    /* It is illegal to have Ra = 0 */
    if (ra == 0) {
        return -1;
    }

    GPR reg = tc->mapGPRForRead(rs);
    GPR base = tc->mapGPRForReadAndWrite(ra);

    /* Ra is a register, check if displacement can be used for store directly */
    if (d >= -256 && d <= 255) {
        tc->emit( strh_offset_preindex(base, reg, d));
    }
    else if (d >= 0 && d <= 0xfff && (d & 1) == 0) {
        tc->emit({ 
            strh_offset(base, reg, d),
            add_immed(base, base, d)
        });
    }
    else {
        GPR ea = GPR::allocate();

        if (d < 0) {
            tc->emit( movn_immed_u16(ea, ~d & 0xffff, 0));
        }
        else {
            tc->emit( mov_immed_u16(ea, d, 0));
        }
        
        tc->emit({ 
            strh_regoffset(base, reg, ea, SXTW, 0),
            add_reg(base, base, ea, LSL, 0)
        });
    }

    tc->advancePC(4);

    return 1;
}

int stwcx_dot(PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if ((opcode & 1) == 0) return -1;

    uint8_t rs = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;

    GPR reg_ra = ra != 0 ? tc->mapGPRForRead(ra) : GPR();
    GPR reg_rb = tc->mapGPRForRead(rb);
    GPR reg_rs = tc->mapGPRForWrite(rs);
    GPR reg_cr = tc->mapGPRForReadAndWrite(CRn);
    GPR tmp = GPR::allocate();
    GPR base(reg_rb.get());

    /* If Ra is 0, then address is the displacement, only */
    if (ra != 0) {
        base = GPR::allocate();
        tc->emit(add_reg(base, reg_rb, reg_ra, LSL, 0));
    }

    tc->emit({
        stlxr(base, reg_rs, tmp),
        cmp_immed(tmp, 0),
        bic_immed(reg_cr, reg_cr, 1, 3),
        orr_immed(tmp, reg_cr, 1, 3),
        csel(reg_cr, tmp, reg_cr, A64_CC_EQ)
    });

    tc->advancePC(4);

    return 1;
}

int sthbrx(PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 1) return -1;

    uint8_t rs = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;

    GPR reg_rs = tc->mapGPRForRead(rs);
    GPR reg_rb = tc->mapGPRForRead(rb);
    GPR tmp = GPR::allocate();
    
    tc->emit( rev16(tmp, reg_rs));

    /* If Ra is 0, then address is the displacement, only */
    if (ra == 0) {
        tc->emit( strh_offset(reg_rb, tmp, 0));
    } else {
        GPR reg_ra = tc->mapGPRForRead(ra);
        tc->emit( strh_regoffset(reg_ra, tmp, reg_rb, SXTW, 0));
    }

    tc->advancePC(4);

    return 1;
}

int stwbrx(PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 1) return -1;

    uint8_t rs = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;

    GPR reg_rs = tc->mapGPRForRead(rs);
    GPR reg_rb = tc->mapGPRForRead(rb);
    GPR tmp = GPR::allocate();

    tc->emit( rev(tmp, reg_rs));

    /* If Ra is 0, then address is the displacement, only */
    if (ra == 0) {
        tc->emit( str_offset(reg_rb, tmp, 0));
    } else {
        GPR reg_ra = tc->mapGPRForRead(ra);
        tc->emit( str_regoffset(reg_ra, tmp, reg_rb, SXTW, 0));
    }

    tc->advancePC(4);

    return 1;
}

int lbz(PPCTranslatorContext *tc, uint32_t opcode)
{
    uint8_t rd = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    int16_t d = opcode & 0xffff;

    GPR base = ra != 0 ? tc->mapGPRForRead(ra) : GPR();
    GPR reg = tc->mapGPRForWrite(rd);

    /* If Ra is 0, then address is the displacement, only */
    if (ra == 0) {
        GPR ea = GPR::allocate();

        if (d < 0) {
            tc->emit( movn_immed_u16(ea, ~d & 0xffff, 0));
        } else {
            tc->emit( mov_immed_u16(ea, d, 0));
        }

        tc->emit( ldrb_offset(ea, reg, 0));
    } else {
        /* Ra is a register, check if displacement can be used for store directly */
        if (d >= 0 && d <= 0xfff) {
            tc->emit( ldrb_offset(base, reg, d));
        }
        else if (d >= -256 && d <= 255) {
            tc->emit( ldurb_offset(base, reg, d));
        }
        else {
            GPR ea = GPR::allocate();

            if (d < 0) {
                tc->emit( movn_immed_u16(ea, ~d & 0xffff, 0));
            }
            else {
                tc->emit( mov_immed_u16(ea, d, 0));
            }
            
            tc->emit( ldrb_regoffset(base, reg, ea, SXTW));
        }
    }

    tc->advancePC(4);

    return 1;
}

int lbzu(PPCTranslatorContext *tc, uint32_t opcode)
{
    uint8_t rd = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    int16_t d = opcode & 0xffff;

    GPR base = tc->mapGPRForReadAndWrite(ra);
    GPR reg = tc->mapGPRForWrite(rd);

    /* Ra is a register, check if displacement can be used for load directly */
    if (d >= -256 && d <= 255) {
        tc->emit( ldrb_offset_preindex(base, reg, d));
    }
    else if (d >= 0 && d <= 0xfff) {
        tc->emit({ 
            ldrb_offset(base, reg, d),
            add_immed(base, base, d)
        });
    }
    else {
        GPR ea = GPR::allocate();

        if (d < 0) {
            tc->emit( movn_immed_u16(ea, ~d & 0xffff, 0));
        }
        else {
            tc->emit( mov_immed_u16(ea, d, 0));
        }
        
        tc->emit({ 
            ldrb_regoffset(base, reg, ea, SXTW),
            add_reg(base, base, ea, LSL, 0)
        });
    }

    tc->advancePC(4);

    return 1;
}

int lbzux(PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 1) return -1;

    uint8_t rd = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;

    if (ra == 0 || ra == rd) return -1;

    GPR reg_rb = tc->mapGPRForRead(rb);
    GPR reg_ra = tc->mapGPRForReadAndWrite(ra);
    GPR reg_rd = tc->mapGPRForWrite(rd);

    tc->emit({ 
        ldrb_regoffset(reg_ra, reg_rd, reg_rb, SXTW),
        add_reg(reg_ra, reg_ra, reg_rb, LSL, 0)
    });

    tc->advancePC(4);

    return 1;
}

int lbzx(PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 1) return -1;

    uint8_t rd = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;

    GPR reg_ra = ra != 0 ? tc->mapGPRForRead(ra) : GPR();
    GPR reg_rb = tc->mapGPRForRead(rb);
    GPR reg_rd = tc->mapGPRForWrite(rd);
    
    /* If Ra is 0, then address is the displacement, only */
    if (ra == 0) {
        tc->emit( ldrb_offset(reg_rb, reg_rd, 0));
    } else {
        tc->emit( ldrb_regoffset(reg_ra, reg_rd, reg_rb, SXTW));
    }

    tc->advancePC(4);

    return 1;
}

int lha(PPCTranslatorContext *tc, uint32_t opcode)
{
    uint8_t rd = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    int16_t d = opcode & 0xffff;

    GPR base = ra != 0 ? tc->mapGPRForRead(ra) : GPR();
    GPR reg = tc->mapGPRForWrite(rd);

    /* If Ra is 0, then address is the displacement, only */
    if (ra == 0) {
        GPR ea = GPR::allocate();

        if (d < 0) {
            tc->emit( movn_immed_u16(ea, ~d & 0xffff, 0));
        } else {
            tc->emit( mov_immed_u16(ea, d, 0));
        }

        tc->emit( ldrsh_offset(ea, reg, 0));
    } else {
        /* Ra is a register, check if displacement can be used for store directly */
        if (d >= 0 && d <= 0x1ffe && (d & 1) == 0) {
            tc->emit( ldrsh_offset(base, reg, d));
        }
        else if (d >= -256 && d <= 255) {
            tc->emit( ldursh_offset(base, reg, d));
        }
        else {
            GPR ea = GPR::allocate();

            if (d < 0) {
                tc->emit( movn_immed_u16(ea, ~d & 0xffff, 0));
            }
            else {
                tc->emit( mov_immed_u16(ea, d, 0));
            }
            
            tc->emit( ldrsh_regoffset(base, reg, ea, SXTW, 0));
        }
    }

    tc->advancePC(4);

    return 1;
}

int lhau(PPCTranslatorContext *tc, uint32_t opcode)
{
    uint8_t rd = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    int16_t d = opcode & 0xffff;

    GPR base = tc->mapGPRForReadAndWrite(ra);
    GPR reg = tc->mapGPRForWrite(rd);

    /* Ra is a register, check if displacement can be used for load directly */
    if (d >= -256 && d <= 255) {
        tc->emit( ldrsh_offset_preindex(base, reg, d));
    }
    else if (d >= 0 && d <= 0xfff && (d & 1) == 0) {
        tc->emit({ 
            ldrsh_offset(base, reg, d),
            add_immed(base, base, d)
        });
    }
    else {
        GPR ea = GPR::allocate();

        if (d < 0) {
            tc->emit( movn_immed_u16(ea, ~d & 0xffff, 0));
        }
        else {
            tc->emit( mov_immed_u16(ea, d, 0));
        }
        
        tc->emit({ 
            ldrsh_regoffset(base, reg, ea, SXTW, 0),
            add_reg(base, base, ea, LSL, 0)
        });
    }

    tc->advancePC(4);

    return 1;
}

int lhax(PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 1) return -1;

    uint8_t rd = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;

    GPR reg_ra = ra != 0 ? tc->mapGPRForRead(ra) : GPR();
    GPR reg_rb = tc->mapGPRForRead(rb);
    GPR reg_rd = tc->mapGPRForWrite(rd);

    /* If Ra is 0, then address is the displacement, only */
    if (ra == 0) {
        tc->emit( ldrsh_offset(reg_rb, reg_rd, 0));
    } else {
        tc->emit( ldrsh_regoffset(reg_ra, reg_rd, reg_rb, SXTW, 0));
    }

    tc->advancePC(4);

    return 1;
}

int lhaux(PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 1) return -1;

    uint8_t rd = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;

    if (ra == 0 || rd == ra) return -1;

    GPR reg_rb = tc->mapGPRForRead(rb);
    GPR reg_ra = tc->mapGPRForReadAndWrite(ra);
    GPR reg_rd = tc->mapGPRForWrite(rd);

    tc->emit({ 
        ldrsh_regoffset(reg_ra, reg_rd, reg_rb, SXTW, 0),
        add_reg(reg_ra, reg_ra, reg_rb, LSL, 0)
    });

    tc->advancePC(4);

    return 1;
}

int lhz(PPCTranslatorContext *tc, uint32_t opcode)
{
    uint8_t rd = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    int16_t d = opcode & 0xffff;

    GPR reg = tc->mapGPRForRead(rd);

    /* If Ra is 0, then address is the displacement, only */
    if (ra == 0) {
        GPR ea = GPR::allocate();

        if (d < 0) {
            tc->emit( movn_immed_u16(ea, ~d & 0xffff, 0));
        } else {
            tc->emit( mov_immed_u16(ea, d, 0));
        }

        tc->emit( ldrh_offset(ea, reg, 0));
    } else {
        /* Ra is a register, check if displacement can be used for store directly */
        if (d >= 0 && d <= 0x1ffe && (d & 1) == 0) {
            GPR base = tc->mapGPRForRead(ra);
            tc->emit( ldrh_offset(base, reg, d));
        }
        else if (d >= -256 && d <= 255) {
            GPR base = tc->mapGPRForRead(ra);
            tc->emit( ldurh_offset(base, reg, d));
        }
        else {
            GPR ea = GPR::allocate();
            GPR base = tc->mapGPRForRead(ra);

            if (d < 0) {
                tc->emit( movn_immed_u16(ea, ~d & 0xffff, 0));
            }
            else {
                tc->emit( mov_immed_u16(ea, d, 0));
            }
            
            tc->emit( ldrh_regoffset(base, reg, ea, SXTW, 0));
        }
    }

    tc->advancePC(4);

    return 1;
}

int lhzx(PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 1) return -1;

    uint8_t rd = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;

    GPR reg_ra = ra != 0 ? tc->mapGPRForRead(ra) : GPR();
    GPR reg_rb = tc->mapGPRForRead(rb);
    GPR reg_rd = tc->mapGPRForWrite(rd);
    
    /* If Ra is 0, then address is the displacement, only */
    if (ra == 0) {
        tc->emit( ldrh_offset(reg_rb, reg_rd, 0));
    } else {
        tc->emit( ldrh_regoffset(reg_ra, reg_rd, reg_rb, SXTW, 0));
    }

    tc->advancePC(4);

    return 1;
}

int lhzu(PPCTranslatorContext *tc, uint32_t opcode)
{
    uint8_t rd = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    int16_t d = opcode & 0xffff;

    GPR base = tc->mapGPRForReadAndWrite(ra);
    GPR reg = tc->mapGPRForWrite(rd);

    /* Ra is a register, check if displacement can be used for load directly */
    if (d >= -256 && d <= 255) {
        tc->emit( ldrh_offset_preindex(base, reg, d));
    }
    else if (d >= 0 && d <= 0xfff && (d & 1) == 0) {
        tc->emit({ 
            ldrh_offset(base, reg, d),
            add_immed(base, base, d)
        });
    }
    else {
        GPR ea = GPR::allocate();

        if (d < 0) {
            tc->emit( movn_immed_u16(ea, ~d & 0xffff, 0));
        }
        else {
            tc->emit( mov_immed_u16(ea, d, 0));
        }
        
        tc->emit({ 
            ldrh_regoffset(base, reg, ea, SXTW, 0),
            add_reg(base, base, ea, LSL, 0)
        });
    }

    tc->advancePC(4);

    return 1;
}

int lhzux(PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 1) return -1;

    uint8_t rd = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;

    if (ra == 0 || ra == rd) return -1;

    GPR reg_rb = tc->mapGPRForRead(rb);
    GPR reg_ra = tc->mapGPRForReadAndWrite(ra);
    GPR reg_rd = tc->mapGPRForWrite(rd);
    
    tc->emit({ 
        ldrh_regoffset(reg_ra, reg_rd, reg_rb, SXTW, 0),
        add_reg(reg_ra, reg_ra, reg_rb, LSL, 0)
    });

    tc->advancePC(4);

    return 1;
}

int lhbrx(PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 1) return -1;

    uint8_t rd = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;

    GPR reg_ra = ra != 0 ? tc->mapGPRForRead(ra) : GPR();
    GPR reg_rb = tc->mapGPRForRead(rb);
    GPR reg_rd = tc->mapGPRForWrite(rd);
    
    /* If Ra is 0, then address is the displacement, only */
    if (ra == 0) {
        tc->emit( ldrh_offset(reg_rb, reg_rd, 0));
    } else {
        tc->emit( ldrh_regoffset(reg_ra, reg_rd, reg_rb, SXTW, 0));
    }

    tc->emit( rev16(reg_rd, reg_rd));

    tc->advancePC(4);

    return 1;
}

int lwz(PPCTranslatorContext *tc, uint32_t opcode)
{
    uint8_t rd = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    int16_t d = opcode & 0xffff;

    GPR base = ra != 0 ? tc->mapGPRForRead(ra) : GPR();
    GPR reg = tc->mapGPRForWrite(rd);

    /* If Ra is 0, then address is the displacement, only */
    if (ra == 0) {
        GPR ea = GPR::allocate();

        if (d < 0) {
            tc->emit( movn_immed_u16(ea, ~d & 0xffff, 0));
        } else {
            tc->emit( mov_immed_u16(ea, d, 0));
        }

        tc->emit( ldr_offset(ea, reg, 0));
    } else {
        /* Ra is a register, check if displacement can be used for store directly */
        if (d >= 0 && d <= 0x3ffc && (d & 3) == 0) {
            tc->emit( ldr_offset(base, reg, d));
        }
        else if (d >= -256 && d <= 255) {
            tc->emit( ldur_offset(base, reg, d));
        }
        else {
            GPR ea = GPR::allocate();

            if (d < 0) {
                tc->emit( movn_immed_u16(ea, ~d & 0xffff, 0));
            }
            else {
                tc->emit( mov_immed_u16(ea, d, 0));
            }
            
            tc->emit( ldr_regoffset(base, reg, ea, SXTW, 0));
        }
    }

    tc->advancePC(4);

    return 1;
}

int lwzu(PPCTranslatorContext *tc, uint32_t opcode)
{
    uint8_t rd = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    int16_t d = opcode & 0xffff;

    GPR base = tc->mapGPRForReadAndWrite(ra);
    GPR reg = tc->mapGPRForWrite(rd);

    /* Ra is a register, check if displacement can be used for load directly */
    if (d >= -256 && d <= 255) {
        tc->emit( ldr_offset_preindex(base, reg, d));
    }
    else if (d >= 0 && d <= 0xfff && (d & 3) == 0) {
        tc->emit({ 
            ldr_offset(base, reg, d),
            add_immed(base, base, d)
        });
    }
    else {
        GPR ea = GPR::allocate();

        if (d < 0) {
            tc->emit( movn_immed_u16(ea, ~d & 0xffff, 0));
        }
        else {
            tc->emit( mov_immed_u16(ea, d, 0));
        }
        
        tc->emit({ 
            ldr_regoffset(base, reg, ea, SXTW, 0),
            add_reg(base, base, ea, LSL, 0)
        });
    }

    tc->advancePC(4);

    return 1;
}

int lwzx(PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 1) return -1;

    uint8_t rd = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;

    GPR reg_ra = ra != 0 ? tc->mapGPRForRead(ra) : GPR();
    GPR reg_rb = tc->mapGPRForRead(rb);
    GPR reg_rd = tc->mapGPRForWrite(rd);

    /* If Ra is 0, then address is the displacement, only */
    if (ra == 0) {
        tc->emit( ldr_offset(reg_rb, reg_rd, 0));
    } else {
        tc->emit( ldr_regoffset(reg_ra, reg_rd, reg_rb, SXTW, 0));
    }

    tc->advancePC(4);

    return 1;
}

int lwzux(PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 1) return -1;

    uint8_t rd = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;

    if (ra == 0 || ra == rd) return -1;

    GPR reg_rb = tc->mapGPRForRead(rb);
    GPR reg_ra = tc->mapGPRForReadAndWrite(ra);
    GPR reg_rd = tc->mapGPRForWrite(rd);

    /* If Ra is 0, then address is the displacement, only */
    
    tc->emit({ 
        ldr_regoffset(reg_ra, reg_rd, reg_rb, SXTW, 0),
        add_reg(reg_ra, reg_ra, reg_rb, LSL, 0)
    });

    tc->advancePC(4);

    return 1;
}

int lwbrx(PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 1) return -1;

    uint8_t rd = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;

    GPR reg_ra = ra != 0 ? tc->mapGPRForRead(ra) : GPR();
    GPR reg_rb = tc->mapGPRForRead(rb);
    GPR reg_rd = tc->mapGPRForWrite(rd);

    /* If Ra is 0, then address is the displacement, only */
    if (ra == 0) {
        tc->emit( ldr_offset(reg_rb, reg_rd, 0));
    } else {
        tc->emit( ldr_regoffset(reg_ra, reg_rd, reg_rb, SXTW, 0));
    }

    tc->emit( rev(reg_rd, reg_rd));

    tc->advancePC(4);

    return 1;
}

int lwarx(PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 1) return -1;

    uint8_t rd = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;

    GPR reg_ra = ra != 0 ? tc->mapGPRForRead(ra) : GPR();
    GPR reg_rb = tc->mapGPRForRead(rb);
    GPR reg_rd = tc->mapGPRForWrite(rd);
    GPR base(reg_rb.get());

    /* If Ra is 0, then address is the displacement, only */
    if (ra != 0) {
        base = GPR::allocate();
        tc->emit(add_reg(base, reg_rb, reg_ra, LSL, 0));
    }

    tc->emit(ldxr(base, reg_rd));

    tc->advancePC(4);

    return 1;
}

/* FPU loads and stores */

/* FPU register frS at [addr], single precision is converted from and to double */
static void transferFPR(PPCTranslatorContext *tc, uint8_t fr, uint8_t addr, bool store, bool single)
{
    if (store) {
        FPR reg = tc->mapFPRForRead(fr);

        if (single) {
            FPR tmp = FPR::allocate();

            tc->emit({
                fcvtsd(tmp, reg),
                fsts(tmp, addr, 0)
            });
        } else {
            tc->emit(fstd(reg, addr, 0));
        }
    } else {
        FPR reg = tc->mapFPRForWrite(fr);

        if (single) {
            tc->emit({
                flds(reg, addr, 0),
                fcvtds(reg, reg)
            });
        } else {
            tc->emit(fldd(reg, addr, 0));
        }
    }
}

/* lfsu, lfdu, stfsu, stfdu: rA = rA + d, then the access at rA */
static int loadStoreFPRUpdate(PPCTranslatorContext *tc, Opcode opcode, bool store, bool single)
{
    uint8_t fr = opcode.u8(6, 10);
    uint8_t ra = opcode.u8(11, 15);
    int16_t d = opcode.i16(16, 31);

    /* Invalid form */
    if (ra == 0) return -1;

    GPR base = tc->mapGPRForReadAndWrite(ra);

    addDisplacement(tc, base, base, d);
    transferFPR(tc, fr, base, store, single);

    tc->advancePC(4);

    return 1;
}

/* FPU X forms: ea = (rA|0) + rB, update forms write ea to rA */
static int loadStoreFPRIndexed(PPCTranslatorContext *tc, Opcode opcode, bool store, bool single, bool update)
{
    uint8_t fr = opcode.u8(6, 10);
    uint8_t ra = opcode.u8(11, 15);
    uint8_t rb = opcode.u8(16, 20);

    /* Invalid form */
    if (update && ra == 0) return -1;

    GPR reg_rb = tc->mapGPRForRead(rb);

    if (ra == 0) {
        transferFPR(tc, fr, reg_rb, store, single);
    } else if (update) {
        GPR reg_ra = tc->mapGPRForReadAndWrite(ra);

        tc->emit( add_reg(reg_ra, reg_ra, reg_rb, LSL, 0));
        transferFPR(tc, fr, reg_ra, store, single);
    } else {
        GPR reg_ra = tc->mapGPRForRead(ra);
        GPR ea = GPR::allocate();

        tc->emit( add_reg(ea, reg_ra, reg_rb, LSL, 0));
        transferFPR(tc, fr, ea, store, single);
    }

    tc->advancePC(4);

    return 1;
}

int lfsu(PPCTranslatorContext *tc, uint32_t opcode)
{
    return loadStoreFPRUpdate(tc, opcode, false, true);
}

int lfdu(PPCTranslatorContext *tc, uint32_t opcode)
{
    return loadStoreFPRUpdate(tc, opcode, false, false);
}

int stfsu(PPCTranslatorContext *tc, uint32_t opcode)
{
    return loadStoreFPRUpdate(tc, opcode, true, true);
}

int stfdu(PPCTranslatorContext *tc, uint32_t opcode)
{
    return loadStoreFPRUpdate(tc, opcode, true, false);
}

int lfsx(PPCTranslatorContext *tc, uint32_t opcode)
{
    return loadStoreFPRIndexed(tc, opcode, false, true, false);
}

int lfsux(PPCTranslatorContext *tc, uint32_t opcode)
{
    return loadStoreFPRIndexed(tc, opcode, false, true, true);
}

int lfdx(PPCTranslatorContext *tc, uint32_t opcode)
{
    return loadStoreFPRIndexed(tc, opcode, false, false, false);
}

int lfdux(PPCTranslatorContext *tc, uint32_t opcode)
{
    return loadStoreFPRIndexed(tc, opcode, false, false, true);
}

int stfsx(PPCTranslatorContext *tc, uint32_t opcode)
{
    return loadStoreFPRIndexed(tc, opcode, true, true, false);
}

int stfsux(PPCTranslatorContext *tc, uint32_t opcode)
{
    return loadStoreFPRIndexed(tc, opcode, true, true, true);
}

int stfdx(PPCTranslatorContext *tc, uint32_t opcode)
{
    return loadStoreFPRIndexed(tc, opcode, true, false, false);
}

int stfdux(PPCTranslatorContext *tc, uint32_t opcode)
{
    return loadStoreFPRIndexed(tc, opcode, true, false, true);
}

int stfd(PPCTranslatorContext *tc, uint32_t opcode)
{
    uint8_t rs = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    int16_t d = opcode & 0xffff;

    FPR reg = tc->mapFPRForRead(rs);

    /* If Ra is 0, then address is the displacement, only */
    if (ra == 0) {
        GPR ea = GPR::allocate();
        if (d < 0) {
            tc->emit( movn_immed_u16(ea, ~d & 0xffff, 0));
        } else {
            tc->emit( mov_immed_u16(ea, d, 0));
        }

        tc->emit(fstd(reg, ea, 0));
    } else {
        /* Ra is a register, check if displacement can be used for store directly */
        if (d >= 0 && d <= 0x3ffc && (d & 7) == 0) {
            GPR base = tc->mapGPRForRead(ra);
            tc->emit(fstd_pimm(reg, base, d >> 3));
        }
        else if (d >= -256 && d <= 255) {
            GPR base = tc->mapGPRForRead(ra);
            tc->emit(fstd(reg, base, d));
        }
        else {
            GPR ea = GPR::allocate();
            GPR base = tc->mapGPRForRead(ra);

            if (d < 0) {
                tc->emit(movn_immed_u16(ea, ~d & 0xffff, 0));
            }
            else {
                tc->emit(mov_immed_u16(ea, d, 0));
            }
            
            tc->emit({
                add_reg(ea, ea, base, LSL, 0),
                fstd(reg, ea, 0)
            });
        }
    }

    tc->advancePC(4);

    return 1;
}

int stfs(PPCTranslatorContext *tc, uint32_t opcode)
{
    uint8_t rs = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    int16_t d = opcode & 0xffff;

    FPR reg = tc->mapFPRForRead(rs);
    FPR tmp = FPR::allocate();
    tc->emit(fcvtsd(tmp, reg));

    /* If Ra is 0, then address is the displacement, only */
    if (ra == 0) {
        GPR ea = GPR::allocate();

        if (d < 0) {
            tc->emit( movn_immed_u16(ea, ~d & 0xffff, 0));
        } else {
            tc->emit( mov_immed_u16(ea, d, 0));
        }

        tc->emit(fsts(tmp, ea, 0));
    } else {
        /* Ra is a register, check if displacement can be used for store directly */
        if (d >= 0 && d <= 0x3ffc && (d & 3) == 0) {
            GPR base = tc->mapGPRForRead(ra);
            tc->emit(fsts_pimm(tmp, base, d >> 2));
        }
        else if (d >= -256 && d <= 255) {
            GPR base = tc->mapGPRForRead(ra);
            tc->emit(fsts(tmp, base, d));
        }
        else {
            GPR ea = GPR::allocate();
            GPR base = tc->mapGPRForRead(ra);

            if (d < 0) {
                tc->emit(movn_immed_u16(ea, ~d & 0xffff, 0));
            }
            else {
                tc->emit(mov_immed_u16(ea, d, 0));
            }
            
            tc->emit({
                add_reg(ea, ea, base, LSL, 0),
                fsts(tmp, ea, 0)
            });
        }
    }

    tc->advancePC(4);

    return 1;
}

int stfiwx(PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 1) return -1;

    uint8_t rs = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;

    FPR reg_rs = tc->mapFPRForRead(rs);
    GPR reg_rb = tc->mapGPRForRead(rb);
    
    /* If Ra is 0, then address is the displacement, only */
    if (ra == 0) {
        tc->emit(fsts(reg_rs, reg_rb, 0));
    } else {
        GPR reg_ra = tc->mapGPRForRead(ra);
        tc->emit(fsts_regoffset(reg_rs, reg_ra, reg_rb, SXTW, 0));
    }

    tc->advancePC(4);

    return 1;
}

int lfd(PPCTranslatorContext *tc, uint32_t opcode)
{
    uint8_t rd = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    int16_t d = opcode & 0xffff;

    GPR base = ra != 0 ? tc->mapGPRForRead(ra) : GPR();
    FPR reg = tc->mapFPRForWrite(rd);

    /* If Ra is 0, then address is the displacement, only */
    if (ra == 0) {
        GPR ea = GPR::allocate();

        if (d < 0) {
            tc->emit( movn_immed_u16(ea, ~d & 0xffff, 0));
        } else {
            tc->emit( mov_immed_u16(ea, d, 0));
        }

        tc->emit(fldd(reg, ea, 0));
    } else {
        /* Ra is a register, check if displacement can be used for store directly */
        if (d >= 0 && d <= 0x3ffc && (d & 7) == 0) {
            tc->emit(fldd_pimm(reg, base, d >> 3));
        }
        else if (d >= -256 && d <= 255) {
            tc->emit(fldd(reg, base, d));
        }
        else {
            GPR ea = GPR::allocate();

            if (d < 0) {
                tc->emit(movn_immed_u16(ea, ~d & 0xffff, 0));
            }
            else {
                tc->emit(mov_immed_u16(ea, d, 0));
            }
            
            tc->emit({
                add_reg(ea, ea, base, LSL, 0),
                fldd(reg, ea, 0)
            });
        }
    }

    tc->advancePC(4);

    return 1;
}

int lfs(PPCTranslatorContext *tc, uint32_t opcode)
{
    uint8_t rd = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    int16_t d = opcode & 0xffff;

    GPR base = ra != 0 ? tc->mapGPRForRead(ra) : GPR();
    FPR reg = tc->mapFPRForWrite(rd);

    /* If Ra is 0, then address is the displacement, only */
    if (ra == 0) {
        GPR ea = GPR::allocate();

        if (d < 0) {
            tc->emit( movn_immed_u16(ea, ~d & 0xffff, 0));
        } else {
            tc->emit( mov_immed_u16(ea, d, 0));
        }

        tc->emit(flds(reg, ea, 0));
    } else {
        /* Ra is a register, check if displacement can be used for store directly */
        if (d >= 0 && d <= 0x3ffc && (d & 7) == 0) {
            tc->emit(flds_pimm(reg, base, d >> 2));
        }
        else if (d >= -256 && d <= 255) {
            tc->emit(flds(reg, base, d));
        }
        else {
            GPR ea = GPR::allocate();

            if (d < 0) {
                tc->emit(movn_immed_u16(ea, ~d & 0xffff, 0));
            }
            else {
                tc->emit(mov_immed_u16(ea, d, 0));
            }
            
            tc->emit({
                add_reg(ea, ea, base, LSL, 0),
                flds(reg, ea, 0)
            });
        }
    }

    tc->emit(fcvtds(reg, reg));

    tc->advancePC(4);

    return 1;
}

}
