/*
    Copyright © 2019-2025 Michal Schulz <michal.schulz@gmx.de>
    https://github.com/michalsc

    This Source Code Form is subject to the terms of the
    Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed
    with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#define _GNU_SOURCE 1

#define restrict __restrict__

#include <emu68/List>
#include <emu68/Node>
#include <emu68/LRUCache>

#include "PPC.h"
#include "A64.h"
#include "intc.h"
#include "disasm.h"
#include "DuffCopy.h"
#include "config.h"
#include "support.h"
#include "tlsf.h"
#include "spinlock.h"
#include "doorbell.h"
#include "cache.h"
#include "mmu.h"

#define jit_tlsf DO_NOT_USE_jit_tlsf

namespace Emu68::PPC {

LRUCache __used__ cache __attribute__((aligned(64)));

__attribute__((aligned(4096))) uint8_t ppc_tmp_stack[65536];
__attribute__((aligned(4096))) 
#include "ppc_rom.h"

Emu68::List<PPCTranslationUnit> ICache[EMU68_HASHSIZE] __attribute__((aligned(256)));
Emu68::List<TranslationUnitLRU> LRU;
extern TLSF jit_ppc;
extern PPCTranslatorContext local_translator;
extern struct PPCLocalState *local_state;

PPCTranslationUnit *ppcVerifyUnit(PPCTranslationUnit *unit);
PPCTranslationUnit *PPC_GetTranslationUnit(uint32_t *ppccodeptr);

register uint32_t PC __asm__("w18");
register void (*ARMCode)() __asm__("x12");

void loadContext(struct PPCState *ctx)
{
    __asm__ volatile("mov " CTX_POINTER_ASM ", %0\n"::"r"(ctx));

    __asm__ volatile("mov " CTX_INSN_COUNT_ASM ", %0"::"r"(ctx->INSN_COUNT));
    __asm__ volatile("mov " REG_FPSCR_ASM ", %w0"::"r"(ctx->FPSCR));

    __asm__ volatile("ldp w%0, w%1, %2"::"i"(INT_REG_MAPPING[0]),"i"(INT_REG_MAPPING[1]),"m"(ctx->GPR[0]));
    __asm__ volatile("ldp w%0, w%1, %2"::"i"(INT_REG_MAPPING[2]),"i"(INT_REG_MAPPING[3]),"m"(ctx->GPR[2]));
    __asm__ volatile("ldp w%0, w%1, %2"::"i"(INT_REG_MAPPING[4]),"i"(INT_REG_MAPPING[5]),"m"(ctx->GPR[4]));
    __asm__ volatile("ldp w%0, w%1, %2"::"i"(INT_REG_MAPPING[6]),"i"(INT_REG_MAPPING[7]),"m"(ctx->GPR[6]));
    __asm__ volatile("ldp w%0, w%1, %2"::"i"(INT_REG_MAPPING[8]),"i"(INT_REG_MAPPING[9]),"m"(ctx->GPR[8]));
    __asm__ volatile("ldp w%0, w%1, %2"::"i"(INT_REG_MAPPING[10]),"i"(INT_REG_MAPPING[11]),"m"(ctx->GPR[10]));
    __asm__ volatile("ldp w%0, w%1, %2"::"i"(INT_REG_MAPPING[12]),"i"(INT_REG_MAPPING[13]),"m"(ctx->GPR[12]));

    __asm__ volatile("ldr w%0, %1"::"i"(INT_REG_MAPPING[LRn]),"m"(ctx->LR));
    __asm__ volatile("ldr w%0, %1"::"i"(INT_REG_MAPPING[CTRn]),"m"(ctx->CTR));
    __asm__ volatile("ldr w%0, %1"::"i"(REG_PC),"m"(ctx->PC));

    __asm__ volatile("ld1 {v%0.4s}, [%1]"::"i"(GPR14_VN),"r"(&ctx->GPR[14]));
    __asm__ volatile("ld1 {v%0.4s}, [%1]"::"i"(GPR18_VN),"r"(&ctx->GPR[18]));
    __asm__ volatile("ld1 {v%0.4s}, [%1]"::"i"(GPR22_VN),"r"(&ctx->GPR[22]));
    __asm__ volatile("ld1 {v%0.4s}, [%1]"::"i"(GPR26_VN),"r"(&ctx->GPR[26]));
    __asm__ volatile("ld1 {v%0.4s}, [%1]"::"i"(GPR30_VN),"r"(&ctx->GPR[30]));

    /* FPU part... */
    __asm__ volatile("ldp d%0, d%1, %2"::"i"(REG_FPR0),"i"(REG_FPR1),"m"(ctx->FPR[0]));
    __asm__ volatile("ldp d%0, d%1, %2"::"i"(REG_FPR2),"i"(REG_FPR3),"m"(ctx->FPR[2]));
    __asm__ volatile("ldp d%0, d%1, %2"::"i"(REG_FPR4),"i"(REG_FPR5),"m"(ctx->FPR[4]));
    __asm__ volatile("ldp d%0, d%1, %2"::"i"(REG_FPR6),"i"(REG_FPR7),"m"(ctx->FPR[6]));
    __asm__ volatile("ldp d%0, d%1, %2"::"i"(REG_FPR8),"i"(REG_FPR9),"m"(ctx->FPR[8]));
    __asm__ volatile("ldp d%0, d%1, %2"::"i"(REG_FPR10),"i"(REG_FPR11),"m"(ctx->FPR[10]));
    __asm__ volatile("ldp d%0, d%1, %2"::"i"(REG_FPR12),"i"(REG_FPR13),"m"(ctx->FPR[12]));
    __asm__ volatile("ldp d%0, d%1, %2"::"i"(REG_FPR29),"i"(REG_FPR30),"m"(ctx->FPR[29]));
    __asm__ volatile("ldr d%0, %1"::"i"(REG_FPR31),"m"(ctx->FPR[31]));
}

