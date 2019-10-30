/*
    Copyright © 2019 Michal Schulz <michal.schulz@gmx.de>
    https://github.com/michalsc

    This Source Code Form is subject to the terms of the
    Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed
    with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#define _GNU_SOURCE 1
#include <sys/mman.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "M68k.h"
#include "ARM.h"
#include "RegisterAllocator.h"
#include "lists.h"
#include "tlsf.h"
#include "config.h"
#include "DuffCopy.h"

const int debug = 0;

#ifdef RASPI
#include "support_rpi.h"
static struct List *ICache;
#else
static struct List ICache[65536];
#endif
static struct List LRU;
#ifndef RASPI
static uint32_t *arm_cache;
static const uint32_t arm_cache_size = EMU68_ARM_CACHE_SIZE;
#endif
static const uint32_t m68k_translation_depth = EMU68_M68K_INSN_DEPTH;
void *handle;

static uint32_t temporary_arm_code[EMU68_M68K_INSN_DEPTH * 4 * 64];
static struct M68KLocalState local_state[EMU68_M68K_INSN_DEPTH];

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
if (debug)    printf("Emit_AdvancePC(pc_rel=%d, off=%d)\n", _pc_rel, (int)offset);
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

uint32_t *EmitINSN(uint32_t *arm_ptr, uint16_t **m68k_ptr)
{
    uint32_t *ptr = arm_ptr;
    uint16_t opcode = BE16((*m68k_ptr)[0]);
    uint8_t group = opcode >> 12;

    if (group == 0)
    {
        /* Bit manipulation/MOVEP/Immediate */
        ptr = EMIT_line0(arm_ptr, m68k_ptr);
    }
    else if ((group & 0xc) == 0 && (group & 3))
    {
        /* MOVE .B .W .L */
        ptr = EMIT_move(arm_ptr, m68k_ptr);
    }
    else if (group == 4)
    {
        /* Miscellaneous instructions */
        ptr = EMIT_line4(arm_ptr, m68k_ptr);
    }
    else if (group == 5)
    {
        /* ADDQ/SUBQ/Scc/DBcc/TRAPcc */
        ptr = EMIT_line5(arm_ptr, m68k_ptr);
    }
    else if (group == 6)
    {
        /* Bcc/BSR/BRA */
        ptr = EMIT_line6(arm_ptr, m68k_ptr);
    }
    else if (group == 7)
    {
        ptr = EMIT_moveq(arm_ptr, m68k_ptr);
    }
    else if (group == 8)
    {
        /* OR/DIV/SBCD */
        ptr = EMIT_line8(arm_ptr, m68k_ptr);
    }
    else if (group == 9)
    {
        /* SUB/SUBX */
        ptr = EMIT_line9(arm_ptr, m68k_ptr);
    }
    else if (group == 11)
    {
        /* CMP/EOR */
        ptr = EMIT_lineB(arm_ptr, m68k_ptr);
    }
    else if (group == 12)
    {
        /* AND/MUL/ABCD/EXG */
        ptr = EMIT_lineC(arm_ptr, m68k_ptr);
    }
    else if (group == 13)
    {
        /* ADD/ADDX */
        ptr = EMIT_lineD(arm_ptr, m68k_ptr);
    }
    else if (group == 14)
    {
        /* Shift/Rotate/Bitfield */
        ptr = EMIT_lineE(arm_ptr, m68k_ptr);
    }
    else if (group == 15)
        *ptr++ = udf(opcode);
    else
        (*m68k_ptr)++;

    return ptr;
}

#ifdef RASPI

void __clear_cache(void *begin, void *end)
{
    arm_flush_cache((uintptr_t)begin, (uintptr_t)end - (uintptr_t)begin);
    arm_icache_invalidate((uintptr_t)begin, (uintptr_t)end - (uintptr_t)begin);
}

#else
void __clear_cache(void *begin, void *end);
#endif

static uint8_t got_CC = 0;
static uint8_t mod_CC = 0;

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

