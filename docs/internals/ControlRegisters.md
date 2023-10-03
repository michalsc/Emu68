---
layout: default
title: Emu68 Control Registers
parent: Emu68 Internals
---

# Emu68 Control Registers

| Register name    | Number    | RW   | Size | Description                                          |
| ---------------- | --------- | ---- | ---- | ---------------------------------------------------- |
| ``CNTFRQ``       | ``0xe0``  | RO   | LONG | Frequency in Hz of the free running counter          |
| ``CNTVALLO``     | ``0xe1``  | RO   | LONG | Free running counter value, lower 32 bits            |
| ``CNTVALHI``     | ``0xe2``  | RO   | LONG | Free running counter value, higher 32 bits           |
| ``INSNCNTLO``    | ``0xe3``  | RO   | LONG | Number of executed M68k instructions, lower 32 bits  |
| ``INSNCNTHI``    | ``0xe4``  | RO   | LONG | Number of executed M68k instructions, higher 32 bits |
| ``ARMCNTLO``     | ``0xe5``  | RO   | LONG | Number of executed ARM instructions, lower 32 bits   |
| ``ARMCNTHI``     | ``0xe6``  | RO   | LONG | Number of executed ARM instructions, higher 32 bits  |
| ``JITSIZE``      | ``0xe7``  | RO   | LONG | Total cache size in bytes                            |
| ``JITFREE``      | ``0xe8``  | RO   | LONG | Number of bytes free in the JIT cache                |
| ``JITCOUNT``     | ``0xe9``  | RO   | LONG | Number of JIT units in the cache                     |
| ``JITSCFTHRESH`` | ``0xea``  | RW   | LONG | JIT threshold for soft cache flushes                 |
| ``JITCTRL``      | ``0xeb``  | RW   | LONG | JIT control register                                 |
| ``JITCMISS``     | ``0xec``  | RO   | LONG | Number of JIT cache misses                           |
| ``DBGCTRL``      | ``0xed``  | RW   | LONG | Debug control register                               |
| ``DBGADDRLO``    | ``0xee``  | RW   | LONG | Lowest debug address                                 |
| ``DBGADDRHI``    | ``0xef``  | RW   | LONG | Highest debug address                                |
| ``JITCTRL2``     | ``0x1e0`` | RW   | LONG | JIT control register 2                               |

## CNTFRQ - Counter frequency

AArch64 features a free running 64-bit counter which can be used for timing purposes. This counter is exposed to the M68k and can be freely used by the software. The frequency of the counter is available through this register.

## CNTVALLO, CNTVALHI - Free running counter

The value of free running counter is available through two registers. ``CNTVALLO`` contains lower 32 bits of the free running counter, whereas ``CNTVALHI`` contains the upper 32 bits. In order to make sure that the counter is read properly, i.e. that the lower 32 bit did not wrap between reading lower and higher longword, it is advisable to read CNTVALHI twice. If the value has changed on second read, it means that the lower 32 bits have wrapped and register read procedure should be repeated.

```
# Read CNTVAL register into d0:d1 pair.
ReadCNT:
        move.l  d2, -(a7)
1:      movec.l #0xe2, d2
        movec.l #0xe1, d1
        movec.l #0xe2, d0
        cmp.l   d0, d2
        bne.b   1b
        move.l  (a7)+, d2
        rts
```

## INSNCNTLO, INSNCNTHI - M68k instruction counter

Emu68 provides a real time counter of executed M68k instructions. The value of this 64 bit counter, stored in two read only control registers, allows one to learn about current performance of Emu68.

## ARMCNTLO, ARMCNTHI - ARM instruction counter

Current count of executed ARM instructions, including translated JIT code as well as exceptions, translator and main JIT loop. 

## JITSIZE - JIT cache sile

Total size of JIT cache in bytes. This value is defined once during compilation.

## JITFREE - JIT cache free

Number of free bytes in JIT cache.

## JITCOUNT - JIT unit count

This register contains number of JIT units available in the cache at the moment.

## JITSCFTHRESH - Soft flush threshold

The soft flush of JIT cache, controlled by the ``JITCTRL`` register is time consuming, since the entire cache has to be walked through. If the JIT cache contains less entries than the threshold value, a soft flush will be eventually applied. If number of units exceeds the threshold, regular cache flush will be applied regardless of ``JCC_SOFT`` bit value.

## JITCTRL - JIT control register

Configures behaviour of JIT translator. 

