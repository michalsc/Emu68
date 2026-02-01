

// Copyright (c) 2019-2021 Dennis van der Boon
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

#pragma pack(push,2)
#include <powerpc/powerpc.h>
#include <powerpc/powerpc_protos.h>
#include "powerpc.h"
#pragma pack(pop)

#include "libstructs.h"
#include "support.h"
#include "doorbell.h"
#include "kernel.h"

void Start()
{
    /* We start in C as soon as possible. Reset MSR to known state first */
    uint32_t msr = getMSR();
    setMSR((msr & MSR_IP) | MSR_EE);

    /* Reset SPRG0..3 */
    asm volatile("mtsprg0 %0; mtsprg1 %0; mtsprg2 %0; mtsprg3 %0"::"r"(0));

    /* Nothing can be really done here until the PowerPC base is set up by m68k part */
    kprintf("[PPC] Booting Emu68-PowerPC Kernel\n");

    kprintf("[PPC] Waiting for powerpc.library to come up\n");

    uint32_t msg = doorbell_wait((doorbell_t *)0xffefff80);

    kprintf("[PPC] PPCBase = %08x\n", msg);

    struct PrivatePPCBase *PPCBase = (struct PrivatePPCBase *)msg;

    setBASE(PPCBase);

    PatchLVOTable(&PPCBase->pp_Public);

    kprintf("[PPC] Ringing m68k back\n");

    struct XMessage m;
    m.id = XMSG_SIGNAL_M68K_TASK;
    m.xmSignalM68kTask.task = PPCBase->pp_WaitingTask;
    m.xmSignalM68kTask.sigset = 1 << PPCBase->pp_WaitingTaskBit;

    msr = getMSR();
    setMSR(msr | MSR_EE);

    kprintf("[PPC] External interrupts enabled\n");

    SendPacketMessage(PPCBase, &m);

    kprintf("[PPC] Packet sent\n");

    while(1);
}

void SendPacketMessage(struct PrivatePPCBase * PPCBase, APTR message)
{
    ULONG reg;

    /* Send message */
    doorbell_send(&PPCBase->PPC_to_M68k, STATUS_MSG);
    
    /* Fire interrupt */
    asm volatile("mfspr %0, 921":"=r"(reg));
    reg |= 0x80000000;
    asm volatile("mtspr 921, %0"::"r"(reg));
    
    /* Wait for ACK */
    while(doorbell_wait(&PPCBase->M68k_to_PPC) != STATUS_ACK);

    /* Send message address */
    doorbell_send(&PPCBase->PPC_to_M68k, (ULONG)message);

    /* Wait for ACK */
    while(doorbell_wait(&PPCBase->M68k_to_PPC) != STATUS_ACK);
}

APTR StartRecievingMessage(struct PrivatePPCBase * PPCBase)
{
    while(doorbell_wait(&PPCBase->M68k_to_PPC) != STATUS_MSG);

    doorbell_send(&PPCBase->PPC_to_M68k, STATUS_ACK);

    return (APTR)doorbell_wait(&PPCBase->M68k_to_PPC);
}

void EndReceivingMessage(struct PrivatePPCBase * PPCBase)
{
    doorbell_send(&PPCBase->PPC_to_M68k, STATUS_ACK);
}

void Exception_Entry(struct PrivatePPCBase * PowerPCBase, struct iframe *iframe)
{
    /* Get the vector we are in, recaltulate the fields to match what's expected */
    ULONG ExceptionVector = iframe->if_Context.ec_ExcID & 0xfff0;
    iframe->if_ExcNum = ExceptionVector >> 8;
    iframe->if_Context.ec_ExcID = 1 << iframe->if_ExcNum;

//    kprintf("[PPC] ExceptionEntry if_ExcNum=%x, excid=%d\n", iframe->if_ExcNum, iframe->if_Context.ec_ExcID);

//asm volatile("\n1: b 1b");


    switch(iframe->if_ExcNum) {
        case 5:
        {
            struct XMessage *msg = StartRecievingMessage(PowerPCBase);
            if (msg->id == XMSG_CAUSE) {
                kprintf("[PPC] Cause() triggered from m68k\n");
            }
            EndReceivingMessage(PowerPCBase);
            break;
        }

        case 7:
        {
            extern void* L_Super_Addr;
            
            if (iframe->if_Context.ec_GPR[4] == SUPERKEY && iframe->if_Context.ec_UPC.ec_SRR0 == (ULONG)&L_Super_Addr) {
                iframe->if_Context.ec_UPC.ec_SRR0 += 4;
                iframe->if_Context.ec_SRR1 &= ~MSR_PR;
                iframe->if_Context.ec_GPR[3] = 0;
            }
            else {
                kprintf("[PPC] Program exception @ %08x\n", iframe->if_Context.ec_UPC.ec_SRR0, &L_Super_Addr);
                for (int i=0; i < 32; i+=4) {
                    kprintf("[PPC]    r%02d = 0x%08x   r%02d = 0x%08x   r%02d = 0x%08x   r%02d = 0x%08x\n",
                    i, iframe->if_Context.ec_GPR[i], i+1, iframe->if_Context.ec_GPR[i+1],
                    i+2, iframe->if_Context.ec_GPR[i+2], i+3, iframe->if_Context.ec_GPR[i+3]);
                }
                kprintf("[PPC]\n[PPC]     LR = 0x%08x    CR = 0x%08x   CTR = 0x%08x   XER = 0x%08x\n",
                    iframe->if_Context.ec_LR, iframe->if_Context.ec_CR, iframe->if_Context.ec_CTR, iframe->if_Context.ec_XER);
                kprintf("[PPC]  SPRG0 = 0x%08x SPRG1 = 0x%08x\n",
                    iframe->if_Context.ec_UPC.ec_SRR0, iframe->if_Context.ec_SRR1);
                while(1);
            }
            
            break;
        }

        case 9:
        {
            ULONG tbl, tbu;
            asm volatile("mftbl %0; mftbu %1":"=r"(tbl),"=r"(tbu));
            kprintf("[PPC] Decrementer, tb=%08x%08x\n", tbu, tbl);
            ULONG dec;
            asm volatile("mfspr %0, 904":"=r"(dec));
            kprintf("[PPC]   programming next decrementer to %d (1 second)\n", dec);
            asm volatile("mtdec %0"::"r"(dec));
            break;
        }

        case 12:
        {
            kprintf("[PPC] SystemCall(%d)\n", iframe->if_Context.ec_GPR[3]);
            switch (iframe->if_Context.ec_GPR[3])
            {
                case SC_CAUSE:
                    kprintf("[PPC]   SC_CAUSE\n");
                    break;
            
                default:
                    kprintf("[PPC]   Unknown system call\n");
                    break;
            }
        }
    }

//    kprintf("[PPC] End of kernel... waiting for more to come ;)\n");

//    while(1);
}

#if 0
/********************************************************************************************
*
*
*
*********************************************************************************************/

