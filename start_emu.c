#define _GNU_SOURCE 1

#include <stdio.h>
#include <strings.h>
#include <string.h>
#include <time.h>

#include "RegisterAllocator.h"
#include "M68k.h"
#include "ARM.h"

uint8_t m68kcode[] = {
    0x7c,0x20,
    0x7e,0xff,
    0x06,0x80,0x00,0x00,0x00,0x01,
    0x06,0x80,0x00,0x00,0x00,0x01,
    0x06,0x80,0x00,0x00,0x00,0x01,
    0x06,0x80,0x00,0x00,0x00,0x01,
    0x06,0x80,0x00,0x00,0x00,0x01,
    0x06,0x80,0x00,0x00,0x00,0x01,
    0x06,0x80,0x00,0x00,0x00,0x01,
    0x06,0x80,0x00,0x00,0x00,0x01,
    0x06,0x80,0x00,0x00,0x00,0x01,
    0x06,0x80,0x00,0x00,0x00,0x01,
    0x06,0x80,0x00,0x00,0x00,0x01,
    0x06,0x80,0x00,0x00,0x00,0x01,
    0x06,0x80,0x00,0x00,0x00,0x01,
    0x06,0x80,0x00,0x00,0x00,0x01,
    0x06,0x80,0x00,0x00,0x00,0x01,
    0x06,0x80,0x00,0x00,0x00,0x01,
    0x51,0xcf,0xff,0x9e,
    0x51,0xce,0xff,0x98,
    0x4e,0x75,
    0xff,0xff

};

void *m68kcodeptr = m68kcode;

uint32_t data[128];
uint32_t stack[512];

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

    void (*arm_code)(struct M68KState *ctx);

    struct M68KTranslationUnit * unit = NULL;
    struct M68KState m68k;
    struct timespec t1, t2;

    bzero(&m68k, sizeof(m68k));
    memset(&stack, 0xaa, sizeof(stack));
    m68k.A[0].u32 = BE32((uint32_t)data);
    m68k.A[7].u32 = BE32((uint32_t)&stack[511]);
    m68k.PC = (uint16_t *)BE32((uint32_t)m68kcodeptr);

    stack[511] = 0;

    print_context(&m68k);

    printf("[JIT] Let it go...\n");

    clock_gettime(CLOCK_MONOTONIC, &t1);

    uint32_t last_PC = 0xffffffff;

    do {
        if (last_PC != (uint32_t)m68k.PC)
        {
  //      clock_gettime(CLOCK_MONOTONIC, &t1);
            unit = M68K_GetTranslationUnit((uint16_t *)(BE32((uint32_t)m68k.PC)));
            last_PC = (uint32_t)m68k.PC;
        }
  //      clock_gettime(CLOCK_MONOTONIC, &t2);

  //      printf("[JIT] Getting translation unit took %f ms\n", (double)(t2.tv_sec - t1.tv_sec) * 1000.0 + (double)(t2.tv_nsec - t1.tv_nsec)/1000000.0);
/*printf("-----\n");
        if (unit)
        {
            for (uint32_t i=0; i < unit->mt_ARMInsnCnt; i++)
            {
                uint32_t insn = unit->mt_ARMCode[i];
                printf("    %02x %02x %02x %02x\n", insn & 0xff, (insn >> 8) & 0xff, (insn >> 16) & 0xff, (insn >> 24) & 0xff);
            }
        }
*/
//return(0);
        //m68k.PC = (uint16_t *)BE32((uint32_t)unit->mt_M68kAddress);
        //m68k.SR = 0;


//        printf("\nCalling translated code\n");

        *(void**)(&arm_code) = unit->mt_ARMEntryPoint;
//        clock_gettime(CLOCK_MONOTONIC, &t1);
        arm_code(&m68k);
//        clock_gettime(CLOCK_MONOTONIC, &t2);

//        print_context(&m68k);

//        printf("[JIT] Executing translation unit took %f ms\n", (double)(t2.tv_sec - t1.tv_sec) * 1000.0 + (double)(t2.tv_nsec - t1.tv_nsec)/1000000.0);

    } while(m68k.PC != NULL);

    clock_gettime(CLOCK_MONOTONIC, &t2);

    printf("[JIT] Time in m68k mode %f ms\n", (double)(t2.tv_sec - t1.tv_sec) * 1000.0 + (double)(t2.tv_nsec - t1.tv_nsec)/1000000.0);

    printf("Back from translated code\n");
    print_context(&m68k);

    for (unsigned i=0; i < sizeof(m68kcode); i++)
        printf("%02x ", m68kcode[i]);

    for (int i=0; i < 64; i++)
    {
        if (i % 8 == 0)
            printf("\n");
        printf("%08x ", BE32(data[i]));
    }
    printf("\n");

    for (int i=0; i < 512; i++)
    {
        if (stack[511-i] != 0xaaaaaaaa)
            printf("%08x\n", stack[511-i]);
    }

    return 0;
}
