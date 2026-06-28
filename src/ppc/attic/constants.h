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

#ifndef _CONSTANTS_H
#define _CONSTANTS_H

#define CUSTOMBASE              0xdff000

#define SC_CREATEMSG            0x80000001
#define SC_GETMSG               0x80000002
#define SC_SENDMSG              0x80000003
#define SC_FREEMSG              0x80000004
#define SC_SETCACHE             0x80000005

#define PPC_VECLEN              3           //Number of instructions of VecEntry (setupppc.s)
#define NUM_OF_68K_FUNCS        49
#define NUM_OF_PPC_FUNCS        102
#define TOTAL_FUNCS             (NUM_OF_68K_FUNCS+NUM_OF_PPC_FUNCS)
#define NEGSIZE                 TOTAL_FUNCS*6
#define NEGSIZEALIGN            ((NEGSIZE + 3) & -4)
#define MSGLEN                  192
#define SUPERKEY                0xABADBEEF
#define MAGIC_COOKIE            0x07041776

#define RDFF_JUSTIFY            1
#define RDFF_PREPEND            2
#define RDFF_LONG               4

#define CMP_DESTGREATER         -1
#define CMP_DESTLESS            1
#define CMP_EQUAL               0

#define GETINFO_L2STATE         0x8010200A
#define GETINFO_L2SIZE          0x8010200C

#define TASKATTR_PRIVATE        0x80100011

#define EXCATTR_TIMEDREMOVAL    0x80101007
#define EXCF_DECREMENTER        1<<9
#define PPC_VECLEN              3
#define OPCODE_LEN              4
#define OPCODE_NOP              0x60000000
#define OPCODE_BBRANCH          0x4C000000  //only for backward jump
#define OPCODE_FBRANCH          0x48000000

#define QUANTUM_E300            1333333
#define BUSCLOCK_E300           266666666
#define RCWLR_DDRCM             1
#define BUSCLOCK_100            100000000
#define BUSCLOCK_66             66666666
#define QUANTUM_100             500000
#define QUANTUM_66              333333

#define PPERR_MISCERR           3

#define CPUF_7410               0x00800000
#define CPUF_7400               0x00400000

#define TS_ATOMIC               8

#define SIGF_WAIT               1<<10
#define SYS_SIGALLOC            0x0000FFFF

#define NT_XMSG68K              102
#define NT_XMSGPPC              103

#define CACHE_L2CACHEON         12
#define CACHE_L2CACHEOFF        13
#define CACHE_L2WTON            14
#define CACHE_L2WTOFF           15
#define CACHE_TOGGLEDFLUSH      16
#define CACHELINE_SIZE          32
#define CACHE_L1SIZE            0x400

#define ERR_ESEM                0x4553454D
#define ERR_EFIF                0x45464946
#define ERR_ESNC                0x45534E43
#define ERR_EMEM                0x454D454D
#define ERR_ECOR                0x45434F52
#define ERR_ETIM                0x4554494D
#define ERR_DEAD                0xDEADDEAD
#define ERR_R68K                0x5236384B

#define ERR_PPCOK               0x426F6F6E
#define ERR_PPCMMU              0x45727231
#define ERR_PPCMEM              0x45727232
#define ERR_PPCCORRUPT          0x45727233
#define ERR_PPCSETUP            0x45727234

#define VEC_SYSTEMRESET         0x0100
#define VEC_MACHINECHECK        0x0200
#define VEC_DATASTORAGE         0x0300
#define VEC_INSTSTORAGE         0x0400
#define VEC_EXTERNAL            0x0500
#define VEC_ALIGNMENT           0x0600
#define VEC_PROGRAM             0x0700
#define VEC_FPUNAVAILABLE       0x0800
#define VEC_DECREMENTER         0x0900
#define VEC_SYSTEMCALL          0x0C00
#define VEC_TRACE               0x0D00
#define VEC_FPASSIST            0x0E00
#define VEC_PERFMONITOR         0x0F00
#define VEC_ALTIVECUNAV         0x0F20
#define VEC_ITLBMISS            0x1000
#define VEC_DLOADTLBMISS        0x1100
#define VEC_DSTORETLBMISS       0x1200
#define VEC_IBREAKPOINT         0x1300
#define VEC_SYSMANAGEMENT       0x1400
#define VEC_ALTIVECASSIST       0x1600
#define VEC_THERMAL             0x1700

