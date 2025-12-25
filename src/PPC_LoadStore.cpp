#define restrict __restrict__

#include "config.h"
#include "PPC.h"
#include "A64.h"

namespace Emu68::PPC {

int EMIT_stb(PPCTranslatorContext *tc, uint32_t opcode)
{
    uint8_t rs = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    int16_t d = opcode & 0xffff;

    uint8_t reg = MapGPRForRead(tc, rs);

    /* If Ra is 0, then address is the displacement, only */
    if (ra == 0) {
        uint8_t ea = AllocARMRegister(tc);

        if (d < 0) {
            tc->emit( movn_immed_u16(ea, ~d & 0xffff, 0));
        } else {
            tc->emit( mov_immed_u16(ea, d, 0));
        }

        tc->emit( strb_offset(ea, reg, 0));

        FreeARMRegister(tc, ea);
    } else {
        uint8_t base = MapGPRForRead(tc, ra);
        /* Ra is a register, check if displacement can be used for store directly */
        if (d >= 0 && d <= 0xfff) {
            tc->emit( strb_offset(base, reg, d));
        } else if (d >= -256 && d < 255) {
            tc->emit( sturb_offset(base, reg, d));
        }
        else {
            uint8_t ea = AllocARMRegister(tc);

            if (d < 0) {
                tc->emit( movn_immed_u16(ea, ~d & 0xffff, 0));
            }
            else {
                tc->emit( mov_immed_u16(ea, d, 0));
            }
            
            tc->emit( strb_regoffset(base, reg, ea, SXTX));

            FreeARMRegister(tc, ea);
        }
    }

    tc->tc_PPCCodePtr++;
    tc->advancePC(4);
    return 1;
}

int EMIT_stbu(PPCTranslatorContext *tc, uint32_t opcode)
{
    uint8_t rs = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    int16_t d = opcode & 0xffff;

    /* It is illegal to have Ra = 0 */
    if (ra == 0) {
        return -1;
    }

    uint8_t reg = MapGPRForRead(tc, rs);
    uint8_t base = MapGPRForReadAndWrite(tc, ra);

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
        uint8_t ea = AllocARMRegister(tc);

        if (d < 0) {
            tc->emit( movn_immed_u16(ea, ~d & 0xffff, 0));
        }
        else {
            tc->emit( mov_immed_u16(ea, d, 0));
        }
        
        tc->emit({ 
            strb_regoffset(base, reg, ea, SXTX),
            add_reg(base, base, ea, LSL, 0)
        });

        FreeARMRegister(tc, ea);
    }

    tc->tc_PPCCodePtr++;
    tc->advancePC(4);
    return 1;
}

int EMIT_sth(PPCTranslatorContext *tc, uint32_t opcode)
{
    uint8_t rs = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    int16_t d = opcode & 0xffff;

    uint8_t reg = MapGPRForRead(tc, rs);

    /* If Ra is 0, then address is the displacement, only */
    if (ra == 0) {
        uint8_t ea = AllocARMRegister(tc);

        if (d < 0) {
            tc->emit( movn_immed_u16(ea, ~d & 0xffff, 0));
        } else {
            tc->emit( mov_immed_u16(ea, d, 0));
        }

        tc->emit( strh_offset(ea, reg, 0));

        FreeARMRegister(tc, ea);
    } else {
        /* Ra is a register, check if displacement can be used for store directly */
        if (d >= 0 && d <= 0x1ffe && (d & 1) == 0) {
            uint8_t base = MapGPRForRead(tc, ra);
            tc->emit( strh_offset(base, reg, d));
        }
        else if (d >= -256 && d <= 255) {
            uint8_t base = MapGPRForRead(tc, ra);
            tc->emit( sturh_offset(base, reg, d));
        }
        else {
            uint8_t ea = AllocARMRegister(tc);
            uint8_t base = MapGPRForRead(tc, ra);

            if (d < 0) {
                tc->emit( movn_immed_u16(ea, ~d & 0xffff, 0));
            }
            else {
                tc->emit( mov_immed_u16(ea, d, 0));
            }
            
            tc->emit( strh_regoffset(base, reg, ea, SXTX, 0));

            FreeARMRegister(tc, ea);
        }
    }

    tc->tc_PPCCodePtr++;
    tc->advancePC(4);
    return 1;
}

int EMIT_stbx(PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 1) return -1;

    uint8_t rs = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;

    uint8_t reg_rd = MapGPRForRead(tc, rs);
    uint8_t reg_rb = MapGPRForRead(tc, rb);
    
    /* If Ra is 0, then address is the displacement, only */
    if (ra == 0) {
        tc->emit( strb_offset(reg_rb, reg_rd, 0));
    } else {
        uint8_t reg_ra = MapGPRForRead(tc, ra);
        tc->emit( strb_regoffset(reg_ra, reg_rd, reg_rb, SXTX));
    }

    tc->tc_PPCCodePtr++;
    tc->advancePC(4);
    return 1;
}

