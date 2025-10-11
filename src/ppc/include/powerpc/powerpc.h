#ifndef POWERPC_POWERPC_H
#define POWERPC_POWERPC_H

#ifndef EXEC_TYPES_H
#include <exec/types.h>
#endif

#ifndef EXEC_LIBRARIES_H
#include <exec/libraries.h>
#endif

#define POWERPCNAME "powerpc.library"

struct PPCBase {
        struct Library PPC_LibNode;
        APTR PPC_SysLib;
        APTR PPC_DosLib;
        APTR PPC_SegList;
        APTR PPC_NearBase;
        UBYTE PPC_Flags;
        UBYTE PPC_DosVer;
};

/* tagitem values for GetHALInfo (V14+) */

#define HINFO_TAGS            (TAG_USER+0x103000)
#define HINFO_ALEXC_HIGH      (HINFO_TAGS+0)     /* High word of emulated alignment exceptions */
#define HINFO_ALEXC_LOW       (HINFO_TAGS+1)     /* Low word of ... */

/* tagitem values for SetScheduling (V14+) */

#define SCHED_TAGS            (TAG_USER+0x104000)
#define SCHED_REACTION        (SCHED_TAGS+0)     /* reaction of low activity tasks */

/* tagitem values for GetInfo */

#define GETINFO_TAGS    (TAG_USER+0x102000)
#define GETINFO_CPU     (GETINFO_TAGS+0)   /* CPU type (see below) */
#define GETINFO_PVR     (GETINFO_TAGS+1)   /* PVR type (see below) */
#define GETINFO_ICACHE  (GETINFO_TAGS+2)   /* Instruction cache state */
#define GETINFO_DCACHE  (GETINFO_TAGS+3)   /* Data cache state */
#define GETINFO_PAGETABLE (GETINFO_TAGS+4)   /* Page table location */
#define GETINFO_TABLESIZE (GETINFO_TAGS+5)   /* Page table size */
#define GETINFO_BUSCLOCK (GETINFO_TAGS+6)   /* PPC bus clock */
#define GETINFO_CPUCLOCK (GETINFO_TAGS+7)   /* PPC CPU clock */
#define GETINFO_CPULOAD (GETINFO_TAGS+8)   /* Total CPU usage */
#define GETINFO_SYSTEMLOAD (GETINFO_TAGS+9) /* Total system usage */


/* PPCINFO_ICACHE / PPCINFO_DCACHE */

#define CACHEB_ON_UNLOCKED      0
#define CACHEB_ON_LOCKED        1
#define CACHEB_OFF_UNLOCKED     2
#define CACHEB_OFF_LOCKED       3

#define CACHEF_ON_UNLOCKED      (1L<<0)
#define CACHEF_ON_LOCKED        (1L<<1)
#define CACHEF_OFF_UNLOCKED     (1L<<2)
#define CACHEF_OFF_LOCKED       (1L<<3)


#define CPUB_603   4
#define CPUB_603E  8
#define CPUB_604   12
#define CPUB_604E  16
#define CPUB_620   20
#define CPUB_G3    21
#define CPUB_G4    22
#define CPUB_G5    23

#define CPUF_603  (1L<<4)
#define CPUF_603E (1L<<8)
#define CPUF_604  (1L<<12)
#define CPUF_604E (1L<<16)
#define CPUF_620  (1L<<20)
#define CPUF_G3   (1L<<21)
#define CPUF_G4   (1L<<22)
#define CPUF_G5   (1L<<23)

struct PPCArgs {
        APTR  PP_Code;          /* Code Entry / Basevariable (OS Callback) */
        LONG  PP_Offset;        /* Offset into Library-Jumptable (OS Callback) */
        ULONG PP_Flags;         /* see below */
        APTR  PP_Stack;         /* Pointer to first argument to be copied or NULL */
        ULONG PP_StackSize;     /* Size of stack area to be copied or 0 */
        ULONG PP_Regs[15];      /* Registervalues to be transferred */
        DOUBLE PP_FRegs[8];     /* FPU Registervalues to be transferred */
};

/* PP_Flags */

#define PPF_ASYNC   (1L<<0) /* call PPC/68K asynchron */
#define PPF_LINEAR  (1L<<1) /* pass r3-r10/f1-f8 (V15+) */
#define PPF_THROW   (1L<<2) /* throw exception before entering function */

/* status returned by RunPPC, WaitForPPC, Run68K and WaitFor68K */

#define PPERR_SUCCESS  0 /* success */
#define PPERR_ASYNCERR 1 /* synchron call after asynchron call */
#define PPERR_WAITERR  2 /* WaitFor[PPC/68K] after synchron call */

