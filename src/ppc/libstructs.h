#ifndef _LIBSTRUCTS_H
#define _LIBSTRUCTS_H

#include "config.h"

#include <exec/types.h>
#include <powerpc/powerpc.h>
#include <powerpc/tasksPPC.h>

/* Zero page shortcuts */
struct PPCZeroPage {
    ULONG               zp_PPCMemBase;
    struct ExecBase*    zp_SysBase;
    struct MemHeader*   zp_PPCMemHeader;
    volatile ULONG      zp_Status;
    struct PPCBase*     zp_PowerPCBase;         //Also used in kernel.s as 16(r0)!
    ULONG               zp_PageTableSize;
    ULONG               zp_CacheGap[2];
    ULONG               zp_MemSize;
    volatile ULONG      zp_DECCounter;
};

/* Structure to store BAT entries */
struct BATArray {
    ULONG                       ba_ibatu;
    ULONG                       ba_ibatl;
    ULONG                       ba_dbatu;
    ULONG                       ba_dbatl;
};

/* The stack frame as prepared by exception entry */
struct iframe {
    struct EXCContext           if_Context;
    ULONG                       if_AlignStore[2];
#if HAVE_ALTIVEC
    ULONG                       if_VSCR[4];
    ULONG                       if_regAltivec[32*4];
    ULONG                       if_VRSAVE;
#endif
#if HAVE_BAT
    struct BATArray             if_BATs[4];
#endif
#if HAVE_SEGMENTS
    ULONG                       if_Segments[16];
#endif
    ULONG                       if_ExcNum;
};

#endif /* _LIBSTRUCTS_H */
