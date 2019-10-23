/*
    Copyright © 2019 Michal Schulz <michal.schulz@gmx.de>
    https://github.com/michalsc

    This Source Code Form is subject to the terms of the
    Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed
    with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#define _GNU_SOURCE 1

#include <stdio.h>
#include <strings.h>
#include <string.h>
#include <time.h>

#include "HunkLoader.h"
#include "RegisterAllocator.h"
#include "M68k.h"
#include "ARM.h"

uint8_t m68kcode[] = {
/*
    0x7c, 0x20,
    0x7e, 0xff,
    0x52, 0x80, 0x52, 0x80, 0x52, 0x80,
    0x52, 0x80, 0x52, 0x80, 0x52, 0x80,
    0x52, 0x80, 0x52, 0x80, 0x52, 0x80,
    0x52, 0x80, 0x52, 0x80, 0x52, 0x80,
    0x52, 0x80, 0x52, 0x80, 0x52, 0x80,
    0x52, 0x80, 0x52, 0x80, 0x52, 0x80,
    0x52, 0x80, 0x52, 0x80, 0x52, 0x80,
    0x52, 0x80, 0x52, 0x80, 0x52, 0x80,
    0x52, 0x80, 0x52, 0x80, 0x52, 0x80,
    0x52, 0x80, 0x52, 0x80, 0x52, 0x80,
    0x52, 0x80, 0x52, 0x80, 0x52, 0x80,
    0x52, 0x80, 0x52, 0x80, 0x52, 0x80,
    0x52, 0x80, 0x52, 0x80, 0x52, 0x80,
    0x52, 0x80, 0x52, 0x80, 0x52, 0x80,
    0x52, 0x80, 0x52, 0x80, 0x52, 0x80,
    0x52, 0x80, 0x52, 0x80, 0x52, 0x80,
    0x51, 0xcf, 0xff, 0x9e,
    0x51, 0xce, 0xff, 0x98,
    0x4e, 0x75,
    0xff, 0xff
*/
    /*0x74,0x08,
    0x20,0x3c,0xaa,0x55,0xaa,0x55,
    0x22,0x3c,0x87,0x65,0x43,0x21,
    0xe5,0xb8,
    0xe4,0xb9,
    0x4e,0x75,*/
    /*
    0x20,0x3c,0xde,0xad,0xbe,0xef,
    0x22,0x3c,0x12,0x34,0x56,0x78,
    0x34,0x3c,0xaa,0x55,
    0x16,0x3c,0x00,0xc7,
    0x4e,0x75,
*/
    0x2a,0x49,  // movea.l  a1,a5
    0x2c,0x4a,  // movea.l  a2,a6
    0x22,0x5e,  //           	movea.l (a6)+,a1
    0x24,0x5e,  //           	movea.l (a6)+,a2
    0x26,0x5e,  //           	movea.l (a6)+,a3
    0x28,0x56,  //           	movea.l (a6),a4
    0x2c,0x3c,0x55,0x55 ,0x55,0x55, // 	move.l #1431655765,d6
    0x2e,0x3c ,0x33,0x33,0x33,0x33, // 	move.l #858993459,d7
    0x28,0x3c ,0x0f,0x0f ,0x0f,0x0f,// 	loop: move.l #252645135,d4 
    0x2a,0x3c ,0x00,0xff ,0x00,0xff, //	move.l #16711935,d5
    0x20,0x18, // move.l (a0)+, d0
    0xc0,0x84, //           	and.l d4,d0
    0x22,0x18, // 	move.l (a0)+,d1
    0xe9,0x88, //           	lsl.l #4,d0
    0xc2,0x84, //           	and.l d4,d1
    0x80,0x81, //           	or.l d1,d0
    0x22,0x18, // 	move.l (a0)+,d1
    0xc2,0x84, //           	and.l d4,d1
    0x24,0x18, // 	move.l (a0)+,d2
    0xe9,0x89, //           	lsl.l #4,d1
    0xc4,0x84, //           	and.l d4,d2
    0x82,0x82, //           	or.l d2,d1
    0x24,0x00, //           	move.l d0,d2
    0x26,0x01, //           	move.l d1,d3
    0xc0,0x85, //           	and.l d5,d0
    0xc6,0x85, //           	and.l d5,d3
    0xb1,0x82, //           	eor.l d0,d2
    0xb7,0x81, //           	eor.l d3,d1
    0xe1,0x88, //           	lsl.l #8,d0
    0xe0,0x89, //           	lsr.l #8,d1
    0x80,0x83, //           	or.l d3,d0
    0x82,0x82, //           	or.l d2,d1
    0x24,0x00, //           	move.l d0,d2
    0x26,0x01, //           	move.l d1,d3
    0xc0,0x86, //           	and.l d6,d0
    0xc6,0x86, //           	and.l d6,d3
    0xb1,0x82, //           	eor.l d0,d2
    0xb7, 0x81,//           	eor.l d3,d1
    0xd6,0x83, //           	add.l d3,d3
    0xe2,0x8a, //           	lsr.l #1,d2
    0x80,0x83, //           	or.l d3,d0
    0x82,0x82, //           	or.l d2,d1
    0x24,0x18, // 	move.l (a0)+,d2
    0xc4,0x84, //           	and.l d4,d2
    0x26,0x18, // 	move.l (a0)+,d3
    0xe9,0x8a, //           	lsl.l #4,d2
    0xc6,0x84, //           	and.l d4,d3
    0x84,0x83, //           	or.l d3,d2
    0x26,0x18, // 	move.l (a0)+,d3
    0xc6,0x84, //           	and.l d4,d3
    0xc8,0x98, // 	move.l (a0)+,d4
    0xe9,0x8b, //           	lsl.l #4,d3
    0x86,0x84, //           	or.l d4,d3
    0x28,0x02, //           	move.l d2,d4
    0xc4,0x85, //           	and.l d5,d2
    0xca,0x83, //           	and.l d3,d5
    0xb5,0x84, //           	eor.l d2,d4
    0xbb,0x83, //           	eor.l d5,d3
    0xe1,0x8a, //           	lsl.l #8,d2
    0xe0,0x8b, //           	lsr.l #8,d3
    0x84,0x85, //           	or.l d5,d2
    0x86,0x84, //           	or.l d4,d3
    0x28,0x02, //           	move.l d2,d4
    0x2a,0x03, //           	move.l d3,d5
    0xc4,0x86, //           	and.l d6,d2
    0xca,0x86, //           	and.l d6,d5
    0xb5,0x84, //           	eor.l d2,d4
    0xbb,0x83, //           	eor.l d5,d3
    0xda,0x85, //           	add.l d5,d5
    0xe2,0x8c, //           	lsr.l #1,d4
    0x84,0x85, //           	or.l d5,d2
    0x86,0x84, //           	or.l d4,d3
    0x48,0x42, //           	swap d2
    0x48,0x43, //           	swap d3
    0xb1,0x42, //           	eor.w d0,d2
    0xb3,0x43, //           	eor.w d1,d3
    0xb5,0x40, //           	eor.w d2,d0
    0xb7,0x41, //           	eor.w d3,d1
    0xb1,0x42, //           	eor.w d0,d2
    0xb3,0x43, //           	eor.w d1,d3
    0x48,0x42, //           	swap d2
    0x48,0x43, //           	swap d3
    0x28,0x00, //           	move.l d0,d4
    0x2a,0x02, //           	move.l d2,d5
    0xc0,0x87, //           	and.l d7,d0
    0xca,0x87, //           	and.l d7,d5
    0xb1,0x84, //           	eor.l d0,d4
    0xbb,0x82, //           	eor.l d5,d2
    0xe5,0x88, //           	lsl.l #2,d0
    0xe4,0x8a, //           	lsr.l #2,d2
    0x80,0x85, //           	or.l d5,d0
    0x22,0xc0, //           	move.l d0,(a1)+
    0x84,0x84, //           	or.l d4,d2
    0x26,0xc2, //           	move.l d2,(a3)+
    0x28,0x01, //           	move.l d1,d4
    0x2a,0x03, //           	move.l d3,d5
    0xc2,0x87, //           	and.l d7,d1
    0xca,0x87, //           	and.l d7,d5
    0xb3,0x84, //           	eor.l d1,d4
    0xbb,0x83, //           	eor.l d5,d3
    0xe5,0x89, //           	lsl.l #2,d1
    0xe4,0x8b, //           	lsr.l #2,d3
    0x82,0x85, //           	or.l d5,d1
    0x24,0xc1, //           	move.l d1,(a2)+
    0x86,0x84, //           	or.l d4,d3
    0x28,0xc3, //           	move.l d3,(a4)+
    0xb1,0xcd, //       cmpa.l a5,a0
    0x6d,0x00,0xff,0x30, // blt.w loop
    0x4e,0x75,  
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

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>

