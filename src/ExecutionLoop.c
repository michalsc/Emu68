/*
    Copyright © 2019-2025 Michal Schulz <michal.schulz@gmx.de>
    https://github.com/michalsc

    This Source Code Form is subject to the terms of the
    Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed
    with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <M68k.h>
#include <support.h>
#include <config.h>
#include <arm_neon.h>

#include "disasm.h"
#include "RegisterMapping.h"

#ifdef PISTORM_CLASSIC
#define PS_PROTOCOL_IMPL
#include "pistorm/ps_protocol.h"
#endif

register uint64_t (*ARMCode)() __asm__("x12");
extern uint32_t EPOCH;

#if 0
static inline uint32_t getLastPC()
{
    uint32_t lastPC;
    __asm__ volatile("mov %w0, "CTX_LAST_PC_ASM"":"=r"(lastPC));
    return lastPC;
}
#endif

static inline struct M68KState *getCTX()
{
    struct M68KState *ctx;
    __asm__ volatile("mov %0, "CTX_POINTER_ASM:"=r"(ctx));
    return ctx;
}

static inline void setCTX(struct M68KState *ctx)
{
    __asm__ volatile("mov "CTX_POINTER_ASM",%0"::"r"(ctx));
}

static inline void setLastPC(uint32_t pc)
{
    __asm__ volatile("mov "CTX_LAST_PC_ASM", %w0": :"r"(pc));
}

extern struct List ICache[EMU68_HASHSIZE];
//void M68K_LoadContext(struct M68KState *ctx);
//void M68K_SaveContext(struct M68KState *ctx);
void M68K_PrintContext(struct M68KState *ctx);

void M68K_LoadContext(struct M68KState *ctx)
{
    setCTX(ctx);

    uint32x4_t tmp = vdupq_n_u32(0);
    tmp = vsetq_lane_u32(ctx->CACR, tmp, 0);
    tmp = vsetq_lane_u32(ctx->USP.u32, tmp, 1);
    tmp = vsetq_lane_u32(ctx->ISP.u32, tmp, 2);
    tmp = vsetq_lane_u32(ctx->MSP.u32, tmp, 3);
    reserved_reg_q21 = vreinterpretq_u64_u32(tmp);

    reserved_reg_q20 = vsetq_lane_u64(ctx->INSN_COUNT, reserved_reg_q20, 0);

    reserved_reg_q19 = vreinterpretq_u64_u32(vsetq_lane_u32(ctx->FPSR, vreinterpretq_u32_u64(reserved_reg_q19), 0));
    reserved_reg_q19 = vreinterpretq_u64_u32(vsetq_lane_u32(ctx->FPIAR, vreinterpretq_u32_u64(reserved_reg_q19), 1));
    reserved_reg_q19 = vreinterpretq_u64_u16(vsetq_lane_u16(ctx->FPCR, vreinterpretq_u16_u64(reserved_reg_q19), 4));
    reserved_reg_q19 = vreinterpretq_u64_u16(vsetq_lane_u16(swapVC(ctx->SR), vreinterpretq_u16_u64(reserved_reg_q19), 5));

    PC = ctx->PC;

    D0 = ctx->D[0].u32; D1 = ctx->D[1].u32;
    D2 = ctx->D[2].u32; D3 = ctx->D[3].u32;
    D4 = ctx->D[4].u32; D5 = ctx->D[5].u32;
    D6 = ctx->D[6].u32; D7 = ctx->D[7].u32;

    A0 = ctx->A[0].u32; A1 = ctx->A[1].u32;
    A2 = ctx->A[2].u32; A3 = ctx->A[3].u32;
    A4 = ctx->A[4].u32; A5 = ctx->A[5].u32;
    A6 = ctx->A[6].u32; A7 = ctx->A[7].u32;

    FP0 = ctx->FP[0].d; FP1 = ctx->FP[1].d;
    FP2 = ctx->FP[2].d; FP3 = ctx->FP[3].d;
    FP4 = ctx->FP[4].d; FP5 = ctx->FP[5].d;
    FP6 = ctx->FP[6].d; FP7 = ctx->FP[7].d;

    if (ctx->SR & SR_S) {
        if (ctx->SR & SR_M) {
            A7 = ctx->MSP.u32;
        } else {
            A7 = ctx->ISP.u32;
        }
    } else {
        A7 = ctx->USP.u32;
    }
}

void M68K_SaveContext(struct M68KState *ctx)
{
    ctx->CACR = vgetq_lane_u32(vreinterpretq_u32_u64(reserved_reg_q21), 0);
    ctx->INSN_COUNT = vgetq_lane_u64(reserved_reg_q20, 0);
    ctx->FPSR = vgetq_lane_u32(vreinterpretq_u32_u64(reserved_reg_q19), 0);
    ctx->FPIAR = vgetq_lane_u32(vreinterpretq_u32_u64(reserved_reg_q19), 1);
    ctx->FPCR = vgetq_lane_u16(vreinterpretq_u16_u64(reserved_reg_q19), 4);
    ctx->SR = swapVC(vgetq_lane_u16(vreinterpretq_u16_u64(reserved_reg_q19), 5));
    
    ctx->PC = PC;

    ctx->D[0].u32 = D0; ctx->D[1].u32 = D1;
    ctx->D[2].u32 = D2; ctx->D[3].u32 = D3;
    ctx->D[4].u32 = D4; ctx->D[5].u32 = D5;
    ctx->D[6].u32 = D6; ctx->D[7].u32 = D7;

    ctx->A[0].u32 = A0; ctx->A[1].u32 = A1;
    ctx->A[2].u32 = A2; ctx->A[3].u32 = A3;
    ctx->A[4].u32 = A4; ctx->A[5].u32 = A5;
    ctx->A[6].u32 = A6; ctx->A[7].u32 = A7;

    ctx->FP[0].d = FP0; ctx->FP[1].d = FP1;
    ctx->FP[2].d = FP2; ctx->FP[3].d = FP3;
    ctx->FP[4].d = FP4; ctx->FP[5].d = FP5;
    ctx->FP[6].d = FP6; ctx->FP[7].d = FP7;

    if (ctx->SR & SR_S) {
        if (ctx->SR & SR_M) {
            ctx->MSP.u32 = A7;
            ctx->ISP.u32 = vgetq_lane_u32(vreinterpretq_u32_u64(reserved_reg_q21), 2);
        } else {
            ctx->ISP.u32 = A7;
            ctx->MSP.u32 = vgetq_lane_u32(vreinterpretq_u32_u64(reserved_reg_q21), 3);
        }
        ctx->USP.u32 = vgetq_lane_u32(vreinterpretq_u32_u64(reserved_reg_q21), 1);
    } else {
        ctx->USP.u32 = A7;
        ctx->ISP.u32 = vgetq_lane_u32(vreinterpretq_u32_u64(reserved_reg_q21), 2);
        ctx->MSP.u32 = vgetq_lane_u32(vreinterpretq_u32_u64(reserved_reg_q21), 3);
    }
} 

struct Entry {
    uintptr_t m68k;
    uint32_t *arm;
};

struct LRU {
    uint64_t        alloc[EMU68_LRU_SET_COUNT];
    struct Entry    cache[EMU68_LRU_WAY_COUNT * EMU68_LRU_SET_COUNT];
} __attribute__((aligned(64)));

static struct LRU LRU;

static_assert((EMU68_LRU_SET_COUNT * sizeof(uint64_t)) % 64 == 0,
              "cache[] must land on its own cache line");
static_assert((EMU68_LRU_WAY_COUNT * sizeof(struct Entry)) % 64 == 0,
              "each set must fit in whole cache lines");

#define ADDR_2_SET(addr) (((addr) >> 2) % EMU68_LRU_SET_COUNT)
#define ALLOC_MASK (((1ULL << EMU68_LRU_WAY_COUNT) - 1) << (sizeof(LRU.alloc[0]) * 8 - EMU68_LRU_WAY_COUNT))
#define ALLOC_TOP_BIT (1ULL << (sizeof(LRU.alloc[0]) * 8 - 1))

static uint32_t* LRU_FindBlock(struct LRU *lru, uint32_t address)
{
    const uint32_t set = ADDR_2_SET(address);
    struct Entry *e = &lru->cache[set * EMU68_LRU_WAY_COUNT];
    uint64_t mask = ALLOC_TOP_BIT;
    
    for (int i=0; i < EMU68_LRU_WAY_COUNT; i++, mask >>= 1)
    {
        if (likely(e[i].m68k == address))
        {
            /* Tell CPU we are going to execute the code soon, give it time to prefetch eventually */
            asm volatile ("prfm plil1keep, [%0]"::"r"(e[i].arm));

            uint64_t current = lru->alloc[set] & ~mask; 
            if ((current & ALLOC_MASK) == 0) current = ~mask;
            lru->alloc[set] = current;
            
            return e[i].arm;
        }
    }

    return NULL;
}