int EMIT_stbux(PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 1) return -1;

    uint8_t rs = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;

    if (ra == 0 || ra == rs) return -1;

    uint8_t reg_rd = MapGPRForRead(tc, rs);
    uint8_t reg_rb = MapGPRForRead(tc, rb);
    uint8_t reg_ra = MapGPRForReadAndWrite(tc, ra);

    tc->emit({ 
        strb_regoffset(reg_ra, reg_rd, reg_rb, SXTX),
        add_reg(reg_ra, reg_ra, reg_rb, LSL, 0)
    });

    tc->tc_PPCCodePtr++;
    tc->advancePC(4);
    return 1;
}

int EMIT_sthx(PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 1) return -1;

    uint8_t rs = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;

    uint8_t reg_rd = MapGPRForRead(tc, rs);
    uint8_t reg_rb = MapGPRForRead(tc, rb);
    
    /* If Ra is 0, then address is the displacement, only */
    if (ra == 0) {
        tc->emit( strh_offset(reg_rb, reg_rd, 0));
    } else {
        uint8_t reg_ra = MapGPRForRead(tc, ra);
        tc->emit( strh_regoffset(reg_ra, reg_rd, reg_rb, SXTX, 0));
    }

    tc->tc_PPCCodePtr++;
    tc->advancePC(4);
    return 1;
}

int EMIT_sthux(PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 1) return -1;

    uint8_t rs = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;

    if (ra == 0 || ra == rs) return -1;

    uint8_t reg_rd = MapGPRForRead(tc, rs);
    uint8_t reg_rb = MapGPRForRead(tc, rb);
    uint8_t reg_ra = MapGPRForReadAndWrite(tc, ra);

    tc->emit({ 
        strh_regoffset(reg_ra, reg_rd, reg_rb, SXTX, 0),
        add_reg(reg_ra, reg_ra, reg_rb, LSL, 0)
    });

    tc->tc_PPCCodePtr++;
    tc->advancePC(4);
    return 1;
}

int EMIT_stwx(PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 1) return -1;

    uint8_t rs = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;

    uint8_t reg_rd = MapGPRForRead(tc, rs);
    uint8_t reg_rb = MapGPRForRead(tc, rb);
    
    /* If Ra is 0, then address is the displacement, only */
    if (ra == 0) {
        tc->emit( str_offset(reg_rb, reg_rd, 0));
    } else {
        uint8_t reg_ra = MapGPRForRead(tc, ra);
        tc->emit( str_regoffset(reg_ra, reg_rd, reg_rb, SXTX, 0));
    }

    tc->tc_PPCCodePtr++;
    tc->advancePC(4);
    return 1;
}

int EMIT_stwux(PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 1) return -1;

    uint8_t rs = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;

    if (ra == 0 || ra == rs) return -1;

    uint8_t reg_rd = MapGPRForRead(tc, rs);
    uint8_t reg_rb = MapGPRForRead(tc, rb);
    uint8_t reg_ra = MapGPRForReadAndWrite(tc, ra);

    tc->emit({ 
        str_regoffset(reg_ra, reg_rd, reg_rb, SXTX, 0),
        add_reg(reg_ra, reg_ra, reg_rb, LSL, 0)
    });

    tc->tc_PPCCodePtr++;
    tc->advancePC(4);
    return 1;
}

int EMIT_stw(PPCTranslatorContext *tc, uint32_t opcode)
{
    uint8_t rs = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    int16_t d = opcode & 0xffff;

    uint8_t reg = MapGPRForRead(tc, rs);

    /* If Ra is 0, then address is the displacement, only */
    if (ra == 0) {
        uint8_t ea = AllocARMRegister(tc);

        if (d < 0) {
            tc->emit( movn_immed_u16(ea, ~d & 0xffff, 0));
        } else {
            tc->emit( mov_immed_u16(ea, d, 0));
        }

        tc->emit( str_offset(ea, reg, 0));

        FreeARMRegister(tc, ea);
    } else {
        /* Ra is a register, check if displacement can be used for store directly */
        if (d >= 0 && d <= 0x3ffc && (d & 3) == 0) {
            uint8_t base = MapGPRForRead(tc, ra);
            tc->emit( str_offset(base, reg, d));
        }
        else if (d >= -256 && d <= 255) {
            uint8_t base = MapGPRForRead(tc, ra);
            tc->emit( stur_offset(base, reg, d));
        }
        else {
            uint8_t ea = AllocARMRegister(tc);
            uint8_t base = MapGPRForRead(tc, ra);

            if (d < 0) {
                tc->emit( movn_immed_u16(ea, ~d & 0xffff, 0));
            }
            else {
                tc->emit( mov_immed_u16(ea, d, 0));
            }
            
            tc->emit( str_regoffset(base, reg, ea, SXTX, 0));

            FreeARMRegister(tc, ea);
        }
    }

    tc->tc_PPCCodePtr++;
    tc->advancePC(4);
    return 1;
}

