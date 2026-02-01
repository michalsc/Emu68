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
#ifdef PISTORM_CLASSIC
#define PS_PROTOCOL_IMPL
#include "pistorm/ps_protocol.h"
#endif

register uint32_t PC __asm__("w18");
register void (*ARMCode)() __asm__("x12");
extern uint32_t EPOCH;

static inline uint32_t getLastPC()
{
    uint32_t lastPC;
    __asm__ volatile("mov %w0, "CTX_LAST_PC_ASM"":"=r"(lastPC));
    return lastPC;
}

static inline struct M68KState *getCTX()
{
    struct M68KState *ctx;
    __asm__ volatile("mov %0, "CTX_POINTER_ASM:"=r"(ctx));
    return ctx;
}

static inline uint32_t getSR()
{
    uint32_t sr;
    __asm__ volatile("umov %w0, "REG_SR_ASM:"=r"(sr));
    return sr;
}

static inline void setLastPC(uint32_t pc)
{
    __asm__ volatile("mov "CTX_LAST_PC_ASM", %w0": :"r"(pc));
}

static inline void setSR(uint32_t sr)
{
    __asm__ volatile("mov "REG_SR_ASM", %w0": :"r"(sr));
}

extern struct List ICache[EMU68_HASHSIZE];
void M68K_LoadContext(struct M68KState *ctx);
void M68K_SaveContext(struct M68KState *ctx);

struct Entry {
    uintptr_t m68k;
    uint32_t *arm;
};

struct Entry    LRU_cache[EMU68_LRU_WAY_COUNT * EMU68_LRU_SET_COUNT] __attribute__((aligned(64)));
uint32_t        LRU_alloc[EMU68_LRU_SET_COUNT];

#define ADDR_2_SET(addr) (((addr) >> 4) % EMU68_LRU_SET_COUNT)
#define BIT_MASK (((1ULL << EMU68_LRU_WAY_COUNT) - 1) << (32 - EMU68_LRU_WAY_COUNT))

uint32_t *LRU_FindBlock(uint32_t address)
{
    const uint32_t set = ADDR_2_SET(address);
    struct Entry *e = &LRU_cache[set * EMU68_LRU_WAY_COUNT];
    uint32_t mask = 0x80000000;
    
    for (int i=0; i < EMU68_LRU_WAY_COUNT; i++, mask >>= 1)
    {
        if (likely(e[i].m68k == address))
        {
            /* Tell CPU we are going to execute the code soon, give it time to prefetch eventually */
            asm volatile ("prfm plil1keep, [%0]"::"r"(e[i].arm));

            uint32_t current = LRU_alloc[set] & ~mask; 
            if (current >> (32 - EMU68_LRU_WAY_COUNT) == 0) current = ~mask;
            LRU_alloc[set] = current;
            
            return e[i].arm;
        }
    }

    return NULL;
}

void LRU_MarkForVerify(uint32_t *addr)
{
    for (int i = 0; i < EMU68_LRU_SET_COUNT * EMU68_LRU_WAY_COUNT; i++)
    {
        if (LRU_cache[i].arm == addr)
        {
            uintptr_t e = (uintptr_t)addr;
            e &= 0x00ffffffffffffffULL;
            e |= 0xaa00000000000000ULL;
            LRU_cache[i].arm = (uint32_t *)e;
            break;
        }
    }
}

void LRU_InvalidateByARMAddress(uint32_t *addr)
{
    for (int i = 0; i < EMU68_LRU_SET_COUNT * EMU68_LRU_WAY_COUNT; i++)
    {
        if (LRU_cache[i].arm == addr)
        {
            LRU_cache[i].arm = (void*)0;
            LRU_cache[i].m68k = 0xffffffff;
            break;
        }
    }
}

void LRU_InvalidateByM68kAddress(uint32_t addr)
{
    const uint32_t set = ADDR_2_SET(addr);
    struct Entry *e = &LRU_cache[set * EMU68_LRU_WAY_COUNT];

    for (int i = 0; i < EMU68_LRU_WAY_COUNT; i++)
    {
        if (e[i].m68k == addr)
        {
            e[i].arm= (void*)0;
            e[i].m68k = 0xffffffff;
            LRU_alloc[set] |= (0x80000000 >> i);
            break;
        }
    }
}

void LRU_InvalidateAll()
{
    for (int i = 0; i < EMU68_LRU_SET_COUNT * EMU68_LRU_WAY_COUNT; i++)
    {
        LRU_cache[i].m68k = 0xffffffff;
        LRU_cache[i].arm = (void*)0;
    }

    for (int i = 0; i < EMU68_LRU_SET_COUNT; i++)
    {
        LRU_alloc[i] = 0xffffffff;
    }
}

void LRU_InsertBlock(struct M68KTranslationUnit *unit)
{
    const uint32_t set = ADDR_2_SET(unit->mt_M68kAddress);
    struct Entry *e = &LRU_cache[set * EMU68_LRU_WAY_COUNT];
    int loc = __builtin_clz(LRU_alloc[set]);
    uint32_t mask = 0x80000000 >> loc;

    // Insert new entry
    e[loc].m68k = unit->mt_M68kAddress;
    e[loc].arm = unit->mt_ARMEntryPoint;

    // Touch the last used
    uint32_t current = LRU_alloc[set] & ~mask; 
    if (current >> (32 - EMU68_LRU_WAY_COUNT) == 0) current = ~mask;
    LRU_alloc[set] = current;
}

