#ifndef WARPUP_GCCLIB_PROTOS_H
#define WARPUP_GCCLIB_PROTOS_H

/*
**  $VER: waprup_protos.h 2.0 (15.03.98)
**  WarpOS Release 14.1
**
**  '(C) Copyright 1998 Haage & Partner Computer GmbH'
**       All Rights Reserved
*/


#ifndef UTILITY_TAGITEM_H
#include <utility/tagitem.h>
#endif

#ifndef DEVICES_TIMER_H
#include <devices/timer.h>
#endif

#ifndef POWERPC_PORTSPPC_H
#include <powerpc/portsPPC.h>
#endif

#ifndef POWERPC_TASKS_H
#include <powerpc/tasksPPC.h>
#endif

#ifndef POWERPC_SEMAPHORES_H
#include <powerpc/semaphoresPPC.h>
#endif

extern struct Library *PowerPCBase;

#include <powerpc/warpup_macros.h>

        /* *** call 68K */
#define Run68K(v1) 		PPCLP2	(PowerPCBase,-300,ULONG,	struct Library *,3,PowerPCBase,struct PPCArgs *,4,v1)
#define WaitFor68K(v1) 		PPCLP2	(PowerPCBase,-306,ULONG,	struct Library *,3,PowerPCBase,struct PPCArgs *,4,v1)

        /* *** debugging */
#define SPrintF(v1,v2) 		PPCLP3NR(PowerPCBase,-312,		struct Library *,3,PowerPCBase,STRPTR,4,v1,APTR,5,v2)

        /* *** memory */
#define AllocVecPPC(v1,v2,v3) 	PPCLP4	(PowerPCBase,-324,APTR,		struct Library *,3,PowerPCBase,ULONG,4,v1,ULONG,5,v2,ULONG,6,v3)
#define FreeVecPPC(v1)		PPCLP2	(PowerPCBase,-330,LONG,		struct Library *,3,PowerPCBase,APTR,4,v1)

        /* *** time measurement */
#define GetSysTimePPC(v1)	PPCLP2NR(PowerPCBase,-684,		struct Library *,3,PowerPCBase,struct timeval *,4,v1)
#define AddTimePPC(v1,v2)	PPCLP3NR(PowerPCBase,-690,		struct Library *,3,PowerPCBase,struct timeval *,4,v1,struct timeval *,5,v2)
#define SubTimePPC(v1,v2)	PPCLP3NR(PowerPCBase,-696,		struct Library *,3,PowerPCBase,struct timeval *,4,v1,struct timeval *,5,v2)
#define CmpTimePPC(v1,v2)	PPCLP3	(PowerPCBase,-702,LONG,		struct Library *,3,PowerPCBase,struct timeval *,4,v1,struct timeval *,5,v2)

        /* *** more debugging */
#define SnoopTask(v1)		PPCLP2	(PowerPCBase,-714,ULONG,	struct Library *,3,PowerPCBase,struct TagItem *,4,v1)
#define EndSnoopTask(v1)	PPCLP2NR(PowerPCBase,-720,		struct Library *,3,PowerPCBase,ULONG,4,v1)

        /* *** more memory */
#define FreeAllMem()		PPCLP1NR(PowerPCBase,-654,		struct Library *,3,PowerPCBase)
#define CopyMemPPC(v1,v2,v3)	PPCLP4NR(PowerPCBase,-660,		struct Library *,3,PowerPCBase,APTR,4,v1,APTR,5,v2,ULONG,6,v3)
#define CreatePoolPPC(v1,v2,v3)	PPCLP4	(PowerPCBase,-816,void *,	struct Library *,3,PowerPCBase,ULONG,4,v1,ULONG,5,v2,ULONG,6,v3)
#define DeletePoolPPC(v1)	PPCLP2NR(PowerPCBase,-822,		struct Library *,3,PowerPCBase,void *,4,v1)
#define AllocPooledPPC(v1,v2)	PPCLP3	(PowerPCBase,-828,void *,	struct Library *,3,PowerPCBase,void *,4,v1,ULONG,5,v2)
#define FreePooledPPC(v1,v2,v3)	PPCLP4NR(PowerPCBase,-834,		struct Library *,3,PowerPCBase,void *,4,v1,void *,5,v2,ULONG,6,v3)

        /* *** lists */
