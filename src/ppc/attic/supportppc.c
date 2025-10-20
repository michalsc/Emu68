// Copyright (c) 2019, 2020 Dennis van der Boon
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include <powerpc/powerpc.h>
#include <powerpc/tasksPPC.h>
#include "libstructs.h"
#include "constants.h"
#include "support.h"

/********************************************************************************************
*
*
*
*********************************************************************************************/

VOID InsertOnPri(struct PrivatePPCBase* PowerPCBase, struct List* list, struct TaskPPC* myTask)
{
    //myEnqueuePPC(PowerPCBase, list, (struct Node*)myTask);
    //return;

    LONG realPri    = myTask->tp_Priority + myTask->tp_Prioffset;
    LONG defaultPri = PowerPCBase->pp_LowActivityPri + PowerPCBase->pp_LowActivityPriOffset;

    if (realPri >= defaultPri)
    {
        realPri = defaultPri;
        myTask->tp_Prioffset = defaultPri - myTask->tp_Priority;
    }

    struct Node* nextNode;
    struct Node* myNode = list->lh_Head;
    while ((nextNode = myNode->ln_Succ))
    {
        struct TaskPPC* chkTask = (struct TaskPPC*)myNode;

        if (chkTask->tp_Flags & TASKPPCF_ATOMIC)
        {
            myNode = nextNode;
            break;
        }

        LONG cmpPri = chkTask->tp_Priority + chkTask->tp_Prioffset;

        if (realPri > cmpPri)
        {
            break;
        }

        myNode = nextNode;
    }

    struct Node* pred = myNode->ln_Pred;
	myNode->ln_Pred = (struct Node*)myTask;
	myTask->tp_Task.tc_Node.ln_Succ = myNode;
	myTask->tp_Task.tc_Node.ln_Pred = pred;
	pred->ln_Succ = (struct Node*)myTask;
    return;
}

/********************************************************************************************
*
*
*
*********************************************************************************************/

VOID ForbidPPC(struct PrivatePPCBase* PowerPCBase)
{
    PowerPCBase->pp_FlagForbid = 1;

    return;
}

/********************************************************************************************
*
*
*
*********************************************************************************************/

VOID PermitPPC(struct PrivatePPCBase* PowerPCBase)
{
    PowerPCBase->pp_FlagForbid = 0;

    return;
}


/********************************************************************************************
*
*
*
*********************************************************************************************/

struct MsgFrame* CreateMsgFramePPC(struct PrivatePPCBase* PowerPCBase)
{
    (void)PowerPCBase;

	struct SysCall sc;
    sc.sc_Function = SC_CREATEMSG;

    ULONG msgFrame = SystemCall(&sc);

    return (struct MsgFrame*)msgFrame;
}

/********************************************************************************************
*
*
*
*********************************************************************************************/

struct MsgFrame* GetMsgFramePPC(struct PrivatePPCBase* PowerPCBase)
{
    (void)PowerPCBase;

    struct SysCall sc;
    sc.sc_Function = SC_GETMSG;

    ULONG msgFrame = SystemCall(&sc);

	return (struct MsgFrame*)msgFrame;
}

/********************************************************************************************
*
*
*
*********************************************************************************************/

VOID SendMsgFramePPC(struct PrivatePPCBase* PowerPCBase, struct MsgFrame* msgFrame)
{
    (void)PowerPCBase;

	struct SysCall sc;
    sc.sc_Function = SC_SENDMSG;
    sc.sc_Arg[0] = (ULONG)msgFrame;

    SystemCall(&sc);

	return;
}

/********************************************************************************************
*
*
*
*********************************************************************************************/

VOID FreeMsgFramePPC(struct PrivatePPCBase* PowerPCBase, struct MsgFrame* msgFrame)
{
    (void)PowerPCBase;

	struct SysCall sc;
    sc.sc_Function = SC_FREEMSG;
    sc.sc_Arg[0] = (ULONG)msgFrame;

    SystemCall(&sc);

	return;
}


/********************************************************************************************
*
*
*
*********************************************************************************************/

LONG StricmpPPC(STRPTR string1, STRPTR string2)
{
    LONG result = 0;
	ULONG offset = 0;
	UBYTE s1,s2;

	do
	{
		s1 = string1[offset];
		s2 = string2[offset];

		if ((0x40 < s1) && (s1< 0x5b))
		{
			s1 |= 0x20;
		}
		else if (s1 > 0x5a)
		{
			if ((0xc0 < s1) && (s1 < 0xe0))
			{
				s1 |= 0x20;
			}
		}
		if ((0x40 < s2) && (s2 < 0x5b))
		{
			s2 |= 0x20;
		}
		else if (s2 > 0x5a)
		{
			if ((0xc0 < s2) && (s2 < 0xe0))
			{
				s2 |= 0x20;
			}
		}
		if (s1 != s2)
		{
			if (s1 < s2)
			{
				result = -1;
			}
			else
			{
				result = 1;
			}
			break;
		}
		offset ++;
	} while (s1);

	return result;
}