void saveContext(struct PPCState *ctx)
{
    __asm__ volatile("mov x1, " CTX_INSN_COUNT_ASM "; str x1, %0"::"m"(ctx->INSN_COUNT):"x1");
    __asm__ volatile("mov w1, " REG_FPSCR_ASM "; str w1, %0"::"m"(ctx->FPSCR):"x1");

    __asm__ volatile("stp w%0, w%1, %2"::"i"(INT_REG_MAPPING[0]),"i"(INT_REG_MAPPING[1]),"m"(ctx->GPR[0]));
    __asm__ volatile("stp w%0, w%1, %2"::"i"(INT_REG_MAPPING[2]),"i"(INT_REG_MAPPING[3]),"m"(ctx->GPR[2]));
    __asm__ volatile("stp w%0, w%1, %2"::"i"(INT_REG_MAPPING[4]),"i"(INT_REG_MAPPING[5]),"m"(ctx->GPR[4]));
    __asm__ volatile("stp w%0, w%1, %2"::"i"(INT_REG_MAPPING[6]),"i"(INT_REG_MAPPING[7]),"m"(ctx->GPR[6]));
    __asm__ volatile("stp w%0, w%1, %2"::"i"(INT_REG_MAPPING[8]),"i"(INT_REG_MAPPING[9]),"m"(ctx->GPR[8]));
    __asm__ volatile("stp w%0, w%1, %2"::"i"(INT_REG_MAPPING[10]),"i"(INT_REG_MAPPING[11]),"m"(ctx->GPR[10]));
    __asm__ volatile("stp w%0, w%1, %2"::"i"(INT_REG_MAPPING[12]),"i"(INT_REG_MAPPING[13]),"m"(ctx->GPR[12]));

    __asm__ volatile("str w%0, %1"::"i"(INT_REG_MAPPING[LRn]),"m"(ctx->LR));
    __asm__ volatile("str w%0, %1"::"i"(INT_REG_MAPPING[CTRn]),"m"(ctx->CTR));
    __asm__ volatile("str w%0, %1"::"i"(REG_PC),"m"(ctx->PC));

    __asm__ volatile("st1 {v%0.4s}, [%1]"::"i"(GPR14_VN),"r"(&ctx->GPR[14]));
    __asm__ volatile("st1 {v%0.4s}, [%1]"::"i"(GPR18_VN),"r"(&ctx->GPR[18]));
    __asm__ volatile("st1 {v%0.4s}, [%1]"::"i"(GPR22_VN),"r"(&ctx->GPR[22]));
    __asm__ volatile("st1 {v%0.4s}, [%1]"::"i"(GPR26_VN),"r"(&ctx->GPR[26]));
    __asm__ volatile("st1 {v%0.4s}, [%1]"::"i"(GPR30_VN),"r"(&ctx->GPR[30]));

    /* FPU part... */
    __asm__ volatile("stp d%0, d%1, %2"::"i"(REG_FPR0),"i"(REG_FPR1),"m"(ctx->FPR[0]));
    __asm__ volatile("stp d%0, d%1, %2"::"i"(REG_FPR2),"i"(REG_FPR3),"m"(ctx->FPR[2]));
    __asm__ volatile("stp d%0, d%1, %2"::"i"(REG_FPR4),"i"(REG_FPR5),"m"(ctx->FPR[4]));
    __asm__ volatile("stp d%0, d%1, %2"::"i"(REG_FPR6),"i"(REG_FPR7),"m"(ctx->FPR[6]));
    __asm__ volatile("stp d%0, d%1, %2"::"i"(REG_FPR8),"i"(REG_FPR9),"m"(ctx->FPR[8]));
    __asm__ volatile("stp d%0, d%1, %2"::"i"(REG_FPR10),"i"(REG_FPR11),"m"(ctx->FPR[10]));
    __asm__ volatile("stp d%0, d%1, %2"::"i"(REG_FPR12),"i"(REG_FPR13),"m"(ctx->FPR[12]));
    __asm__ volatile("stp d%0, d%1, %2"::"i"(REG_FPR29),"i"(REG_FPR30),"m"(ctx->FPR[29]));
    __asm__ volatile("str d%0, %1"::"i"(REG_FPR31),"m"(ctx->FPR[31]));
}

