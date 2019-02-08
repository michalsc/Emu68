#ifndef _M68K_H
#define _M68K_H

#include <stdint.h>

#include "nodes.h"

struct M68KTranslationUnit {
    struct Node     mt_HashNode;
    struct Node     mt_LRUNode;
    uint16_t *      mt_M68kAddress;
    uint32_t        mt_M68kInsnCnt;
    uint32_t        mt_ARMInsnCnt;
    void *          mt_ARMEntryPoint;
    uint32_t        mt_ARMCode[] __attribute__((aligned(32)));
};

struct M68KState
{
    union {
        uint8_t u8[4];
        int8_t s8[4];
        uint16_t u16[2];
        int16_t s16[2];
        uint32_t u32;
        int32_t s32;
    } D[8];
    union {
        uint16_t u16[2];
        int16_t s16[2];
        uint32_t u32;
        int32_t s32;
        void *p32;
    } A[8];
    union {
        uint16_t u16[2];
        int16_t s16[2];
        uint32_t u32;
        int32_t s32;
        void *p32;
    } USP, MSP, ISP;

    uint16_t *PC;
    uint16_t SR;
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

#define M_CC_T  0x00
#define M_CC_F  0x01
#define M_CC_HI 0x02
#define M_CC_LS 0x03
#define M_CC_CC 0x04
#define M_CC_CS 0x05
#define M_CC_NE 0x06
#define M_CC_EQ 0x07
#define M_CC_VC 0x08
#define M_CC_VS 0x09
#define M_CC_PL 0x0a
#define M_CC_MI 0x0b
#define M_CC_GE 0x0c
#define M_CC_LT 0x0d
#define M_CC_GT 0x0e
#define M_CC_LE 0x0f


uint32_t *EMIT_GetOffsetPC(uint32_t *ptr, int8_t *offset);
uint32_t *EMIT_AdvancePC(uint32_t *ptr, uint8_t offset);
uint32_t *EMIT_FlushPC(uint32_t *ptr);
uint32_t *EMIT_LoadFromEffectiveAddress(uint32_t *ptr, uint8_t size, uint8_t *arm_reg, uint8_t ea, uint16_t *m68k_ptr, uint8_t *ext_words);
uint32_t *EMIT_StoreToEffectiveAddress(uint32_t *ptr, uint8_t size, uint8_t *arm_reg, uint8_t ea, uint16_t *m68k_ptr, uint8_t *ext_words);

uint32_t *EMIT_line0(uint32_t *ptr, uint16_t **m68k_ptr);
uint32_t *EMIT_line4(uint32_t *ptr, uint16_t **m68k_ptr);
uint32_t *EMIT_line5(uint32_t *ptr, uint16_t **m68k_ptr);
uint32_t *EMIT_line6(uint32_t *ptr, uint16_t **m68k_ptr);
uint32_t *EMIT_moveq(uint32_t *ptr, uint16_t **m68k_ptr);
uint32_t *EMIT_line8(uint32_t *ptr, uint16_t **m68k_ptr);
uint32_t *EMIT_line9(uint32_t *ptr, uint16_t **m68k_ptr);
uint32_t *EMIT_lineD(uint32_t *ptr, uint16_t **m68k_ptr);
uint32_t *EMIT_move(uint32_t *ptr, uint16_t **m68k_ptr);

uint8_t M68K_GetSRMask(uint16_t opcode);
void M68K_InitializeCache();
struct M68KTranslationUnit *M68K_GetTranslationUnit(uint16_t *ptr);

#endif /* _M68K_H */
