#pragma pack(push,2)
#include <exec/types.h>
#include <devices/timer.h>
#include <powerpc/powerpc.h>
#pragma pack(pop)

#include "../support.h"
#include "../powerpc.h"

#define DFUNC(x) x

void L_User(struct PPCBase *, ULONG key)
{
    DFUNC(kprintf("[PPC] powerpc.library/User(%08x)\n", key));

    if (key == 0) {
        ULONG msr = getMSR();
        msr |= MSR_PR;
        setMSR(msr);
    }
}