#define EXCB_ALTIVECUNAV        10          //Is normally reserved for implementation specific exceptions
#define EXCF_ALTIVECUNAV        1<<10

#define ID_PPC                  0x5F505043
#define ID_TPPC                 0x54505043
#define ID_FREE                 0x46524545
#define ID_LLPP                 0x4C4C5050
#define ID_LL68                 0x4C4C3638
#define ID_FPPC                 0x46505043
#define ID_T68K                 0x5436384B
#define ID_DONE                 0x444F4E45
#define ID_END                  0x454E4421
#define ID_SIG                  0x53494721
#define ID_XMSG                 0x584D5347
#define ID_RX68                 0x52583638
#define ID_GETV                 0x47455456
#define ID_PUTB                 0x50555442
#define ID_PUTH                 0x50555448
#define ID_PUTW                 0x50555457
#define ID_DBGS                 0x44424753
#define ID_DBGE                 0x44424745
#define ID_CRSH                 0x43525348
#define ID_DNLL                 0x444E4C4C
#define ID_XPPC                 0x58505043

#define STATUS_INIT             0x494E4954
#define STATUS_READY            0x52454459
#define STATUS_MEM              0x4D454D00

#define BASE_KMSG               0x200000
#define SIZE_KBASE              0xc0000
#define SIZE_KFIFO              0x10000
#define SIZE_HFIFO              0x40000
#define SIZE_SFIFO              0x4000
#define FIFO_OFFSET             0x380000
#define FIFO_END                FIFO_OFFSET + (4 * SIZE_KFIFO)

#define HW_SETDEBUGMODE         12          //Private
#define HW_PPCSTATE             13          //Private

#define HINFO_DSEXC_HIGH        0x80103002
#define HINFO_DSEXC_LOW         0x80103003
#define ID_MPC834X              0x8083
#define ID_MPC831X              0x8085

#define MEMF_PPC                (1L<<13)
#define MMUF_BAT                (1L<<1)
#define MEM_GAP                 0x410000
#define TF_PPC                  1<<2
#define VENDOR_ELBOX            0x89e
#define MEDIATOR_MKII           33
#define MEDIATOR_1200TX         60
#define MEDIATOR_MKIII          63
#define MEDIATOR_LOGIC          161
#define MEDIATOR_1200LOGIC      188
#define MEDIATOR_LOGICIII       191

#define VENDOR_E3B              0xe3b
#define PROMETHEUS_FIRESTORM    0xc8

#define MAX_PCI_SLOTS           6
#define MAX_PPCI_SLOTS          4
#define PCI_OFFSET_ID           0
#define PCI_OFFSET_COMMAND      4
#define DEVICENUMBER_SHIFT      3
#define BUSNUMBER_SHIFT         8
#define BUSMASTER_ENABLE        1<<2
#define MEMORYSPACE_ENABLE      1<<1

#define DEVICE_MPC107           0x0004
#define VENDOR_MOTOROLA         0x1057
#define DEVICE_HARRIER          0x480B
#define DEVICE_MPC8314E         0x00B6
#define DEVICE_MPC8343E         0x0086
#define VENDOR_FREESCALE        0x1957

#define VENDOR_ATI              0x1002
#define VENDOR_TI               0x104c
#define VENDOR_3DFX             0x121a
#define DEVICE_VOODOO45         0x0009
#define DEVICE_VOODOO3          0x0005
#define DEVICE_VBANSHEE         0x0003
#define DEVICE_PERMEDIA2        0x3d07
#define DEVICE_RV280PRO         0x5960
#define DEVICE_RV280            0x5961
#define DEVICE_RV280_2          0x5962
#define DEVICE_RV280MOB         0x5C63
#define DEVICE_RV280SE          0x5964

#define OFFSET_KERNEL           0x3000
#define OFFSET_ZEROPAGE         0x10000
#define OFFSET_MESSAGES         0x200000
#define OFFSET_SYSMEM           0x400000
#define OFFSET_PCIMEM           0x60000000
#define VECTOR_TABLE_DEFAULT    0xfff00000

//Bit defines for the L2CR register