static __used__ void PrintContext(struct PPCState *ppc)
{
    kprintf("[PPC] PPC Context: ");

    for (int i=0; i < 32; i++) {
        if (i % 4 == 0)
            kprintf("\n[PPC] ");
        kprintf("   r%02d = 0x%08x", i, BE32(ppc->GPR[i]));
    }
    kprintf("\n[PPC]\n[PPC] ");

    kprintf("   PC = 0x%08x     LR = 0x%08x ", BE32(ppc->PC), BE32(ppc->LR));
    kprintf("\n[PPC] ");
    kprintf("   CR = 0x%08x    CTR = 0x%08x   XER = 0x%08x\n", BE32(ppc->CR), BE32(ppc->CTR), BE32(ppc->XER));
    
    kprintf("[PPC]\n[PPC] ");

    for (int i=0; i < 32; i++) {
        union {
            double d;
            uint64_t u64;
            uint32_t u[2];
        } u;
        if ((i != 0) && ((i & 1) == 0))
            kprintf("\n[PPC] ");
        u.u64 = ppc->FPR_u64[i];
        kprintf("   fr%02d = %08x%08x (%f)", i, u.u[0], u.u[1], u.d);
    }
    kprintf("\n[PPC]\n[PPC]    SRR0 = 0x%08x   SRR1 = 0x%08x   MSR = 0x%08x\n", BE32(ppc->SRR0), BE32(ppc->SRR1), BE32(ppc->MSR));
    kprintf("[PPC]    SPRG = { 0x%08x, 0x%08x, 0x%08x, 0x%08x,\n", BE32(ppc->SPRG[0]), BE32(ppc->SPRG[1]), BE32(ppc->SPRG[2]), BE32(ppc->SPRG[3]));
    kprintf("[PPC]             0x%08x, 0x%08x, 0x%08x, 0x%08x }\n", BE32(ppc->SPRG[4]), BE32(ppc->SPRG[5]), BE32(ppc->SPRG[6]), BE32(ppc->SPRG[7]));
}