/********************************************************************************************
*
*
*
*********************************************************************************************/

ULONG GetLen(STRPTR string)
{
	ULONG offset = 0;

	while (string[offset])
	{
	 	offset++;
	}
	return offset;
}

/********************************************************************************************
*
*
*
*********************************************************************************************/

STRPTR CopyStr(APTR source, APTR dest)
{
	ULONG offset = -1;

    UBYTE* mySource = (UBYTE*)source;
    UBYTE* myDest   = (UBYTE*)dest;

	do
	{
		offset ++;
		myDest[offset] = mySource[offset];
	} while (mySource[offset]);

    offset ++;

    return (STRPTR)&myDest[offset];
}


#if 0
#pragma pack(push,2)
#include <exec/exec.h>
#include <powerpc/tasksPPC.h>
#include <powerpc/powerpc.h>
#include <exec/memory.h>
#include "constants.h"
#include "libstructs.h"
#include "Internalsppc.h"
#pragma pack(pop)


/********************************************************************************************
*
*
*
*********************************************************************************************/

PPCFUNCTION APTR AllocVec68K(__reg("r3") struct PrivatePPCBase* PowerPCBase, __reg("r4") ULONG size, __reg("r5") ULONG flags)
{
	APTR memBlock = NULL;

	if (size)
	{
		flags = (flags & ~MEMF_CHIP) | MEMF_PPC;
		memBlock = (APTR)myRun68KLowLevel(PowerPCBase, (ULONG)PowerPCBase, _LVOAllocVec32, 0, 0, size, flags);
	
        if (memBlock)
	    {
            mySetCache(PowerPCBase, CACHE_DCACHEINV, memBlock, size); //no longer working...have to check
	    }
	}
	return memBlock;
}

/********************************************************************************************
*
*
*
*********************************************************************************************/

PPCFUNCTION VOID FreeVec68K(__reg("r3") struct PrivatePPCBase* PowerPCBase, __reg("r4") APTR memBlock)
{
    myRun68KLowLevel(PowerPCBase, (ULONG)PowerPCBase, _LVOFreeVec32, 0, (ULONG)memBlock, 0, 0);

	return;
}



/********************************************************************************************
*
*
*
*********************************************************************************************/

PPCFUNCTION VOID CauseDECInterrupt(__reg("r3") struct PrivatePPCBase* PowerPCBase)
{
	ULONG key;

	if (!(PowerPCBase->pp_ExceptionMode))
	{
		key = mySuper(PowerPCBase);
        PowerPCBase->pp_ExceptionMode = -1;
		setDEC(30);
		myUser(PowerPCBase, key);
        while (PowerPCBase->pp_ExceptionMode);
	}
	return;
}

/********************************************************************************************
*
*
*
*********************************************************************************************/

PPCFUNCTION ULONG CheckExcSignal(__reg("r3") struct PrivatePPCBase* PowerPCBase, __reg("r4") struct TaskPPC* myTask, __reg("r5") ULONG signal)
{
	ULONG sigmask, test;

	while (!(LockMutexPPC((volatile ULONG)&PowerPCBase->pp_Mutex)));

	sigmask = (myTask->tp_Task.tc_SigRecvd | signal) & myTask->tp_Task.tc_SigExcept;

	if (!(sigmask))
	{
		FreeMutexPPC((ULONG)&PowerPCBase->pp_Mutex);
		return signal;
	}

	myTask->tp_Task.tc_SigRecvd |= sigmask;
	signal = signal & ~sigmask;
	PowerPCBase->pp_TaskExcept = myTask;
	FreeMutexPPC((ULONG)&PowerPCBase->pp_Mutex);
	CauseDECInterrupt(PowerPCBase);

    while (test = (volatile ULONG)PowerPCBase->pp_TaskExcept);

	return signal;

}

/********************************************************************************************
*
*        APTR = AllocatePPC(struct Library*, struct MemHeader*, ULONG byteSize)
*
*********************************************************************************************/

