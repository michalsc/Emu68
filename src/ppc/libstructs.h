#ifndef _LIBSTRUCTS_H
#define _LIBSTRUCTS_H

#include "config.h"

#include <exec/types.h>
#include <powerpc/powerpc.h>
#include <powerpc/tasksPPC.h>

struct MsgFrame {
    struct Message      mf_Message;
    volatile ULONG      mf_Identifier;
    struct MsgPort *    mf_MirrorPort;
    struct TaskPPC *    mf_PPCTask;
    struct PPCArgs      mf_PPCArgs;
    ULONG               mf_Signals;
    ULONG               mf_Arg[3];
};

struct CacheSize {
    ULONG               cz_SizeBit;
    ULONG               cz_SizeBytes;
};

struct DebugArgs {
    STRPTR              db_ProcessName;
    ULONG               db_Function;
    ULONG               db_Arg[4];
};


struct SysCall {
    ULONG               sc_Function;
    ULONG               sc_Arg[4];
};

struct MirrorTask {
    struct MinNode      mt_Node;
    struct TaskPPC *    mt_PPCTask;
    struct Task *       mt_Task;
    struct MsgPort *    mt_Port;
    ULONG               mt_Flags;
};

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

struct InternalConsts {
    struct ExecBase*        ic_SysBase;
    BPTR                    ic_SegList;
    struct DosLibrary*      ic_DOSBase;
    struct PciBase*         ic_PciBase;
    struct PPCBase*         ic_PowerPCBase;
    struct ExpansionBase*   ic_ExpansionBase;
    ULONG                   ic_MemBase;
    ULONG                   ic_gfxMem;
    ULONG                   ic_gfxSize;
    ULONG                   ic_gfxConfig;
    ULONG                   ic_startBAT;
    ULONG                   ic_sizeBAT;
    ULONG                   ic_deviceID;
    ULONG                   ic_env1;
    ULONG                   ic_env2;
    ULONG                   ic_env3;
    UWORD                   ic_gfxType;
    UWORD                   ic_gfxSubType;
    UWORD                   ic_pciType;
};


struct InitData {
    ULONG               id_Reserved;
    volatile ULONG      id_Status;
    ULONG               id_MemBase;
    ULONG               id_MemSize;
    ULONG               id_GfxMemBase;
    ULONG               id_GfxMemSize;
    UWORD               id_GfxType;
    UWORD               id_GfxSubType;
    ULONG               id_GfxConfigBase;
    ULONG               id_Environment1;
    ULONG               id_Environment2;
    ULONG               id_Environment3;
    ULONG               id_DeviceID;
    ULONG               id_ConfigBase;
    ULONG               id_MsgsBase;
    ULONG               id_MPICBase;
    ULONG               id_StartBat;
    ULONG               id_SizeBat;
};

struct MemSettings {
    ULONG               ms_msar1;
    ULONG               ms_msar2;
    ULONG               ms_mear1;
    ULONG               ms_mear2;
    ULONG               ms_mesar1;
    ULONG               ms_mesar2;
    ULONG               ms_meear1;
    ULONG               ms_meear2;
    ULONG               ms_mben;
    ULONG               ms_mccr1;
    ULONG               ms_mccr1testhigh;
    ULONG               ms_mccr1testlow1;
    ULONG               ms_mccr1testlow2;
    ULONG               ms_mccr1testlow3;
    ULONG               ms_memsize;
};

/* Structure to store BAT entries */
struct BATArray {
    ULONG                       ba_ibatu;
    ULONG                       ba_ibatl;
    ULONG                       ba_dbatu;
    ULONG                       ba_dbatl;
};

struct LibSema {
    struct SignalSemaphorePPC   ls_Semaphore;
    ULONG                       ls_Reserved[8];
};

