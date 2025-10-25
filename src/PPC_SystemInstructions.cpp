#define restrict __restrict__

#include <cpp/LRUCache>
#include <cpp/ReturnStack>
#include "config.h"
#include "PPC.h"
#include "A64.h"

namespace Emu68::PPC {

extern ReturnStack returnStack;

int EMIT_mftb(struct PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Stanity check */
    if (opcode & 1) return -1;

    uint32_t tbr = (opcode >> 11) & 0x3ff;
    uint8_t rd = (opcode >> 21) & 31;
    /* tbr is a split field, fix it */
    tbr = ((tbr >> 5) & 0x1f) | ((tbr & 0x1f) << 5);

    if (tbr != 268 && tbr != 269) return -1;

    uint8_t reg_rd = MapGPRForWrite(tc, rd);
    uint8_t tmp = AllocARMRegister(tc);

    tc->EMIT(mrs(tmp, sys_CNTPCT_EL0));

    if (tbr == 268) // TBL
        tc->EMIT( mov_reg(reg_rd, tmp));
    else // TBH
        tc->EMIT( lsr64(reg_rd, tmp, 32));

    FreeARMRegister(tc, tmp);

    tc->tc_PPCCodePtr++;
    tc->AdvancePC(4);
    return 1;
}

int EMIT_mfmsr(struct PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Stanity check */
    if (opcode & 0x001ff801) return -1;

    uint8_t rd = (opcode >> 21) & 31;
    uint8_t reg_rd = MapGPRForWrite(tc, rd);
    uint8_t ctx = GetCTX(tc);
    uint8_t tmp = AllocARMRegister(tc);

    /* 
        This fetches must not be context synchronizing and one supervisor check per
        code block is sufficient.
    */
    if (!tc->tc_SupervisorChecked)
    {
        /* We need to flush PC now, just in case */
        tc->FlushPC();

        /* 
            Fetch MSR and check if exceptions are enabled - it is illegal to call
            RFI from user
        */
        tc->EMIT({
            ldr_offset(ctx, tmp, __builtin_offsetof(PPCState, MSR)),
            tbnz(tmp, 14, 0)
        });
    }

    /* Emit jump, remember its location and fixup type */
    uint32_t fixup_type = FIXUP_TBZ;
    uint32_t *jump_location = tc->tc_CodePtr - 1;

    tc->EMIT(ldr_offset(ctx, reg_rd, __builtin_offsetof(PPCState, MSR)));

    FreeARMRegister(tc, tmp);

    if (!tc->tc_SupervisorChecked)
    {
        /* Now insert the program exception path - raise exception if RFI was not allowed */
        uint32_t *exit_code_start = tc->tc_CodePtr;
        tc->EMIT_Exception(0x700);
        uint32_t *exit_code_end = tc->tc_CodePtr;

        /* Insert fixup location */
        tc->EMIT({ 
            (uint32_t)(exit_code_end - jump_location),
            fixup_type,
            1,
            (uint32_t)(exit_code_end - exit_code_start),
            INSN_TO_LE(MARKER_EXIT_BLOCK)
        });

        tc->tc_SupervisorChecked = true;
    }

    tc->AdvancePC(4);
    tc->tc_PPCCodePtr++;
    return 1;
}

int EMIT_mtmsr(struct PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Stanity check */
    if (opcode & 0x001ff801) return -1;

    uint8_t rs = (opcode >> 21) & 31;
    uint8_t reg_rs = MapGPRForRead(tc, rs);
    uint8_t ctx = GetCTX(tc);
    uint8_t tmp = AllocARMRegister(tc);

    /* 
        This fetches must not be context synchronizing and one supervisor check per
        code block is sufficient.
    */
    if (!tc->tc_SupervisorChecked)
    {
        /* We need to flush PC now, just in case */
        tc->FlushPC();

        /* 
            Fetch MSR and check if exceptions are enabled - it is illegal to call
            RFI from user
        */
        tc->EMIT({
            ldr_offset(ctx, tmp, __builtin_offsetof(PPCState, MSR)),
            tbnz(tmp, 14, 0)
        });
    }

    /* Emit jump, remember its location and fixup type */
    uint32_t fixup_type = FIXUP_TBZ;
    uint32_t *jump_location = tc->tc_CodePtr - 1;

    tc->EMIT({
        /* Clear top + POW, ILE */
        bic_immed(tmp, reg_rs, 16, 16),
        /* Clear IP, IR, DR, RI, LE */
        bic_immed(tmp, tmp, 8, 0),
        /* Set IP again */
        orr_immed(tmp, tmp, 1, 26),
        str_offset(ctx, tmp, __builtin_offsetof(PPCState, MSR))
    });

    FreeARMRegister(tc, tmp);

    if (!tc->tc_SupervisorChecked)
    {
        /* Now insert the program exception path - raise exception if RFI was not allowed */
        uint32_t *exit_code_start = tc->tc_CodePtr;
        tc->EMIT_Exception(0x700);
        uint32_t *exit_code_end = tc->tc_CodePtr;

        /* Insert fixup location */
        tc->EMIT({ 
            (uint32_t)(exit_code_end - jump_location),
            fixup_type,
            1,
            (uint32_t)(exit_code_end - exit_code_start),
            INSN_TO_LE(MARKER_EXIT_BLOCK)
        });

        tc->tc_SupervisorChecked = true;
    }

    /* Stop here */
    tc->tc_PPCCodePtr++;
    tc->AdvancePC(4);
    tc->STOP();

    return 1;
}

int EMIT_mtspr(struct PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Stanity check */
    if (opcode & 1) return -1;

    uint32_t spr = (opcode >> 11) & 0x3ff;
    uint8_t rs = (opcode >> 21) & 31;
    /* tbr is a split field, fix it */
    spr = ((spr >> 5) & 0x1f) | ((spr & 0x1f) << 5);

    uint8_t reg_rs = MapGPRForRead(tc, rs);

    if (spr & 0x10) {
        uint8_t ctx = GetCTX(tc);
        uint8_t tmp = AllocARMRegister(tc);
        uint8_t tmp2;

        /* 
            This fetches must not be context synchronizing and one supervisor check per
            code block is sufficient.
        */
        if (!tc->tc_SupervisorChecked)
        {
            /* We need to flush PC now, just in case */
            tc->FlushPC();

            /* 
                Fetch MSR and check if exceptions are enabled - it is illegal to call
                RFI from user
            */
            tc->EMIT({
                ldr_offset(ctx, tmp, __builtin_offsetof(PPCState, MSR)),
                tbnz(tmp, 14, 0)
            });
        }

        /* Emit jump, remember its location and fixup type */
        uint32_t fixup_type = FIXUP_TBZ;
        uint32_t *jump_location = tc->tc_CodePtr - 1;

        switch(spr) {
            case 18:   /* DSISR */
                tc->EMIT(str_offset(ctx, reg_rs, __builtin_offsetof(PPCState, DSISR)));
                break;
            case 19:   /* DAR */
                tc->EMIT(str_offset(ctx, reg_rs, __builtin_offsetof(PPCState, DAR)));
                break;
            case 22:    /* DEC */
                tc->EMIT({
                    /* Set new counter value, starts immediately counting */
                    msr(reg_rs, sys_CNTP_TVAL_EL0),
                    mov_immed_u16(tmp, 1, 0),
                    /* Set 1 to CNTP_CTL_EL0 to enable timer and unmask interrupt */
                    msr(tmp, sys_CNTP_CTL_EL0)
                });
                break;
            case 26:    /* SRR0 */
                tc->EMIT({
                    bic_immed(tmp, reg_rs, 2, 0), 
                    str_offset(ctx, tmp, __builtin_offsetof(PPCState, SRR0))
                });
                break;
            case 27:    /* SRR1 */
                tc->EMIT({
                    /* Set IP */
                    orr_immed(tmp, reg_rs, 1, 26),
                    str_offset(ctx, tmp, __builtin_offsetof(PPCState, SRR1))
                });
                break;
            case 272:   /* SPRG0 */
                tc->EMIT(str_offset(ctx, reg_rs, __builtin_offsetof(PPCState, SPRG[0])));
                break;
            case 273:   /* SPRG1 */
                tc->EMIT(str_offset(ctx, reg_rs, __builtin_offsetof(PPCState, SPRG[1])));
                break;
            case 274:   /* SPRG2 */
                tc->EMIT(str_offset(ctx, reg_rs, __builtin_offsetof(PPCState, SPRG[2])));
                break;
            case 275:   /* SPRG3 */
                tc->EMIT(str_offset(ctx, reg_rs, __builtin_offsetof(PPCState, SPRG[3])));
                break;
            case 276:   /* SPRG4 */
                tc->EMIT(str_offset(ctx, reg_rs, __builtin_offsetof(PPCState, SPRG[4])));
                break;
            case 277:   /* SPRG5 */
                tc->EMIT(str_offset(ctx, reg_rs, __builtin_offsetof(PPCState, SPRG[5])));
                break;
            case 278:   /* SPRG6 */
                tc->EMIT(str_offset(ctx, reg_rs, __builtin_offsetof(PPCState, SPRG[6])));
                break;
            case 279:   /* SPRG7 */
                tc->EMIT(str_offset(ctx, reg_rs, __builtin_offsetof(PPCState, SPRG[7])));
                break;
            case 920:   /* JIT_CONTROL */
                kprintf("[PPC] JIT_CONTROL written to, need update\n");
                tc->EMIT(str_offset(ctx, reg_rs, __builtin_offsetof(PPCState, JIT_CONTROL)));
                break;
            case 921:   /* JIT_CONTROL_2 */
                kprintf("[PPC] JIT_CONTROL2 written to, need update\n");
                tmp2 = AllocARMRegister(tc);
                tc->EMIT({
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
                FreeARMRegister(tc, tmp2);
                break;
            case 944:   /* BASE */
                tc->EMIT(str_offset(ctx, reg_rs, __builtin_offsetof(PPCState, BASEREG)));
                break;
        }

        FreeARMRegister(tc, tmp);

        if (!tc->tc_SupervisorChecked)
        {
            /* Now insert the program exception path - raise exception if RFI was not allowed */
            uint32_t *exit_code_start = tc->tc_CodePtr;
            tc->EMIT_Exception(0x700);
            uint32_t *exit_code_end = tc->tc_CodePtr;

            /* Insert fixup location */
            tc->EMIT({ 
                (uint32_t)(exit_code_end - jump_location),
                fixup_type,
                1,
                (uint32_t)(exit_code_end - exit_code_start),
                INSN_TO_LE(MARKER_EXIT_BLOCK)
            });

            tc->tc_SupervisorChecked = true;
        }

        tc->AdvancePC(4);
        tc->tc_PPCCodePtr++;
        return 1;
    } else {
        /* Accessing user level SPRs */
        switch(spr) {
            case 1:
            {
                uint8_t reg_xer = IsGPRMapped(tc, XERn);
                if (reg_xer != 0xff) {
                    tc->EMIT( mov_reg(reg_xer, reg_rs));
                    SetDirtyGPR(tc, XERn);
                } else {
                    tc->EMIT( mov_reg_to_simd(REG_XER, reg_rs));
                }
                break;
            }
            case 8:
                tc->EMIT( mov_reg(MapGPRForWrite(tc, LRn), reg_rs));
                returnStack.Reset();
                break;
            case 9:
                tc->EMIT( mov_reg(MapGPRForWrite(tc, CTRn), reg_rs));
                break;
            default:
                return -1;
        }
    }

    tc->tc_PPCCodePtr++;
    tc->AdvancePC(4);
    return 1;
}

int EMIT_mfspr(struct PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Stanity check */
    if (opcode & 1) return -1;

    uint32_t spr = (opcode >> 11) & 0x3ff;
    uint8_t rd = (opcode >> 21) & 31;
    /* tbr is a split field, fix it */
    spr = ((spr >> 5) & 0x1f) | ((spr & 0x1f) << 5);

    uint8_t reg_rd = MapGPRForWrite(tc, rd);

    if (spr & 0x10)
    {
        uint8_t ctx = GetCTX(tc);
        uint8_t tmp = AllocARMRegister(tc);

        /* 
            This fetches must not be context synchronizing and one supervisor check per
            code block is sufficient.
        */
        if (!tc->tc_SupervisorChecked)
        {
            /* We need to flush PC now, just in case */
            tc->FlushPC();

            /* 
                Fetch MSR and check if exceptions are enabled - it is illegal to call
                RFI from user
            */
            tc->EMIT({
                ldr_offset(ctx, tmp, __builtin_offsetof(PPCState, MSR)),
                tbnz(tmp, 14, 0)
            });
        }

        /* Emit jump, remember its location and fixup type */
        uint32_t fixup_type = FIXUP_TBZ;
        uint32_t *jump_location = tc->tc_CodePtr - 1;

        switch(spr) {
            case 18:    /* DSISR */
                tc->EMIT(ldr_offset(ctx, reg_rd, __builtin_offsetof(PPCState, DSISR)));
                break;
            case 19:    /* DAR */
                tc->EMIT(ldr_offset(ctx, reg_rd, __builtin_offsetof(PPCState, DAR)));
                break;
            case 22:    /* DEC */
                tc->EMIT(mrs(reg_rd, sys_CNTP_TVAL_EL0));
                break;
            case 26:    /* SRR0 */
                tc->EMIT(ldr_offset(ctx, reg_rd, __builtin_offsetof(PPCState, SRR0)));
                break;
            case 27:    /* SRR1 */
                tc->EMIT(ldr_offset(ctx, reg_rd, __builtin_offsetof(PPCState, SRR1)));
                break;
            case 272:   /* SPRG0 */
                tc->EMIT(ldr_offset(ctx, reg_rd, __builtin_offsetof(PPCState, SPRG[0])));
                break;
            case 273:   /* SPRG1 */
                tc->EMIT(ldr_offset(ctx, reg_rd, __builtin_offsetof(PPCState, SPRG[1])));
                break;
            case 274:   /* SPRG2 */
                tc->EMIT(ldr_offset(ctx, reg_rd, __builtin_offsetof(PPCState, SPRG[2])));
                break;
            case 275:   /* SPRG3 */
                tc->EMIT(ldr_offset(ctx, reg_rd, __builtin_offsetof(PPCState, SPRG[3])));
                break;
            case 276:   /* SPRG4 */
                tc->EMIT(ldr_offset(ctx, reg_rd, __builtin_offsetof(PPCState, SPRG[4])));
                break;
            case 277:   /* SPRG5 */
                tc->EMIT(ldr_offset(ctx, reg_rd, __builtin_offsetof(PPCState, SPRG[5])));
                break;
            case 278:   /* SPRG6 */
                tc->EMIT(ldr_offset(ctx, reg_rd, __builtin_offsetof(PPCState, SPRG[6])));
                break;
            case 279:   /* SPRG7 */
                tc->EMIT(ldr_offset(ctx, reg_rd, __builtin_offsetof(PPCState, SPRG[7])));
                break;
            case 287:   /* PVR */
                tc->EMIT({
                    mov_immed_u16(reg_rd, (EMU68_VERSION_MAJOR << 8) | EMU68_VERSION_MINOR, 0),
                    movk_immed_u16(reg_rd, 0xee68, 1)
                });
                break;
            case 916:   /* JIT_CACHE_TOTAL */
                tc->EMIT(ldr_offset(ctx, reg_rd, __builtin_offsetof(PPCState, JIT_CACHE_TOTAL)));
                break;
            case 917:   /* JIT_CACHE_FREE */
                tc->EMIT(ldr_offset(ctx, reg_rd, __builtin_offsetof(PPCState, JIT_CACHE_FREE)));
                break;
            case 918:   /* JIT_CACHE_MISS */
                tc->EMIT(ldr_offset(ctx, reg_rd, __builtin_offsetof(PPCState, JIT_CACHE_MISS)));
                break;
            case 919:   /* JIT_UNIT_COUNT */
                tc->EMIT(ldr_offset(ctx, reg_rd, __builtin_offsetof(PPCState, JIT_UNIT_COUNT)));
                break;
            case 920:   /* JIT_CONTROL */
                tc->EMIT(ldr_offset(ctx, reg_rd, __builtin_offsetof(PPCState, JIT_CONTROL)));
                break;
            case 921:   /* JIT_CONTROL_2 */
                tc->EMIT(ldr_offset(ctx, reg_rd, __builtin_offsetof(PPCState, JIT_CONTROL2)));
                break;
            case 944:   /* BASE */
                tc->EMIT(ldr_offset(ctx, reg_rd, __builtin_offsetof(PPCState, BASEREG)));
                break;
        }

        FreeARMRegister(tc, tmp);

        if (!tc->tc_SupervisorChecked)
        {
            /* Now insert the program exception path - raise exception if RFI was not allowed */
            uint32_t *exit_code_start = tc->tc_CodePtr;
            tc->EMIT_Exception(0x700);
            uint32_t *exit_code_end = tc->tc_CodePtr;

            /* Insert fixup location */
            tc->EMIT({ 
                (uint32_t)(exit_code_end - jump_location),
                fixup_type,
                1,
                (uint32_t)(exit_code_end - exit_code_start),
                INSN_TO_LE(MARKER_EXIT_BLOCK)
            });

            tc->tc_SupervisorChecked = true;
        }

        tc->AdvancePC(4);
        tc->tc_PPCCodePtr++;
        return 1;
    }
    else
    {
        uint8_t tmp;

        /* Accessing user level SPRs */
        switch(spr) {
            case 1:
                tc->EMIT( mov_reg(reg_rd, MapGPRForRead(tc, XERn)));
                break;
            case 8:
                tc->EMIT( mov_reg(reg_rd, MapGPRForRead(tc, LRn)));
                break;
            case 9:
                tc->EMIT( mov_reg(reg_rd, MapGPRForRead(tc, CTRn)));
                break;
            case 900: /* INSNCNTLO - lower 32 bits of PPC instruction counter */
                tmp = AllocARMRegister(tc);
                tc->EMIT({ 
                    mov_simd_to_reg(tmp, CTX_INSN_COUNT),
                    add64_immed(tmp, tmp, tc->tc_InsnCount & 0xfff)
                });
                if (tc->tc_InsnCount & 0xfff000)
                    tc->EMIT(add64_immed_lsl12(tmp, tmp, tc->tc_InsnCount >> 12));
                tc->EMIT(mov_reg(reg_rd, tmp));
                FreeARMRegister(tc, tmp);
                break;
            case 901: /* INSNCNTHI - higher 32 bits of PPC instruction counter */
                tmp = AllocARMRegister(tc);
                tc->EMIT({
                    mov_simd_to_reg(tmp, CTX_INSN_COUNT),
                    add64_immed(tmp, tmp, tc->tc_InsnCount & 0xfff)
                });
                if (tc->tc_InsnCount & 0xfff000)
                    tc->EMIT(add64_immed_lsl12(tmp, tmp, tc->tc_InsnCount >> 12));
                tc->EMIT(lsr64(reg_rd, tmp, 32));
                FreeARMRegister(tc, tmp);
                break;
            case 902: /* ARMCNTLO - lower 32 bits of ARM instruction counter */
                tmp = AllocARMRegister(tc);
                tc->EMIT({
                    mrs(tmp, sys_PMCCNTR_EL0),
                    mov_reg(reg_rd, tmp)
                });
                FreeARMRegister(tc, tmp);
                break;
            case 903: /* ARMCNTHI - higher 32 bits of ARM instruction counter */
                tmp = AllocARMRegister(tc);
                tc->EMIT({
                    mrs(tmp, sys_PMCCNTR_EL0),
                    lsr64(reg_rd, tmp, 32)
                });
                FreeARMRegister(tc, tmp);
                break;

            default:
                return -1;
        }
    }

    tc->tc_PPCCodePtr++;
    tc->AdvancePC(4);
    return 1;
}

int EMIT_tw(struct PPCTranslatorContext *tc, uint32_t opcode)
{
    /* sanity check */
    if (opcode & 0x00000001) return -1;

    uint8_t to = (opcode >> 21) & 31;

    if (to == 0)
    {
        /* Not expecting any condition generating exception so skip */
        tc->EMIT(nop());
        
        tc->tc_PPCCodePtr++;
        tc->AdvancePC(4);
        return 1;
    }
    else if (to == 31)
    {
        /* All bits set, trap always */
        tc->EMIT_Exception(0x700);
        tc->STOP();
        return 1;
    }

    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;

    uint8_t reg_ra = MapGPRForRead(tc, ra);
    uint8_t reg_rb = MapGPRForRead(tc, rb);
    int number_of_cases = 0;
    uint32_t *locations[5];

    /* Emit comparison */
    tc->EMIT(cmp_reg(reg_ra, reg_rb, LSL, 0));
    
    if (to & 1) {
        
        tc->EMIT(b_cc(A64_CC_HI, 0));
        locations[number_of_cases++] = tc->tc_CodePtr - 1;
    }
    if (to & 2) {
        
        tc->EMIT(b_cc(A64_CC_CC, 0));
        locations[number_of_cases++] = tc->tc_CodePtr - 1;
    }
    if (to & 4) {
        
        tc->EMIT(b_cc(A64_CC_EQ, 0));
        locations[number_of_cases++] = tc->tc_CodePtr - 1;
    }
    if (to & 8) {
        
        tc->EMIT(b_cc(A64_CC_GT, 0));
        locations[number_of_cases++] = tc->tc_CodePtr - 1;
    }
    if (to & 16) {
        
        tc->EMIT(b_cc(A64_CC_LT, 0));
        locations[number_of_cases++] = tc->tc_CodePtr - 1;
    }

    uint32_t *exit_code_start = tc->tc_CodePtr;
    tc->EMIT_Exception(0x700);
    uint32_t *exit_code_end = tc->tc_CodePtr;

    /* Insert additional exit points */
    for (int i=0; i < number_of_cases; i++) {
        tc->EMIT({ 
            (uint32_t)(exit_code_end - locations[i]),
            FIXUP_BCC
        });
    }

    tc->EMIT({
        (uint32_t)number_of_cases,
        (uint32_t)(exit_code_end - exit_code_start),
        INSN_TO_LE(MARKER_EXIT_BLOCK)
    });

    tc->tc_PPCCodePtr++;
    tc->AdvancePC(4);
    return 1;
}

int EMIT_twi(struct PPCTranslatorContext *tc, uint32_t opcode)
{
    uint8_t to = (opcode >> 21) & 31;

    if (to == 0)
    {
        /* Not expecting any condition generating exception so skip */
        tc->EMIT(nop());
        
        tc->tc_PPCCodePtr++;
        tc->AdvancePC(4);
        return 1;
    }
    else if (to == 31)
    {
        /* All bits set, trap always */
        tc->EMIT_Exception(0x700);
        tc->STOP();
        return 1;
    }

    uint8_t ra = (opcode >> 16) & 31;
    uint16_t imm = opcode & 0xffff;
    int32_t simm = (int16_t)imm;

    uint8_t reg_ra = MapGPRForRead(tc, ra);

    int number_of_cases = 0;
    uint32_t *locations[5];

    /* Is the immediate in range for CMP? */
    if ((simm & 0xfffff000) == 0) {
        tc->EMIT( cmp_immed(reg_ra, simm));
    }
    else if ((simm & 0xff000fff) == 0) {
        tc->EMIT( cmp_immed_lsl12(reg_ra, simm >> 12));
    }
    else {
        uint8_t tmp = AllocARMRegister(tc);

        tc->LoadImmediate(tmp, simm);
        tc->EMIT( cmp_reg(reg_ra, tmp, LSL, 0));

        FreeARMRegister(tc, tmp);
    }

    if (to & 1) {
        
        tc->EMIT(b_cc(A64_CC_HI, 0));
        locations[number_of_cases++] = tc->tc_CodePtr - 1;
    }
    if (to & 2) {
        
        tc->EMIT(b_cc(A64_CC_CC, 0));
        locations[number_of_cases++] = tc->tc_CodePtr - 1;
    }
    if (to & 4) {
        
        tc->EMIT(b_cc(A64_CC_EQ, 0));
        locations[number_of_cases++] = tc->tc_CodePtr - 1;
    }
    if (to & 8) {
        
        tc->EMIT(b_cc(A64_CC_GT, 0));
        locations[number_of_cases++] = tc->tc_CodePtr - 1;
    }
    if (to & 16) {
        
        tc->EMIT(b_cc(A64_CC_LT, 0));
        locations[number_of_cases++] = tc->tc_CodePtr - 1;
    }

    uint32_t *exit_code_start = tc->tc_CodePtr;
    tc->EMIT_Exception(0x700);
    uint32_t *exit_code_end = tc->tc_CodePtr;

    /* Insert additional exit points */
    for (int i=0; i < number_of_cases; i++) {
        tc->EMIT({ 
            (uint32_t)(exit_code_end - locations[i]),
            FIXUP_BCC
        });
    }

    tc->EMIT({
        (uint32_t)number_of_cases,
        (uint32_t)(exit_code_end - exit_code_start),
        INSN_TO_LE(MARKER_EXIT_BLOCK)
    });

    tc->tc_PPCCodePtr++;
    tc->AdvancePC(4);
    return 1;
}

int EMIT_eieio(struct PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 0x03fff800) return -1;

    tc->EMIT( dmb_sy());

    tc->tc_PPCCodePtr++;
    tc->AdvancePC(4);
    return 1;
}

int EMIT_icbi(struct PPCTranslatorContext *tc, uint32_t opcode)
{
    extern LRUCache cache;

    /* Sanity check */
    if (opcode & 0x03e00001) return -1;

    uint8_t base = AllocARMRegister(tc);
    uint8_t cnt = AllocARMRegister(tc);
    uint8_t fill = AllocARMRegister(tc);
    
    /*
        PPC Architecture allows that icbi flushes more than requested. This is what
        will be done here.
    */
    tc->EMIT({
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
    
    FreeARMRegister(tc, cnt);
    FreeARMRegister(tc, fill);
    FreeARMRegister(tc, base);

    tc->tc_PPCCodePtr++;
    tc->AdvancePC(4);
    return 1;
}

int EMIT_sync(struct PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 0x001ff800) return -1;

    uint8_t op = (opcode >> 21) & 31;

    switch(op) {
        case 0:
            tc->EMIT(isb());
            break;
        case 1:
            tc->EMIT(dmb_sy());
            break;
        default:
            return -1;
    }

    tc->tc_PPCCodePtr++;
    tc->AdvancePC(4);
    return 1;
}

int EMIT_dcbst(struct PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 0x03e00001) return -1;

    uint8_t ra = (opcode >> 21) & 31;
    uint8_t rb = (opcode >> 16) & 31;
    uint8_t reg_rb = MapGPRForRead(tc, rb);
    uint8_t base;

    if (ra == 0) {
        base = reg_rb;
    } else {
        uint8_t reg_ra = MapGPRForRead(tc, ra);
        base = AllocARMRegister(tc);
        tc->EMIT(add_reg(base, reg_ra, reg_rb, LSL, 0));
    }

    tc->EMIT({
        dsb_sy(),
        dc_cvac(base),
        dsb_sy()
    });

    if (ra != 0)
        FreeARMRegister(tc, base);

    tc->tc_PPCCodePtr++;
    tc->AdvancePC(4);
    return 1;
}

int EMIT_dcbtst(struct PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 0x03e00001) return -1;

    uint8_t ra = (opcode >> 21) & 31;
    uint8_t rb = (opcode >> 16) & 31;
    uint8_t reg_rb = MapGPRForRead(tc, rb);
    uint8_t base;

    if (ra == 0) {
        base = reg_rb;
    } else {
        uint8_t reg_ra = MapGPRForRead(tc, ra);
        base = AllocARMRegister(tc);
        tc->EMIT(add_reg(base, reg_ra, reg_rb, LSL, 0));
    }

    tc->EMIT(prfm_pst(base));

    if (ra != 0)
        FreeARMRegister(tc, base);

    tc->tc_PPCCodePtr++;
    tc->AdvancePC(4);
    return 1;
}

int EMIT_dcbt(struct PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 0x03e00001) return -1;