| Name                 | Offset | Field size | Description                    |
| -------------------- | ------ | ---------- | ------------------------------ |
| ``JCC_SOFT``         | 0      | 1          | Use "soft flush" of JIT cache. |
| ``JCC_LOOP_COUNT``   | 4      | 4          | Inline loop count              |
| ``JCC_INLINE_RANGE`` | 8      | 16         | Maximal distance for inline    |
| ``JCC_INSN_DEPTH``   | 24     | 8          | Maximal JIT unit size          |

### JCC_SOFT

If this bit is set, instruction cache flush does not remove units from the JIT cache. Instead, they are marked as not verified. On next execution of the code the CRC32 checksum of the unit will be verified and, if unchanged, the unit will be marked as valid, omitting compilation phase.

### JCC_LOOP_COUNT

If JIT Translator finds a way to unroll the loop in the code, it will attempt to fit up to ``JCC_LOOP_COUNT`` loops, provided there is enough place to fit given number of m68k instructions into the cache.

### JCC_INLINE_RANGE

When JIT translator finds a branch (conditional or unconditional) with target address computable during compilation time, the branch will be inlined into current JIT translation unit if the branch distance is within a proximity given by ``JCC_INLINE_RANGE`` in bytes. Value of ``0`` disables branch inlining.

### JCC_INSN_DEPTH

Translator will put not more than ``JCC_INSN_DEPTH`` m68k instructions within single JIT compilation unit. Value of ``0`` sets maximal number of instructions to ``256``. It must be noted that the JIT unit can contain less m68k instructions than the value set here, since every branch which is not computable during compilation phase as well as many context-synchronising instructions will break the translation.

## JITCMISS - Cache miss counter

The value of this 32 bit counter is increased every time a JIT cache miss occurred and the JIT compiler is started.

## DBGCTRL - Debug control register

Configures behaviour of debug messages. It can be switched on the fly to change verbosity of debug messages as well as to switch disassemble of translated code on or off. The change affects only the newly compiled units, therefore, it is advisable to flush entire code cache after applying any changes here.

| Name           | Offset | Field size | Description                  |
| -------------- | ------ | ---------- | ---------------------------- |
| ``DC_VERBOSE`` | 0      | 2          | Set verbosity level of debug |
| ``DC_DISASM``  | 2      | 1          | Enable/disable disassembler  |

## DBGADDRLO, DBGADDRHI - Debug range

Debug information about  JIT units is usually shown for all blocks of the memory going into the translator. Since such debug can be extremely huge (above 200 megabytes on regular system boot), the range where the verbosity of JIT units is elevated through ``DBGCTRL`` register may be limited. If M68k address is not within a range between ``DBGADDRLO`` and ``DBGADDRHI``, no information about such JIT unit will be written to the console.

## JITCTRL2 - second JIT control register

Second control register influencing behavior of Emu68

| Name                   | Offset | Field size | Description                                  |
| ---------------------- | ------ | ---------- | -------------------------------------------- |
| ``JC2_CHIP_SLOWDOWN``  | 0      | 1          | Slow down code executing from CHIP memory    |
| ``JC2_DBF_SLOWDOWN``   | 1      | 1          | Slow down special case of DBF busy loops     |
| ``JC2_CCR_SCAN_DEPTH`` | 3      | 5          | Controls forward scan depth of CCR optimizer |

### JC2_CHIP_SLOWDOWN

If this bit is set, Emu68 will add a word read from current PC location before every translated m68k instruction. This setting will make code executed from CHIP memory significantly slower. Might be used in case of some ancient software designed for much slower CPUs.

### JC2_DBF_SLOWDOWN

This bit slows down special case of DBF instruction often used e.g. in old MOD replayers as a busy loop delay:

```assembly
        move.w #xxx, Dn
loop:   dbf    Dn, loop
```

Due to nature of Emu68 such busy loops are much faster then expected. When this bit is set, each DBF executed from CHIP memory branching to itself will take the same amount of time as three subsequent byte reads from CHIP.

### JC2_CCR_SCAN_DEPTH

When Emu68 is translating m68k code to AArch64 code, it perform forward scanning of further m68k instructions to estimate if and, if yes, which bits of CCR should be updated. This greatly reduces amount of generated AArch64 code, but might be prone to errors e.g. in case of self-modifying code. By adjusting JC2_CCR_SCAN_DEPTH field it is possible to instruct Emu68 how many opcodes shall be scanned in advance. Valid values vary from 0 (CCR optimization completely disabled) up to 31. Default value on startup of Emu68 is 20.

