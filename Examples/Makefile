export M68K_CC     := /opt/amiga/bin/m68k-amigaos-gcc
export M68K_CXX    := /opt/amiga/bin/m68k-amigaos-g++
export M68K_CFLAGS := -m68020 -m68881 -fomit-frame-pointer -Os -mhard-float -mregparm=4 -pedantic -pedantic-errors -fno-exceptions
export M68K_CXXFLAGS:= -m68020 -m68881 -fomit-frame-pointer -Os -mhard-float -mregparm=4 -pedantic -pedantic-errors -fno-threadsafe-statics -fno-rtti -fno-exceptions
export M68K_LDFLAGS:= -nostdlib -nostartfiles
SUBDIRS := SmallPT

all: $(SUBDIRS)

.PHONY: all $(SUBDIRS)

$(SUBDIRS):
	@make -C $@ all
