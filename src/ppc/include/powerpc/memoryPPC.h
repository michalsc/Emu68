#ifndef POWERPC_MEMORYPPC_H
#define POWERPC_MEMORYPPC_H

/* additional memory flags for AllocVecPPC */

#ifndef EXEC_MEMORY_H
#include <exec/memory.h>
#endif

#define MEMB_WRITETHROUGH 20
#define MEMB_COPYBACK     21
#define MEMB_CACHEON      22
#define MEMB_CACHEOFF     23
#define MEMB_GUARDED      24
#define MEMB_NOTGUARDED   25
#define MEMB_BAT          26
#define MEMB_PROTECT      27
#define MEMB_WRITEPROTECT 28

#define MEMF_WRITETHROUGH (1L<<20)
#define MEMF_COPYBACK     (1L<<21)
#define MEMF_CACHEON      (1L<<22)
#define MEMF_CACHEOFF     (1l<<23)
#define MEMF_GUARDED      (1L<<24)
#define MEMF_NOTGUARDED   (1L<<25)
#define MEMF_BAT          (1L<<26)
#define MEMF_PROTECT      (1L<<27)
#define MEMF_WRITEPROTECT (1L<<28)

/* status returned by FreeVecPPC */

#define MEMERR_SUCCESS   0

#endif
