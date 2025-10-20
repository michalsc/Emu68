#ifndef POWERPC_TASKSPPC_H
#define POWERPC_TASKSPPC_H

#ifndef POWERPC_PORTSPPC_H
#include <powerpc/portsPPC.h>
#endif

#ifndef EXEC_TASKS_H
#include <exec/tasks.h>
#endif

#ifndef UTILITY_TAGITEM_H
#include <utility/tagitem.h>
#endif

/* private structure */

struct TaskLink {
        struct MinNode tl_Node;
        APTR tl_Task;
        ULONG tl_Sig;
        UWORD tl_Used;
};

/* task structure for ppc. fields not commented are private*/

struct TaskPPC {
        struct Task tp_Task;                    /* exec task structure */
        ULONG tp_StackSize;                     /* stack size: read only */
        APTR tp_StackMem;
        APTR tp_ContextMem;
        APTR tp_TaskPtr;
        ULONG tp_Flags;                         /* flags (see below): read only */
        struct TaskLink tp_Link;
        APTR tp_BATStorage;
        ULONG tp_Core;
        struct MinNode tp_TableLink;
        APTR tp_Table;                          /* task's page table: read only */
        ULONG tp_DebugData;                     /* free space for debuggers */

        UWORD tp_Pad;
        ULONG tp_Timestamp;
        ULONG tp_Timestamp2;
        ULONG tp_Elapsed;
        ULONG tp_Elapsed2;
        ULONG tp_Totalelapsed;
        ULONG tp_Quantum;
        ULONG tp_Priority;
        ULONG tp_Prioffset;
        APTR tp_PowerPCBase;
        ULONG tp_Desired;
        ULONG tp_CPUusage;                      /* CPU usage: read only */
        ULONG tp_Busy;                          /* busy time: read only */
        ULONG tp_Activity;                      /* activity: read only */
        ULONG tp_Id;                            /* task ID: read only */
        ULONG tp_Nice;                          /* NICE value: read only */
        struct MsgPortPPC* tp_Msgport;          /* Msg port: read only */
        struct List tp_TaskPools;               /* private: for V15-MM */
        ULONG tp_PoolMem;                       /* private: for V15-MM */
        struct Message* tp_MessageRIP;          /* private */

}; /* don't depend on sizeof(TaskPPC) */

#define NT_PPCTASK 100

/* tc_State (additional task states) */

#define TS_CHANGING      7

/* tp_Flags */

#define TASKPPCB_SYSTEM    0
#define TASKPPCB_BAT       1
#define TASKPPCB_THROW     2
#define TASKPPCB_CHOWN     3
#define TASKPPCB_ATOMIC    4

#define TASKPPCF_SYSTEM    (1L<<0)
#define TASKPPCF_BAT       (1L<<1)
#define TASKPPCF_THROW     (1L<<2)
#define TASKPPCF_CHOWN     (1L<<3)
#define TASKPPCF_ATOMIC    (1L<<4)

/* tags passed to CreateTaskPPC */

#define TASKATTR_TAGS       (TAG_USER+0x100000)
#define TASKATTR_CODE       (TASKATTR_TAGS+0)   /* entry code */
#define TASKATTR_EXITCODE   (TASKATTR_TAGS+1)   /* exit code */
#define TASKATTR_NAME       (TASKATTR_TAGS+2)   /* task name */
#define TASKATTR_PRI        (TASKATTR_TAGS+3)   /* task priority */
#define TASKATTR_STACKSIZE  (TASKATTR_TAGS+4)   /* task stacksize */
#define TASKATTR_R2         (TASKATTR_TAGS+5)   /* smalldata/TOC base */
#define TASKATTR_R3         (TASKATTR_TAGS+6)   /* first parameter */
#define TASKATTR_R4         (TASKATTR_TAGS+7)
#define TASKATTR_R5         (TASKATTR_TAGS+8)
#define TASKATTR_R6         (TASKATTR_TAGS+9)
#define TASKATTR_R7         (TASKATTR_TAGS+10)
#define TASKATTR_R8         (TASKATTR_TAGS+11)
#define TASKATTR_R9         (TASKATTR_TAGS+12)
#define TASKATTR_R10        (TASKATTR_TAGS+13)
#define TASKATTR_SYSTEM     (TASKATTR_TAGS+14)  /* private */
#define TASKATTR_MOTHERPRI  (TASKATTR_TAGS+15)  /* inherit mothers pri */
#define TASKATTR_BAT        (TASKATTR_TAGS+16)  /* BAT MMU setup (BOOL) */
#define TASKATTR_NICE       (TASKATTR_TAGS+18)  /* initial NICE value (-20..20)*/
#define TASKATTR_INHERITR2  (TASKATTR_TAGS+19)  /* inherit r2 from parent task
                                                   (overrides TASKATTR_R2) (V15+) */
#define TASKATTR_ATOMIC     (TASKATTR_TAGS+20) /* noninterruptable task */
#define TASKATTR_NOTIFYMSG  (TASKATTR_TAGS+21) /* notification upon task death (V16+) */

/* taskptr structure */

struct TaskPtr {
        struct Node tptr_Node;
        APTR tptr_Task;
};

/* return values of ChangeStack */

#define CHSTACK_SUCCESS  -1
#define CHSTACK_NOMEM    0

/* parameter to ChangeMMU */

#define CHMMU_STANDARD   1
#define CHMMU_BAT        2

/* tags passed to SnoopTask */

#define SNOOP_TAGS          (TAG_USER+0x103000)
#define SNOOP_CODE          (SNOOP_TAGS+0)      /* pointer to callback function */
#define SNOOP_DATA          (SNOOP_TAGS+1)      /* custom data, passed in r2 */
#define SNOOP_TYPE          (SNOOP_TAGS+2)      /* snoop type (see below) */

/* possible values of SNOOP_TYPE */

#define SNOOP_START     1                       /*monitor task start */
#define SNOOP_EXIT      2                       /*monitor task exit */

/* possible values for the CreatorCPU parameter of the callback function */

#define CREATOR_PPC     1
#define CREATOR_68K     2

#endif
