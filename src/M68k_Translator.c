/*
    Copyright © 2019 Michal Schulz <michal.schulz@gmx.de>
    https://github.com/michalsc

    This Source Code Form is subject to the terms of the
    Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed
    with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#define _GNU_SOURCE 1

#include "support.h"
#include "EmuFeatures.h"
#include "M68k.h"
#include "RegisterAllocator.h"
#include "lists.h"
#include "tlsf.h"
#include "config.h"
#include "DuffCopy.h"
#include "disasm.h"
#include "cache.h"

#if SET_FEATURES_AT_RUNTIME
features_t Features;
#endif

#if SET_OPTIONS_AT_RUNTIME
options_t Options = {
    EMU68_M68K_INSN_DEPTH,
    1,
};
#endif

int disasm = 0;
int debug = 0;
const int debug_cnt = 0;

static inline int globalDebug() {
    return debug;
}

static inline int globalDisasm() {
    return disasm;
}

struct List ICache[EMU68_HASHSIZE];
struct List LRU;
static uint32_t *temporary_arm_code;
static struct M68KLocalState *local_state;

int32_t _pc_rel = 0;

void EMIT_GetOffsetPC(struct TranslatorContext *ctx, int8_t *offset)
{
    // Calculate new PC relative offset
    int new_offset = _pc_rel + *offset;

    // If overflow would occur then compute PC and get new offset
    if (new_offset > 127 || new_offset < -127)
    {
        if (_pc_rel > 0)
            EMIT(ctx, add_immed(REG_PC, REG_PC, _pc_rel));
        else
            EMIT(ctx, sub_immed(REG_PC, REG_PC, -_pc_rel));

        _pc_rel = 0;
        new_offset = *offset;
    }

    *offset = new_offset;
}

void EMIT_AdvancePC(struct TranslatorContext *ctx, uint8_t offset)
{
//if (debug)    kprintf("Emit_AdvancePC(pc_rel=%d, off=%d)\n", _pc_rel, (int)offset);
    // Calculate new PC relative offset
    _pc_rel += (int)offset;

    // If overflow would occur then compute PC and get new offset
    if (_pc_rel > 120 || _pc_rel < -120)
    {
        if (_pc_rel > 0)
            EMIT(ctx, add_immed(REG_PC, REG_PC, _pc_rel));
        else
            EMIT(ctx, sub_immed(REG_PC, REG_PC, -_pc_rel));

        _pc_rel = 0;
    }
}

void EMIT_FlushPC(struct TranslatorContext *ctx)
{
    if (_pc_rel > 0)
            EMIT(ctx, add_immed(REG_PC, REG_PC, _pc_rel));
    else if (_pc_rel < 0)
            EMIT(ctx, sub_immed(REG_PC, REG_PC, -_pc_rel));

    _pc_rel = 0;
}

void EMIT_ResetOffsetPC(struct TranslatorContext *)
{
    _pc_rel = 0;
}

uint32_t EMIT_lineA(struct TranslatorContext *ctx)
{
    uint16_t opcode = cache_read_16(ICACHE, (uintptr_t)ctx->tc_M68kCodePtr++);

    EMIT_FlushPC(ctx);
    if (debug)
        EMIT_InjectDebugString(ctx, "[JIT] LINE A exception (opcode %04x) at %08x not implemented\n", opcode, ctx->tc_M68kCodePtr - 1);

    EMIT_Exception(ctx, VECTOR_LINE_A, 0);

    EMIT(ctx, INSN_TO_LE(0xffffffff));

    return 1;
}

static uint32_t (*line_array[16])(struct TranslatorContext *ctx) = {
    EMIT_line0,
    EMIT_move,
    EMIT_move,
    EMIT_move,
    EMIT_line4,
    EMIT_line5,
    EMIT_line6,
    EMIT_moveq,
    EMIT_line8,
    EMIT_line9,
    EMIT_lineA,
    EMIT_lineB,
    EMIT_lineC,
    EMIT_lineD,
    EMIT_lineE,
    EMIT_lineF
};

extern struct M68KState *__m68k_state;

static inline uint32_t EmitINSN(struct TranslatorContext *ctx)
{
    uint16_t opcode = cache_read_16(ICACHE, (uint32_t)(uintptr_t)ctx->tc_M68kCodePtr);
    uint8_t group = opcode >> 12;

    if (debug > 2)
    {
        EMIT(ctx,
            hint(0),
            movw_immed_u16(31, opcode),
            movk_immed_u16(31, ((uintptr_t)ctx->tc_M68kCodePtr) >> 16, 1),
            movk_immed_u16(31, ((uintptr_t)ctx->tc_M68kCodePtr), 0)
        );
    }
    if (debug > 1)
        EMIT(ctx, hint(1));
    if (debug_cnt & 1)
    {
        uint8_t reg = RA_AllocARMRegister(ctx);
        EMIT(ctx,
            mov_immed_u16(reg, 1, 0),
            msr(reg, 3, 3, 9, 12, 4)
        );
        RA_FreeARMRegister(ctx, reg);
    }

    if ((__m68k_state->JIT_CONTROL2 & JC2F_CHIP_SLOWDOWN) && (uintptr_t)ctx->tc_M68kCodePtr < 0x200000)
    {
        static uint32_t counter;
        const uint32_t repeat_every = 1 + ((__m68k_state->JIT_CONTROL2 >> JC2B_CHIP_SLOWDOWN_RATIO) & JC2_CHIP_SLOWDOWN_RATIO_MASK);
        if (counter++ % repeat_every == 0)
        {
            int8_t off = 0;
            EMIT_GetOffsetPC(ctx, &off);
            EMIT(ctx, ldurh_offset(REG_PC, 0, off));
        }
    }

    return line_array[group](ctx);
}

void __clear_cache(void *begin, void *end)
{
    arm_flush_cache((uintptr_t)begin, (uintptr_t)end - (uintptr_t)begin);
    arm_icache_invalidate((uintptr_t)begin, (uintptr_t)end - (uintptr_t)begin);
}

#define RTSTACK_SIZE    32
uint16_t *ReturnStack[RTSTACK_SIZE];
uint16_t ReturnStackDepth = 0;

void M68K_PushReturnAddress(uint16_t *ret_addr)
{
    if (ReturnStackDepth >= RTSTACK_SIZE) {
        for (int i=1; i < RTSTACK_SIZE; i++) {
            ReturnStack[i-1] = ReturnStack[i];
        }
        ReturnStackDepth--;
    }

    ReturnStack[ReturnStackDepth++] = ret_addr;
}

uint16_t *M68K_PopReturnAddress(uint8_t *success)
{
    uint16_t *ptr;

    if (EMU68_USE_RETURN_STACK && ReturnStackDepth > 0)
    {
        ptr = ReturnStack[--ReturnStackDepth];

        if (success)
            *success = 1;
    }
    else
    {
        ptr = (uint16_t *)0xffffffff;
        if (success)
            *success = 0;
    }

    return ptr;
}

void M68K_ResetReturnStack()
{
    ReturnStackDepth = 0;
}

uint16_t *m68k_high;
uint16_t *m68k_low;
uint32_t insn_count;
uint32_t prologue_size = 0;
uint32_t epilogue_size = 0;
uint32_t conditionals_count = 0;

void M68K_PrintContext(void *);



uint32_t debug_range_min = 0x00000000;
uint32_t debug_range_max = 0xffffffff;

// Bad hack: two registers holding addresses of Load96bit and Save96bit
uint8_t reg_Load96;
uint8_t reg_Save96;
uint32_t val_FPIAR;

void EMIT_LocalExit(struct TranslatorContext *ctx, uint32_t insn_fixup)
{
#if EMU68_INSN_COUNTER
    EMIT(ctx, mov_simd_to_reg(0, 30, TS_D, 0));
#endif

    RA_StoreDirtyFPURegs(ctx);
    RA_StoreDirtyM68kRegs(ctx);

    EMIT_FlushPC(ctx);

    RA_StoreCC(ctx);
    RA_StoreFPCR(ctx);
    RA_StoreFPSR(ctx);

#if EMU68_INSN_COUNTER
    EMIT(ctx,
        add64_immed(0, 0, (insn_count + insn_fixup) & 0xfff),
        mov_reg_to_simd(30, TS_D, 0, 0)
    );
#else
    (void)insn_fixup;
#endif
    if (val_FPIAR != 0xffffffff)
    {
        EMIT(ctx, 
            mov_immed_u16(0, val_FPIAR & 0xffff, 0),
            movk_immed_u16(0, val_FPIAR >> 16, 1),
            mov_reg_to_simd(29, TS_S, 1, 0)
        );
    }

    EMIT(ctx, bx_lr());
}

uint16_t * m68k_entry_point;
uint8_t host_flags;

struct DisasmOut {
    uint16_t *do_M68kAddr;
    uint32_t *do_ArmAddr;
    uint32_t do_M68kCount;
    uint32_t do_ArmCount;
} disasm_items[512], *disasm_ptr;

static inline uintptr_t M68K_Translate(uint16_t *M68kCodePtr)
{
    struct List exitList;
    m68k_entry_point = M68kCodePtr;
    uint16_t *orig_m68kcodeptr = M68kCodePtr;
    uintptr_t hash = (uintptr_t)M68kCodePtr;
    int var_EMU68_MAX_LOOP_COUNT = (__m68k_state->JIT_CONTROL >> JCCB_LOOP_COUNT) & JCCB_LOOP_COUNT_MASK;
    if (var_EMU68_MAX_LOOP_COUNT == 0)
        var_EMU68_MAX_LOOP_COUNT = JCCB_LOOP_COUNT_MASK + 1;
    uint32_t var_EMU68_M68K_INSN_DEPTH = (__m68k_state->JIT_CONTROL >> JCCB_INSN_DEPTH) & JCCB_INSN_DEPTH_MASK;
    if (var_EMU68_M68K_INSN_DEPTH == 0)
        var_EMU68_M68K_INSN_DEPTH = JCCB_INSN_DEPTH_MASK + 1;

    struct TranslatorContext ctx;

    ctx.tc_CodePtr = temporary_arm_code;
    ctx.tc_CodeStart = temporary_arm_code;
    ctx.tc_M68kCodePtr = M68kCodePtr;
    ctx.tc_M68kCodeStart = M68kCodePtr;

    uint16_t *last_rev_jump = (uint16_t *)0xffffffff;

    NEWLIST(&exitList);

    disasm_ptr = disasm_items;

    host_flags = 0;
    reg_Load96 = 0xff;
    reg_Save96 = 0xff;
    val_FPIAR = 0xffffffff;

    int debug = 0;
    int disasm = 0;

    if ((uint32_t)(uintptr_t)M68kCodePtr >= debug_range_min && (uint32_t)(uintptr_t)M68kCodePtr <= debug_range_max) {
        debug = globalDebug();
        disasm = globalDisasm();
    }

    if (RA_GetTempAllocMask()) {
        kprintf("[ICache] Temporary register alloc mask on translate start is non-zero %x\n", RA_GetTempAllocMask());

        while(1);
    }

    if (disasm) {
        disasm_open();
    }

    M68K_ResetReturnStack();

    if (debug) {
        uint32_t hash_calc = (hash >> EMU68_HASHSHIFT) & EMU68_HASHMASK;
        kprintf("[ICache] Creating new translation unit with hash %04x (m68k code @ %p)\n", hash_calc, (void*)M68kCodePtr);
        if (debug > 1)
            M68K_PrintContext(__m68k_state);
    }

    int lr_is_saved = 0;
    
    prologue_size = 0;
    epilogue_size = 0;
    conditionals_count = 0;

    insn_count = 0;

    (void)prologue_size;
    (void)lr_is_saved;

    RA_ClearChangedMask();

    if (debug_cnt & 2)
    {
        uint8_t reg = RA_AllocARMRegister(&ctx);
        EMIT(&ctx,
            mov_immed_u16(reg, 4, 0),
            msr(reg, 3, 3, 9, 12, 4)
        );
        RA_FreeARMRegister(&ctx, reg);
    }

    prologue_size = ctx.tc_CodePtr - ctx.tc_CodeStart;

    int break_loop = FALSE;
    int inner_loop = FALSE;
    int soft_break = FALSE;
    int max_rev_jumps = 0;
    int inner_loop_insn = -1;
    int inner_loop_count = 0;

    m68k_low = ctx.tc_M68kCodePtr;
    m68k_high = ctx.tc_M68kCodePtr + 16;

    while (break_loop == FALSE && soft_break == FALSE && insn_count < var_EMU68_M68K_INSN_DEPTH)
    {
        uint16_t insn_consumed;
        uint16_t * const in_code = ctx.tc_M68kCodePtr;
        uint32_t * const out_code = ctx.tc_CodePtr;

        if (insn_count && ((uintptr_t)ctx.tc_M68kCodePtr < (uintptr_t)local_state[insn_count-1].mls_M68kPtr))
        {
            int found = -1;

            for (int i=insn_count - 1; i >= 0; --i)
            {
                if (local_state[i].mls_M68kPtr == ctx.tc_M68kCodePtr)
                {
                    found = i;
                    break;
                }
            }

            if (found > 0)
            {
                if ((insn_count - found - 1) > (var_EMU68_M68K_INSN_DEPTH - insn_count))
                {
                    break;
                }
            }
        }

        local_state[insn_count].mls_ARMOffset = ctx.tc_CodePtr - ctx.tc_CodeStart;
        local_state[insn_count].mls_M68kPtr = ctx.tc_M68kCodePtr;
        local_state[insn_count].mls_PCRel = _pc_rel;

        insn_consumed = EmitINSN(&ctx);

        if (ctx.tc_M68kCodePtr < m68k_low)
            m68k_low = ctx.tc_M68kCodePtr;
        if (ctx.tc_M68kCodePtr + 16 > m68k_high)
            m68k_high = ctx.tc_M68kCodePtr + 16;

        insn_count+=insn_consumed;
        if (ctx.tc_CodePtr[-1] == INSN_TO_LE(0xfffffff0))
        {
            lr_is_saved = 1;
            ctx.tc_CodePtr--;
        }
        if (ctx.tc_CodePtr[-1] == INSN_TO_LE(MARKER_STOP))
        {
            ctx.tc_CodePtr--;
            break_loop = TRUE;
        }
        if (ctx.tc_CodePtr[-1] == INSN_TO_LE(MARKER_BREAK))
        {
            ctx.tc_CodePtr--;
            soft_break = TRUE;
        }
        if (ctx.tc_CodePtr[-1] == INSN_TO_LE(MARKER_EXIT_BLOCK))
        {
            struct ExitBlock *eb;
            
            ctx.tc_CodePtr -= 4;
            
            uint32_t insn_count = ctx.tc_CodePtr[2];
            uint32_t fixup_type = ctx.tc_CodePtr[1];
            uint32_t fixup_target = ctx.tc_CodePtr[0];

            eb = tlsf_malloc(tlsf, sizeof(struct ExitBlock) + 4 * insn_count);

            eb->eb_Type = MARKER_EXIT_BLOCK;
            eb->eb_InstructionCount = insn_count;
            eb->eb_FixupType = fixup_type;
            eb->eb_FixupLocation = ctx.tc_CodePtr - fixup_target;

            ctx.tc_CodePtr -= insn_count;

            for (unsigned i=0; i < insn_count; i++) {
                eb->eb_ARMCode[i] = ctx.tc_CodePtr[i];
            }

            ADDTAIL(&exitList, eb);
        }
        if (ctx.tc_CodePtr[-1] == INSN_TO_LE(MARKER_DOUBLE_EXIT))
        {
            struct DoubleExitBlock *eb;

            ctx.tc_CodePtr -= 6;

            uint32_t insn_count = ctx.tc_CodePtr[4];
            uint32_t fixup2_type = ctx.tc_CodePtr[3];
            uint32_t fixup2_target = ctx.tc_CodePtr[2];
            uint32_t fixup1_type = ctx.tc_CodePtr[1];
            uint32_t fixup1_target = ctx.tc_CodePtr[0];

            eb = tlsf_malloc(tlsf, sizeof(struct DoubleExitBlock) + 4 * insn_count);

            eb->eb_Type = MARKER_DOUBLE_EXIT;
            eb->eb_InstructionCount = insn_count;
            eb->eb_Fixup1Type = fixup1_type;
            eb->eb_Fixup1Location = ctx.tc_CodePtr - fixup1_target;
            eb->eb_Fixup2Type = fixup2_type;
            eb->eb_Fixup2Location = ctx.tc_CodePtr - fixup2_target;

            ctx.tc_CodePtr -= insn_count;

            for (unsigned i = 0; i < insn_count; i++)
            {
                eb->eb_ARMCode[i] = ctx.tc_CodePtr[i];
            }

            ADDTAIL(&exitList, eb);
        }
        if (ctx.tc_CodePtr[-1] == INSN_TO_LE(0xfffffffe))
        {
            uint32_t *tmpptr;
            uint32_t *branch_mod[10];
            uint32_t branch_cnt;
            int local_branch_done = 0;
            ctx.tc_CodePtr--;
            ctx.tc_CodePtr--;  /* Remove branch target (unused!) */
            branch_cnt = *--ctx.tc_CodePtr;

            for (unsigned i=0; i < branch_cnt; i++)
            {
                uintptr_t ptr = *(uint32_t *)--ctx.tc_CodePtr;
                ptr |= (uintptr_t)ctx.tc_CodePtr & 0xffffffff00000000;
                branch_mod[i] = (uint32_t *)ptr;
            }

            tmpptr = ctx.tc_CodePtr;

            conditionals_count++;

            if (!local_branch_done)
            {
                EMIT_LocalExit(&ctx, 0);
            }
            int distance = ctx.tc_CodePtr - tmpptr;

            for (unsigned i=0; i < branch_cnt; i++) {
                //kprintf("[ICache] Branch modification at %p : distance increase by %d\n", (void*) branch_mod[i], distance);
                *(branch_mod[i]) = INSN_TO_LE((INSN_TO_LE(*(branch_mod[i])) + (distance << 5)));
            }
            epilogue_size += distance;
        }

        if (disasm) {
            disasm_ptr->do_M68kAddr = in_code;
            disasm_ptr->do_ArmAddr = out_code;
            disasm_ptr->do_M68kCount = insn_consumed;
            disasm_ptr->do_ArmCount = ctx.tc_CodePtr - out_code;
            disasm_ptr++;
        }

        if (in_code > ctx.tc_M68kCodePtr)
        {
            if (last_rev_jump == ctx.tc_M68kCodePtr) {
                if (--max_rev_jumps == 0) {
                    break;
                }
            }
            else {
                last_rev_jump = ctx.tc_M68kCodePtr;
                max_rev_jumps = var_EMU68_MAX_LOOP_COUNT - 1;
            }
        }

        if (!break_loop && (orig_m68kcodeptr == ctx.tc_M68kCodePtr))
        {
            inner_loop = TRUE;
            soft_break = TRUE;
        }

        if (inner_loop)
        {
            /* Set inner loop instruction count */
            if (inner_loop_insn == -1) {
                inner_loop_insn = insn_count;
                inner_loop_count = var_EMU68_MAX_LOOP_COUNT;
            }

            /* If inner loop count is not 0 and there is still place for one loop, put it here */
            if (--inner_loop_count) {
                if ((var_EMU68_M68K_INSN_DEPTH - insn_count) > (uint32_t)inner_loop_insn)
                {
                    soft_break = 0;
                }
                else
                {
                    soft_break = 1;
                }
            }
        }
    }

    uint32_t *out_code = ctx.tc_CodePtr;
    uint32_t *tmpptr = ctx.tc_CodePtr;