#if 0
struct PrivatePPCBase {
    struct PPCBase              pp_PowerPCBase;
    struct MinList              pp_MirrorList;
    ULONG                       pp_DeviceID;
    ULONG                       pp_BridgeConfig;
    ULONG                       pp_BridgeMsgs;
    ULONG                       pp_BridgeMPIC;
    struct MsgPort*             pp_MasterControl;
    struct UtilityBase*         pp_UtilityBase;
    UBYTE                       pp_DebugLevel;
    UBYTE                       pp_EnAlignExc;
    UBYTE                       pp_EnDAccessExc;
    UBYTE                       pp_Pad;
    ULONG                       pp_TaskExitCode;
    ULONG                       pp_PPCMemBase;
    ULONG                       pp_PPCMemSize;
    volatile ULONG              pp_Mutex;
    ULONG                       pp_L2Size;
    ULONG                       pp_CurrentL2Size;
    ULONG                       pp_L2State;
    ULONG                       pp_CPUSDR1;
    ULONG                       pp_CPUHID0;
    ULONG                       pp_CPUSpeed;
    ULONG                       pp_CPUInfo;
    ULONG                       pp_PageTableSize;
    ULONG                       pp_LowerLimit;
    ULONG                       pp_UpperLimit;
    struct TaskPPC*             pp_ThisPPCProc;
    struct TaskPPC*             pp_TaskExcept;
    struct MinList              pp_WaitingTasks;
    struct MinList              pp_ReadyTasks;
    struct MinList              pp_NewTasks;
    struct MinList              pp_RemovedTasks;
    struct MinList              pp_AllTasks;
    struct MinList              pp_Snoop;
    struct MinList              pp_Semaphores;
    struct MinList              pp_RemovedExc;
    struct MinList              pp_ReadyExc;
    struct MinList              pp_InstalledExc;
    struct MinList              pp_ExcInterrupt;
    struct MinList              pp_ExcIABR;
    struct MinList              pp_ExcPerfMon;
    struct MinList              pp_ExcTrace;
    struct MinList              pp_ExcSystemCall;
    struct MinList              pp_ExcDecrementer;
    struct MinList              pp_ExcFPUn;
    struct MinList              pp_ExcProgram;
    struct MinList              pp_ExcAlign;
    struct MinList              pp_ExcIAccess;
    struct MinList              pp_ExcDAccess;
    struct MinList              pp_ExcMCheck;
    struct MinList              pp_ExcSysMan;
    struct MinList              pp_ExcTherMan;
    struct MinList              pp_WaitTime;
    struct MinList              pp_Ports;
    struct MinList              pp_MsgQueue;
    struct LibSema              pp_SemWaitList;
    struct LibSema              pp_SemTaskList;
    struct LibSema              pp_SemSemList;
    struct LibSema              pp_SemPortList;
    struct LibSema              pp_SemSnoopList;
    struct LibSema              pp_SemMemory;
    ULONG                       pp_AlignmentExcHigh;
    ULONG                       pp_AlignmentExcLow;
    ULONG                       pp_DataExcHigh;
    ULONG                       pp_DataExcLow;
    struct MsgPortPPC*          pp_CurrentPort;
    UBYTE                       pp_EnAltivec;
    volatile UBYTE              pp_ExceptionMode;
    UBYTE                       pp_CacheDisDFlushAll;
    UBYTE                       pp_CacheDState;
    UBYTE                       pp_CacheDLockState;
    UBYTE                       pp_FlagReschedule;
    UBYTE                       pp_FlagForbid;
    UBYTE                       pp_FlagWait;
    UBYTE                       pp_FlagPortInUse;  //for nt_IntPort....not used
    UBYTE                       pp_BusyCounter;
    UBYTE                       pp_BytePad[2];
    ULONG                       pp_NumAllTasks;
    ULONG                       pp_StartTBL;
    ULONG                       pp_CurrentTBL;
    ULONG                       pp_SystemLoad;
    ULONG                       pp_CPULoad;
    ULONG                       pp_NICETable;
    LONG                        pp_LowActivityPri;
    ULONG                       pp_IdSysTasks;
    ULONG                       pp_IdUsrTasks;
    LONG                        pp_LowActivityPriOffset;
    ULONG                       pp_ErrorStrings;
    ULONG                       pp_BusClock;
    ULONG                       pp_Quantum;
    ULONG                       pp_StdQuantum;
    ULONG                       pp_NumRun68k;
    struct BATArray             pp_ExceptionBATs[4];
    struct BATArray             pp_StoredBATs[4];
    struct BATArray             pp_SystemBATs[4];
    ULONG                       pp_SystemSegs[16];
};
#endif