static inline uint32_t * FindUnitQuick()
{
#if EMU68_USE_LRU
    uint32_t *code = cache.findBlock(PC);

    if (likely(code != NULL))
        return code;
#endif

    union {
        struct {
            uint32_t    ptu_Epoch;          /* 16: 2 x 4 bytes - first 32-bit epoch incremented after every cache flush */
            uint32_t    ptu_PPCAddress;     /*                   followed by 32-bit PPC entry address */
        };
        uint64_t        ptu_Key;            /*     1 x 8 bytes - match key, the two above combined */
    } u;

    u.ptu_Epoch = GET_EPOCH();
    u.ptu_PPCAddress = PC;

    uint64_t key = u.ptu_Key;

    /* Perform search */
    uint32_t hash = (PC >> EMU68_HASHSHIFT) & EMU68_HASHMASK;
    Emu68::List<PPCTranslationUnit> &bucket = ICache[hash];

    /* Go through the list of translated units */
    for(auto node: bucket)
    {
        /* Check if unit is found */
        if (node->ptu_Key == key)
        {
            /* Tell CPU we are going to execute the code soon, give it time to prefetch eventually */
            asm volatile ("prfm plil1keep, [%0]"::"r"(node->ptu_ARMEntryPoint));

#if EMU68_USE_LRU
            cache.insertBlock(node->ptu_PPCAddress, (uint32_t*)node->ptu_ARMEntryPoint);
#endif
            return (uint32_t *)(node->ptu_ARMEntryPoint);
        }
    }

    return nullptr;
}

static inline uint32_t getLastPC()
{
    uint32_t lastPC;
    __asm__ volatile("mov %w0, " CTX_LAST_PC_ASM:"=r"(lastPC));
    return lastPC;
}

static inline struct PPCState *getCTX()
{
    struct PPCState *ctx;
    __asm__ volatile("mov %0, " CTX_POINTER_ASM:"=r"(ctx));
    return ctx;
}

static inline void setLastPC(uint32_t pc)
{
    __asm__ volatile("mov " CTX_LAST_PC_ASM ", %w0": :"r"(pc));
}

void PPCMainLoop()
{
    uint32_t LastPC;
    struct PPCState *ctx = GET_HOST_CTX();

    cache.invalidateAll();

    loadContext(ctx);

    /* The JIT loop is running forever */
    while(1)
    {   
        /* Load m68k context and last used PC counter into temporary register */ 
        LastPC = getLastPC();
        ctx = GET_HOST_CTX();

#ifndef PISTORM_ANY_MODEL
        if (unlikely(PC == 0)) {
            saveContext(ctx);
            return;
        }
#endif

        /* If any interrupts are pending */
        if (unlikely(ctx->INT64))
        {
            uint32_t vector = 0;

            /* Check flags by the priority */
            if (ctx->INT.EXT) {
                if (ctx->MSR & MSR_EE) {
                    vector = 0x500;
                }
            }
            else if (ctx->INT.DEC) {
                if (ctx->MSR & MSR_EE) {
                    ctx->INT.DEC = 0;
                    vector = 0x900;
                }
            }

            if (vector == 0x500) ctx->INT.EXT = 0;

            if (vector) {
                /* 
                    When entering interrupt or exception:
                    - remember PC and MSR in SRR0 and SRR1
                    - clear almost all MSR flags, keep IP and ILE
                    - copy ILE bit to LE bit
                    - if IP is set, put vector into 0xfff0xxxx space
                    - Load PC with new address
                    - reset LastPC to avoid short JIT path
                */

                /* Store SRR1 (MSR) and SRR0 (PC) */
                ctx->SRR0 = PC;
                ctx->SRR1 = ctx->MSR;

                /* IP == 1 -> use high vectors, low vectors otherwise */
                if (ctx->MSR & MSR_IP) {
                    vector |= 0xfff00000;
                }

                /* MSR is almost entirely cleared, keep IP and ILE */
                ctx->MSR &= MSR_IP | MSR_ILE;

                /* Normally ILE should be copied to LE to set endian mode. we ignore that */
                //ctx->MSR |= (ctx->MSR >> 16) & 1;

                /* Set PC to new vector */
                LastPC = 0xffffffff;
                PC = vector;
            }
        }

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
            if (likely(code != NULL))
            {
                /* Store m68k PC of corresponding ARM code in CTX_LAST_PC */
                __asm__ volatile("mov " CTX_LAST_PC_ASM ", %w0": :"r"(PC));

                /* This is the case, load entry point into x12 */
                ARMCode = (void (*)())code;
                
                ARMCode();

                /* Go back to beginning of the loop */
                continue;
            }

            /* If we are that far there was no JIT unit found */
            saveContext(ctx);

            uint32_t copyPC = getCTX()->PC;

            /* Perform search without testing Epoch */
            PPCTranslationUnit *node = nullptr;
            uint32_t hash = (copyPC >> EMU68_HASHSHIFT) & EMU68_HASHMASK;
            auto bucket = &ICache[hash];

            /* Go through the list of translated units */
            for(auto n: *bucket)
            {
                /* Check if unit is found */
                if (n->ptu_PPCAddress == copyPC)
                {
                    /* Node found, most likely Epoch broken */
                    node = ppcVerifyUnit(n);
                    break;
                }
            }

            if (node == NULL) {
                /* Get the code. This never fails */
                node = PPC_GetTranslationUnit((uint32_t *)(uintptr_t)copyPC);
            }

#if EMU68_USE_LRU
            cache.insertBlock(node->ptu_PPCAddress, (uint32_t *)node->ptu_ARMEntryPoint);
#endif
            /* Load CPU context */
            loadContext(GET_HOST_CTX());
            __asm__ volatile("mov " CTX_LAST_PC_ASM ", %w0": :"r"(PC));
            /* Prepare ARM pointer in x12 and call it */
            ARMCode = (void (*)())node->ptu_ARMEntryPoint;
            ARMCode();
        }
    }
}

