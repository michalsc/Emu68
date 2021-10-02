# Emu68
M68K emulation for ARM



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