PPCFUNCTION APTR AllocatePPC(__reg("r3") struct PrivatePPCBase* PowerPCBase, __reg("r4") struct MemHeader* memHeader, __reg("r5") ULONG byteSize)
{
    struct DebugArgs args;
    args.db_Function = 66 | (2<<8) | (1<<16) | (3<<17);
    args.db_Arg[0] = (ULONG)memHeader;
    args.db_Arg[1] = byteSize;
    printDebug(PowerPCBase, (struct DebugArgs*)&args);

    struct MemChunk* currChunk = NULL;

    if (byteSize)
    {
        byteSize = (byteSize + 31) & -32;

        if (byteSize <= memHeader->mh_Free)
        {
            struct MemChunk* newChunk;
            struct MemChunk* prevChunk = (struct MemChunk*)&memHeader->mh_First;

            while (currChunk = prevChunk->mc_Next)
            {
                if (currChunk->mc_Bytes == byteSize)
                {
                    prevChunk->mc_Next = currChunk->mc_Next;
                    break;
                }
                else if (currChunk->mc_Bytes > byteSize)
                {
                    newChunk = (struct MemChunk*)((ULONG)currChunk + byteSize);
                    newChunk->mc_Next = currChunk->mc_Next;
                    newChunk->mc_Bytes = currChunk->mc_Bytes - byteSize;
                    prevChunk->mc_Next = newChunk;
                    break;
                }
                prevChunk = currChunk;
            }

            if (currChunk)
            {
                memHeader->mh_Free -= byteSize;

                UBYTE* buffer = (APTR)currChunk;
                for (int i=0; i < byteSize; i++)
                {
                    buffer[i] = 0;
                }
            }
        }
    }

    args.db_Arg[0] = (ULONG)currChunk;
    printDebug(PowerPCBase, (struct DebugArgs*)&args);

    return (APTR)currChunk;
}

/********************************************************************************************
*
*        VOID DeallocatePPC(struct Library*, struct MemHeader*, APTR, ULONG)
*
*********************************************************************************************/

PPCFUNCTION VOID DeallocatePPC(__reg("r3") struct PrivatePPCBase* PowerPCBase, __reg("r4") struct MemHeader* memHeader,
                   __reg("r5") APTR memoryBlock, __reg("r6") ULONG byteSize)
{
	struct DebugArgs args;
    args.db_Function = 67 | (3<<8) | (1<<16) | (3<<17);
    args.db_Arg[0] = (ULONG)memHeader;
    args.db_Arg[1] = (ULONG)memoryBlock;
    args.db_Arg[2] = byteSize;
    printDebug(PowerPCBase, (struct DebugArgs*)&args);

    if (byteSize)
    {
        ULONG testSize = (((ULONG)memoryBlock) - ((ULONG)(memoryBlock) & -32));
        struct MemChunk* testChunk = (struct MemChunk*)((ULONG)(memoryBlock) & -32);
        ULONG freeSize = testSize + byteSize + 31;

        if (freeSize &= -32)
        {
	        struct MemChunk* testChunk = (struct MemChunk*)memoryBlock;
            struct MemChunk* currChunk = (struct MemChunk*)&memHeader->mh_First;

	        ULONG flag = 0;

	        while (currChunk->mc_Next)
	        {
		        if (currChunk->mc_Next > testChunk)
		        {
			        if ((currChunk == (struct MemChunk*)&memHeader->mh_First) || ((ULONG)testChunk > currChunk->mc_Bytes + (ULONG)currChunk))
			        {
                        break;
			        }
			        else if ((ULONG)testChunk < currChunk->mc_Bytes + (ULONG)currChunk)
			        {
                        HaltError(ERR_EMEM);
			        }
			        flag = 1;
                    currChunk->mc_Bytes += freeSize;
		            testChunk = currChunk;
			        break;
		        }
		        else if (currChunk->mc_Next == testChunk)
		        {
                    HaltError(ERR_EMEM);
		        }
	            currChunk = currChunk->mc_Next;
	        }
	
            if (!(flag))
            {
		        testChunk->mc_Next = currChunk->mc_Next;
		        currChunk->mc_Next = testChunk;
		        testChunk->mc_Bytes = freeSize;
	        }

	        struct MemChunk* nextChunk = testChunk->mc_Next;
	
            if (nextChunk)
	        {
		        if ((ULONG)nextChunk < (ULONG)(testChunk) + testChunk->mc_Bytes)
		        {
                    HaltError(ERR_EMEM);
		        }
		        else if ((ULONG)nextChunk == (ULONG)(testChunk) + testChunk->mc_Bytes)
		        {
			        testChunk->mc_Next = nextChunk->mc_Next;
			        testChunk->mc_Bytes = nextChunk->mc_Bytes + testChunk->mc_Bytes;
		        }
	        }
	        memHeader->mh_Free += freeSize;
        }
    }
	return;
}

/********************************************************************************************
*
*
*
*********************************************************************************************/