uint64_t arm_cycle_counter_start;

} // Emu68::PPC

spinlock_t PPCStart;

extern "C" void InitPPC()
{
    
    kprintf("[PPC] InitPPC()\n");

    kprintf("[PPC] JIT memory at %p\n", (void*)(0xffffffe000000000 + ((KERNEL_JIT_PAGES / 2) << 21)));

    

    kprintf("[PPC] Setting up LRU\n");

    kprintf("[PPC] LRU Cache @ %p\n", &Emu68::PPC::cache);

    asm volatile("mov " EPOCH_ASM ", %w0"::"r"(0));

    kprintf("[PPC] Setting up ICache\n");

    Emu68::PPC::local_translator.tc_CodeStart = (uint32_t *)Emu68::PPC::jit_ppc.malloc((JCCB_INSN_DEPTH_MASK + 1) * 16 * 64);
    kprintf("[PPC] Temporary code at %p\n", Emu68::PPC::local_translator.tc_CodeStart);
    Emu68::PPC::local_state = (struct Emu68::PPC::PPCLocalState *)tlsf_malloc(tlsf, sizeof(Emu68::PPC::PPCLocalState)*(JCCB_INSN_DEPTH_MASK + 1)*2);
    kprintf("[PPC] ICache array at %p\n", Emu68::PPC::ICache);

    kprintf("[PPC] Mapping PPC ROM at 0x%08x - 0x%08x\n", 0xfff00000, 0xfff00000 + Emu68::PPC::ppc_rom_img_len - 1);
    kprintf("[PPC] Mapping PPC boot stack at 0x%08x - 0x%08x\n", 0xfff00000 - sizeof(Emu68::PPC::ppc_tmp_stack), 0xfff00000 - 1);
    mmu_map(mmu_virt2phys((uintptr_t)Emu68::PPC::ppc_rom_img), 0xfff00000, Emu68::PPC::ppc_rom_img_len, MMU_ACCESS | MMU_ISHARE | MMU_ALLOW_EL0 | MMU_READ_ONLY | MMU_ATTR_CACHED, 0);
    mmu_map(mmu_virt2phys((uintptr_t)Emu68::PPC::ppc_tmp_stack), 0xfff00000 - sizeof(Emu68::PPC::ppc_tmp_stack), sizeof(Emu68::PPC::ppc_tmp_stack), MMU_ACCESS | MMU_ISHARE | MMU_ALLOW_EL0 | MMU_ATTR_CACHED, 0);
}


