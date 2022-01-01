---
layout: default
title: Emu68 Control Registers
parent: Emu68 Internals
---

# Emu68 Control Registers



| Register name    | Number   | Description                                          |
| ---------------- | -------- | ---------------------------------------------------- |
| ``CNTFRQ``       | ``0xe0`` | Frequency in Hz of the free running counter          |
| ``CNTVALLO``     | ``0xe1`` | Free running counter value, lower 32 bits            |
| ``CNTVALHI``     | ``0xe2`` | Free running counter value, higher 32 bits           |
| ``INSNCNTLO``    | ``0xe3`` | Number of executed M68k instructions, lower 32 bits  |
| ``INSNCNTLO``    | ``0xe4`` | Number of executed M68k instructions, higher 32 bits |
| ``ARMCNTLO``     | ``0xe5`` | Number of executed ARM instructions, lower 32 bits   |
| ``ARMCNTHI``     | ``0xe6`` | Number of executed ARM instructions, higher 32 bits  |
| ``JITSIZE``      | ``0xe7`` | Total cache size in bytes                            |
| ``JITFREE``      | ``0xe8`` | Number of bytes free in the JIT cache                |
| ``JITCOUNT``     | ``0xe9`` | Number of JIT units in the cache                     |
| ``JITSCFTHRESH`` | ``0xea`` | JIT trethold for soft cache flushes                  |
| ``JITCTRL``      | ``0xeb`` | JIT control register                                 |
| ``JITCMISS``     | ``0xec`` | Number of JIT cache misses                           |
| ``DBGCTRL``      | ``0xed`` | Debug control register                               |
| ``DBGADDRLO``    | ``0xee`` | Lowest debug address                                 |
| ``DBGADDRHI``    | ``0xef`` | Highest debug address                                |