PPCFUNCTION VOID printDebug(__reg("r3") struct PrivatePPCBase* PowerPCBase, __reg("r4") struct DebugArgs* args)
{
#if 0
    ULONG* mem = (APTR)(PowerPCBase->pp_PPCMemBase + OFFSET_SYSMEM + 0x100);
    mem [args->db_Function & 0xff] += 1;
    mem [-1] = (args->db_Function & 0xff);
#endif
    if ((PowerPCBase->pp_DebugLevel) && (!(PowerPCBase->pp_ExceptionMode)))
    {
        ULONG flag = args->db_Function & (1<<16);
        ULONG level = args->db_Function >> 17;

        if (level > PowerPCBase->pp_DebugLevel - 1)
        {
            return;
        }

        args->db_Function &= ~(7<<16);

        struct MsgFrame* myFrame = CreateMsgFramePPC(PowerPCBase);
        args->db_ProcessName = PowerPCBase->pp_ThisPPCProc->tp_Task.tc_Node.ln_Name;
        UBYTE oldlevel = PowerPCBase->pp_DebugLevel;

        while (!(LockMutexPPC((volatile ULONG)&PowerPCBase->pp_Mutex)));
        PowerPCBase->pp_DebugLevel = 0;
        myCopyMemPPC(PowerPCBase, (APTR)args, (APTR)&myFrame->mf_PPCArgs, sizeof(struct DebugArgs));
        PowerPCBase->pp_DebugLevel = oldlevel;
        FreeMutexPPC((ULONG)&PowerPCBase->pp_Mutex);

        if (flag)
        {
            myFrame->mf_Identifier = ID_DBGS;
        }
        else
        {
            myFrame->mf_Identifier = ID_DBGE;
        }

        SendMsgFramePPC(PowerPCBase, myFrame);

        args->db_Function |= (level << 17);
    }
    return;
}

/********************************************************************************************
*
*
*
*********************************************************************************************/

PPCFUNCTION VOID GetBATs(__reg("r3") struct PrivatePPCBase* PowerPCBase, __reg("r4") struct TaskPPC* task)
{
    ULONG key = mySuper(PowerPCBase);

    MoveFromBAT(CHMMU_BAT0, (struct BATArray*)(((ULONG)task->tp_BATStorage) + 0));
    MoveFromBAT(CHMMU_BAT1, (struct BATArray*)(((ULONG)task->tp_BATStorage) + 16));
    MoveFromBAT(CHMMU_BAT2, (struct BATArray*)(((ULONG)task->tp_BATStorage) + 32));
    MoveFromBAT(CHMMU_BAT3, (struct BATArray*)(((ULONG)task->tp_BATStorage) + 48));

    MoveToBAT(CHMMU_BAT0, (struct BATArray*)&PowerPCBase->pp_StoredBATs[0]);
    MoveToBAT(CHMMU_BAT1, (struct BATArray*)&PowerPCBase->pp_StoredBATs[1]);
    MoveToBAT(CHMMU_BAT2, (struct BATArray*)&PowerPCBase->pp_StoredBATs[2]);
    MoveToBAT(CHMMU_BAT3, (struct BATArray*)&PowerPCBase->pp_StoredBATs[3]);

    myUser(PowerPCBase, key);

    return;
}

/********************************************************************************************
*
*
*
*********************************************************************************************/

PPCFUNCTION VOID StoreBATs(__reg("r3") struct PrivatePPCBase* PowerPCBase, __reg("r4") struct TaskPPC* task)
{
    ULONG key = mySuper(PowerPCBase);

    MoveToBAT(CHMMU_BAT0, (struct BATArray*)&PowerPCBase->pp_StoredBATs[0]);
    MoveToBAT(CHMMU_BAT1, (struct BATArray*)&PowerPCBase->pp_StoredBATs[1]);
    MoveToBAT(CHMMU_BAT2, (struct BATArray*)&PowerPCBase->pp_StoredBATs[2]);
    MoveToBAT(CHMMU_BAT3, (struct BATArray*)&PowerPCBase->pp_StoredBATs[3]);

    MoveFromBAT(CHMMU_BAT0, (struct BATArray*)(((ULONG)task->tp_BATStorage) + 0));
    MoveFromBAT(CHMMU_BAT1, (struct BATArray*)(((ULONG)task->tp_BATStorage) + 16));
    MoveFromBAT(CHMMU_BAT2, (struct BATArray*)(((ULONG)task->tp_BATStorage) + 32));
    MoveFromBAT(CHMMU_BAT3, (struct BATArray*)(((ULONG)task->tp_BATStorage) + 64));

    myUser(PowerPCBase, key);
}

/********************************************************************************************
*
*
*
*********************************************************************************************/