#define AddHeadPPC(v1,v2)	PPCLP3NR(PowerPCBase,-408,		struct Library *,3,PowerPCBase,struct List *,4,v1,struct Node *,5,v2)
#define AddTailPPC(v1,v2)	PPCLP3NR(PowerPCBase,-414,		struct Library *,3,PowerPCBase,struct List *,4,v1,struct Node *,5,v2)
#define EnqueuePPC(v1,v2)	PPCLP3NR(PowerPCBase,-438,		struct Library *,3,PowerPCBase,struct List *,4,v1,struct Node *,5,v2)
#define FindNamePPC(v1,v2)	PPCLP3	(PowerPCBase,-444,struct Node *,struct Library *,3,PowerPCBase,struct List *,4,v1,STRPTR,5,v2)
#define	InsertPPC(v1,v2)	PPCLP3NR(PowerPCBase,-402,		struct Library *,3,PowerPCBase,struct Node *,4,v1,struct Node *,5,v2)
#define RemHeadPPC(v1)		PPCLP2	(PowerPCBase,-426,struct Node *,struct Library *,3,PowerPCBase,struct List *,4,v1)
#define RemovePPC(v1)		PPCLP2NR(PowerPCBase,-420,		struct Library *,3,PowerPCBase,struct Node *,4,v1)
#define RemTailPPC(v1)		PPCLP2	(PowerPCBase,-432,struct Node *,struct Library *,3,PowerPCBase,struct List *,4,v1)
#define NewListPPC(v1)		PPCLP2NR(PowerPCBase,-774,		struct Library *,3,PowerPCBase,struct List *,4,v1)

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

        /* *** tag items */
#define FindTagItemPPC(v1,v2)	PPCLP3	(PowerPCBase,-450,struct TagItem *,struct Library *,3,PowerPCBase,ULONG,4,v1,struct TagItem *,5,v2)
#define GetTagDataPPC(v1,v2,v3)	PPCLP4	(PowerPCBase,-456,ULONG,	struct Library *,3,PowerPCBase,ULONG,4,v1,ULONG,5,v2,struct TagItem *,6,v3)
#define NextTagItemPPC(v1)	PPCLP2	(PowerPCBase,-462,struct TagItem *,struct Library *,3,PowerPCBase,struct TagItem **,4,v1)


        /* *** hardware */
#define ChangeMMU(v1)		PPCLP2NR(PowerPCBase,-588,		struct Library *,3,PowerPCBase,ULONG,4,v1)
#define	GetInfo(v1)		PPCLP2NR(PowerPCBase,-594,		struct Library *,3,PowerPCBase,struct TagItem *,4,v1)
#define SetCache(v1,v2,v3)	PPCLP4NR(PowerPCBase,-510,		struct Library *,3,PowerPCBase,ULONG,4,v1,APTR,5,v2,ULONG,6,v3)
#define SetHardware(v1,v2)	PPCLP3	(PowerPCBase,-540,ULONG,	struct Library *,3,PowerPCBase,ULONG,4,v1,APTR,5,v2)
#define GetHALInfo(v1)		PPCLP2NR(PowerPCBase,-726,		struct Library *,3,PowerPCBase,struct TagItem *,4,v1)
#define	SetScheduling(v1)	PPCLP2NR(PowerPCBase,-732,		struct Library *,3,PowerPCBase,struct TagItem *,4,v1)

        /* *** exceptions */
#define ModifyFPExc(v1)		PPCLP2NR(PowerPCBase,-546,		struct Library *,3,PowerPCBase,ULONG,4,v1)
#define RemExcHandler(v1)	PPCLP2NR(PowerPCBase,-522,		struct Library *,3,PowerPCBase,APTR,4,v1)
#define	SetExcHandler(v1)	PPCLP2	(PowerPCBase,-516,APTR,		struct Library *,3,PowerPCBase,struct TagItem *,4,v1)
#define	SetExcMMU()		PPCLP1NR(PowerPCBase,-576,		struct Library *,3,PowerPCBase)
#define	ClearExcMMU()		PPCLP1NR(PowerPCBase,-582,		struct Library *,3,PowerPCBase)
#define	CauseInterrupt()	PPCLP1NR(PowerPCBase,-810,		struct Library *,3,PowerPCBase)
#define IsExceptionMode()	PPCLP1(PowerPCBase,-864,BOOL,struct Library *,3,PowerPCBase)

        /* *** supervisor */
#define Super()			PPCLP1	(PowerPCBase,-528,ULONG,	struct Library *,3,PowerPCBase)
#define User(v1)		PPCLP2NR(PowerPCBase,-534,		struct Library *,3,PowerPCBase,ULONG,4,v1)

        /* *** 68K connection */
#define Signal68K(v1,v2)	PPCLP3NR(PowerPCBase,-504,		struct Library *,3,PowerPCBase,struct Task *,4,v1,ULONG,5,v2)
#define RawDoFmtPPC(v1,v2,v3,v4) PPCLP5(PowerPCBase,-840,APTR,		struct Library *,3,PowerPCBase,STRPTR,4,v1,APTR,5,v2,void (*)(void),6,v3,APTR,7,v4)	

#endif /* POWERPC_GCCLIB_PROTOS_H */
