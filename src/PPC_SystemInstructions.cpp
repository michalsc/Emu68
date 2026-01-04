/*
    Copyright © 2019-2025 Michal Schulz <michal.schulz@gmx.de>
    https://github.com/michalsc

    This Source Code Form is subject to the terms of the
    Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed
    with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#define restrict __restrict__

#include <span>

#include <emu68/LRUCache>
#include <emu68/ReturnStack>
#include <emu68/GPR>
#include <emu68/FPR>

#include "config.h"
#include "PPC.h"
#include "A64.h"

namespace Emu68::PPC {

extern ReturnStack return_stack;
extern LRUCache cache;

namespace Emit {

class SupervisorCheck {
    PPCTranslatorContext *tc;
    std::span<uint32_t> exception;
    uint32_t* fixup_location;
    uint32_t fixup_type;
public:
    SupervisorCheck(PPCTranslatorContext *t);
    ~SupervisorCheck();
};

SupervisorCheck::SupervisorCheck(PPCTranslatorContext *t) : tc(t)
{
    uint32_t* exception_data;

    if (!tc->tc_SupervisorChecked)
    {
        GPR ctx = tc->getCTX();
        GPR tmp = GPR::allocate();

        /* We need to flush PC now, just in case */
        tc->flushPC();

        /* 
            Fetch MSR and check if exceptions are enabled - it is illegal to call
            RFI from user
        */
        tc->emit({
            ldr_offset(ctx, tmp, __builtin_offsetof(PPCState, MSR)),
            tbnz(tmp, 14, 0)
        });

        /* Remember jump location and fixup type */
        fixup_type = FIXUP_TBZ;
        fixup_location = tc->tc_CodePtr - 1;

        /* Now insert the program exception path - raise exception if RFI was not allowed */
        uint32_t *exit_code_start = tc->save();
        tc->emitException(0x700);
        uint32_t *exit_code_end = tc->restore();
        std::size_t exception_size = exit_code_end - exit_code_start;

        exception_data = new uint32_t[exception_size];
        exception = std::span<uint32_t>(exception_data, exception_size);

        for (size_t i=0; i < exception_size; i++) {
            exception_data[i] = tc->tc_CodePtr[i];
        }
        
        tc->tc_SupervisorChecked = true;
    }
}

SupervisorCheck::~SupervisorCheck()
{
    if (!exception.empty())
    {
        tc->emit(exception);
        
        uint32_t *exit_code_end = tc->tc_CodePtr;

        tc->emit({
            (uint32_t)(exit_code_end - fixup_location),
            fixup_type,
            1,
            (uint32_t)(exception.size()),
            INSN_TO_LE(MARKER_EXIT_BLOCK)
        });
        
        delete exception.data();
    }
}

int mftb(PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 1) return -1;

    uint32_t tbr = (opcode >> 11) & 0x3ff;
    uint8_t rd = (opcode >> 21) & 31;

    /* tbr is a split field, fix it */
    tbr = ((tbr >> 5) & 0x1f) | ((tbr & 0x1f) << 5);

    if (tbr != 268 && tbr != 269) return -1;

    GPR reg_rd = tc->mapGPRForWrite(rd);
    GPR tmp = GPR::allocate();

    tc->emit(mrs(tmp, sys_CNTPCT_EL0));

    if (tbr == 268) // TBL
        tc->emit( mov_reg(reg_rd, tmp));
    else // TBH
        tc->emit( lsr64(reg_rd, tmp, 32));

    tc->advancePC(4);

    return 1;
}

int mfmsr(PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 0x001ff801) return -1;

    SupervisorCheck sc(tc);

    uint8_t rd = (opcode >> 21) & 31;

    GPR reg_rd = tc->mapGPRForWrite(rd);
    GPR ctx = tc->getCTX();

    tc->emit(ldr_offset(ctx, reg_rd, __builtin_offsetof(PPCState, MSR)));

    tc->advancePC(4);

    return 1;
}

