#ifndef POWERPC_SEMAPHORESPPC_H
#define POWERPC_SEMAPHORESPPC_H

#ifndef EXEC_SEMAPHORES_H
#include <exec/semaphores.h>
#endif

/* SignalSemaphorePPC structure used by PPC semaphore functions */

struct SignalSemaphorePPC {
        struct SignalSemaphore ssppc_SS;
        APTR ssppc_reserved;                    /* private */
        UWORD ssppc_lock;                       /* private */
};

/* return value from InitSemaphore and AddSemaphore */

#define SSPPC_SUCCESS     -1
#define SSPPC_NOMEM       0

/* return values of AttemptSemaphore */

#define ATTEMPT_SUCCESS   -1
#define ATTEMPT_FAILURE   0

/* status returned by AddUniqueSemaphorePPC */

#define UNISEM_SUCCESS    -1
#define UNISEM_NOTUNIQUE  0

#endif
