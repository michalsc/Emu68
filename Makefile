include version.mk

CC := arm-linux-gnueabihf-gcc
CXX := arm-linux-gnueabihf-g++
CC64 := aarch64-linux-gcc
CXX64:= aarch64-linux-g++
OBJCOPY := arm-linux-gnueabihf-objcopy
OBJCOPY64 := aarch64-linux-objcopy
CFLAGS64:= $(EXTRA_FLAGS) -mbig-endian -std=gnu11 -O3 -pedantic -pedantic-errors -ffixed-x19 -ffixed-x20 -ffixed-x21 -ffixed-x22 -ffixed-x23 -ffixed-x24 -ffixed-x25 -ffixed-x26 -ffixed-x27 -ffixed-x28 -ffixed-x29 -ffixed-x13 -ffixed-x14 -ffixed-x15 -ffixed-x16 -ffixed-x17 -ffixed-x18 -fomit-frame-pointer -Wall -Wextra -Werror -DVERSION_STRING_DATE='$(VERSION_STRING_DATE)'
CFLAGS  := $(EXTRA_FLAGS) -mbig-endian -mcpu=cortex-a7 -mfpu=neon-vfpv4 -std=gnu11 -O3 -pedantic -pedantic-errors -ffixed-r11 -fomit-frame-pointer -Wall -Wextra -Werror -DVERSION_STRING_DATE='$(VERSION_STRING_DATE)'
CXXFLAGS:= $(EXTRA_FLAGS) -mbig-endian -mcpu=cortex-a7 -mfpu=neon-vfpv4 -std=c++11 -O3 -pedantic -pedantic-errors -ffixed-r11 -fomit-frame-pointer -Wall -Wextra -Werror -DVERSION_STRING_DATE='$(VERSION_STRING_DATE)'
LDFLAGS := -static -lrt -s

HOST_CXX := g++
HOST_CXXFLAGS := -O2 -std=c++11
HOST_LDFLAGS :=

RPI_OBJS := start_rpi.o support_rpi.o devicetree.o tlsf.o HunkLoader.o M68k_Translator.o RegisterAllocator.o M68k_EA.o M68k_SR.o \
        M68k_MOVE.o M68k_LINE0.o M68k_LINE4.o M68k_LINE5.o M68k_LINE6.o M68k_LINE8.o M68k_LINEF.o \
        M68k_LINE9.o M68k_LINEB.o M68k_LINEC.o M68k_LINED.o M68k_LINEE.o M68k_MULDIV.o EmuLogo.o support.o

RPI64_OBJS := start_rpi64.o support_rpi.o support.o tlsf.o devicetree.o EmuLogo.o HunkLoader.o \
        RegisterAllocator64.o

#devicetree.o tlsf.o HunkLoader.o support.o support_rpi.o

OBJDIR := Build
OBJDIR64 := Build64

TESTOBJS :=

TESTOBJDIR := BuildTest

raspi: pre-build raspi-build raspi64-build

examples:
	@echo "Building Examples"
	@docker run --rm -v $(shell pwd):/work -i amigadev/crosstools:m68k-amigaos make -C Examples all

pre-build:
	@mkdir -p $(OBJDIR) >/dev/null
	@mkdir -p $(OBJDIR64) >/dev/null

raspi-build:
	@touch start_rpi.c
	@make --no-print-directory EXTRA_FLAGS="-ffreestanding -DRASPI" $(OBJDIR)/kernel.img

raspi64-build:
	@touch start_rpi64.c
	@make --no-print-directory EXTRA_FLAGS="-ffreestanding -DRASPI" $(OBJDIR)/kernel8.img

$(OBJDIR)/kernel.img: $(addprefix $(OBJDIR)/, $(RPI_OBJS))
	@echo "Building target: $@"
	@$(CC) $(foreach f,$(RPI_OBJS),$(OBJDIR)/$(f)) -Wl,--Map=$@.map -Wl,--build-id -Wl,--be8 -Wl,--format=elf32-bigarm -nostdlib -nostartfiles -static -T ldscript-be.lds -o $@.elf
	@$(OBJCOPY) -O binary $@.elf $@
	@echo "Build completed"