int mtmsr(PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 0x001ff801) return -1;

    SupervisorCheck sc(tc);

    uint8_t rs = (opcode >> 21) & 31;

    GPR reg_rs = tc->mapGPRForRead(rs);
    GPR ctx = tc->getCTX();
    GPR tmp = GPR::allocate();

    tc->emit({
        /* Clear top + POW, ILE */
        bic_immed(tmp, reg_rs, 16, 16),
        /* Clear IP, IR, DR, RI, LE */
        bic_immed(tmp, tmp, 8, 0),
        /* Set IP again */
        orr_immed(tmp, tmp, 1, 26),
        str_offset(ctx, tmp, __builtin_offsetof(PPCState, MSR))
    });

    /* Stop here */
    tc->advancePC(4);
    tc->emitStop();

    return 1;
}

int mtspr(PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 1) return -1;

    uint32_t spr = (opcode >> 11) & 0x3ff;
    uint8_t rs = (opcode >> 21) & 31;
    /* tbr is a split field, fix it */
    spr = ((spr >> 5) & 0x1f) | ((spr & 0x1f) << 5);

    GPR reg_rs = tc->mapGPRForRead(rs);

    if (spr & 0x10) {
        SupervisorCheck sc(tc);

        GPR ctx = tc->getCTX();
        GPR tmp = GPR::allocate();
        GPR tmp2;

        switch(spr) {
            case 18:   /* DSISR */
                tc->emit(str_offset(ctx, reg_rs, __builtin_offsetof(PPCState, DSISR)));
                break;
            case 19:   /* DAR */
                tc->emit(str_offset(ctx, reg_rs, __builtin_offsetof(PPCState, DAR)));
                break;
            case 22:    /* DEC */
                tc->emit({
                    /* Set new counter value, starts immediately counting */
                    msr(reg_rs, sys_CNTP_TVAL_EL0),
                    mov_immed_u16(tmp, 1, 0),
                    /* Set 1 to CNTP_CTL_EL0 to enable timer and unmask interrupt */
                    msr(tmp, sys_CNTP_CTL_EL0)
                });
                break;
            case 26:    /* SRR0 */
                tc->emit({
                    bic_immed(tmp, reg_rs, 2, 0), 
                    str_offset(ctx, tmp, __builtin_offsetof(PPCState, SRR0))
                });
                break;
            case 27:    /* SRR1 */
                tc->emit({
                    /* Set IP */
                    orr_immed(tmp, reg_rs, 1, 26),
                    str_offset(ctx, tmp, __builtin_offsetof(PPCState, SRR1))
                });
                break;
            case 272:   /* SPRG0 */
                tc->emit(str_offset(ctx, reg_rs, __builtin_offsetof(PPCState, SPRG[0])));
                break;
            case 273:   /* SPRG1 */
                tc->emit(str_offset(ctx, reg_rs, __builtin_offsetof(PPCState, SPRG[1])));
                break;
            case 274:   /* SPRG2 */
                tc->emit(str_offset(ctx, reg_rs, __builtin_offsetof(PPCState, SPRG[2])));
                break;
            case 275:   /* SPRG3 */
                tc->emit(str_offset(ctx, reg_rs, __builtin_offsetof(PPCState, SPRG[3])));
                break;
            case 276:   /* SPRG4 */
                tc->emit(str_offset(ctx, reg_rs, __builtin_offsetof(PPCState, SPRG[4])));
                break;
            case 277:   /* SPRG5 */
                tc->emit(str_offset(ctx, reg_rs, __builtin_offsetof(PPCState, SPRG[5])));
                break;
            case 278:   /* SPRG6 */
                tc->emit(str_offset(ctx, reg_rs, __builtin_offsetof(PPCState, SPRG[6])));
                break;
            case 279:   /* SPRG7 */
                tc->emit(str_offset(ctx, reg_rs, __builtin_offsetof(PPCState, SPRG[7])));
                break;
            case 920:   /* JIT_CONTROL */
                kprintf("[PPC] JIT_CONTROL written to, need update\n");
                tc->emit(str_offset(ctx, reg_rs, __builtin_offsetof(PPCState, JIT_CONTROL)));
                break;
            case 921:   /* JIT_CONTROL_2 */
                kprintf("[PPC] JIT_CONTROL2 written to, need update\n");
                tmp2 = GPR::allocate();
                tc->emit({
                    /* Test if topmost bit was set, if not, skip causing m68k interrupt */
                    tbz(reg_rs, 31, 6),
                        ldr64_offset(ctx, tmp, __builtin_offsetof(PPCState, M68K_FLAG)),
                        mov_immed_u16(tmp2, 255, 0),
                        strb_offset(tmp, tmp2, 0),
                        dmb_ish(),
                        sev(),
                    and_immed(tmp, reg_rs, 31, 0),
                    str_offset(ctx, tmp, __builtin_offsetof(PPCState, JIT_CONTROL2))
                });
                break;
            case 944:   /* BASE */
                tc->emit(str_offset(ctx, reg_rs, __builtin_offsetof(PPCState, BASEREG)));
                break;
        }

        tc->advancePC(4);

        return 1;
    } else {
        /* Accessing user level SPRs */
        switch(spr) {
            case 1:
            {
                GPR reg_xer = tc->tryGetGPR(XERn);
                if (reg_xer.isValid()) {
                    tc->emit( mov_reg(reg_xer, reg_rs));
                    tc->setDirtyGPR(XERn);
                } else {
                    tc->emit( mov_reg_to_simd(REG_XER, reg_rs));
                }
                break;
            }
            case 8:
                tc->emit( mov_reg(tc->mapGPRForWrite(LRn), reg_rs));
                return_stack.reset();
                break;
            case 9:
                tc->emit( mov_reg(tc->mapGPRForWrite(CTRn), reg_rs));
                break;
            default:
                return -1;
        }
    }

    tc->advancePC(4);

    return 1;
}

