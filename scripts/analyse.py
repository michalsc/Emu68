from unicorn import *
from unicorn.ppc_const import *
from capstone import *
from elftools.elf.elffile import ELFFile
from sys import argv

# --- configuration ---
ELF_PATH = argv[1]
MEM_BASE = 0x00000000
MEM_SIZE = 2 * 1024 * 1024  # 2MB

# --- load ELF ---
with open(ELF_PATH, "rb") as f:
    elf = ELFFile(f)
    entry = elf.header["e_entry"]

    segments = []
    for seg in elf.iter_segments():
        if seg["p_type"] == "PT_LOAD":
            data = seg.data()
            addr = seg["p_vaddr"]
            if seg["p_memsz"] > len(data):
                # print(f"Resizing segment from {len(data)} to {seg["p_memsz"]}")
                data = data + bytes(seg["p_memsz"] - len(data))
            segments.append((addr, data))
            print(f"Loaded segment at 0x{addr:08x}, size={len(data):x}")

# --- initialize Unicorn ---
mu = Uc(UC_ARCH_PPC, UC_MODE_PPC32 | UC_MODE_BIG_ENDIAN)
mu.mem_map(0x80000000, 0x10000)
mu.mem_map(0, 0x20000000)

# load segments into Unicorn memory
for addr, data in segments:
    base_addr = addr & 0xfffff000
    block_end = (addr + len(data) + 4095) & 0xfffff000
    #mu.mem_map(base_addr, block_end - base_addr)
    mu.mem_write(addr, data)

# --- initialize disassembler ---
md = Cs(CS_ARCH_PPC, CS_MODE_32 | CS_MODE_BIG_ENDIAN)

# --- set PC to entry point ---
mu.reg_write(UC_PPC_REG_PC, entry)

# Set stack
mu.reg_write(UC_PPC_REG_1, 0x8000ff00)

# optional initial values
mu.reg_write(UC_PPC_REG_CTR, 0)
mu.reg_write(UC_PPC_REG_XER, 0)
mu.reg_write(UC_PPC_REG_CR, 0)

# --- helper: dump registers ---


