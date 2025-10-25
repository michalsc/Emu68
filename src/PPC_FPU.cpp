#define restrict __restrict__

#include "config.h"
#include "PPC.h"
#include "A64.h"

namespace Emu68::PPC {

int EMIT_fsubx(struct PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 0x000007c0) return -1;

    uint8_t rd = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;
    uint8_t rc = opcode & 1;

    uint8_t reg_ra = MapFPRForRead(tc, ra);
    uint8_t reg_rb = MapFPRForRead(tc, rb);
    uint8_t reg_rd = MapFPRForWrite(tc, rd);

    if (rc) {
        kprintf("fsub. not supported yet!");
        return -1;
    }

    tc->EMIT(fsubd(reg_rd, reg_ra, reg_rb));

    tc->tc_PPCCodePtr++;
    tc->AdvancePC(4);
    return 1;
}

int EMIT_faddx(struct PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 0x000007c0) return -1;

    uint8_t rd = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;
    uint8_t rc = opcode & 1;

    uint8_t reg_ra = MapFPRForRead(tc, ra);
    uint8_t reg_rb = MapFPRForRead(tc, rb);
    uint8_t reg_rd = MapFPRForWrite(tc, rd);

    if (rc) {
        kprintf("fadd. not supported yet!");
        return -1;
    }

    tc->EMIT(faddd(reg_rd, reg_ra, reg_rb));

    tc->tc_PPCCodePtr++;
    tc->AdvancePC(4);
    return 1;
}

int EMIT_fmulx(struct PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 0x0000f800) return -1;

    uint8_t rd = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 6) & 31;
    uint8_t rc = opcode & 1;

    uint8_t reg_ra = MapFPRForRead(tc, ra);
    uint8_t reg_rb = MapFPRForRead(tc, rb);
    uint8_t reg_rd = MapFPRForWrite(tc, rd);

    if (rc) {
        kprintf("fmul. not supported yet!");
        return -1;
    }

    tc->EMIT(fmuld(reg_rd, reg_ra, reg_rb));

    tc->tc_PPCCodePtr++;
    tc->AdvancePC(4);
    return 1;
}

int EMIT_fdivx(struct PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 0x000007c0) return -1;

    uint8_t rd = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;
    uint8_t rc = opcode & 1;

    uint8_t reg_ra = MapFPRForRead(tc, ra);
    uint8_t reg_rb = MapFPRForRead(tc, rb);
    uint8_t reg_rd = MapFPRForWrite(tc, rd);

    if (rc) {
        kprintf("fdiv. not supported yet!");
        return -1;
    }

    tc->EMIT(fdivd(reg_rd, reg_ra, reg_rb));

    tc->tc_PPCCodePtr++;
    tc->AdvancePC(4);
    return 1;
}

int EMIT_fctiwzx(struct PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 0x001f07c0) return -1;

    uint8_t rd = (opcode >> 21) & 31;
    uint8_t rb = (opcode >> 11) & 31;
    uint8_t rc = opcode & 1;

    uint8_t reg_rb = MapFPRForRead(tc, rb);
    uint8_t reg_rd = MapFPRForWrite(tc, rd);
    uint8_t tmp = AllocARMRegister(tc);

    if (rc) {
        kprintf("fctiwzx. not supported yet!");
        return -1;
    }

    tc->EMIT({
        fcvtzs_Dto32(tmp, reg_rb),
        mov_reg_to_simd(reg_rd, TS_S, 0, tmp)
    });

    FreeARMRegister(tc, tmp);

    tc->tc_PPCCodePtr++;
    tc->AdvancePC(4);
    return 1;
}

int EMIT_fmrx(struct PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 0x001f0000) return -1;

    uint8_t rd = (opcode >> 21) & 31;
    uint8_t rb = (opcode >> 11) & 31;
    uint8_t rc = opcode & 1;

    uint8_t reg_rb = MapFPRForRead(tc, rb);
    uint8_t reg_rd = MapFPRForWrite(tc, rd);

    if (rc) {
        kprintf("fmr. not supported yet!");
        return -1;
    }

    tc->EMIT(fcpyd(reg_rd, reg_rb));

    tc->tc_PPCCodePtr++;
    tc->AdvancePC(4);
    return 1;
}

int EMIT_fcmpu(struct PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 0x00600001) return -1;

    uint8_t crn = (opcode >> 23) & 7;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;

    uint8_t reg_rb = MapFPRForRead(tc, rb);
    uint8_t reg_ra = MapFPRForRead(tc, ra);

    tc->EMIT(fcmpd(reg_ra, reg_rb));

    EMIT_set_crn_signed(tc, crn);

    tc->tc_PPCCodePtr++;
    tc->AdvancePC(4);
    return 1;
}

int EMIT_fmadd(struct PPCTranslatorContext *tc, uint32_t opcode)
{
    uint8_t rd = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;
    uint8_t rc = (opcode >> 6) & 31;
    uint8_t c = opcode & 1;

    uint8_t reg_ra = MapFPRForRead(tc, ra);
    uint8_t reg_rb = MapFPRForRead(tc, rb);
    uint8_t reg_rc = MapFPRForRead(tc, rc);
    uint8_t reg_rd = MapFPRForWrite(tc, rd);

    if (c) {
        kprintf("fmadd. not supported yet!");
        return -1;
    }

    tc->EMIT(fmaddd(reg_rd, reg_ra, reg_rc, reg_rb));

    tc->tc_PPCCodePtr++;
    tc->AdvancePC(4);
    return 1;
}

} // Emu68::PPC