void LRU_InvalidateByARMAddress(uint32_t *addr)
{
    for (int i = 0; i < EMU68_LRU_SET_COUNT * EMU68_LRU_WAY_COUNT; i++)
    {
        if (LRU.cache[i].arm == addr)
        {
            const uint32_t set = i / EMU68_LRU_WAY_COUNT;
            const uint32_t way = i % EMU68_LRU_WAY_COUNT;

            LRU.cache[i].arm = (void*)-1;
            LRU.cache[i].m68k = -1;
            
            LRU.alloc[set] |= (ALLOC_TOP_BIT >> way);
//            break;
        }
    }
}

void LRU_InvalidateByM68kAddress(uint32_t addr)
{
    const uint32_t set = ADDR_2_SET(addr);
    struct Entry *e = &LRU.cache[set * EMU68_LRU_WAY_COUNT];

    for (int i = 0; i < EMU68_LRU_WAY_COUNT; i++)
    {
        if (e[i].m68k == addr)
        {
            e[i].arm= (void*)-1;
            e[i].m68k = -1;
            LRU.alloc[set] |= (ALLOC_TOP_BIT >> i);
//            break;
        }
    }
}

void LRU_InvalidateAll()
{
    for (int i = 0; i < EMU68_LRU_SET_COUNT * EMU68_LRU_WAY_COUNT; i++)
    {
        LRU.cache[i].m68k = -1;
        LRU.cache[i].arm = (void*)-1;
    }

    for (int i = 0; i < EMU68_LRU_SET_COUNT; i++)
    {
        LRU.alloc[i] = -1;
    }
}

