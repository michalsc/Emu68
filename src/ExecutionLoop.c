#include <M68k.h>
#include <support.h>
#include <config.h>
#ifdef PISTORM
#if !defined(PISTORM32) && !defined(PISTORM16)
#define PS_PROTOCOL_IMPL
#include "pistorm/ps_protocol.h"
#endif
#endif

extern struct List ICache[EMU68_HASHSIZE];
void M68K_LoadContext(struct M68KState *ctx);
void M68K_SaveContext(struct M68KState *ctx);

static inline void CallARMCode()
{
    register void *ARM asm("x12");
    asm volatile("":"=r"(ARM));
    void (*ptr)() = (void*)ARM;
    ptr();
}

#define LRU_DEPTH 8

static struct {
    uint16_t * m68k_pc;
    uint32_t * arm_pc;
} LRU[LRU_DEPTH];

static uint32_t LRU_usage;

uint32_t *LRU_FindBlock(uint16_t *address)
{
    uint32_t mask = 1;
    for (int i=0; i < LRU_DEPTH; mask<<=1, i++) {
        if (LRU_usage & mask) {
            if (LRU[i].m68k_pc == address) {
                return LRU[i].arm_pc;
            }
        }
    }

    return NULL;
}

void LRU_InsertBlock(struct M68KTranslationUnit *unit)
{
    int loc = __builtin_ffs(~LRU_usage) - 1;

    // Insert new entry
    LRU[loc].m68k_pc = unit->mt_M68kAddress;
    LRU[loc].arm_pc = unit->mt_ARMEntryPoint;

    LRU_usage |= (1 << loc);
    if (LRU_usage == (1 << LRU_DEPTH) - 1) {
        LRU_usage = (1 << loc);
    }
}

static inline uint32_t * FindUnitQuick()
{
    register uint16_t *PC asm("x18");

#if EMU68_USE_LRU
    uint32_t *code = LRU_FindBlock(PC);

    if (likely(code != NULL))
        return code;
#endif
    struct M68KTranslationUnit *node;

    /* Perform search */
    uint32_t hash = (uint32_t)(uintptr_t)PC;
    struct List *bucket = &ICache[(hash >> EMU68_HASHSHIFT) & EMU68_HASHMASK];

    /* Go through the list of translated units */
    ForeachNode(bucket, node)
    {
        /* Force reload of PC*/
        asm volatile("" : "=r"(PC));

        /* Check if unit is found */
        if (node->mt_M68kAddress == PC)
        {
#if EMU68_USE_LRU
            LRU_InsertBlock(node);
#endif
            return node->mt_ARMEntryPoint;
        }
    }

    return NULL;
}

static inline struct M68KTranslationUnit *FindUnit()
{
    struct M68KTranslationUnit *node;
    register uint16_t *PC asm("x18");

    /* Perform search */
    uint32_t hash = (uint32_t)(uintptr_t)PC;
    struct List *bucket = &ICache[(hash >> EMU68_HASHSHIFT) & EMU68_HASHMASK];

    /* Go through the list of translated units */
    ForeachNode(bucket, node)
    {
        /* Force reload of PC*/
        asm volatile("" : "=r"(PC));

        /* Check if unit is found */
        if (node->mt_M68kAddress == PC)
        {
#if EMU68_USE_LRU
            LRU_InsertBlock(node);
#endif
            return node;
        }
    }

    return NULL;
}

#ifdef PISTORM
#if !defined(PISTORM32) && !defined(PISTORM16)

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
#endif
#else
static inline int GetIPLLevel() { return 0; }
#endif

static inline uint16_t *getLastPC()
{
    uint16_t *lastPC;
    asm volatile("mrs %0, TPIDR_EL1":"=r"(lastPC));
    return lastPC;
}

static inline struct M68KState *getCTX()
{
    struct M68KState *ctx;
    asm volatile("mrs %0, TPIDRRO_EL0":"=r"(ctx));
    return ctx;
}

static inline uint32_t getSR()
{
    uint32_t sr;
    asm volatile("mrs %0, TPIDR_EL0":"=r"(sr));
    return sr;
}

static inline void setLastPC(uint16_t *pc)
{
    asm volatile("msr TPIDR_EL1, %0"::"r"(pc));
}

static inline void setSR(uint32_t sr)
{
    asm volatile("msr TPIDR_EL0, %0"::"r"(sr));
}

