## What does this change do?

<!-- Short description. Link the issue/discussion if there is one. -->

## Checklist

- [ ] I have read [`AGENTS.md`](../AGENTS.md) and this change follows all hard constraints
      (no ISA extensions, no control channel other than MOVEC control registers,
      no native ARM code execution, no hardware/chipset emulation,
      no ARM-side virtual devices, no EL2 use, no use of "free" CPU cores)
- [ ] If this touches JIT core, exceptions, MMU/cache, core startup, boot path,
      control registers, PiStorm protocol, toolchain or CI: it was discussed first (link: )
- [ ] The change is focused; no unrelated reformatting or refactoring

## Testing

<!-- Required. Board(s) and PiStorm variant(s), what you ran, what you observed. -->
- Hardware:
- Tested with:
- Results (measured, not estimated):

## AI assistance

- [ ] No AI tools were used
- [ ] AI tools were used for: <!-- e.g. drafting code, tests, commit message -->
      and I have reviewed, understood and tested every line