/* Offsets into the RegisterArrays.for 68K Callbacks */
#define PPREG_D0 0
#define PPREG_D1 1
#define PPREG_D2 2
#define PPREG_D3 3
#define PPREG_D4 4
#define PPREG_D5 5
#define PPREG_D6 6
#define PPREG_D7 7

#define PPREG_A0 8
#define PPREG_A1 9
#define PPREG_A2 10
#define PPREG_A3 11
#define PPREG_A4 12
#define PPREG_A5 13
#define PPREG_A6 14

#define PPREG_FP0 0
#define PPREG_FP1 1
#define PPREG_FP2 2
#define PPREG_FP3 3
#define PPREG_FP4 4
#define PPREG_FP5 5
#define PPREG_FP6 6
#define PPREG_FP7 7

#ifndef POWERPCLIB_V7 /* use max. version 7 of powerpc.library -> */
                      /* ppc.library can be used instead of WarpKernal */
                      /* V7 is recommended for "simple" applications */

/* Cache flags (required by SetCache/SetCache68K) */

#define CACHE_DCACHEOFF    1
#define CACHE_DCACHEON     2
#define CACHE_DCACHELOCK   3
#define CACHE_DCACHEUNLOCK 4
#define CACHE_DCACHEFLUSH  5
#define CACHE_ICACHEOFF    6
#define CACHE_ICACHEON     7
#define CACHE_ICACHELOCK   8
#define CACHE_ICACHEUNLOCK 9
#define CACHE_ICACHEINV    10
#define CACHE_DCACHEINV    11

/* Hardware flags (required by SetHardware) */

#define HW_TRACEON              1             /* enable singlestep mode */
#define HW_TRACEOFF             2             /* disable singlestep mode */
#define HW_BRANCHTRACEON        3             /* enable branch trace mode */
#define HW_BRANCHTRACEOFF       4             /* disable branch trace mode */
#define HW_FPEXCON              5             /* enable FP exceptions */
#define HW_FPEXCOFF             6             /* disable FP exceptions */
#define HW_SETIBREAK            7             /* set instruction breakpoint */
#define HW_CLEARIBREAK          8             /* clear instruction breakpoint */
#define HW_SETDBREAK            9             /* set data breakpoint (604[E] only) */
#define HW_CLEARDBREAK          10            /* clear data breakpoint (604[E] only) */

/* return values of SetHardware */

#define HW_AVAILABLE      -1              /* feature available */
#define HW_NOTAVAILABLE    0              /* feature not available */

/* return values of GetPPCState */

#define PPCSTATEB_POWERSAVE     0              /* PPC is in power save mode */
#define PPCSTATEB_APPACTIVE     1              /* PPC application tasks are active */
#define PPCSTATEB_APPRUNNING    2              /* PPC application task is running */

#define PPCSTATEF_POWERSAVE     (1L<<0)
#define PPCSTATEF_APPACTIVE     (1L<<1)
#define PPCSTATEF_APPRUNNING    (1L<<2)

/* FP flags (required by ModifyFPExc) */

#define FPB_EN_OVERFLOW    0        /* enable overflow exception */
#define FPB_EN_UNDERFLOW   1        /* enable underflow exception */
#define FPB_EN_ZERODIVIDE  2        /* enable zerodivide exception */
#define FPB_EN_INEXACT     3        /* enable inexact op. exception */
#define FPB_EN_INVALID     4        /* enable invalid op. exception */
#define FPB_DIS_OVERFLOW   5        /* disable overflow exception */
#define FPB_DIS_UNDERFLOW  6        /* disable underflow exception */
#define FPB_DIS_ZERODIVIDE 7        /* disable zerodivide exception */
#define FPB_DIS_INEXACT    8        /* disable inexact op. exception */
#define FPB_DIS_INVALID    9        /* disable invalid op. exception */

#define FPF_EN_OVERFLOW    (1L<<0)
#define FPF_EN_UNDERFLOW   (1L<<1)
#define FPF_EN_ZERODIVIDE  (1L<<2)
#define FPF_EN_INEXACT     (1L<<3)
#define FPF_EN_INVALID     (1L<<4)
#define FPF_DIS_OVERFLOW   (1L<<5)
#define FPF_DIS_UNDERFLOW  (1L<<6)
#define FPF_DIS_ZERODIVIDE (1L<<7)
#define FPF_DIS_INEXACT    (1L<<8)
#define FPF_DIS_INVALID    (1L<<9)

