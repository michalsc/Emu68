static struct OpcodeDef InsnTable[512] = {
  [0010 ... 0037] = { { EMIT_CPUSH_NOP }, NULL, SR_S, 0, 1, 0, 0},
  [0050 ... 0077] = { { EMIT_CPUSH_NOP }, NULL, SR_S, 0, 1, 0, 0},
  [0110 ... 0137] = { { EMIT_CINV }, NULL, SR_S, 0, 1, 0, 0},
  [0150 ... 0177] = { { EMIT_CPUSH }, NULL, SR_S, 0, 1, 0, 0},
  [0210 ... 0237] = { { EMIT_CINV }, NULL, SR_S, 0, 1, 0, 0},
  [0250 ... 0277] = { { EMIT_CPUSH }, NULL, SR_S, 0, 1, 0, 0},
  [0310 ... 0337] = { { EMIT_CINV }, NULL, SR_S, 0, 1, 0, 0},
  [0350 ... 0377] = { { EMIT_CPUSH }, NULL, SR_S, 0, 1, 0, 0},
  [0400 ... 0437] = { { EMIT_PFLUSH }, NULL, SR_S, 0, 1, 0, 0 },
  [0500 ... 0517] = { { EMIT_PTEST }, NULL, SR_S, 0, 1, 0, 0 },
  [0540 ... 0557] = { { EMIT_PTEST }, NULL, SR_S, 0, 1, 0, 0 },
};

uint32_t *EMIT_MMU(uint32_t *ptr, uint16_t **m68k_ptr, uint16_t *insn_consumed) {

  uint16_t opcode = BE16((*m68k_ptr)[0]);
    (*m68k_ptr)++;
    *insn_consumed = 1;

    if (InsnTable[opcode & 0777].od_Emit)
        ptr = InsnTable[opcode & 0777].od_Emit(ptr, opcode, m68k_ptr);

    else {
        ptr = EMIT_FlushPC(ptr);
        ptr = EMIT_InjectDebugString(ptr, "[JIT] opcode %04x at %08x not implemented\n", opcode, *m68k_ptr - 1);
        *ptr++ = svc(0x100);
        *ptr++ = svc(0x101);
        *ptr++ = svc(0x103);
        *ptr++ = (uint32_t)(uintptr_t)(*m68k_ptr - 8);
        *ptr++ = 48;
        ptr = EMIT_Exception(ptr, VECTOR_ILLEGAL_INSTRUCTION, 0);
        *ptr++ = INSN_TO_LE(0xffffffff);
    }

    return ptr;
}

uint32_t GetSR_LineF(uint16_t opcode)
{
    /* If instruction is in the table, return what flags it needs (shifted 16 bits left) and flags it sets */
    if (InsnTable[opcode & 0777].od_Emit) {
        return (InsnTable[opcode & 0777].od_SRNeeds << 16) | InsnTable[opcode & 0777].od_SRSets;
    }
    /* Instruction not found, i.e. it needs all flags and sets none (ILLEGAL INSTRUCTION exception) */
    else {
        kprintf("Undefined LineE\n");
        return SR_CCR << 16;
    }
}

int M68K_GetLineELength(uint16_t *insn_stream) {

    uint16_t opcode = BE16(*insn_stream);
    
    int length = 0;
    int need_ea = 0;
    int opsize = 0;

    if (InsnTable[opcode & 0777].od_Emit) {
        length = InsnTable[opcode & 0777].od_BaseLength;
        need_ea = InsnTable[opcode & 0777].od_HasEA;
        opsize = InsnTable[opcode & 0777].od_OpSize;
    }

    if (need_ea) {
        length += SR_GetEALength(&insn_stream[length], opcode & 077, opsize);
    }

    return length;
}
  
  
  
  
  
  
}