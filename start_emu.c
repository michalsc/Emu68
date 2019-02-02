#include <stdio.h>
#include <string.h>

#include "RegisterAllocator.h"
#include "M68k.h"
#include "ARM.h"

uint8_t m68kcode[] = {
    0x20,0x3c,0xde,0xad,0xbe,0xef,  // move.l # - 559038737,d0
    0x20,0x80,
    0x21,0x40,0x00,0x04,
    0x21,0x40,0x00,0x08,
    0x06,0x98,0x01,0x02,0x03,0x04,
    0x06,0x98,0x04,0x03,0x02,0x01,
    0xff,0xff

};
uint32_t armcode[1024];
uint32_t *armcodeptr = armcode;
void *m68kcodeptr = m68kcode;

uint32_t data[128];

void print_context(struct M68KState *m68k)
{
    printf("\nM68K Context:\n");

    for (int i=0; i < 8; i++) {
        if (i==4)
            printf("\n");
        printf("    D%d = 0x%08x", i, BE32(m68k->D[i].u32));
    }
    printf("\n");

    for (int i=0; i < 8; i++) {
        if (i==4)
            printf("\n");
        printf("    A%d = 0x%08x", i, BE32(m68k->A[i].u32));
    }
    printf("\n");

    printf("    PC = 0x%08x    SR = ", BE32((int)m68k->PC));
    uint16_t sr = BE16(m68k->SR);
    if (sr & SR_X)
        printf("X");
    else
        printf(".");

    if (sr & SR_N)
        printf("N");
    else
        printf(".");

    if (sr & SR_Z)
        printf("Z");
    else
        printf(".");

    if (sr & SR_V)
        printf("V");
    else
        printf(".");

    if (sr & SR_C)
        printf("C");
    else
        printf(".");

    printf("\n    USP= 0x%08x    MSP= 0x%08x    ISP= 0x%08x\n", BE32(m68k->USP.u32), BE32(m68k->MSP.u32), BE32(m68k->ISP.u32));
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    M68K_InitializeCache();

    struct M68KTranslationUnit * unit = M68K_GetTranslationUnit(m68kcodeptr);
    struct M68KState m68k;
    register struct M68KState *m68k_ptr asm("r11") = &m68k;

    bzero(&m68k, sizeof(m68k));
    m68k.A[0].u32 = BE32((uint32_t)data);

    if (unit)
    {
        for (uint32_t i=0; i < unit->mt_ARMInsnCnt; i++)
        {
            uint32_t insn = unit->mt_ARMEntryPoint[i];
            printf("    %02x %02x %02x %02x\n", insn & 0xff, (insn >> 8) & 0xff, (insn >> 16) & 0xff, (insn >> 24) & 0xff);
        }
    }

    m68k.PC = (uint16_t *)BE32((uint32_t)unit->mt_M68kAddress);
    m68k.SR = 0;

    print_context(&m68k);
    printf("\nCalling translated code\n");

    asm volatile("setend be\n\tblx %0\n\tsetend le"::"r"(unit->mt_ARMEntryPoint),"r"(m68k_ptr));

    printf("Back from translated code\n");
    print_context(&m68k);

    for (int i=0; i < 64; i++)
    {
        if (i % 8 == 0)
            printf("\n");
        printf("%08x ", BE32(data[i]));
    }
    printf("\n");

    return 0;
}