#define L2CR_L2E                0x80000000      /* bit 0 - enable */
#define L2CR_L2PE               0x40000000      /* bit 1 - data parity */
#define L2CR_L2SIZ_2M           0x00000000      /* bits 2-3 2 MB; MPC7400 ONLY! */
#define L2CR_L2SIZ_1M           0x30000000      /* bits 2-3 1MB */
#define L2CR_L2SIZ_HM           0x20000000      /* bits 2-3 512K */
#define L2CR_L2SIZ_QM           0x10000000      /* bits 2-3 256K; MPC750 ONLY */
#define L2CR_L2CLK_1            0x02000000      /* bits 4-6 Clock Ratio div 1 */
#define L2CR_L2CLK_1_5          0x04000000      /* bits 4-6 Clock Ratio div 1.5 */
#define L2CR_L2CLK_2            0x08000000      /* bits 4-6 Clock Ratio div 2 */
#define L2CR_L2CLK_2_5          0x0a000000      /* bits 4-6 Clock Ratio div 2.5 */
#define L2CR_L2CLK_3            0x0c000000      /* bits 4-6 Clock Ratio div 3 */
#define L2CR_L2CLK_4            0x0e000000      /* bits 4-6 Clock Ratio div 4 */
#define L2CR_L2RAM_BURST        0x01000000      /* bits 7-8 burst SRAM */
#define L2CR_DO                 0x00400000      /* bit 9 Disable caching of instr. in L2 (Not 7450) */
#define L2CR_L2I                0x00200000      /* bit 10 Global invalidate bit */
#define L2CR_L2IO               0x00100000      /* bit 11 L2 Instruction only (disables data cache) */
#define L2CR_L2WT               0x00080000      /* bit 12 write-through */
#define L2CR_TS                 0x00040000      /* bit 13 Test support on */
#define L2CR_L2DO               0x00010000      /* bit 15 L2 Data only (disables instruction cache) */
#define L2CR_L2HWF              0x00000800      /* bit 20 L2 Hardware Flush (not 7400/750) */
#define L2CR_L2OH_5             0x00000000      /* bits 14-15 Output Hold time = 0.5ns */
#define L2CR_L2OH_1             0x00010000      /* bits 14-15 Output Hold time = 1.0ns */
#define L2CR_L2OH_INV           0x00020000      /* bits 14-15 Output Hold time = 1.0ns */
#define L2CR_L2IP               0x00000001

#define L2_ADR_INCR             0x100
#define L2_SIZE_2M              0x2000
#define L2_SIZE_1M              0x1000
#define L2_SIZE_HM              0x800
#define L2_SIZE_QM              0x400

#define L2_SIZE_1M_U            0x0010
#define L2_SIZE_2M_U            0x0020

//Bit defines for the HID0 register

#define HID0_TBEN               0x04000000
#define HID0_NHR                0x00010000
#define HID0_ICFI               0x00000800
#define HID0_DCFI               0x00000400
#define HID0_ICE                0x00008000
#define HID0_DCE                0x00004000
#define HID0_ILOCK              0x00002000
#define HID0_DLOCK              0x00001000
#define HID0_SGE                0x00000080
#define HID0_BTIC               0x00000020
#define HID0_BHTE               0x00000004

#define MMU_PAGESIZE            0x1000

// MSR settings