void LRU_InsertBlock(struct LRU *lru, struct M68KTranslationUnit *unit)
{
    const uint32_t set = ADDR_2_SET(unit->mt_M68kAddress);
    struct Entry *e = &lru->cache[set * EMU68_LRU_WAY_COUNT];
    int loc = __builtin_clzl(lru->alloc[set]);
    uint64_t mask = ALLOC_TOP_BIT >> loc;

    // Insert new entry
    e[loc].m68k = unit->mt_M68kAddress;
    e[loc].arm = unit->mt_ARMEntryPoint;

    // Touch the last used
    uint64_t current = lru->alloc[set] & ~mask; 
    if ((current & ALLOC_MASK) == 0) current = ~mask;
    lru->alloc[set] = current;
}

static uint32_t* FindUnitQuick(struct LRU *lru, struct List *icache, struct M68KState *ctx)
{
    struct M68KTranslationUnit *candidateUnit = NULL;

#if EMU68_USE_LRU
    uint32_t *code = LRU_FindBlock(lru, PC);

    if (likely(code != NULL)) {
        return code;
    }
#endif

    union {
        struct Node * node;
        struct M68KTranslationUnit * unit;
    } un;

    union {
        struct {
            uint32_t    mt_Epoch;           /* 16: 2 x 4 bytes - first 32-bit epoch incremented after every cache flush */
            uint32_t    mt_M68kAddress;     /*                   followed by 32-bit m68k entry address */
        };
        uint64_t        mt_Key;             /*     1 x 8 bytes - match key, the two above combined */
    } u;

    u.mt_Epoch = EPOCH;
    u.mt_M68kAddress = PC;

    uint64_t key = u.mt_Key;

    /* Perform search */
    uint32_t hash = (PC >> EMU68_HASHSHIFT) & EMU68_HASHMASK;
    struct List *bucket = &icache[hash];

    /* Go through the list of translated units */
    ForeachNode(bucket, un.node)
    {
        /* Check if unit is found */
        if (un.unit->mt_Key == key)
        {
            /* Tell CPU we are going to execute the code soon, give it time to prefetch eventually */
            asm volatile ("prfm plil1keep, [%0]"::"r"(un.unit->mt_ARMEntryPoint));

            /* Node was found and it was not the first one in the bucket, move it to the top now */
            if (bucket->lh_Head.ln_Next != un.node) {
                REMOVE(un.node);
                ADDHEAD(bucket, un.node);
            }

#if EMU68_USE_LRU
            LRU_InsertBlock(lru, un.unit);
#endif
            return (void*)un.unit->mt_ARMEntryPoint;
        } else if (un.unit->mt_M68kAddress == u.mt_M68kAddress) {
            /* 
                The code from current epoch was not found, but there is a code with right entry point 
                from previous epoch. Remember it for later.
            */
            candidateUnit = un.unit;
        }
    }

    /* No suitable translation was found, but there is a candidate with wrong EPOCH. Verify it now. */
    M68K_SaveContext(ctx);

    if (candidateUnit != NULL) {
        candidateUnit = M68K_VerifyUnit(candidateUnit);

        /* 
            If the unit is still there after verification, it was valid and has now updated EPOCH.
            Insert it into LRU and return ARM entry point.
        */
        if (candidateUnit != NULL) {
            /* Prefetch first, during prefetch we will insert the entry into LRU */
            asm volatile ("prfm plil1keep, [%0]"::"r"(candidateUnit->mt_ARMEntryPoint));
            LRU_InsertBlock(lru, candidateUnit);
            M68K_LoadContext(ctx);
            return candidateUnit->mt_ARMEntryPoint;
        }
    }

    /* Nothing found. Save context and fall back to new translation then. */
    return NULL;
}
#if 0
static inline struct M68KTranslationUnit *FindUnit()
{
    union {
        struct Node * node;
        struct M68KTranslationUnit * unit;
    } un;

    /* Perform search */
    uint32_t hash = (PC >> EMU68_HASHSHIFT) & EMU68_HASHMASK;
    struct List *bucket = &ICache[hash];

    /* Go through the list of translated units */
    ForeachNode(bucket, un.node)
    {
        /* Check if unit is found */
        if (un.unit->mt_M68kAddress == PC)
        {
#if EMU68_USE_LRU
            LRU_InsertBlock(un.unit);
#endif
            return un.unit;
        }
    }

    return NULL;
}
#endif
static inline struct M68KTranslationUnit *FindUnitNoLRU()
{
    union {
        struct Node * node;
        struct M68KTranslationUnit * unit;
    } un;

    /* Perform search */
    uint32_t hash = (PC >> EMU68_HASHSHIFT) & EMU68_HASHMASK;
    struct List *bucket = &ICache[hash];

    /* Go through the list of translated units */
    ForeachNode(bucket, un.node)
    {
        /* Check if unit is found */
        if (un.unit->mt_M68kAddress == PC)
        {
            return un.unit;
        }
    }

    return NULL;
}

