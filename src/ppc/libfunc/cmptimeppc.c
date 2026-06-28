#pragma pack(push,2)
#include <exec/types.h>
#include <devices/timer.h>
#include <powerpc/powerpc.h>
#pragma pack(pop)

#include "../support.h"

#define DFUNC(x) x

LONG L_CmpTimePPC(struct PPCBase *, struct timeval *dest, struct timeval *source)
{
    DFUNC(kprintf("[PPC] powerpc.library/CmpTimePPC(%08x, %08x)\n", dest, source));

    if (dest->tv_micro == source->tv_micro && dest->tv_secs == source->tv_secs)
        return 0;
    
    if (dest->tv_secs > source->tv_secs) {
        return -1;
    }
    else if (dest->tv_secs == source->tv_secs && dest->tv_micro > source->tv_micro) {
        return -1;
    }
    else return 1;
}