#define	   PSL_VEC	  0x02000000  /* ..6. AltiVec vector unit available */
#define	   PSL_SPV	  0x02000000  /* B... (e500) SPE enable */
#define	   PSL_UCLE	  0x00400000  /* B... user-mode cache lock enable */
#define	   PSL_POW	  0x00040000  /* ..6. power management */
#define	   PSL_WE	  PSL_POW     /* B4.. wait state enable */
#define	   PSL_TGPR	  0x00020000  /* ..6. temp. gpr remapping (mpc603e) */
#define	   PSL_CE	  PSL_TGPR    /* B4.. critical interrupt enable */
#define	   PSL_ILE	  0x00010000  /* ..6. interrupt endian mode (1 == le) */
#define	   PSL_EE	  0x00008000  /* B468 external interrupt enable */
#define	   PSL_PR	  0x00004000  /* B468 privilege mode (1 == user) */
#define	   PSL_FP	  0x00002000  /* B.6. floating point enable */
#define	   PSL_ME	  0x00001000  /* B468 machine check enable */
#define	   PSL_FE0	  0x00000800  /* B.6. floating point mode 0 */
#define	   PSL_SE	  0x00000400  /* ..6. single-step trace enable */
#define	   PSL_DWE	  PSL_SE      /* .4.. debug wait enable */
#define	   PSL_UBLE	  PSL_SE      /* B... user BTB lock enable */
#define	   PSL_BE	  0x00000200  /* ..6. branch trace enable */
#define	   PSL_DE	  PSL_BE      /* B4.. debug interrupt enable */
#define	   PSL_FE1	  0x00000100  /* B.6. floating point mode 1 */
#define	   PSL_IP	  0x00000040  /* ..6. interrupt prefix */
#define	   PSL_IR	  0x00000020  /* .468 instruction address relocation */
#define	   PSL_IS	  PSL_IR      /* B... instruction address space */
#define	   PSL_DR	  0x00000010  /* .468 data address relocation */
#define	   PSL_DS	  PSL_DR      /* B... data address space */
#define	   PSL_PM	  0x00000008  /* ..6. Performance monitor */
#define	   PSL_PMM	  PSL_PM      /* B... Performance monitor */
#define	   PSL_RI	  0x00000002  /* ..6. recoverable interrupt */
#define	   PSL_LE	  0x00000001  /* ..6. endian mode (1 == le) */

#define MACHINESTATE_DEFAULT    PSL_IR|PSL_DR|PSL_FP|PSL_PR|PSL_EE|PSL_ME

// general BAT defines for bit settings to compose BAT regs
// represent all the different block lengths
// The BL field	is part of the Upper Bat Register

#define BAT_BL_128K             0x00000000
#define BAT_BL_256K             0x00000004
#define BAT_BL_512K             0x0000000C
#define BAT_BL_1M               0x0000001C
#define BAT_BL_2M               0x0000003C
#define BAT_BL_4M               0x0000007C
#define BAT_BL_8M               0x000000FC
#define BAT_BL_16M              0x000001FC
#define BAT_BL_32M              0x000003FC
#define BAT_BL_64M              0x000007FC
#define BAT_BL_128M             0x00000FFC
#define BAT_BL_256M             0x00001FFC
#define BAT_BL_512M             0x00003FFC

// supervisor/user valid mode definitions  - Upper BAT
#define BAT_VALID_SUPERVISOR    0x00000002
#define BAT_VALID_USER          0x00000001
#define BAT_INVALID             0x00000000

// WIMG bit settings  - Lower BAT
#define BAT_WRITE_THROUGH       0x00000040
#define BAT_CACHE_INHIBITED     0x00000020
#define BAT_COHERENT            0x00000010
#define BAT_GUARDED             0x00000008

// General PTE bits
#define PTE_REFERENCED          0x00000100
#define PTE_CHANGED             0x00000080
#define PTE_HASHID              0x00000040
#define PTE_VALID               0x80000000

// PageTable Access bits
#define PP_USER_RW              2
#define PP_SUPERVISOR_RW        0

// WIMG bit settings  - Lower PTE
#define PTE_WRITE_THROUGH       0x00000008
#define PTE_CACHE_INHIBITED     0x00000004
#define PTE_COHERENT            0x00000002
#define PTE_GUARDED             0x00000001
#define PTE_COPYBACK            0x00000000

// Protection bits - Lower BAT
#define BAT_NO_ACCESS           0x00000000
#define BAT_READ_ONLY           0x00000001
#define BAT_READ_WRITE          0x00000002

#define CHMMU_BAT0              0
#define CHMMU_BAT1              1
#define CHMMU_BAT2              2
#define CHMMU_BAT3              3

// IMMR offsets
//#define IMMR_ADDR_DEFAULT       0xFF400000
#define IMMR_ADDR_DEFAULT       0xE0000000
#define IMMR_IMMRBAR            0x0

#define IMMR_PCILAWBAR0         0x60
#define IMMR_PCILAWAR0          0x64
#define IMMR_PCILAWBAR1         0x68
#define IMMR_PCILAWAR1          0x6C
#define IMMR_PCIEXP1LAWBAR      0x80
#define IMMR_PCIEXP1LAWAR       0x84

#define IMMR_SWCRR              0x204
#define IMMR_SWCNR              0x208
#define IMMR_SWSRR              0x20E