extern struct M68KState * volatile __m68k_state;
uint32_t ppc_boot_addr;

extern "C" void StartupPPC()
{
    static struct Emu68::PPC::PPCState ppc;

    /* Init spinlock as busy */
    PPCStart.lock = 1;

    /* Init PPCState */
    bzero(&ppc, sizeof(ppc));

    /* Init FPR with NAN */
    for (int fp=0; fp < 32; fp++) {
        ppc.FPR_u64[fp] = 0x7fffffffffffffffULL;
    }

    ppc.MSR = MSR_IP | MSR_RI;

    ppc.JIT_CACHE_TOTAL = Emu68::PPC::jit_ppc.total_size();
    ppc.JIT_CACHE_FREE = Emu68::PPC::jit_ppc.free_size();
    ppc.JIT_UNIT_COUNT = 0;
    ppc.JIT_CONTROL = 0;
    ppc.JIT_CONTROL |= (EMU68_M68K_INSN_DEPTH & JCCB_INSN_DEPTH_MASK) << JCCB_INSN_DEPTH;
    ppc.JIT_CONTROL |= (EMU68_BRANCH_INLINE_DISTANCE & JCCB_INLINE_RANGE_MASK) << JCCB_INLINE_RANGE;
    ppc.JIT_CONTROL |= (EMU68_MAX_LOOP_COUNT & JCCB_LOOP_COUNT_MASK) << JCCB_LOOP_COUNT;
    ppc.JIT_CONTROL2 = 0;
    
    /* Start at reset vector */
    ppc.PC = 0xfff00100;

    /* Put some start parameters for now, remove later */
    extern uint16_t *framebuffer;
    extern uint32_t pitch;
    extern uint32_t fb_width;
    extern uint32_t fb_height;

    /* Set PPC context pointer */
    __asm__ volatile("mov " CTX_POINTER_ASM ", %0\n"::"r"(&ppc));

    kprintf("[PPC] Waiting for startup\n");

    spinlock_acquire(&PPCStart);

    kprintf("[PPC] Starting up!\n");

    ppc.GPR[3] = BE32((uint32_t)(intptr_t)framebuffer);
    ppc.GPR[4] = BE32((uint32_t)fb_width);
    ppc.GPR[5] = BE32((uint32_t)fb_height);
    ppc.GPR[6] = BE32((uint32_t)pitch);
    ppc.CTR = BE32((uint32_t)ppc_boot_addr);

    Emu68::PPC::PrintContext(&ppc);

    /* It is time to enable the non-secure physical timer on GIC, if available */
    if (gic_available()) {
        gic_local_init();
        gic_set_priority(GIC_PPI_NPTIMER, 0x80);
        gic_irq_eanble(GIC_PPI_NPTIMER);
    }

    /* Enable external interrupts on ARM - it doesn't mean PPC will get them */
    __asm__ volatile("msr daifclr, #7");

    /* Get arm cycle counter on start - debug only, can go away later */
    __asm__ volatile("mrs %0, PMCCNTR_EL0":"=r"(Emu68::PPC::arm_cycle_counter_start));

    /* Chain pointers to flag causing interrupt on the counterpart */
    __m68k_state->PPC_EE_FLAG = &ppc.INT.EXT;
    ppc.M68K_FLAG = &__m68k_state->INTF.PPC;

    Emu68::PPC::PPCMainLoop();

    Emu68::PPC::PrintContext(&ppc);
}

extern "C" void PPCReportInterrupt(int interrupt)
{
    struct Emu68::PPC::PPCState * ctx = Emu68::PPC::GET_HOST_CTX();

    switch (interrupt)
    {
        case 0x900:
            ctx->INT.DEC = 1;
            break;
        case 0x500:
            ctx->INT.EXT = 1;
            break;
    }
}