    uint8_t ra = (opcode >> 21) & 31;
    uint8_t rb = (opcode >> 16) & 31;
    uint8_t reg_rb = MapGPRForRead(tc, rb);
    uint8_t base;

    if (ra == 0) {
        base = reg_rb;
    } else {
        uint8_t reg_ra = MapGPRForRead(tc, ra);
        base = AllocARMRegister(tc);
        tc->EMIT(add_reg(base, reg_ra, reg_rb, LSL, 0));
    }

    tc->EMIT(prfm_pld(base));

    if (ra != 0)
        FreeARMRegister(tc, base);

    tc->tc_PPCCodePtr++;
    tc->AdvancePC(4);
    return 1;
}

int EMIT_dcbf(struct PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 0x03e00001) return -1;

    uint8_t ra = (opcode >> 21) & 31;
    uint8_t rb = (opcode >> 16) & 31;
    uint8_t reg_rb = MapGPRForRead(tc, rb);
    uint8_t base;

    if (ra == 0) {
        base = reg_rb;
    } else {
        uint8_t reg_ra = MapGPRForRead(tc, ra);
        base = AllocARMRegister(tc);
        tc->EMIT(add_reg(base, reg_ra, reg_rb, LSL, 0));
    }

    tc->EMIT({
        dsb_sy(),
        dc_civac(base),
        dsb_sy()
    });

    if (ra != 0)
        FreeARMRegister(tc, base);

    tc->tc_PPCCodePtr++;
    tc->AdvancePC(4);
    return 1;
}

