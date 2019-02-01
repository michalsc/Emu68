#ifndef _M68K_H
#define _M68K_H

#include <stdint.h>

uint32_t *EMIT_LoadFromEffectiveAddress(uint32_t *ptr, uint8_t size, uint8_t *arm_reg, uint8_t ea, uint16_t *m68k_ptr, uint8_t *ext_words);
uint32_t *EMIT_StoreToEffectiveAddress(uint32_t *ptr, uint8_t size, uint8_t *arm_reg, uint8_t ea, uint16_t *m68k_ptr, uint8_t *ext_words);

uint32_t *EMIT_line0(uint32_t *ptr, uint16_t **m68k_ptr);
uint32_t *EMIT_moveq(uint32_t *ptr, uint16_t **m68k_ptr);
uint32_t *EMIT_move(uint32_t *ptr, uint16_t **m68k_ptr);

uint8_t M68K_GetSRMask(uint16_t opcode);

struct M68KState
{
    union {
        unsigned char u8[4];
        signed char s8[4];
        unsigned short u16[2];
        signed short s16[2];
        unsigned int u32;
        signed int s32;
    } D[8];
    union {
        unsigned short u16[2];
        signed short s16[2];
        unsigned int u32;
        signed int s32;
        void *p32;
    } A[8];
    union {
        unsigned short u16[2];
        signed short s16[2];
        unsigned int u32;
        signed int s32;
        void *p32;
    } USP, MSP, ISP;
};

#define SR_C    0x0001
#define SR_V    0x0002
#define SR_Z    0x0004
#define SR_N    0x0008
#define SR_X    0x0010
#define SR_IPL  0x0700
#define SR_M    0x1000
#define SR_S    0x2000
#define SR_T0   0x4000
#define SR_T1   0x8000

#endif /* _M68K_H */