int mfspr(PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 1) return -1;

    uint32_t spr = (opcode >> 11) & 0x3ff;
    uint8_t rd = (opcode >> 21) & 31;
    /* tbr is a split field, fix it */
    spr = ((spr >> 5) & 0x1f) | ((spr & 0x1f) << 5);

    if (spr & 0x10)
    {
        SupervisorCheck sc(tc);

        GPR reg_rd = tc->mapGPRForWrite(rd);
        GPR ctx = tc->getCTX();
        GPR tmp = GPR::allocate();

        switch(spr) {
            case 18:    /* DSISR */
                tc->emit(ldr_offset(ctx, reg_rd, __builtin_offsetof(PPCState, DSISR)));
                break;
            case 19:    /* DAR */
                tc->emit(ldr_offset(ctx, reg_rd, __builtin_offsetof(PPCState, DAR)));
                break;
            case 22:    /* DEC */
                tc->emit(mrs(reg_rd, sys_CNTP_TVAL_EL0));
                break;
            case 26:    /* SRR0 */
                tc->emit(ldr_offset(ctx, reg_rd, __builtin_offsetof(PPCState, SRR0)));
                break;
            case 27:    /* SRR1 */
                tc->emit(ldr_offset(ctx, reg_rd, __builtin_offsetof(PPCState, SRR1)));
                break;
            case 272:   /* SPRG0 */
                tc->emit(ldr_offset(ctx, reg_rd, __builtin_offsetof(PPCState, SPRG[0])));
                break;
            case 273:   /* SPRG1 */
                tc->emit(ldr_offset(ctx, reg_rd, __builtin_offsetof(PPCState, SPRG[1])));
                break;
            case 274:   /* SPRG2 */
                tc->emit(ldr_offset(ctx, reg_rd, __builtin_offsetof(PPCState, SPRG[2])));
                break;
            case 275:   /* SPRG3 */
                tc->emit(ldr_offset(ctx, reg_rd, __builtin_offsetof(PPCState, SPRG[3])));
                break;
            case 276:   /* SPRG4 */
                tc->emit(ldr_offset(ctx, reg_rd, __builtin_offsetof(PPCState, SPRG[4])));
                break;
            case 277:   /* SPRG5 */
                tc->emit(ldr_offset(ctx, reg_rd, __builtin_offsetof(PPCState, SPRG[5])));
                break;
            case 278:   /* SPRG6 */
                tc->emit(ldr_offset(ctx, reg_rd, __builtin_offsetof(PPCState, SPRG[6])));
                break;
            case 279:   /* SPRG7 */
                tc->emit(ldr_offset(ctx, reg_rd, __builtin_offsetof(PPCState, SPRG[7])));
                break;
            case 287:   /* PVR */
                tc->emit({
                    mov_immed_u16(reg_rd, (EMU68_VERSION_MAJOR << 8) | EMU68_VERSION_MINOR, 0),
                    movk_immed_u16(reg_rd, 0xee68, 1)
                });
                break;
            case 916:   /* JIT_CACHE_TOTAL */
                tc->emit(ldr_offset(ctx, reg_rd, __builtin_offsetof(PPCState, JIT_CACHE_TOTAL)));
                break;
            case 917:   /* JIT_CACHE_FREE */
                tc->emit(ldr_offset(ctx, reg_rd, __builtin_offsetof(PPCState, JIT_CACHE_FREE)));
                break;
            case 918:   /* JIT_CACHE_MISS */
                tc->emit(ldr_offset(ctx, reg_rd, __builtin_offsetof(PPCState, JIT_CACHE_MISS)));
                break;
            case 919:   /* JIT_UNIT_COUNT */
                tc->emit(ldr_offset(ctx, reg_rd, __builtin_offsetof(PPCState, JIT_UNIT_COUNT)));
                break;
            case 920:   /* JIT_CONTROL */
                tc->emit(ldr_offset(ctx, reg_rd, __builtin_offsetof(PPCState, JIT_CONTROL)));
                break;
            case 921:   /* JIT_CONTROL_2 */
                tc->emit(ldr_offset(ctx, reg_rd, __builtin_offsetof(PPCState, JIT_CONTROL2)));
                break;
            case 944:   /* BASE */
                tc->emit(ldr_offset(ctx, reg_rd, __builtin_offsetof(PPCState, BASEREG)));
                break;
        }

        tc->advancePC(4);
    
        return 1;
    }
    else
    {
        GPR tmp;
        GPR reg_rd = tc->mapGPRForWrite(rd);

        /* Accessing user level SPRs */
        switch(spr) {
            case 1:
                tc->emit( mov_reg(reg_rd, tc->mapGPRForRead(XERn)));
                break;
            case 8:
                tc->emit( mov_reg(reg_rd, tc->mapGPRForRead(LRn)));
                break;
            case 9:
                tc->emit( mov_reg(reg_rd, tc->mapGPRForRead(CTRn)));
                break;
            case 900: /* INSNCNTLO - lower 32 bits of PPC instruction counter */
                tmp = GPR::allocate();
                tc->emit({ 
                    mov_simd_to_reg(tmp, CTX_INSN_COUNT),
                    add64_immed(tmp, tmp, tc->tc_InsnCount & 0xfff)
                });
                if (tc->tc_InsnCount & 0xfff000)
                    tc->emit(add64_immed_lsl12(tmp, tmp, tc->tc_InsnCount >> 12));
                tc->emit(mov_reg(reg_rd, tmp));
                break;
            case 901: /* INSNCNTHI - higher 32 bits of PPC instruction counter */
                tmp = GPR::allocate();
                tc->emit({
                    mov_simd_to_reg(tmp, CTX_INSN_COUNT),
                    add64_immed(tmp, tmp, tc->tc_InsnCount & 0xfff)
                });
                if (tc->tc_InsnCount & 0xfff000)
                    tc->emit(add64_immed_lsl12(tmp, tmp, tc->tc_InsnCount >> 12));
                tc->emit(lsr64(reg_rd, tmp, 32));
                break;
            case 902: /* ARMCNTLO - lower 32 bits of ARM instruction counter */
                tmp = GPR::allocate();
                tc->emit({
                    mrs(tmp, sys_PMCCNTR_EL0),
                    mov_reg(reg_rd, tmp)
                });
                break;
            case 903: /* ARMCNTHI - higher 32 bits of ARM instruction counter */
                tmp = GPR::allocate();
                tc->emit({
                    mrs(tmp, sys_PMCCNTR_EL0),
                    lsr64(reg_rd, tmp, 32)
                });
                break;
            case 904: /* CNTFRQ - Decrementer counter frequency */
                tc->emit(mrs(reg_rd, sys_CNTFRQ_EL0));
                break;
                
            default:
                return -1;
        }
    }

    tc->advancePC(4);

    return 1;
}

