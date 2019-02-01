#include <stdio.h>
#include <string.h>

#include "RegisterAllocator.h"
#include "M68k.h"
#include "ARM.h"

uint16_t m68kcode[] = {
    0x7090,
    0xffff,
    0x23b0,0x0d22,0x3039,   // 	move.l ([12345,a0,d0.l*4],6789),([2345,a1],d2.w*2,999)
    0x1a85,0x2326,0x0929,0x03e7,
    0xffff,
};
uint32_t armcode[1024];
uint32_t *armcodeptr = armcode;
uint16_t *m68kcodeptr = m68kcode;


static inline uint32_t BE32(uint32_t x)
{
    union {
        uint32_t v;
        uint8_t u[4];
    } tmp;

    tmp.v = x;

    return (tmp.u[0] << 24) | (tmp.u[1] << 16) | (tmp.u[2] << 8) | (tmp.u[3]);
}

static inline uint16_t BE16(uint16_t x)
{
    union {
        uint16_t v;
        uint8_t u[2];
    } tmp;

    tmp.v = x;

    return (tmp.u[0] << 8) | (tmp.u[1]);
}

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

    if (sr & SR_Z)
        printf("Z");
    else
        printf(".");

    if (sr & SR_N)
        printf("N");
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

    printf("    %04x\n", sr);
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
/*
    uint32_t v1, v2;

    v1 = 0xdeadbeef;
*/
//    asm volatile("setend be\n\tstr %1, [%0]\n\t setend le"::"r"(&v2),"r"(v1));

//    printf("%08x %08x\n", v1, v2);
/*
    while (p != end) {
        uint32_t insn = *p++;
        printf("    %02x %02x %02x %02x\n", insn & 0xff, (insn >> 8) & 0xff, (insn >> 16) & 0xff, (insn >> 24) & 0xff);
    }
*/

    return 0;
}