PPCFUNCTION VOID MoveToBAT(__reg("r3") ULONG BATnumber, __reg("r4") struct BATArray* batArray)
{
    switch (BATnumber)
    {
       case CHMMU_BAT0:
       {
           mvtoBAT0(batArray);
           break;
       }
       case CHMMU_BAT1:
       {
           mvtoBAT1(batArray);
           break;
       }
       case CHMMU_BAT2:
       {
           mvtoBAT2(batArray);
           break;
       }
       case CHMMU_BAT3:
       {
           mvtoBAT3(batArray);
           break;
       }
    }
    return;
}

/********************************************************************************************
*
*
*
*********************************************************************************************/

PPCFUNCTION VOID MoveFromBAT(__reg("r3") ULONG BATnumber, __reg("r4") struct BATArray* batArray)
{
    switch (BATnumber)
    {
       case CHMMU_BAT0:
       {
           mvfrBAT0(batArray);
           break;
       }
       case CHMMU_BAT1:
       {
           mvfrBAT1(batArray);
           break;
       }
       case CHMMU_BAT2:
       {
           mvfrBAT2(batArray);
           break;
       }
       case CHMMU_BAT3:
       {
           mvfrBAT3(batArray);
           break;
       }
    }
    return;
}

/********************************************************************************************
*
*    Function to set up the system processes.
*
*********************************************************************************************/

PPCFUNCTION VOID SystemStart(__reg("r3") struct PrivatePPCBase* PowerPCBase)
{
    struct TaskPPC* myTask;
    APTR myPool;
    struct MemList* myMem;

    while (1)
    {
        myWaitTime(PowerPCBase, 0, 0x4c0000); // Around 5 seconds.
        while (myTask = (struct TaskPPC*)myRemHeadPPC(PowerPCBase, (struct List*)&PowerPCBase->pp_RemovedTasks))
        {
            if ((myTask->tp_Flags & TASKPPCF_CRASHED) && (myTask->tp_Flags & TASKPPCF_CREATORPPC))
            {
                myDeleteTaskPPC(PowerPCBase, myTask);
            }
            while (myPool = (APTR)myRemHeadPPC(PowerPCBase, (struct List*)&myTask->tp_TaskPools))
            {
                myDeletePoolPPC(PowerPCBase, myPool);
            }
            while (myMem = (struct MemList*)myRemHeadPPC(PowerPCBase, (struct List*)&myTask->tp_Task.tc_MemEntry))
            {
                FreeVec68K(PowerPCBase, myMem->ml_ME[0].me_Un.meu_Addr);
                FreeVec68K(PowerPCBase, myMem);
            }
            FreeVec68K(PowerPCBase, myTask);
         }
    }
}

/********************************************************************************************
*
*
*
*********************************************************************************************/

PPCFUNCTION VOID FreeAllExcMem(__reg("r3") struct PrivatePPCBase* PowerPCBase, __reg("r4") struct ExcInfo* myInfo)
{
    APTR myData;

    if (myData = myInfo->ei_MachineCheck)
    {
        FreeVec68K(PowerPCBase,myData);
    }
    if (myData = myInfo->ei_DataAccess)
    {
        FreeVec68K(PowerPCBase,myData);
    }
    if (myData = myInfo->ei_InstructionAccess)
    {
        FreeVec68K(PowerPCBase,myData);
    }
    if (myData = myInfo->ei_Alignment)
    {
        FreeVec68K(PowerPCBase,myData);
    }
    if (myData = myInfo->ei_Program)
    {
        FreeVec68K(PowerPCBase,myData);
    }
    if (myData = myInfo->ei_FPUnavailable)
    {
        FreeVec68K(PowerPCBase,myData);
    }
    if (myData = myInfo->ei_Decrementer)
    {
        FreeVec68K(PowerPCBase,myData);
    }
    if (myData = myInfo->ei_SystemCall)
    {
        FreeVec68K(PowerPCBase,myData);
    }
    if (myData = myInfo->ei_Trace)
    {
        FreeVec68K(PowerPCBase,myData);
    }
    if (myData = myInfo->ei_PerfMon)
    {
        FreeVec68K(PowerPCBase,myData);
    }
    if (myData = myInfo->ei_IABR)
    {
        FreeVec68K(PowerPCBase,myData);
    }
    if (myData = myInfo->ei_Interrupt)
    {
        FreeVec68K(PowerPCBase,myData);
    }
    return;
}

/********************************************************************************************
*
*
*
*********************************************************************************************/