int EMIT_stwu(PPCTranslatorContext *tc, uint32_t opcode)
{
    uint8_t rs = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    int16_t d = opcode & 0xffff;

    /* It is illegal to have Ra = 0 */
    if (ra == 0) {
        return -1;
    }

    uint8_t reg = MapGPRForRead(tc, rs);
    uint8_t base = MapGPRForReadAndWrite(tc, ra);

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
        uint8_t ea = AllocARMRegister(tc);

        if (d < 0) {
            tc->emit( movn_immed_u16(ea, ~d & 0xffff, 0));
        }
        else {
            tc->emit( mov_immed_u16(ea, d, 0));
        }
        
        tc->emit({ 
            str_regoffset(base, reg, ea, SXTX, 0),
            add_reg(base, base, ea, LSL, 0)
        });

        FreeARMRegister(tc, ea);
    }

    tc->tc_PPCCodePtr++;
    tc->advancePC(4);
    return 1;
}

int EMIT_sthu(PPCTranslatorContext *tc, uint32_t opcode)
{
    uint8_t rs = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    int16_t d = opcode & 0xffff;

    /* It is illegal to have Ra = 0 */
    if (ra == 0) {
        return -1;
    }

    uint8_t reg = MapGPRForRead(tc, rs);
    uint8_t base = MapGPRForReadAndWrite(tc, ra);

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
        uint8_t ea = AllocARMRegister(tc);

        if (d < 0) {
            tc->emit( movn_immed_u16(ea, ~d & 0xffff, 0));
        }
        else {
            tc->emit( mov_immed_u16(ea, d, 0));
        }
        
        tc->emit({ 
            strh_regoffset(base, reg, ea, SXTX, 0),
            add_reg(base, base, ea, LSL, 0)
        });

        FreeARMRegister(tc, ea);
    }

    tc->tc_PPCCodePtr++;
    tc->advancePC(4);
    return 1;
}

int EMIT_stwcx_dot(PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if ((opcode & 1) == 0) return -1;

    uint8_t rs = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;

    uint8_t reg_ra = ra != 0 ? MapGPRForRead(tc, ra) : 0xff;
    uint8_t reg_rb = MapGPRForRead(tc, rb);
    uint8_t reg_rs = MapGPRForWrite(tc, rs);
    uint8_t reg_cr = MapGPRForReadAndWrite(tc, CRn);
    uint8_t tmp = AllocARMRegister(tc);
    uint8_t base = reg_rb;

    /* If Ra is 0, then address is the displacement, only */
    if (ra != 0) {
        base = AllocARMRegister(tc);
        tc->emit(add_reg(base, reg_rb, reg_ra, LSL, 0));
    }

    tc->emit({
        stlxr(base, reg_rs, tmp),
        cmp_immed(tmp, 0),
        bic_immed(reg_cr, reg_cr, 1, 3),
        orr_immed(tmp, reg_cr, 1, 3),
        csel(reg_cr, tmp, reg_cr, A64_CC_EQ)
    });

    if (ra != 0) {
        FreeARMRegister(tc, base);
    }

    FreeARMRegister(tc, tmp);

    tc->tc_PPCCodePtr++;
    tc->advancePC(4);
    return 1;
}

int EMIT_sthbrx(PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 1) return -1;

    uint8_t rs = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;

    uint8_t reg_rs = MapGPRForRead(tc, rs);
    uint8_t reg_rb = MapGPRForRead(tc, rb);
    uint8_t tmp = AllocARMRegister(tc);
    
    tc->emit( rev16(tmp, reg_rs));

    /* If Ra is 0, then address is the displacement, only */
    if (ra == 0) {
        tc->emit( strh_offset(reg_rb, tmp, 0));
    } else {
        uint8_t reg_ra = MapGPRForRead(tc, ra);
        tc->emit( strh_regoffset(reg_ra, tmp, reg_rb, SXTX, 0));
    }

    FreeARMRegister(tc, tmp);

    tc->tc_PPCCodePtr++;
    tc->advancePC(4);
    return 1;
}

int EMIT_stwbrx(PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 1) return -1;

    uint8_t rs = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;

    uint8_t reg_rs = MapGPRForRead(tc, rs);
    uint8_t reg_rb = MapGPRForRead(tc, rb);
    uint8_t tmp = AllocARMRegister(tc);
    
    tc->emit( rev(tmp, reg_rs));

    /* If Ra is 0, then address is the displacement, only */
    if (ra == 0) {
        tc->emit( str_offset(reg_rb, tmp, 0));
    } else {
        uint8_t reg_ra = MapGPRForRead(tc, ra);
        tc->emit( str_regoffset(reg_ra, tmp, reg_rb, SXTX, 0));
    }

    FreeARMRegister(tc, tmp);

    tc->tc_PPCCodePtr++;
    tc->advancePC(4);
    return 1;
}