static inline uint32_t * FindUnitQuick()
{
#if EMU68_USE_LRU
    uint32_t *code = LRU_FindBlock(PC);

    if (likely(code != NULL))
        return code;
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
    struct List *bucket = &ICache[hash];

    /* Go through the list of translated units */
    ForeachNode(bucket, un.node)
    {
        /* Check if unit is found */
        if (un.unit->mt_Key == key)
        {
            /* Tell CPU we are going to execute the code soon, give it time to prefetch eventually */
            asm volatile ("prfm plil1keep, [%0]"::"r"(un.unit->mt_ARMEntryPoint));

#if EMU68_USE_LRU
            LRU_InsertBlock(un.unit);
#endif
            return un.unit->mt_ARMEntryPoint;
        }
    }

    return NULL;
}

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

void MainLoop()
{
    uint32_t LastPC;
    struct M68KState *ctx = getCTX();

    LRU_InvalidateAll();

    M68K_LoadContext(ctx);

    __asm__ volatile("mov v28.d[0], xzr");

    /* The JIT loop is running forever */
    while(1)
    {   
        /* Load m68k context and last used PC counter into temporary register */ 
        LastPC = getLastPC();
        ctx = getCTX();

#ifndef PISTORM_ANY_MODEL
        if (unlikely(PC == 0)) {
            M68K_SaveContext(ctx);
            return;
        }
#endif

        /* If (unlikely) there was interrupt pending, check if it needs to be processed */
        if (unlikely(ctx->INT64 != 0))
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
                register uint64_t sp __asm__("r29");

                if (likely((SR & SR_S) == 0))
                {
                    /* If we are not yet in supervisor mode, the USP needs to be updated */
                    __asm__ volatile("mov "REG_USP_ASM", %w0": :"r"(sp));

                    /* Load eiter ISP or MSP */
                    if (unlikely((SR & SR_M) != 0))
                    {
                        __asm__ volatile("mov %w0, "REG_MSP_ASM:"=r"(sp));
                    }
                    else
                    {
                        __asm__ volatile("mov %w0, "REG_ISP_ASM:"=r"(sp));
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
                __asm__ volatile("strh %w1, [%0, #-8]!":"=r"(sp):"r"(SRcopy),"0"(sp));
                __asm__ volatile("str %w1, [%0, #2]": :"r"(sp),"r"(PC));
                __asm__ volatile("strh %w1, [%0, #6]": :"r"(sp),"r"(vector));

                /* Set SR */
                setSR(SR);

                /* Get VBR */
                vbr = ctx->VBR;

                /* Load PC */
                __asm__ volatile("ldr %w0, [%1, %2]":"=r"(PC):"r"(vbr),"r"(vector)); 
            }

            /* All interrupts masked or new PC loaded and stack swapped, continue with code execution */
        }

        /* Check if JIT cache is enabled */
        uint32_t cacr;
        __asm__ volatile("mov %w0, "REG_CACR_ASM:"=r"(cacr));

        if (likely(cacr & CACR_IE))
        {
            /* The last PC is the same as currently set PC? */
            if (LastPC == PC)
            {
                /* Jump to the code now */
                ARMCode();
                continue;
            }
            else
            {
                /* Find unit in the hashtable based on the PC value */
                uint32_t *code = FindUnitQuick();

                /* Unit exists ? */
                if (code != NULL)
                {
                    /* Store m68k PC of corresponding ARM code in CTX_LAST_PC */
                    __asm__ volatile("mov "CTX_LAST_PC_ASM", %w0": :"r"(PC));

                    /* This is the case, load entry point into x12 */
                    ARMCode = (void*)code;
                    
                    ARMCode();

                    /* Go back to beginning of the loop */
                    continue;
                }

                /* If we are that far there was no JIT unit found */
                M68K_SaveContext(ctx);

                uint32_t copyPC = getCTX()->PC;

                /* Perform search without testing Epoch */
                struct M68KTranslationUnit __attribute__((may_alias)) *node = NULL;
                struct Node *n;
                uint32_t hash = (copyPC >> EMU68_HASHSHIFT) & EMU68_HASHMASK;
                struct List *bucket = &ICache[hash];

                /* Go through the list of translated units */
                ForeachNode(bucket, n)
                {
                    union {
                        struct Node *n;
                        struct M68KTranslationUnit *u;
                    } conv;

                    conv.n = n;
                    struct M68KTranslationUnit *u = conv.u;

                    /* Check if unit is found */
                    if (u->mt_M68kAddress == copyPC)
                    {
                        /* Node found, most likely Epoch broken */
                        node = M68K_VerifyUnit(u);
                        break;
                    }
                }

                if (node == NULL) {
                    /* Get the code. This never fails */
                    node = M68K_GetTranslationUnit((void*)(uintptr_t)copyPC);
                }

#if EMU68_USE_LRU
                LRU_InsertBlock(node);
#endif
                /* Load CPU context */
                M68K_LoadContext(getCTX());
                __asm__ volatile("mov "CTX_LAST_PC_ASM", %w0": :"r"(PC));
                /* Prepare ARM pointer in x12 and call it */
                ARMCode = node->mt_ARMEntryPoint;
                ARMCode();
            }
        }
        else
        {
            struct M68KTranslationUnit *node = NULL;

            /* Uncached mode - reset LastPC */
            setLastPC(~0);

            /* Save context since C code will be called */
            M68K_SaveContext(ctx);

            /* Find the unit */
            node = FindUnitNoLRU();

            /* If node is found verify it */
            if (likely(node != NULL))
            {
                node = M68K_VerifyUnitCRC32(node);
            }
            /* If node was not found or invalidated, translate code */
            if (unlikely(node == NULL))
            {
                /* Get the code */
                node = M68K_GetTranslationUnit((uint16_t *)(uintptr_t)getCTX()->PC);
            }

            M68K_LoadContext(getCTX());
            ARMCode = node->mt_ARMEntryPoint;
            ARMCode();
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
