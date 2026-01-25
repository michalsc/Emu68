#include <exec/types.h>
#include <powerpc/warpup_macros.h>

#include "libfunc.h"
#include "support.h"

#define D(x) x

const struct {
    WORD lvo;
    APTR function;
} patch[] = {
    { -312, L_SPrintF },
    { -402, L_InsertPPC },
    { -408, L_AddHeadPPC },
    { -414, L_AddTailPPC },
    { -420, L_RemovePPC },
    { -426, L_RemHeadPPC },
    { -432, L_RemTailPPC },
    { -432, L_EnqueuePPC },
    { -444, L_FindNamePPC },
    { -450, L_FindTagItemPPC },
    { -456, L_GetTagDataPPC },
    { -462, L_NextTagItemPPC },
    { -528, L_Super },
    { -534, L_User },
    { -588, L_ChangeMMU },
    { -594, L_GetInfo },
    { -660, L_CopyMemPPC },
    { -690, L_AddTimePPC },
    { -696, L_SubTimePPC },
    { -702, L_CmpTimePPC },
    { -726, L_GetHALInfo },
    { -774, L_NewListdPPC },
    { -810, L_CauseInterrupt },

#if 0
#define Run68K(v1) 		PPCLP2	(PowerPCBase,-300,ULONG,	struct Library *,3,PowerPCBase,struct PPCArgs *,4,v1)
#define WaitFor68K(v1) 		PPCLP2	(PowerPCBase,-306,ULONG,	struct Library *,3,PowerPCBase,struct PPCArgs *,4,v1)

        /* *** memory */
#define AllocVecPPC(v1,v2,v3) 	PPCLP4	(PowerPCBase,-324,APTR,		struct Library *,3,PowerPCBase,ULONG,4,v1,ULONG,5,v2,ULONG,6,v3)
#define FreeVecPPC(v1)		PPCLP2	(PowerPCBase,-330,LONG,		struct Library *,3,PowerPCBase,APTR,4,v1)

        /* *** time measurement */
#define GetSysTimePPC(v1)	PPCLP2NR(PowerPCBase,-684,		struct Library *,3,PowerPCBase,struct timeval *,4,v1)

        /* *** more debugging */
#define SnoopTask(v1)		PPCLP2	(PowerPCBase,-714,ULONG,	struct Library *,3,PowerPCBase,struct TagItem *,4,v1)
#define EndSnoopTask(v1)	PPCLP2NR(PowerPCBase,-720,		struct Library *,3,PowerPCBase,ULONG,4,v1)

        /* *** more memory */
#define FreeAllMem()		PPCLP1NR(PowerPCBase,-654,		struct Library *,3,PowerPCBase)
#define CreatePoolPPC(v1,v2,v3)	PPCLP4	(PowerPCBase,-816,void *,	struct Library *,3,PowerPCBase,ULONG,4,v1,ULONG,5,v2,ULONG,6,v3)
#define DeletePoolPPC(v1)	PPCLP2NR(PowerPCBase,-822,		struct Library *,3,PowerPCBase,void *,4,v1)
#define AllocPooledPPC(v1,v2)	PPCLP3	(PowerPCBase,-828,void *,	struct Library *,3,PowerPCBase,void *,4,v1,ULONG,5,v2)
#define FreePooledPPC(v1,v2,v3)	PPCLP4NR(PowerPCBase,-834,		struct Library *,3,PowerPCBase,void *,4,v1,void *,5,v2,ULONG,6,v3)

        /* *** semaphores */
#define AddSemaphorePPC(v1)	PPCLP2NR(PowerPCBase,-366,		struct Library *,3,PowerPCBase,struct SignalSemaphorePPC *,4,v1)
#define AttemptSemaphorePPC(v1)	PPCLP2	(PowerPCBase,-384,LONG,		struct Library *,3,PowerPCBase,struct SignalSemaphorePPC *,4,v1)
#define FindSemaphorePPC(v1)	PPCLP2	(PowerPCBase,-396,struct SignalSemaphorePPC *,struct Library *,3,PowerPCBase,STRPTR,4,v1)
#define FreeSemaphorePPC(v1)	PPCLP2NR(PowerPCBase,-360,		struct Library *,3,PowerPCBase,struct SignalSemaphorePPC *,4,v1)
#define InitSemaphorePPC(v1)	PPCLP2	(PowerPCBase,-354,LONG,		struct Library *,3,PowerPCBase,struct SignalSemaphorePPC *,4,v1)
#define ObtainSemaphorePPC(v1)	PPCLP2NR(PowerPCBase,-378,		struct Library *,3,PowerPCBase,struct SignalSemaphorePPC *,4,v1)
#define	ReleaseSemaphorePPC(v1)	PPCLP2NR(PowerPCBase,-390,		struct Library *,3,PowerPCBase,struct SignalSemaphorePPC *,4,v1)
#define RemSemaphorePPC(v1)	PPCLP2NR(PowerPCBase,-372,		struct Library *,3,PowerPCBase,struct SignalSemaphorePPC *,4,v1)
#define TrySemaphorePPC(v1,v2)	PPCLP3	(PowerPCBase,-750,LONG,		struct Library *,3,PowerPCBase,struct SignalSemaphorePPC *,4,v1,ULONG,5,v2)
#define ObtainSemaphoreSharedPPC(v1) PPCLP2NR(PowerPCBase,-786,		struct Library *,3,PowerPCBase,struct SignalSemaphorePPC *,4,v1)
#define AttemptSemaphoreSharedPPC(v1) PPCLP2(PowerPCBase,-792,LONG,	struct Library *,3,PowerPCBase,struct SignalSemaphorePPC *,4,v1)
#define ProcurePPC(v1,v2)	PPCLP3NR(PowerPCBase,-798,		struct Library *,3,PowerPCBase,struct SignalSemaphorePPC *,4,v1,struct SemaphoreMessage *,5,v2)
#define VacatePPC(v1,v2)	PPCLP3NR(PowerPCBase,-804,		struct Library *,3,PowerPCBase,struct SignalSemaphorePPC *,4,v1,struct SemaphoreMessage *,5,v2)
#define AddUniqueSemaphorePPC(v1)  PPCLP2(PowerPCBase,-858,LONG,		struct Library *,3,PowerPCBase,struct SignalSemaphorePPC *,4,v1)	

        /* *** signals */
#define AllocSignalPPC(v1)	PPCLP2	(PowerPCBase,-468,LONG,		struct Library *,3,PowerPCBase,LONG,4,v1)
#define FreeSignalPPC(v1)	PPCLP2NR(PowerPCBase,-474,		struct Library *,3,PowerPCBase,LONG,4,v1)
#define SetSignalPPC(v1,v2)	PPCLP3	(PowerPCBase,-480,ULONG,	struct Library *,3,PowerPCBase,ULONG,4,v1,ULONG,5,v2)
#define SignalPPC(v1,v2)	PPCLP3NR(PowerPCBase,-486,		struct Library *,3,PowerPCBase,struct TaskPPC *,4,v1,ULONG,5,v2)
#define WaitPPC(v1)		PPCLP2	(PowerPCBase,-492,ULONG,	struct Library *,3,PowerPCBase,ULONG,4,v1)
#define WaitTime(v1,v2)		PPCLP3	(PowerPCBase,-552,ULONG,	struct Library *,3,PowerPCBase,ULONG,4,v1,ULONG,5,v2)
#define SetExceptPPC(v1,v2,v3)	PPCLP4	(PowerPCBase,-780,ULONG,	struct Library *,3,PowerPCBase,ULONG,4,v1,ULONG,5,v2,ULONG,6,v3)

        /* *** tasks */
#define CreateTaskPPC(v1)	PPCLP2	(PowerPCBase,-336,struct TaskPPC *,struct Library *,3,PowerPCBase,struct TagItem *,4,v1)
#define DeleteTaskPPC(v1)	PPCLP2NR(PowerPCBase,-342,		struct Library *,3,PowerPCBase,struct TaskPPC *,4,v1)
#define FindTaskPPC(v1)		PPCLP2	(PowerPCBase,-348,struct TaskPPC *,struct Library *,3,PowerPCBase,STRPTR,4,v1)
#define LockTaskList()		PPCLP1	(PowerPCBase,-564,struct TaskPtr *,struct Library *,3,PowerPCBase)
#define SetTaskPriPPC(v1,v2)	PPCLP3	(PowerPCBase,-498,LONG,		struct Library *,3,PowerPCBase,struct TaskPPC *,4,v1,LONG,5,v2)
#define UnLockTaskList()	PPCLP1NR(PowerPCBase,-570,		struct Library *,3,PowerPCBase)
#define FindTaskByID(v1)	PPCLP2	(PowerPCBase,-738,struct TaskPPC *,struct Library *,3,PowerPCBase,LONG,4,v1)
#define SetNiceValue(v1,v2)	PPCLP3	(PowerPCBase,-744,LONG,		struct Library *,3,PowerPCBase,struct TaskPPC *,4,v1,LONG,5,v2)

        /* *** ports */
#define AddPortPPC(v1)		PPCLP2NR(PowerPCBase,-612,		struct Library *,3,PowerPCBase,struct MsgPortPPC *,4,v1)
#define CreateMsgPortPPC()	PPCLP1	(PowerPCBase,-600,struct MsgPortPPC *,struct Library *,3,PowerPCBase)
#define DeleteMsgPortPPC(v1)	PPCLP2NR(PowerPCBase,-606,		struct Library *,3,PowerPCBase,struct MsgPortPPC *,4,v1)
#define FindPortPPC(v1)		PPCLP2	(PowerPCBase,-624,struct MsgPortPPC *,struct Library *,3,PowerPCBase,STRPTR,4,v1)
#define GetMsgPPC(v1)		PPCLP2	(PowerPCBase,-642,struct Message *,struct Library *,3,PowerPCBase,struct MsgPortPPC *,4,v1)
#define PutMsgPPC(v1,v2)	PPCLP3NR(PowerPCBase,-636,		struct Library *,3,PowerPCBase,struct MsgPortPPC *,4,v1,struct Message *,5,v2)
#define WaitPortPPC(v1)		PPCLP2	(PowerPCBase,-630,struct Message *,struct Library *,3,PowerPCBase,struct MsgPortPPC *,4,v1)
#define RemPortPPC(v1)		PPCLP2NR(PowerPCBase,-618,		struct Library *,3,PowerPCBase,struct MsgPortPPC *,4,v1)
#define ReplyMsgPPC(v1)		PPCLP2NR(PowerPCBase,-648,		struct Library *,3,PowerPCBase,struct Message *,4,v1)
#define AllocXMsgPPC(v1,v2)	PPCLP3	(PowerPCBase,-666,struct Message *,struct Library *,3,PowerPCBase,ULONG,4,v1,struct MsgPortPPC *,5,v2)
#define FreeXMsgPPC(v1)		PPCLP2NR(PowerPCBase,-672,		struct Library *,3,PowerPCBase,struct Message *,4,v1)
#define PutXMsgPPC(v1,v2)	PPCLP3NR(PowerPCBase,-678,		struct Library *,3,PowerPCBase,struct MsgPort *,4,v1,struct Message *,5,v2)
#define SetReplyPortPPC(v1,v2)	PPCLP3	(PowerPCBase,-708,struct MsgPortPPC *,struct Library *,3,PowerPCBase,struct Message *,4,v1,struct MsgPortPPC *,5,v2)
#define PutPublicMsgPPC(v1,v2)  PPCLP3  (PowerPCBase,-846,LONG,struct Library *,3,PowerPCBase,STRPTR,4,v1,struct Message *,5,v2)
#define AddUniquePortPPC(v1)	PPCLP2  (PowerPCBase,-852,LONG,struct Library *,3,PowerPCBase,struct MsgPortPPC *,4,v1)

        /* *** hardware */
#define SetCache(v1,v2,v3)	PPCLP4NR(PowerPCBase,-510,		struct Library *,3,PowerPCBase,ULONG,4,v1,APTR,5,v2,ULONG,6,v3)
#define SetHardware(v1,v2)	PPCLP3	(PowerPCBase,-540,ULONG,	struct Library *,3,PowerPCBase,ULONG,4,v1,APTR,5,v2)
#define	SetScheduling(v1)	PPCLP2NR(PowerPCBase,-732,		struct Library *,3,PowerPCBase,struct TagItem *,4,v1)

        /* *** exceptions */
#define ModifyFPExc(v1)		PPCLP2NR(PowerPCBase,-546,		struct Library *,3,PowerPCBase,ULONG,4,v1)
#define RemExcHandler(v1)	PPCLP2NR(PowerPCBase,-522,		struct Library *,3,PowerPCBase,APTR,4,v1)
#define	SetExcHandler(v1)	PPCLP2	(PowerPCBase,-516,APTR,		struct Library *,3,PowerPCBase,struct TagItem *,4,v1)
#define	SetExcMMU()		PPCLP1NR(PowerPCBase,-576,		struct Library *,3,PowerPCBase)
#define	ClearExcMMU()		PPCLP1NR(PowerPCBase,-582,		struct Library *,3,PowerPCBase)
#define IsExceptionMode()	PPCLP1(PowerPCBase,-864,BOOL,struct Library *,3,PowerPCBase)

        /* *** 68K connection */
#define Signal68K(v1,v2)	PPCLP3NR(PowerPCBase,-504,		struct Library *,3,PowerPCBase,struct Task *,4,v1,ULONG,5,v2)
#define RawDoFmtPPC(v1,v2,v3,v4) PPCLP5(PowerPCBase,-840,APTR,		struct Library *,3,PowerPCBase,STRPTR,4,v1,APTR,5,v2,void (*)(void),6,v3,APTR,7,v4)	

#endif


    { 0, NULL }
};

void PatchLVOTable(struct PPCBase *ppcbase)
{
    ULONG lvo = (ULONG)ppcbase;
    ULONG pos;

    D(kprintf("[PPC] Patching PowerPC Library base\n"));

    for (pos = 0; patch[pos].lvo; pos++) {
        *(APTR *)(lvo + patch[pos].lvo + 2) = patch[pos].function;
    }

    sync();

    D(kprintf("[PPC] Patched %d functions\n", pos));
}