struct killFIFO {
    ULONG                       kf_MIOFH;
    ULONG                       kf_MIOPT;
    ULONG                       kf_MIIFT;
    ULONG                       kf_MIIPH;
    ULONG                       kf_CreatePrevious;
    ULONG                       kf_GetPrevious;
    ULONG                       kf_CacheGap[2];
    ULONG                       kf_MIOFT;
    ULONG                       kf_MIOPH;
    ULONG                       kf_MIIFH;
    ULONG                       kf_MIIPT;
};

struct poolHeader {
    struct MinNode              ph_Node;
    ULONG                       ph_Requirements;
    ULONG                       ph_PuddleSize;
    ULONG                       ph_ThresholdSize;
    struct MinList              ph_PuddleList;
    struct MinList              ph_BlockList;
};

struct SemWait {
    struct MinNode              sw_Node;
    struct TaskPPC*             sw_Task;
    ULONG                       sw_Pad[2];
    struct SignalSemaphorePPC*  sw_Semaphore;
};

struct TagItemPtr {
    struct TagItem* tip_TagItem;
};

struct ExcData {
    struct Node                 ed_Node;
    APTR                        ed_Code;
    ULONG                       ed_Data;
    struct TaskPPC*             ed_Task;
    volatile ULONG              ed_Flags;
    ULONG                       ed_ExcID;
    ULONG                       ed_RemovalTime;
    ULONG                       ed_TimeBaseUpper;
    ULONG                       ed_TimeBaseLower;
    struct ExcData*             ed_LastExc;
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

struct ExcInfo {
    struct ExcData              ei_ExcData;
    struct Node*                ei_MachineCheck;
    struct Node*                ei_DataAccess;
    struct Node*                ei_InstructionAccess;
    struct Node*                ei_Alignment;
    struct Node*                ei_Program;
    struct Node*                ei_FPUnavailable;
    struct Node*                ei_Decrementer;
    struct Node*                ei_SystemCall;
    struct Node*                ei_Trace;
    struct Node*                ei_PerfMon;
    struct Node*                ei_IABR;
    struct Node*                ei_Interrupt;
};

struct DataMsg {
    ULONG                       dm_Type;
    ULONG                       dm_Value;
    ULONG                       dm_Address;
    ULONG                       dm_LoadType;
    ULONG                       dm_RegNumber;
    ULONG                       dm_LoadFlag;
    ULONG                       dm_IndexedFlag;
};

struct WaitTime {
    struct Node                 wt_Node;
    ULONG                       wt_TimeUpper;
    ULONG                       wt_TimeLower;
    struct TaskPPC*             wt_Task;
};

struct UInt64 {
    ULONG                       ui_High;
    ULONG                       ui_Low;
};

struct SnoopData {
    struct Node                 sd_Node;
    APTR                        sd_Code;
    ULONG                       sd_Data;
    ULONG                       sd_Type;
};

struct RDFData {
    APTR                        rd_DataStream;
    UBYTE                       rd_Buffer[16];
    STRPTR                      rd_BufPointer;
    STRPTR                      rd_FormatString;
    ULONG                       rd_TruncateNum;
    APTR                        rd_PutChData;
};

struct PrivateTask {
    struct TaskPPC              pt_Task;
    UWORD                       pt_Pad1;
    struct Message*             pt_StartMsg;
    struct Task*                pt_Mirror68K;
    struct MsgPort*             pt_MirrorPort;
};

struct NewTask {
    struct PrivateTask          nt_Task;
    struct iframe               nt_Context;
    struct TaskPtr              nt_TaskPtr;
    UWORD                       nt_Pad2;
    struct MsgPortPPC           nt_Port;
    ULONG                       nt_SSReserved1[8];  //Belongs to Semaphore of nt_Port
    struct MsgPortPPC           nt_IntPort;         //Currently not used or set-up. Also removed from functions atm.
    ULONG                       nt_SSReserved2[8];  //Belongs to Sempahore of nt_IntPort
    struct BATArray             nt_BatStore[4];
    UBYTE                       nt_Name[256];
};

#endif /* _LIBSTRUCTS_H */
