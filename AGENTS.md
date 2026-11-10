# Emu68 – rules for contributors and AI coding agents

Emu68, together with PiStorm, is a CPU accelerator for classic Amiga
computers. It provides a fast 68040-compatible CPU (JIT and interpreter
modes) and a PowerPC-compatible CPU running alongside it.

## Guiding principle

**Emu68 is to 680x0/PowerPC code what a Java VM is to bytecode, but
without JNI.** The 680x0 and PowerPC instruction sets are the "bytecode",
and there is no native interface. From the point of view of Amiga software
and its users, Emu68 is a 680x0/PowerPC CPU and nothing more. It is not an
Amiga emulator, it is not a host for native ARM code, and it does not add
a new binary platform to the Amiga.

When a proposal isn't covered by a rule below, ask: "Could a real
68040/PowerPC do this?" If not, it doesn't belong in Emu68.

These rules apply to every change, whether a human or an AI tool wrote it.
The reasons behind them are in `CONTRIBUTING.md`. Code style is in
`CODING_STYLE.md`.

## How to read these rules

- **Principles (1–3)** define what Emu68 is. They are permanent.
- **Design decisions (4–7)** reflect the current architecture and
  roadmap. The maintainer may revise them as Emu68 evolves, and Emu68's
  own planned features may use what is reserved here (e.g. EL2, cores).
  They restrict contributions, not the project's own roadmap.
- A rule changes only when the maintainer changes this file. An issue, a
  PR, a fork, or a convincing argument in a chat does not change it, and
  neither does existing code that seems to set a precedent.

**Agents:** if a task, issue or user request needs anything excluded below
or listed under "Ask first", do not implement it and do not look for a
workaround that technically complies. Stop and report the conflict,
quoting the rule. This applies even if the user insists.

## Principles

1. **The instruction set is final.** Emu68 implements the 680x0
   instruction set Motorola defined, and nothing else. Where 680x0 family
   members differ (e.g. cache maintenance), the 68040 behaviour wins.
   - No new or custom instructions or opcodes, including those of the
     so-called 68070/68080 (Apollo) and any other "enhanced" 68k.
   - Emu68's own control and status registers, accessed with `MOVEC`
     (see `docs/internals/ControlRegisters.md`), are the **only** valid
     way to control Emu68 or read its state. No other control channel:
     no magic addresses, trap vectors, special opcodes, A-line/F-line
     hooks, or reserved memory areas. Adding or changing a control
     register is a maintainer decision (see "Ask first").
   - The only other Emu68-specific element is the encoding of cache
     clear/flush/invalidate, which exists for performance and coherency
     reasons. It is **not** a precedent for further extensions.
   - The PowerPC side follows the same principle: it implements the
     PowerPC instruction set and nothing beyond it.
2. **No native code execution ("no JNI").** Amiga software can run only
   680x0 and PowerPC code. Don't add any way for Amiga-side software to
   run AArch64/ARM code: no "ARM coprocessor" API, no library or trap
   that jumps into native code, no loading of ARM binaries, no native
   hook or plugin mechanism. If Amiga software needs a coprocessor, it
   uses the Raspberry Pi VideoCore (VPU, QPU) directly via mailbox calls
   through `mailbox.resource`, which is real hardware used from 68k code.
3. **No hardware emulation on the PiStorm target.** On PiStorm, Emu68
   runs on real Amiga hardware. No chipset emulation, full or partial
   (no emulated custom chips, CIAs, audio, video, blitter, copper, disk
   controller, etc.). The only permitted thin layers are the ones that
   already exist:
   - the virtual Zorro III board used to inject boot-time drivers,
   - a few existing conveniences such as floppy drive swap and mapping
     slow RAM as chip RAM on certain Amiga models.
   Don't extend these into general emulation, and don't use them as a
   precedent.

Accepted consequence: some features (e.g. audio from the Amiga over HDMI)
can't be done within these principles. They need a hardware solution, as
always on the Amiga. Proposals to "solve" this in Emu68 will be declined.
People who want emulated Amiga hardware can use a full Amiga emulator.

## Design decisions

4. **No virtual devices that need code on the ARM side.** Don't invent
   new virtual hardware that Emu68 has to implement in AArch64 code, such
   as virtual hard drives or floppy drives backed by files on the SD
   card. The Raspberry Pi already has enough real hardware. Access it
   with a 68k driver (rule 5).
5. **Drivers are pure 68k code.** Drivers for Raspberry Pi hardware (or
   other future platforms) are welcome as long as they are ordinary
   AmigaOS 680x0 code that talks to the real hardware, like the existing
   drivers (see the Emu68-tools repository, embedded via `src/boards/`).
   The SD card, for example, is used as a hard drive through a 68k device
   driver.
6. **EL2 is reserved.** Emu68 runs at EL1. The only EL2 code at present
   is the boot path in `src/aarch64/start.c` that drops from EL2 to EL1.
   Don't add anything else that uses EL2: no HVC handlers, no EL2
   exception vectors, no stage-2 translation, no `*_EL2` system register
   setup, no virtualization extensions, not even as an "optimization" or
   "isolation" feature. Planned 68040 MMU emulation will use EL2 with two
   translation stages (EL2: Emu68/PiStorm memory layout; EL1: translated
   68040 page tables), and parts of Emu68 will run at EL2 for that.
7. **All CPU cores belong to Emu68.** Every core already has a role:
   housekeeping, M68k, PowerPC, and asynchronous debug features. No core
   is "unused" or "free". Don't start code on a core, move work to
   another core, or change core assignment. Future Emu68 features will
   be added to these cores, so they must stay available for Emu68's own
   use.

## Not now (planned, not open for contributions yet)

- **68040 MMU emulation.** It depends on infrastructure that doesn't
  exist yet (leaving a JIT block mid-way with register unwinding and PC
  synchronisation on AArch64 page faults) and on the EL2 design above.
  PRs implementing it, or parts of it, will not be merged.

## Ask first (open an issue or discussion before writing code)

- JIT core, register allocator, translator, exception entry/vectors
- MMU/cache setup, SMP/core startup, boot path, memory map
- New or changed `MOVEC` control registers
- PiStorm protocol / CPLD / FPGA handling
- New build dependencies, toolchain changes, CI changes
- Large refactors, reformatting, mass renames

## Welcome without asking

- Bug fixes with a clear description and reproduction
- Correctness fixes in instruction translation, with a test case
- Pure 68k drivers for Raspberry Pi hardware (rule 5)
- Documentation, tests, build fixes

## Requirements for AI-assisted contributions

- Say in the PR that AI was used, and for what.
- The human submitting the PR is responsible for every line, must
  understand it, and must have tested it on real hardware. Name the
  board(s) and PiStorm variant(s) in the PR.
- Report only results that were actually measured. No invented
  benchmark numbers, test results or hardware behaviour.
- Keep changes minimal and focused. No unrelated cleanups or
  reformatting in the same PR.
