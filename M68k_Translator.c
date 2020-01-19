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


#if SET_FEATURES_AT_RUNTIME
features_t Features;
#endif

#if SET_OPTIONS_AT_RUNTIME
options_t Options = {
    EMU68_M68K_INSN_DEPTH,
    1,
};
#endif

const int debug = 1;

static struct List *ICache;
struct List LRU;
static uint32_t *temporary_arm_code;
static struct M68KLocalState *local_state;

int32_t _pc_rel = 0;

uint32_t *EMIT_GetOffsetPC(uint32_t *ptr, int8_t *offset)
{
    // Calculate new PC relative offset
    int new_offset = _pc_rel + *offset;

    // If overflow would occur then compute PC and get new offset
    while (new_offset > 127 || new_offset < -127)
    {
        if (_pc_rel > 0)
            *ptr++ = add_immed(REG_PC, REG_PC, _pc_rel);
        else
            *ptr++ = sub_immed(REG_PC, REG_PC, -_pc_rel);

        _pc_rel = 0;
    }

    *offset = new_offset;

    return ptr;
}

uint32_t *EMIT_AdvancePC(uint32_t *ptr, uint8_t offset)
{
//if (debug)    kprintf("Emit_AdvancePC(pc_rel=%d, off=%d)\n", _pc_rel, (int)offset);
    // Calculate new PC relative offset
    _pc_rel += (int)offset;

    // If overflow would occur then compute PC and get new offset
    if (_pc_rel > 120 || _pc_rel < -120)
    {
        if (_pc_rel > 0)
            *ptr++ = add_immed(REG_PC, REG_PC, _pc_rel);
        else
            *ptr++ = sub_immed(REG_PC, REG_PC, -_pc_rel);

        _pc_rel = 0;
    }

    return ptr;
}

uint32_t *EMIT_FlushPC(uint32_t *ptr)
{
    if (_pc_rel > 0)
            *ptr++ = add_immed(REG_PC, REG_PC, _pc_rel);
    else if (_pc_rel < 0)
            *ptr++ = sub_immed(REG_PC, REG_PC, -_pc_rel);

    _pc_rel = 0;

    return ptr;
}

uint32_t *EMIT_ResetOffsetPC(uint32_t *ptr)
{
    _pc_rel = 0;
    return ptr;
}

uint32_t *EMIT_lineA(uint32_t *arm_ptr, uint16_t **m68k_ptr)
{
    (*m68k_ptr)++;
    *arm_ptr++ = udf(0xaaaa);

    return arm_ptr;
}

#ifdef __aarch64__

uint32_t *EMIT_line0(uint32_t *arm_ptr, uint16_t **m68k_ptr)
{
    (*m68k_ptr)++;
    *arm_ptr++ = udf(0x0000);

    return arm_ptr;
}

uint32_t *EMIT_line4(uint32_t *arm_ptr, uint16_t **m68k_ptr)
{
    (*m68k_ptr)++;
    *arm_ptr++ = udf(0x4444);

    return arm_ptr;
}

uint32_t *EMIT_line5(uint32_t *arm_ptr, uint16_t **m68k_ptr)
{
    (*m68k_ptr)++;
    *arm_ptr++ = udf(0x5555);

    return arm_ptr;
}

uint32_t *EMIT_line6(uint32_t *arm_ptr, uint16_t **m68k_ptr)
{
    (*m68k_ptr)++;
    *arm_ptr++ = udf(0x6666);

    return arm_ptr;
}

uint32_t *EMIT_line8(uint32_t *arm_ptr, uint16_t **m68k_ptr)
{
    (*m68k_ptr)++;
    *arm_ptr++ = udf(0x8888);

    return arm_ptr;
}

uint32_t *EMIT_line9(uint32_t *arm_ptr, uint16_t **m68k_ptr)
{
    (*m68k_ptr)++;
    *arm_ptr++ = udf(0x9999);

    return arm_ptr;
}

uint32_t *EMIT_lineB(uint32_t *arm_ptr, uint16_t **m68k_ptr)
{
    (*m68k_ptr)++;
    *arm_ptr++ = udf(0x6666);

    return arm_ptr;
}

uint32_t *EMIT_lineC(uint32_t *arm_ptr, uint16_t **m68k_ptr)
{
    (*m68k_ptr)++;
    *arm_ptr++ = udf(0x6666);

    return arm_ptr;
}
uint32_t *EMIT_lineD(uint32_t *arm_ptr, uint16_t **m68k_ptr)
{
    (*m68k_ptr)++;
    *arm_ptr++ = udf(0x6666);

    return arm_ptr;
}
uint32_t *EMIT_lineE(uint32_t *arm_ptr, uint16_t **m68k_ptr)
{
    (*m68k_ptr)++;
    *arm_ptr++ = udf(0x6666);

    return arm_ptr;
}
uint32_t *EMIT_lineF(uint32_t *arm_ptr, uint16_t **m68k_ptr)
{
    (*m68k_ptr)++;
    *arm_ptr++ = udf(0x6666);

    return arm_ptr;
}
#endif