#ifdef PISTORM_CLASSIC

extern volatile unsigned char bus_lock;

static inline int GetIPLLevel()
{
    volatile uint32_t *gpio = (void *)0xf2200000;

    *(gpio + 7) = LE32(REG_STATUS << PIN_A0);
    *(gpio + 7) = LE32(1 << PIN_RD);
    *(gpio + 7) = LE32(1 << PIN_RD);
    *(gpio + 7) = LE32(1 << PIN_RD);
    *(gpio + 7) = LE32(1 << PIN_RD);

    unsigned int value = LE32(*(gpio + 13));

    *(gpio + 10) = LE32(0xffffec);

    return (value >> 21) & 7;
}

#else
static inline int GetIPLLevel() { return 0; }
#endif

void ProcessIRQ(struct M68KState *ctx)
{
    uint32_t SR, SRcopy;
    int level = 0;
    uint32_t vector;
    uint32_t vbr;

    /* Find out requested IPL level based on ARM state and real IPL line */
    if (ctx->INTF.ARM_err)
    {
        level = 7;
        ctx->INTF.ARM_err = 0;
    }
    else
    {
        /* Assert one of external interrupts if ARM or PPC flag was active */
        if (ctx->INTF.ARM)
        {
            level = 6;
        }
        else if (ctx->INTF.PPC)
        {
            level = 2;
        }

        /* Now, if Higher-level interrupt was coming from Paula, report it */
#if defined(PISTORM)
        /* On PiStorm32 IPL level is obtained by second CPU core from the GPIO directly */
        if (ctx->INTF.IPL > level)
        {
            level = ctx->INTF.IPL;
        }
#else
        /* On classic pistorm we need to obtain IPL from PiStorm status register */
        if (ctx->INTF.IPL)
        {
            int ipl_level;

#if PISTORM_WRITE_BUFFER
            while(__atomic_test_and_set(&bus_lock, __ATOMIC_ACQUIRE)) { __asm__ volatile("yield"); }
#endif

            ipl_level = GetIPLLevel();

#if PISTORM_WRITE_BUFFER
            __atomic_clear(&bus_lock, __ATOMIC_RELEASE);
#endif
            /* Obtained IPL level higher than until now detected? */
            if (ipl_level > level)
            {
                level = ipl_level;
            }
        }           
#endif
    }

    /* Get SR and test the IPL mask value */
    SR = getSR();

    int IPL_mask = (SR & SR_IPL) >> SRB_IPL;

    /* Any unmasked interrupts? Proceess them */
    if (level == 7 || level > IPL_mask)
    {
        if (likely((SR & SR_S) == 0))
        {
            /* If we are not yet in supervisor mode, the USP needs to be updated */
            __asm__ volatile("mov "REG_USP_ASM", %w0": :"r"(A7));

            /* Load eiter ISP or MSP */
            if (unlikely((SR & SR_M) != 0))
            {
                __asm__ volatile("mov %w0, "REG_MSP_ASM:"=r"(A7));
            }
            else
            {
                __asm__ volatile("mov %w0, "REG_ISP_ASM:"=r"(A7));
            }
        }
        
        SRcopy = SR;
        /* Swap C and V flags in the copy */
        if ((SRcopy & 3) != 0 && (SRcopy & 3) != 3)
        SRcopy ^= 3;
        vector = 0x60 + (level << 2);

        /* Set supervisor mode */
        SR |= SR_S;

        /* Clear Trace mode */
        SR &= ~(SR_T0 | SR_T1);

        /* Insert current level into SR */
        SR &= ~SR_IPL;
        SR |= ((level & 7) << SRB_IPL);

        /* Push exception frame */
        __asm__ volatile("strh %w1, [%0, #-8]!":"+r"(A7):"r"(SRcopy));
        __asm__ volatile("str %w1, [%0, #2]": :"r"(A7),"r"(PC));
        __asm__ volatile("strh %w1, [%0, #6]": :"r"(A7),"r"(vector));

        /* Set SR */
        setSR(SR);

        /* Get VBR */
        vbr = ctx->VBR;

        /* Load PC */
        __asm__ volatile("ldr %w0, [%1, %2]":"=r"(PC):"r"(vbr),"r"(vector)); 
    }
}