$(OBJDIR)/kernel8.img: ldscript-be64.lds $(addprefix $(OBJDIR64)/, $(RPI64_OBJS))
	@echo "Building target: $@"
	@$(CC64) $(foreach f,$(RPI64_OBJS),$(OBJDIR64)/$(f)) -Wl,--Map=$@.map -Wl,--build-id -Wl,-EB -Wl,--format=elf64-bigaarch64 -nostdlib -nostartfiles -static -T ldscript-be64.lds -o $@.elf
	@$(OBJCOPY64) -O binary $@.elf $@
	@echo "Build completed"

.PHONY: all clean examples
.SECONDARY: main-build pre-build test

$(OBJDIR)/%.o: %.cpp
	@mkdir -p $(@D)
	@echo "Compiling: $*.cpp"
	@$(CXX) -c $(CXXFLAGS) $< -o $@

$(OBJDIR)/%.d: %.cpp
	@mkdir -p $(@D)
	@set -e; rm -f $@; \
         $(CXX) -MM -MT $(basename $@).o $(CXXFLAGS) $< > $@.$$$$; \
         sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' < $@.$$$$ > $@; \
         rm -f $@.$$$$

$(TESTOBJDIR)/%.o: %.cpp
	@mkdir -p $(@D)
	@echo "Compiling: $*.cpp"
	@$(HOST_CXX) -c $(HOST_CXXFLAGS) $< -o $@

$(TESTOBJDIR)/%.d: %.cpp
	@mkdir -p $(@D)
	@set -e; rm -f $@; \
         $(HOST_CXX) -MM -MT $(basename $@).o $(HOST_CXXFLAGS) $< > $@.$$$$; \
         sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' < $@.$$$$ > $@; \
         rm -f $@.$$$$

$(OBJDIR)/%.o: %.c
	@mkdir -p $(@D)
	@echo "Compiling: $*.c"
	@$(CC) -c $(CFLAGS) $< -o $@

$(OBJDIR)/%.d: %.c
	@mkdir -p $(@D)
	@set -e; rm -f $@; \
         $(CC) -MM -MT $(basename $@).o $(CFLAGS) $< > $@.$$$$; \
         sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' < $@.$$$$ > $@; \
         rm -f $@.$$$$

$(OBJDIR64)/%.o: %.c
	@mkdir -p $(@D)
	@echo "Compiling: $*.c"
	@$(CC64) -c $(CFLAGS64) $< -o $@

$(OBJDIR64)/%.d: %.c
	@mkdir -p $(@D)
	@set -e; rm -f $@; \
         $(CC64) -MM -MT $(basename $@).o $(CFLAGS64) $< > $@.$$$$; \
         sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' < $@.$$$$ > $@; \
         rm -f $@.$$$$

$(TESTOBJDIR)/%.o: %.c
	@mkdir -p $(@D)
	@echo "Compiling: $*.c"
	@$(HOST_CXX) -c $(HOST_CXXFLAGS) $< -o $@

$(TESTOBJDIR)/%.d: %.cpp
	@mkdir -p $(@D)
	@set -e; rm -f $@; \
         $(HOST_CXX) -MM -MT $(basename $@).o $(HOST_CXXFLAGS) $< > $@.$$$$; \
         sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' < $@.$$$$ > $@; \
         rm -f $@.$$$$

clean:
	rm -rf *.o *.d $(OBJDIR) $(OBJDIR64) $(TESTOBJDIR)

-include $(foreach f,$(OBJS:.o=.d),$(OBJDIR)/$(f))
-include $(foreach f,$(RPI_OBJS:.o=.d),$(OBJDIR)/$(f))
-include $(foreach f,$(RPI64_OBJS:.o=.d),$(OBJDIR64)/$(f))
-include $(foreach f,$(TESTOBJS:.o=.d),$(TESTOBJDIR)/$(f))