#define IMMR_RCWLR              0x900
#define IMMR_RSR                0x910
#define IMMR_RPR                0x918
#define IMMR_RCR                0x91C
#define IMMR_RCER               0x920

#define IMMR_OMISR              0x8030
#define IMMR_IMR0               0x8050
#define IMMR_OMR0               0x8058
#define IMMR_IDR                0x8068
#define IMMR_IMISR              0x8080

#define IMMR_PITAR0             0x8568
#define IMMR_PIBAR0             0x8570
#define IMMR_PIWAR0             0x8578

#define IMMR_POTAR0             0x8400
#define IMMR_POTAR1             0x8418
#define IMMR_POTAR2             0x8430
#define IMMR_POTAR3             0x8448

#define IMMR_POBAR0             0x8408
#define IMMR_POBAR1             0x8420
#define IMMR_POBAR2             0x8438
#define IMMR_POBAR3             0x8450

#define IMMR_POCMR0             0x8410
#define IMMR_POCMR1             0x8428
#define IMMR_POCMR2             0x8440
#define IMMR_POCMR3             0x8458
#define IMMR_POCMR4             0x8470
#define IMMR_POCMR5             0x8488

#define IMMR_PEX_BAR_SIZEL      0x94D8
#define IMMR_PEX_BAR_SEL        0x94E0
#define IMMR_PEX_OWAR0          0x9CA0
#define IMMR_PEX_OWBAR0         0x9CA4
#define IMMR_PEX_OWTARL0        0x9CA8
#define IMMR_PEX_OWTARH0        0x9CAC
#define IMMR_PEX_EPIWTAR1       0x9DE4
#define IMMR_PEX_RCIWAR0        0x9E60
#define IMMR_PEX_RCIWTAR0       0x9E64
#define IMMR_PEX_RCIBARL0       0x9E68
#define IMMR_PEX_RCIBARH0       0x9E6C

#define IMMR_OMISR_OM0I         0x01000000
#define IMMR_IMISR_IM0I         0x00000001
#define IMMR_IMISR_IDI          0x00000008
#define IMMR_IDR_IDR0           0x01000000

#define PIWAR_EN                0x80000000
#define PIWAR_PF                0x20000000
#define PIWAR_RTT_SNOOP         0x00060000
#define PIWAR_WTT_SNOOP         0x00006000
#define PIWAR_IWS_64MB          0x00000019
#define PIWAR_IWS_128MB         0x0000001A

#define POCMR_EN                0x80000000
#define POCMR_CM_512MB          0x000E0000
#define POCMR_CM_256MB          0x000F0000
#define POCMR_CM_128MB          0x000F8000
#define POCMR_CM_64MB           0x000FC000
#define POCMR_CM_32MB           0x000FE000
#define POCMR_CM_16MB           0x000FF000
#define POCMR_CM_8MB            0x000FF800
#define POCMR_CM_4MB            0x000FFC00
#define POCMR_CM_64KB           0x000FFFF0
#define POCMR_CM_128KB          0x000FFFE0

#define LAWAR_EN                0x80000000
#define LAWAR_64MB              0x00000019
#define LAWAR_128MB             0x0000001a
#define LAWAR_256MB             0x0000001b
#define LAWAR_512MB             0x0000001c

#define PEX_BAR_SIZEL_128MB     0xF8000000
#define PEX_OWAR_SIZE_512MB     0x1FFFF000
#define PEX_IWAR_SIZE_1MB       0x000FF000
#define PEX_IWAR_SIZE_128MB     0x07FFF000

#define PEX_EPIWTAR_EN          0x00000001
#define PEX_OWAR_EN             0x00000001
#define PEX_OWAR_TYPE_MEM       0x00000004
#define PEX_BAR_SEL_1           0x00000001

#define IMMR_SIMSR_L            0x724
#define SIMSR_L_MU              0x04000000
#define KILLER_RESET            0x52535445
#define SWCRR_SWTC_DEFAULT      0x10000000
#define SWCRR_SWEN_EN           0x00000004
#define SWCRR_SWRI_MPC          0x00000000
#define SWCRR_SWPR_NOPR         0x00000000

// Harrier Stuff