void MainLoop()
{
    register uint16_t *PC asm("x18");
    register void *ARM asm("x12");
    uint16_t *LastPC;
    struct M68KState *ctx = getCTX();

    LRU_usage = 0;

    M68K_LoadContext(ctx);

    asm volatile("mov v28.d[0], xzr");

    /* The JIT loop is running forever */
    while(1)
    {   
        /* Load m68k context and last used PC counter into temporary register */ 
        LastPC = getLastPC();
        ctx = getCTX();

        /* If (unlikely) there was interrupt pending, check if it needs to be processed */
        if (unlikely(ctx->INT32 != 0))
        {
            uint32_t SR, SRcopy;
            int level = 0;
            uint32_t vector;
            uint32_t vbr;

            /* Find out requested IPL level based on ARM state and real IPL line */
            if (ctx->INT.ARM_err)
            {
                level = 7;
                ctx->INT.ARM_err = 0;
            }
            else
            {
                if (ctx->INT.ARM)
                {
                    level = 6;
                    ctx->INT.ARM = 0;
                }
#if defined(PISTORM32) || defined(PISTORM16)
                /* On PiStorm32 IPL level is obtained by second CPU core from the GPIO directly */
                if (ctx->INT.IPL > level)
                {
                    level = ctx->INT.IPL;
                }    
#else
                /* On classic pistorm we need to obtain IPL from PiStorm status register */
                if (ctx->INT.IPL)
                {
                    int ipl_level;

#if PISTORM_WRITE_BUFFER
                    while(__atomic_test_and_set(&bus_lock, __ATOMIC_ACQUIRE)) { asm volatile("yield"); }
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
                register uint64_t sp asm("r29");

                if (likely((SR & SR_S) == 0))
                {
                    /* If we are not yet in supervisor mode, the USP needs to be updated */
                    asm volatile("mov v31.S[1], %w0"::"r"(sp));

                    /* Load eiter ISP or MSP */
                    if (unlikely((SR & SR_M) != 0))
                    {
                        asm volatile("mov %w0, v31.S[3]":"=r"(sp));
                    }
                    else
                    {
                        asm volatile("mov %w0, v31.S[2]":"=r"(sp));
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
                asm volatile("strh %w1, [%0, #-8]!":"=r"(sp):"r"(SRcopy),"0"(sp));
                asm volatile("str %w1, [%0, #2]"::"r"(sp),"r"(PC));
                asm volatile("strh %w1, [%0, #6]"::"r"(sp),"r"(vector));

                /* Set SR */
                setSR(SR);

                /* Get VBR */
                vbr = ctx->VBR;

                /* Load PC */
                asm volatile("ldr %w0, [%1, %2]":"=r"(PC):"r"(vbr),"r"(vector)); 
            }

            /* All interrupts masked or new PC loaded and stack swapped, continue with code execution */
        }

        /* Check if JIT cache is enabled */
        uint32_t cacr;
        asm volatile("mov %w0, v31.s[0]":"=r"(cacr));

        if (likely(cacr & CACR_IE))
        {   
            /* Force reload of PC*/
            asm volatile("":"=r"(PC));

            /* The last PC is the same as currently set PC? */
            if (LastPC == PC)
            {
                asm volatile("":"=r"(ARM));
                /* Jump to the code now */
                CallARMCode();
                continue;
            }
            else
            {
                /* Find unit in the hashtable based on the PC value */
                uint32_t *code = FindUnitQuick();

                /* Unit exists ? */
                if (code != NULL)
                {
                    /* Store m68k PC of corresponding ARM code in TPIDR_EL1 */
                    asm volatile("msr TPIDR_EL1, %0"::"r"(PC));

                    /* This is the case, load entry point into x12 */
                    ARM = code;
                    asm volatile("":"=r"(ARM):"0"(ARM));
                    
                    CallARMCode();

                    /* Go back to beginning of the loop */
                    continue;
                }

                /* If we are that far there was no JIT unit found */
                asm volatile("":"=r"(PC));
                uint16_t *copyPC = PC;
                M68K_SaveContext(ctx);
                /* Get the code. This never fails */
                struct M68KTranslationUnit *node = M68K_GetTranslationUnit(copyPC);
#if EMU68_USE_LRU
                LRU_InsertBlock(node);
#endif
                /* Load CPU context */
                M68K_LoadContext(getCTX());
                asm volatile("msr TPIDR_EL1, %0"::"r"(PC));
                /* Prepare ARM pointer in x12 and call it */
                ARM = node->mt_ARMEntryPoint;
                asm volatile("":"=r"(ARM):"0"(ARM));
                CallARMCode();
            }
        }
        else
        {
            struct M68KTranslationUnit *node = NULL;

            /* Uncached mode - reset LastPC */
            setLastPC((void*)~(0));

            /* Save context since C code will be called */
            M68K_SaveContext(ctx);

            /* Find the unit */
            node = FindUnit();
            /* If node is found verify it */
            if (likely(node != NULL))
            {
                node = M68K_VerifyUnit(node);
            }
            /* If node was not found or invalidated, translate code */
            if (unlikely(node == NULL))
            {
                /* Get the code */
                node = M68K_GetTranslationUnit((uint16_t *)(uintptr_t)getCTX()->PC);
            }

            M68K_LoadContext(getCTX());
            ARM = node->mt_ARMEntryPoint;
            asm volatile("":"=r"(ARM):"0"(ARM));
            CallARMCode();
        }
    }
}
