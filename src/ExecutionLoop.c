#include <M68k.h>
#include <support.h>
#include <config.h>
#ifdef PISTORM
#ifndef PISTORM32
#define PS_PROTOCOL_IMPL
#include "pistorm/ps_protocol.h"
#endif
#endif

extern struct List ICache[65536];
void M68K_LoadContext(struct M68KState *ctx);
void M68K_SaveContext(struct M68KState *ctx);

static inline void CallARMCode()
{
    register void *ARM asm("x12");
    asm volatile("":"=r"(ARM));
    void (*ptr)() = (void*)ARM;
    ptr();
}

static inline struct M68KTranslationUnit *FindUnit()
{
    register uint16_t *PC asm("x18");
    
    /* Perform search */
    uint32_t hash = (uint32_t)(uintptr_t)PC;
    struct List *bucket = &ICache[(hash >> 5) & 0xffff];
    struct M68KTranslationUnit *node;
    
    /* Go through the list of translated units */
    ForeachNode(bucket, node)
    {
        /* Force reload of PC*/
        asm volatile("":"=r"(PC));

        /* Check if unit is found */
        if (node->mt_M68kAddress == PC)
        {
#if 0
            /* Move node to front of the list */
            REMOVE(&node->mt_HashNode);
            ADDHEAD(bucket, &node->mt_HashNode);
#elif 0
            struct Node *prev = node->mt_HashNode.ln_Pred;
            struct Node *succ = node->mt_HashNode.ln_Succ;
            
            /* If node is not head, then move it one level up */
            if (prev->ln_Pred != NULL)
            {
                node->mt_HashNode.ln_Pred = prev->ln_Pred;
                node->mt_HashNode.ln_Succ = prev;

                prev->ln_Succ = succ;
                prev->ln_Pred = &node->mt_HashNode;

                succ->ln_Pred = prev;
            }
#endif                   
            return node;    
        }
    }

    return NULL;
}

#ifdef PISTORM
#ifndef PISTORM32

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

void MainLoop()
{
    register uint16_t *PC asm("x18");
    register void *ARM asm("x12");
    uint16_t *LastPC;
    struct M68KState *ctx = getCTX();
    
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
#ifdef PISTORM32
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

                if ((SR & SR_S) == 0)
                {
                    /* If we are not yet in supervisor mode, the USP needs to be updated */
                    asm volatile("mov v31.S[1], %w0"::"r"(sp));

                    /* Load eiter ISP or MSP */
                    if (unlikely(SR & SR_M))
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
                if ((SRcopy & 3) != 0 && (SRcopy & 3) < 3)
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
                asm volatile("str %w0, [%1, #2]"::"r"(PC),"r"(sp));
                asm volatile("strh %w0, [%1, #6]"::"r"(vector),"r"(sp));

                /* Set SR */
                asm volatile("msr TPIDR_EL0, %0"::"r"(SR));

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
                /* Jump to the code now */
                CallARMCode();
                continue;
            }
            else
            {
                /* Find unit in the hashtable based on the PC value */
                struct M68KTranslationUnit *node = FindUnit();

                /* Unit exists ? */
                if (node != NULL)
                {
                    /* This is the case, load entry point into x12 */
                    asm volatile("ldr x12, %0"::"m"(node->mt_ARMEntryPoint));
                    /* Store m68k PC of corresponding ARM code in TPIDR_EL1 */
                    asm volatile("msr TPIDR_EL1, %0"::"r"(PC));

                    CallARMCode();

                    /* Go back to beginning of the loop */
                    continue;
                }

                /* If we are that far there was no JIT unit found */
                M68K_SaveContext(ctx);
                /* Store PC*/
                asm volatile("msr TPIDR_EL1, %0":"=r"(PC));
                /* Get the code. This never fails */
                node = M68K_GetTranslationUnit(PC);
                asm volatile("mrs %0, TPIDRRO_EL0":"=r"(ctx));
                asm volatile("mov %0, %1":"=r"(ARM):"r"(node->mt_ARMEntryPoint));
                M68K_LoadContext(ctx);
                CallARMCode();
            }
        }
        else
        {
            struct M68KTranslationUnit *node = NULL;

            /* Uncached mode - reset LastPC */
            asm volatile("msr TPIDR_EL1, %0"::"r"(0));
            
            /* Save context since C code will be called */
            M68K_SaveContext(ctx);
            /* Find the unit */
            node = FindUnit();
            /* Verify it, if NULL was returned Verify will return NULL too */
            node = M68K_VerifyUnit(node);

            /* If node was not found, translate code */
            if (unlikely(node == NULL))
            {
                /* Force reload of PC*/
                asm volatile("":"=r"(PC));
                /* Get the code */
                node = M68K_GetTranslationUnit(PC);
            }

            asm volatile("mov %0, %1":"=r"(ARM):"r"(node->mt_ARMEntryPoint));
            M68K_LoadContext(ctx);
            CallARMCode();
        }
    }
}