int EMIT_lbz(PPCTranslatorContext *tc, uint32_t opcode)
{
    uint8_t rd = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    int16_t d = opcode & 0xffff;

    uint8_t base = ra != 0 ? MapGPRForRead(tc, ra) : 0xff;
    uint8_t reg = MapGPRForWrite(tc, rd);

    /* If Ra is 0, then address is the displacement, only */
    if (ra == 0) {
        uint8_t ea = AllocARMRegister(tc);

        if (d < 0) {
            tc->emit( movn_immed_u16(ea, ~d & 0xffff, 0));
        } else {
            tc->emit( mov_immed_u16(ea, d, 0));
        }

        tc->emit( ldrb_offset(ea, reg, 0));

        FreeARMRegister(tc, ea);
    } else {
        /* Ra is a register, check if displacement can be used for store directly */
        if (d >= 0 && d <= 0xfff) {
            tc->emit( ldrb_offset(base, reg, d));
        }
        else if (d >= -256 && d <= 255) {
            tc->emit( ldurb_offset(base, reg, d));
        }
        else {
            uint8_t ea = AllocARMRegister(tc);

            if (d < 0) {
                tc->emit( movn_immed_u16(ea, ~d & 0xffff, 0));
            }
            else {
                tc->emit( mov_immed_u16(ea, d, 0));
            }
            
            tc->emit( ldrb_regoffset(base, reg, ea, SXTX));

            FreeARMRegister(tc, ea);
        }
    }

    tc->tc_PPCCodePtr++;
    tc->advancePC(4);
    return 1;
}

int EMIT_lbzu(PPCTranslatorContext *tc, uint32_t opcode)
{
    uint8_t rd = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    int16_t d = opcode & 0xffff;

    uint8_t base = MapGPRForReadAndWrite(tc, ra);
    uint8_t reg = MapGPRForWrite(tc, rd);

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
        uint8_t ea = AllocARMRegister(tc);

        if (d < 0) {
            tc->emit( movn_immed_u16(ea, ~d & 0xffff, 0));
        }
        else {
            tc->emit( mov_immed_u16(ea, d, 0));
        }
        
        tc->emit({ 
            ldrb_regoffset(base, reg, ea, SXTX),
            add_reg(base, base, ea, LSL, 0)
        });

        FreeARMRegister(tc, ea);
    }

    tc->tc_PPCCodePtr++;
    tc->advancePC(4);
    return 1;
}

int EMIT_lbzux(PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 1) return -1;

    uint8_t rd = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;

    if (ra == 0 || ra == rd) return -1;

    uint8_t reg_rb = MapGPRForRead(tc, rb);
    uint8_t reg_ra = MapGPRForReadAndWrite(tc, ra);
    uint8_t reg_rd = MapGPRForWrite(tc, rd);

    tc->emit({ 
        ldrb_regoffset(reg_ra, reg_rd, reg_rb, SXTX),
        add_reg(reg_ra, reg_ra, reg_rb, LSL, 0)
    });

    tc->tc_PPCCodePtr++;
    tc->advancePC(4);
    return 1;
}

int EMIT_lbzx(PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 1) return -1;

    uint8_t rd = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;

    uint8_t reg_ra = ra != 0 ? MapGPRForRead(tc, ra) : 0xff;
    uint8_t reg_rb = MapGPRForRead(tc, rb);
    uint8_t reg_rd = MapGPRForWrite(tc, rd);
    
    /* If Ra is 0, then address is the displacement, only */
    if (ra == 0) {
        tc->emit( ldrb_offset(reg_rb, reg_rd, 0));
    } else {
        tc->emit( ldrb_regoffset(reg_ra, reg_rd, reg_rb, SXTX));
    }

    tc->tc_PPCCodePtr++;
    tc->advancePC(4);
    return 1;
}

int EMIT_lha(PPCTranslatorContext *tc, uint32_t opcode)
{
    uint8_t rd = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    int16_t d = opcode & 0xffff;

    uint8_t base = ra != 0 ? MapGPRForRead(tc, ra) : 0xff;
    uint8_t reg = MapGPRForWrite(tc, rd);

    /* If Ra is 0, then address is the displacement, only */
    if (ra == 0) {
        uint8_t ea = AllocARMRegister(tc);

        if (d < 0) {
            tc->emit( movn_immed_u16(ea, ~d & 0xffff, 0));
        } else {
            tc->emit( mov_immed_u16(ea, d, 0));
        }

        tc->emit( ldrsh_offset(ea, reg, 0));

        FreeARMRegister(tc, ea);
    } else {
        /* Ra is a register, check if displacement can be used for store directly */
        if (d >= 0 && d <= 0x1ffe && (d & 1) == 0) {
            tc->emit( ldrsh_offset(base, reg, d));
        }
        else if (d >= -256 && d <= 255) {
            tc->emit( ldursh_offset(base, reg, d));
        }
        else {
            uint8_t ea = AllocARMRegister(tc);

            if (d < 0) {
                tc->emit( movn_immed_u16(ea, ~d & 0xffff, 0));
            }
            else {
                tc->emit( mov_immed_u16(ea, d, 0));
            }
            
            tc->emit( ldrsh_regoffset(base, reg, ea, SXTX, 0));

            FreeARMRegister(tc, ea);
        }
    }

    tc->tc_PPCCodePtr++;
    tc->advancePC(4);
    return 1;
}

