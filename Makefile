CC := gcc
CXX := g++
CFLAGS  := -O2 -pedantic -pedantic-errors -Wall -Wextra -Werror -c -Wno-extended-offsetof
CXXFLAGS:= -std=c++0x -O2 -pedantic -pedantic-errors -Wall -Wextra -Werror -c
LDFLAGS :=

HOST_CXX := g++
HOST_CXXFLAGS := -O2 -std=c++0x
HOST_LDFLAGS :=

OBJS := start_emu.o RegisterAllocator.o M68k_MOVE.o M68k_EA.o
OBJDIR := Build

TESTOBJS := 

TESTOBJDIR := BuildTest

all: pre-build main-build

pre-build:
	@mkdir -p $(OBJDIR) >/dev/null

main-build: pre-build
	@make --no-print-directory $(OBJDIR)/Emu68

test:
	@make --no-print-directory $(TESTOBJDIR)/Emu68Test
	@$(TESTOBJDIR)/Emu68Test

$(TESTOBJDIR)/Emu68Test: $(addprefix $(TESTOBJDIR)/, $(TESTOBJS))
	@echo "Building test: $@"
	@$(HOST_CXX) $(foreach f,$(TESTOBJS),$(TESTOBJDIR)/$(f)) $(HOST_LDFLAGS) -o $@

$(OBJDIR)/Emu68: $(addprefix $(OBJDIR)/, $(OBJS))
	@echo "Building target: $@"
	@$(CC) $(foreach f,$(OBJS),$(OBJDIR)/$(f)) $(LDFLAGS) -o $@
	@echo "Build completed"

.PHONY: all clean
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
	rm -rf *.o *.d $(OBJDIR) $(TESTOBJDIR)

-include $(foreach f,$(OBJS:.o=.d),$(OBJDIR)/$(f))
-include $(foreach f,$(TESTOBJS:.o=.d),$(TESTOBJDIR)/$(f))