int tw(PPCTranslatorContext *tc, uint32_t opcode)
{
    /* sanity check */
    if (opcode & 0x00000001) return -1;

    uint8_t to = (opcode >> 21) & 31;

    if (to == 0)
    {
        /* Not expecting any condition generating exception so skip */
        tc->emit(nop());
        tc->advancePC(4);
        return 1;
    }
    else if (to == 31)
    {
        /* All bits set, trap always */
        tc->flushPC();
        tc->emitException(0x700);
        tc->emitStop();
        return 1;
    }
    else
    {
        tc->flushPC();
    }

    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;

    GPR reg_ra = tc->mapGPRForRead(ra);
    GPR reg_rb = tc->mapGPRForRead(rb);
    int number_of_cases = 0;
    uint32_t *locations[5];

    /* Emit comparison */
    tc->emit(cmp_reg(reg_ra, reg_rb, LSL, 0));
    
    if (to & 1) {
        tc->emit(b_cc(A64_CC_HI, 0));
        locations[number_of_cases++] = tc->tc_CodePtr - 1;
    }
    if (to & 2) {
        tc->emit(b_cc(A64_CC_CC, 0));
        locations[number_of_cases++] = tc->tc_CodePtr - 1;
    }
    if (to & 4) {
        tc->emit(b_cc(A64_CC_EQ, 0));
        locations[number_of_cases++] = tc->tc_CodePtr - 1;
    }
    if (to & 8) {
        tc->emit(b_cc(A64_CC_GT, 0));
        locations[number_of_cases++] = tc->tc_CodePtr - 1;
    }
    if (to & 16) {
        tc->emit(b_cc(A64_CC_LT, 0));
        locations[number_of_cases++] = tc->tc_CodePtr - 1;
    }

    uint32_t *exit_code_start = tc->tc_CodePtr;
    tc->emitException(0x700);
    uint32_t *exit_code_end = tc->tc_CodePtr;

    /* Insert additional exit points */
    for (int i=0; i < number_of_cases; i++) {
        tc->emit({ 
            (uint32_t)(exit_code_end - locations[i]),
            FIXUP_BCC
        });
    }

    tc->emit({
        (uint32_t)number_of_cases,
        (uint32_t)(exit_code_end - exit_code_start),
        INSN_TO_LE(MARKER_EXIT_BLOCK)
    });

    tc->advancePC(4);

    return 1;
}

