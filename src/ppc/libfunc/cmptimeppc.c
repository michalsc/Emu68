#pragma pack(push,2)
#include <exec/types.h>
#include <devices/timer.h>
#include <powerpc/powerpc.h>
#pragma pack(pop)

LONG L_CmpTimePPC(struct PowerPCBase *, struct timeval *dest, struct timeval *source)
{
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
