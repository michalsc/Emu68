---
layout: default
title: Emu68 Control Registers
parent: Emu68 Internals
---

# Emu68 Control Registers



| Register name    | Number   | RW   | Size | Description                                          |
| ---------------- | -------- | ---- | ---- | ---------------------------------------------------- |
| ``CNTFRQ``       | ``0xe0`` | RO   | LONG | Frequency in Hz of the free running counter          |
| ``CNTVALLO``     | ``0xe1`` | RO   | LONG | Free running counter value, lower 32 bits            |
| ``CNTVALHI``     | ``0xe2`` | RO   | LONG | Free running counter value, higher 32 bits           |
| ``INSNCNTLO``    | ``0xe3`` | RO   | LONG | Number of executed M68k instructions, lower 32 bits  |
| ``INSNCNTLO``    | ``0xe4`` | RO   | LONG | Number of executed M68k instructions, higher 32 bits |
| ``ARMCNTLO``     | ``0xe5`` | RO   | LONG | Number of executed ARM instructions, lower 32 bits   |
| ``ARMCNTHI``     | ``0xe6`` | RO   | LONG | Number of executed ARM instructions, higher 32 bits  |
| ``JITSIZE``      | ``0xe7`` | RO   | LONG | Total cache size in bytes                            |
| ``JITFREE``      | ``0xe8`` | RO   | LONG | Number of bytes free in the JIT cache                |
| ``JITCOUNT``     | ``0xe9`` | RO   | LONG | Number of JIT units in the cache                     |
| ``JITSCFTHRESH`` | ``0xea`` | RW   | LONG | JIT threshold for soft cache flushes                 |
| ``JITCTRL``      | ``0xeb`` | RW   | LONG | JIT control register                                 |
| ``JITCMISS``     | ``0xec`` | RO   | LONG | Number of JIT cache misses                           |
| ``DBGCTRL``      | ``0xed`` | RW   | LONG | Debug control register                               |
| ``DBGADDRLO``    | ``0xee`` | RW   | LONG | Lowest debug address                                 |
| ``DBGADDRHI``    | ``0xef`` | RW   | LONG | Highest debug address                                |



