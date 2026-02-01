#ifndef _234234_POWERPC_H
#define _234234_POWERPC_H

#include <exec/lists.h>
#include <powerpc/powerpc.h>

#include "doorbell.h"

#define PPC_PRIORITY     0
#define PPC_VERSION      17
#define PPC_REVISION     15

struct PrivatePPCBase {
    struct PPCBase      pp_Public;

    /* Task management */
    struct TaskPPC *    pp_ThisPPCProc;
    struct List         pp_PPCTaskReady;
    struct List         pp_PPCTaskWait;

    /* Two doorbells used for communication - subject of change in future */
    doorbell_t          M68k_to_PPC;
    doorbell_t          PPC_to_M68k;

    /* Placeholder for NULL-Task context */
    APTR                pp_iFrame;
    ULONG               pp_pvr;

    /* Task waiting for a signal */
    struct Task *       pp_WaitingTask;
    UBYTE               pp_WaitingTaskBit;

};

#define RED_ZONE_SIZE           256
#define STACK_ALLOC_SIZE        (RED_ZONE_SIZE + 32)

#define LIB_POSSIZE             sizeof(struct PrivatePPCBase)
#define NUM_OF_68K_FUNCS        49
#define NUM_OF_PPC_FUNCS        102
#define TOTAL_FUNCS             (NUM_OF_68K_FUNCS+NUM_OF_PPC_FUNCS)
#define NEGSIZE                 TOTAL_FUNCS*6
#define LIB_NEGSIZE             ((NEGSIZE + 3) & -4)

#define SUPERKEY                0xABADBEEF
#define MAGIC_COOKIE            0x07041776

#define TF_PPC                  (1 << 2)

#define STATUS_IDLE             0x69646c65
#define STATUS_MSG              0x006d7367
#define STATUS_ACK              0x0061636b

enum XMsgType {
    XMSG_SIGNAL_M68K_TASK = 1,
    XMSG_CAUSE,
    XMSG_CALL_M68K_IN_SUPERSTATE,
    XMSG_KPRINTF,
};

struct XMessage {
    enum XMsgType id;
    union {
        struct {
            struct Task*        task;
            ULONG               sigset;
        } xmSignalM68kTask;
        struct {
            struct Interrupt*   interrupt;
        } xmCause;
        struct {
            ULONG               code;
            ULONG               offset;
            ULONG               dn[8];
            ULONG               an[7];
        } xmCallM68kInSuperstate;
        struct {
            const char *        msg;
            void *              args;
        } xmKPrintF;
    };
};

#define SC_BASE     0x4F09
#define SC_CAUSE    (SC_BASE + 1)

#endif /* _234234_POWERPC_H */
