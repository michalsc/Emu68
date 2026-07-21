#ifndef _EMU68_M68K_INTERPRETER_HPP_
#define _EMU68_M68K_INTERPRETER_HPP_

#ifdef __cplusplus
#include <cstdint>
#else
#include <stdint.h>
#endif

#ifdef __cplusplus

void INTERPRET_Exception_F0(uint32_t exception);

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