static uint32_t * (*line_array[16])(uint32_t *arm_ptr, uint16_t **m68k_ptr) = {
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

static inline uint32_t *EmitINSN(uint32_t *arm_ptr, uint16_t **m68k_ptr)
{
    uint32_t *ptr = arm_ptr;
    uint16_t opcode = BE16((*m68k_ptr)[0]);
    uint8_t group = opcode >> 12;

    ptr = line_array[group](arm_ptr, m68k_ptr);

    return ptr;
}

void __clear_cache(void *begin, void *end)
{
    arm_flush_cache((uintptr_t)begin, (uintptr_t)end - (uintptr_t)begin);
    arm_icache_invalidate((uintptr_t)begin, (uintptr_t)end - (uintptr_t)begin);
}

#ifndef __aarch64__

static uint8_t got_CC = 0;
static uint8_t mod_CC = 0;

static uint8_t reg_FPCR = 0xff;
static uint8_t mod_FPCR = 0;
static uint8_t reg_FPSR = 0xff;
static uint8_t mod_FPSR = 0;

uint8_t M68K_GetFPCR(uint32_t **ptr)
{
    if (reg_FPCR == 0xff)
    {
        reg_FPCR = RA_AllocARMRegister(ptr);
        **ptr = ldrh_offset(REG_CTX, reg_FPCR, __builtin_offsetof(struct M68KState, FPCR));
        (*ptr)++;
        mod_FPCR = 0;
    }

    return reg_FPCR;
}

uint8_t M68K_ModifyFPCR(uint32_t **ptr)
{
    uint8_t fpcr = M68K_GetFPCR(ptr);
    mod_FPCR = 1;
    return fpcr;
}

void M68K_StoreFPCR(uint32_t **ptr)
{
    if (reg_FPCR != 0xff && mod_FPCR)
    {
        **ptr = strh_offset(REG_CTX, reg_FPCR, __builtin_offsetof(struct M68KState, FPCR));
        (*ptr)++;
    }
}

void M68K_FlushFPCR(uint32_t **ptr)
{
    if (reg_FPCR != 0xff && mod_FPCR)
    {
        **ptr = strh_offset(REG_CTX, reg_FPCR, __builtin_offsetof(struct M68KState, FPCR));
        (*ptr)++;
        RA_FreeARMRegister(ptr, reg_FPCR);
    }
    reg_FPCR = 0xff;
    mod_FPCR = 0;
}

uint8_t M68K_GetFPSR(uint32_t **ptr)
{
    if (reg_FPSR == 0xff)
    {
        reg_FPSR = RA_AllocARMRegister(ptr);
        **ptr = ldr_offset(REG_CTX, reg_FPSR, __builtin_offsetof(struct M68KState, FPSR));
        (*ptr)++;
        mod_FPSR = 0;
    }

    return reg_FPSR;
}

uint8_t M68K_ModifyFPSR(uint32_t **ptr)
{
    uint8_t fpsr = M68K_GetFPSR(ptr);
    mod_FPSR = 1;
    return fpsr;
}

void M68K_StoreFPSR(uint32_t **ptr)
{
    if (reg_FPSR != 0xff && mod_FPSR)
    {
        **ptr = str_offset(REG_CTX, reg_FPSR, __builtin_offsetof(struct M68KState, FPSR));
        (*ptr)++;
    }
}

void M68K_FlushFPSR(uint32_t **ptr)
{
    if (reg_FPSR != 0xff && mod_FPSR)
    {
        **ptr = str_offset(REG_CTX, reg_FPSR, __builtin_offsetof(struct M68KState, FPSR));
        (*ptr)++;
        RA_FreeARMRegister(ptr, reg_FPSR);
    }
    reg_FPSR = 0xff;
    mod_FPSR = 0;
}

void M68K_GetCC(uint32_t **ptr)
{
    if (got_CC == 0)
    {
        **ptr = ldrh_offset(REG_CTX, REG_SR, __builtin_offsetof(struct M68KState, SR));
        (*ptr)++;
        got_CC = 1;
        mod_CC = 0;
    }
}

void M68K_ModifyCC(uint32_t **ptr)
{
    M68K_GetCC(ptr);
    mod_CC = 1;
}

void M68K_StoreCC(uint32_t **ptr)
{
    if (got_CC && mod_CC)
    {
        **ptr = strh_offset(REG_CTX, REG_SR, __builtin_offsetof(struct M68KState, SR));
        (*ptr)++;
    }
}

void M68K_FlushCC(uint32_t **ptr)
{
    if (got_CC && mod_CC)
    {
        **ptr = strh_offset(REG_CTX, REG_SR, __builtin_offsetof(struct M68KState, SR));
        (*ptr)++;
    }
    got_CC = 0;
}
#endif

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

    if (ReturnStackDepth > 0) {

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

/*
    Get M68K code unit from the instruction cache. Return NULL if code was not found and needs to be
    translated first.

    If the code was found, update its position in the LRU cache.
*/
struct M68KTranslationUnit *M68K_GetTranslationUnit(uint16_t *m68kcodeptr)
{
    struct M68KTranslationUnit *unit = NULL, *n;
    uintptr_t hash = (uintptr_t)m68kcodeptr;
    uint16_t *orig_m68kcodeptr = m68kcodeptr;
    uint16_t *m68k_low = m68kcodeptr;
    uint16_t *m68k_high = m68kcodeptr;

    /* Get 16-bit has from the pointer to m68k code */
    hash = (hash ^ (hash >> 16)) & 0xffff;

    if (debug > 1)
        kprintf("[ICache] GetTranslationUnit(%08x)\n[ICache] Hash: 0x%04x\n", (void*)m68kcodeptr, (int)hash);

    /* Find entry with correct address */
    ForeachNode(&ICache[hash], n)
    {
        if (n->mt_M68kAddress == m68kcodeptr)
        {
            /* Unit found? Move it to the front of LRU list */
            unit = n;

            struct Node *this = &unit->mt_LRUNode;

            if (1)
            {
                // Update LRU for least *frequently* used strategy
                if (this->ln_Pred->ln_Pred) {
                    struct Node *pred = this->ln_Pred;
                    struct Node *succ = this->ln_Succ;

                    this->ln_Pred = pred->ln_Pred;
                    this->ln_Succ = pred;
                    this->ln_Pred->ln_Succ = this;
                    pred->ln_Pred = this;
                    pred->ln_Succ = succ;
                    succ->ln_Pred = pred;
                }
            }
            else
            {
                // Update LRU for least *recently* used strategy
                REMOVE(&unit->mt_LRUNode);
                ADDHEAD(&LRU, &unit->mt_LRUNode);
            }
            return unit;
        }
    }

    if (unit == NULL)
    {
        uint32_t *pop_update_loc[EMU68_M68K_INSN_DEPTH];
        uint32_t pop_cnt=0;

        for (int i=0; i < EMU68_M68K_INSN_DEPTH; i++)
            pop_update_loc[i] = (uint32_t *)0;

        M68K_ResetReturnStack();
#ifndef __aarch64__
        got_CC = 0;
        mod_CC = 0;
#endif

        if (debug)
            kprintf("[ICache] Creating new translation unit with hash %04x (m68k code @ %p)\n", hash, (void*)m68kcodeptr);

        uint32_t prologue_size = 0;
        uint32_t epilogue_size = 0;
        uint32_t conditionals_count = 0;
        int lr_is_saved = 0;

        uint32_t insn_count = 0;
        uint32_t *arm_code = temporary_arm_code;
        uint32_t *end = arm_code;

        (void)prologue_size;
        (void)lr_is_saved;

        RA_ClearChangedMask();

        uint32_t *tmpptr = end;
        pop_update_loc[pop_cnt++] = end;

#ifndef __aarch64__
        *end++ = push(0); // Space for register saving, aarch32 only

  #if !(EMU68_HOST_BIG_ENDIAN) && EMU68_HAS_SETEND
        *end++ = setend_be();
  #endif

        *end++ = ldr_offset(REG_CTX, REG_PC, __builtin_offsetof(struct M68KState, PC));
#endif

        prologue_size = end - tmpptr;

        while (*m68kcodeptr != 0xffff && insn_count < Options.M68K_TRANSLATION_DEPTH)
        {
            if (m68kcodeptr < m68k_low)
                m68k_low = m68kcodeptr;
            if (m68kcodeptr + 16 > m68k_high)
                m68k_high = m68kcodeptr + 16;

            local_state[insn_count].mls_ARMOffset = end - arm_code;
            local_state[insn_count].mls_M68kPtr = m68kcodeptr;
            local_state[insn_count].mls_PCRel = _pc_rel;
#ifndef __aarch64__
            for (int r=0; r < 16; r++)
                local_state[insn_count].mls_RegMap[r] = RA_GetMappedARMRegister(r);
#endif
            end = EmitINSN(end, &m68kcodeptr);
            insn_count++;
            if (end[-1] == INSN_TO_LE(0xfffffff0))
            {
                lr_is_saved = 1;
                end--;
            }
            if (end[-1] == INSN_TO_LE(0xffffffff))
            {
                end--;
                break;
            }
            else if (end[-1] == INSN_TO_LE(0xfffffffe))
            {
                uint32_t *tmpptr;
                uint32_t *branch_mod[10];
                uint32_t branch_cnt;
                int local_branch_done = 0;
                end--;
                end--;  /* Remove branch target (unused!) */
                branch_cnt = *--end;

                for (unsigned i=0; i < branch_cnt; i++)
                {
                    branch_mod[i] = *(uint32_t **)--end;
                }

                tmpptr = end;

                conditionals_count++;

                if (!local_branch_done)
                {
                    RA_StoreDirtyFPURegs(&end);
                    RA_StoreDirtyM68kRegs(&end);
                    end = EMIT_FlushPC(end);
#ifndef __aarch64__
                    M68K_StoreCC(&end);
                    M68K_StoreFPCR(&end);
                    M68K_StoreFPSR(&end);

                    *end++ = str_offset(REG_CTX, REG_PC, __builtin_offsetof(struct M68KState, PC));
#if !(EMU68_HOST_BIG_ENDIAN) && EMU68_HAS_SETEND
                    *end++ = setend_le();
#endif

#else
                    RA_StoreCC(&end);
                    RA_StoreFPCR(&end);
                    RA_StoreFPSR(&end);
#endif
                    pop_update_loc[pop_cnt++] = end;
#ifndef __aarch64__
                    *end++ = pop((1 << REG_SR));// | (1 << REG_CTX));
                    if (!lr_is_saved)
                        *end++ = bx_lr();
#else
                    *end++ = bx_lr();
#endif
                }
                int distance = end - tmpptr;
//                printf("[ICache] Branch modification at %p : distance increase by %d\n", (void*) branch_mod, distance);
                for (unsigned i=0; i < branch_cnt; i++)
                    *(branch_mod[i]) = INSN_TO_LE((INSN_TO_LE(*(branch_mod[i])) + distance));
                epilogue_size += distance;
            }
        }
        tmpptr = end;
        RA_FlushFPURegs(&end);
        RA_FlushM68kRegs(&end);
        end = EMIT_FlushPC(end);
#ifndef __aarch64__
        M68K_FlushCC(&end);
        M68K_FlushFPCR(&end);
        M68K_FlushFPSR(&end);
        *end++ = str_offset(REG_CTX, REG_PC, __builtin_offsetof(struct M68KState, PC));
        uint16_t mask = RA_GetChangedMask() & 0xfff0;
        if (mod_CC)
            mask |= 1 << REG_SR;
#if !(EMU68_HOST_BIG_ENDIAN) && EMU68_HAS_SETEND
        *end++ = setend_le();
#endif
        if (!mask && !lr_is_saved)
#else
        RA_FlushCC(&end);
        RA_FlushFPCR(&end);
        RA_FlushFPSR(&end);
#endif
        {
#ifndef __aarch64__
            arm_code++;
#endif
            int i=1;

            while (pop_update_loc[i]) {
                *pop_update_loc[i++] = bx_lr();
            }
        }
#ifndef __aarch64__
        if (lr_is_saved)
        {
            *end++ = pop(mask /*| (1 << REG_SR)*/ /*| (1 << REG_CTX)*/ | (1 << 15));
        }
        else if (mask)
        {
            *end++ = pop(mask /*| (1 << REG_SR)*/ /*| (1 << REG_CTX)*/);
        }

        if (mask || lr_is_saved)
        {
            int i=1;
            if (lr_is_saved)
                *pop_update_loc[0] = push(mask /*| (1 << REG_SR)*/ /*| (1 << REG_CTX)*/ | (1 << 14));
            else
                *pop_update_loc[0] = push(mask /*| (1 << REG_SR)*/ /*| (1 << REG_CTX)*/);
            while (pop_update_loc[i]) {
                if (lr_is_saved)
                    *pop_update_loc[i++] = pop(mask /*| (1 << REG_SR)*/ /*| (1 << REG_CTX)*/ | (1 << 15));
                else
                    *pop_update_loc[i++] = pop(mask /*| (1 << REG_SR)*/ /*| (1 << REG_CTX)*/);
            }
        }
        if (!lr_is_saved)
#endif
            *end++ = bx_lr();
        epilogue_size += end - tmpptr;

        // Put a marker at the end of translation unit
        *end++ = 0xffffffff;

        if (debug)
        {
            kprintf("[ICache]   Translated %d M68k instructions to %d ARM instructions\n", insn_count, (int)(end - arm_code));
            kprintf("[ICache]   Prologue size: %d, Epilogue size: %d, Conditionals: %d\n",
                prologue_size, epilogue_size, conditionals_count);
            kprintf("[ICache]   Mean epilogue size pro exit point: %d\n", epilogue_size / (1 + conditionals_count));
            uint32_t mean = 100 * (end - arm_code - (prologue_size + epilogue_size));
            mean = mean / insn_count;
            uint32_t mean_n = mean / 100;
            uint32_t mean_f = mean % 100;
            kprintf("[ICache]   Mean ARM instructions per m68k instruction: %d.%02d\n", mean_n, mean_f);
        }

        uintptr_t line_length = (uintptr_t)end - (uintptr_t)arm_code;
        line_length = (line_length + 31 + sizeof(struct M68KTranslationUnit)) & ~31;

        do {
            unit = tlsf_malloc_aligned(jit_tlsf, line_length, 32);
            if (unit == NULL)
            {
                extern uint32_t last_PC;
                struct Node *n = REMTAIL(&LRU);
                void *ptr = (char *)n - __builtin_offsetof(struct M68KTranslationUnit, mt_LRUNode);
                REMOVE((struct Node *)ptr);
                kprintf("[ICache] Run out of cache. Removing least recently used cache line node @ %p\n", ptr);
                tlsf_free(jit_tlsf, ptr);
                last_PC = 0xffffffff;
            }
        } while(unit == NULL);

        unit->mt_ARMEntryPoint = &unit->mt_ARMCode[0];
#ifdef __aarch64__
        unit->mt_ARMEntryPoint = (void *)((uintptr_t)unit->mt_ARMEntryPoint | 0x0000001000000000);
#endif
        unit->mt_M68kInsnCnt = insn_count;
        unit->mt_ARMInsnCnt = (uint32_t)(end - arm_code - 1);
        unit->mt_UseCount = 0;
        unit->mt_M68kAddress = orig_m68kcodeptr;
        unit->mt_M68kLow = m68k_low;
        unit->mt_M68kHigh = m68k_high;
        unit->mt_PrologueSize = prologue_size;
        unit->mt_EpilogueSize = epilogue_size;
        unit->mt_Conditionals = conditionals_count;
        DuffCopy(&unit->mt_ARMCode[0], arm_code, end - arm_code);

        ADDHEAD(&LRU, &unit->mt_LRUNode);
        ADDHEAD(&ICache[hash], &unit->mt_HashNode);

        kprintf("[ICache]   ARM code at %p\n", unit->mt_ARMEntryPoint);
        __clear_cache(&unit->mt_ARMCode[0], &unit->mt_ARMCode[unit->mt_ARMInsnCnt]);
        arm_icache_invalidate((intptr_t)unit->mt_ARMEntryPoint, 4 * unit->mt_ARMInsnCnt);

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
            if (debug > 1)
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

    return unit;
}

void M68K_InitializeCache()
{
    kprintf("[ICache] Initializing caches\n");

#ifndef __aarch64__

    jit_tlsf = tlsf;

#endif

    kprintf("[ICache] Setting up LRU\n");
    NEWLIST(&LRU);

    kprintf("[ICache] Setting up ICache\n");
    ICache = tlsf_malloc(tlsf, sizeof(struct List) * 65536);
    temporary_arm_code = tlsf_malloc(jit_tlsf, EMU68_M68K_INSN_DEPTH * 16 * 64);
    kprintf("[ICache] Temporary code at %p\n", temporary_arm_code);
    local_state = tlsf_malloc(tlsf, sizeof(struct M68KLocalState)*EMU68_M68K_INSN_DEPTH*2);
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

    kprintf("[ICache] Listing translation units:\n");
    ForeachNode(&LRU, n)
    {
        cnt++;
        unit = (void *)((char *)n - __builtin_offsetof(struct M68KTranslationUnit, mt_LRUNode));
        kprintf("[ICache]   Unit %p, mt_UseCount=%lld, M68K address %p (range %p-%p)\n[ICache]      M68K insn count=%d, ARM insn count=%d\n", (void*)unit, unit->mt_UseCount,
            (void*)unit->mt_M68kAddress, (void*)unit->mt_M68kLow, (void*)unit->mt_M68kHigh, unit->mt_M68kInsnCnt, unit->mt_ARMInsnCnt);

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