PPCFUNCTION VOID AddExcList(__reg("r3") struct PrivatePPCBase* PowerPCBase, __reg("r4") struct ExcInfo* excInfo, __reg("r5") struct ExcData* newData, __reg("r6") ULONG* currExc, __reg("r7") ULONG flag)
{
    myCopyMemPPC(PowerPCBase, (APTR)excInfo, (APTR)newData, sizeof(struct ExcData));
    currExc[0] = (ULONG)newData;
    newData->ed_ExcID = flag;
    excInfo->ei_ExcData.ed_LastExc = newData;
    while (!(LockMutexPPC((volatile ULONG)&PowerPCBase->pp_Mutex)));
    myAddHeadPPC(PowerPCBase, (struct List*)&PowerPCBase->pp_ReadyExc, (struct Node*)newData);
    FreeMutexPPC((ULONG)&PowerPCBase->pp_Mutex);
    return;
}

/********************************************************************************************
*
*
*
*********************************************************************************************/

PPCFUNCTION VOID SetupRunPPC(__reg("r3") struct PrivatePPCBase* PowerPCBase, __reg("r4") struct MsgFrame* myFrame)
{
    while (!(LockMutexPPC((volatile ULONG)&PowerPCBase->pp_Mutex)));

    struct TaskPPC* myTask = PowerPCBase->pp_ThisPPCProc;

    myTask->tp_Task.tc_SigRecvd |= myFrame->mf_Signals;
    FreeMutexPPC((ULONG)&PowerPCBase->pp_Mutex);

    myObtainSemaphorePPC(PowerPCBase, (struct SignalSemaphorePPC*)&PowerPCBase->pp_SemSnoopList);

    ULONG myCode = (ULONG)myFrame->mf_PPCArgs.PP_Code + myFrame->mf_PPCArgs.PP_Offset;
    if (myFrame->mf_PPCArgs.PP_Offset)
    {
        myCode = *((ULONG*)(myCode + 2));
    }

    struct SnoopData* currSnoop = (struct SnoopData*)PowerPCBase->pp_Snoop.mlh_Head;
    struct SnoopData* nextSnoop;

    while (nextSnoop = (struct SnoopData*)currSnoop->sd_Node.ln_Succ)
    {
        if (currSnoop->sd_Type == SNOOP_START)
        {
            ULONG (*runSnoop)(__reg("r2") ULONG, __reg("r3") struct TaskPPC*,
            __reg("r4") ULONG, __reg("r5") struct Task*,
            __reg("r6") ULONG) = currSnoop->sd_Code;
            ULONG tempR2 = getR2();
            runSnoop(currSnoop->sd_Data, myTask, myCode, (struct Task*)myFrame->mf_Arg[2], CREATOR_68K);
            storeR2(tempR2);
        }
        currSnoop = nextSnoop;
    }

    myReleaseSemaphorePPC(PowerPCBase, (struct SignalSemaphorePPC*)&PowerPCBase->pp_SemSnoopList);

    if ((myFrame->mf_PPCArgs.PP_Stack) && (myFrame->mf_PPCArgs.PP_StackSize))
    {
        mySetCache(PowerPCBase, CACHE_DCACHEINV, myFrame->mf_PPCArgs.PP_Stack, myFrame->mf_PPCArgs.PP_StackSize);
    }

    myTask->tp_Task.tc_SigAlloc = myFrame->mf_Arg[1];

    struct iframe storeFrame;

    RunCPP((struct iframe*)&storeFrame, myCode, &myFrame->mf_PPCArgs);

    struct MsgFrame* newFrame = CreateMsgFramePPC(PowerPCBase);

    newFrame->mf_Identifier = ID_FPPC;
    newFrame->mf_Message.mn_Length = MSGLEN;
    newFrame->mf_Message.mn_Node.ln_Type = NT_MESSAGE;

    while (!(LockMutexPPC((volatile ULONG)&PowerPCBase->pp_Mutex)));

    newFrame->mf_Arg[0]  = myTask->tp_Task.tc_SigAlloc;
    newFrame->mf_Signals = myTask->tp_Task.tc_SigRecvd & 0xfffff000;
    myTask->tp_Task.tc_SigRecvd &= 0xfff;

    FreeMutexPPC((ULONG)&PowerPCBase->pp_Mutex);

    newFrame->mf_Message.mn_ReplyPort = myFrame->mf_Message.mn_ReplyPort;
    newFrame->mf_PPCTask = myTask;

    myCopyMemPPC(PowerPCBase, &myFrame->mf_PPCArgs.PP_Regs, &newFrame->mf_PPCArgs.PP_Regs, (15*4) + (8*8));

    SendMsgFramePPC(PowerPCBase, newFrame);
    FreeMsgFramePPC(PowerPCBase, myFrame);

    return;
}

/********************************************************************************************
*
*
*
*********************************************************************************************/