VOID CPUStats(struct PrivatePPCBase* PowerPCBase)
{
    ULONG currentTBL = getTBL();
    ULONG prevTBL = PowerPCBase->pp_CurrentTBL;
    PowerPCBase->pp_CurrentTBL = currentTBL;

    if (prevTBL)
    {
        ULONG systemLoad = 0;
        ULONG cpuLoad = 0;
        ULONG cpuUsage = 0;

        ULONG cpuTime = currentTBL - prevTBL;

        struct TaskPtr* currTask = (struct TaskPtr*)PowerPCBase->pp_AllTasks.mlh_Head;
        struct TaskPtr* nextTask;

        while ((nextTask = (struct TaskPtr*)currTask->tptr_Node.ln_Succ))
        {
            struct TaskPPC* ppcTask = (struct TaskPPC*)currTask->tptr_Task;

            ppcTask->tp_Totalelapsed += cpuTime;
            if ((ppcTask->tp_Task.tc_State == TS_RUN) || (ppcTask->tp_Task.tc_State == TS_CHANGING))
            {
                ppcTask->tp_Elapsed += cpuTime;
            }
            else if (ppcTask->tp_Task.tc_State == TS_WAIT)
            {
                ppcTask->tp_Elapsed2 += cpuTime;
            }

            if (!(PowerPCBase->pp_BusyCounter))
            {
                ULONG elapsed = ppcTask->tp_Elapsed;
                ULONG elapsed2 = ppcTask->tp_Elapsed2;
                ULONG total = ppcTask->tp_Totalelapsed;

                ULONG activity = (elapsed + elapsed2) >> 10;
                ULONG actelps = (elapsed >> 10) * 10000;

                if (activity)
                {
                    activity = actelps / activity;
                }

                ppcTask->tp_Activity = activity;

                LONG busy = (total >> 10) - (elapsed2 >> 10);

                if (busy < 0)
                {
                    busy = 0;
                }

                busy = (busy * 10000) / (total >> 10);

                ppcTask->tp_Busy = busy;

                systemLoad += busy;

                ULONG total10 = total >> 10;

                if (total10)
                {
                    cpuUsage = actelps / total10;
                }

                if (cpuUsage > 10000)
                {
                   cpuUsage = 10000;
                }

                ppcTask->tp_CPUusage = cpuUsage;

                cpuLoad += cpuUsage;

                ppcTask->tp_Elapsed = 0;
                ppcTask->tp_Elapsed2 = 0;
                ppcTask->tp_Totalelapsed = 0;
            }
            currTask = nextTask;
        }

        if (!(PowerPCBase->pp_BusyCounter))
        {
            PowerPCBase->pp_BusyCounter = 25;
            PowerPCBase->pp_SystemLoad = systemLoad;
            PowerPCBase->pp_CPULoad = cpuLoad;
        }

        PowerPCBase->pp_BusyCounter -= 1;

    }
    return;
}

#if 0

#pragma pack(push,2)
#include <proto/powerpc.h>
#include <powerpc/powerpc.h>
#include "constants.h"
#include "libstructs.h"
#include "Internalsppc.h"
#pragma pack(pop)

/********************************************************************************************
*
*	Entry point after kernel.s. This directs to the correct exception code.
*
*********************************************************************************************/

