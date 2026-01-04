
#pragma pack(push,2)
#include <exec/types.h>
#include <powerpc/powerpc.h>
#pragma pack(pop)

#include "../support.h"

#define DFUNC(x) x

void L_ChangeMMU(struct PPCBase *, ULONG)
{
    /* On Emu68 ChangeMMU does notning */
    DFUNC(kprintf("[PPC] powerpc.library/ChangeMMU - NOT IMPLEMENTED, WONTFIX!\n"));
}