#if EMU68_INSN_COUNTER
    EMIT(&ctx, mov_simd_to_reg(0, 30, TS_D, 0));
#endif
    RA_FlushFPURegs(&ctx);
    RA_FlushM68kRegs(&ctx);

    EMIT_FlushPC(&ctx);
    RA_FlushCC(&ctx);
    RA_FlushFPCR(&ctx);
    RA_FlushFPSR(&ctx);

#if EMU68_INSN_COUNTER
    EMIT(&ctx, add64_immed(0, 0, insn_count & 0xfff));
#endif

    uint8_t tmp2 = RA_AllocARMRegister(&ctx);
    if (inner_loop)
    {
        uint8_t cpuctx = RA_GetCTX(&ctx);
        EMIT(&ctx, ldr_offset(cpuctx, tmp2, __builtin_offsetof(struct M68KState, INT)));
    }
#if EMU68_INSN_COUNTER
    EMIT(&ctx, mov_reg_to_simd(30, TS_D, 0, 0));
#endif
    if (val_FPIAR != 0xffffffff)
    {
        EMIT(&ctx,
            mov_immed_u16(0, val_FPIAR & 0xffff, 0),
            movk_immed_u16(0, val_FPIAR >> 16, 1),
            mov_reg_to_simd(29, TS_S, 1, 0)
        );
    }

    if (inner_loop)
    {
        uint32_t *tmpptr = ctx.tc_CodePtr;
        EMIT(&ctx, cbz(tmp2, ctx.tc_CodeStart - tmpptr));
    }
    EMIT(&ctx, bx_lr());
    
    uint32_t *_tmpptr = ctx.tc_CodePtr;
    RA_FreeARMRegister(&ctx, tmp2);
    RA_FlushCTX(&ctx);
    ctx.tc_CodePtr = _tmpptr;
    
    epilogue_size += ctx.tc_CodePtr - tmpptr;

    if (disasm) {
        disasm_ptr->do_M68kAddr = NULL;
        disasm_ptr->do_M68kCount = 0;
        disasm_ptr->do_ArmAddr = out_code;
        disasm_ptr->do_ArmCount = ctx.tc_CodePtr - out_code;
        disasm_ptr++;
    }

    /* Get all exit entries and append them here */
    struct ExitBlock *n = NULL;
    //int exit_num = 0;
    while ((n = (struct ExitBlock *)REMHEAD(&exitList)))
    {
        uint32_t *old_end = ctx.tc_CodePtr;
        uint32_t op;

        if (n->eb_Type == MARKER_DOUBLE_EXIT)
        {
            struct DoubleExitBlock *eb2 = (struct DoubleExitBlock *)n;

            for (unsigned i = 0; i < eb2->eb_InstructionCount; i++)
            {
                EMIT(&ctx, eb2->eb_ARMCode[i]);
            }

            switch (eb2->eb_Fixup1Type)
            {
                case FIXUP_BCC:
                    op = I32(*eb2->eb_Fixup1Location);
                    op &= ~(0x7ffff << 5);
                    op |= ((old_end - eb2->eb_Fixup1Location) & 0x7ffff) << 5;
                    *eb2->eb_Fixup1Location = I32(op);
                    break;

                case FIXUP_TBZ:
                    op = I32(*eb2->eb_Fixup1Location);
                    op &= ~(0x3fff << 5);
                    op |= ((old_end - eb2->eb_Fixup1Location) & 0x3fff) << 5;
                    *eb2->eb_Fixup1Location = I32(op);
                    break;

                default:
                    kprintf("[JIT] I don't know how to deal with fixup type 0x%08x\n", eb2->eb_Fixup1Type);
            }

            switch (eb2->eb_Fixup2Type)
            {
                case FIXUP_BCC:
                    op = I32(*eb2->eb_Fixup2Location);
                    op &= ~(0x7ffff << 5);
                    op |= ((old_end - eb2->eb_Fixup2Location) & 0x7ffff) << 5;
                    *eb2->eb_Fixup2Location = I32(op);
                    break;

                case FIXUP_TBZ:
                    op = I32(*eb2->eb_Fixup2Location);
                    op &= ~(0x3fff << 5);
                    op |= ((old_end - eb2->eb_Fixup2Location) & 0x3fff) << 5;
                    *eb2->eb_Fixup2Location = I32(op);
                    break;

                default:
                    kprintf("[JIT] I don't know how to deal with fixup type 0x%08x\n", eb2->eb_Fixup2Type);
            }
        }
        else
        {
            struct ExitBlock *eb = n;
            
            for (unsigned i = 0; i < eb->eb_InstructionCount; i++)
            {
                EMIT(&ctx, eb->eb_ARMCode[i]);
            }

            switch (eb->eb_FixupType)
            {
                case FIXUP_BCC:
                    op = I32(*eb->eb_FixupLocation);
                    op &= ~(0x7ffff << 5);
                    op |= ((old_end - eb->eb_FixupLocation) & 0x7ffff) << 5;
                    *eb->eb_FixupLocation = I32(op);
                    break;

                case FIXUP_TBZ:
                    op = I32(*eb->eb_FixupLocation);
                    op &= ~(0x3fff << 5);
                    op |= ((old_end - eb->eb_FixupLocation) & 0x3fff) << 5;
                    *eb->eb_FixupLocation = I32(op);
                    break;

                default:
                    kprintf("[JIT] I don't know how to deal with fixup type 0x%08x\n", eb->eb_FixupType);
            }
        }

        if (disasm) {
            disasm_ptr->do_M68kAddr = NULL;
            disasm_ptr->do_M68kCount = 0;
            disasm_ptr->do_ArmAddr = old_end;
            disasm_ptr->do_ArmCount = ctx.tc_CodePtr - old_end;
            disasm_ptr++;
        }

        tlsf_free(tlsf, n);
    }

    disasm_ptr->do_ArmAddr = NULL;

    if (disasm) {
        int exit_num = 0;
        for (disasm_ptr = disasm_items; disasm_ptr->do_ArmAddr; disasm_ptr++)
        {
            if (disasm_ptr->do_M68kAddr == NULL) {
                if (exit_num == 0) {
                    kprintf("[JIT] EXIT_DEF:\n");
                } else {
                    kprintf("[JIT] EXIT_%03d:\n", exit_num);
                }
                exit_num++;
            }
            disasm_print(
                disasm_ptr->do_M68kAddr, disasm_ptr->do_M68kCount,
                disasm_ptr->do_ArmAddr, 4 * disasm_ptr->do_ArmCount, temporary_arm_code);
        }
        disasm_close();
    }

    // Put a marker at the end of translation unit
    EMIT(&ctx, 0xffffffff);

    if (reg_Load96)
        RA_FreeARMRegister(NULL, reg_Load96);
    
    if (reg_Save96)
        RA_FreeARMRegister(NULL, reg_Save96);

    if (debug)
    {
        kprintf("[ICache]   Translated %d M68k instructions to %d ARM instructions\n", insn_count, (int)(ctx.tc_CodePtr - ctx.tc_CodeStart));
        kprintf("[ICache]   Prologue size: %d, Epilogue size: %d, Conditionals: %d\n",
            prologue_size, epilogue_size, conditionals_count);
        kprintf("[ICache]   Mean epilogue size pro exit point: %d\n", epilogue_size / (1 + conditionals_count));
        uint32_t mean = 100 * (ctx.tc_CodePtr - ctx.tc_CodeStart - (prologue_size + epilogue_size));
        mean = mean / insn_count;
        uint32_t mean_n = mean / 100;
        uint32_t mean_f = mean % 100;
        kprintf("[ICache]   Mean ARM instructions per m68k instruction: %d.%02d\n", mean_n, mean_f);
    }

    return (uintptr_t)ctx.tc_CodePtr - (uintptr_t)ctx.tc_CodeStart;
}

