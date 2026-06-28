# Emu68 overlays

## Introduction

Starting with Emu68 1.1 the use of cmdline.txt for adjusting Emu68 parameters
is obsolete. Instead, device tree overlays can be injected through config.txt into
the device tree visible and maintained by Emu68.

This change has several advantages over the previous use of cmdline.txt. First, the
command line text file had to be really a single line. The startup subsystem of RaspberryPi
is not capable of merging several lines together. Further, the overlay mechanism allows one
to selectively load the overlay objects (.dtbo files) depending on the Pi model or depenending
on other parameters such as e.g. GPIO configuration at the moment where start.elf is active.

## Loading overlays

Loading the overlays is simple. Just open the config.txt file (or any part of config which
gets included) and add following:

```
dtoverlay=overlay_file_name
```

where ```overlay_file_name``` stands for the overlay that should be loaded, **without the .dtbo
extension**. If there are some boot-time adjustable parameters for that overlay, you can append
them, comma-separated, right after the overlay file name. For example, if unicam overlay shall 
be loaded, enabled on boot and set to smooth pixel scaling mode and scanlines parameter set to 3,
 the proper line for loading it would be

```
dtoverlay=unicam,boot,smooth,sc=3
```

Another way to parametrize the overlay is to use the ``dtparam=`` entry in config.txt, followed with
the parameter that should be applied to the overlay. This entry does refer to **the last loaded** 
overlay file and can be repeated many times. Therefore, one could re-write the example above to 
following version:

```
dtoverlay=unicam
dtparam=boot
dtparam=smooth
dtparam=sc=3
```

Please remember, one ``dtoverlay=`` loads a single overlay object, only. If you want to load another
overlay, simple put an additional line in config.txt starting with ``dtoverlay=``.

## Main overlay and CM4 antenna option

During the boot process of the raspberry pi, the start.elf file applies one "silent" overlay to the
device tree, which adjusts some of the device tree entries based on the Pi model. This overlay has 
no name, but may be very important for CM4 users willing to work with the WiFi and external antenna.
This overlay is active before and other overlay file has been loaded, but can be re-referenced in the
config file at any time by using empty overlay name, as shown below:

```
dtoverlay=
```

Once this overlay is activated, CM4 users can enable the external antenna by applying the ``ant2`` parameter:

```
dtparam=ant2
```

## List of overlays and their parameters

Below you will find the list of the overlays and their parameters

## emu68.dtbo

Main overlay controlling behavior of Emu68 with following parameters:

| Parameter             | Description                                                                     |
| --------------------- | ------------------------------------------------------------------------------- |
| ``no_fpu``            | Disables the FPU entirely. Every FPU instruction will throw an exception        |
| ``vbr_move``          | Forces 68040.library to move VBR to FAST memory                                 |
| ``m68k_jit_size=num`` | The integer ``num`` determines size of the M68k JIT cache in megabytes          |
| ``ppc_enable``        | Activates PPC JIT translator running in parallel on another AArch64 CPU core    |
| ``ppc_jit_size=num``  | The integer ``num`` determines size of the PPC JIT cache in megabytes           |
| ``ICNT=num``          | Maximal number of m68k instructions per JIT block is limited to at most ``num`` |
| ``CCRD=num``          | Maximal scan depth for CCR optimizer limited to ``num`` instructions            |
| ``IRNG=num``          | Maximal distance for branch inlining                                            |
| ``SC``                | Forces chip-slowdown by adding special delays into JIT-ed code                  |
| ``SCS=num``           | Instruction distance at which the slowdowns are added                           |
| ``FP0``               | When enabled, first 4KB will be mapped to ARM FAST memory instead of CHIP       |
| ``BW``                | When enabled, some chipset accesses will be held as long as blitter is active   |
| ``DBF``               | When enabled, empty DBF loop will be explicitly slowed down                     |