#define PCFS_MBAR               0x10
#define PCFS_ITBAR0             0x14
#define PCFS_ITBAR1             0x18
#define PCFS_ITBAR2             0x1C
#define PCFS_MPAT               0x44
#define PCFS_ITOFSZ0            0x48
#define PCFS_ITAT0              0x4C
#define PCFS_ITOFSZ1            0x50
#define PCFS_ITAT1              0x54
#define PCFS_ITOFSZ2            0x58
#define PCFS_ITAT2              0x5C
#define PCFS_MPAT_ENA           0x00000080
#define PCFS_MPAT_GBL           0x00010000
#define PCFS_ITAT_ENA           0x00000080
#define PCFS_ITAT_WPE           0x00000020
#define PCFS_ITAT_RAE           0x00000010
#define PCFS_ITAT_GBL           0x00010000
#define PCFS_ITSZ_4K            0x00
#define PCFS_ITSZ_256K          0x06
#define PCFS_ITSZ_64MB          0x0e
#define PCFS_ITSZ_128MB         0x0f
#define PCFS_ITSZ_256MB         0x10
#define PPC_XCSR_BASE           0xFEFF0000
#define PPC_RAM_BASE            0x00000000

#define XCSR_OTAT_ENA           0x80
#define XCSR_OTAT_WPE           0x10
#define XCSR_OTAT_SGE           0x08
#define XCSR_OTAT_RAE           0x04
#define XCSR_OTAT_MEM           0x02
#define XCSR_XPAT_BAM_ENA       0x20000000
#define XCSR_XPAT_AD_DELAY15    0x00F00000
#define XCSR_SDBA_16M8          0x00050000
#define XCSR_SDBA_32M8          0x00080000
#define XCSR_SDBA_64M8          0x000B0000
#define XCSR_SDBA_256M4         0x000F0000
#define XCSR_SDBA_SIZE          0x000F0000
#define XCSR_SDBA_ENA           0x00000100
#define XCSR_SDGC_MXRR_7        0x30000000
#define XCSR_SDGC_DERC          0x02000000
#define XCSR_SDGC_ENRV_ENA      0x00800000
#define XCSR_SDTC_DEFAULT       0x07130000
#define XCSR_BXCS_P0H_ENA       0x00100000
#define XCSR_BXCS_BP0H          1<<20
#define XCSR_MBAR_ENA           0x00010000
#define XCSR_XARB_ENA           0x00010000
#define XCSR_XARB_PRKCPU0       0x00200000
#define XCSR_MICT_QSZ_16K       0x00030000
#define XCSR_MICT_ENA           0x01000000
#define XCSR_FEEN_MIP           0x08000000
#define XCSR_FEEN_MIM0          0x20000000
#define XCSR_FEMA_MIPM0         0xD7000000
#define XCSR_FEST_MIM0          0x20000000
#define XCSR_FEST_MIP           0x08000000
#define XCSR_FECL_MIM0          0x20000000
#define XCSR_MCSR_OPI           0x40000000
#define XCSR_MIQB_DEFAULT       0x100000

#define XCSR_FEEN               0x40
#define XCSR_FEST               0x44
#define XCSR_FEMA               0x48
#define XCSR_FECL               0x4C
#define XCSR_XARB               0x94
#define XCSR_MBAR               0xE0
#define XCSR_MCSR               0xE4
#define XCSR_SDGC               0x100
#define XCSR_SDTC               0x104
#define XCSR_SDBAA              0x110
#define XCSR_XPAT0              0x154
#define XCSR_XPAT1              0x15C
#define XCSR_XPAT2              0x164
#define XCSR_XPAT3              0x16C
#define XCSR_BXCS               0x204
#define XCSR_OTAD0              0x220
#define XCSR_OTAT0              0x224
#define XCSR_OTAD1              0x228
#define XCSR_OTAT1              0x22C
#define XCSR_MIOFH              0x2C0
#define XCSR_MIOFT              0x2C4
#define XCSR_MIOPH              0x2C8
#define XCSR_MIOPT              0x2CC
#define XCSR_MIIFH              0x2D0
#define XCSR_MIIFT              0x2D4
#define XCSR_MIIPH              0x2D8
#define XCSR_MIIPT              0x2DC
#define XCSR_MICT               0x2E0
#define XCSR_MIQB               0x2E4