/*
    Translate portion of m68k code into ARM. No new unit is created, instead
    a raw pointer to ARM code is returned and instruction cache on host side is
    invalidated
*/
void *M68K_TranslateNoCache(uint16_t *m68kcodeptr)
{
    uintptr_t line_length = M68K_Translate(m68kcodeptr);
    void *entry_point = (void*)temporary_arm_code;

    entry_point = (void *)((uintptr_t)entry_point | 0x0000001000000000ULL);

    arm_flush_cache((uintptr_t)entry_point, line_length);
    arm_icache_invalidate((intptr_t)entry_point, line_length);

    return entry_point;
} 

/*
    Verify if the translated code has changed since the unit was created. In order
    to do this MD5 sum of the block is compared with the previousy calculated one.

    If th sums are not same, the block is removed form LRU cache and hashtable and memory
    is released.

    The function returns poitner to verified unit or NULL if the unit changed
*/
struct M68KTranslationUnit *M68K_VerifyUnit(struct M68KTranslationUnit *unit)
{
    if (unit)
    {
        uint32_t crc = CalcCRC32(unit->mt_M68kLow, unit->mt_M68kHigh);

        if (crc != unit->mt_CRC32)
        {
            REMOVE(&unit->mt_LRUNode);
            REMOVE(&unit->mt_HashNode);
            tlsf_free(jit_tlsf, unit);

            __m68k_state->JIT_UNIT_COUNT--;
            __m68k_state->JIT_CACHE_FREE = tlsf_get_free_size(jit_tlsf);

            unit = NULL;
        }
    }

