# Building with docker made easy

## Parameters
```bash
$ ./build-with-docker -h
Helper script to build Emu68 with docker

Syntax: build-with-docker [-t|v|c|b|h]
Options:
-t		Sets which Emu68 target to build (raspi|raspi64|pbpro|rockpro64|virt).
-v		Sets which Emu68 variant to build (none|pistorm).
-c		Cleans build directory before building.
-b		Builds example programs to run in Emu68.
-h		Print this Help.
```

## Build
```bash
$ mkdir <PATH-TO-EMU68-DIR>/build
$ cd <PATH-TO-EMU68-DIR>/build
$ ../build-scripts/build-with-docker -t raspi64 -v pistorm -c
```

## Run in QEMU
```bash
qemu-system-aarch64 -M raspi3 -kernel ./Emu68.img -dtb ./firmware/bcm2710-rpi-3-b.dtb -serial stdio -initrd ./SysInfo -append "enable_cache" -accel tcg,tb-size=64
```