void M68K_FlushCC(uint32_t **ptr)
{
    if (got_CC && mod_CC)
    {
        **ptr = strh_offset(REG_CTX, REG_SR, __builtin_offsetof(struct M68KState, SR));
        (*ptr)++;
    }
    got_CC = 0;
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

//    printf("[ICache] GetTranslationUnit(%08x)\n[ICache] Hash: 0x%04x\n", (void*)m68kcodeptr, (int)hash);

    /* Find entry with correct address */
    ForeachNode(&ICache[hash], n)
    {
        if (n->mt_M68kAddress == m68kcodeptr)
        {
            /* Unit found? Move it to the front of LRU list */
            unit = n;
            REMOVE(&unit->mt_LRUNode);
            ADDHEAD(&LRU, &unit->mt_LRUNode);
            return unit;
        }
    }

    if (unit == NULL)
    {
        uint32_t *pop_update_loc[EMU68_M68K_INSN_DEPTH];
        uint32_t pop_cnt=0;
        for (int i=0; i < EMU68_M68K_INSN_DEPTH; i++)
            pop_update_loc[i] = (uint32_t *)0;

        got_CC = 0;
        mod_CC = 0;

//        unit = tlsf_malloc(handle, m68k_translation_depth * 4 * 64);
if (debug)        printf("[ICache] Creating new translation unit with hash %04x (m68k code @ %08x)\n", hash, m68kcodeptr);
//        unit->mt_M68kAddress = m68kcodeptr;

        uint32_t prologue_size = 0;
        uint32_t epilogue_size = 0;
        uint32_t conditionals_count = 0;
        int lr_is_saved = 0;

        uint32_t insn_count = 0;
        uint32_t *arm_code = temporary_arm_code; //&unit->mt_ARMCode[0];
        //unit->mt_ARMEntryPoint = arm_code;
        uint32_t *end = arm_code;

        (void)prologue_size;
//        printf("[ICache] ARM code entry at %p\n", (void*)arm_code);

        RA_ClearChangedMask();

        uint32_t *tmpptr = end;
        pop_update_loc[pop_cnt++] = end;
        *end++ = push(0); //(1 << REG_SR));// | (1 << REG_CTX));
#if !(EMU68_HOST_BIG_ENDIAN) && EMU68_HAS_SETEND
        *end++ = setend_be();
#endif
        //*end++ = mov_reg(REG_CTX, 0);
        *end++ = ldr_offset(REG_CTX, REG_PC, __builtin_offsetof(struct M68KState, PC));
        prologue_size = end - tmpptr;
        while (*m68kcodeptr != 0xffff && insn_count < m68k_translation_depth)
        {
            if (m68kcodeptr < m68k_low)
                m68k_low = m68kcodeptr;
            if (m68kcodeptr + 16 > m68k_high)
                m68k_high = m68kcodeptr + 16;

            local_state[insn_count].mls_ARMOffset = end - arm_code;
            local_state[insn_count].mls_M68kPtr = m68kcodeptr;
            for (int r=0; r < 16; r++)
                local_state[insn_count].mls_RegMap[r] = RA_GetMappedARMRegister(r);
            end = EmitINSN(end, &m68kcodeptr);
            insn_count++;
            if (end[-1] == INSN_TO_LE(0xfffffff0))
            {
                lr_is_saved = 1;
                end--;
            }
            if (end[-1] == INSN_TO_LE(0xffffffff))
            {
//                printf("[ICache] Unconditional PC change. End of translation block after %d M68K instructions\n", insn_count);
                end--;
                break;
            }
            else if (end[-1] == INSN_TO_LE(0xfffffffe))
            {
                uint32_t *tmpptr;
                uint32_t *branch_mod[10];
                uint32_t branch_cnt;
//                printf("[ICache] Conditional PC change.\n");
                end--;
                branch_cnt = *--end;
//                printf("[ICache] Need to adjust %d branches\n", branch_cnt);
                for (unsigned i=0; i < branch_cnt; i++)
                {
                    branch_mod[i] = *(uint32_t **)--end;
//                    printf("[ICache] Loc %d: %p\n", i, (void*)branch_mod[i]);
                }

                tmpptr = end;
                conditionals_count++;

                RA_StoreDirtyM68kRegs(&end);
                end = EMIT_FlushPC(end);
                *end++ = strh_offset(REG_CTX, REG_SR, __builtin_offsetof(struct M68KState, SR));
                *end++ = str_offset(REG_CTX, REG_PC, __builtin_offsetof(struct M68KState, PC));
#if !(EMU68_HOST_BIG_ENDIAN) && EMU68_HAS_SETEND
                *end++ = setend_le();
#endif
                pop_update_loc[pop_cnt++] = end;
                *end++ = pop((1 << REG_SR));// | (1 << REG_CTX));
                if (!lr_is_saved)
                    *end++ = bx_lr();
                int distance = end - tmpptr;
//                printf("[ICache] Branch modification at %p : distance increase by %d\n", (void*) branch_mod, distance);
                for (unsigned i=0; i < branch_cnt; i++)
                    *(branch_mod[i]) = INSN_TO_LE((INSN_TO_LE(*(branch_mod[i])) + distance));
                epilogue_size += distance;
            }
        }
        tmpptr = end;
        RA_FlushM68kRegs(&end);
        end = EMIT_FlushPC(end);
        M68K_FlushCC(&end);
        *end++ = str_offset(REG_CTX, REG_PC, __builtin_offsetof(struct M68KState, PC));
        uint16_t mask = RA_GetChangedMask() & 0xfff0;
        if (mod_CC)
            mask |= 1 << REG_SR;
#if !(EMU68_HOST_BIG_ENDIAN) && EMU68_HAS_SETEND
        *end++ = setend_le();
#endif
        if (!mask && !lr_is_saved) {
            arm_code++;
            int i=1;

            while (pop_update_loc[i]) {
                *pop_update_loc[i++] = bx_lr();
            }
        }

        if (lr_is_saved)
            *end++ = pop(mask /*| (1 << REG_SR)*/ /*| (1 << REG_CTX)*/ | (1 << 15));
        else if (mask)
            *end++ = pop(mask /*| (1 << REG_SR)*/ /*| (1 << REG_CTX)*/);

        if (mask || lr_is_saved) {
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
            *end++ = bx_lr();
        epilogue_size += end - tmpptr;


    if (debug)      {

        printf("[ICache]   Translated %d M68k instructions to %d ARM instructions\n", insn_count, (int)(end - arm_code));
        printf("[ICache]   Prologue size: %d, Epilogue size: %d, Conditionals: %d\n",
            prologue_size, epilogue_size, conditionals_count);
        printf("[ICache]   Mean epilogue size pro exit point: %d\n", epilogue_size / (1 + conditionals_count));
            uint32_t mean = 100 * (end - arm_code - (prologue_size + epilogue_size));
            mean = mean / insn_count;
            uint32_t mean_n = mean / 100;
            uint32_t mean_f = mean % 100;
            printf("[ICache]   Mean ARM instructions per m68k instruction: %d.%02d\n", mean_n, mean_f);
        }

        uintptr_t line_length = (uintptr_t)end - (uintptr_t)arm_code;
        line_length = (line_length + 31 + sizeof(struct M68KTranslationUnit)) & ~31;

        do {
            unit = tlsf_malloc_aligned(handle, line_length, 32);
            if (unit == NULL)
            {
                struct Node *n = REMTAIL(&LRU);
                void *ptr = (char *)n - __builtin_offsetof(struct M68KTranslationUnit, mt_LRUNode);
                printf("[ICache] Run out of cache. Removing least recently used cache line node @ %p\n", ptr);
                tlsf_free(handle, ptr);
            }
        } while(unit == NULL);

        unit->mt_ARMEntryPoint = &unit->mt_ARMCode[0];
        unit->mt_M68kInsnCnt = insn_count;
        unit->mt_ARMInsnCnt = (uint32_t)(end - arm_code);
        unit->mt_UseCount = 0;
        unit->mt_M68kAddress = orig_m68kcodeptr;
        unit->mt_M68kLow = m68k_low;
        unit->mt_M68kHigh = m68k_high;
        unit->mt_PrologueSize = prologue_size;
        unit->mt_EpilogueSize = epilogue_size;
        unit->mt_Conditionals = conditionals_count;
        DuffCopy(unit->mt_ARMEntryPoint, arm_code, end - arm_code);

//        printf("[ICache] Trimming translation unit length to %d bytes\n", (int)line_length);

//        unit = tlsf_realloc(handle, unit, line_length);

//        printf("[ICache] Adding translation unit to LRU and Hashtable\n");
        ADDHEAD(&LRU, &unit->mt_LRUNode);
        ADDHEAD(&ICache[hash], &unit->mt_HashNode);

        __clear_cache(&unit->mt_ARMCode[0], &unit->mt_ARMCode[unit->mt_ARMInsnCnt]);

if (debug) {
        printf("-- ARM Code dump --\n");
        for (uint32_t i=0; i < unit->mt_ARMInsnCnt; i++)
        {
            uint32_t insn = unit->mt_ARMCode[i];
            printf("    %02x %02x %02x %02x\n", insn & 0xff, (insn >> 8) & 0xff, (insn >> 16) & 0xff, (insn >> 24) & 0xff);
        }

        printf("-- Local State --\n");
        for (unsigned i=0; i < insn_count; i++)
        {
            printf("    %p -> %08x", local_state[i].mls_M68kPtr, local_state[i].mls_ARMOffset);
            for (int r=0; r < 16; r++) {
                if (local_state[i].mls_RegMap[r] != 0xff) {
                    printf(" %c%d=r%d", r < 8 ? 'D' : 'A', r % 8, local_state[i].mls_RegMap[r]);
                }
            }
            printf("\n");
        }
        }
/*                exit(0);
*/
    }

    return unit;
}

void M68K_InitializeCache()
{
    printf("[ICache] Initializing caches\n");

#ifdef RASPI
    handle = tlsf;
#else
    handle = tlsf_init();
    arm_cache = (uint32_t *)mmap(NULL, arm_cache_size, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);

    printf("[ICache] ARM insn cache at %p\n", (void*)arm_cache);

    tlsf_add_memory(handle, arm_cache, arm_cache_size);
#endif

    printf("[ICache] Setting up LRU\n");
    NEWLIST(&LRU);

    printf("[ICache] Setting up ICache\n");
#ifdef RASPI
    ICache = tlsf_malloc(tlsf, sizeof(struct List) * 65536);
    printf("[ICache] ICache array at %08x\n", ICache);
#endif
    for (int i=0; i < 65536; i++)
        NEWLIST(&ICache[i]);
}

void M68K_DumpStats()
{
    struct M68KTranslationUnit *unit = NULL;
    struct Node *n;
    unsigned cnt = 0;
    unsigned size = 0;
    unsigned m68k_count = 0;
    unsigned arm_count = 0;
    unsigned total_arm_count = 0;

    printf("[ICache] Listing translation units:\n");
    ForeachNode(&LRU, n)
    {
        cnt++;
        unit = (void *)((char *)n - __builtin_offsetof(struct M68KTranslationUnit, mt_LRUNode));
        printf("[ICache]   Unit %p, mt_UseCount=%lld, M68K address %p (range %p-%p)\n[ICache]      M68K insn count=%d, ARM insn count=%d\n", (void*)unit, unit->mt_UseCount,
            (void*)unit->mt_M68kAddress, (void*)unit->mt_M68kLow, (void*)unit->mt_M68kHigh, unit->mt_M68kInsnCnt, unit->mt_ARMInsnCnt);

        size = size + (unsigned)(&unit->mt_ARMCode[unit->mt_ARMInsnCnt]) - (unsigned)unit;
        m68k_count += unit->mt_M68kInsnCnt;
        total_arm_count += unit->mt_ARMInsnCnt;
        arm_count += unit->mt_ARMInsnCnt - (unit->mt_PrologueSize + unit->mt_EpilogueSize);
    }
    printf("[ICache] In total %d units (%d bytes) in cache\n", cnt, size);

    uint32_t mean = 100 * (arm_count);
    mean = mean / m68k_count;
    uint32_t mean_n = mean / 100;
    uint32_t mean_f = mean % 100;
    printf("[ICache] Mean ARM instructions per m68k instruction: %d.%02d\n", mean_n, mean_f);

    mean = 100 * (total_arm_count);
    mean = mean / m68k_count;
    mean_n = mean / 100;
    mean_f = mean % 100;
    printf("[ICache] Mean total ARM instructions per m68k instruction: %d.%02d\n", mean_n, mean_f);

}