    return unit;
}

/*
    Get M68K code unit from the instruction cache. Return NULL if code was not found and needs to be
    translated first.

    If the code was found, update its position in the LRU cache.
*/
struct M68KTranslationUnit *M68K_GetTranslationUnit(uint16_t *m68kcodeptr)
{
    struct M68KTranslationUnit *unit = NULL; //, *n;
    uintptr_t hash = (uintptr_t)m68kcodeptr;
    uint16_t *orig_m68kcodeptr = m68kcodeptr;

    int debug = 0;

    if ((uint32_t)(uintptr_t)m68kcodeptr >= debug_range_min && (uint32_t)(uintptr_t)m68kcodeptr <= debug_range_max) {
        debug = globalDebug();
    }

    /* Get 16-bit has from the pointer to m68k code */
    hash = (hash >> EMU68_HASHSHIFT) & EMU68_HASHMASK;

    if (debug > 2)
        kprintf("[ICache] GetTranslationUnit(%08x)\n[ICache] Hash: 0x%04x\n", (void*)m68kcodeptr, (int)hash);

    if (unit == NULL)
    {
        uintptr_t line_length = M68K_Translate(m68kcodeptr);
        uintptr_t arm_insn_count = line_length/4 - 1;

        uintptr_t unit_length = (line_length + 63 + sizeof(struct M68KTranslationUnit)) & ~63;

        do {
            unit = tlsf_malloc_aligned(jit_tlsf, unit_length, 64);

            __m68k_state->JIT_CACHE_FREE = tlsf_get_free_size(jit_tlsf);

            if (unit == NULL)
            {
                if (debug > 0) {
                    kprintf("[ICache] Requested block was %d bytes long\n", unit_length);
                }

                for (int i=0; i < 8; i++) {
                    struct Node *n = REMTAIL(&LRU);

                    if (n == NULL)
                        break;

                    void *ptr = (char *)n - __builtin_offsetof(struct M68KTranslationUnit, mt_LRUNode);
                    REMOVE((struct Node *)ptr);
                    if (debug > 0)
                    {    
                        kprintf("[ICache] Run out of cache. Removing least recently used cache line node @ %p\n", ptr);
                    }
                    tlsf_free(jit_tlsf, ptr);
                    __m68k_state->JIT_UNIT_COUNT--;
                }
                __m68k_state->JIT_CACHE_FREE = tlsf_get_free_size(jit_tlsf);
                
                __asm__ volatile("msr tpidr_el1, %0"::"r"(0xffffffff));
            }
        } while(unit == NULL);

        unit->mt_ARMEntryPoint = &unit->mt_ARMCode[0];
        unit->mt_ARMEntryPoint = (void *)((uintptr_t)unit->mt_ARMEntryPoint | 0x0000001000000000ULL);
        unit->mt_M68kInsnCnt = insn_count;
        unit->mt_ARMInsnCnt = arm_insn_count;
        unit->mt_UseCount = 0;
        unit->mt_FetchCount = 0;
        unit->mt_M68kAddress = orig_m68kcodeptr;
        unit->mt_M68kLow = m68k_low;
        unit->mt_M68kHigh = m68k_high;
        unit->mt_CRC32 = CalcCRC32(m68k_low, m68k_high);
        unit->mt_PrologueSize = prologue_size;
        unit->mt_EpilogueSize = epilogue_size;
        unit->mt_Conditionals = conditionals_count;
        DuffCopy(&unit->mt_ARMCode[0], temporary_arm_code, line_length/4);

        ADDHEAD(&LRU, &unit->mt_LRUNode);
        ADDHEAD(&ICache[hash], &unit->mt_HashNode);

        __m68k_state->JIT_UNIT_COUNT++;
        __m68k_state->JIT_CACHE_MISS++;

        if (debug) {
            kprintf("[ICache]   Block checksum: %08x\n", unit->mt_CRC32);
            kprintf("[ICache]   ARM code at %p\n", unit->mt_ARMEntryPoint);
        }

        arm_flush_cache((uintptr_t)&unit->mt_ARMCode, line_length);
        arm_icache_invalidate((intptr_t)unit->mt_ARMEntryPoint, line_length);

        if (debug)
        {
            kprintf("-- ARM Code dump --\n");
            for (uint32_t i=0; i < unit->mt_ARMInsnCnt; i++)
            {
                if ((i % 5) == 0)
                    kprintf("   ");
                uint32_t insn = LE32(unit->mt_ARMCode[i]);
                kprintf(" %02x %02x %02x %02x", insn & 0xff, (insn >> 8) & 0xff, (insn >> 16) & 0xff, (insn >> 24) & 0xff);
                if ((i % 5) == 4)
                    kprintf("\n");
            }
            if (unit->mt_ARMInsnCnt % 5 != 0)
                kprintf("\n");
            if (debug > 3)
            {
                kprintf("\n-- Local State --\n");
                for (unsigned i=0; i < insn_count; i++)
                {
                    kprintf("    %p -> %08x", local_state[i].mls_M68kPtr, local_state[i].mls_ARMOffset);
                    for (int r=0; r < 16; r++) {
                        if (local_state[i].mls_RegMap[r] != 0xff) {
                            kprintf(" %c%d=r%d%s", r < 8 ? 'D' : 'A', r % 8, local_state[i].mls_RegMap[r] & 15,
                            local_state[i].mls_RegMap[r] & 0x80 ? "!":"");
                        }
                    }
                    kprintf(" PC_Rel=%d\n", local_state[i].mls_PCRel);
                }
            }
        }
    }