int EMIT_lhau(PPCTranslatorContext *tc, uint32_t opcode)
{
    uint8_t rd = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    int16_t d = opcode & 0xffff;

    uint8_t base = MapGPRForReadAndWrite(tc, ra);
    uint8_t reg = MapGPRForWrite(tc, rd);

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
        uint8_t ea = AllocARMRegister(tc);

        if (d < 0) {
            tc->emit( movn_immed_u16(ea, ~d & 0xffff, 0));
        }
        else {
            tc->emit( mov_immed_u16(ea, d, 0));
        }
        
        tc->emit({ 
            ldrsh_regoffset(base, reg, ea, SXTX, 0),
            add_reg(base, base, ea, LSL, 0)
        });

        FreeARMRegister(tc, ea);
    }

    tc->tc_PPCCodePtr++;
    tc->advancePC(4);
    return 1;
}

int EMIT_lhax(PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 1) return -1;

    uint8_t rd = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;

    uint8_t reg_ra = ra != 0 ? MapGPRForRead(tc, ra) : 0xff;
    uint8_t reg_rb = MapGPRForRead(tc, rb);
    uint8_t reg_rd = MapGPRForWrite(tc, rd);
    
    /* If Ra is 0, then address is the displacement, only */
    if (ra == 0) {
        tc->emit( ldrsh_offset(reg_rb, reg_rd, 0));
    } else {
        tc->emit( ldrsh_regoffset(reg_ra, reg_rd, reg_rb, SXTX, 0));
    }

    tc->tc_PPCCodePtr++;
    tc->advancePC(4);
    return 1;
}

int EMIT_lhaux(PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 1) return -1;

    uint8_t rd = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;

    if (ra == 0 || rd == ra) return -1;

    uint8_t reg_rb = MapGPRForRead(tc, rb);
    uint8_t reg_ra = MapGPRForReadAndWrite(tc, ra);
    uint8_t reg_rd = MapGPRForWrite(tc, rd);

    tc->emit({ 
        ldrsh_regoffset(reg_ra, reg_rd, reg_rb, SXTX, 0),
        add_reg(reg_ra, reg_ra, reg_rb, LSL, 0)
    });

    tc->tc_PPCCodePtr++;
    tc->advancePC(4);
    return 1;
}

int EMIT_lhz(PPCTranslatorContext *tc, uint32_t opcode)
{
    uint8_t rd = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    int16_t d = opcode & 0xffff;

    uint8_t reg = MapGPRForRead(tc, rd);

    /* If Ra is 0, then address is the displacement, only */
    if (ra == 0) {
        uint8_t ea = AllocARMRegister(tc);

        if (d < 0) {
            tc->emit( movn_immed_u16(ea, ~d & 0xffff, 0));
        } else {
            tc->emit( mov_immed_u16(ea, d, 0));
        }

        tc->emit( ldrh_offset(ea, reg, 0));

        FreeARMRegister(tc, ea);
    } else {
        /* Ra is a register, check if displacement can be used for store directly */
        if (d >= 0 && d <= 0x1ffe && (d & 1) == 0) {
            uint8_t base = MapGPRForRead(tc, ra);
            tc->emit( ldrh_offset(base, reg, d));
        }
        else if (d >= -256 && d <= 255) {
            uint8_t base = MapGPRForRead(tc, ra);
            tc->emit( ldurh_offset(base, reg, d));
        }
        else {
            uint8_t ea = AllocARMRegister(tc);
            uint8_t base = MapGPRForRead(tc, ra);

            if (d < 0) {
                tc->emit( movn_immed_u16(ea, ~d & 0xffff, 0));
            }
            else {
                tc->emit( mov_immed_u16(ea, d, 0));
            }
            
            tc->emit( ldrh_regoffset(base, reg, ea, SXTX, 0));

            FreeARMRegister(tc, ea);
        }
    }

    tc->tc_PPCCodePtr++;
    tc->advancePC(4);
    return 1;
}

