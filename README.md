# Emu68
M68K emulation for ARM

## PiStorm32 multi-Kickstart selector

The `pistorm32-multirom` branch adds a keyboard-controlled Kickstart ROM
selector for PiStorm and PiStorm32/32-Lite systems. It keeps several ROM images
in one pack and changes ROM without editing the SD card between boots.

No Kickstart ROM images are included or downloaded. You must provide ROM files
that you are licensed to use.

### Create a ROM pack

Clone this branch and create a pack from two or more ROM images:

```bash
git clone --branch pistorm32-multirom https://github.com/Matrixite/Emu68.git
cd Emu68
python3 tools/make_rom_pack.py kickstarts.pack kick13.rom kick31.rom
```

Packs can contain up to 16 ROMs. Supported sizes are 256 KiB, 512 KiB, 1 MiB
and 2 MiB. All ROMs in a keyboard-switchable pack must be the same size. Slot
numbers follow the order of the filenames in the command, starting at slot 0.

### Configure the SD card

Copy `kickstarts.pack` to the FAT32 boot partition and use it as the initramfs
in `config.txt`:

```ini
initramfs kickstarts.pack
```

The optional `rom_slot=N` setting in the single-line `cmdline.txt` chooses the
initial zero-based slot. Slot 0 is used when the option is omitted or invalid:

```text
rom_slot=0
```

Other Emu68 command-line options can remain on the same line.

### Switch ROM with the keyboard

- Press Ctrl-Amiga-Amiga briefly for a normal reset using the current ROM.
- Hold Ctrl-Amiga-Amiga for approximately three seconds to load the next ROM.
- Holding it on the final slot wraps around to slot 0.

The ROM pack remains in memory while Emu68 is running. A complete power cycle
starts again from the slot selected by `rom_slot=N`. A traditional single ROM,
such as `initramfs kick.rom`, remains compatible and keeps the original reset
behaviour.

More details are available in [the multi-ROM guide](docs/MultiROM.md).



# Building instructions

In order to build Emu68 several tools are necessary. The first one is of course git, which will be used to clone the repository. Further, cmake will be used to configure the build. Firmware, if required, will be downloaded by cmake during project configure phase, here, either curl or wget will be used. Finally, aarch64 or aarch32 toolchain will be necessary.

## Building on Ubuntu

Make sure your package repository is up to date

```bash
sudo apt-get update
```

Subsequently install the ``build-essential`` package as well as cross-compiler

```bash
sudo apt-get install build-essential gcc-aarch64-linux-gnu g++-aarch64-linux-gnu
```

After the compiler is fetched we are almost done. Almost. The AArch64 architecture allows bi-endian operation, but the majority of the world is focusing on little-endian format as the native one. The cross-compiler provided by Ubuntu is not an exception and it defaults to little-endian systems. Furthermore, it lacks one big-endian relevant header which will need to be fixed now

```bash
sudo cp /usr/aarch64-linux-gnu/include/gnu/stubs-lp64.h /usr/aarch64-linux-gnu/include/gnu/stubs-lp64_be.h
```

One might wonder how it could work if a little-endian header is taken for the big-endian target. Well, in this case it is fully possible - this file is almost empty. At this point everything is configured properly and building of Emu68 can start. First, clone the repository

```bash
git clone https://github.com/michalsc/Emu68.git
```

After entering the ``Emu68`` directory created by git, one need to pull the submodules

```bash
git submodule update --init --recursive
```

Now, create build directory and install directory, enter the build directory

```bash
mkdir build install
cd build
```

Finally, configure the cmake project

```bash
cmake .. -DCMAKE_INSTALL_PREFIX=../install -DTARGET=raspi64 -DVARIANT=pistorm \
				 -DCMAKE_TOOLCHAIN_FILE=../toolchains/aarch64-linux-gnu.cmake
```

In this example a 64-bit RaspberryPi build is selected with a PiStorm variant. Should you prefer a version of Emu68 which works in bare metal on Raspberry, but does not require PiStorm board, remove the ``-DVARIANT=pistorm`` from this line. If you prefer to not use the toolchain file, you need to specify your preferred compiler by yourself, e.g.

```bash
CC=aarch64-linux-gcc-10 CXX=aarch64-linux-g++-10 cmake .. -DCMAKE_INSTALL_PREFIX=../install \
         -DTARGET=raspi64 -DVARIANT=pistorm
```

During configuration process cmake will fetch the most recent RaspberryPi firmware by itself. After the configuration is completed, start building process and finally install the compiled files to previously created ``install`` directory

```bash
make && make install
```

Now, build process is completed. Copy the contents of the install directory onto FAT32 or FAT16 formatted SD card. Your Emu68 build is completed.
