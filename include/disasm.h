#ifndef _DISASM_H
#define _DISASM_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

void disasm_init();
void disasm_open();
void disasm_close();
void disasm_print(uint16_t *m68k_addr, uint16_t m68k_count, uint32_t *arm_addr, size_t arm_size, uint32_t *arm_start);
void disasm_print_ppc(uint32_t *ppc_addr, uint32_t ppc_count, uint32_t *arm_addr, size_t arm_size, uint32_t *arm_start);
void disasm_print_ppc_only(uint32_t *ppc_addr);
void disasm_print_arm_only(uint32_t *arm_addr);

#ifdef __cplusplus
}
#endif

#endif /* _DISASM_H */
