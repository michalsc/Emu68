#pragma pack(push,2)
#include <exec/types.h>
#include <devices/timer.h>
#include <powerpc/powerpc.h>
#pragma pack(pop)

#include "../support.h"
#include "../powerpc.h"

#define DFUNC(x) x

ULONG L_Super(struct PPCBase *)
{
    ULONG key;

    DFUNC(kprintf("[PPC] powerpc.library/Super()\n"));

    asm volatile(".globl L_Super_Addr\n\tlis 4, %1; ori 4, 4, %2;\nL_Super_Addr: mfpvr %0":
                    "=r"(key):
                    "i"(SUPERKEY >> 16), "i"(SUPERKEY & 0xffff):
                    "r4");

    return key;
}