int twi(PPCTranslatorContext *tc, uint32_t opcode)
{
    uint8_t to = (opcode >> 21) & 31;

    if (to == 0)
    {
        /* Not expecting any condition generating exception so skip */
        tc->emit(nop());
        tc->advancePC(4);
        return 1;
    }
    else if (to == 31)
    {
        /* All bits set, trap always */
        tc->flushPC();
        tc->emitException(0x700);
        tc->emitStop();
        return 1;
    }
    else
    {
        tc->flushPC();
    }

    uint8_t ra = (opcode >> 16) & 31;
    uint16_t imm = opcode & 0xffff;
    int32_t simm = (int16_t)imm;

    GPR reg_ra = tc->mapGPRForRead(ra);

    int number_of_cases = 0;
    uint32_t *locations[5];

    /* Is the immediate in range for CMP? */
    if ((simm & 0xfffff000) == 0) {
        tc->emit( cmp_immed(reg_ra, simm));
    }
    else if ((simm & 0xff000fff) == 0) {
        tc->emit( cmp_immed_lsl12(reg_ra, simm >> 12));
    }
    else {
        GPR tmp = GPR::allocate();

        tc->emitLoadImmediate(tmp, simm);
        tc->emit(cmp_reg(reg_ra, tmp, LSL, 0));
    }

    if (to & 1) {
        tc->emit(b_cc(A64_CC_HI, 0));
        locations[number_of_cases++] = tc->tc_CodePtr - 1;
    }
    if (to & 2) {
        tc->emit(b_cc(A64_CC_CC, 0));
        locations[number_of_cases++] = tc->tc_CodePtr - 1;
    }
    if (to & 4) {
        tc->emit(b_cc(A64_CC_EQ, 0));
        locations[number_of_cases++] = tc->tc_CodePtr - 1;
    }
    if (to & 8) {
        tc->emit(b_cc(A64_CC_GT, 0));
        locations[number_of_cases++] = tc->tc_CodePtr - 1;
    }
    if (to & 16) {
        tc->emit(b_cc(A64_CC_LT, 0));
        locations[number_of_cases++] = tc->tc_CodePtr - 1;
    }

    uint32_t *exit_code_start = tc->tc_CodePtr;
    tc->emitException(0x700);
    uint32_t *exit_code_end = tc->tc_CodePtr;

    /* Insert additional exit points */
    for (int i=0; i < number_of_cases; i++) {
        tc->emit({ 
            (uint32_t)(exit_code_end - locations[i]),
            FIXUP_BCC
        });
    }

    tc->emit({
        (uint32_t)number_of_cases,
        (uint32_t)(exit_code_end - exit_code_start),
        INSN_TO_LE(MARKER_EXIT_BLOCK)
    });

    tc->advancePC(4);

    return 1;
}

