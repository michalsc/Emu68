# PiStorm multi-ROM selector

This variant can select one of up to 16 Kickstart images at boot. It does not
contain or download Kickstart ROMs; supply ROM files that you are licensed to use.
It is intended for PiStorm and PiStorm32/32-Lite builds.

```sh
python3 tools/make_rom_pack.py kickstarts.pack kick13.rom kick31.rom
```

Copy `kickstarts.pack` to the FAT32 boot partition and set `initramfs
kickstarts.pack` in `config.txt`. Select the zero-based slot in the single line
of `cmdline.txt`, for example:

```text
rom_slot=1
```

Hold Ctrl-Amiga-Amiga for about three seconds to advance to the next ROM and
wrap around after the last slot. A short keyboard reset keeps its original
behaviour and does not change slots. All ROMs in a keyboard-switchable pack
must have the same size. Slot `0` is the default and fallback. Plain 256 KiB,
512 KiB, 1 MiB and 2 MiB ROM files remain supported. In the example, slot 0 is
`kick13.rom` and slot 1 is `kick31.rom`.
