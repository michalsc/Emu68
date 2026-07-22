#ifndef _EMU68_M68K_INTERPRETER_HPP_
#define _EMU68_M68K_INTERPRETER_HPP_

#ifdef __cplusplus
#include <cstdint>
#else
#include <stdint.h>
#endif

#ifdef __cplusplus

#define I_SIZE_BYTE   1
#define I_SIZE_WORD   2
#define I_SIZE_LONG   4

void INTERPRET_Exception_F0(uint32_t exception);
void INTERPRET_LoadFromEffectiveAddress(uint8_t src_reg, uint8_t size, void *out, uint8_t mode);
void INTERPRET_StoreToEffectiveAddress(uint8_t dst_reg, uint32_t value, uint8_t size, uint8_t mode);
void INTERPRET_GetEffectiveAddress(uint8_t src_reg, uint8_t size, uint32_t *out, uint8_t mode);

extern "C" {
#endif

void INTERPRET_UNIMPLEMENTED(uint32_t opcode);
void INTERPRET_line0(uint32_t opcode);
void INTERPRET_line1(uint32_t opcode);
void INTERPRET_line2(uint32_t opcode);
void INTERPRET_line3(uint32_t opcode);
void INTERPRET_line4(uint32_t opcode);
void INTERPRET_line6(uint32_t opcode);
void INTERPRET_line7(uint32_t opcode);

#ifdef __cplusplus
}
#endif

#endif /* _EMU68_M68K_INTERPRETER_HPP_ */