int eieio(PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 0x03fff800) return -1;

    tc->emit( dmb_sy());

    tc->advancePC(4);

    return 1;
}

int isync(PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 0x03fff801) return -1;

    tc->emit({ 
        isb(),
        INSN_TO_LE(MARKER_STOP)
    });

    tc->advancePC(4);

    return 1;
}

int icbi(PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 0x03e00001) return -1;

    GPR base = GPR::allocate();
    GPR cnt = GPR::allocate();
    GPR fill = GPR::allocate();

    /*
        PPC Architecture allows that icbi flushes more than requested. This is what
        will be done here.
    */
    tc->emit({
        /* Load offset for LRU cache */
        mov_immed_u16(base, (uint16_t)(uintptr_t)cache.cacheLoc(), 0),
        movk_immed_u16(base, (uint16_t)((uintptr_t)cache.cacheLoc() >> 16), 1),

        /* make "high" address from offset */
        orr64_immed(base, base, 25, 25, 1),

        /* Load fill values */
        movn64_immed_u16(fill, 0, 0),

        /* Load counter */
        mov_immed_u16(cnt, EMU68_LRU_SET_COUNT * EMU68_LRU_WAY_COUNT, 0),

        /* In the loop: */
        stp64_postindex(base, fill, XZR, 16),
        subs_immed(cnt, cnt, 1),
        b_cc(A64_CC_NE, -2),

        /* Upper base bits are still valid, update lower ones */
        movk_immed_u16(base, (uint16_t)(uintptr_t)cache.allocLoc(), 0),
        movk_immed_u16(base, (uint16_t)((uintptr_t)cache.allocLoc() >> 16), 1),

        /* Load counter, we clear 4 items with one stp */
        mov_immed_u16(cnt, EMU68_LRU_SET_COUNT / 4, 0),

        /* In the loop: */
        stp64_postindex(base, fill, fill, 16),
        subs_immed(cnt, cnt, 1),
        b_cc(A64_CC_NE, -2),

        /* Clear last PC value so that a short path in JIT loop is avoided, fill register is already there */
        mov_reg_to_simd(CTX_LAST_PC, fill),

        /* Last but not least - increase epoch */
        mov_simd_to_reg(cnt, EPOCH),
        add_immed(cnt, cnt, 1),
        mov_reg_to_simd(EPOCH, cnt),
    });

    tc->advancePC(4);

    return 1;
}

