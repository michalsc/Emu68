#define restrict __restrict__

#include "config.h"
#include "PPC.h"
#include "A64.h"

namespace Emu68::PPC {

void PPCTranslatorContext::EMIT_Exception(uint16_t type)
{
    /* When entering exception, all dirty registers must be stored! */
    // StoreDirtyFPRs(tc);
    StoreDirtyGPRs(this);

    /* Flush program counter */
    FlushPC();

    /* Get PPCState to shuffle regs */
    uint8_t ctx = GetCTX(this);
    uint8_t tmp = AllocARMRegister(this);

    EMIT({
        /* Store MSR into SRR1, Store PC into SRR0 */
        ldr_offset(ctx, tmp, __builtin_offsetof(PPCState, MSR)),
        str_offset(ctx, tmp, __builtin_offsetof(PPCState, SRR1)),
        str_offset(ctx, REG_PC, __builtin_offsetof(PPCState, SRR0)),
        
        /* Set supervisor mode in MSR */
        bic_immed(tmp, tmp, 1, 18),
        str_offset(ctx, tmp, __builtin_offsetof(PPCState, MSR)),

        /* Load new program counter */
        mov_immed_u16(REG_PC, type, 0),
        movk_immed_u16(REG_PC, 0xfff0, 1),
    });

    FreeARMRegister(this, tmp);

    LocalExit(0);
}

void PPCTranslatorContext::LocalExit(uint32_t insn_fixup)
{
#if EMU68_INSN_COUNTER
    uint8_t icnt_reg = AllocARMRegister(this);
    EMIT( mov_simd_to_reg(icnt_reg, CTX_INSN_COUNT));
#endif

    StoreDirtyFPRs(this);
    StoreDirtyGPRs(this);

    FlushPC();

#if EMU68_INSN_COUNTER
    EMIT({
        add64_immed(icnt_reg, icnt_reg, (tc_InsnCount + insn_fixup) & 0xfff),
        mov_reg_to_simd(CTX_INSN_COUNT, icnt_reg)
    });
    FreeARMRegister(this, icnt_reg);
#else
    (void)insn_fixup;
#endif

    EMIT( bx_lr());
}

}