#define FPF_ENABLEALL      $0000001f   /* enable all FP exceptions */
#define FPF_DISABLEALL     $000003e0   /* disable all FP exceptions */

/* tags passed to SetExcHandler (exception handler attributes) */

#define EXCATTR_TAGS    (TAG_USER+0x101000)
#define EXCATTR_CODE    (EXCATTR_TAGS+0)   /* exception code (required) */
#define EXCATTR_DATA    (EXCATTR_TAGS+1)   /* exception data */
#define EXCATTR_TASK    (EXCATTR_TAGS+2)   /* ppc task address (or NULL) */
#define EXCATTR_EXCID   (EXCATTR_TAGS+3)   /* exception ID */
#define EXCATTR_FLAGS   (EXCATTR_TAGS+4)   /* see below */
#define EXCATTR_NAME    (EXCATTR_TAGS+5)   /* identification name */
#define EXCATTR_PRI     (EXCATTR_TAGS+6)   /* handler priority */

/* EXCATTR_FLAGS (either EXC_GLOBAL or EXC_LOCAL, resp. */
/*                EXC_SMALLCONTEXT or EXC_LARGECONTEXT must be */
/*                specified) */

#define EXCB_GLOBAL       0        /* global handler */
#define EXCB_LOCAL        1        /* local handler */
#define EXCB_SMALLCONTEXT 2        /* small context structure */
#define EXCB_LARGECONTEXT 3        /* large context structure */
#define EXCB_ACTIVE       4        /* private */

#define EXCF_GLOBAL       (1L<<0)
#define EXCF_LOCAL        (1L<<1)
#define EXCF_SMALLCONTEXT (1L<<2)
#define EXCF_LARGECONTEXT (1L<<3)
#define EXCF_ACTIVE       (1L<<4)

/* EXCATTR_EXCID (Exception ID) */

#define EXCB_MCHECK   2              /* machine check exception */
#define EXCB_DACCESS  3              /* data access exception */
#define EXCB_IACCESS  4              /* instruction access exception */
#define EXCB_INTERRUPT 5             /* external interrupt (V15+) */
#define EXCB_ALIGN    6              /* alignment exception */
#define EXCB_PROGRAM  7              /* program exception */
#define EXCB_FPUN     8              /* FP unavailable exception */
#define EXCB_SC       12             /* system call exception */
#define EXCB_TRACE    13             /* trace exception */
#define EXCB_PERFMON  15             /* performance monitor exception */
#define EXCB_IABR     19             /* IA breakpoint exception */

#define EXCF_MCHECK   (1L<<2)
#define EXCF_DACCESS  (1L<<3)
#define EXCF_IACCESS  (1L<<4)
#define EXCF_INTERRUPT (1L<<5)
#define EXCF_ALIGN    (1L<<6)
#define EXCF_PROGRAM  (1L<<7)
#define EXCF_FPUN     (1L<<8)
#define EXCF_SC       (1L<<12)
#define EXCF_TRACE    (1L<<13)
#define EXCF_PERFMON  (1L<<15)
#define EXCF_IABR     (1L<<19)

/* large exception context structure (if EXCATTR_LARGECONTEXT was set) */

struct EXCContext {
        ULONG ec_ExcID;       /* exception ID (see above) */
        union {
                ULONG  ec_SRR0;    /* process' program counter */
                APTR   ec_PC;
        } ec_UPC;
        ULONG ec_SRR1;        /* process' context */
        ULONG ec_DAR;         /* DAR register */
        ULONG ec_DSISR;       /* DSISR register */
        ULONG ec_CR;          /* condition code register */
        ULONG ec_CTR;         /* count register */
        ULONG ec_LR;          /* link register */
        ULONG ec_XER;         /* integer exception register */
        ULONG ec_FPSCR;       /* FP status register */
        ULONG ec_GPR[32];     /* r0 - r31 */
        DOUBLE ec_FPR[32];    /* f0 - f31 */
}; /* don't depend on sizeof(EXCContext) */

/* small exception context structure (if EXCATTR_SMALLCONTEXT was set) */

struct XContext {
        ULONG xco_ExcID;      /* exception ID (see above) */
        ULONG xco_R3;         /* register r3 */
}; /* don't depend on sizeof(XCOSize) */

/* return values for exception handlers */

#define EXCRETURN_NORMAL 0       /* allow the next exc handlers to complete */
#define EXCRETURN_ABORT  1       /* exception is immediately leaved, all */
                                 /* other exception handlers are ignored */

#endif /* POWERPCLIB_V7 */

#endif