### diagnostic.dtbo
This overlay controls several diagnostic features of Emu68 with following parameters:

| Parameter                 | Description                                                                                    |
| ------------------------- | ---------------------------------------------------------------------------------------------- |
| ``buptesst``              | Enables simplistic bus test of the pistorm                                                     |
| ``bupiter=num``           | The integer ``num`` determines number of bus test loops to run                                 |
| ``bupsize=num``           | The integer ``num`` determines the number of bytes to test, starting from address 0            |
| ``membench``              | Enables small memory benchmark of the pistorm                                                  |
| ``membase=num``           | The integer ``num`` determines the base address where benchmark should work                    |
| ``memsize=num``           | The integer ``num`` determines the block size in bytes where benchmarks shall run              |
| ``debug_not_implemented`` | When enabled, unimplemented instructions will be also reported over the debug interface        |
| ``debug``                 | Enables debug output of JIT translator. Use with care as it will generate really a lot of data |
| ``disassemble``           | Enables side-by-side debug output of M68k/PPC and translated AArch64 code                      |
| ``dump_dt``               | Writes whole device tree to the debug output on Emu68 start                                    |

### noscsi.dtbo

This is a parameter-less overlay. When loaded, Emu68 will shadow IDE interface registers
effectively disallowing AmigaOS to detect any harddrive on the IDE bus. Useful if your
amiga tends to hang for a long time attempting to detect a non existing harddisk.

### ntsc.dtbo

This is a parameter-less overlay. When loaded, Emu68 will attempt to convince AmigaOS, that 
it is running on a NTSC Amiga model.

### pal.dtbo

This is a parameter-less overlay. When loaded, Emu68 will attempt to convince AmigaOS, that 
it is running on a PAL Amiga model.

### z2ram.dtbo

This overlay allows to add Zorro II memory mapped to fast ARM memory to the Amiga. This memory
will be autodetected and configured by AmigaOS automatically. By default, 4 megabytes are mapped, 
but the amount can be controlled through an overlay parameter:

| Parameter    | Description                                                                       |
| ------------ | --------------------------------------------------------------------------------- |
| ``size=num`` | The integer ``num`` determines size of the Zorro II memory expansion in megabytes |

### sdhc.dtbo

This overlay controls behavior of the ``brcm-sdhc.device`` driver used by Pi3 and PiZero2 models.
It controls the device driver through following parameters:

| Parameter       | Description                                                                |
| --------------- | -------------------------------------------------------------------------- |
| ``disable``     | Deactivates the device driver entirely                                     |
| ``low_speed``   | Disables high-speed (usually 50MHz) mode of operation                      |
| ``clock=num``   | Overclocks high-speed mode to ``num`` MHz                                  |
| ``verbose=num`` | Increases verbosity of the driver to ``num``                               |
| ``rawputchar``  | Additionaly redirects the debug output to RawPutChar exec function         |
| ``unit0=``      | Makes the unit 0 hidden (``off``), read-only (``ro``) or writable (``rw``) |

### emmc.dtbo

This overlay controls behavior of the ``brcm-emmc.device`` driver used by Pi4 and CM4 models.
It controls the device driver through following parameters:

| Parameter       | Description                                                                |
| --------------- | -------------------------------------------------------------------------- |
| ``disable``     | Deactivates the device driver entirely                                     |
| ``low_speed``   | Disables high-speed (usually 50MHz) mode of operation                      |
| ``clock=num``   | Overclocks high-speed mode to ``num`` MHz                                  |
| ``verbose=num`` | Increases verbosity of the driver to ``num``                               |
| ``rawputchar``  | Additionaly redirects the debug output to RawPutChar exec function         |
| ``unit0=``      | Makes the unit 0 hidden (``off``), read-only (``ro``) or writable (``rw``) |
| ``irq``         | Enables using of IRQs by the driver                                        |
| ``dma``         | Enables using of DMA by the driver, auto-enables IRQs too                  |