    //asm volatile ("prfm plil1keep, [%0]"::"r"(unit->mt_ARMEntryPoint));

    return unit;
}

void M68K_InitializeCache()
{
    kprintf("[ICache] Initializing caches\n");

    kprintf("[ICache] Setting up LRU\n");
    NEWLIST(&LRU);

    kprintf("[ICache] Setting up ICache\n");

    temporary_arm_code = tlsf_malloc(jit_tlsf, (JCCB_INSN_DEPTH_MASK + 1) * 16 * 64);
    __m68k_state->JIT_CACHE_FREE = tlsf_get_free_size(jit_tlsf);
    kprintf("[ICache] Temporary code at %p\n", temporary_arm_code);
    local_state = tlsf_malloc(tlsf, sizeof(struct M68KLocalState)*(JCCB_INSN_DEPTH_MASK + 1)*2);
    kprintf("[ICache] ICache array at %p\n", ICache);

    for (int i=0; i < 65536; i++)
        NEWLIST(&ICache[i]);
}

void M68K_DumpStats()
{
    struct M68KTranslationUnit *unit = NULL;
    struct Node *n;
    unsigned cnt = 0;
    uintptr_t size = 0;
    unsigned m68k_count = 0;
    unsigned arm_count = 0;
    unsigned total_arm_count = 0;

    if (debug)
        kprintf("[ICache] Listing translation units:\n");
    ForeachNode(&LRU, n)
    {
        cnt++;
        unit = (void *)((char *)n - __builtin_offsetof(struct M68KTranslationUnit, mt_LRUNode));
        if (debug)
            kprintf("[ICache]   Unit %p, mt_UseCount=%lld, mt_FetchCount=%lld, M68K address %08x (range %08x-%08x)\n[ICache]      M68K insn count=%d, ARM insn count=%d\n", 
                (void*)unit, unit->mt_UseCount, unit->mt_FetchCount,
                (void*)unit->mt_M68kAddress, (void*)unit->mt_M68kLow, (void*)unit->mt_M68kHigh, 
                unit->mt_M68kInsnCnt, unit->mt_ARMInsnCnt);

        size = size + (uintptr_t)(&unit->mt_ARMCode[unit->mt_ARMInsnCnt]) - (uintptr_t)unit;
        m68k_count += unit->mt_M68kInsnCnt;
        total_arm_count += unit->mt_ARMInsnCnt;
        arm_count += unit->mt_ARMInsnCnt - (unit->mt_PrologueSize + unit->mt_EpilogueSize);
    }
    kprintf("[ICache] In total %d units (%d bytes) in cache\n", cnt, size);

    uint32_t mean = 100 * (arm_count);
    mean = mean / m68k_count;
    uint32_t mean_n = mean / 100;
    uint32_t mean_f = mean % 100;
    kprintf("[ICache] Mean ARM instructions per m68k instruction: %d.%02d\n", mean_n, mean_f);

    mean = 100 * (total_arm_count);
    mean = mean / m68k_count;
    mean_n = mean / 100;
    mean_f = mean % 100;
    kprintf("[ICache] Mean total ARM instructions per m68k instruction: %d.%02d\n", mean_n, mean_f);
}