char out_data[1024];

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    void *hunk = (void*)0xfffffffc;

    if (argc > 1)
    {
        struct stat64 s;
        printf("opening file %s\n", argv[1]);
        int fd = open(argv[1], O_RDONLY);
        fstat64(fd, &s);
        printf("fd=%d, size=%d\n", fd, (int)s.st_size);
        void *buff = malloc(s.st_size);
        read(fd, buff, s.st_size);
        close(fd);

        hunk = LoadHunkFile(buff);
    }

    M68K_InitializeCache();

    void (*arm_code)(struct M68KState *ctx);

    struct M68KTranslationUnit * unit = NULL;
    struct M68KState m68k;
    struct timespec t1, t2;
    struct timespec t3, t4;

    double translation_time = 0.0;

    bzero(&m68k, sizeof(m68k));
    memset(&stack, 0xaa, sizeof(stack));
    m68k.A[0].u32 = BE32((uint32_t)out_data);
    m68k.A[7].u32 = BE32((uint32_t)&stack[511]);
    m68k.PC = (uint16_t *)BE32((uint32_t)hunk + 4);

    stack[511] = 0;

    print_context(&m68k);

    printf("[JIT] Let it go...\n");

    clock_gettime(CLOCK_MONOTONIC, &t1);

    uint32_t last_PC = 0xffffffff;

    do {
        if (last_PC != (uint32_t)m68k.PC)
        {
            clock_gettime(CLOCK_MONOTONIC, &t3);
            unit = M68K_GetTranslationUnit((uint16_t *)(BE32((uint32_t)m68k.PC)));
            clock_gettime(CLOCK_MONOTONIC, &t4);
            translation_time += (double)(t4.tv_sec - t3.tv_sec) * 1000.0 + (double)(t4.tv_nsec - t3.tv_nsec)/1000000.0;
            last_PC = (uint32_t)m68k.PC;
        }

        *(void**)(&arm_code) = unit->mt_ARMEntryPoint;
        unit->mt_UseCount++;
        arm_code(&m68k);

    } while(m68k.PC != NULL);

    clock_gettime(CLOCK_MONOTONIC, &t2);

    printf("[JIT] Time in m68k mode %f ms\n", (double)(t2.tv_sec - t1.tv_sec) * 1000.0 + (double)(t2.tv_nsec - t1.tv_nsec)/1000000.0);
    printf("[JIT] Time spent translating m68k mode %f ms\n", translation_time);

    printf("%s", out_data);

    printf("Back from translated code\n");
    print_context(&m68k);

    return 0;
}