int sync(PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 0x001ff800) return -1;

    uint8_t op = (opcode >> 21) & 31;

    switch(op) {
        case 0:
            tc->emit(isb());
            break;
        case 1:
            tc->emit(dmb_sy());
            break;
        default:
            return -1;
    }

    tc->advancePC(4);

    return 1;
}

int dcbst(PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 0x03e00001) return -1;

    uint8_t ra = (opcode >> 21) & 31;
    uint8_t rb = (opcode >> 16) & 31;
    GPR reg_rb = tc->mapGPRForRead(rb);
    GPR base;

    if (ra == 0) {
        base = GPR(reg_rb.get());
    } else {
        GPR reg_ra = tc->mapGPRForRead(ra);
        GPR base = GPR::allocate();
        tc->emit(add_reg(base, reg_ra, reg_rb, LSL, 0));
    }

    tc->emit({
        dsb_sy(),
        dc_cvac(base),
        dsb_sy()
    });

    tc->advancePC(4);

    return 1;
}

int dcbtst(PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 0x03e00001) return -1;

    uint8_t ra = (opcode >> 21) & 31;
    uint8_t rb = (opcode >> 16) & 31;

    GPR reg_rb = tc->mapGPRForRead(rb);
    GPR base;

    if (ra == 0) {
        base = GPR(reg_rb.get());
    } else {
        GPR reg_ra = tc->mapGPRForRead(ra);
        base = GPR::allocate();
        tc->emit(add_reg(base, reg_ra, reg_rb, LSL, 0));
    }

    tc->emit(prfm_pst(base));

    tc->advancePC(4);

    return 1;
}

int dcbt(PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 0x03e00001) return -1;

    uint8_t ra = (opcode >> 21) & 31;
    uint8_t rb = (opcode >> 16) & 31;

    GPR reg_rb = tc->mapGPRForRead(rb);
    GPR base;

    if (ra == 0) {
        base = GPR(reg_rb.get());
    } else {
        GPR reg_ra = tc->mapGPRForRead(ra);
        base = GPR::allocate();
        tc->emit(add_reg(base, reg_ra, reg_rb, LSL, 0));
    }

    tc->emit(prfm_pld(base));

    tc->advancePC(4);

    return 1;
}

int dcbf(PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 0x03e00001) return -1;

    uint8_t ra = (opcode >> 21) & 31;
    uint8_t rb = (opcode >> 16) & 31;

    GPR reg_rb = tc->mapGPRForRead(rb);
    GPR base;

    if (ra == 0) {
        base = GPR(reg_rb.get());
    } else {
        GPR reg_ra = tc->mapGPRForRead(ra);
        base = GPR::allocate();
        tc->emit(add_reg(base, reg_ra, reg_rb, LSL, 0));
    }

    tc->emit({
        dsb_sy(),
        dc_civac(base),
        dsb_sy()
    });

    tc->advancePC(4);

    return 1;
}