void EMIT_InjectPrintContext(struct TranslatorContext *ctx)
{
    extern void M68K_PrintContext(void*);

    union {
        uint64_t u64;
        uint16_t u16[4];
    } u;

    u.u64 = (uintptr_t)M68K_PrintContext;

    EMIT(ctx, 
        stp64_preindex(31, 0, 1, -80),
        stp64(31, 2, 3, 16),
        stp64(31, 4, 5, 32),
        stp64(31, 6, 7, 48),
        str64_offset(31, 30, 64),

        mrs(0, 3, 3, 13, 0, 3),

        mov64_immed_u16(1, u.u16[3], 0),
        movk64_immed_u16(1, u.u16[2], 1),
        movk64_immed_u16(1, u.u16[1], 2),
        movk64_immed_u16(1, u.u16[0], 3),

        blr(1),

        ldp64(31, 2, 3, 16),
        ldp64(31, 4, 5, 32),
        ldp64(31, 6, 7, 48),
        ldr64_offset(31, 30, 64),
        ldp64_postindex(31, 0, 1, 80)
    );
}

static void put_to_stream(void *d, char c)
{
    char **pptr = (char**)d;
    char *ptr = *pptr;

    *ptr++ = c;

    *pptr = ptr;
}

void EMIT_InjectDebugStringV(struct TranslatorContext *ctx, const char * restrict format, va_list args)
{
    void *tmp;
    uint32_t *tmpptr;

    union {
        uint64_t u64;
        uint16_t u16[4];
    } u;

    u.u64 = (uintptr_t)kprintf;

    EMIT(ctx, stp64_preindex(31, 0, 1, -256));
    for (int i=2; i < 30; i += 2)
        EMIT(ctx, stp64(31, i, i+1, i*8));
    EMIT(ctx, str64_offset(31, 30, 240));

    tmpptr = ctx->tc_CodePtr;
    
    EMIT(ctx, 
        adr(0, 48),

        mov64_immed_u16(1, u.u16[3], 0),
        movk64_immed_u16(1, u.u16[2], 1),
        movk64_immed_u16(1, u.u16[1], 2),
        movk64_immed_u16(1, u.u16[0], 3),

        blr(1)
    );

    for (int i=2; i < 30; i += 2)
        EMIT(ctx, ldp64(31, i, i+1, i*8));
    
    EMIT(ctx, 
        ldr64_offset(31, 30, 240),
        ldp64_postindex(31, 0, 1, 256)
    );

    EMIT(ctx, b(0));
    tmp = ctx->tc_CodePtr;

    *tmpptr = adr(0, (uintptr_t)ctx->tc_CodePtr - (uintptr_t)tmpptr);

    vkprintf_pc(put_to_stream, &tmp, format, args);

    *(char*)tmp = 0;

    tmp = (void*)(((uintptr_t)tmp + 4) & ~3);

    ctx->tc_CodePtr[-1] = b(1 + ((uintptr_t)tmp - (uintptr_t)ctx->tc_CodePtr) / 4);
    
    ctx->tc_CodePtr = (uint32_t *)tmp;
}

void EMIT_InjectDebugString(struct TranslatorContext *ctx, const char * restrict format, ...)
{
    va_list v;
    va_start(v, format);
    EMIT_InjectDebugStringV(ctx, format, v);
    va_end(v);
}
