#ifndef _EMU68_INTERPRETER_LINE4_HPP_
#define _EMU68_INTERPRETER_LINE4_HPP_

#ifdef __cplusplus
#include <cstdint>
extern "C" {
#else
#include <stdint.h>
#endif

void INTERPRET_NOP(uint16_t);
void INTERPRET_RTS(uint16_t);

void INTERPRET_SWAP_D0(uint16_t);
void INTERPRET_SWAP_D1(uint16_t);
void INTERPRET_SWAP_D2(uint16_t);
void INTERPRET_SWAP_D3(uint16_t);
void INTERPRET_SWAP_D4(uint16_t);
void INTERPRET_SWAP_D5(uint16_t);
void INTERPRET_SWAP_D6(uint16_t);
void INTERPRET_SWAP_D7(uint16_t);


#ifdef __cplusplus
}
#endif

#endif /* _EMU68_INTERPRETER_LINE4_HPP_ */
