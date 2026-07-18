#ifndef _REGISTERMAPPING_H
#define _REGISTERMAPPING_H

#include <stdint.h>

register uint32_t PC asm("w18");
register uint32_t D0 asm("w19");
register uint32_t D1 asm("w20");
register uint32_t D2 asm("w21");
register uint32_t D3 asm("w22");
register uint32_t D4 asm("w23");
register uint32_t D5 asm("w24");
register uint32_t D6 asm("w25");
register uint32_t D7 asm("w26");

register uint32_t A0 asm("w13");
register uint32_t A1 asm("w14");
register uint32_t A2 asm("w15");
register uint32_t A3 asm("w16");
register uint32_t A4 asm("w17");
register uint32_t A5 asm("w27");
register uint32_t A6 asm("w28");
register uint32_t A7 asm("w29");

#endif /* _REGISTERMAPPING_H */