int dcbz(PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 0x03e00001) return -1;

    uint8_t ra = (opcode >> 21) & 31;
    uint8_t rb = (opcode >> 16) & 31;

    GPR reg_rb = tc->mapGPRForRead(rb);
    GPR base;

    if (ra == 0) {
        base = GPR(reg_rb.get());
    } else {
        GPR reg_ra = tc->mapGPRForRead(ra);
        base = GPR::allocate();
        tc->emit(add_reg(base, reg_ra, reg_rb, LSL, 0));
    }

    tc->emit({
        dsb_sy(),
        dc_zva(base),
        dsb_sy()
    });

    tc->advancePC(4);

    return 1;
}

int dcba(PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 0x03e00001) return -1;

    uint8_t ra = (opcode >> 21) & 31;
    uint8_t rb = (opcode >> 16) & 31;

    GPR reg_rb = tc->mapGPRForRead(rb);
    GPR base;

    if (ra == 0) {
        base = GPR(reg_rb.get());
    } else {
        GPR reg_ra = tc->mapGPRForRead(ra);
        base = GPR::allocate();
        tc->emit(add_reg(base, reg_ra, reg_rb, LSL, 0));
    }

    tc->emit({
        prfm_pst(base)
    });

    tc->advancePC(4);

    return 1;
}

int dcbi(PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 0x03e00001) return -1;

    SupervisorCheck sc(tc);

    uint8_t ra = (opcode >> 21) & 31;
    uint8_t rb = (opcode >> 16) & 31;
    
    GPR reg_rb = tc->mapGPRForRead(rb);
    GPR base;

    GPR ctx = tc->getCTX();
    GPR tmp = GPR::allocate();

    if (ra == 0) {
        base = GPR(reg_rb.get());
    } else {
        GPR reg_ra = tc->mapGPRForRead(ra);
        base = GPR::allocate();
        tc->emit(add_reg(base, reg_ra, reg_rb, LSL, 0));
    }

    tc->emit({
        dc_ivac(base)
    });
    
    tc->advancePC(4);
    
    return 1;
}

int sc(PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode != 0x44000002) return -1;

    /* Advance program counter by 4 */
    tc->advancePC(4);

    /* Flush it - this is a point of no return */
    tc->flushPC();

    /* Emit exception itself */
    GPR ctx = tc->getCTX();
    GPR tmp = GPR::allocate();

    tc->emit({
        /* Store MSR into SRR1, Store PC into SRR0 */
        ldr_offset(ctx, tmp, __builtin_offsetof(PPCState, MSR)),
        str_offset(ctx, tmp, __builtin_offsetof(PPCState, SRR1)),
        str_offset(ctx, REG_PC, __builtin_offsetof(PPCState, SRR0)),
        
        /* Set supervisor mode in MSR */
        bic_immed(tmp, tmp, 1, 18),

        /* Load new program counter */
        mov_immed_u16(REG_PC, 0xc00, 0),
        movk_immed_u16(REG_PC, 0xfff0, 1),

        /* Store updated MSR */
        str_offset(ctx, tmp, __builtin_offsetof(PPCState, MSR))
    });

    tc->emitStop();

    return 1;
}

int rfi(PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode != 0x4c000064) return -1;

    SupervisorCheck sc(tc);

    GPR ctx = tc->getCTX();
    GPR tmp = GPR::allocate();

    tc->emit({
        /* Load SRR1, clear POW bit, store into MSR */
        ldr_offset(ctx, tmp, __builtin_offsetof(PPCState, SRR1)),
        bic_immed(tmp, tmp, 1, 14),
        str_offset(ctx, tmp, __builtin_offsetof(PPCState, MSR)),

        /* Load SRR0, store into PC */
        ldr_offset(ctx, REG_PC, __builtin_offsetof(PPCState, SRR0)),
        bic_immed(REG_PC, REG_PC, 2, 0),
    });

    tc->resetOffsetPC();

    /* This instruction exits the JIT loop */
    tc->emitStop();

    return 1;
}

} // Emit

} // Emu68::PPC
