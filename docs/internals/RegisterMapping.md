---
layout: default
title: M68k register mapping
parent: Emu68 Internals
---

# M68k register mapping

In order to maintain good execution speed of M68k code, Emu68 keeps most important registers and context pointer in ARM registers all the time. Thanks to that memory (cache) fetches and stores are reduced to the necessary minimum. The following table shows the assignment. Please note the ``Dn`` and ``Vn`` registers are the same bank, whereas ``Dn`` writes clear entire 128 bit clearing the topmost 64-bits, and ``Vn`` contents such as ``INSN_COUNT`` or ``CACR`` are accessed as low-size components of ``Vn`` vectors.

| M68k register            | AArch64 register             | Description                                      |
| ------------------------ | ---------------------------- | ------------------------------------------------ |
| ``D0...D7``              | ``W19...W26``                | General purpose data register                    |
| ``A0...A4``, ``A5...A7`` | ``W13...W17``, ``W27...W29`` | General purpose address register                 |
| ``PC``                   | ``W18``                      | Program counter                                  |
| ``FP0...FP7``            | ``D8...D15``                 | Floating point register                          |
| ``SR``                   | ``TPIDR_EL0``                | Status register                                  |
| ``FPSR``                 | ``V29.S[0]``                 | FPU Status register                              |
| ``FPIAR``                | ``V29.S[1]``                 | FPU Instruction address register                 |
| ``FPCR``                 | ``V29.H[4]``                 | FPU Control register                             |
| ``CACR``                 | ``V31.S[0]``                 | Cache control register                           |
| ``INSN_COUNT``           | ``V30.D[0]``                 | 64-bit M68k instruction counter                  |
| *last PC*                | ``TPIDR_EL1``                | Program counter of previously executed JIT block |
| *m68k context*           | ``TPIDRRO_EL0``              | Pointer to M68k context of JIT machine           |