#define XMPI_FREP               0x1000
#define XMPI_GLBC               0x1020
#define XMPI_IFEVP              0x10200
#define XMPI_IFEDE              0x10210
#define XMPI_P0CTP              0x20080
#define XMPI_P0IAC              0x200A0
#define XMPI_P0EOI              0x200B0
#define XMPI_GLBC_RESET         0x80000000
#define XMPI_GLBC_M             0x20000000
#define XMPI_IFEDE_P0           0x00000001

#define PMEP_MIST               0x30
#define PMEP_MIMS               0x34
#define PMEP_MIIQ               0x40
#define PMEP_MIOQ               0x44
#define PMEP_MGIM0              0x2A0

#define PCIMEM_4K               0x00F0FFFF
#define PCIMEM_256KB            0x0000FCFF
#define PCIMEM_64MB             0x000000FC
#define PCIMEM_128MB            0x000000F8
#define PCIMEM_256MB            0x000000F0
#define PCIBAR_0                0
#define PCIBAR_1                1
#define PCIBAR_2                2
#define PCIBAR_3                3
#define PCIBAR_4                4
#define PCIBAR_5                5

// MPC107 stuff

#define CMD_BASE                0x80000000
#define CONFIG_ADDR             0xFEC00000
#define CONFIG_DAT              0xFEE00000

#define PPC_EUMB_BASE           0xF0000000
#define PPC_EUMB_EPICPROC       0xF0060000

#define MPC107_LMBAR            0x10             //Local Memory Base Address register

#define EPIC_IACK               0xA0
#define EPIC_EOI                0xB0
#define EPIC_GCR                0x41020          //Global Configuration register
#define EPIC_PCTPR              0x60080          //Processor Current Task Priority register
#define EPIC_FRR                0x41000          //Feature Reporting register
#define EPIC_GTBCR0             0x41110          //Global Timer Base Count register 0
#define EPIC_GTVPR0             0x41120          //Global Timer Vector/Priority register 0
#define EPIC_EICR               0x41030          //EPIC interrupt Configuration register
#define EPIC_IVPR0              0x50200          //Interrupt Vector/Priority register 0
#define EPIC_IVPR3              0x50260          //Interrupt Vector/Priority register 3
#define EPIC_IVPR4              0x50280          //Interrupt Vector/Priority register 4
#define EPIC_IIVPR3             0x510c0          //I2C Interrupt Vector/Priority register 3
#define EPIC_GCR_RESET          0xA0
#define EPIC_GCR_RSTATUS        0x80
#define EPIC_GCR_MIXMODE        0x20

#define MPC107_OMISR            0x30
#define MPC107_OMIMR            0x34
#define MPC107_IFQPR            0x40
#define MPC107_OFQPR            0x44
#define MPC107_IMR0             0x50
#define MPC107_IMR1             0x54
#define MPC107_OMR0             0x58
#define MPC107_OMR1             0x5C
#define MPC107_EUMBBAR          0x78
#define MPC107_MSAR1            0x80             //Memory Start Address register 1
#define MPC107_MSAR2            0x84             //Memory Start Address register 2
#define MPC107_MESAR1           0x88             //Memory Extended Start Address register 1
#define MPC107_MESAR2           0x8C             //Memory Extended Start Address register 2
#define MPC107_MEAR1            0x90             //Memory End Address register 1
#define MPC107_MEAR2            0x94             //Memory End Address register 2
#define MPC107_MEEAR1           0x98             //Memory Extended End Address register 1
#define MPC107_MEEAR2           0x9C             //Memory Extended End Address register 2
#define MPC107_MBEN             0xA0             //Memory Bank Enable
#define MPC107_PGMAX            0xA3
#define MPC107_MCCR1            0xF0             //#Memory Control Configuration register 1
#define MPC107_MCCR2            0xF4             //Memory Control Configuration register 2
#define MPC107_MCCR3            0xF8             //Memory Control Configuration register 3
#define MPC107_MCCR4            0xFC             //Memory Control Configuration register 4
#define MPC107_IMISR            0x100		     //Inbound Message Interrupt Status register
#define MPC107_IMIMR            0x104		     //Inbound Message Interrupt Mask register