PPCFUNCTION VOID StartTask(__reg("r3") struct PrivatePPCBase* PowerPCBase, __reg("r4") struct MsgFrame* myFrame)
{
    SetupRunPPC(PowerPCBase, myFrame);

    struct TaskPPC* myTask = PowerPCBase->pp_ThisPPCProc;
    struct PrivateTask* myPTask = (struct PrivateTask*)myTask;
    struct MsgFrame* newFrame;
    struct MsgPort* myPort;
    ULONG signals;
    ULONG mask;

    while(1)
    {
        signals = myWaitPPC(PowerPCBase, myTask->tp_Task.tc_SigAlloc & 0xfffff100);

        if (signals & SIGF_DOS)
        {
            if (mask = signals & ~SIGF_DOS)
            {
                while (!(LockMutexPPC((volatile ULONG)&PowerPCBase->pp_Mutex)));

                myTask->tp_Task.tc_SigRecvd |= mask;

                FreeMutexPPC((ULONG)&PowerPCBase->pp_Mutex);
            }
            while (newFrame = (struct MsgFrame*)myGetMsgPPC(PowerPCBase, myTask->tp_Msgport))
            {
                switch (newFrame->mf_Identifier)
                {
                    case ID_TPPC:
                    {
                        SetupRunPPC(PowerPCBase, newFrame);
                        break;
                    }
                    case ID_END:
                    {
                        KillTask(PowerPCBase, newFrame);
                        break;
                    }
                    default:
                    {
                        HaltError(0xBAD0BAD0);
                        break; //error
                    }
                }
            }
        }
        else if (mask = signals & ~SIGF_DOS)
        {
            if (myPort = myPTask->pt_MirrorPort)
            {
                mySignal68K(PowerPCBase, myPort->mp_SigTask, mask);
            }
        }
    }
}

/********************************************************************************************
*
*
*
*********************************************************************************************/

PPCFUNCTION VOID EndTask(VOID)
{
    struct PPCZeroPage *myZP = 0;
    ULONG key = mySuper(NULL);             //Super does not use PowerPCBase
    struct PrivatePPCBase* PowerPCBase = (struct PrivatePPCBase*)myZP->zp_PowerPCBase;
    myUser(PowerPCBase, key);
    myDeleteTaskPPC(PowerPCBase, NULL);

    return;
}

/********************************************************************************************
*
*
*
*********************************************************************************************/

PPCFUNCTION VOID KillTask(__reg("r3") struct PrivatePPCBase* PowerPCBase, __reg("r4") struct MsgFrame* myFrame)
{
    FreeMsgFramePPC(PowerPCBase, myFrame);

    myObtainSemaphorePPC(PowerPCBase, (struct SignalSemaphorePPC*)&PowerPCBase->pp_SemSnoopList);

    struct SnoopData* currSnoop = (struct SnoopData*)PowerPCBase->pp_Snoop.mlh_Head;
    struct SnoopData* nextSnoop;

    struct TaskPPC* PPCtask = PowerPCBase->pp_ThisPPCProc;

    while (nextSnoop = (struct SnoopData*)currSnoop->sd_Node.ln_Succ)
    {
        if (currSnoop->sd_Type == SNOOP_EXIT)
        {
            ULONG tempR2 = getR2();
            ULONG (*runSnoop)(__reg("r2") ULONG, __reg("r3") struct TaskPPC*) = currSnoop->sd_Code;
            runSnoop(currSnoop->sd_Data, PPCtask);
            storeR2(tempR2);
        }
        currSnoop = nextSnoop;
    }

    myReleaseSemaphorePPC(PowerPCBase, (struct SignalSemaphorePPC*)&PowerPCBase->pp_SemSnoopList);

    myObtainSemaphorePPC(PowerPCBase, (struct SignalSemaphorePPC*)&PowerPCBase->pp_SemTaskList);

    if (PPCtask->tp_TaskPtr)
    {
        myRemovePPC(PowerPCBase, (struct Node*)PPCtask->tp_TaskPtr);
    }

    PowerPCBase->pp_NumAllTasks -= 1;

    myReleaseSemaphorePPC(PowerPCBase, (struct SignalSemaphorePPC*)&PowerPCBase->pp_SemTaskList);

    PPCtask->tp_Task.tc_State = TS_REMOVED;
    PowerPCBase->pp_FlagReschedule = -1;
    CauseDECInterrupt(PowerPCBase);
    TaskHalt();
}

/********************************************************************************************
*
*
*
*********************************************************************************************/

PPCFUNCTION ULONG getNum(__reg("r3") struct RDFData* rdfData)
{
    ULONG result = 0;

    while (1)
    {
        UBYTE myChar = rdfData->rd_FormatString[0];
        rdfData->rd_FormatString += 1;
        if ((myChar >= '0') && (myChar <= '9'))
        {
            result = result * 10;
            myChar -= '0';
            result = result + myChar;
        }
        else
        {
            rdfData->rd_FormatString -= 1;
            break;
        }
    }
    return result;
}