int EMIT_dcbz(struct PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 0x03e00001) return -1;

    uint8_t ra = (opcode >> 21) & 31;
    uint8_t rb = (opcode >> 16) & 31;
    uint8_t reg_rb = MapGPRForRead(tc, rb);
    uint8_t base;

    if (ra == 0) {
        base = reg_rb;
    } else {
        uint8_t reg_ra = MapGPRForRead(tc, ra);
        base = AllocARMRegister(tc);
        tc->EMIT(add_reg(base, reg_ra, reg_rb, LSL, 0));
    }

    tc->EMIT({
        dsb_sy(),
        dc_zva(base),
        dsb_sy()
    });

    if (ra != 0)
        FreeARMRegister(tc, base);

    tc->tc_PPCCodePtr++;
    tc->AdvancePC(4);
    return 1;
}

int EMIT_dcba(struct PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 0x03e00001) return -1;

    uint8_t ra = (opcode >> 21) & 31;
    uint8_t rb = (opcode >> 16) & 31;
    uint8_t reg_rb = MapGPRForRead(tc, rb);
    uint8_t base;

    if (ra == 0) {
        base = reg_rb;
    } else {
        uint8_t reg_ra = MapGPRForRead(tc, ra);
        base = AllocARMRegister(tc);
        tc->EMIT(add_reg(base, reg_ra, reg_rb, LSL, 0));
    }

    tc->EMIT({
        prfm_pst(base)
    });

    if (ra != 0)
        FreeARMRegister(tc, base);

    tc->tc_PPCCodePtr++;
    tc->AdvancePC(4);
    return 1;
}

