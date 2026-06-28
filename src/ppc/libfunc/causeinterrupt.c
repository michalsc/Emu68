#pragma pack(push,2)
#include <exec/types.h>
#include <utility/tagitem.h>
#include <powerpc/powerpc.h>
#pragma pack(pop)

#include "../support.h"
#include "../powerpc.h"

#define DFUNC(x) x

void L_CauseInterrupt(struct PPCBase *)
{
    DFUNC(kprintf("[PPC] powerpc.library/CauseInterrupt()\n"));

    SystemCall(SC_CAUSE, NULL);
}
