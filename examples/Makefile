export M68K_CC     := m68k-amigaos-gcc
export M68K_CXX    := m68k-amigaos-g++
export M68K_CFLAGS := -m68040 -m68881 -ffast-math -Os -fomit-frame-pointer -fno-exceptions
export M68K_CXXFLAGS:= $(M68K_CFLAGS) -fno-threadsafe-statics -fno-rtti -fno-exceptions
export M68K_LDFLAGS:= -nostdlib -nostartfiles
SUBDIRS := Buddha SysInfo Dhrystone2.1 SmallPT Linpack

all: $(SUBDIRS)

.PHONY: all $(SUBDIRS)

$(SUBDIRS):
	@make -C $@ all