PPCKERNEL void Exception_Entry(__reg("r3") struct PrivatePPCBase* PowerPCBase, __reg("r4") struct iframe* iframe)
{
    PowerPCBase->pp_ExceptionMode = -1;
    PowerPCBase->pp_Quantum = PowerPCBase->pp_StdQuantum;
    PowerPCBase->pp_CPUSDR1 = getSDR1();
    PowerPCBase->pp_CPUHID0 = getHID0();

    if ((PowerPCBase->pp_DeviceID != DEVICE_MPC8343E) && (PowerPCBase->pp_DeviceID != DEVICE_MPC8314E))
    {
       PowerPCBase->pp_L2State = getL2State();
    }

    ULONG ExceptionVector = iframe->if_Context.ec_ExcID - (PPC_VECLEN * OPCODE_LEN);
    iframe->if_ExcNum = ExceptionVector >> 8;
    iframe->if_Context.ec_ExcID = 1 << iframe->if_ExcNum;

    switch (ExceptionVector)
    {
        case VEC_EXTERNAL:
        {
            switch (PowerPCBase->pp_DeviceID)
	        {
		        case DEVICE_HARRIER:
		        {
			        if (readmemLongPPC(PPC_XCSR_BASE, XCSR_FEST) & XCSR_FEST_MIM0)
                    {
                        writememLongPPC(PPC_XCSR_BASE, XCSR_FECL, XCSR_FECL_MIM0);
                        CommonExcHandler(PowerPCBase, iframe, (struct List*)&PowerPCBase->pp_ExcInterrupt);
                    }

                    if (readmemLongPPC(PowerPCBase->pp_BridgeMPIC, XMPI_P0IAC) == 0xff)
                    {
                        PowerPCBase->pp_ExceptionMode = 0;
                        setDEC(PowerPCBase->pp_Quantum);
                        return;
                    }

                    struct MsgFrame* msgFrame;

                    while (msgFrame = KGetMsgFramePPC(PowerPCBase))
                    {
                        AddTailPPC((struct List*)&PowerPCBase->pp_MsgQueue , (struct Node*)msgFrame);
                    }
                    writememLongPPC(PowerPCBase->pp_BridgeMPIC, XMPI_P0EOI, 0);
                    break;
		        }

		        case DEVICE_MPC8343E:
                case DEVICE_MPC8314E:
		        {
                    if (loadPCI(IMMR_ADDR_DEFAULT, IMMR_IMISR) & IMMR_IMISR_IDI)
                    {
                        storePCI(IMMR_ADDR_DEFAULT, IMMR_IDR, 1);
                        CommonExcHandler(PowerPCBase, iframe, (struct List*)&PowerPCBase->pp_ExcInterrupt);
                    }
                    struct MsgFrame* msgFrame;

                    while (msgFrame = KGetMsgFramePPC(PowerPCBase))
                    {
                        AddTailPPC((struct List*)&PowerPCBase->pp_MsgQueue, (struct Node*)msgFrame);
                    }

                    if (loadPCI(IMMR_ADDR_DEFAULT, IMMR_IMISR) & IMMR_IMISR_IM0I)
                    {
                        storePCI(IMMR_ADDR_DEFAULT, IMMR_IMISR, IMMR_IMISR_IM0I);
                    }
                    break;
		        }

		        case DEVICE_MPC107:
		        {
                    if (loadPCI(PPC_EUMB_BASE, MPC107_IMISR) & MPC107_IMISR_IM0I)
                    {
                        storePCI(PPC_EUMB_BASE, MPC107_IMISR, (MPC107_IMISR_IM0I | MPC107_IMISR_IM1I));
                        CommonExcHandler(PowerPCBase, iframe, (struct List*)&PowerPCBase->pp_ExcInterrupt);
                    }

                    if ((readmemLongPPC(PPC_EUMB_EPICPROC, EPIC_IACK) >> 24) == 0xff)
                    {
                        PowerPCBase->pp_ExceptionMode = 0;
                        setDEC(PowerPCBase->pp_Quantum);
                        return;
                    }
                    struct MsgFrame* msgFrame;

                    while (msgFrame = KGetMsgFramePPC(PowerPCBase))
                    {
                        AddTailPPC((struct List*)&PowerPCBase->pp_MsgQueue, (struct Node*)msgFrame);
                    }
                    storePCI(PPC_EUMB_BASE, MPC107_IMISR, MPC107_IMISR_IPQI);
                    writememLongPPC(PPC_EUMB_EPICPROC, EPIC_EOI, 0);
			        break;
		        }
	        }

            CPUStats(PowerPCBase);

            ULONG currAddress = iframe->if_Context.ec_UPC.ec_SRR0;
            if ((PowerPCBase->pp_LowerLimit <= currAddress) && (currAddress < PowerPCBase->pp_UpperLimit))
            {
                PowerPCBase->pp_Quantum = 0x1000;
                break;
            }

            if ((PowerPCBase->pp_Mutex) || (PowerPCBase->pp_FlagForbid))
            {
                PowerPCBase->pp_Quantum = 0x1000;
                break;
            }

            struct TaskPPC* currTask = PowerPCBase->pp_ThisPPCProc;

            if (currTask)
            {
                if (currTask->tp_Task.tc_State == TS_ATOMIC)
                {
                    break;
                }
            }

            HandleMsgs(PowerPCBase);
            TaskCheck(PowerPCBase);
            SwitchPPC(PowerPCBase, iframe);
            break;
        }
        case VEC_DECREMENTER:
        {
            CommonExcHandler(PowerPCBase, iframe, (struct List*)&PowerPCBase->pp_ExcDecrementer);

            struct ExcData* eData;

            while (eData = (struct ExcData*)RemHeadPPC((struct List*)&PowerPCBase->pp_ReadyExc))
            {
                if (eData->ed_ExcID & EXCF_MCHECK)
                    EnqueuePPC((struct List*)&PowerPCBase->pp_ExcMCheck, (struct Node*)eData);
                if (eData->ed_ExcID & EXCF_DACCESS)
                    EnqueuePPC((struct List*)&PowerPCBase->pp_ExcDAccess, (struct Node*)eData);
                if (eData->ed_ExcID & EXCF_IACCESS)
                    EnqueuePPC((struct List*)&PowerPCBase->pp_ExcIAccess, (struct Node*)eData);
                if (eData->ed_ExcID & EXCF_INTERRUPT)
                    EnqueuePPC((struct List*)&PowerPCBase->pp_ExcInterrupt, (struct Node*)eData);
                if (eData->ed_ExcID & EXCF_ALIGN)
                    EnqueuePPC((struct List*)&PowerPCBase->pp_ExcAlign, (struct Node*)eData);
                if (eData->ed_ExcID & EXCF_PROGRAM)
                    EnqueuePPC((struct List*)&PowerPCBase->pp_ExcProgram, (struct Node*)eData);
                if (eData->ed_ExcID & EXCF_FPUN)
                    EnqueuePPC((struct List*)&PowerPCBase->pp_ExcFPUn, (struct Node*)eData);
                if (eData->ed_ExcID & EXCF_DECREMENTER)
                    EnqueuePPC((struct List*)&PowerPCBase->pp_ExcDecrementer, (struct Node*)eData);
                if (eData->ed_ExcID & EXCF_SC)
                    EnqueuePPC((struct List*)&PowerPCBase->pp_ExcSystemCall, (struct Node*)eData);
                if (eData->ed_ExcID & EXCF_TRACE)
                    EnqueuePPC((struct List*)&PowerPCBase->pp_ExcTrace, (struct Node*)eData);
                if (eData->ed_ExcID & EXCF_PERFMON)
                    EnqueuePPC((struct List*)&PowerPCBase->pp_ExcPerfMon, (struct Node*)eData);
                if (eData->ed_ExcID & EXCF_IABR)
                    EnqueuePPC((struct List*)&PowerPCBase->pp_ExcIABR, (struct Node*)eData);
                eData->ed_Flags |= EXCF_ACTIVE;
            }

            while (eData = (struct ExcData*)RemHeadPPC((struct List*)&PowerPCBase->pp_RemovedExc))
            {
                ULONG nodeAddr = (ULONG)&eData->ed_LastExc;
                for (int i=0; i<12; i++)
                {
                    nodeAddr += 4;
                    ULONG actNode = *((ULONG*)(nodeAddr));
                    if (actNode)
                    {
                        RemovePPC((struct Node*)actNode);
                    }
                }
                struct ExcData* lastExc = eData->ed_LastExc;
                lastExc->ed_Flags &= ~EXCF_ACTIVE;
            }

            CPUStats(PowerPCBase);

            ULONG currAddress = iframe->if_Context.ec_UPC.ec_SRR0;
            if ((PowerPCBase->pp_LowerLimit <= currAddress) && (currAddress < PowerPCBase->pp_UpperLimit))
            {
                PowerPCBase->pp_Quantum = 0x1000;
                break;
            }

            if ((PowerPCBase->pp_Mutex) || (PowerPCBase->pp_FlagForbid))
            {
                PowerPCBase->pp_Quantum = 0x1000;
                break;
            }

            struct TaskPPC* currTask = PowerPCBase->pp_ThisPPCProc;

            if (currTask)
            {
                if (currTask->tp_Task.tc_State == TS_ATOMIC)
                {
                    break;
                }
            }

            HandleMsgs(PowerPCBase);
            TaskCheck(PowerPCBase);
            SwitchPPC(PowerPCBase, iframe);
            break;
        }
        case VEC_DATASTORAGE:
        {
            if (PowerPCBase->pp_EnDAccessExc)  //not standard WOS behaviour
            {
                CommonExcHandler(PowerPCBase, iframe, (struct List*)&PowerPCBase->pp_ExcDAccess);
            }

            if ((iframe->if_Context.ec_DAR == 4) || ((iframe->if_Context.ec_DAR < 0xfffff800) && (iframe->if_Context.ec_DAR > 0x800)))
            {
                if (iframe->if_Context.ec_UPC.ec_SRR0 > 0x10000)
                {
                    if(!(PowerPCBase->pp_DataExcLow += 1))
                    {
                        PowerPCBase->pp_DataExcHigh += 1;
                    }
                    struct DataMsg myData;
                    ULONG Status = DoDataStore(iframe, iframe->if_Context.ec_UPC.ec_SRR0, &myData);
                    if (Status)
                    {
                        struct MsgFrame* myFrame = KCreateMsgFramePPC(PowerPCBase);
                        myFrame->mf_Identifier = myData.dm_Type;
                        myFrame->mf_Arg[1] = myData.dm_Address;
                        myFrame->mf_Arg[0] = myData.dm_Value;
                        KSendMsgFramePPC(PowerPCBase, myFrame);
                        if (!(myData.dm_LoadFlag))
                        {
                            while (myFrame->mf_Identifier != ID_DONE);
#ifdef DEBUG
                            if (myFrame->mf_Arg[0] == ERR_EMEM)
                            {
                                CommonExcError(PowerPCBase, iframe);
                                break;
                            }
#endif
                            if (!(FinDataStore(myFrame->mf_Arg[0], iframe, iframe->if_Context.ec_UPC.ec_SRR0, &myData)))
                            {
                                CommonExcError(PowerPCBase, iframe);
                                break;
                            }
                        }
                        iframe->if_Context.ec_UPC.ec_SRR0  += 4;
                        //PowerPCBase->pp_Quantum = getDEC() + 1000; //Do not start quantum all over
                        break;
                    }
                }
            }
            CommonExcError(PowerPCBase, iframe);
            break;
        }
        case VEC_PROGRAM:
        {
            if (iframe->if_Context.ec_GPR[4] == SUPERKEY)
            {
                iframe->if_Context.ec_UPC.ec_SRR0  += 4;
                iframe->if_Context.ec_SRR1         &= ~PSL_PR;
                iframe->if_Context.ec_GPR[3]        = 0;            //Set SuperKey
                //PowerPCBase->pp_Quantum = getDEC() + 1000;
            }
            else
            {
                CommonExcHandler(PowerPCBase, iframe, (struct List*)&PowerPCBase->pp_ExcProgram);
            }
            break;
        }
        case VEC_ALIGNMENT:
        {

            if ((PowerPCBase->pp_EnAlignExc) || (iframe->if_Context.ec_DAR < 0x200000))
            {
                CommonExcHandler(PowerPCBase, iframe, (struct List*)&PowerPCBase->pp_ExcAlign);
            }
            else
            {
                if (DoAlign(iframe, iframe->if_Context.ec_UPC.ec_SRR0))
                {
                    iframe->if_Context.ec_UPC.ec_SRR0  += 4;
                    if (!(PowerPCBase->pp_AlignmentExcLow += 1))
                    {
                        PowerPCBase->pp_AlignmentExcHigh += 1;
                    }
                    //PowerPCBase->pp_Quantum = getDEC() + 1000;
                }
                else
                {
                    CommonExcHandler(PowerPCBase, iframe, (struct List*)&PowerPCBase->pp_ExcAlign);
                }
            }
            break;
        }
        case VEC_ALTIVECUNAV:
        {
            iframe->if_Context.ec_ExcID = EXCF_ALTIVECUNAV;
            iframe->if_ExcNum = EXCB_ALTIVECUNAV;
            if (PowerPCBase->pp_EnAltivec)
            {
                iframe->if_Context.ec_SRR1 |= PSL_VEC;
            }
            else
            {
                CommonExcError(PowerPCBase, iframe);
            }
            break;
        }
        case VEC_MACHINECHECK:
        {
            CommonExcHandler(PowerPCBase, iframe, (struct List*)&PowerPCBase->pp_ExcMCheck);
            break;
        }
        case VEC_INSTSTORAGE:
        {
            CommonExcHandler(PowerPCBase, iframe, (struct List*)&PowerPCBase->pp_ExcIAccess);
            break;
        }
        case VEC_FPUNAVAILABLE:
        {
            CommonExcHandler(PowerPCBase, iframe, (struct List*)&PowerPCBase->pp_ExcFPUn);
            break;
        }
        case VEC_SYSTEMCALL:
        {
            struct SysCall* sc = (struct SysCall*)iframe->if_Context.ec_GPR[3]; //no checking of valid address

            switch (sc->sc_Function)
            {
                case SC_CREATEMSG:
                {
                    iframe->if_Context.ec_GPR[3] = (ULONG)KCreateMsgFramePPC(PowerPCBase);
                    break;
                }
                case SC_GETMSG:
                {
                    iframe->if_Context.ec_GPR[3] = (ULONG)KGetMsgFramePPC(PowerPCBase);
                    break;
                }
                case SC_SENDMSG:
                {
                    KSendMsgFramePPC(PowerPCBase, (struct MsgFrame*)sc->sc_Arg[0]);
                    break;
                }
                case SC_FREEMSG:
                {
                    KFreeMsgFramePPC(PowerPCBase, (struct MsgFrame*)sc->sc_Arg[0]);
                    break;
                }
                case SC_SETCACHE:
                {
                    KSetCache(PowerPCBase, sc->sc_Arg[0], (APTR)sc->sc_Arg[1], sc->sc_Arg[2]);
                    break;
                }
                default:
                {
                    CommonExcHandler(PowerPCBase, iframe, (struct List*)&PowerPCBase->pp_ExcSystemCall);
                    break;
                }
            }
            break;
        }
        case VEC_TRACE:
        {
            CommonExcHandler(PowerPCBase, iframe, (struct List*)&PowerPCBase->pp_ExcTrace);
            break;
        }
        case VEC_PERFMONITOR:
        {
            CommonExcHandler(PowerPCBase, iframe, (struct List*)&PowerPCBase->pp_ExcPerfMon);
            break;
        }
        case VEC_IBREAKPOINT:
        {
            CommonExcHandler(PowerPCBase, iframe, (struct List*)&PowerPCBase->pp_ExcIABR);
            break;
        }
        default:
        {
            CommonExcError(PowerPCBase, iframe);
        }
    }
    PowerPCBase->pp_ExceptionMode = 0;
    setDEC(PowerPCBase->pp_Quantum);
    return;
}

