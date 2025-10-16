#ifndef _234234_POWERPC_H
#define _234234_POWERPC_H

#include <powerpc/powerpc.h>

#include "doorbell.h"

#define PPC_PRIORITY     0
#define PPC_VERSION      17
#define PPC_REVISION     15

struct PrivatePPCBase {
    struct PPCBase      pp_Public;
    struct TaskPPC *    pp_ThisPPCProc;

    /* Two doorbells used for communication - subject of change in future */
    doorbell_t          M68k_to_PPC;
    doorbell_t          PPC_to_M68k;

    /* Placeholder for NULL-Task context */
    APTR                pp_iFrame;

    /* Task waiting for a signal */
    struct Task *       pp_WaitingTask;
    UBYTE               pp_WaitingTaskBit;

};

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
    XMSG_SIGNAL_TASK = 1,
    XMSG_CAUSE
};

struct XMessage {
    enum XMsgType id;
    union {
        struct {
            struct Task*    task;
            ULONG           sigset;
        } SignalTask;
    };
};

#endif /* _234234_POWERPC_H */
