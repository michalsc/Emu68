#include <M68k.h>
#include <support.h>
#include <config.h>
#ifdef PISTORM_CLASSIC
#define PS_PROTOCOL_IMPL
#include "pistorm/ps_protocol.h"
#endif

extern struct List ICache[EMU68_HASHSIZE];
void M68K_LoadContext(struct M68KState *ctx);
void M68K_SaveContext(struct M68KState *ctx);

static inline void CallARMCode()
{
    register void *ARM __asm__("x12");
    __asm__ volatile("":"=r"(ARM));
    void (*ptr)() = (void*)ARM;
    ptr();
}

#define LRU_DEPTH 8

uint16_t *      LRU_m68k[LRU_DEPTH];
uint32_t *      LRU_arm[LRU_DEPTH];
static uint32_t LRU_usage;

uint32_t *LRU_FindBlock(uint16_t *address)
{
    uint32_t mask = 1;
    for (int i=0; i < LRU_DEPTH; mask<<=1, i++) {
        if (LRU_usage & mask) {
            if (LRU_m68k[i] == address) {
                return LRU_arm[i];
            }
        }
    }

    return NULL;
}

void LRU_MarkForVerify(uint32_t *addr)
{
    uint32_t mask = 1;
    for (int i = 0; i < LRU_DEPTH; mask <<= 1, i++)
    {
        if (LRU_usage & mask)
        {
            if (LRU_arm[i] == addr)
            {
                uintptr_t e = (uintptr_t)addr;
                e &= 0x00ffffffffffffffULL;
                e |= 0xaa00000000000000ULL;
                LRU_arm[i] = (uint32_t *)e;
            }
        }
    }
}

void LRU_InvalidateByARMAddress(uint32_t *addr)
{
    uint32_t mask = 1;
    for (int i = 0; i < LRU_DEPTH; mask <<= 1, i++)
    {
        if (LRU_usage & mask)
        {
            if (LRU_arm[i] == addr)
            {
                LRU_usage &= ~mask;
            }
        }
    }
}

void LRU_InvalidateByM68kAddress(uint16_t *addr)
{
    uint32_t mask = 1;
    for (int i = 0; i < LRU_DEPTH; mask <<= 1, i++)
    {
        if (LRU_usage & mask)
        {
            if (LRU_m68k[i] == addr)
            {
                LRU_usage &= ~mask;
            }
        }
    }
}

void LRU_InvalidateAll()
{
    LRU_usage = 0;
}

void LRU_InsertBlock(struct M68KTranslationUnit *unit)
{
    int loc = __builtin_ffs(~LRU_usage) - 1;

    // Insert new entry
    LRU_m68k[loc] = unit->mt_M68kAddress;
    LRU_arm[loc] = unit->mt_ARMEntryPoint;

    LRU_usage |= (1 << loc);
    if (LRU_usage == (1 << LRU_DEPTH) - 1) {
        LRU_usage = (1 << loc);
    }
}

static inline uint32_t * FindUnitQuick()
{
    register uint16_t *PC __asm__("x18");

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
        __asm__ volatile("" : "=r"(PC));

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
    register uint16_t *PC __asm__("x18");

    /* Perform search */
    uint32_t hash = (uint32_t)(uintptr_t)PC;
    struct List *bucket = &ICache[(hash >> EMU68_HASHSHIFT) & EMU68_HASHMASK];

    /* Go through the list of translated units */
    ForeachNode(bucket, node)
    {
        /* Force reload of PC*/
        __asm__ volatile("" : "=r"(PC));

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

static inline uint16_t *getLastPC()
{
    uint16_t *lastPC;
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

static inline void setLastPC(uint16_t *pc)
{
    __asm__ volatile("mov "CTX_LAST_PC_ASM", %w0": :"r"(pc));
}

static inline void setSR(uint32_t sr)
{
    __asm__ volatile("mov "REG_SR_ASM", w0": :"r"(sr));
}

void MainLoop()
{
    register uint16_t *PC __asm__("x18");
    register void *ARM __asm__("x12");
    uint16_t *LastPC;
    struct M68KState *ctx = getCTX();

    LRU_usage = 0;

    M68K_LoadContext(ctx);

    __asm__ volatile("mov v28.d[0], xzr");

    /* The JIT loop is running forever */
    while(1)
    {   
        /* Load m68k context and last used PC counter into temporary register */ 
        LastPC = getLastPC();
        ctx = getCTX();

#ifndef PISTORM_ANY_MODEL
        /* Force reload of PC*/
        __asm__ volatile("" : "=r"(PC));
        if (unlikely(PC == NULL)) {
            M68K_SaveContext(ctx);
            return;
        }
#endif

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
#if defined(PISTORM)
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
            /* Force reload of PC*/
            __asm__ volatile("":"=r"(PC));

            /* The last PC is the same as currently set PC? */
            if (LastPC == PC)
            {
                __asm__ volatile("":"=r"(ARM));
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
                    /* Store m68k PC of corresponding ARM code in CTX_LAST_PC */
                    __asm__ volatile("mov "CTX_LAST_PC_ASM", %w0": :"r"(PC));

                    /* This is the case, load entry point into x12 */
                    ARM = code;
                    __asm__ volatile("":"=r"(ARM):"0"(ARM));
                    
                    CallARMCode();

                    /* Go back to beginning of the loop */
                    continue;
                }

                /* If we are that far there was no JIT unit found */
                __asm__ volatile("":"=r"(PC));
                uint16_t *copyPC = PC;
                M68K_SaveContext(ctx);
                /* Get the code. This never fails */
                struct M68KTranslationUnit *node = M68K_GetTranslationUnit(copyPC);
#if EMU68_USE_LRU
                LRU_InsertBlock(node);
#endif
                /* Load CPU context */
                M68K_LoadContext(getCTX());
                __asm__ volatile("mov "CTX_LAST_PC_ASM", %w0": :"r"(PC));
                /* Prepare ARM pointer in x12 and call it */
                ARM = node->mt_ARMEntryPoint;
                __asm__ volatile("":"=r"(ARM):"0"(ARM));
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
            __asm__ volatile("":"=r"(ARM):"0"(ARM));
            CallARMCode();
        }
    }
}
