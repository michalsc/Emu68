# Contributing to Emu68

Thank you for wanting to help. Please read this page before opening a
pull request. It explains what Emu68 is meant to be, and what it is
deliberately not, so your time doesn't go into work that can't be
merged.

The binding, compact list of rules is in [`AGENTS.md`](AGENTS.md). It is
written for both people and AI coding tools. This page explains the
reasons behind it. Code style: [`CODING_STYLE.md`](CODING_STYLE.md).

## What Emu68 is

Emu68 together with PiStorm is an **accelerator for classic Amiga
computers**. It gives the Amiga a very fast 68040-compatible CPU, with
JIT and interpreter modes, and a PowerPC-compatible CPU running in
parallel.

The best way to think about it: **Emu68 is to 680x0/PowerPC code what a
Java VM is to bytecode, but without JNI.** The 680x0 and PowerPC
instruction sets are the bytecode, and there is no native interface.
From the user's point of view, Emu68 is nothing more than a 680x0 or
PowerPC CPU would be.

It is therefore **not** an Amiga emulator, and it does **not** add a new
CPU platform to the Amiga. Two goals follow from that:

- keep the classic Amiga environment clean. Software written for an
  Emu68 machine must also make sense on a real 680x0/PPC Amiga.
- don't create yet another binary platform that splits the Amiga
  software base.

If you're unsure whether an idea fits, ask yourself: "Could a real
68040 or PowerPC CPU do this?" If not, it doesn't belong in Emu68.

## Principles and design decisions

The rules in `AGENTS.md` come in two kinds:

- **Principles** define what Emu68 is: no instruction set extensions,
  no native code, no hardware emulation on PiStorm. They won't change.
- **Design decisions** follow from the current architecture and roadmap:
  no ARM-side virtual devices, drivers in pure 68k code, EL2 reserved,
  all CPU cores reserved. They limit what contributions may do. Emu68's
  own roadmap may use what is reserved, and the maintainer may revise
  them as Emu68 evolves.

Either kind changes only when the maintainer updates `AGENTS.md`. A pull
request, a popular fork, or existing code that looks like a precedent
doesn't change it.

## What we won't accept, and why

### Instruction set extensions
Emu68 implements the 680x0 instruction set as Motorola defined it. Where
family members differ (cache maintenance, for example), Emu68 follows
the 68040. We don't implement custom or third-party extensions such as
the instructions of the so-called 68070/68080 (Apollo).

Software that uses such instructions runs on only one accelerator, and
that splits the software base.

Emu68 does need to be controlled and to report its state (JIT settings,
counters, debug features). For that it provides its own control
registers, accessed with the standard `MOVEC` instruction, exactly as a
real CPU exposes its control registers. They are documented in
[`docs/internals/ControlRegisters.md`](docs/internals/ControlRegisters.md)
and are the **only** valid way to control Emu68. There are no magic
addresses, special traps or extra opcodes for this. The only other
Emu68-specific element is the encoding of cache clear/flush/invalidate,
needed for performance and coherency of the JIT. It is a narrow technical
necessity, not a precedent.

### Running native ARM code
This is the "no JNI" part. Emu68 and the PiStorm team don't want to add a
third CPU architecture to the Amiga. Amiga software gets 680x0 and
PowerPC, and that's all. If you need extra compute, the Raspberry Pi's
VideoCore (VPU and QPU) can be used directly from AmigaOS through mailbox
calls via `mailbox.resource`. This uses the real hardware, as Amiga
software always has.

### Hardware or chipset emulation on PiStorm
On PiStorm, Emu68 runs inside a real Amiga with its real chipset.
Emulating that chipset, fully or partly, is out of scope. The few thin
layers that exist (the virtual Zorro III board that injects boot
drivers, floppy drive swap, mapping slow RAM as chip RAM on some models)
are deliberately minimal and won't be extended.

This means some things can't be done in Emu68. For example, audio from
the Amiga over HDMI needs a hardware solution. That's how Amiga hardware
has always worked, and we don't intend to change it. If you want
emulated Amiga hardware, a full Amiga emulator is the right tool.

### Virtual devices
We don't add new virtual hardware that Emu68 has to implement on the ARM
side, for example virtual hard drives or floppy drives backed by files on
the SD card. The Raspberry Pi has plenty of real hardware. The SD card is
used as a hard drive through a 68k device driver, and that is the pattern
to follow.

### Hypervisor mode (EL2)
Emu68 runs at EL1. EL2 is reserved for the planned 68040 MMU emulation,
which will use two levels of translation: EL2 for the Emu68/PiStorm
memory layout and EL1 for page tables translated from 68040 page
tables. Parts of Emu68 will run at EL2 for this. Any other use of EL2
would conflict with that design, so it won't be accepted.

### "Unused" CPU cores
There are none. Emu68 manages all cores and already uses each of them:
housekeeping, M68k, PowerPC and asynchronous debug features. New Emu68
functionality will be added to these cores in the future, so they must
stay available for Emu68's own needs. Don't put your own work on a core.

## What we're glad to receive

- **Drivers** for Raspberry Pi hardware (or future platforms), written as
  pure 680x0 AmigaOS code that talks to the real hardware, like the
  existing drivers in the Emu68-tools repository.
- Bug fixes and instruction-translation correctness fixes, ideally with
  a test case.
- Documentation, tests and build fixes.

## Planned but not open yet

68040 MMU emulation is on the long-term roadmap. It needs infrastructure
that doesn't exist yet, so please don't send implementations of it.

## Before you start bigger work

Open an issue or discussion first if you want to touch the JIT core,
register allocator, exception handling, MMU/cache setup, core startup,
boot path, control registers, PiStorm protocol, toolchain or CI, or if
you plan a large refactor.

## Using AI tools

AI-assisted contributions are fine under these conditions:

- Say in the PR that you used AI, and for what.
- You are responsible for the change. You must understand every line
  and have tested it on real hardware.
- Report only results you actually measured.
- Your AI tool should read `AGENTS.md`. Most tools load it
  automatically; Claude Code loads it via `CLAUDE.md`. If your tool
  suggests something that `AGENTS.md` excludes, the answer is no, even
  if the tool argues otherwise.