def dump_regs(mu : Uc):
    reg = mu.reg_read_batch((UC_PPC_REG_0, UC_PPC_REG_1, UC_PPC_REG_2, UC_PPC_REG_3, \
                             UC_PPC_REG_4, UC_PPC_REG_5, UC_PPC_REG_6, UC_PPC_REG_7))
    for i in range(0, len(reg)):
        if reg[i] != dump_regs.old_r0[i]:
            print(f"  r{i:02}: \x1b[7m{reg[i]:08X}\x1b[27m", end="")
        else:
            print(f"  r{i:02}: {reg[i]:08X}", end="")
    dump_regs.old_r0 = reg
    print("")
    reg = mu.reg_read_batch((UC_PPC_REG_8, UC_PPC_REG_9, UC_PPC_REG_10, UC_PPC_REG_11, \
                             UC_PPC_REG_12, UC_PPC_REG_13, UC_PPC_REG_14, UC_PPC_REG_15))
    for i in range(0, len(reg)):
        if reg[i] != dump_regs.old_r8[i]:
            print(f"  r{i+8:02}: \x1b[7m{reg[i]:08X}\x1b[27m", end="")
        else:
            print(f"  r{i+8:02}: {reg[i]:08X}", end="")
    dump_regs.old_r8 = reg
    print("")
    reg = mu.reg_read_batch((UC_PPC_REG_16, UC_PPC_REG_17, UC_PPC_REG_18, UC_PPC_REG_19, \
                             UC_PPC_REG_20, UC_PPC_REG_21, UC_PPC_REG_22, UC_PPC_REG_23))
    for i in range(0, len(reg)):
        if reg[i] != dump_regs.old_r16[i]:
            print(f"  r{i+16:02}: \x1b[7m{reg[i]:08X}\x1b[27m", end="")
        else:
            print(f"  r{i+16:02}: {reg[i]:08X}", end="")
    dump_regs.old_r16 = reg
    print("")
    reg = mu.reg_read_batch((UC_PPC_REG_24, UC_PPC_REG_25, UC_PPC_REG_26, UC_PPC_REG_27, \
                             UC_PPC_REG_28, UC_PPC_REG_29, UC_PPC_REG_30, UC_PPC_REG_31))
    for i in range(0, len(reg)):
        if reg[i] != dump_regs.old_r24[i]:
            print(f"  r{i+24:02}: \x1b[7m{reg[i]:08X}\x1b[27m", end="")
        else:
            print(f"  r{i+24:02}: {reg[i]:08X}", end="")
    dump_regs.old_r24 = reg
    print("")
    
    reg = mu.reg_read(UC_PPC_REG_CTR) #, UC_PPC_REG_XER, UC_PPC_REG_CR, UC_PPC_REG_LR))
    if reg != dump_regs.old_ctr:
        print(f"  ctr: \x1b[7m{reg:08x}\x1b[27m", end="")
    else:
        print(f"  ctr: {reg:08x}", end="")
    dump_regs.old_ctr = reg

    reg = mu.reg_read(UC_PPC_REG_CR) #, UC_PPC_REG_XER, UC_PPC_REG_CR, UC_PPC_REG_LR))
    if reg != dump_regs.old_cr:
        print(f"   cr: \x1b[7m{reg:08x}\x1b[27m", end="")
    else:
        print(f"   cr: {reg:08x}", end="")
    dump_regs.old_cr = reg

    reg = mu.reg_read(UC_PPC_REG_XER) #, UC_PPC_REG_XER, UC_PPC_REG_CR, UC_PPC_REG_LR))
    if reg != dump_regs.old_xer:
        print(f"  xer: \x1b[7m{reg:08x}\x1b[27m", end="")
    else:
        print(f"  xer: {reg:08x}", end="")
    dump_regs.old_xer = reg

    reg = mu.reg_read(UC_PPC_REG_LR) #, UC_PPC_REG_XER, UC_PPC_REG_CR, UC_PPC_REG_LR))
    if reg != dump_regs.old_lr:
        print(f"   lr: \x1b[7m{reg:08x}\x1b[27m")
    else:
        print(f"   lr: {reg:08x}")
    dump_regs.old_lr = reg

dump_regs.old_r0 = (0,0,0,0,0,0,0,0)
dump_regs.old_r8 = (0,0,0,0,0,0,0,0)
dump_regs.old_r16 = (0,0,0,0,0,0,0,0)
dump_regs.old_r24 = (0,0,0,0,0,0,0,0)
dump_regs.old_ctr = 0
dump_regs.old_xer = 0
dump_regs.old_cr = 0
dump_regs.old_lr = 0

insn_count = 0

def hook_code(uc, address, size, user_data):
    global insn_count
    pc = mu.reg_read(UC_PPC_REG_PC)
    code = mu.mem_read(pc, 4)
    insn = next(md.disasm(code, pc, count=1), None)

    dump_regs(mu)

    if insn:
        print(f"\n[{insn_count:05}] 0x{pc:08x}: {insn.mnemonic} {insn.op_str}")
    else:
        print(f"\n[{insn_count:05}] 0x{pc:08x}: (unknown)")
    
    insn_count = insn_count + 1

mu.hook_add(UC_HOOK_CODE, hook_code)
mu.emu_start(entry, 0)

# --- single-step loop ---
#for i in range(100):  # step 10 instructions max
#    pc = mu.reg_read(UC_PPC_REG_PC)
#    code = mu.mem_read(pc, 4)
#    insn = next(md.disasm(code, pc, count=1), None)
#
#    if insn:
#        print(f"\n[{i}] 0x{pc:08x}: {insn.mnemonic} {insn.op_str}")
#    else:
#        print(f"\n[{i}] 0x{pc:08x}: (unknown)")
#        break
#
#    
#    try:
#        mu.emu_start(pc, pc + 4)
#    except UcError as e:
#        print(f"Stopped: {e}")
#        break

#    dump_regs(mu)