/********************************************************************************************
*
*
*
*********************************************************************************************/

PPCFUNCTION void TaskCheck(__reg("r3") struct PrivatePPCBase* PowerPCBase)
{
    ULONG mask;
    struct TaskPPC* currTask = PowerPCBase->pp_ThisPPCProc;

    if(currTask)
    {
        if ((PowerPCBase->pp_TaskExcept) && (PowerPCBase->pp_TaskExcept == currTask) &&
            (currTask->tp_Task.tc_ExceptData) && (mask = currTask->tp_Task.tc_SigRecvd & currTask->tp_Task.tc_SigExcept))
        {
            currTask->tp_Task.tc_SigRecvd &= ~mask;
            currTask->tp_Task.tc_SigExcept &= ~mask;

            ULONG (*ExcHandler)(__reg("r2") ULONG, __reg("r3") ULONG) = currTask->tp_Task.tc_ExceptCode;
            ULONG tempR2 = getR2();
            ULONG signal = ExcHandler((ULONG)currTask->tp_Task.tc_ExceptData, mask);
            storeR2(tempR2);

            currTask->tp_Task.tc_SigExcept |= signal;
        }
    }

    if (!(PowerPCBase->pp_FlagWait))
    {
        struct WaitTime* currWait = (struct WaitTime*)PowerPCBase->pp_WaitTime.mlh_Head;
        struct WaitTime* nxtWait;

        while (nxtWait = (struct WaitTime*)currWait->wt_Node.ln_Succ)
        {
            if ((currWait->wt_TimeUpper < getTBU()) || ((currWait->wt_TimeUpper == getTBU()) && (currWait->wt_TimeLower < getTBL())))
            {
                if ((currTask) && (currTask == currWait->wt_Task))
                {
                    currWait->wt_Task->tp_Task.tc_State = TS_RUN;
                }
                else
                {
                    currWait->wt_Task->tp_Task.tc_State = TS_READY;
                }
                currWait->wt_Task->tp_Task.tc_SigRecvd |= SIGF_WAIT;
                RemovePPC((struct Node*)currWait);
            }
            currWait = nxtWait;
        }
    }

    struct TaskPPC* currWTask = (struct TaskPPC*)PowerPCBase->pp_WaitingTasks.mlh_Head;
    struct TaskPPC* nxtWTask;

    while (nxtWTask = (struct TaskPPC*)currWTask->tp_Task.tc_Node.ln_Succ)
    {
        if ((currWTask->tp_Task.tc_State == TS_READY) || (currWTask->tp_Task.tc_SigWait & currWTask->tp_Task.tc_SigRecvd))
        {
            currWTask->tp_Task.tc_State = TS_READY;
            RemovePPC((struct Node*)currWTask);
            AddTailPPC((struct List*)&PowerPCBase->pp_ReadyTasks, (struct Node*)currWTask); //Enqueue on pri?
        }
        currWTask = nxtWTask;
    }
    return;
}

/********************************************************************************************
*
*	This function dispatches new PPC tasks, switches between tasks and redirect signals.
*
*********************************************************************************************/

PPCKERNEL void HandleMsgs(__reg("r3") struct PrivatePPCBase* PowerPCBase)
{
    struct MsgFrame* currMsg = (struct MsgFrame*)PowerPCBase->pp_MsgQueue.mlh_Head;
    struct MsgFrame* nxtMsg;
    while (nxtMsg = (struct MsgFrame*)currMsg->mf_Message.mn_Node.ln_Succ)
    {
        switch (currMsg->mf_Identifier)
        {
            case ID_TPPC:
            case ID_DONE:
            case ID_END:
            case ID_DNLL:
            {
                struct TaskPPC* myTask = currMsg->mf_PPCTask;
                if ((currMsg->mf_Identifier == ID_TPPC) && (!(myTask)))
                {
                    RemovePPC((struct Node*)currMsg);
                    AddTailPPC((struct List*)&PowerPCBase->pp_NewTasks, (struct Node*)currMsg);
                }
                else
                {
                    struct MsgPortPPC* myPort = myTask->tp_Msgport;
                    if (myPort->mp_Semaphore.ssppc_SS.ss_QueueCount == -1)
                    {
                        RemovePPC((struct Node*)currMsg);

                        ULONG signal = 1 << (myPort->mp_Port.mp_SigBit);
                        myTask->tp_Task.tc_SigRecvd |= signal;

                        AddTailPPC(&myPort->mp_Port.mp_MsgList, (struct Node*)currMsg);
                        if (currMsg->mf_Identifier == ID_DONE)
                        {
                            myTask->tp_Task.tc_SigAlloc = currMsg->mf_Arg[1];
                        }
                        if (myTask == PowerPCBase->pp_ThisPPCProc)
                        {
                            myTask->tp_Task.tc_State = TS_RUN;
                        }
                        else
                        {
                            myTask->tp_Task.tc_State = TS_READY;
                        }
                    }
                }
                break;
            }
            case ID_XMSG:
            {
                struct MsgPortPPC* myPort = (struct MsgPortPPC*)currMsg->mf_Message.mn_ReplyPort;
                if (myPort->mp_Semaphore.ssppc_SS.ss_QueueCount != -1)
                {
                    break;
                }

                struct MsgFrame* oldMsg = (struct MsgFrame*)currMsg->mf_Arg[0];

                RemovePPC((struct Node*)currMsg);
                KFreeMsgFramePPC(PowerPCBase, currMsg);

                myPort = (struct MsgPortPPC*)oldMsg->mf_Message.mn_ReplyPort;
                struct TaskPPC* sigTask = (struct TaskPPC*)myPort->mp_Port.mp_SigTask;

                if (!(sigTask))
                {
                    break;
                }

                ULONG signal = 1 << (myPort->mp_Port.mp_SigBit);
                sigTask->tp_Task.tc_SigRecvd |= signal;
                AddTailPPC(&myPort->mp_Port.mp_MsgList, (struct Node*)oldMsg);

                if (sigTask == PowerPCBase->pp_ThisPPCProc)
                {
                    sigTask->tp_Task.tc_State = TS_RUN;
                }
                else
                {
                    sigTask->tp_Task.tc_State = TS_READY;
                }
                break;
            }
            case ID_XPPC:
            {
                struct MsgPortPPC* myPort = (struct MsgPortPPC*)currMsg->mf_Arg[0];
                if (myPort->mp_Semaphore.ssppc_SS.ss_QueueCount != -1)
                {
                    break;
                }

                struct Node* xMsg = (struct Node*)currMsg->mf_Arg[1];

                RemovePPC((struct Node*)currMsg);
                KFreeMsgFramePPC(PowerPCBase, currMsg);

                struct TaskPPC* sigTask = (struct TaskPPC*)myPort->mp_Port.mp_SigTask;

                if (!(sigTask))
                {
                    break;
                }

                ULONG signal = 1 << (myPort->mp_Port.mp_SigBit);
                sigTask->tp_Task.tc_SigRecvd |= signal;
                AddTailPPC(&myPort->mp_Port.mp_MsgList, xMsg);

                if (sigTask == PowerPCBase->pp_ThisPPCProc)
                {
                    sigTask->tp_Task.tc_State = TS_RUN;
                }
                else
                {
                    sigTask->tp_Task.tc_State = TS_READY;
                }
                break;
            }
            case ID_LLPP:
            {
                struct PrivateTask* myTask = (struct PrivateTask*)PowerPCBase->pp_ThisPPCProc;

                if ((myTask) && (myTask->pt_Mirror68K == (struct Task*)currMsg->mf_Arg[0]))
                {
                    myTask->pt_Task.tp_Task.tc_State = TS_RUN;
                    myTask->pt_Task.tp_Task.tc_SigRecvd |= currMsg->mf_Signals;
                }
                else
                {
                    struct PrivateTask* nxtTask;
                    struct PrivateTask* currTask = (struct PrivateTask*)PowerPCBase->pp_WaitingTasks.mlh_Head;
                    while (nxtTask = (struct PrivateTask*)currTask->pt_Task.tp_Task.tc_Node.ln_Succ)
                    {
                        if (currTask->pt_Mirror68K == (struct Task*)currMsg->mf_Arg[0])
                        {
                            currTask->pt_Task.tp_Task.tc_State = TS_READY;
                            currTask->pt_Task.tp_Task.tc_SigRecvd |= currMsg->mf_Signals;
                            break;
                        }
                        currTask = nxtTask;
                    }
                    if (!(nxtTask))
                    {
                        currTask = (struct PrivateTask*)PowerPCBase->pp_ReadyTasks.mlh_Head;
                        while (nxtTask = (struct PrivateTask*)currTask->pt_Task.tp_Task.tc_Node.ln_Succ)
                        {
                            if (currTask->pt_Mirror68K == (struct Task*)currMsg->mf_Arg[0])
                            {
                                currTask->pt_Task.tp_Task.tc_SigRecvd |= currMsg->mf_Signals;
                                break;
                            }
                            currTask = nxtTask;
                        }
                    }
                }
                RemovePPC((struct Node*)currMsg);
                KFreeMsgFramePPC(PowerPCBase, currMsg);
                break;
            }
            default:
            {
                RemovePPC((struct Node*)currMsg);
                KFreeMsgFramePPC(PowerPCBase, currMsg);
            }
        }
        currMsg = nxtMsg;
    }
    return;
}