int EMIT_dcbi(struct PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 0x03e00001) return -1;

    uint8_t ra = (opcode >> 21) & 31;
    uint8_t rb = (opcode >> 16) & 31;
    uint8_t reg_rb = MapGPRForRead(tc, rb);
    uint8_t base;

    uint8_t ctx = GetCTX(tc);
    uint8_t tmp = AllocARMRegister(tc);

    /* 
        This fetches must not be context synchronizing and one supervisor check per
        code block is sufficient.
    */
    if (!tc->tc_SupervisorChecked)
    {
        /* We need to flush PC now, just in case */
        tc->FlushPC();

        /* 
            Fetch MSR and check if exceptions are enabled - it is illegal to call
            RFI from user
        */
        tc->EMIT({
            ldr_offset(ctx, tmp, __builtin_offsetof(PPCState, MSR)),
            tbnz(tmp, 14, 0)
        });
    }

    uint32_t fixup_type = FIXUP_TBZ;
    uint32_t *jump_location = tc->tc_CodePtr - 1;

    if (ra == 0) {
        base = reg_rb;
    } else {
        uint8_t reg_ra = MapGPRForRead(tc, ra);
        base = AllocARMRegister(tc);
        tc->EMIT(add_reg(base, reg_ra, reg_rb, LSL, 0));
    }

    tc->EMIT({
        dc_ivac(base)
    });

    if (ra != 0)
        FreeARMRegister(tc, base);

    if (!tc->tc_SupervisorChecked)
    {
        /* Now insert the program exception path - raise exception if RFI was not allowed */
        uint32_t *exit_code_start = tc->tc_CodePtr;
        tc->EMIT_Exception(0x700);
        uint32_t *exit_code_end = tc->tc_CodePtr;

        /* Insert fixup location */
        tc->EMIT({ 
            (uint32_t)(exit_code_end - jump_location),
            fixup_type,
            1,
            (uint32_t)(exit_code_end - exit_code_start),
            INSN_TO_LE(MARKER_EXIT_BLOCK)
        });

        tc->tc_SupervisorChecked = true;
    }
    
    tc->tc_PPCCodePtr++;
    tc->AdvancePC(4);
    return 1;
}

