export M68K_CC     := m68k-amigaos-gcc
export M68K_CXX    := m68k-amigaos-g++
export M68K_CFLAGS := -m68020 -m68881 -Os -pedantic -fomit-frame-pointer -pedantic-errors -fno-exceptions
export M68K_CXXFLAGS:= $(M68K_CFLAGS) -fno-threadsafe-statics -fno-rtti -fno-exceptions
export M68K_LDFLAGS:= -nostdlib -nostartfiles
SUBDIRS := SmallPT

all: $(SUBDIRS)

.PHONY: all $(SUBDIRS)

$(SUBDIRS):
	@make -C $@ all
