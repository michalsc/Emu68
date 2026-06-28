#ifndef _DEVICES_TIMER_H
#define _DEVICES_TIMER_H

#include <exec/types.h>

struct timeval {
    ULONG tv_secs;
    ULONG tv_micro;
};

#endif /* _DEVICES_TIMER_H */
