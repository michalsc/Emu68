# Configuring Emu68

Since Emu68 JIT is not aware of surrounding hardware or peripherals attached to the machine (with only few exceptions) the configuration can be done only through the command line parameters. These can be set through the ``cmdline.txt`` file placed on the boot FAT32 partition together with Emu68.img file.

## Options in cmdline.txt

The cmdline.txt is a one-line file which is passed directly into the device tree available on the M68k through a resource. It can be found in the ``bootargs`` property of the ``/chosen`` node like in the following example:

```c
// The prototypes for devicetree.resource
#include <proto/devicetree.h>

APTR DeviceTreeBase = OpenResource("devicetree.resource");

/* Open /chosen node */
APTR key = DT_OpenKey("/chosen");

if (key) 
{
  APTR property = DT_FindProperty(key, "bootargs");
  if (property)
  {
    CONST_STRPTR bootargs = DT_GetPropValue(property);
    ULONG bootargs_len = DT_GetPropLen(property);
    
    /*
      Now bootargs points to a **NOT** terminated character sequence which can be
      searched for any necessary parameter.
    */
    [...]
  }
  DT_CloseKey(key);
}
```

Below are currently implemented options, grouped by the affected modules and/or functionalities.

### brcm-sdhc.device

* ``sd.verbose=0 | 1 | 2``  
  Adjust verbosity of the driver. Value ``0`` is default one and it shuts off debug nearly completely. Value ``1`` gives some more details from the driver, whereas value ``2`` should be used only for debugging purposes since it reports every single BeginIO call.
* ``sd.unit0=off | ro | rw`` 
  Change the behaviour of sdhc-brcm.device unit 0. Default value is ``ro`` which means the unit is not hidden, but cannot be written to. Changing the value to ``off`` disables the unit 0 completely whereas ``rw`` allows one to write data to it. Use with care, as Unit 0 of the device represents the entire card, including partition table and FAT32 boot partition.
* ``sd.low_speed`` 
  Disables 50 MHz clock even if the microSD card reports that the clock is supported. Setting this option disables SD card overclocking (see subsequent option).
* ``sd.clock=num`` 
  Selects the clock speed in MHz which will be switched on instead of default 50 MHz if the card supports the high-speed mode. The option has no effect if ``sd.low_speed`` was also applied.

### brcm-emmc.device

The same set of options as in case of ``brcm-sdhc.device``. Replace ``sd``  with ``emmc`` prefix in this case.

### 68040.library

* ``vbr_move`` 
  Moves VBR (base address for exception vector table) from CHIP to FAST ram. This setting improves the performance slightly, since the FAST ram on Emu68 is at least 500 times faster than CHIP, but reduces compatibility with old games and demos started from floppy drive drastically. However, system-friendly software as well as demos and games started through WHDLoad should not be affected.

### emu68-vc4.card

* ``vc4.mem=num`` 
  Sets size of VC4 memory reported to P96 subsystem to ``num``  MB. Default is 16 in case of PiStorm build and 0 in all other Emu68 variants. Please note this is not the same as ``gpu_mem`` setting in config.txt file. The latter is used to assign general purpose memory to the VPU.

### PiStorm32-lite only

* ``one_slot``
  Forces a one-slot pistorm32 protocol resulting in slower write accesses to chipset and to CHIP memory

### Debugging

* ``debug`` 
  Enables debugging of the JIT engine. Every portion of m68k code translated to AArch64 will be shown in form of short statistics and binary dump of ARM code. Statistics include number of m68k instructions translated, resulting number of ARM instructions, mean ARM instruction number per m68k opcode and CRC32 checksum of translated memory block.
* ``disassemble`` 
  Shows disassembled blocks in two columns. The left column contains m68k code disassembly whereas the right column is the AArch64 code. The two are aligned vertically so that the translated code can be assigned to every single m68k opcode properly.
* ``async_log`` 
  Use asynchronous log on separate ARM core with a 8 MB large ring buffer. Improves performance of m68k when debug is enabled.
* ``fast_serial`` 
  Use synchronous serial port running at a speed varying between 10 and 50 MBit instead of regular serial protocol at 921.6 kBit. Requires proper hardware such as e.g. FT232H.
* ``buptest=num``
  When Emu68 is starting it will perform a bus test of the PiStorm interface. A ``num`` kilobytes of CHIP memory will be written with random patterns and subsequently will be read in many different ways with varying read sizes and data alignment. In case of error, which indicates some issues with PiStorm interface or connection to the Amiga, the test will stop and Emu68 will not start.
* ``bupiter=num``
  Sets the number of iterations (of different randomised data patterns) of the bus test mentioned above.

### Memory

* ``limit_2g`` 
  Limit the mapped ARM memory to two gigabytes. Useful on machines which offer more RAM and, because of that, confuse e.g. AmigaOS.
* ``enable_c0_slow`` 
  Enables "slow" memory in ``0xc00000...0xc7ffff`` range.
* ``enable_c8_slow`` 
  Enables "slow" memory in ``0xc80000...0xcfffff`` range. Requires ``enable_c0_slow``.
* ``enable_d0_slow`` 
  Enables "slow" memory in ``0xd00000...0xd7ffff`` range. Requires ``enable_c0_slow`` and ``enable_c8_slow`` activated.
* ``move_slow_to_chip`` 
  Maps 512K memory expansion of A500 to the CHIP ram range.
* ``z2_ram_size=0 | 1 | 2 | 4 | 8`` 
  Set size of Zorro II RAM expansion to 0 to 8 MB. Default is 8, but eventually has to be lowered if other Zorro II devices are installed in the system.

### Miscellaneous 

* ``chip_slowdown``
  Every translated m68k opcode will have additional ARM instruction reading the opcode word from memory before executing it. Can improve compatibility of old software using busy loops for delay purposes.
* ``checksum_rom``
  Recalculates checksum of mapped rom. Might be useful in case of modded kickstart files with broken checksum.
* ``copy_rom=256 | 512 | 1024 | 2048``
  When Emu68 is starting the original Amiga ROM installed in your computer will be copied to fast ARM memory. The number determines size of the ROM image (in KB) which should be copied.
* ``enable_cache`` 
  Turns on JIT cache in ``CACR`` register on startup. Useful in case of bare metal software started instead of AROS or AmigaOS ROM.
* ``nofpu`` 
  Disables the FPU unit of Emu68. All LineF opcodes related to FPU will trigger the exception.
* ``swap_df0_with_df1`` 
  Swaps DF0 with DF1 floppy drive.
* ``swap_df0_with_df2`` 
  Swaps DF0 with DF2 floppy drive.
* ``swap_df0_with_df3`` 
  Swaps DF0 with DF3 floppy drive.

