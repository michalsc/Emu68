#ifndef _DISASM_H
#define _DISASM_H

void disasm_init();
void disasm_open();
void disasm_close();
void disasm_print(uint16_t *m68k_addr, uint16_t m68k_count, uint32_t *arm_addr, size_t arm_size, uint32_t *arm_start);

#endif /* _DISASM_H */
