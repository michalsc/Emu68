#pragma pack(push,2)
#include <exec/types.h>
#include <devices/timer.h>
#include <powerpc/powerpc.h>
#pragma pack(pop)

void L_AddTimePPC(struct PowerPCBase *, struct timeval *dest, struct timeval *source)
{
    dest->tv_micro += source->tv_micro;
    dest->tv_secs += source->tv_secs;
    if (dest->tv_micro >= 1000000) {
        dest->tv_secs++;
        dest->tv_micro -= 1000000;
    }
}