int EMIT_lhzx(PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 1) return -1;

    uint8_t rd = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;

    uint8_t reg_ra = ra != 0 ? MapGPRForRead(tc, ra) : 0xff;
    uint8_t reg_rb = MapGPRForRead(tc, rb);
    uint8_t reg_rd = MapGPRForWrite(tc, rd);
    
    /* If Ra is 0, then address is the displacement, only */
    if (ra == 0) {
        tc->emit( ldrh_offset(reg_rb, reg_rd, 0));
    } else {
        tc->emit( ldrh_regoffset(reg_ra, reg_rd, reg_rb, SXTX, 0));
    }

    tc->tc_PPCCodePtr++;
    tc->advancePC(4);
    return 1;
}

int EMIT_lhzu(PPCTranslatorContext *tc, uint32_t opcode)
{
    uint8_t rd = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    int16_t d = opcode & 0xffff;

    uint8_t base = MapGPRForReadAndWrite(tc, ra);
    uint8_t reg = MapGPRForWrite(tc, rd);

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
        uint8_t ea = AllocARMRegister(tc);

        if (d < 0) {
            tc->emit( movn_immed_u16(ea, ~d & 0xffff, 0));
        }
        else {
            tc->emit( mov_immed_u16(ea, d, 0));
        }
        
        tc->emit({ 
            ldrh_regoffset(base, reg, ea, SXTX, 0),
            add_reg(base, base, ea, LSL, 0)
        });

        FreeARMRegister(tc, ea);
    }

    tc->tc_PPCCodePtr++;
    tc->advancePC(4);
    return 1;
}

int EMIT_lhzux(PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 1) return -1;

    uint8_t rd = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;

    if (ra == 0 || ra == rd) return -1;

    uint8_t reg_rb = MapGPRForRead(tc, rb);
    uint8_t reg_ra = MapGPRForReadAndWrite(tc, ra);
    uint8_t reg_rd = MapGPRForWrite(tc, rd);
    
    tc->emit({ 
        ldrh_regoffset(reg_ra, reg_rd, reg_rb, SXTX, 0),
        add_reg(reg_ra, reg_ra, reg_rb, LSL, 0)
    });

    tc->tc_PPCCodePtr++;
    tc->advancePC(4);
    return 1;
}

int EMIT_lhbrx(PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 1) return -1;

    uint8_t rd = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;

    uint8_t reg_ra = ra != 0 ? MapGPRForRead(tc, ra) : 0;
    uint8_t reg_rb = MapGPRForRead(tc, rb);
    uint8_t reg_rd = MapGPRForWrite(tc, rd);
    
    /* If Ra is 0, then address is the displacement, only */
    if (ra == 0) {
        tc->emit( ldrh_offset(reg_rb, reg_rd, 0));
    } else {
        tc->emit( ldrh_regoffset(reg_ra, reg_rd, reg_rb, SXTX, 0));
    }

    tc->emit( rev16(reg_rd, reg_rd));

    tc->tc_PPCCodePtr++;
    tc->advancePC(4);
    return 1;
}

int EMIT_lwz(PPCTranslatorContext *tc, uint32_t opcode)
{
    uint8_t rd = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    int16_t d = opcode & 0xffff;

    uint8_t base = ra != 0 ? MapGPRForRead(tc, ra) : 0xff;
    uint8_t reg = MapGPRForWrite(tc, rd);

    /* If Ra is 0, then address is the displacement, only */
    if (ra == 0) {
        uint8_t ea = AllocARMRegister(tc);

        if (d < 0) {
            tc->emit( movn_immed_u16(ea, ~d & 0xffff, 0));
        } else {
            tc->emit( mov_immed_u16(ea, d, 0));
        }

        tc->emit( ldr_offset(ea, reg, 0));

        FreeARMRegister(tc, ea);
    } else {
        /* Ra is a register, check if displacement can be used for store directly */
        if (d >= 0 && d <= 0x3ffc && (d & 3) == 0) {
            tc->emit( ldr_offset(base, reg, d));
        }
        else if (d >= -256 && d <= 255) {
            tc->emit( ldur_offset(base, reg, d));
        }
        else {
            uint8_t ea = AllocARMRegister(tc);

            if (d < 0) {
                tc->emit( movn_immed_u16(ea, ~d & 0xffff, 0));
            }
            else {
                tc->emit( mov_immed_u16(ea, d, 0));
            }
            
            tc->emit( ldr_regoffset(base, reg, ea, SXTX, 0));

            FreeARMRegister(tc, ea);
        }
    }

    tc->tc_PPCCodePtr++;
    tc->advancePC(4);
    return 1;
}

int EMIT_lwzu(PPCTranslatorContext *tc, uint32_t opcode)
{
    uint8_t rd = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    int16_t d = opcode & 0xffff;

    uint8_t base = MapGPRForReadAndWrite(tc, ra);
    uint8_t reg = MapGPRForWrite(tc, rd);

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
        uint8_t ea = AllocARMRegister(tc);

        if (d < 0) {
            tc->emit( movn_immed_u16(ea, ~d & 0xffff, 0));
        }
        else {
            tc->emit( mov_immed_u16(ea, d, 0));
        }
        
        tc->emit({ 
            ldr_regoffset(base, reg, ea, SXTX, 0),
            add_reg(base, base, ea, LSL, 0)
        });

        FreeARMRegister(tc, ea);
    }

    tc->tc_PPCCodePtr++;
    tc->advancePC(4);
    return 1;
}

