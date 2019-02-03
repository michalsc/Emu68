#include <sys/mman.h>
#include <stdio.h>
#include <stdint.h>
#include "M68k.h"
#include "ARM.h"
#include "RegisterAllocator.h"
#include "lists.h"
#include "tlsf.h"
#include "config.h"

static struct List ICache[65536];
static struct List LRU;
static uint32_t *arm_cache;
static const uint32_t arm_cache_size = EMU68_ARM_CACHE_SIZE;
static const uint32_t m68k_translation_depth = EMU68_M68K_INSN_DEPTH;
void *handle;

uint32_t *EmitINSN(uint32_t *arm_ptr, uint16_t **m68k_ptr)
{
    uint32_t *ptr = arm_ptr;
    uint16_t opcode = BE16((*m68k_ptr)[0]);
    uint8_t group = opcode >> 12;

    if (group == 0)
    {
        ptr = EMIT_line0(arm_ptr, m68k_ptr);
    }
    else if ((group & 0xc) == 0 && (group & 3))
    {
        ptr = EMIT_move(arm_ptr, m68k_ptr);
    }
    else if (group == 4)
    {
        ptr = EMIT_line4(arm_ptr, m68k_ptr);
    }
    else if (group == 7)
    {
        ptr = EMIT_moveq(arm_ptr, m68k_ptr);
    }
    else
        (*m68k_ptr)++;

    /* No progress? Assume undefined instruction and emit udf to trigger exception */
    if (ptr == arm_ptr)
    {
        ptr = arm_ptr;
        *ptr++ = udf(opcode);
    }

    return ptr;
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

    /* Get 16-bit has from the pointer to m68k code */
    hash = (hash ^ (hash >> 16)) & 0xffff;

    printf("[ICache] GetTranslationUnit(%p)\n[ICache] Hash: 0x%04x\n", (void*)m68kcodeptr, (int)hash);

    /* Find entry with correct address */
    ForeachNode(&ICache[hash], n)
    {
        if (n->mt_M68kAddress == m68kcodeptr) {
            unit = n;
            break;
        }
    }

    /* Unit found? Move it to the front of LRU list */
    if (unit != NULL)
    {
        REMOVE(&(unit->mt_LRUNode));
        ADDHEAD(&LRU, &unit->mt_LRUNode);
    }
    else
    {
        unit = tlsf_malloc(handle, m68k_translation_depth * 4 * 64);
        printf("[ICache] Creating new translation unit at %p\n", (void*)unit);
        unit->mt_M68kAddress = m68kcodeptr;

        uint32_t insn_count = 0;
        uint32_t *arm_code = &unit->mt_ARMCode[0];
        unit->mt_ARMEntryPoint = arm_code;
        uint32_t *end = arm_code;

        printf("[ICache] ARM code entry at %p\n", (void*)arm_code);

        RA_ClearChangedMask();
        *end++ = push((1 << REG_SR) | (1 << REG_CTX));
#if !(EMU68_HOST_BIG_ENDIAN) && EMU68_HAS_SETEND
        *end++ = setend_be();
#endif
        *end++ = mov_reg(REG_CTX, 0);
        *end++ = ldr_offset(REG_CTX, REG_PC, __builtin_offsetof(struct M68KState, PC));
        *end++ = ldrh_offset(REG_CTX, REG_SR, __builtin_offsetof(struct M68KState, SR));
        while (*m68kcodeptr != 0xffff && insn_count++ < m68k_translation_depth)
        {
            end = EmitINSN(end, &m68kcodeptr);
        }
        RA_FlushM68kRegs(&end);
        *end++ = strh_offset(REG_CTX, REG_SR, __builtin_offsetof(struct M68KState, SR));
        *end++ = str_offset(REG_CTX, REG_PC, __builtin_offsetof(struct M68KState, PC));
        uint16_t mask = RA_GetChangedMask() & 0xfff0;
#if !(EMU68_HOST_BIG_ENDIAN) && EMU68_HAS_SETEND
        *end++ = setend_le();
#endif
        *end++ = pop(mask | (1 << REG_SR) | (1 << REG_CTX));
        if (mask)
            arm_code[0] = push(mask | (1 << REG_SR) | (1 << REG_CTX));
        *end++ = bx_lr();

        printf("[ICache] Translated %d M68k instructions to %d ARM instructions\n", insn_count, (int)(end - arm_code));

        unit->mt_M68kInsnCnt = insn_count;
        unit->mt_ARMInsnCnt = (uint32_t)(end - arm_code);

        uintptr_t line_length = (uintptr_t)end - (uintptr_t)unit;
        line_length = (line_length + 31) & ~31;

        printf("[ICache] Trimming translation unit length to %d bytes\n", (int)line_length);

        unit = tlsf_realloc(handle, unit, line_length);

        printf("[ICache] Adding translation unit to LRU and Hashtable\n");
        ADDHEAD(&LRU, &unit->mt_LRUNode);
        ADDHEAD(&ICache[hash], &unit->mt_HashNode);
    }

    return unit;
}

void M68K_InitializeCache()
{
    handle = tlsf_init();
    printf("[ICache] Initializing caches\n");

    arm_cache = (uint32_t *)mmap(NULL, arm_cache_size, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);

    printf("[ICache] ARM insn cache at %p\n", (void*)arm_cache);

    tlsf_add_memory(handle, arm_cache, arm_cache_size);

    printf("[ICache] Setting up LRU\n");
    NEWLIST(&LRU);

    printf("[ICache] Setting up ICache\n");
    for (int i=0; i < 65536; i++)
        NEWLIST(&ICache[i]);
}
