**
**      $VER: powerpc_lib.i 14.0 (5.3.98)
**
**      Library offsets for powerpc.library V14
**

****  68K Library offsets

_LVORunPPC              =       -30
_LVOWaitForPPC          =       -36
_LVOGetCPU              =       -42
_LVOPowerDebugMode      =       -48
_LVOAllocVec32          =       -54
_LVOFreeVec32           =       -60
_LVOSPrintF68K          =       -66
_LVOAllocXMsg           =       -72
_LVOFreeXMsg            =       -78
_LVOPutXMsg             =       -84
_LVOGetPPCState         =       -90
_LVOSetCache68K         =       -96
_LVOCreatePPCTask       =       -102
_LVOCausePPCInterrupt   =       -108

****  PPC Library offsets

_LVORun68K              =       -300
_LVOWaitFor68K          =       -306
_LVOSPrintF             =       -312
_LVORun68KLowLevel      =       -318    ;PRIVATE!
_LVOAllocVecPPC         =       -324
_LVOFreeVecPPC          =       -330
_LVOCreateTaskPPC       =       -336
_LVODeleteTaskPPC       =       -342
_LVOFindTaskPPC         =       -348
_LVOInitSemaphorePPC    =       -354
_LVOFreeSemaphorePPC    =       -360
_LVOAddSemaphorePPC     =       -366
_LVORemSemaphorePPC     =       -372
_LVOObtainSemaphorePPC  =       -378
_LVOAttemptSemaphorePPC =       -384
_LVOReleaseSemaphorePPC =       -390
_LVOFindSemaphorePPC    =       -396
_LVOInsertPPC           =       -402
_LVOAddHeadPPC          =       -408
_LVOAddTailPPC          =       -414
_LVORemovePPC           =       -420
_LVORemHeadPPC          =       -426
_LVORemTailPPC          =       -432
_LVOEnqueuePPC          =       -438
_LVOFindNamePPC         =       -444
_LVOFindTagItemPPC      =       -450
_LVOGetTagDataPPC       =       -456
_LVONextTagItemPPC      =       -462
_LVOAllocSignalPPC      =       -468
_LVOFreeSignalPPC       =       -474
_LVOSetSignalPPC        =       -480
_LVOSignalPPC           =       -486
_LVOWaitPPC             =       -492
_LVOSetTaskPriPPC       =       -498
_LVOSignal68K           =       -504
_LVOSetCache            =       -510
_LVOSetExcHandler       =       -516
_LVORemExcHandler       =       -522
_LVOSuper               =       -528
_LVOUser                =       -534
_LVOSetHardware         =       -540
_LVOModifyFPExc         =       -546
_LVOWaitTime            =       -552
_LVOChangeStack         =       -558    ;PRIVATE!
_LVOLockTaskList        =       -564
_LVOUnLockTaskList      =       -570
_LVOSetExcMMU           =       -576
_LVOClearExcMMU         =       -582
_LVOChangeMMU           =       -588
_LVOGetInfo             =       -594
_LVOCreateMsgPortPPC    =       -600
_LVODeleteMsgPortPPC    =       -606
_LVOAddPortPPC          =       -612
_LVORemPortPPC          =       -618
_LVOFindPortPPC         =       -624
_LVOWaitPortPPC         =       -630
_LVOPutMsgPPC           =       -636
_LVOGetMsgPPC           =       -642
_LVOReplyMsgPPC         =       -648
_LVOFreeAllMem          =       -654
_LVOCopyMemPPC          =       -660
_LVOAllocXMsgPPC        =       -666
_LVOFreeXMsgPPC         =       -672
_LVOPutXMsgPPC          =       -678
_LVOGetSysTimePPC       =       -684
_LVOAddTimePPC          =       -690
_LVOSubTimePPC          =       -696
_LVOCmpTimePPC          =       -702
_LVOSetReplyPortPPC     =       -708
_LVOSnoopTask           =       -714
_LVOEndSnoopTask        =       -720
_LVOGetHALInfo          =       -726
_LVOSetScheduling       =       -732
_LVOFindTaskByID        =       -738
_LVOSetNiceValue        =       -744
_LVOTrySemaphorePPC     =       -750
_LVOAllocPrivateMem     =       -756    ;PRIVATE!
_LVOFreePrivateMem      =       -762    ;PRIVATE!
_LVOResetCPU            =       -768    ;PRIVATE!
_LVONewListPPC          =       -774
_LVOSetExceptPPC        =       -780
_LVOObtainSemaphoreSharedPPC    =       -786
_LVOAttemptSemaphoreSharedPPC   =       -792
_LVOProcurePPC          =       -798
_LVOVacatePPC           =       -804
_LVOCauseInterrupt      =       -810
_LVOCreatePoolPPC       =       -816
_LVODeletePoolPPC       =       -822
_LVOAllocPooledPPC      =       -828
_LVOFreePooledPPC       =       -834
_LVORawDoFmtPPC         =       -840

		IFND    _POWERMODE

CALLPOWERPC     MACRO
		move.l  _PowerPCBase,a6
		jsr     _LVO\1(a6)
		ENDM

		ELSEIF

		IFND    POWERPC_PPCMACROS_I
		INCLUDE powerpc/ppcmacros.i
		ENDC

CALLPOWERPC     MACRO
		lw      r3,_PowerPCBase
		lwz     r0,_LVO\1+2(r3)
		mtlr    r0
		blrl
		ENDM

		ENDC

POWERPCNAME     MACRO
		dc.b    'powerpc.library',0
		ENDM