int EMIT_lwzx(PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 1) return -1;

    uint8_t rd = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;

    uint8_t reg_ra = ra != 0 ? MapGPRForRead(tc, ra) : 0xff;
    uint8_t reg_rb = MapGPRForRead(tc, rb);
    uint8_t reg_rd = MapGPRForWrite(tc, rd);

    
    /* If Ra is 0, then address is the displacement, only */
    if (ra == 0) {
        tc->emit( ldr_offset(reg_rb, reg_rd, 0));
    } else {
        tc->emit( ldr_regoffset(reg_ra, reg_rd, reg_rb, SXTX, 0));
    }

    tc->tc_PPCCodePtr++;
    tc->advancePC(4);
    return 1;
}

int EMIT_lwzux(PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 1) return -1;

    uint8_t rd = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;

    if (ra == 0 || ra == rd) return -1;

    uint8_t reg_rb = MapGPRForRead(tc, rb);
    uint8_t reg_ra = MapGPRForReadAndWrite(tc, ra);
    uint8_t reg_rd = MapGPRForWrite(tc, rd);

    /* If Ra is 0, then address is the displacement, only */
    
    tc->emit({ 
        ldr_regoffset(reg_ra, reg_rd, reg_rb, SXTX, 0),
        add_reg(reg_ra, reg_ra, reg_rb, LSL, 0)
    });

    tc->tc_PPCCodePtr++;
    tc->advancePC(4);
    return 1;
}

int EMIT_lwbrx(PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 1) return -1;

    uint8_t rd = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;

    uint8_t reg_ra = ra != 0 ? MapGPRForRead(tc, ra) : 0xff;
    uint8_t reg_rb = MapGPRForRead(tc, rb);
    uint8_t reg_rd = MapGPRForWrite(tc, rd);
    
    /* If Ra is 0, then address is the displacement, only */
    if (ra == 0) {
        tc->emit( ldr_offset(reg_rb, reg_rd, 0));
    } else {
        tc->emit( ldr_regoffset(reg_ra, reg_rd, reg_rb, SXTX, 0));
    }

    tc->emit( rev(reg_rd, reg_rd));

    tc->tc_PPCCodePtr++;
    tc->advancePC(4);
    return 1;
}

int EMIT_lwarx(PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 1) return -1;

    uint8_t rd = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;

    uint8_t reg_ra = ra != 0 ? MapGPRForRead(tc, ra) : 0xff;
    uint8_t reg_rb = MapGPRForRead(tc, rb);
    uint8_t reg_rd = MapGPRForWrite(tc, rd);
    uint8_t base = reg_rb;

    /* If Ra is 0, then address is the displacement, only */
    if (ra != 0) {
        base = AllocARMRegister(tc);
        tc->emit(add_reg(base, reg_rb, reg_ra, LSL, 0));
    }

    tc->emit(ldxr(base, reg_rd));

    if (ra != 0) {
        FreeARMRegister(tc, base);
    }

    tc->tc_PPCCodePtr++;
    tc->advancePC(4);
    return 1;
}

/* FPU loads and stores */
int EMIT_stfd(PPCTranslatorContext *tc, uint32_t opcode)
{
    uint8_t rs = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    int16_t d = opcode & 0xffff;

    uint8_t reg = MapFPRForRead(tc, rs);

    /* If Ra is 0, then address is the displacement, only */
    if (ra == 0) {
        uint8_t ea = AllocARMRegister(tc);

        if (d < 0) {
            tc->emit( movn_immed_u16(ea, ~d & 0xffff, 0));
        } else {
            tc->emit( mov_immed_u16(ea, d, 0));
        }

        tc->emit(fstd(reg, ea, 0));

        FreeARMRegister(tc, ea);
    } else {
        /* Ra is a register, check if displacement can be used for store directly */
        if (d >= 0 && d <= 0x3ffc && (d & 7) == 0) {
            uint8_t base = MapGPRForRead(tc, ra);
            tc->emit(fstd_pimm(reg, base, d >> 3));
        }
        else if (d >= -256 && d <= 255) {
            uint8_t base = MapGPRForRead(tc, ra);
            tc->emit(fstd(reg, base, d));
        }
        else {
            uint8_t ea = AllocARMRegister(tc);
            uint8_t base = MapGPRForRead(tc, ra);

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

            FreeARMRegister(tc, ea);
        }
    }

    tc->tc_PPCCodePtr++;
    tc->advancePC(4);
    return 1;
}