/********************************************************************************************
*
*
*
*********************************************************************************************/

PPCKERNEL void SwitchPPC(__reg("r3") struct PrivatePPCBase* PowerPCBase, __reg("r4") struct iframe* iframe)
{
    struct TaskPPC* currTask = PowerPCBase->pp_ThisPPCProc;
    while (1)
    {
        if (!(currTask))
        {
            if (currTask = (struct TaskPPC*)RemHeadPPC((struct List*)&PowerPCBase->pp_NewTasks))
            {
                DispatchPPC(PowerPCBase, (struct MsgFrame*)currTask);
                break;
            }
            if (currTask = (struct TaskPPC*)RemHeadPPC((struct List*)&PowerPCBase->pp_ReadyTasks))
            {
                currTask->tp_Task.tc_State = TS_RUN;
                PowerPCBase->pp_ThisPPCProc = currTask;
                iframe = (struct iframe*)currTask->tp_ContextMem;
                currTask->tp_Task.tc_SPReg = (APTR)iframe->if_Context.ec_GPR[1];
                break;
            }
            break;
        }
        if (currTask->tp_Task.tc_State == TS_REMOVED)
        {
            AddTailPPC((struct List*)&PowerPCBase->pp_RemovedTasks, (struct Node*)currTask);
            currTask = NULL;
            PowerPCBase->pp_ThisPPCProc = currTask;
            iframe->if_Context.ec_SRR1 = MACHINESTATE_DEFAULT;
            iframe->if_Context.ec_UPC.ec_SRR0 = (PowerPCBase->pp_PPCMemBase) + OFFSET_SYSMEM;
        }
        else if (currTask->tp_Task.tc_State == TS_CHANGING)
        {
            currTask->tp_Task.tc_State = TS_WAIT;
            AddTailPPC((struct List*)&PowerPCBase->pp_WaitingTasks, (struct Node*)currTask);
            currTask = NULL;
            PowerPCBase->pp_ThisPPCProc = currTask;
        }
        else
        {
            if (currTask = (struct TaskPPC*)RemHeadPPC((struct List*)&PowerPCBase->pp_NewTasks))
            {
                struct TaskPPC* oldTask = PowerPCBase->pp_ThisPPCProc;
                oldTask->tp_Task.tc_State = TS_READY;
                AddTailPPC((struct List*)&PowerPCBase->pp_ReadyTasks, (struct Node*)oldTask);
                DispatchPPC(PowerPCBase, (struct MsgFrame*)currTask);
            }
            else if (currTask = (struct TaskPPC*)RemHeadPPC((struct List*)&PowerPCBase->pp_ReadyTasks))
            {
                struct TaskPPC* oldTask = PowerPCBase->pp_ThisPPCProc;
                oldTask->tp_Task.tc_State = TS_READY;
                currTask->tp_Task.tc_State = TS_RUN;
                AddTailPPC((struct List*)&PowerPCBase->pp_ReadyTasks, (struct Node*)oldTask);
                PowerPCBase->pp_ThisPPCProc = currTask;
                iframe = (struct iframe*)currTask->tp_ContextMem;
                currTask->tp_Task.tc_SPReg = (APTR)iframe->if_Context.ec_GPR[1];
            }
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

PPCKERNEL void DispatchPPC(__reg("r3") struct PrivatePPCBase* PowerPCBase, __reg("r4") struct MsgFrame* myFrame)
{
    struct NewTask* newTask = (struct NewTask*)myFrame->mf_Arg[0];

    PowerPCBase->pp_IdUsrTasks += 1;

    newTask->nt_Task.pt_Task.tp_Id = PowerPCBase->pp_IdUsrTasks;
    newTask->nt_Task.pt_Task.tp_PowerPCBase = PowerPCBase;
    newTask->nt_Task.pt_Task.tp_Task.tc_Node.ln_Type = NT_PPCTASK;
    newTask->nt_Task.pt_Task.tp_Task.tc_State = TS_RUN;
    newTask->nt_Task.pt_Task.tp_ContextMem = &newTask->nt_Context;
    newTask->nt_Task.pt_Task.tp_BATStorage = &newTask->nt_BatStore;
    newTask->nt_Task.pt_Task.tp_Link.tl_Task = newTask;
    newTask->nt_Task.pt_Task.tp_Link.tl_Sig = 0xfff;
    newTask->nt_Task.pt_Task.tp_StackMem = (APTR)(myFrame->mf_Arg[0] + 2048);
    newTask->nt_Task.pt_Mirror68K = (struct Task*)myFrame->mf_Arg[2];
    newTask->nt_Task.pt_MirrorPort = myFrame->mf_MirrorPort;
    newTask->nt_Task.pt_Task.tp_Task.tc_Node.ln_Name = (APTR)&newTask->nt_Name;
    newTask->nt_Task.pt_Task.tp_StackSize = (ULONG)myFrame->mf_Message.mn_Node.ln_Name;
    newTask->nt_Task.pt_Task.tp_Task.tc_SPLower = newTask->nt_Task.pt_Task.tp_StackMem;
    newTask->nt_Task.pt_Task.tp_Task.tc_SPUpper = (APTR)((ULONG)newTask->nt_Task.pt_Task.tp_StackMem + newTask->nt_Task.pt_Task.tp_StackSize);
    newTask->nt_Task.pt_Task.tp_Task.tc_SPReg   = (APTR)((ULONG)newTask->nt_Task.pt_Task.tp_Task.tc_SPUpper - 32);

    NewListPPC((struct List*)&newTask->nt_Task.pt_Task.tp_Task.tc_MemEntry);
    newTask->nt_Task.pt_Task.tp_Task.tc_SigAlloc = myFrame->mf_Arg[1];
    PowerPCBase->pp_ThisPPCProc = (struct TaskPPC*)newTask;

    newTask->nt_Task.pt_Task.tp_TaskPtr = &newTask->nt_TaskPtr;
    newTask->nt_TaskPtr.tptr_Task = (struct TaskPPC*)newTask;
    newTask->nt_TaskPtr.tptr_Node.ln_Name = newTask->nt_Task.pt_Task.tp_Task.tc_Node.ln_Name;
    AddTailPPC((struct List*)&PowerPCBase->pp_AllTasks, (struct Node*)&newTask->nt_TaskPtr);

    PowerPCBase->pp_NumAllTasks += 1;

    NewListPPC((struct List*)&newTask->nt_Task.pt_Task.tp_TaskPools);
    NewListPPC((struct List*)&newTask->nt_Port.mp_IntMsg);
    NewListPPC((struct List*)&newTask->nt_Port.mp_Port.mp_MsgList);
    newTask->nt_Port.mp_Port.mp_SigBit = SIGB_DOS;
    NewListPPC((struct List*)&newTask->nt_Port.mp_Semaphore.ssppc_SS.ss_WaitQueue);
    newTask->nt_Port.mp_Semaphore.ssppc_SS.ss_Owner = NULL;
    newTask->nt_Port.mp_Semaphore.ssppc_SS.ss_NestCount = 0;
    newTask->nt_Port.mp_Semaphore.ssppc_SS.ss_QueueCount = -1;
    newTask->nt_Port.mp_Semaphore.ssppc_reserved = &newTask->nt_SSReserved1;
    newTask->nt_Port.mp_Port.mp_SigTask = newTask;
    newTask->nt_Port.mp_Port.mp_Flags = PA_SIGNAL;
    newTask->nt_Port.mp_Port.mp_Node.ln_Type = NT_MSGPORTPPC;
    newTask->nt_Task.pt_Task.tp_Msgport = &newTask->nt_Port;

    struct iframe* newFrame = (struct iframe*)&newTask->nt_Context;

    newFrame->if_Context.ec_SRR1 = MACHINESTATE_DEFAULT;
    newFrame->if_Context.ec_UPC.ec_SRR0 = *((ULONG*)((ULONG)PowerPCBase + 2 + _LVOStartTask));
    newFrame->if_Context.ec_GPR[1] = (ULONG)newTask->nt_Task.pt_Task.tp_Task.tc_SPReg;
    newFrame->if_Context.ec_GPR[2] = NULL;
    newFrame->if_Context.ec_GPR[3] = (ULONG)PowerPCBase;
    newFrame->if_Context.ec_GPR[4] = (ULONG)myFrame;

    ULONG* EndofStack = (APTR)newFrame->if_Context.ec_GPR[1];
    EndofStack[0] = 0; //Clear end of stack chain for SwapStack

    CopyMemPPC(&PowerPCBase->pp_SystemBATs, &newFrame->if_BATs, sizeof(struct BATArray) * 4);
    CopyMemPPC(&PowerPCBase->pp_SystemBATs, newTask->nt_Task.pt_Task.tp_BATStorage, sizeof(struct BATArray) * 4);

    for (int i = 0; i < 16; i++)
    {
        newFrame->if_Segments[i] = PowerPCBase->pp_SystemSegs[i];
    }

    return;
}

/********************************************************************************************
*
*
*
*********************************************************************************************/

PPCKERNEL void CommonExcHandler(__reg("r3") struct PrivatePPCBase* PowerPCBase, __reg("r4") struct iframe* iframe, __reg("r5") struct List* excList)
{
    struct ExcData* nxtNode;
    struct ExcData* currNode = (struct ExcData*)excList->lh_Head;
    ULONG status = EXCRETURN_NORMAL;

    while (nxtNode = (struct ExcData*)currNode->ed_Node.ln_Succ)
    {
        if (currNode->ed_ExcID & iframe->if_Context.ec_ExcID)
        {
            if ((currNode->ed_Flags & EXCF_GLOBAL) || ((currNode->ed_Flags & EXCF_LOCAL) && (currNode->ed_Task) && (currNode->ed_Task == PowerPCBase->pp_ThisPPCProc)))
            {
                if (currNode->ed_Flags & EXCF_LARGECONTEXT)
                {
                    ULONG (*ExcHandler)(__reg("r2") ULONG, __reg("r3") struct EXCContext*) = currNode->ed_Code;
                    ULONG tempR2 = getR2();
                    status = ExcHandler(currNode->ed_Data, &iframe->if_Context);
                    storeR2(tempR2);
                }
                else if (currNode->ed_Flags & EXCF_SMALLCONTEXT)
                {
                    status = SmallExcHandler(currNode, iframe);
                }
                if (status == EXCRETURN_ABORT)
                {
                    break;
                }
            }
        }
        currNode = nxtNode;
    }

    if ((status == EXCRETURN_NORMAL) && (iframe->if_Context.ec_ExcID != EXCF_DACCESS))
    {
        CommonExcError(PowerPCBase, iframe);
    }
    return;
}

#if 0
/********************************************************************************************
*
*
*
*********************************************************************************************/

PPCKERNEL VOID writememLongPPC(__reg("r3") ULONG Base, __reg("r4") ULONG offset, __reg("r5") ULONG value)
{
	*((ULONG*)(Base + offset)) = value;
	return;
}

/********************************************************************************************
*
*
*
*********************************************************************************************/

PPCKERNEL ULONG readmemLongPPC(__reg("r3") ULONG Base, __reg("r4") ULONG offset)
{
    ULONG res;
    res = *((ULONG*)(Base + offset));
    return res;
}
#endif
/********************************************************************************************
*
*
*
*********************************************************************************************/

PPCKERNEL struct MsgFrame* KCreateMsgFramePPC(__reg("r3") struct PrivatePPCBase* PowerPCBase)
{
	ULONG msgFrame = 0;

    switch (PowerPCBase->pp_DeviceID)
	{
		case DEVICE_HARRIER:
		{
			ULONG msgOffset = readmemLongPPC(PPC_XCSR_BASE, XCSR_MIOFT);
            msgFrame = readmemLongPPC(msgOffset, 0);
            writememLongPPC(PPC_XCSR_BASE, XCSR_MIOFT, (msgOffset + 4) & 0xffff3fff);
            break;
		}

		case DEVICE_MPC8343E:
        case DEVICE_MPC8314E:
		{
			struct killFIFO* myFIFO = (struct killFIFO*)((ULONG)(PowerPCBase->pp_PPCMemBase + FIFO_END));
			msgFrame = *((ULONG*)(myFIFO->kf_MIOFT));
			myFIFO->kf_MIOFT = (myFIFO->kf_MIOFT + 4) & 0xffff3fff;
			break;
		}

		case DEVICE_MPC107:
		{
			ULONG msgOffset = loadPCI(PPC_EUMB_BASE, MPC107_OFTPR);
            msgFrame = readmemLongPPC(msgOffset, 0);
            storePCI(PPC_EUMB_BASE, MPC107_OFTPR, (((msgOffset + 4) | 0xc000) & 0xffff));
            break;
		}

		default:
		{
			break;
		}
	}
    return (struct MsgFrame*)msgFrame;
}

/********************************************************************************************
*
*
*
*********************************************************************************************/

PPCKERNEL struct MsgFrame* KGetMsgFramePPC(__reg("r3") struct PrivatePPCBase* PowerPCBase)
{
    ULONG msgFrame = 0;

    switch (PowerPCBase->pp_DeviceID)
	{
		case DEVICE_HARRIER:
		{
			ULONG msgOffset1 = readmemLongPPC(PPC_XCSR_BASE, XCSR_MIIPH);
            ULONG msgOffset2 = readmemLongPPC(PPC_XCSR_BASE, XCSR_MIIPT);
            if (msgOffset1 != msgOffset2)
            {
                writememLongPPC(PPC_XCSR_BASE, XCSR_MIIPT, (msgOffset2 + 4) & 0xffff3fff);
                msgFrame = readmemLongPPC(msgOffset2, 0);
            }
            break;
		}

		case DEVICE_MPC8343E:
        case DEVICE_MPC8314E:
		{
			struct killFIFO* myFIFO = (struct killFIFO*)((ULONG)(PowerPCBase->pp_PPCMemBase + FIFO_END));
			if (myFIFO->kf_MIIPH != myFIFO->kf_MIIPT)
            {
                msgFrame = *((ULONG*)(myFIFO->kf_MIIPT));
			    myFIFO->kf_MIIPT = (myFIFO->kf_MIIPT + 4) & 0xffff3fff;
            }
			break;
		}

		case DEVICE_MPC107:
		{
            ULONG msgOffset1 = loadPCI(PPC_EUMB_BASE, MPC107_IPHPR);
            ULONG msgOffset2 = loadPCI(PPC_EUMB_BASE, MPC107_IPTPR);
            if (msgOffset1 != msgOffset2)
            {
                storePCI(PPC_EUMB_BASE, MPC107_IPTPR, ((msgOffset2 + 4) | 0x4000) & 0xffff7fff);
                msgFrame = readmemLongPPC(msgOffset2, 0);
            }
			break;
		}

		default:
		{
			break;
		}
	}
	return (struct MsgFrame*)msgFrame;
}

/********************************************************************************************
*
*
*
*********************************************************************************************/

PPCKERNEL VOID KSendMsgFramePPC(__reg("r3") struct PrivatePPCBase* PowerPCBase, __reg("r4") struct MsgFrame* msgFrame)
{
	switch (PowerPCBase->pp_DeviceID)
	{
		case DEVICE_HARRIER:
		{
            ULONG msgOffset = readmemLongPPC(PPC_XCSR_BASE, XCSR_MIOPH);
            writememLongPPC(msgOffset, 0, (ULONG)msgFrame);
            writememLongPPC(PPC_XCSR_BASE, XCSR_MIOPH, (msgOffset + 4) & 0xffff3fff);
			break;
		}

		case DEVICE_MPC8343E:
        case DEVICE_MPC8314E:
		{
            struct killFIFO* myFIFO = (struct killFIFO*)((ULONG)(PowerPCBase->pp_PPCMemBase + FIFO_END));
			*((ULONG*)(myFIFO->kf_MIOPH)) = (ULONG)msgFrame;
			myFIFO->kf_MIOPH = (myFIFO->kf_MIOPH + 4) & 0xffff3fff;
			storePCI(IMMR_ADDR_DEFAULT, IMMR_OMR0, (ULONG)msgFrame);
            break;
		}

		case DEVICE_MPC107:
		{
            ULONG msgOffset = loadPCI(PPC_EUMB_BASE, MPC107_OPHPR);
            writememLongPPC(msgOffset, 0, (ULONG)msgFrame);
            storePCI(PPC_EUMB_BASE, MPC107_OPHPR, ((msgOffset + 4) & 0xbfff));
			break;
		}

		default:
		{
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

PPCKERNEL VOID KFreeMsgFramePPC(__reg("r3") struct PrivatePPCBase* PowerPCBase, __reg("r4") struct MsgFrame* msgFrame)
{
    //msgFrame->mf_Identifier = ID_FREE;

	switch (PowerPCBase->pp_DeviceID)
	{
		case DEVICE_HARRIER:
		{
            ULONG msgOffset = readmemLongPPC(PPC_XCSR_BASE, XCSR_MIIFH);
            writememLongPPC(msgOffset, 0, (ULONG)msgFrame);
            writememLongPPC(PPC_XCSR_BASE, XCSR_MIIFH, (msgOffset + 4) & 0xffff3fff);
            break;
		}

		case DEVICE_MPC8343E:
        case DEVICE_MPC8314E:
		{
			struct killFIFO* myFIFO = (struct killFIFO*)((ULONG)(PowerPCBase->pp_PPCMemBase + FIFO_END));
			*((ULONG*)(myFIFO->kf_MIIFH)) = (ULONG)msgFrame;
			myFIFO->kf_MIIFH = (myFIFO->kf_MIIFH + 4) & 0xffff3fff;
			break;
		}

		case DEVICE_MPC107:
		{
            ULONG msgOffset = loadPCI(PPC_EUMB_BASE, MPC107_IFHPR);
            writememLongPPC(msgOffset , 0, (ULONG)msgFrame);
            storePCI(PPC_EUMB_BASE, MPC107_IFHPR, (msgOffset + 4) & 0x3fff);
			break;
		}

		default:
		{
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

PPCKERNEL VOID FlushDCache(__reg("r3") struct PrivatePPCBase* PowerPCBase)
{
    ULONG cacheSize;

    if (PowerPCBase->pp_CacheDisDFlushAll)
    {
        cacheSize = 0;
    }
    else
    {
        cacheSize = PowerPCBase->pp_CurrentL2Size;
    }

    cacheSize = (cacheSize >> 5) + CACHE_L1SIZE;
    //ULONG mem = ((PowerPCBase->pp_PPCMemSize - 0x400000) + PowerPCBase->pp_PPCMemBase);

    ULONG mem = 0;

    ULONG mem2 = mem;

    for (int i = 0; i < cacheSize; i++)
    {
        loadWord(mem);
        mem += CACHELINE_SIZE;
    }

    for (int i = 0; i < cacheSize; i++)
    {
        dFlush(mem2);
        mem2 += CACHELINE_SIZE;
    }

    return;
}

/********************************************************************************************
*
*
*
*********************************************************************************************/

PPCKERNEL VOID KSetCache(__reg("r3") struct PrivatePPCBase* PowerPCBase, __reg("r4") ULONG flags, __reg("r5") APTR start, __reg("r6") ULONG length)
{
    ULONG value;

    switch (flags)
    {
        case CACHE_DCACHEOFF:
        {
            if (!(PowerPCBase->pp_CacheDState))
            {
                FlushDCache(PowerPCBase);

                value = getHID0();
                value &= ~HID0_DCE;
                setHID0(value);
                PowerPCBase->pp_CacheDState = -1;
            }
            break;
        }
        case CACHE_DCACHEON:
        {
            PowerPCBase->pp_CacheDState = 0;
            value = getHID0();
            value |= HID0_DCE;
            setHID0(value);
            break;
        }
        case CACHE_DCACHELOCK:
        {
            if ((start) && (length) && !(PowerPCBase->pp_CacheDLockState))
            {
                FlushDCache(PowerPCBase);
                ULONG iterations = (ULONG)start + length;
                ULONG mask = -32;
                start = (APTR)((ULONG)start & mask);
                ULONG mem = (ULONG)start;
                iterations = (((iterations + 31) & mask) - (ULONG)start) >> 5;

                for (int i = 0; i < iterations; i++)
                {
                    loadWord(mem);
                    mem += CACHELINE_SIZE;
                }

                value = getHID0();
                value |= HID0_DLOCK;
                setHID0(value);
                PowerPCBase->pp_CacheDLockState = -1;
            }
            break;
        }
        case CACHE_DCACHEUNLOCK:
        {
            PowerPCBase->pp_CacheDLockState = 0;
            value = getHID0();
            value &= ~HID0_DLOCK;
            setHID0(value);
            break;
        }
        case CACHE_DCACHEFLUSH:
        {
            if (!(PowerPCBase->pp_CacheDState) && !(PowerPCBase->pp_CacheDLockState))
            {
                if (PowerPCBase->pp_L2Size)
                {
                    if ((start) && (length))
                    {
                        ULONG iterations = (ULONG)start + length;
                        ULONG mask = -32;
                        start = (APTR)((ULONG)start & mask);
                        ULONG mem = (ULONG)start;
                        iterations = (((iterations + 31) & mask) - (ULONG)start) >> 5;

                        for (int i = 0; i < iterations; i++)
                        {
                            dFlush(mem);
                            mem += CACHELINE_SIZE;
                        }
                        sync();
                    }
                    else
                    {
                        FlushDCache(PowerPCBase);
                    }
                }
                else
                {
                    ULONG mem = 0;
                    for (int i = 0; i < CACHE_L1SIZE; i++)
                    {
                        loadWord(mem);
                        mem += CACHELINE_SIZE;
                    }
                }
            }
            break;
        }
        case CACHE_ICACHEOFF:
        {
            value = getHID0();
            value &= ~HID0_ICE;
            setHID0(value);
            break;
        }
        case CACHE_ICACHEON:
        {
            value = getHID0();
            value |= HID0_ICE;
            setHID0(value);
            break;
        }
        case CACHE_ICACHELOCK:
        {
            value = getHID0();
            value |= HID0_ILOCK;
            setHID0(value);
            break;
        }
        case CACHE_ICACHEUNLOCK:
        {
            value = getHID0();
            value &= ~HID0_ILOCK;
            setHID0(value);
            break;
        }
        case CACHE_DCACHEINV:
        {
            if ((start) && (length))
            {
                if (PowerPCBase->pp_L2Size)
                {
                    ULONG iterations = (ULONG)start + length;
                    ULONG mask = -32;
                    start = (APTR)((ULONG)start & mask);
                    ULONG mem = (ULONG)start;
                    iterations = (((iterations + 31) & mask) - (ULONG)start) >> 5;

                    for (int i = 0; i < iterations; i++)
                    {
                        dInval(mem);
                        mem += CACHELINE_SIZE;
                    }
                    sync();
                }
                else
                {
                    ULONG mem = 0;
                    for (int i = 0; i < CACHE_L1SIZE; i++)
                    {
                        loadWord(mem);
                        mem += CACHELINE_SIZE;
                    }
                }
            }
            break;
        }
        case CACHE_ICACHEINV:
        {
            if ((start) && (length))
            {
                ULONG iterations = (ULONG)start + length;
                ULONG mask = -32;
                start = (APTR)((ULONG)start & mask);
                ULONG mem = (ULONG)start;
                iterations = (((iterations + 31) & mask) - (ULONG)start) >> 5;

                for (int i = 0; i < iterations; i++)
                {
                    iInval(mem);
                    mem += CACHELINE_SIZE;
                }
                isync();
            }
            else
            {
                FlushICache();
            }
            break;
        }
    }
    if ((PowerPCBase->pp_DeviceID != DEVICE_MPC8343E) && (PowerPCBase->pp_DeviceID != DEVICE_MPC8314E))
    {
        switch (flags) //To prevent SDA_BASE
        {
            case  CACHE_L2CACHEON:
            {
                value = getL2State();
                value |= L2CR_L2E;
                setL2State(value);
                PowerPCBase->pp_CurrentL2Size = PowerPCBase->pp_L2Size;
                break;
            }
            case CACHE_L2CACHEOFF:
            {
                FlushDCache(PowerPCBase);
                value = getL2State();
                value &= ~L2CR_L2E;
                setL2State(value);
                PowerPCBase->pp_CurrentL2Size = 0;
                break;
            }
            case CACHE_L2WTON:
            {
                value = getL2State();
                value |= L2CR_L2WT;
                setL2State(value);
                break;
            }
            case CACHE_L2WTOFF:
            {
                value = getL2State();
                value &= ~L2CR_L2WT;
                setL2State(value);
                break;
            }
            case CACHE_TOGGLEDFLUSH:
            {
                PowerPCBase->pp_CacheDisDFlushAll = !PowerPCBase->pp_CacheDisDFlushAll;
                break;
            }
        }
    }
    return;
}

/********************************************************************************************
*
*     Entry point to print an exception error in a window.
*
*********************************************************************************************/

PPCKERNEL void CommonExcError(__reg("r3") struct PrivatePPCBase* PowerPCBase, __reg("r4") struct iframe* iframe)
{
    if ((iframe->if_Context.ec_ExcID == EXCF_INTERRUPT) || (iframe->if_Context.ec_ExcID == EXCF_DECREMENTER)
     || (iframe->if_Context.ec_ExcID == EXCF_TRACE)     || (iframe->if_Context.ec_ExcID == EXCF_PERFMON))
    {
        return;
    }

    ULONG* errorData = (ULONG*)(PowerPCBase->pp_PPCMemBase + (ULONG)(FIFO_END + 0x100));
    errorData[0]  = (ULONG)PowerPCBase->pp_ThisPPCProc->tp_Task.tc_Node.ln_Name;
    errorData[1]  = (ULONG)PowerPCBase->pp_ThisPPCProc;
    errorData[2]  = iframe->if_ExcNum;
    errorData[3]  = iframe->if_Context.ec_UPC.ec_SRR0;
    errorData[4]  = iframe->if_Context.ec_SRR1;
    errorData[5]  = getMSR();
    errorData[6]  = getHID0();
    errorData[7]  = getPVR();
    errorData[8]  = iframe->if_Context.ec_DAR;
    errorData[9]  = iframe->if_Context.ec_DSISR;
    errorData[10] = getSDR1();
    errorData[11] = getDEC();
    errorData[12] = getTBU();
    errorData[13] = getTBL();
    errorData[14] = iframe->if_Context.ec_XER;
    errorData[15] = iframe->if_Context.ec_CR;
    errorData[16] = iframe->if_Context.ec_FPSCR;
    errorData[17] = iframe->if_Context.ec_LR;
    errorData[18] = iframe->if_Context.ec_CTR;
    errorData[19] = iframe->if_Context.ec_GPR[0];
    errorData[20] = iframe->if_Context.ec_GPR[1];
    errorData[21] = iframe->if_Context.ec_GPR[2];
    errorData[22] = iframe->if_Context.ec_GPR[3];
    errorData[23] = iframe->if_BATs[0].ba_ibatu;;
    errorData[24] = iframe->if_BATs[0].ba_ibatl;
    errorData[25] = iframe->if_Context.ec_GPR[4];
    errorData[26] = iframe->if_Context.ec_GPR[5];
    errorData[27] = iframe->if_Context.ec_GPR[6];
    errorData[28] = iframe->if_Context.ec_GPR[7];
    errorData[29] = iframe->if_BATs[1].ba_ibatu;
    errorData[30] = iframe->if_BATs[1].ba_ibatl;
    errorData[31] = iframe->if_Context.ec_GPR[8];
    errorData[32] = iframe->if_Context.ec_GPR[9];
    errorData[33] = iframe->if_Context.ec_GPR[10];
    errorData[34] = iframe->if_Context.ec_GPR[11];
    errorData[35] = iframe->if_BATs[2].ba_ibatu;
    errorData[36] = iframe->if_BATs[2].ba_ibatl;
    errorData[37] = iframe->if_Context.ec_GPR[12];
    errorData[38] = iframe->if_Context.ec_GPR[13];
    errorData[39] = iframe->if_Context.ec_GPR[14];
    errorData[40] = iframe->if_Context.ec_GPR[15];
    errorData[41] = iframe->if_BATs[3].ba_ibatu;
    errorData[42] = iframe->if_BATs[3].ba_ibatl;
    errorData[43] = iframe->if_Context.ec_GPR[16];
    errorData[44] = iframe->if_Context.ec_GPR[17];
    errorData[45] = iframe->if_Context.ec_GPR[18];
    errorData[46] = iframe->if_Context.ec_GPR[19];
    errorData[47] = iframe->if_BATs[0].ba_dbatu;
    errorData[48] = iframe->if_BATs[0].ba_dbatl;
    errorData[49] = iframe->if_Context.ec_GPR[20];
    errorData[50] = iframe->if_Context.ec_GPR[21];
    errorData[51] = iframe->if_Context.ec_GPR[22];
    errorData[52] = iframe->if_Context.ec_GPR[23];
    errorData[53] = iframe->if_BATs[1].ba_dbatu;
    errorData[54] = iframe->if_BATs[1].ba_dbatl;
    errorData[55] = iframe->if_Context.ec_GPR[24];
    errorData[56] = iframe->if_Context.ec_GPR[25];
    errorData[57] = iframe->if_Context.ec_GPR[26];
    errorData[58] = iframe->if_Context.ec_GPR[27];
    errorData[59] = iframe->if_BATs[2].ba_dbatu;
    errorData[60] = iframe->if_BATs[2].ba_dbatl;
    errorData[61] = iframe->if_Context.ec_GPR[28];
    errorData[62] = iframe->if_Context.ec_GPR[29];
    errorData[63] = iframe->if_Context.ec_GPR[30];
    errorData[64] = iframe->if_Context.ec_GPR[31];
    errorData[65] = iframe->if_BATs[3].ba_dbatu;
    errorData[66] = iframe->if_BATs[3].ba_dbatl;

    struct MsgFrame* myFrame = KCreateMsgFramePPC(PowerPCBase);
    myFrame->mf_Identifier = ID_CRSH;
    KSendMsgFramePPC(PowerPCBase, myFrame);

    PowerPCBase->pp_ThisPPCProc->tp_Task.tc_State = TS_REMOVED;
    PowerPCBase->pp_ThisPPCProc->tp_Flags |= TASKPPCF_CRASHED;
    RemovePPC((struct Node*)PowerPCBase->pp_ThisPPCProc->tp_TaskPtr);
    SwitchPPC(PowerPCBase, iframe);
}

#endif
#endif