void InterpreterLoop()
{
    struct M68KState *ctx = getCTX();

    /* Prepare vector 0; 1 which gets added to the INSN_COUNTER after every executed instruction */
    asm volatile("mov v22.d[1], xzr\n\tmov v22.d[0], %0"::"r"(1));

    /* ARMCode pointer misused - it will hold interpreter jump table instead */
    extern INTERPRET_Function __interpreter_jumptable_start __attribute__((aligned(4096)));
    ARMCode = (uint64_t (*)())&__interpreter_jumptable_start;

    /* 
        Endless lope will break if and only if ARMCode pointer (register w12) 
        will be reset to NULL. This can happen in interpreter e.g. as a result 
        of writing to CACR.
    */
    do
    {
#ifndef PISTORM_ANY_MODEL
        if (unlikely(PC == 0)) {
            return;
        }
#endif
        /* 
            Small "improved" version of code below, saves register shuffling 
            done by gcc which does not handle the global register variables well
        */
        uint32_t opcode;
        void (*code)(uint32_t);
        asm volatile(
            "ldrh	%w0, [%2]\n\t"
            "ldr	%1, [%3, %w0, uxtw #3]\n\t"
            :"=r"(opcode), "=r"(code), "+r"(PC), "+r"(ARMCode)
        );
        code(opcode);

        /* Increase instruction counter */
        reserved_reg_q20 = vaddq_u64(reserved_reg_q20, reserved_reg_q22);

        /* Check if any interrupts are pending, modify PC and stack if necessary */
        if (unlikely(ctx->INT64 != 0))
        {
            ProcessIRQ(ctx);
        }
    } while(ARMCode != NULL);
}