int EMIT_stfs(PPCTranslatorContext *tc, uint32_t opcode)
{
    uint8_t rs = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    int16_t d = opcode & 0xffff;

    uint8_t reg = MapFPRForRead(tc, rs);
    uint8_t tmp = AllocFPRegister(tc);
    tc->emit(fcvtsd(tmp, reg));

    /* If Ra is 0, then address is the displacement, only */
    if (ra == 0) {
        uint8_t ea = AllocARMRegister(tc);

        if (d < 0) {
            tc->emit( movn_immed_u16(ea, ~d & 0xffff, 0));
        } else {
            tc->emit( mov_immed_u16(ea, d, 0));
        }

        tc->emit( str_offset(ea, reg, 0));

        FreeARMRegister(tc, ea);
    } else {
        /* Ra is a register, check if displacement can be used for store directly */
        if (d >= 0 && d <= 0x3ffc && (d & 3) == 0) {
            uint8_t base = MapGPRForRead(tc, ra);
            tc->emit(fsts_pimm(tmp, base, d >> 2));
        }
        else if (d >= -256 && d <= 255) {
            uint8_t base = MapGPRForRead(tc, ra);
            tc->emit(fsts(tmp, base, d));
        }
        else {
            uint8_t ea = AllocARMRegister(tc);
            uint8_t base = MapGPRForRead(tc, ra);

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

            FreeARMRegister(tc, ea);
        }
    }

    FreeFPRegister(tc, tmp);

    tc->tc_PPCCodePtr++;
    tc->advancePC(4);
    return 1;
}

int EMIT_stfiwx(PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 1) return -1;

    uint8_t rs = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;

    uint8_t reg_rs = MapFPRForRead(tc, rs);
    uint8_t reg_rb = MapGPRForRead(tc, rb);
    
    /* If Ra is 0, then address is the displacement, only */
    if (ra == 0) {
        tc->emit(fsts(reg_rs, reg_rb, 0));
    } else {
        uint8_t reg_ra = MapGPRForRead(tc, ra);
        tc->emit(fsts_regoffset(reg_rs, reg_ra, reg_rb, SXTX, 0));
    }

    tc->tc_PPCCodePtr++;
    tc->advancePC(4);
    return 1;
}

int EMIT_lfd(PPCTranslatorContext *tc, uint32_t opcode)
{
    uint8_t rd = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    int16_t d = opcode & 0xffff;

    uint8_t base = ra != 0 ? MapGPRForRead(tc, ra) : 0xff;
    uint8_t reg = MapFPRForWrite(tc, rd);

    /* If Ra is 0, then address is the displacement, only */
    if (ra == 0) {
        uint8_t ea = AllocARMRegister(tc);

        if (d < 0) {
            tc->emit( movn_immed_u16(ea, ~d & 0xffff, 0));
        } else {
            tc->emit( mov_immed_u16(ea, d, 0));
        }

        tc->emit(fldd(reg, ea, 0));

        FreeARMRegister(tc, ea);
    } else {
        /* Ra is a register, check if displacement can be used for store directly */
        if (d >= 0 && d <= 0x3ffc && (d & 7) == 0) {
            tc->emit(fldd_pimm(reg, base, d >> 3));
        }
        else if (d >= -256 && d <= 255) {
            tc->emit(fldd(reg, base, d));
        }
        else {
            uint8_t ea = AllocARMRegister(tc);

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

            FreeARMRegister(tc, ea);
        }
    }

    tc->tc_PPCCodePtr++;
    tc->advancePC(4);
    return 1;
}

int EMIT_lfs(PPCTranslatorContext *tc, uint32_t opcode)
{
    uint8_t rd = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    int16_t d = opcode & 0xffff;

    uint8_t base = ra != 0 ? MapGPRForRead(tc, ra) : 0xff;
    uint8_t reg = MapFPRForWrite(tc, rd);

    /* If Ra is 0, then address is the displacement, only */
    if (ra == 0) {
        uint8_t ea = AllocARMRegister(tc);

        if (d < 0) {
            tc->emit( movn_immed_u16(ea, ~d & 0xffff, 0));
        } else {
            tc->emit( mov_immed_u16(ea, d, 0));
        }

        tc->emit(fldd(reg, ea, 0));

        FreeARMRegister(tc, ea);
    } else {
        /* Ra is a register, check if displacement can be used for store directly */
        if (d >= 0 && d <= 0x3ffc && (d & 7) == 0) {
            tc->emit(flds_pimm(reg, base, d >> 2));
        }
        else if (d >= -256 && d <= 255) {
            tc->emit(flds(reg, base, d));
        }
        else {
            uint8_t ea = AllocARMRegister(tc);

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

            FreeARMRegister(tc, ea);
        }
    }

    tc->emit(fcvtds(reg, reg));

    tc->tc_PPCCodePtr++;
    tc->advancePC(4);
    return 1;
}

}
