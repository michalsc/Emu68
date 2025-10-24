---
layout: default
title: M68k register mapping
parent: Emu68 Internals
---

# PowerPC register mapping

Registers GPR0..GPR13 have fixed mapping onto AArch64 general purpose registers. Registers GPR14..GPR31 are
dynamically allocated and assigned to the free entries in W0..W11 pool. Registers FPR14..FPR31 are dynamically 
allocated and assigned to the free entries in D0..D7,D29..D31 pool.

Register ``X12`` is a pointer to the entry point of currently used JIT block.

| PowerPC register         | AArch64 register             | Description                                      |
| ------------------------ | ---------------------------- | ------------------------------------------------ |
| *JIT Entry Point*        | ``X12``                      | Current AArch64 JIT Entry point                  |
| ``GPR0``                 | ``W13``                      | General purpose data register                    |
| ``GPR1``                 | ``W14``                      | General purpose data register                    |
| ``GPR2``                 | ``W15``                      | General purpose data register                    |
| ``GPR3``                 | ``W16``                      | General purpose data register                    |
| ``GPR4``                 | ``W17``                      | General purpose data register                    |
| ``PC``                   | ``W18``                      | Program counter                                  |
| ``GPR5``                 | ``W19``                      | General purpose data register                    |
| ``GPR6``                 | ``W20``                      | General purpose data register                    |
| ``GPR7``                 | ``W21``                      | General purpose data register                    |
| ``GPR8``                 | ``W22``                      | General purpose data register                    |
| ``GPR9``                 | ``W23``                      | General purpose data register                    |
| ``GPR10``                | ``W24``                      | General purpose data register                    |
| ``GPR11``                | ``W25``                      | General purpose data register                    |
| ``GPR12``                | ``W26``                      | General purpose data register                    |
| ``GPR13``                | ``W27``                      | General purpose data register                    |
| ``LR``                   | ``W28``                      | Link register                                    |
| ``CTR``                  | ``W29``                      | Counter/branch register                          |
| ``FPR0``                 | ``D8``                       | Floating point register                          |
| ``FPR1``                 | ``D9``                       | Floating point register                          |
| ``FPR2``                 | ``D10``                      | Floating point register                          |
| ``FPR3``                 | ``D11``                      | Floating point register                          |
| ``FPR4``                 | ``D12``                      | Floating point register                          |
| ``FPR5``                 | ``D13``                      | Floating point register                          |
| ``FPR6``                 | ``D14``                      | Floating point register                          |
| ``FPR7``                 | ``D15``                      | Floating point register                          |
| ``FPR8``                 | ``D16``                      | Floating point register                          |
| ``FPR9``                 | ``D17``                      | Floating point register                          |
| ``FPR10``                | ``D18``                      | Floating point register                          |
| ``FPR11``                | ``D19``                      | Floating point register                          |
| ``FPR12``                | ``D27``                      | Floating point register                          |
| ``FPR13``                | ``D28``                      | Floating point register                          |
| ``FPR29``                | ``D29``                      | Floating point register                          |
| ``FPR30``                | ``D30``                      | Floating point register                          |
| ``FPR31``                | ``D31``                      | Floating point register                          |
| ``FPSCR``                | ``V20.S[0]``                 | FPU Status and Control register                  |
| *EPOCH*                  | ``V20.S[1]``                 | Cache EPOCH counter.                             |
| *last PC*                | ``V20.S[3]``                 | Program counter of previously executed JIT block |
| ``INSN_COUNT``           | ``V21.D[0]``                 | 64-bit M68k instruction counter                  |
| *m68k context*           | ``V21.D[1]``                 | Pointer to M68k context of JIT machine           |
| ``GPR14``                | ``V22.S[0]``                 | General purpose data register                    |
| ``GPR15``                | ``V22.S[1]``                 | General purpose data register                    |
| ``GPR16``                | ``V22.S[2]``                 | General purpose data register                    |
| ``GPR17``                | ``V22.S[3]``                 | General purpose data register                    |
| ``GPR18``                | ``V23.S[0]``                 | General purpose data register                    |
| ``GPR19``                | ``V23.S[1]``                 | General purpose data register                    |
| ``GPR20``                | ``V23.S[2]``                 | General purpose data register                    |
| ``GPR22``                | ``V23.S[3]``                 | General purpose data register                    |
| ``GPR22``                | ``V24.S[0]``                 | General purpose data register                    |
| ``GPR23``                | ``V24.S[1]``                 | General purpose data register                    |
| ``GPR24``                | ``V24.S[2]``                 | General purpose data register                    |
| ``GPR25``                | ``V24.S[3]``                 | General purpose data register                    |
| ``GPR26``                | ``V25.S[0]``                 | General purpose data register                    |
| ``GPR27``                | ``V25.S[1]``                 | General purpose data register                    |
| ``GPR28``                | ``V25.S[2]``                 | General purpose data register                    |
| ``GPR29``                | ``V25.S[3]``                 | General purpose data register                    |
| ``GPR30``                | ``V26.S[0]``                 | General purpose data register                    |
| ``GPR31``                | ``V26.S[1]``                 | General purpose data register                    |
| ``CR``                   | ``V26.S[2]``                 | CR Register                                      |
| ``XER``                  | ``V26.S[3]``                 | XER Register                                     |