void JITLoop()
{
    struct M68KState *ctx = getCTX();
    struct List *cache = ICache;
    struct LRU* lru = &LRU;

    while(1)
    {
#ifndef PISTORM_ANY_MODEL
        if (unlikely(PC == 0)) {
            return;
        }
#endif
        /*
            Find unit in the LRU, if faild in the hashtable and if that faild
            in the "dumpster" along old code portions from previous "cache flush EPOCH"
            based on the PC value
        */
        ARMCode = (void*)FindUnitQuick(lru, cache, ctx);

        /* Unit does not exists? It was neither found nor recycled. Translate! */
        if (unlikely(ARMCode == NULL))
        {
            struct M68KTranslationUnit *unit;
            uint32_t copyPC = ctx->PC;

            /* Get the code. This never fails */
            unit = M68K_GetTranslationUnit((void*)(uintptr_t)copyPC);

#if EMU68_USE_LRU
            LRU_InsertBlock(lru, unit);
#endif
            /* Load CPU context - it was saved in FindUnitQuick */
            M68K_LoadContext(ctx);

            /* Prepare ARM pointer in x12 */
            ARMCode = unit->mt_ARMEntryPoint;
        }

        /* Call the code */
        ARMCode();

        /* 
            If the JIT block cleared ARMCode (x12 register) e.g. as a result of
            writing to CACR, break the JIT loop and let the main loop decide which one
            to run again.
        */
        if (unlikely(ARMCode == 0)) return;

        /* Check if any interrupts are pending, modify PC and stack if necessary */
        if (unlikely(ctx->INT64 != 0))
        {
            ProcessIRQ(ctx);
        }
    }
}

void MainLoop()
{
    struct M68KState *ctx = getCTX();

    LRU_InvalidateAll();

    M68K_LoadContext(ctx);

    /* The JIT loop is running forever */
    while(1)
    {
#ifndef PISTORM_ANY_MODEL
        if (unlikely(PC == 0)) {
            M68K_SaveContext(ctx);
            return;
        }
#endif

        /* Check if JIT cache is enabled */
        if (likely(getCACR() & CACR_IE))
        {
            JITLoop();
        }
        else
        {
            InterpreterLoop();
        }
    }
}

void M68kReportInterrupt(int irq)
{
    struct M68KState * ctx = getCTX();
    
    /* TODO - add more types (we have 8 slots in total) here */
    if (irq == 1) ctx->INTF.ARM = 1;
    else if (irq == 2) ctx->INTF.ARM_err = 1;
}