#define MPC107_IFHPR            0x120            //Inbound Free_FIFO Head Pointer register
#define MPC107_IFTPR            0x128            //Inbound Free_FIFO Tail Pointer register
#define MPC107_IPHPR            0x130            //Inbound Post_FIFO Head Pointer register
#define MPC107_IPTPR            0x138            //Inbound Post_FIFO Tail Pointer register
#define MPC107_OFHPR            0x140            //Outbound Free_FIFO Head Pointer register
#define MPC107_OFTPR            0x148            //Outbound Free_FIFO Tail Pointer register
#define MPC107_OPHPR            0x150            //Outbound Post_FIFO Head Pointer register
#define MPC107_OPTPR            0x158            //Outbound Post_FIFO Tail Pointer register
#define MPC107_MUCR             0x164            //Message Unit Control register
#define MPC107_QBAR             0x170            //Queue Base Address register

#define MPC107_OMBAR            0x300
#define MPC107_OTWR             0x308
#define MPC107_ITWR             0x310
#define MPC107_WP_CONTROL       0xF48

#define MPC107_TWR_64KB         0x0F
#define MPC107_TWR_128KB        0x10
#define MPC107_TWR_64MB         0x19
#define MPC107_TWR_128MB        0x1A
#define MPC107_TWR_256MB        0x1B
#define MPC107_TWR_512MB        0x1C
#define MPC107_OPQI             0x20000000
#define MPC107_WP_TRIG01        0xc0000000

#define MPC107_QBAR_DEFAULT     0x100000
#define MPC107_MUCR_CQS_FIFO4K  0x00000002
#define MPC107_MUCR_CQE_ENABLE  0x00000001
#define MPC107_IMISR_IM0I       0x00000001		 //Inbound Message 0 Interrupt
#define MPC107_IMISR_IM1I       0x00000002		 //Inbound Message 1 Interrupt
#define MPC107_IMISR_IPQI       0x00000020		 //Inbound Post Queue Interrupt

#define MPC107_PPC_OMBAR        0x2300           //Outbound Memory Base Address register
#define MPC107_PPC_OTWR         0x2308           //Outbound Translation Window register
#define MPC107_PPC_ITWR         0x2310           //Inbound Translation Window register

#define MPC107_PICR1               0xA8         //Processor Interface Configuration register 1
#define PICR1_CF_MP_MULTI          0x00000003
#define PICR1_SPEC_PCI             0x00000004
#define PICR1_CF_APARK             0x00000008
#define PICR1_CF_LOOP_SNOOP        0x00000010
#define PICR1_CF_LE_MODE           0x00000020
#define PICR1_ST_GATH_EN           0x00000040
#define PICR1_NO_BUS_WIDTH_CHECK   0x00000080
#define PICR1_TEA_EN               0x00000400
#define PICR1_MCP_EN               0x00000800
#define PICR1_FLASH_WR_EN          0x00001000
#define PICR1_CF_LBA_EN            0x00002000
#define PICR1_PROC_TYPE_7XX        0x00040000
#define MPC107_PICR1_DEFAULT       PICR1_SPEC_PCI|PICR1_CF_APARK|PICR1_CF_LOOP_SNOOP|PICR1_ST_GATH_EN|PICR1_TEA_EN|PICR1_MCP_EN|PICR1_FLASH_WR_EN|PICR1_PROC_TYPE_7XX

#define MPC107_PICR2               0xAC         //Processor Interface Configuration register 2
#define PICR2_CF_LBCLAIM_WS        0x00000600
#define PICR2_NO_SNOOP_EN          0x08000000
#define MPC107_PICR2_DEFAULT       PICR2_CF_LBCLAIM_WS

#define MPC107_PMCR1               0x70         //Peripheral Logic Power Management Configuration register 1
#define PMCR1_SLEEP                0x0008
#define PMCR1_NAP                  0x0010
#define PMCR1_DOZE                 0x0020
#define PMCR1_BR1_WAKE             0x0040
#define PMCR1_PM                   0x0080
#define PMCR1_LP_REF_EN            0x1000
#define PMCR1_NO_SLEEP_MSG         0x4000
#define PMCR1_NO_NAP_MSG           0x8000
#define PMCR1_NO_MSG               0xC000
#define MPC107_PMCR1_DEFAULT       PMCR1_DOZE|PMCR1_BR1_WAKE|PMCR1_LP_REF_EN|PMCR1_NO_MSG

#endif /* _CONSTANTS_H */