int EMIT_sc(struct PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode != 0x44000002) return -1;

    /* Advance program counter by 4 */
    tc->AdvancePC(4);

    /* Flush it - this is a point of no return */
    tc->FlushPC();

    /* Emit exception itself */
    uint8_t ctx = GetCTX(tc);
    uint8_t tmp = AllocARMRegister(tc);

    tc->EMIT({
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

    FreeARMRegister(tc, tmp);

    tc->STOP();

    return 1;
}

int EMIT_rfi(struct PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode != 0x4c000064) return -1;

    uint8_t ctx = GetCTX(tc);
    uint8_t tmp = AllocARMRegister(tc);

    /* 
        Fetch MSR and check if exceptions are enabled - it is illegal to call
        RFI from user
    */
    tc->EMIT({
        ldr_offset(ctx, tmp, __builtin_offsetof(PPCState, MSR)),
        tbnz(tmp, 14, 0)
    });

    /* Emit jump, remember its location and fixup type */
    uint32_t fixup_type = FIXUP_TBZ;
    uint32_t *jump_location = tc->tc_CodePtr - 1;

    tc->EMIT({
        /* Load SRR1, clear POW bit, store into MSR */
        ldr_offset(ctx, tmp, __builtin_offsetof(PPCState, SRR1)),
        bic_immed(tmp, tmp, 1, 14),
        str_offset(ctx, tmp, __builtin_offsetof(PPCState, MSR)),

        /* Load SRR0, store into PC */
        ldr_offset(ctx, REG_PC, __builtin_offsetof(PPCState, SRR0)),
        bic_immed(REG_PC, REG_PC, 2, 0),
    });

    FreeARMRegister(tc, tmp);

    /* This instruction exits the JIT loop */
    tc->STOP();

    /* Now insert the program exception path - raise exception if RFI was not allowed */
    uint32_t *exit_code_start = tc->tc_CodePtr;
    tc->EMIT_Exception(0x700);
    uint32_t *exit_code_end = tc->tc_CodePtr;

    /* Insert fixup location */
    tc->EMIT({ 
        (uint32_t)(exit_code_end - jump_location),
        fixup_type,
        1,
        (uint32_t)(exit_code_end - exit_code_start),
        INSN_TO_LE(MARKER_EXIT_BLOCK)
    });

    return 1;
}

} // Emu68::PPC