/********************************************************************************************/

PPCFUNCTION LONG AdjustParamInt(__reg("r3") struct RDFData* rdfData)
{
    ULONG* mem = (ULONG*)rdfData->rd_DataStream;
    LONG value = mem[0];
    mem = (ULONG*)((ULONG)mem + 4);
    rdfData->rd_DataStream = (APTR)mem;
    return value;
}

/********************************************************************************************/

PPCFUNCTION LONG AdjustParam(__reg("r3") struct RDFData* rdfData, __reg("r4") ULONG flag)
{
    if (flag & RDFF_LONG)
    {
        return(AdjustParamInt(rdfData));
    }
    else
    {
        WORD value;
        UWORD* mem = (UWORD*)rdfData->rd_DataStream;
        value = mem[0];
        mem = (UWORD*)((ULONG)mem + 2);
        rdfData->rd_DataStream = (APTR)mem;
        return (LONG)value;
    }
}

/********************************************************************************************/

PPCFUNCTION VOID MakeDecimal(__reg("r3") struct RDFData* rdfData, __reg("r4") BOOL sign, __reg("r5") LONG value)
{
    if ((sign) && (value < 0))
    {
        rdfData->rd_BufPointer[0] = '-';
        rdfData->rd_BufPointer += 1;
        value = -value;
    }
    ULONG* myTable = GetDecTable();
    ULONG tableValue, number;
    ULONG cmpnumber = '0';
    {
        while (tableValue = myTable[0])
        {
            number = '0';
            while (tableValue <= value)
            {
               value -= tableValue;
               number += 1;
            }
            if (number != cmpnumber)
            {
               cmpnumber = 0;
               rdfData->rd_BufPointer[0] = (UBYTE)number;
               rdfData->rd_BufPointer += 1;
            }
            myTable += 1;
        }
    }
    rdfData->rd_BufPointer[0] = (UBYTE)(value + '0');
    rdfData->rd_BufPointer += 1;
    return;
}

/********************************************************************************************/

PPCFUNCTION VOID MakeHex(__reg("r3") struct RDFData* rdfData, __reg("r4") ULONG flag, __reg("r5") LONG value)
{
    ULONG iterations, currNibble;
    ULONG mySwitch = 0;

    if (value)
    {
        if (flag & RDFF_LONG)
        {
            iterations = 8;
        }
        else
        {
            iterations = 4;
            value = roll(value, 16);
        }
        for (int i = 0; i < iterations; i++)
        {
            currNibble = roll(value, 4) & 0xf;
            if ((currNibble) || (mySwitch))
            {
                mySwitch = -1;
                if (currNibble > 9)
                {
                    currNibble += 55;
                }
                else
                {
                    currNibble += 48;
                }

                rdfData->rd_BufPointer[0] = currNibble;
                rdfData->rd_BufPointer += 1;
            }
        }
        return;
    }
    rdfData->rd_BufPointer[0] = (UBYTE)(value + '0');
    rdfData->rd_BufPointer += 1;
    return;
}

/********************************************************************************************/

PPCFUNCTION VOID PerformPad(__reg("r3") struct RDFData* rdfData, __reg("r4") ULONG flag, __reg("r5") APTR (*putchproc)(), __reg("r6") LONG prependNum)
{
    UBYTE currChar;
    UBYTE useChar;

    if (flag & RDFF_PREPEND)
    {
        currChar = rdfData->rd_BufPointer[0];
        if (currChar == '-')
        {
            rdfData->rd_BufPointer += 1;
            rdfData->rd_TruncateNum -= 1;
            if (putchproc)
            {
                rdfData->rd_PutChData = putchproc(rdfData->rd_PutChData, currChar);
            }
            else
            {
                STRPTR myData = (STRPTR)rdfData->rd_PutChData;
                myData[0] = currChar;
                rdfData->rd_PutChData = (APTR)((ULONG)rdfData->rd_PutChData + 1);
            }
         }
        useChar = '0';
    }
    else
    {
        useChar = ' ';
    }
    if (prependNum)
    {
        for (int i = 0; i < prependNum; i++)
        {
            if (putchproc)
            {
                rdfData->rd_PutChData  = putchproc(rdfData->rd_PutChData, useChar);
            }
            else
            {
                STRPTR myData = (STRPTR)rdfData->rd_PutChData;
                myData[0] = useChar;
                rdfData->rd_PutChData = (APTR)((ULONG)rdfData->rd_PutChData + 1);
            }
        }
    }
    return;
}

/********************************************************************************************/
#endif