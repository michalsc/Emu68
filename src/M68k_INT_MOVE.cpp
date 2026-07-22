#include <cstdint>
#include <arm_neon.h>
#include <array>

#include <emu68/m68k/interpreter.hpp>

#include "RegisterMapping.h"


#define _REGLOCK_H
extern "C" {
    #include "M68k.h"
    #include "support.h"
}

/*
    First cover all combinations of B/W/L Dn->Dn transfers
*/
#define INTERPRET_MOVE_L_Dn_to_Dn(src, dest) \
void INTERPRET_MOVE_L_D##src##_to_D##dest(uint32_t) \
{ \
    uint32_t sr = SR & 0xfff0; \
    D##dest = D##src; \
    if (unlikely(D##dest == 0)) { \
        sr |= SR_Z; \
    } else { \
        if ((int32_t)D##dest < 0) { \
            sr |= SR_N; \
        } \
    } \
    SR = sr; \
    PC += 2; \
}

#define INTERPRET_MOVE_L_Dn_to_D(src) \
    INTERPRET_MOVE_L_Dn_to_Dn(src, 0); \
    INTERPRET_MOVE_L_Dn_to_Dn(src, 1); \
    INTERPRET_MOVE_L_Dn_to_Dn(src, 2); \
    INTERPRET_MOVE_L_Dn_to_Dn(src, 3); \
    INTERPRET_MOVE_L_Dn_to_Dn(src, 4); \
    INTERPRET_MOVE_L_Dn_to_Dn(src, 5); \
    INTERPRET_MOVE_L_Dn_to_Dn(src, 6); \
    INTERPRET_MOVE_L_Dn_to_Dn(src, 7);

INTERPRET_MOVE_L_Dn_to_D(0);
INTERPRET_MOVE_L_Dn_to_D(1);
INTERPRET_MOVE_L_Dn_to_D(2);
INTERPRET_MOVE_L_Dn_to_D(3);
INTERPRET_MOVE_L_Dn_to_D(4);
INTERPRET_MOVE_L_Dn_to_D(5);
INTERPRET_MOVE_L_Dn_to_D(6);
INTERPRET_MOVE_L_Dn_to_D(7);


#define INTERPRET_MOVE_W_Dn_to_Dn(src, dest) \
void INTERPRET_MOVE_W_D##src##_to_D##dest(uint32_t) \
{ \
    uint32_t sr = SR & 0xfff0; \
    int16_t tmp = D##src; \
    D##dest = (D##dest & 0xffff0000) | (D##src & 0x0000ffff); \
    if (unlikely(tmp == 0)) { \
        sr |= SR_Z; \
    } else { \
        if (tmp < 0) { \
            sr |= SR_N; \
        } \
    } \
    SR = sr; \
    PC += 2; \
}

#define INTERPRET_MOVE_W_Dn_to_D(src) \
    INTERPRET_MOVE_W_Dn_to_Dn(src, 0); \
    INTERPRET_MOVE_W_Dn_to_Dn(src, 1); \
    INTERPRET_MOVE_W_Dn_to_Dn(src, 2); \
    INTERPRET_MOVE_W_Dn_to_Dn(src, 3); \
    INTERPRET_MOVE_W_Dn_to_Dn(src, 4); \
    INTERPRET_MOVE_W_Dn_to_Dn(src, 5); \
    INTERPRET_MOVE_W_Dn_to_Dn(src, 6); \
    INTERPRET_MOVE_W_Dn_to_Dn(src, 7);

INTERPRET_MOVE_W_Dn_to_D(0);
INTERPRET_MOVE_W_Dn_to_D(1);
INTERPRET_MOVE_W_Dn_to_D(2);
INTERPRET_MOVE_W_Dn_to_D(3);
INTERPRET_MOVE_W_Dn_to_D(4);
INTERPRET_MOVE_W_Dn_to_D(5);
INTERPRET_MOVE_W_Dn_to_D(6);
INTERPRET_MOVE_W_Dn_to_D(7);


#define INTERPRET_MOVE_B_Dn_to_Dn(src, dest) \
void INTERPRET_MOVE_B_D##src##_to_D##dest(uint32_t) \
{ \
    uint32_t sr = SR & 0xfff0; \
    int8_t tmp = D##src; \
    D##dest = (D##dest & 0xffffff00) | (D##src & 0x000000ff); \
    if (unlikely(tmp == 0)) { \
        sr |= SR_Z; \
    } else { \
        if (tmp < 0) { \
            sr |= SR_N; \
        } \
    } \
    SR = sr; \
    PC += 2; \
}

#define INTERPRET_MOVE_B_Dn_to_D(src) \
    INTERPRET_MOVE_B_Dn_to_Dn(src, 0); \
    INTERPRET_MOVE_B_Dn_to_Dn(src, 1); \
    INTERPRET_MOVE_B_Dn_to_Dn(src, 2); \
    INTERPRET_MOVE_B_Dn_to_Dn(src, 3); \
    INTERPRET_MOVE_B_Dn_to_Dn(src, 4); \
    INTERPRET_MOVE_B_Dn_to_Dn(src, 5); \
    INTERPRET_MOVE_B_Dn_to_Dn(src, 6); \
    INTERPRET_MOVE_B_Dn_to_Dn(src, 7);

INTERPRET_MOVE_B_Dn_to_D(0);
INTERPRET_MOVE_B_Dn_to_D(1);
INTERPRET_MOVE_B_Dn_to_D(2);
INTERPRET_MOVE_B_Dn_to_D(3);
INTERPRET_MOVE_B_Dn_to_D(4);
INTERPRET_MOVE_B_Dn_to_D(5);
INTERPRET_MOVE_B_Dn_to_D(6);
INTERPRET_MOVE_B_Dn_to_D(7);

/*
    MOVE W/L variants of An->Dn, without sign extension and with sign checking
*/
#define INTERPRET_MOVE_L_An_to_Dn(src, dest) \
void INTERPRET_MOVE_L_A##src##_to_D##dest(uint32_t) \
{ \
    uint32_t sr = SR & 0xfff0; \
    D##dest = A##src; \
    if (unlikely(D##dest == 0)) { \
        sr |= SR_Z; \
    } else { \
        if ((int32_t)D##dest < 0) { \
            sr |= SR_N; \
        } \
    } \
    SR = sr; \
    PC += 2; \
}

#define INTERPRET_MOVE_L_An_to_D(src) \
    INTERPRET_MOVE_L_An_to_Dn(src, 0); \
    INTERPRET_MOVE_L_An_to_Dn(src, 1); \
    INTERPRET_MOVE_L_An_to_Dn(src, 2); \
    INTERPRET_MOVE_L_An_to_Dn(src, 3); \
    INTERPRET_MOVE_L_An_to_Dn(src, 4); \
    INTERPRET_MOVE_L_An_to_Dn(src, 5); \
    INTERPRET_MOVE_L_An_to_Dn(src, 6); \
    INTERPRET_MOVE_L_An_to_Dn(src, 7);

INTERPRET_MOVE_L_An_to_D(0);
INTERPRET_MOVE_L_An_to_D(1);
INTERPRET_MOVE_L_An_to_D(2);
INTERPRET_MOVE_L_An_to_D(3);
INTERPRET_MOVE_L_An_to_D(4);
INTERPRET_MOVE_L_An_to_D(5);
INTERPRET_MOVE_L_An_to_D(6);
INTERPRET_MOVE_L_An_to_D(7);


#define INTERPRET_MOVE_W_An_to_Dn(src, dest) \
void INTERPRET_MOVE_W_A##src##_to_D##dest(uint32_t) \
{ \
    uint32_t sr = SR & 0xfff0; \
    int16_t tmp = A##src; \
    D##dest = (D##dest & 0xffff0000) | (A##src & 0x0000ffff); \
    if (unlikely(tmp == 0)) { \
        sr |= SR_Z; \
    } else { \
        if (tmp < 0) { \
            sr |= SR_N; \
        } \
    } \
    SR = sr; \
    PC += 2; \
}

#define INTERPRET_MOVE_W_An_to_D(src) \
    INTERPRET_MOVE_W_An_to_Dn(src, 0); \
    INTERPRET_MOVE_W_An_to_Dn(src, 1); \
    INTERPRET_MOVE_W_An_to_Dn(src, 2); \
    INTERPRET_MOVE_W_An_to_Dn(src, 3); \
    INTERPRET_MOVE_W_An_to_Dn(src, 4); \
    INTERPRET_MOVE_W_An_to_Dn(src, 5); \
    INTERPRET_MOVE_W_An_to_Dn(src, 6); \
    INTERPRET_MOVE_W_An_to_Dn(src, 7);

INTERPRET_MOVE_W_An_to_D(0);
INTERPRET_MOVE_W_An_to_D(1);
INTERPRET_MOVE_W_An_to_D(2);
INTERPRET_MOVE_W_An_to_D(3);
INTERPRET_MOVE_W_An_to_D(4);
INTERPRET_MOVE_W_An_to_D(5);
INTERPRET_MOVE_W_An_to_D(6);
INTERPRET_MOVE_W_An_to_D(7);


/*
    Now MOVEA variant, W/L transfers Dn->An without CCR setting. W transfers sign extend
*/
#define INTERPRET_MOVEA_L_Dn_to_An(src, dest) \
void INTERPRET_MOVEA_L_D##src##_to_A##dest(uint32_t) \
{ \
    A##dest = D##src; \
    PC += 2; \
}

#define INTERPRET_MOVEA_L_Dn_to_A(src) \
    INTERPRET_MOVEA_L_Dn_to_An(src, 0); \
    INTERPRET_MOVEA_L_Dn_to_An(src, 1); \
    INTERPRET_MOVEA_L_Dn_to_An(src, 2); \
    INTERPRET_MOVEA_L_Dn_to_An(src, 3); \
    INTERPRET_MOVEA_L_Dn_to_An(src, 4); \
    INTERPRET_MOVEA_L_Dn_to_An(src, 5); \
    INTERPRET_MOVEA_L_Dn_to_An(src, 6); \
    INTERPRET_MOVEA_L_Dn_to_An(src, 7);

INTERPRET_MOVEA_L_Dn_to_A(0);
INTERPRET_MOVEA_L_Dn_to_A(1);
INTERPRET_MOVEA_L_Dn_to_A(2);
INTERPRET_MOVEA_L_Dn_to_A(3);
INTERPRET_MOVEA_L_Dn_to_A(4);
INTERPRET_MOVEA_L_Dn_to_A(5);
INTERPRET_MOVEA_L_Dn_to_A(6);
INTERPRET_MOVEA_L_Dn_to_A(7);


#define INTERPRET_MOVEA_W_Dn_to_An(src, dest) \
void INTERPRET_MOVEA_W_D##src##_to_A##dest(uint32_t) \
{ \
    A##dest = (int16_t)D##src; \
    PC += 2; \
}

#define INTERPRET_MOVEA_W_Dn_to_A(src) \
    INTERPRET_MOVEA_W_Dn_to_An(src, 0); \
    INTERPRET_MOVEA_W_Dn_to_An(src, 1); \
    INTERPRET_MOVEA_W_Dn_to_An(src, 2); \
    INTERPRET_MOVEA_W_Dn_to_An(src, 3); \
    INTERPRET_MOVEA_W_Dn_to_An(src, 4); \
    INTERPRET_MOVEA_W_Dn_to_An(src, 5); \
    INTERPRET_MOVEA_W_Dn_to_An(src, 6); \
    INTERPRET_MOVEA_W_Dn_to_An(src, 7);

INTERPRET_MOVEA_W_Dn_to_A(0);
INTERPRET_MOVEA_W_Dn_to_A(1);
INTERPRET_MOVEA_W_Dn_to_A(2);
INTERPRET_MOVEA_W_Dn_to_A(3);
INTERPRET_MOVEA_W_Dn_to_A(4);
INTERPRET_MOVEA_W_Dn_to_A(5);
INTERPRET_MOVEA_W_Dn_to_A(6);
INTERPRET_MOVEA_W_Dn_to_A(7);


/*
    MOVE B/W/L Dn to Abs.L
*/
#define INTERPRET_MOVE_L_Dn_to_ABS_L(src) \
void INTERPRET_MOVE_L_D##src##_to_ABS_L(uint32_t) \
{ \
    uint32_t sr = SR & 0xfff0; \
    uint32_t abs = *(uint32_t*)(uintptr_t)(PC + 2); \
    *(uint32_t*)(uintptr_t)abs = D##src; \
    if (unlikely(D##src == 0)) { \
        sr |= SR_Z; \
    } else { \
        if ((int32_t)D##src < 0) { \
            sr |= SR_N; \
        } \
    } \
    SR = sr; \
    PC += 6; \
}

INTERPRET_MOVE_L_Dn_to_ABS_L(0);
INTERPRET_MOVE_L_Dn_to_ABS_L(1);
INTERPRET_MOVE_L_Dn_to_ABS_L(2);
INTERPRET_MOVE_L_Dn_to_ABS_L(3);
INTERPRET_MOVE_L_Dn_to_ABS_L(4);
INTERPRET_MOVE_L_Dn_to_ABS_L(5);
INTERPRET_MOVE_L_Dn_to_ABS_L(6);
INTERPRET_MOVE_L_Dn_to_ABS_L(7);

#define INTERPRET_MOVE_L_Dn_to_ABS_W(src) \
void INTERPRET_MOVE_L_D##src##_to_ABS_W(uint32_t) \
{ \
    uint32_t sr = SR & 0xfff0; \
    uint32_t abs = *(int16_t*)(uintptr_t)(PC + 2); \
    *(uint32_t*)(uintptr_t)abs = D##src; \
    if (unlikely(D##src == 0)) { \
        sr |= SR_Z; \
    } else { \
        if ((int32_t)D##src < 0) { \
            sr |= SR_N; \
        } \
    } \
    SR = sr; \
    PC += 4; \
}

INTERPRET_MOVE_L_Dn_to_ABS_W(0);
INTERPRET_MOVE_L_Dn_to_ABS_W(1);
INTERPRET_MOVE_L_Dn_to_ABS_W(2);
INTERPRET_MOVE_L_Dn_to_ABS_W(3);
INTERPRET_MOVE_L_Dn_to_ABS_W(4);
INTERPRET_MOVE_L_Dn_to_ABS_W(5);
INTERPRET_MOVE_L_Dn_to_ABS_W(6);
INTERPRET_MOVE_L_Dn_to_ABS_W(7);

#define INTERPRET_MOVE_W_Dn_to_ABS_L(src) \
void INTERPRET_MOVE_W_D##src##_to_ABS_L(uint32_t) \
{ \
    uint32_t sr = SR & 0xfff0; \
    uint32_t abs = *(uint32_t*)(uintptr_t)(PC + 2); \
    int16_t val = (int16_t)D##src; \
    *(uint16_t*)(uintptr_t)abs = D##src; \
    if (unlikely(val == 0)) { \
        sr |= SR_Z; \
    } else { \
        if (val < 0) { \
            sr |= SR_N; \
        } \
    } \
    SR = sr; \
    PC += 6; \
}

INTERPRET_MOVE_W_Dn_to_ABS_L(0);
INTERPRET_MOVE_W_Dn_to_ABS_L(1);
INTERPRET_MOVE_W_Dn_to_ABS_L(2);
INTERPRET_MOVE_W_Dn_to_ABS_L(3);
INTERPRET_MOVE_W_Dn_to_ABS_L(4);
INTERPRET_MOVE_W_Dn_to_ABS_L(5);
INTERPRET_MOVE_W_Dn_to_ABS_L(6);
INTERPRET_MOVE_W_Dn_to_ABS_L(7);

#define INTERPRET_MOVE_W_Dn_to_ABS_W(src) \
void INTERPRET_MOVE_W_D##src##_to_ABS_W(uint32_t) \
{ \
    uint32_t sr = SR & 0xfff0; \
    uint32_t abs = *(int16_t*)(uintptr_t)(PC + 2); \
    int16_t val = (int16_t)D##src; \
    *(uint16_t*)(uintptr_t)abs = D##src; \
    if (unlikely(val == 0)) { \
        sr |= SR_Z; \
    } else { \
        if ((int32_t)val < 0) { \
            sr |= SR_N; \
        } \
    } \
    SR = sr; \
    PC += 4; \
}

INTERPRET_MOVE_W_Dn_to_ABS_W(0);
INTERPRET_MOVE_W_Dn_to_ABS_W(1);
INTERPRET_MOVE_W_Dn_to_ABS_W(2);
INTERPRET_MOVE_W_Dn_to_ABS_W(3);
INTERPRET_MOVE_W_Dn_to_ABS_W(4);
INTERPRET_MOVE_W_Dn_to_ABS_W(5);
INTERPRET_MOVE_W_Dn_to_ABS_W(6);
INTERPRET_MOVE_W_Dn_to_ABS_W(7);

#define INTERPRET_MOVE_B_Dn_to_ABS_L(src) \
void INTERPRET_MOVE_B_D##src##_to_ABS_L(uint32_t) \
{ \
    uint32_t sr = SR & 0xfff0; \
    uint32_t abs = *(uint32_t*)(uintptr_t)(PC + 2); \
    int8_t val = (int8_t)D##src; \
    *(uint8_t*)(uintptr_t)abs = D##src; \
    if (unlikely(val == 0)) { \
        sr |= SR_Z; \
    } else { \
        if (val < 0) { \
            sr |= SR_N; \
        } \
    } \
    SR = sr; \
    PC += 6; \
}

INTERPRET_MOVE_B_Dn_to_ABS_L(0);
INTERPRET_MOVE_B_Dn_to_ABS_L(1);
INTERPRET_MOVE_B_Dn_to_ABS_L(2);
INTERPRET_MOVE_B_Dn_to_ABS_L(3);
INTERPRET_MOVE_B_Dn_to_ABS_L(4);
INTERPRET_MOVE_B_Dn_to_ABS_L(5);
INTERPRET_MOVE_B_Dn_to_ABS_L(6);
INTERPRET_MOVE_B_Dn_to_ABS_L(7);

#define INTERPRET_MOVE_B_Dn_to_ABS_W(src) \
void INTERPRET_MOVE_B_D##src##_to_ABS_W(uint32_t) \
{ \
    uint32_t sr = SR & 0xfff0; \
    uint32_t abs = *(int16_t*)(uintptr_t)(PC + 2); \
    int8_t val = (int8_t)D##src; \
    *(uint8_t*)(uintptr_t)abs = D##src; \
    if (unlikely(val == 0)) { \
        sr |= SR_Z; \
    } else { \
        if ((int32_t)val < 0) { \
            sr |= SR_N; \
        } \
    } \
    SR = sr; \
    PC += 4; \
}

INTERPRET_MOVE_B_Dn_to_ABS_W(0);
INTERPRET_MOVE_B_Dn_to_ABS_W(1);
INTERPRET_MOVE_B_Dn_to_ABS_W(2);
INTERPRET_MOVE_B_Dn_to_ABS_W(3);
INTERPRET_MOVE_B_Dn_to_ABS_W(4);
INTERPRET_MOVE_B_Dn_to_ABS_W(5);
INTERPRET_MOVE_B_Dn_to_ABS_W(6);
INTERPRET_MOVE_B_Dn_to_ABS_W(7);


/*
    MOVE W/L An to Abs.L
*/
#define INTERPRET_MOVE_L_An_to_ABS_L(src) \
void INTERPRET_MOVE_L_A##src##_to_ABS_L(uint32_t) \
{ \
    uint32_t sr = SR & 0xfff0; \
    uint32_t abs = *(uint32_t*)(uintptr_t)(PC + 2); \
    *(uint32_t*)(uintptr_t)abs = A##src; \
    if (unlikely(A##src == 0)) { \
        sr |= SR_Z; \
    } else { \
        if ((int32_t)A##src < 0) { \
            sr |= SR_N; \
        } \
    } \
    SR = sr; \
    PC += 6; \
}

INTERPRET_MOVE_L_An_to_ABS_L(0);
INTERPRET_MOVE_L_An_to_ABS_L(1);
INTERPRET_MOVE_L_An_to_ABS_L(2);
INTERPRET_MOVE_L_An_to_ABS_L(3);
INTERPRET_MOVE_L_An_to_ABS_L(4);
INTERPRET_MOVE_L_An_to_ABS_L(5);
INTERPRET_MOVE_L_An_to_ABS_L(6);
INTERPRET_MOVE_L_An_to_ABS_L(7);

#define INTERPRET_MOVE_L_An_to_ABS_W(src) \
void INTERPRET_MOVE_L_A##src##_to_ABS_W(uint32_t) \
{ \
    uint32_t sr = SR & 0xfff0; \
    uint32_t abs = *(int16_t*)(uintptr_t)(PC + 2); \
    *(uint32_t*)(uintptr_t)abs = A##src; \
    if (unlikely(A##src == 0)) { \
        sr |= SR_Z; \
    } else { \
        if ((int32_t)A##src < 0) { \
            sr |= SR_N; \
        } \
    } \
    SR = sr; \
    PC += 4; \
}

INTERPRET_MOVE_L_An_to_ABS_W(0);
INTERPRET_MOVE_L_An_to_ABS_W(1);
INTERPRET_MOVE_L_An_to_ABS_W(2);
INTERPRET_MOVE_L_An_to_ABS_W(3);
INTERPRET_MOVE_L_An_to_ABS_W(4);
INTERPRET_MOVE_L_An_to_ABS_W(5);
INTERPRET_MOVE_L_An_to_ABS_W(6);
INTERPRET_MOVE_L_An_to_ABS_W(7);

#define INTERPRET_MOVE_W_An_to_ABS_L(src) \
void INTERPRET_MOVE_W_A##src##_to_ABS_L(uint32_t) \
{ \
    uint32_t sr = SR & 0xfff0; \
    uint32_t abs = *(uint32_t*)(uintptr_t)(PC + 2); \
    int16_t val = (int16_t)A##src; \
    *(uint16_t*)(uintptr_t)abs = A##src; \
    if (unlikely(val == 0)) { \
        sr |= SR_Z; \
    } else { \
        if (val < 0) { \
            sr |= SR_N; \
        } \
    } \
    SR = sr; \
    PC += 6; \
}

INTERPRET_MOVE_W_An_to_ABS_L(0);
INTERPRET_MOVE_W_An_to_ABS_L(1);
INTERPRET_MOVE_W_An_to_ABS_L(2);
INTERPRET_MOVE_W_An_to_ABS_L(3);
INTERPRET_MOVE_W_An_to_ABS_L(4);
INTERPRET_MOVE_W_An_to_ABS_L(5);
INTERPRET_MOVE_W_An_to_ABS_L(6);
INTERPRET_MOVE_W_An_to_ABS_L(7);

#define INTERPRET_MOVE_W_An_to_ABS_W(src) \
void INTERPRET_MOVE_W_A##src##_to_ABS_W(uint32_t) \
{ \
    uint32_t sr = SR & 0xfff0; \
    uint32_t abs = *(int16_t*)(uintptr_t)(PC + 2); \
    int16_t val = (int16_t)A##src; \
    *(uint16_t*)(uintptr_t)abs = A##src; \
    if (unlikely(val == 0)) { \
        sr |= SR_Z; \
    } else { \
        if ((int32_t)val < 0) { \
            sr |= SR_N; \
        } \
    } \
    SR = sr; \
    PC += 4; \
}

INTERPRET_MOVE_W_An_to_ABS_W(0);
INTERPRET_MOVE_W_An_to_ABS_W(1);
INTERPRET_MOVE_W_An_to_ABS_W(2);
INTERPRET_MOVE_W_An_to_ABS_W(3);
INTERPRET_MOVE_W_An_to_ABS_W(4);
INTERPRET_MOVE_W_An_to_ABS_W(5);
INTERPRET_MOVE_W_An_to_ABS_W(6);
INTERPRET_MOVE_W_An_to_ABS_W(7);


#define INTERPRET_MOVE_L_IMMED_to_Dn(dst) \
void INTERPRET_MOVE_L_IMMED_to_D##dst(uint32_t) \
{ \
    uint32_t sr = SR & 0xfff0; \
    int32_t val = *(int32_t*)(uintptr_t)(PC + 2); \
    D##dst = val; \
    if (unlikely(val == 0)) { \
        sr |= SR_Z; \
    } else { \
        if (val < 0) { \
            sr |= SR_N; \
        } \
    } \
    SR = sr; \
    PC += 6; \
}

INTERPRET_MOVE_L_IMMED_to_Dn(0);
INTERPRET_MOVE_L_IMMED_to_Dn(1);
INTERPRET_MOVE_L_IMMED_to_Dn(2);
INTERPRET_MOVE_L_IMMED_to_Dn(3);
INTERPRET_MOVE_L_IMMED_to_Dn(4);
INTERPRET_MOVE_L_IMMED_to_Dn(5);
INTERPRET_MOVE_L_IMMED_to_Dn(6);
INTERPRET_MOVE_L_IMMED_to_Dn(7);

#define INTERPRET_MOVE_W_IMMED_to_Dn(dst) \
void INTERPRET_MOVE_W_IMMED_to_D##dst(uint32_t) \
{ \
    uint32_t sr = SR & 0xfff0; \
    int16_t val = *(int16_t*)(uintptr_t)(PC + 2); \
    D##dst = (D##dst & 0xffff0000) | (val & 0x0000ffff); \
    if (unlikely(val == 0)) { \
        sr |= SR_Z; \
    } else { \
        if (val < 0) { \
            sr |= SR_N; \
        } \
    } \
    SR = sr; \
    PC += 4; \
}

INTERPRET_MOVE_W_IMMED_to_Dn(0);
INTERPRET_MOVE_W_IMMED_to_Dn(1);
INTERPRET_MOVE_W_IMMED_to_Dn(2);
INTERPRET_MOVE_W_IMMED_to_Dn(3);
INTERPRET_MOVE_W_IMMED_to_Dn(4);
INTERPRET_MOVE_W_IMMED_to_Dn(5);
INTERPRET_MOVE_W_IMMED_to_Dn(6);
INTERPRET_MOVE_W_IMMED_to_Dn(7);


#define INTERPRET_MOVE_B_IMMED_to_Dn(dst) \
void INTERPRET_MOVE_B_IMMED_to_D##dst(uint32_t) \
{ \
    uint32_t sr = SR & 0xfff0; \
    int8_t val = *(int8_t*)(uintptr_t)(PC + 3); \
    D##dst = (D##dst & 0xffffff00) | (val & 0x000000ff); \
    if (unlikely(val == 0)) { \
        sr |= SR_Z; \
    } else { \
        if (val < 0) { \
            sr |= SR_N; \
        } \
    } \
    SR = sr; \
    PC += 4; \
}

INTERPRET_MOVE_B_IMMED_to_Dn(0);
INTERPRET_MOVE_B_IMMED_to_Dn(1);
INTERPRET_MOVE_B_IMMED_to_Dn(2);
INTERPRET_MOVE_B_IMMED_to_Dn(3);
INTERPRET_MOVE_B_IMMED_to_Dn(4);
INTERPRET_MOVE_B_IMMED_to_Dn(5);
INTERPRET_MOVE_B_IMMED_to_Dn(6);
INTERPRET_MOVE_B_IMMED_to_Dn(7);


#define INTERPRET_MOVEA_L_IMMED_to_An(dst) \
void INTERPRET_MOVEA_L_IMMED_to_A##dst(uint32_t) \
{ \
    int32_t val = *(int32_t*)(uintptr_t)(PC + 2); \
    A##dst = val; \
    PC += 6; \
}

INTERPRET_MOVEA_L_IMMED_to_An(0);
INTERPRET_MOVEA_L_IMMED_to_An(1);
INTERPRET_MOVEA_L_IMMED_to_An(2);
INTERPRET_MOVEA_L_IMMED_to_An(3);
INTERPRET_MOVEA_L_IMMED_to_An(4);
INTERPRET_MOVEA_L_IMMED_to_An(5);
INTERPRET_MOVEA_L_IMMED_to_An(6);
INTERPRET_MOVEA_L_IMMED_to_An(7);

#define INTERPRET_MOVEA_W_IMMED_to_An(dst) \
void INTERPRET_MOVEA_W_IMMED_to_A##dst(uint32_t) \
{ \
    int16_t val = *(int16_t*)(uintptr_t)(PC + 2); \
    A##dst = val; \
    PC += 4; \
}

INTERPRET_MOVEA_W_IMMED_to_An(0);
INTERPRET_MOVEA_W_IMMED_to_An(1);
INTERPRET_MOVEA_W_IMMED_to_An(2);
INTERPRET_MOVEA_W_IMMED_to_An(3);
INTERPRET_MOVEA_W_IMMED_to_An(4);
INTERPRET_MOVEA_W_IMMED_to_An(5);
INTERPRET_MOVEA_W_IMMED_to_An(6);
INTERPRET_MOVEA_W_IMMED_to_An(7);



#define INTERPRET_MOVE_L_IMMED_to_An_Addr(dst) \
void INTERPRET_MOVE_L_IMMED_to_A##dst##_Addr(uint32_t) \
{ \
    uint32_t sr = SR & 0xfff0; \
    int32_t val = *(int32_t*)(uintptr_t)(PC + 2); \
    *(uint32_t*)(uintptr_t)A##dst = val; \
    if (unlikely(val == 0)) { \
        sr |= SR_Z; \
    } else { \
        if (val < 0) { \
            sr |= SR_N; \
        } \
    } \
    SR = sr; \
    PC += 6; \
}

INTERPRET_MOVE_L_IMMED_to_An_Addr(0);
INTERPRET_MOVE_L_IMMED_to_An_Addr(1);
INTERPRET_MOVE_L_IMMED_to_An_Addr(2);
INTERPRET_MOVE_L_IMMED_to_An_Addr(3);
INTERPRET_MOVE_L_IMMED_to_An_Addr(4);
INTERPRET_MOVE_L_IMMED_to_An_Addr(5);
INTERPRET_MOVE_L_IMMED_to_An_Addr(6);
INTERPRET_MOVE_L_IMMED_to_An_Addr(7);


#define INTERPRET_MOVE_W_IMMED_to_An_Addr(dst) \
void INTERPRET_MOVE_W_IMMED_to_A##dst##_Addr(uint32_t) \
{ \
    uint32_t sr = SR & 0xfff0; \
    int16_t val = *(int16_t*)(uintptr_t)(PC + 2); \
    *(uint16_t*)(uintptr_t)A##dst = val; \
    if (unlikely(val == 0)) { \
        sr |= SR_Z; \
    } else { \
        if (val < 0) { \
            sr |= SR_N; \
        } \
    } \
    SR = sr; \
    PC += 4; \
}

INTERPRET_MOVE_W_IMMED_to_An_Addr(0);
INTERPRET_MOVE_W_IMMED_to_An_Addr(1);
INTERPRET_MOVE_W_IMMED_to_An_Addr(2);
INTERPRET_MOVE_W_IMMED_to_An_Addr(3);
INTERPRET_MOVE_W_IMMED_to_An_Addr(4);
INTERPRET_MOVE_W_IMMED_to_An_Addr(5);
INTERPRET_MOVE_W_IMMED_to_An_Addr(6);
INTERPRET_MOVE_W_IMMED_to_An_Addr(7);


#define INTERPRET_MOVE_B_IMMED_to_An_Addr(dst) \
void INTERPRET_MOVE_B_IMMED_to_A##dst##_Addr(uint32_t) \
{ \
    uint32_t sr = SR & 0xfff0; \
    int8_t val = *(int8_t*)(uintptr_t)(PC + 3); \
    *(uint8_t*)(uintptr_t)A##dst = val; \
    if (unlikely(val == 0)) { \
        sr |= SR_Z; \
    } else { \
        if (val < 0) { \
            sr |= SR_N; \
        } \
    } \
    SR = sr; \
    PC += 4; \
}

INTERPRET_MOVE_B_IMMED_to_An_Addr(0);
INTERPRET_MOVE_B_IMMED_to_An_Addr(1);
INTERPRET_MOVE_B_IMMED_to_An_Addr(2);
INTERPRET_MOVE_B_IMMED_to_An_Addr(3);
INTERPRET_MOVE_B_IMMED_to_An_Addr(4);
INTERPRET_MOVE_B_IMMED_to_An_Addr(5);
INTERPRET_MOVE_B_IMMED_to_An_Addr(6);
INTERPRET_MOVE_B_IMMED_to_An_Addr(7);


#define INTERPRET_MOVE_L_IMMED_to_An_PostInc(dst) \
void INTERPRET_MOVE_L_IMMED_to_A##dst##_PostInc(uint32_t) \
{ \
    uint32_t sr = SR & 0xfff0; \
    int32_t val = *(int32_t*)(uintptr_t)(PC + 2); \
    *(uint32_t*)(uintptr_t)A##dst = val; \
    if (unlikely(val == 0)) { \
        sr |= SR_Z; \
    } else { \
        if (val < 0) { \
            sr |= SR_N; \
        } \
    } \
    A##dst += 4; \
    SR = sr; \
    PC += 6; \
}

INTERPRET_MOVE_L_IMMED_to_An_PostInc(0);
INTERPRET_MOVE_L_IMMED_to_An_PostInc(1);
INTERPRET_MOVE_L_IMMED_to_An_PostInc(2);
INTERPRET_MOVE_L_IMMED_to_An_PostInc(3);
INTERPRET_MOVE_L_IMMED_to_An_PostInc(4);
INTERPRET_MOVE_L_IMMED_to_An_PostInc(5);
INTERPRET_MOVE_L_IMMED_to_An_PostInc(6);
INTERPRET_MOVE_L_IMMED_to_An_PostInc(7);


#define INTERPRET_MOVE_W_IMMED_to_An_PostInc(dst) \
void INTERPRET_MOVE_W_IMMED_to_A##dst##_PostInc(uint32_t) \
{ \
    uint32_t sr = SR & 0xfff0; \
    int16_t val = *(int16_t*)(uintptr_t)(PC + 2); \
    *(uint16_t*)(uintptr_t)A##dst = val; \
    if (unlikely(val == 0)) { \
        sr |= SR_Z; \
    } else { \
        if (val < 0) { \
            sr |= SR_N; \
        } \
    } \
    A##dst += 2; \
    SR = sr; \
    PC += 4; \
}

INTERPRET_MOVE_W_IMMED_to_An_PostInc(0);
INTERPRET_MOVE_W_IMMED_to_An_PostInc(1);
INTERPRET_MOVE_W_IMMED_to_An_PostInc(2);
INTERPRET_MOVE_W_IMMED_to_An_PostInc(3);
INTERPRET_MOVE_W_IMMED_to_An_PostInc(4);
INTERPRET_MOVE_W_IMMED_to_An_PostInc(5);
INTERPRET_MOVE_W_IMMED_to_An_PostInc(6);
INTERPRET_MOVE_W_IMMED_to_An_PostInc(7);


#define INTERPRET_MOVE_B_IMMED_to_An_PostInc(dst) \
void INTERPRET_MOVE_B_IMMED_to_A##dst##_PostInc(uint32_t) \
{ \
    uint32_t sr = SR & 0xfff0; \
    int8_t val = *(int8_t*)(uintptr_t)(PC + 3); \
    *(uint8_t*)(uintptr_t)A##dst = val; \
    if (unlikely(val == 0)) { \
        sr |= SR_Z; \
    } else { \
        if (val < 0) { \
            sr |= SR_N; \
        } \
    } \
    A##dst += ((dst) == 7) ? 2 : 1; \
    SR = sr; \
    PC += 4; \
}

INTERPRET_MOVE_B_IMMED_to_An_PostInc(0);
INTERPRET_MOVE_B_IMMED_to_An_PostInc(1);
INTERPRET_MOVE_B_IMMED_to_An_PostInc(2);
INTERPRET_MOVE_B_IMMED_to_An_PostInc(3);
INTERPRET_MOVE_B_IMMED_to_An_PostInc(4);
INTERPRET_MOVE_B_IMMED_to_An_PostInc(5);
INTERPRET_MOVE_B_IMMED_to_An_PostInc(6);
INTERPRET_MOVE_B_IMMED_to_An_PostInc(7);


#define INTERPRET_MOVE_L_IMMED_to_An_PreDec(dst) \
void INTERPRET_MOVE_L_IMMED_to_A##dst##_PreDec(uint32_t) \
{ \
    uint32_t sr = SR & 0xfff0; \
    int32_t val = *(int32_t*)(uintptr_t)(PC + 2); \
    A##dst -= 4; \
    *(uint32_t*)(uintptr_t)A##dst = val; \
    if (unlikely(val == 0)) { \
        sr |= SR_Z; \
    } else { \
        if (val < 0) { \
            sr |= SR_N; \
        } \
    } \
    SR = sr; \
    PC += 6; \
}

INTERPRET_MOVE_L_IMMED_to_An_PreDec(0);
INTERPRET_MOVE_L_IMMED_to_An_PreDec(1);
INTERPRET_MOVE_L_IMMED_to_An_PreDec(2);
INTERPRET_MOVE_L_IMMED_to_An_PreDec(3);
INTERPRET_MOVE_L_IMMED_to_An_PreDec(4);
INTERPRET_MOVE_L_IMMED_to_An_PreDec(5);
INTERPRET_MOVE_L_IMMED_to_An_PreDec(6);
INTERPRET_MOVE_L_IMMED_to_An_PreDec(7);


#define INTERPRET_MOVE_W_IMMED_to_An_PreDec(dst) \
void INTERPRET_MOVE_W_IMMED_to_A##dst##_PreDec(uint32_t) \
{ \
    uint32_t sr = SR & 0xfff0; \
    int16_t val = *(int16_t*)(uintptr_t)(PC + 2); \
    A##dst -= 2; \
    *(uint16_t*)(uintptr_t)A##dst = val; \
    if (unlikely(val == 0)) { \
        sr |= SR_Z; \
    } else { \
        if (val < 0) { \
            sr |= SR_N; \
        } \
    } \
    SR = sr; \
    PC += 4; \
}

INTERPRET_MOVE_W_IMMED_to_An_PreDec(0);
INTERPRET_MOVE_W_IMMED_to_An_PreDec(1);
INTERPRET_MOVE_W_IMMED_to_An_PreDec(2);
INTERPRET_MOVE_W_IMMED_to_An_PreDec(3);
INTERPRET_MOVE_W_IMMED_to_An_PreDec(4);
INTERPRET_MOVE_W_IMMED_to_An_PreDec(5);
INTERPRET_MOVE_W_IMMED_to_An_PreDec(6);
INTERPRET_MOVE_W_IMMED_to_An_PreDec(7);


#define INTERPRET_MOVE_B_IMMED_to_An_PreDec(dst) \
void INTERPRET_MOVE_B_IMMED_to_A##dst##_PreDec(uint32_t) \
{ \
    uint32_t sr = SR & 0xfff0; \
    int8_t val = *(int8_t*)(uintptr_t)(PC + 3); \
    A##dst -= ((dst) == 7) ? 2 : 1; \
    *(uint8_t*)(uintptr_t)A##dst = val; \
    if (unlikely(val == 0)) { \
        sr |= SR_Z; \
    } else { \
        if (val < 0) { \
            sr |= SR_N; \
        } \
    } \
    SR = sr; \
    PC += 4; \
}

INTERPRET_MOVE_B_IMMED_to_An_PreDec(0);
INTERPRET_MOVE_B_IMMED_to_An_PreDec(1);
INTERPRET_MOVE_B_IMMED_to_An_PreDec(2);
INTERPRET_MOVE_B_IMMED_to_An_PreDec(3);
INTERPRET_MOVE_B_IMMED_to_An_PreDec(4);
INTERPRET_MOVE_B_IMMED_to_An_PreDec(5);
INTERPRET_MOVE_B_IMMED_to_An_PreDec(6);
INTERPRET_MOVE_B_IMMED_to_An_PreDec(7);

void INTERPRET_MOVE_B_Generic(uint32_t opcode)
{
    int8_t value;
    int src_mode = (opcode >> 3) & 7;
    int src_reg = opcode & 7;
    int dst_mode = (opcode >> 6) & 7;
    int dst_reg = (opcode >> 9) & 7;

    /* Update PC to point either to next instruction or first extension word */
    PC += 2;
    
    INTERPRET_LoadFromEffectiveAddress(src_reg, 1, &value, src_mode);
    INTERPRET_StoreToEffectiveAddress(dst_reg, value, 1, dst_mode);

    if (dst_mode != 1) {
        uint32_t sr = SR & 0xfff0;

        if (value == 0) {
            sr |= SR_Z;
        } else if (value < 0) {
            sr |= SR_N;
        }

        SR = sr;
    }
}

void INTERPRET_MOVE_W_Generic(uint32_t opcode)
{
    int16_t value;
    int src_mode = (opcode >> 3) & 7;
    int src_reg = opcode & 7;
    int dst_mode = (opcode >> 6) & 7;
    int dst_reg = (opcode >> 9) & 7;

    /* Update PC to point either to next instruction or first extension word */
    PC += 2;

    INTERPRET_LoadFromEffectiveAddress(src_reg, 2, &value, src_mode);
    INTERPRET_StoreToEffectiveAddress(dst_reg, value, 2, dst_mode);

    if (dst_mode != 1) {
        uint32_t sr = SR & 0xfff0;

        if (value == 0) {
            sr |= SR_Z;
        } else if (value < 0) {
            sr |= SR_N;
        }

        SR = sr;
    }
}

void INTERPRET_MOVE_L_Generic(uint32_t opcode)
{
    int32_t value;
    int src_mode = (opcode >> 3) & 7;
    int src_reg = opcode & 7;
    int dst_mode = (opcode >> 6) & 7;
    int dst_reg = (opcode >> 9) & 7;

    /* Update PC to point either to next instruction or first extension word */
    PC += 2;

    /* Load data and store it back */
    INTERPRET_LoadFromEffectiveAddress(src_reg, 4, &value, src_mode);
    INTERPRET_StoreToEffectiveAddress(dst_reg, value, 4, dst_mode);

    /* If destination is not An, calculate CCR */
    if (dst_mode != 1) {
        uint32_t sr = SR & 0xfff0;

        if (value == 0) {
            sr |= SR_Z;
        } else if (value < 0) {
            sr |= SR_N;
        }

        SR = sr;
    }
}

static constexpr std::array<INTERPRET_Function, 4096> BuildInsnTable_Long()
{
    std::array<INTERPRET_Function, 4096> table{};

    auto fill = [&table](int first, int last, INTERPRET_Function func) {
        for (int i = first; i <= last; ++i)
            table[i] = func;
    };

    /* Make all entries unimplemented first */
    fill(00000, 07777, INTERPRET_UNIMPLEMENTED);

    /*
        Fill all allowed defaults with generic version:
        modes 0 to 6 allow all register combinations
        mode 7 allows only register 0 and 1

        Register and mode are decoded as reg:mod - the loop can therefore 
        start at octal 000 (mode 0, register 0) and iterate up to octal 017
        (mode 7, register 1). Source decoded as mod:reg can go always from 
        000 (mode 0, register 0) up to 074 (mode 7, register 4)
    */
    for (int dst_rm = 0; dst_rm <= 071; dst_rm++) {
        int dst_mode = dst_rm >> 3;
        int dst_reg  = dst_rm & 7;
        int base = (dst_reg << 9) | (dst_mode << 6);
        fill(base + 00000, base + 00074, INTERPRET_MOVE_L_Generic);
    }

    table[00000] = INTERPRET_MOVE_L_D0_to_D0;
    table[00001] = INTERPRET_MOVE_L_D1_to_D0;
    table[00002] = INTERPRET_MOVE_L_D2_to_D0;
    table[00003] = INTERPRET_MOVE_L_D3_to_D0;
    table[00004] = INTERPRET_MOVE_L_D4_to_D0;
    table[00005] = INTERPRET_MOVE_L_D5_to_D0;
    table[00006] = INTERPRET_MOVE_L_D6_to_D0;
    table[00007] = INTERPRET_MOVE_L_D7_to_D0;
    table[01000] = INTERPRET_MOVE_L_D0_to_D1;
    table[01001] = INTERPRET_MOVE_L_D1_to_D1;
    table[01002] = INTERPRET_MOVE_L_D2_to_D1;
    table[01003] = INTERPRET_MOVE_L_D3_to_D1;
    table[01004] = INTERPRET_MOVE_L_D4_to_D1;
    table[01005] = INTERPRET_MOVE_L_D5_to_D1;
    table[01006] = INTERPRET_MOVE_L_D6_to_D1;
    table[01007] = INTERPRET_MOVE_L_D7_to_D1;
    table[02000] = INTERPRET_MOVE_L_D0_to_D2;
    table[02001] = INTERPRET_MOVE_L_D1_to_D2;
    table[02002] = INTERPRET_MOVE_L_D2_to_D2;
    table[02003] = INTERPRET_MOVE_L_D3_to_D2;
    table[02004] = INTERPRET_MOVE_L_D4_to_D2;
    table[02005] = INTERPRET_MOVE_L_D5_to_D2;
    table[02006] = INTERPRET_MOVE_L_D6_to_D2;
    table[02007] = INTERPRET_MOVE_L_D7_to_D2;
    table[03000] = INTERPRET_MOVE_L_D0_to_D3;
    table[03001] = INTERPRET_MOVE_L_D1_to_D3;
    table[03002] = INTERPRET_MOVE_L_D2_to_D3;
    table[03003] = INTERPRET_MOVE_L_D3_to_D3;
    table[03004] = INTERPRET_MOVE_L_D4_to_D3;
    table[03005] = INTERPRET_MOVE_L_D5_to_D3;
    table[03006] = INTERPRET_MOVE_L_D6_to_D3;
    table[03007] = INTERPRET_MOVE_L_D7_to_D3;
    table[04000] = INTERPRET_MOVE_L_D0_to_D4;
    table[04001] = INTERPRET_MOVE_L_D1_to_D4;
    table[04002] = INTERPRET_MOVE_L_D2_to_D4;
    table[04003] = INTERPRET_MOVE_L_D3_to_D4;
    table[04004] = INTERPRET_MOVE_L_D4_to_D4;
    table[04005] = INTERPRET_MOVE_L_D5_to_D4;
    table[04006] = INTERPRET_MOVE_L_D6_to_D4;
    table[04007] = INTERPRET_MOVE_L_D7_to_D4;
    table[05000] = INTERPRET_MOVE_L_D0_to_D5;
    table[05001] = INTERPRET_MOVE_L_D1_to_D5;
    table[05002] = INTERPRET_MOVE_L_D2_to_D5;
    table[05003] = INTERPRET_MOVE_L_D3_to_D5;
    table[05004] = INTERPRET_MOVE_L_D4_to_D5;
    table[05005] = INTERPRET_MOVE_L_D5_to_D5;
    table[05006] = INTERPRET_MOVE_L_D6_to_D5;
    table[05007] = INTERPRET_MOVE_L_D7_to_D5;
    table[06000] = INTERPRET_MOVE_L_D0_to_D6;
    table[06001] = INTERPRET_MOVE_L_D1_to_D6;
    table[06002] = INTERPRET_MOVE_L_D2_to_D6;
    table[06003] = INTERPRET_MOVE_L_D3_to_D6;
    table[06004] = INTERPRET_MOVE_L_D4_to_D6;
    table[06005] = INTERPRET_MOVE_L_D5_to_D6;
    table[06006] = INTERPRET_MOVE_L_D6_to_D6;
    table[06007] = INTERPRET_MOVE_L_D7_to_D6;
    table[07000] = INTERPRET_MOVE_L_D0_to_D7;
    table[07001] = INTERPRET_MOVE_L_D1_to_D7;
    table[07002] = INTERPRET_MOVE_L_D2_to_D7;
    table[07003] = INTERPRET_MOVE_L_D3_to_D7;
    table[07004] = INTERPRET_MOVE_L_D4_to_D7;
    table[07005] = INTERPRET_MOVE_L_D5_to_D7;
    table[07006] = INTERPRET_MOVE_L_D6_to_D7;
    table[07007] = INTERPRET_MOVE_L_D7_to_D7;
    table[00010] = INTERPRET_MOVE_L_A0_to_D0;
    table[00011] = INTERPRET_MOVE_L_A1_to_D0;
    table[00012] = INTERPRET_MOVE_L_A2_to_D0;
    table[00013] = INTERPRET_MOVE_L_A3_to_D0;
    table[00014] = INTERPRET_MOVE_L_A4_to_D0;
    table[00015] = INTERPRET_MOVE_L_A5_to_D0;
    table[00016] = INTERPRET_MOVE_L_A6_to_D0;
    table[00017] = INTERPRET_MOVE_L_A7_to_D0;
    table[01010] = INTERPRET_MOVE_L_A0_to_D1;
    table[01011] = INTERPRET_MOVE_L_A1_to_D1;
    table[01012] = INTERPRET_MOVE_L_A2_to_D1;
    table[01013] = INTERPRET_MOVE_L_A3_to_D1;
    table[01014] = INTERPRET_MOVE_L_A4_to_D1;
    table[01015] = INTERPRET_MOVE_L_A5_to_D1;
    table[01016] = INTERPRET_MOVE_L_A6_to_D1;
    table[01017] = INTERPRET_MOVE_L_A7_to_D1;
    table[02010] = INTERPRET_MOVE_L_A0_to_D2;
    table[02011] = INTERPRET_MOVE_L_A1_to_D2;
    table[02012] = INTERPRET_MOVE_L_A2_to_D2;
    table[02013] = INTERPRET_MOVE_L_A3_to_D2;
    table[02014] = INTERPRET_MOVE_L_A4_to_D2;
    table[02015] = INTERPRET_MOVE_L_A5_to_D2;
    table[02016] = INTERPRET_MOVE_L_A6_to_D2;
    table[02017] = INTERPRET_MOVE_L_A7_to_D2;
    table[03010] = INTERPRET_MOVE_L_A0_to_D3;
    table[03011] = INTERPRET_MOVE_L_A1_to_D3;
    table[03012] = INTERPRET_MOVE_L_A2_to_D3;
    table[03013] = INTERPRET_MOVE_L_A3_to_D3;
    table[03014] = INTERPRET_MOVE_L_A4_to_D3;
    table[03015] = INTERPRET_MOVE_L_A5_to_D3;
    table[03016] = INTERPRET_MOVE_L_A6_to_D3;
    table[03017] = INTERPRET_MOVE_L_A7_to_D3;
    table[04010] = INTERPRET_MOVE_L_A0_to_D4;
    table[04011] = INTERPRET_MOVE_L_A1_to_D4;
    table[04012] = INTERPRET_MOVE_L_A2_to_D4;
    table[04013] = INTERPRET_MOVE_L_A3_to_D4;
    table[04014] = INTERPRET_MOVE_L_A4_to_D4;
    table[04015] = INTERPRET_MOVE_L_A5_to_D4;
    table[04016] = INTERPRET_MOVE_L_A6_to_D4;
    table[04017] = INTERPRET_MOVE_L_A7_to_D4;
    table[05010] = INTERPRET_MOVE_L_A0_to_D5;
    table[05011] = INTERPRET_MOVE_L_A1_to_D5;
    table[05012] = INTERPRET_MOVE_L_A2_to_D5;
    table[05013] = INTERPRET_MOVE_L_A3_to_D5;
    table[05014] = INTERPRET_MOVE_L_A4_to_D5;
    table[05015] = INTERPRET_MOVE_L_A5_to_D5;
    table[05016] = INTERPRET_MOVE_L_A6_to_D5;
    table[05017] = INTERPRET_MOVE_L_A7_to_D5;
    table[06010] = INTERPRET_MOVE_L_A0_to_D6;
    table[06011] = INTERPRET_MOVE_L_A1_to_D6;
    table[06012] = INTERPRET_MOVE_L_A2_to_D6;
    table[06013] = INTERPRET_MOVE_L_A3_to_D6;
    table[06014] = INTERPRET_MOVE_L_A4_to_D6;
    table[06015] = INTERPRET_MOVE_L_A5_to_D6;
    table[06016] = INTERPRET_MOVE_L_A6_to_D6;
    table[06017] = INTERPRET_MOVE_L_A7_to_D6;
    table[07010] = INTERPRET_MOVE_L_A0_to_D7;
    table[07011] = INTERPRET_MOVE_L_A1_to_D7;
    table[07012] = INTERPRET_MOVE_L_A2_to_D7;
    table[07013] = INTERPRET_MOVE_L_A3_to_D7;
    table[07014] = INTERPRET_MOVE_L_A4_to_D7;
    table[07015] = INTERPRET_MOVE_L_A5_to_D7;
    table[07016] = INTERPRET_MOVE_L_A6_to_D7;
    table[07017] = INTERPRET_MOVE_L_A7_to_D7;

    table[00100] = INTERPRET_MOVEA_L_D0_to_A0;
    table[00101] = INTERPRET_MOVEA_L_D1_to_A0;
    table[00102] = INTERPRET_MOVEA_L_D2_to_A0;
    table[00103] = INTERPRET_MOVEA_L_D3_to_A0;
    table[00104] = INTERPRET_MOVEA_L_D4_to_A0;
    table[00105] = INTERPRET_MOVEA_L_D5_to_A0;
    table[00106] = INTERPRET_MOVEA_L_D6_to_A0;
    table[00107] = INTERPRET_MOVEA_L_D7_to_A0;
    table[01100] = INTERPRET_MOVEA_L_D0_to_A1;
    table[01101] = INTERPRET_MOVEA_L_D1_to_A1;
    table[01102] = INTERPRET_MOVEA_L_D2_to_A1;
    table[01103] = INTERPRET_MOVEA_L_D3_to_A1;
    table[01104] = INTERPRET_MOVEA_L_D4_to_A1;
    table[01105] = INTERPRET_MOVEA_L_D5_to_A1;
    table[01106] = INTERPRET_MOVEA_L_D6_to_A1;
    table[01107] = INTERPRET_MOVEA_L_D7_to_A1;
    table[02100] = INTERPRET_MOVEA_L_D0_to_A2;
    table[02101] = INTERPRET_MOVEA_L_D1_to_A2;
    table[02102] = INTERPRET_MOVEA_L_D2_to_A2;
    table[02103] = INTERPRET_MOVEA_L_D3_to_A2;
    table[02104] = INTERPRET_MOVEA_L_D4_to_A2;
    table[02105] = INTERPRET_MOVEA_L_D5_to_A2;
    table[02106] = INTERPRET_MOVEA_L_D6_to_A2;
    table[02107] = INTERPRET_MOVEA_L_D7_to_A2;
    table[03100] = INTERPRET_MOVEA_L_D0_to_A3;
    table[03101] = INTERPRET_MOVEA_L_D1_to_A3;
    table[03102] = INTERPRET_MOVEA_L_D2_to_A3;
    table[03103] = INTERPRET_MOVEA_L_D3_to_A3;
    table[03104] = INTERPRET_MOVEA_L_D4_to_A3;
    table[03105] = INTERPRET_MOVEA_L_D5_to_A3;
    table[03106] = INTERPRET_MOVEA_L_D6_to_A3;
    table[03107] = INTERPRET_MOVEA_L_D7_to_A3;
    table[04100] = INTERPRET_MOVEA_L_D0_to_A4;
    table[04101] = INTERPRET_MOVEA_L_D1_to_A4;
    table[04102] = INTERPRET_MOVEA_L_D2_to_A4;
    table[04103] = INTERPRET_MOVEA_L_D3_to_A4;
    table[04104] = INTERPRET_MOVEA_L_D4_to_A4;
    table[04105] = INTERPRET_MOVEA_L_D5_to_A4;
    table[04106] = INTERPRET_MOVEA_L_D6_to_A4;
    table[04107] = INTERPRET_MOVEA_L_D7_to_A4;
    table[05100] = INTERPRET_MOVEA_L_D0_to_A5;
    table[05101] = INTERPRET_MOVEA_L_D1_to_A5;
    table[05102] = INTERPRET_MOVEA_L_D2_to_A5;
    table[05103] = INTERPRET_MOVEA_L_D3_to_A5;
    table[05104] = INTERPRET_MOVEA_L_D4_to_A5;
    table[05105] = INTERPRET_MOVEA_L_D5_to_A5;
    table[05106] = INTERPRET_MOVEA_L_D6_to_A5;
    table[05107] = INTERPRET_MOVEA_L_D7_to_A5;
    table[06100] = INTERPRET_MOVEA_L_D0_to_A6;
    table[06101] = INTERPRET_MOVEA_L_D1_to_A6;
    table[06102] = INTERPRET_MOVEA_L_D2_to_A6;
    table[06103] = INTERPRET_MOVEA_L_D3_to_A6;
    table[06104] = INTERPRET_MOVEA_L_D4_to_A6;
    table[06105] = INTERPRET_MOVEA_L_D5_to_A6;
    table[06106] = INTERPRET_MOVEA_L_D6_to_A6;
    table[06107] = INTERPRET_MOVEA_L_D7_to_A6;
    table[07100] = INTERPRET_MOVEA_L_D0_to_A7;
    table[07101] = INTERPRET_MOVEA_L_D1_to_A7;
    table[07102] = INTERPRET_MOVEA_L_D2_to_A7;
    table[07103] = INTERPRET_MOVEA_L_D3_to_A7;
    table[07104] = INTERPRET_MOVEA_L_D4_to_A7;
    table[07105] = INTERPRET_MOVEA_L_D5_to_A7;
    table[07106] = INTERPRET_MOVEA_L_D6_to_A7;
    table[07107] = INTERPRET_MOVEA_L_D7_to_A7;

    table[00700] = INTERPRET_MOVE_L_D0_to_ABS_W;
    table[00701] = INTERPRET_MOVE_L_D1_to_ABS_W;
    table[00702] = INTERPRET_MOVE_L_D2_to_ABS_W;
    table[00703] = INTERPRET_MOVE_L_D3_to_ABS_W;
    table[00704] = INTERPRET_MOVE_L_D4_to_ABS_W;
    table[00705] = INTERPRET_MOVE_L_D5_to_ABS_W;
    table[00706] = INTERPRET_MOVE_L_D6_to_ABS_W;
    table[00707] = INTERPRET_MOVE_L_D7_to_ABS_W;

    table[01700] = INTERPRET_MOVE_L_D0_to_ABS_L;
    table[01701] = INTERPRET_MOVE_L_D1_to_ABS_L;
    table[01702] = INTERPRET_MOVE_L_D2_to_ABS_L;
    table[01703] = INTERPRET_MOVE_L_D3_to_ABS_L;
    table[01704] = INTERPRET_MOVE_L_D4_to_ABS_L;
    table[01705] = INTERPRET_MOVE_L_D5_to_ABS_L;
    table[01706] = INTERPRET_MOVE_L_D6_to_ABS_L;
    table[01707] = INTERPRET_MOVE_L_D7_to_ABS_L;

    table[00710] = INTERPRET_MOVE_L_A0_to_ABS_W;
    table[00711] = INTERPRET_MOVE_L_A1_to_ABS_W;
    table[00712] = INTERPRET_MOVE_L_A2_to_ABS_W;
    table[00713] = INTERPRET_MOVE_L_A3_to_ABS_W;
    table[00714] = INTERPRET_MOVE_L_A4_to_ABS_W;
    table[00715] = INTERPRET_MOVE_L_A5_to_ABS_W;
    table[00716] = INTERPRET_MOVE_L_A6_to_ABS_W;
    table[00717] = INTERPRET_MOVE_L_A7_to_ABS_W;

    table[01710] = INTERPRET_MOVE_L_A0_to_ABS_L;
    table[01711] = INTERPRET_MOVE_L_A1_to_ABS_L;
    table[01712] = INTERPRET_MOVE_L_A2_to_ABS_L;
    table[01713] = INTERPRET_MOVE_L_A3_to_ABS_L;
    table[01714] = INTERPRET_MOVE_L_A4_to_ABS_L;
    table[01715] = INTERPRET_MOVE_L_A5_to_ABS_L;
    table[01716] = INTERPRET_MOVE_L_A6_to_ABS_L;
    table[01717] = INTERPRET_MOVE_L_A7_to_ABS_L;

    table[00074] = INTERPRET_MOVE_L_IMMED_to_D0;
    table[01074] = INTERPRET_MOVE_L_IMMED_to_D1;
    table[02074] = INTERPRET_MOVE_L_IMMED_to_D2;
    table[03074] = INTERPRET_MOVE_L_IMMED_to_D3;
    table[04074] = INTERPRET_MOVE_L_IMMED_to_D4;
    table[05074] = INTERPRET_MOVE_L_IMMED_to_D5;
    table[06074] = INTERPRET_MOVE_L_IMMED_to_D6;
    table[07074] = INTERPRET_MOVE_L_IMMED_to_D7;

    table[00174] = INTERPRET_MOVEA_L_IMMED_to_A0;
    table[01174] = INTERPRET_MOVEA_L_IMMED_to_A1;
    table[02174] = INTERPRET_MOVEA_L_IMMED_to_A2;
    table[03174] = INTERPRET_MOVEA_L_IMMED_to_A3;
    table[04174] = INTERPRET_MOVEA_L_IMMED_to_A4;
    table[05174] = INTERPRET_MOVEA_L_IMMED_to_A5;
    table[06174] = INTERPRET_MOVEA_L_IMMED_to_A6;
    table[07174] = INTERPRET_MOVEA_L_IMMED_to_A7;

    table[00274] = INTERPRET_MOVE_L_IMMED_to_A0_Addr;
    table[01274] = INTERPRET_MOVE_L_IMMED_to_A1_Addr;
    table[02274] = INTERPRET_MOVE_L_IMMED_to_A2_Addr;
    table[03274] = INTERPRET_MOVE_L_IMMED_to_A3_Addr;
    table[04274] = INTERPRET_MOVE_L_IMMED_to_A4_Addr;
    table[05274] = INTERPRET_MOVE_L_IMMED_to_A5_Addr;
    table[06274] = INTERPRET_MOVE_L_IMMED_to_A6_Addr;
    table[07274] = INTERPRET_MOVE_L_IMMED_to_A7_Addr;

    table[00374] = INTERPRET_MOVE_L_IMMED_to_A0_PostInc;
    table[01374] = INTERPRET_MOVE_L_IMMED_to_A1_PostInc;
    table[02374] = INTERPRET_MOVE_L_IMMED_to_A2_PostInc;
    table[03374] = INTERPRET_MOVE_L_IMMED_to_A3_PostInc;
    table[04374] = INTERPRET_MOVE_L_IMMED_to_A4_PostInc;
    table[05374] = INTERPRET_MOVE_L_IMMED_to_A5_PostInc;
    table[06374] = INTERPRET_MOVE_L_IMMED_to_A6_PostInc;
    table[07374] = INTERPRET_MOVE_L_IMMED_to_A7_PostInc;

    table[00474] = INTERPRET_MOVE_L_IMMED_to_A0_PreDec;
    table[01474] = INTERPRET_MOVE_L_IMMED_to_A1_PreDec;
    table[02474] = INTERPRET_MOVE_L_IMMED_to_A2_PreDec;
    table[03474] = INTERPRET_MOVE_L_IMMED_to_A3_PreDec;
    table[04474] = INTERPRET_MOVE_L_IMMED_to_A4_PreDec;
    table[05474] = INTERPRET_MOVE_L_IMMED_to_A5_PreDec;
    table[06474] = INTERPRET_MOVE_L_IMMED_to_A6_PreDec;
    table[07474] = INTERPRET_MOVE_L_IMMED_to_A7_PreDec;

    return table;
}


static constexpr std::array<INTERPRET_Function, 4096> BuildInsnTable_Word()
{
    std::array<INTERPRET_Function, 4096> table{};

    auto fill = [&table](int first, int last, INTERPRET_Function func) {
        for (int i = first; i <= last; ++i)
            table[i] = func;
    };

    /* Make all entries unimplemented first */
    fill(00000, 07777, INTERPRET_UNIMPLEMENTED);

    /*
        Fill all allowed defaults with generic version:
        modes 0 to 6 allow all register combinations
        mode 7 allows only register 0 and 1

        Register and mode are decoded as reg:mod - the loop can therefore 
        start at octal 000 (mode 0, register 0) and iterate up to octal 017
        (mode 7, register 1). Source decoded as mod:reg can go always from 
        000 (mode 0, register 0) up to 074 (mode 7, register 4)
    */
    for (int dst_rm = 0; dst_rm <= 071; dst_rm++) {
        int dst_mode = dst_rm >> 3;
        int dst_reg  = dst_rm & 7;
        int base = (dst_reg << 9) | (dst_mode << 6);
        fill(base + 00000, base + 00074, INTERPRET_MOVE_W_Generic);
    }

    table[00000] = INTERPRET_MOVE_W_D0_to_D0;
    table[00001] = INTERPRET_MOVE_W_D1_to_D0;
    table[00002] = INTERPRET_MOVE_W_D2_to_D0;
    table[00003] = INTERPRET_MOVE_W_D3_to_D0;
    table[00004] = INTERPRET_MOVE_W_D4_to_D0;
    table[00005] = INTERPRET_MOVE_W_D5_to_D0;
    table[00006] = INTERPRET_MOVE_W_D6_to_D0;
    table[00007] = INTERPRET_MOVE_W_D7_to_D0;
    table[01000] = INTERPRET_MOVE_W_D0_to_D1;
    table[01001] = INTERPRET_MOVE_W_D1_to_D1;
    table[01002] = INTERPRET_MOVE_W_D2_to_D1;
    table[01003] = INTERPRET_MOVE_W_D3_to_D1;
    table[01004] = INTERPRET_MOVE_W_D4_to_D1;
    table[01005] = INTERPRET_MOVE_W_D5_to_D1;
    table[01006] = INTERPRET_MOVE_W_D6_to_D1;
    table[01007] = INTERPRET_MOVE_W_D7_to_D1;
    table[02000] = INTERPRET_MOVE_W_D0_to_D2;
    table[02001] = INTERPRET_MOVE_W_D1_to_D2;
    table[02002] = INTERPRET_MOVE_W_D2_to_D2;
    table[02003] = INTERPRET_MOVE_W_D3_to_D2;
    table[02004] = INTERPRET_MOVE_W_D4_to_D2;
    table[02005] = INTERPRET_MOVE_W_D5_to_D2;
    table[02006] = INTERPRET_MOVE_W_D6_to_D2;
    table[02007] = INTERPRET_MOVE_W_D7_to_D2;
    table[03000] = INTERPRET_MOVE_W_D0_to_D3;
    table[03001] = INTERPRET_MOVE_W_D1_to_D3;
    table[03002] = INTERPRET_MOVE_W_D2_to_D3;
    table[03003] = INTERPRET_MOVE_W_D3_to_D3;
    table[03004] = INTERPRET_MOVE_W_D4_to_D3;
    table[03005] = INTERPRET_MOVE_W_D5_to_D3;
    table[03006] = INTERPRET_MOVE_W_D6_to_D3;
    table[03007] = INTERPRET_MOVE_W_D7_to_D3;
    table[04000] = INTERPRET_MOVE_W_D0_to_D4;
    table[04001] = INTERPRET_MOVE_W_D1_to_D4;
    table[04002] = INTERPRET_MOVE_W_D2_to_D4;
    table[04003] = INTERPRET_MOVE_W_D3_to_D4;
    table[04004] = INTERPRET_MOVE_W_D4_to_D4;
    table[04005] = INTERPRET_MOVE_W_D5_to_D4;
    table[04006] = INTERPRET_MOVE_W_D6_to_D4;
    table[04007] = INTERPRET_MOVE_W_D7_to_D4;
    table[05000] = INTERPRET_MOVE_W_D0_to_D5;
    table[05001] = INTERPRET_MOVE_W_D1_to_D5;
    table[05002] = INTERPRET_MOVE_W_D2_to_D5;
    table[05003] = INTERPRET_MOVE_W_D3_to_D5;
    table[05004] = INTERPRET_MOVE_W_D4_to_D5;
    table[05005] = INTERPRET_MOVE_W_D5_to_D5;
    table[05006] = INTERPRET_MOVE_W_D6_to_D5;
    table[05007] = INTERPRET_MOVE_W_D7_to_D5;
    table[06000] = INTERPRET_MOVE_W_D0_to_D6;
    table[06001] = INTERPRET_MOVE_W_D1_to_D6;
    table[06002] = INTERPRET_MOVE_W_D2_to_D6;
    table[06003] = INTERPRET_MOVE_W_D3_to_D6;
    table[06004] = INTERPRET_MOVE_W_D4_to_D6;
    table[06005] = INTERPRET_MOVE_W_D5_to_D6;
    table[06006] = INTERPRET_MOVE_W_D6_to_D6;
    table[06007] = INTERPRET_MOVE_W_D7_to_D6;
    table[07000] = INTERPRET_MOVE_W_D0_to_D7;
    table[07001] = INTERPRET_MOVE_W_D1_to_D7;
    table[07002] = INTERPRET_MOVE_W_D2_to_D7;
    table[07003] = INTERPRET_MOVE_W_D3_to_D7;
    table[07004] = INTERPRET_MOVE_W_D4_to_D7;
    table[07005] = INTERPRET_MOVE_W_D5_to_D7;
    table[07006] = INTERPRET_MOVE_W_D6_to_D7;
    table[07007] = INTERPRET_MOVE_W_D7_to_D7;
    table[00010] = INTERPRET_MOVE_W_A0_to_D0;
    table[00011] = INTERPRET_MOVE_W_A1_to_D0;
    table[00012] = INTERPRET_MOVE_W_A2_to_D0;
    table[00013] = INTERPRET_MOVE_W_A3_to_D0;
    table[00014] = INTERPRET_MOVE_W_A4_to_D0;
    table[00015] = INTERPRET_MOVE_W_A5_to_D0;
    table[00016] = INTERPRET_MOVE_W_A6_to_D0;
    table[00017] = INTERPRET_MOVE_W_A7_to_D0;
    table[01010] = INTERPRET_MOVE_W_A0_to_D1;
    table[01011] = INTERPRET_MOVE_W_A1_to_D1;
    table[01012] = INTERPRET_MOVE_W_A2_to_D1;
    table[01013] = INTERPRET_MOVE_W_A3_to_D1;
    table[01014] = INTERPRET_MOVE_W_A4_to_D1;
    table[01015] = INTERPRET_MOVE_W_A5_to_D1;
    table[01016] = INTERPRET_MOVE_W_A6_to_D1;
    table[01017] = INTERPRET_MOVE_W_A7_to_D1;
    table[02010] = INTERPRET_MOVE_W_A0_to_D2;
    table[02011] = INTERPRET_MOVE_W_A1_to_D2;
    table[02012] = INTERPRET_MOVE_W_A2_to_D2;
    table[02013] = INTERPRET_MOVE_W_A3_to_D2;
    table[02014] = INTERPRET_MOVE_W_A4_to_D2;
    table[02015] = INTERPRET_MOVE_W_A5_to_D2;
    table[02016] = INTERPRET_MOVE_W_A6_to_D2;
    table[02017] = INTERPRET_MOVE_W_A7_to_D2;
    table[03010] = INTERPRET_MOVE_W_A0_to_D3;
    table[03011] = INTERPRET_MOVE_W_A1_to_D3;
    table[03012] = INTERPRET_MOVE_W_A2_to_D3;
    table[03013] = INTERPRET_MOVE_W_A3_to_D3;
    table[03014] = INTERPRET_MOVE_W_A4_to_D3;
    table[03015] = INTERPRET_MOVE_W_A5_to_D3;
    table[03016] = INTERPRET_MOVE_W_A6_to_D3;
    table[03017] = INTERPRET_MOVE_W_A7_to_D3;
    table[04010] = INTERPRET_MOVE_W_A0_to_D4;
    table[04011] = INTERPRET_MOVE_W_A1_to_D4;
    table[04012] = INTERPRET_MOVE_W_A2_to_D4;
    table[04013] = INTERPRET_MOVE_W_A3_to_D4;
    table[04014] = INTERPRET_MOVE_W_A4_to_D4;
    table[04015] = INTERPRET_MOVE_W_A5_to_D4;
    table[04016] = INTERPRET_MOVE_W_A6_to_D4;
    table[04017] = INTERPRET_MOVE_W_A7_to_D4;
    table[05010] = INTERPRET_MOVE_W_A0_to_D5;
    table[05011] = INTERPRET_MOVE_W_A1_to_D5;
    table[05012] = INTERPRET_MOVE_W_A2_to_D5;
    table[05013] = INTERPRET_MOVE_W_A3_to_D5;
    table[05014] = INTERPRET_MOVE_W_A4_to_D5;
    table[05015] = INTERPRET_MOVE_W_A5_to_D5;
    table[05016] = INTERPRET_MOVE_W_A6_to_D5;
    table[05017] = INTERPRET_MOVE_W_A7_to_D5;
    table[06010] = INTERPRET_MOVE_W_A0_to_D6;
    table[06011] = INTERPRET_MOVE_W_A1_to_D6;
    table[06012] = INTERPRET_MOVE_W_A2_to_D6;
    table[06013] = INTERPRET_MOVE_W_A3_to_D6;
    table[06014] = INTERPRET_MOVE_W_A4_to_D6;
    table[06015] = INTERPRET_MOVE_W_A5_to_D6;
    table[06016] = INTERPRET_MOVE_W_A6_to_D6;
    table[06017] = INTERPRET_MOVE_W_A7_to_D6;
    table[07010] = INTERPRET_MOVE_W_A0_to_D7;
    table[07011] = INTERPRET_MOVE_W_A1_to_D7;
    table[07012] = INTERPRET_MOVE_W_A2_to_D7;
    table[07013] = INTERPRET_MOVE_W_A3_to_D7;
    table[07014] = INTERPRET_MOVE_W_A4_to_D7;
    table[07015] = INTERPRET_MOVE_W_A5_to_D7;
    table[07016] = INTERPRET_MOVE_W_A6_to_D7;
    table[07017] = INTERPRET_MOVE_W_A7_to_D7;

    table[00100] = INTERPRET_MOVEA_W_D0_to_A0;
    table[00101] = INTERPRET_MOVEA_W_D1_to_A0;
    table[00102] = INTERPRET_MOVEA_W_D2_to_A0;
    table[00103] = INTERPRET_MOVEA_W_D3_to_A0;
    table[00104] = INTERPRET_MOVEA_W_D4_to_A0;
    table[00105] = INTERPRET_MOVEA_W_D5_to_A0;
    table[00106] = INTERPRET_MOVEA_W_D6_to_A0;
    table[00107] = INTERPRET_MOVEA_W_D7_to_A0;
    table[01100] = INTERPRET_MOVEA_W_D0_to_A1;
    table[01101] = INTERPRET_MOVEA_W_D1_to_A1;
    table[01102] = INTERPRET_MOVEA_W_D2_to_A1;
    table[01103] = INTERPRET_MOVEA_W_D3_to_A1;
    table[01104] = INTERPRET_MOVEA_W_D4_to_A1;
    table[01105] = INTERPRET_MOVEA_W_D5_to_A1;
    table[01106] = INTERPRET_MOVEA_W_D6_to_A1;
    table[01107] = INTERPRET_MOVEA_W_D7_to_A1;
    table[02100] = INTERPRET_MOVEA_W_D0_to_A2;
    table[02101] = INTERPRET_MOVEA_W_D1_to_A2;
    table[02102] = INTERPRET_MOVEA_W_D2_to_A2;
    table[02103] = INTERPRET_MOVEA_W_D3_to_A2;
    table[02104] = INTERPRET_MOVEA_W_D4_to_A2;
    table[02105] = INTERPRET_MOVEA_W_D5_to_A2;
    table[02106] = INTERPRET_MOVEA_W_D6_to_A2;
    table[02107] = INTERPRET_MOVEA_W_D7_to_A2;
    table[03100] = INTERPRET_MOVEA_W_D0_to_A3;
    table[03101] = INTERPRET_MOVEA_W_D1_to_A3;
    table[03102] = INTERPRET_MOVEA_W_D2_to_A3;
    table[03103] = INTERPRET_MOVEA_W_D3_to_A3;
    table[03104] = INTERPRET_MOVEA_W_D4_to_A3;
    table[03105] = INTERPRET_MOVEA_W_D5_to_A3;
    table[03106] = INTERPRET_MOVEA_W_D6_to_A3;
    table[03107] = INTERPRET_MOVEA_W_D7_to_A3;
    table[04100] = INTERPRET_MOVEA_W_D0_to_A4;
    table[04101] = INTERPRET_MOVEA_W_D1_to_A4;
    table[04102] = INTERPRET_MOVEA_W_D2_to_A4;
    table[04103] = INTERPRET_MOVEA_W_D3_to_A4;
    table[04104] = INTERPRET_MOVEA_W_D4_to_A4;
    table[04105] = INTERPRET_MOVEA_W_D5_to_A4;
    table[04106] = INTERPRET_MOVEA_W_D6_to_A4;
    table[04107] = INTERPRET_MOVEA_W_D7_to_A4;
    table[05100] = INTERPRET_MOVEA_W_D0_to_A5;
    table[05101] = INTERPRET_MOVEA_W_D1_to_A5;
    table[05102] = INTERPRET_MOVEA_W_D2_to_A5;
    table[05103] = INTERPRET_MOVEA_W_D3_to_A5;
    table[05104] = INTERPRET_MOVEA_W_D4_to_A5;
    table[05105] = INTERPRET_MOVEA_W_D5_to_A5;
    table[05106] = INTERPRET_MOVEA_W_D6_to_A5;
    table[05107] = INTERPRET_MOVEA_W_D7_to_A5;
    table[06100] = INTERPRET_MOVEA_W_D0_to_A6;
    table[06101] = INTERPRET_MOVEA_W_D1_to_A6;
    table[06102] = INTERPRET_MOVEA_W_D2_to_A6;
    table[06103] = INTERPRET_MOVEA_W_D3_to_A6;
    table[06104] = INTERPRET_MOVEA_W_D4_to_A6;
    table[06105] = INTERPRET_MOVEA_W_D5_to_A6;
    table[06106] = INTERPRET_MOVEA_W_D6_to_A6;
    table[06107] = INTERPRET_MOVEA_W_D7_to_A6;
    table[07100] = INTERPRET_MOVEA_W_D0_to_A7;
    table[07101] = INTERPRET_MOVEA_W_D1_to_A7;
    table[07102] = INTERPRET_MOVEA_W_D2_to_A7;
    table[07103] = INTERPRET_MOVEA_W_D3_to_A7;
    table[07104] = INTERPRET_MOVEA_W_D4_to_A7;
    table[07105] = INTERPRET_MOVEA_W_D5_to_A7;
    table[07106] = INTERPRET_MOVEA_W_D6_to_A7;
    table[07107] = INTERPRET_MOVEA_W_D7_to_A7;

    table[00700] = INTERPRET_MOVE_W_D0_to_ABS_W;
    table[00701] = INTERPRET_MOVE_W_D1_to_ABS_W;
    table[00702] = INTERPRET_MOVE_W_D2_to_ABS_W;
    table[00703] = INTERPRET_MOVE_W_D3_to_ABS_W;
    table[00704] = INTERPRET_MOVE_W_D4_to_ABS_W;
    table[00705] = INTERPRET_MOVE_W_D5_to_ABS_W;
    table[00706] = INTERPRET_MOVE_W_D6_to_ABS_W;
    table[00707] = INTERPRET_MOVE_W_D7_to_ABS_W;

    table[01700] = INTERPRET_MOVE_W_D0_to_ABS_L;
    table[01701] = INTERPRET_MOVE_W_D1_to_ABS_L;
    table[01702] = INTERPRET_MOVE_W_D2_to_ABS_L;
    table[01703] = INTERPRET_MOVE_W_D3_to_ABS_L;
    table[01704] = INTERPRET_MOVE_W_D4_to_ABS_L;
    table[01705] = INTERPRET_MOVE_W_D5_to_ABS_L;
    table[01706] = INTERPRET_MOVE_W_D6_to_ABS_L;
    table[01707] = INTERPRET_MOVE_W_D7_to_ABS_L;

    table[00710] = INTERPRET_MOVE_W_A0_to_ABS_W;
    table[00711] = INTERPRET_MOVE_W_A1_to_ABS_W;
    table[00712] = INTERPRET_MOVE_W_A2_to_ABS_W;
    table[00713] = INTERPRET_MOVE_W_A3_to_ABS_W;
    table[00714] = INTERPRET_MOVE_W_A4_to_ABS_W;
    table[00715] = INTERPRET_MOVE_W_A5_to_ABS_W;
    table[00716] = INTERPRET_MOVE_W_A6_to_ABS_W;
    table[00717] = INTERPRET_MOVE_W_A7_to_ABS_W;

    table[01710] = INTERPRET_MOVE_W_A0_to_ABS_L;
    table[01711] = INTERPRET_MOVE_W_A1_to_ABS_L;
    table[01712] = INTERPRET_MOVE_W_A2_to_ABS_L;
    table[01713] = INTERPRET_MOVE_W_A3_to_ABS_L;
    table[01714] = INTERPRET_MOVE_W_A4_to_ABS_L;
    table[01715] = INTERPRET_MOVE_W_A5_to_ABS_L;
    table[01716] = INTERPRET_MOVE_W_A6_to_ABS_L;
    table[01717] = INTERPRET_MOVE_W_A7_to_ABS_L;

    table[00074] = INTERPRET_MOVE_W_IMMED_to_D0;
    table[01074] = INTERPRET_MOVE_W_IMMED_to_D1;
    table[02074] = INTERPRET_MOVE_W_IMMED_to_D2;
    table[03074] = INTERPRET_MOVE_W_IMMED_to_D3;
    table[04074] = INTERPRET_MOVE_W_IMMED_to_D4;
    table[05074] = INTERPRET_MOVE_W_IMMED_to_D5;
    table[06074] = INTERPRET_MOVE_W_IMMED_to_D6;
    table[07074] = INTERPRET_MOVE_W_IMMED_to_D7;

    table[00174] = INTERPRET_MOVEA_W_IMMED_to_A0;
    table[01174] = INTERPRET_MOVEA_W_IMMED_to_A1;
    table[02174] = INTERPRET_MOVEA_W_IMMED_to_A2;
    table[03174] = INTERPRET_MOVEA_W_IMMED_to_A3;
    table[04174] = INTERPRET_MOVEA_W_IMMED_to_A4;
    table[05174] = INTERPRET_MOVEA_W_IMMED_to_A5;
    table[06174] = INTERPRET_MOVEA_W_IMMED_to_A6;
    table[07174] = INTERPRET_MOVEA_W_IMMED_to_A7;

    table[00274] = INTERPRET_MOVE_W_IMMED_to_A0_Addr;
    table[01274] = INTERPRET_MOVE_W_IMMED_to_A1_Addr;
    table[02274] = INTERPRET_MOVE_W_IMMED_to_A2_Addr;
    table[03274] = INTERPRET_MOVE_W_IMMED_to_A3_Addr;
    table[04274] = INTERPRET_MOVE_W_IMMED_to_A4_Addr;
    table[05274] = INTERPRET_MOVE_W_IMMED_to_A5_Addr;
    table[06274] = INTERPRET_MOVE_W_IMMED_to_A6_Addr;
    table[07274] = INTERPRET_MOVE_W_IMMED_to_A7_Addr;

    table[00374] = INTERPRET_MOVE_W_IMMED_to_A0_PostInc;
    table[01374] = INTERPRET_MOVE_W_IMMED_to_A1_PostInc;
    table[02374] = INTERPRET_MOVE_W_IMMED_to_A2_PostInc;
    table[03374] = INTERPRET_MOVE_W_IMMED_to_A3_PostInc;
    table[04374] = INTERPRET_MOVE_W_IMMED_to_A4_PostInc;
    table[05374] = INTERPRET_MOVE_W_IMMED_to_A5_PostInc;
    table[06374] = INTERPRET_MOVE_W_IMMED_to_A6_PostInc;
    table[07374] = INTERPRET_MOVE_W_IMMED_to_A7_PostInc;

    table[00474] = INTERPRET_MOVE_W_IMMED_to_A0_PreDec;
    table[01474] = INTERPRET_MOVE_W_IMMED_to_A1_PreDec;
    table[02474] = INTERPRET_MOVE_W_IMMED_to_A2_PreDec;
    table[03474] = INTERPRET_MOVE_W_IMMED_to_A3_PreDec;
    table[04474] = INTERPRET_MOVE_W_IMMED_to_A4_PreDec;
    table[05474] = INTERPRET_MOVE_W_IMMED_to_A5_PreDec;
    table[06474] = INTERPRET_MOVE_W_IMMED_to_A6_PreDec;
    table[07474] = INTERPRET_MOVE_W_IMMED_to_A7_PreDec;

    return table;
}


static constexpr std::array<INTERPRET_Function, 4096> BuildInsnTable_Byte()
{
    std::array<INTERPRET_Function, 4096> table{};

    auto fill = [&table](int first, int last, INTERPRET_Function func) {
        for (int i = first; i <= last; ++i)
            table[i] = func;
    };

    /* Make all entries unimplemented first */
    fill(00000, 07777, INTERPRET_UNIMPLEMENTED);

    /*
        Fill all allowed defaults with generic version:
        modes 0 to 6 allow all register combinations
        mode 7 allows only register 0 and 1

        Register and mode are decoded as reg:mod - the loop can therefore 
        start at octal 000 (mode 0, register 0) and iterate up to octal 017
        (mode 7, register 1). Source decoded as mod:reg can go always from 
        000 (mode 0, register 0) up to 074 (mode 7, register 4)
    */
    for (int dst_rm = 0; dst_rm <= 071; dst_rm++) {
        int dst_mode = dst_rm >> 3;
        int dst_reg  = dst_rm & 7;
        int base = (dst_reg << 9) | (dst_mode << 6);
        
        /* Skip BYTE to An modes as they do not exist */
        if (dst_mode == 1) continue;

        /* Skip BYTE from An modes as they do not exist */
        fill(base + 00000, base + 00007, INTERPRET_MOVE_B_Generic);
        fill(base + 00020, base + 00074, INTERPRET_MOVE_B_Generic);
    }

    table[00000] = INTERPRET_MOVE_B_D0_to_D0;
    table[00001] = INTERPRET_MOVE_B_D1_to_D0;
    table[00002] = INTERPRET_MOVE_B_D2_to_D0;
    table[00003] = INTERPRET_MOVE_B_D3_to_D0;
    table[00004] = INTERPRET_MOVE_B_D4_to_D0;
    table[00005] = INTERPRET_MOVE_B_D5_to_D0;
    table[00006] = INTERPRET_MOVE_B_D6_to_D0;
    table[00007] = INTERPRET_MOVE_B_D7_to_D0;
    table[01000] = INTERPRET_MOVE_B_D0_to_D1;
    table[01001] = INTERPRET_MOVE_B_D1_to_D1;
    table[01002] = INTERPRET_MOVE_B_D2_to_D1;
    table[01003] = INTERPRET_MOVE_B_D3_to_D1;
    table[01004] = INTERPRET_MOVE_B_D4_to_D1;
    table[01005] = INTERPRET_MOVE_B_D5_to_D1;
    table[01006] = INTERPRET_MOVE_B_D6_to_D1;
    table[01007] = INTERPRET_MOVE_B_D7_to_D1;
    table[02000] = INTERPRET_MOVE_B_D0_to_D2;
    table[02001] = INTERPRET_MOVE_B_D1_to_D2;
    table[02002] = INTERPRET_MOVE_B_D2_to_D2;
    table[02003] = INTERPRET_MOVE_B_D3_to_D2;
    table[02004] = INTERPRET_MOVE_B_D4_to_D2;
    table[02005] = INTERPRET_MOVE_B_D5_to_D2;
    table[02006] = INTERPRET_MOVE_B_D6_to_D2;
    table[02007] = INTERPRET_MOVE_B_D7_to_D2;
    table[03000] = INTERPRET_MOVE_B_D0_to_D3;
    table[03001] = INTERPRET_MOVE_B_D1_to_D3;
    table[03002] = INTERPRET_MOVE_B_D2_to_D3;
    table[03003] = INTERPRET_MOVE_B_D3_to_D3;
    table[03004] = INTERPRET_MOVE_B_D4_to_D3;
    table[03005] = INTERPRET_MOVE_B_D5_to_D3;
    table[03006] = INTERPRET_MOVE_B_D6_to_D3;
    table[03007] = INTERPRET_MOVE_B_D7_to_D3;
    table[04000] = INTERPRET_MOVE_B_D0_to_D4;
    table[04001] = INTERPRET_MOVE_B_D1_to_D4;
    table[04002] = INTERPRET_MOVE_B_D2_to_D4;
    table[04003] = INTERPRET_MOVE_B_D3_to_D4;
    table[04004] = INTERPRET_MOVE_B_D4_to_D4;
    table[04005] = INTERPRET_MOVE_B_D5_to_D4;
    table[04006] = INTERPRET_MOVE_B_D6_to_D4;
    table[04007] = INTERPRET_MOVE_B_D7_to_D4;
    table[05000] = INTERPRET_MOVE_B_D0_to_D5;
    table[05001] = INTERPRET_MOVE_B_D1_to_D5;
    table[05002] = INTERPRET_MOVE_B_D2_to_D5;
    table[05003] = INTERPRET_MOVE_B_D3_to_D5;
    table[05004] = INTERPRET_MOVE_B_D4_to_D5;
    table[05005] = INTERPRET_MOVE_B_D5_to_D5;
    table[05006] = INTERPRET_MOVE_B_D6_to_D5;
    table[05007] = INTERPRET_MOVE_B_D7_to_D5;
    table[06000] = INTERPRET_MOVE_B_D0_to_D6;
    table[06001] = INTERPRET_MOVE_B_D1_to_D6;
    table[06002] = INTERPRET_MOVE_B_D2_to_D6;
    table[06003] = INTERPRET_MOVE_B_D3_to_D6;
    table[06004] = INTERPRET_MOVE_B_D4_to_D6;
    table[06005] = INTERPRET_MOVE_B_D5_to_D6;
    table[06006] = INTERPRET_MOVE_B_D6_to_D6;
    table[06007] = INTERPRET_MOVE_B_D7_to_D6;
    table[07000] = INTERPRET_MOVE_B_D0_to_D7;
    table[07001] = INTERPRET_MOVE_B_D1_to_D7;
    table[07002] = INTERPRET_MOVE_B_D2_to_D7;
    table[07003] = INTERPRET_MOVE_B_D3_to_D7;
    table[07004] = INTERPRET_MOVE_B_D4_to_D7;
    table[07005] = INTERPRET_MOVE_B_D5_to_D7;
    table[07006] = INTERPRET_MOVE_B_D6_to_D7;
    table[07007] = INTERPRET_MOVE_B_D7_to_D7;

    table[00700] = INTERPRET_MOVE_B_D0_to_ABS_W;
    table[00701] = INTERPRET_MOVE_B_D1_to_ABS_W;
    table[00702] = INTERPRET_MOVE_B_D2_to_ABS_W;
    table[00703] = INTERPRET_MOVE_B_D3_to_ABS_W;
    table[00704] = INTERPRET_MOVE_B_D4_to_ABS_W;
    table[00705] = INTERPRET_MOVE_B_D5_to_ABS_W;
    table[00706] = INTERPRET_MOVE_B_D6_to_ABS_W;
    table[00707] = INTERPRET_MOVE_B_D7_to_ABS_W;

    table[01700] = INTERPRET_MOVE_B_D0_to_ABS_L;
    table[01701] = INTERPRET_MOVE_B_D1_to_ABS_L;
    table[01702] = INTERPRET_MOVE_B_D2_to_ABS_L;
    table[01703] = INTERPRET_MOVE_B_D3_to_ABS_L;
    table[01704] = INTERPRET_MOVE_B_D4_to_ABS_L;
    table[01705] = INTERPRET_MOVE_B_D5_to_ABS_L;
    table[01706] = INTERPRET_MOVE_B_D6_to_ABS_L;
    table[01707] = INTERPRET_MOVE_B_D7_to_ABS_L;

    table[00074] = INTERPRET_MOVE_B_IMMED_to_D0;
    table[01074] = INTERPRET_MOVE_B_IMMED_to_D1;
    table[02074] = INTERPRET_MOVE_B_IMMED_to_D2;
    table[03074] = INTERPRET_MOVE_B_IMMED_to_D3;
    table[04074] = INTERPRET_MOVE_B_IMMED_to_D4;
    table[05074] = INTERPRET_MOVE_B_IMMED_to_D5;
    table[06074] = INTERPRET_MOVE_B_IMMED_to_D6;
    table[07074] = INTERPRET_MOVE_B_IMMED_to_D7;

    table[00274] = INTERPRET_MOVE_B_IMMED_to_A0_Addr;
    table[01274] = INTERPRET_MOVE_B_IMMED_to_A1_Addr;
    table[02274] = INTERPRET_MOVE_B_IMMED_to_A2_Addr;
    table[03274] = INTERPRET_MOVE_B_IMMED_to_A3_Addr;
    table[04274] = INTERPRET_MOVE_B_IMMED_to_A4_Addr;
    table[05274] = INTERPRET_MOVE_B_IMMED_to_A5_Addr;
    table[06274] = INTERPRET_MOVE_B_IMMED_to_A6_Addr;
    table[07274] = INTERPRET_MOVE_B_IMMED_to_A7_Addr;

    table[00374] = INTERPRET_MOVE_B_IMMED_to_A0_PostInc;
    table[01374] = INTERPRET_MOVE_B_IMMED_to_A1_PostInc;
    table[02374] = INTERPRET_MOVE_B_IMMED_to_A2_PostInc;
    table[03374] = INTERPRET_MOVE_B_IMMED_to_A3_PostInc;
    table[04374] = INTERPRET_MOVE_B_IMMED_to_A4_PostInc;
    table[05374] = INTERPRET_MOVE_B_IMMED_to_A5_PostInc;
    table[06374] = INTERPRET_MOVE_B_IMMED_to_A6_PostInc;
    table[07374] = INTERPRET_MOVE_B_IMMED_to_A7_PostInc;

    table[00474] = INTERPRET_MOVE_B_IMMED_to_A0_PreDec;
    table[01474] = INTERPRET_MOVE_B_IMMED_to_A1_PreDec;
    table[02474] = INTERPRET_MOVE_B_IMMED_to_A2_PreDec;
    table[03474] = INTERPRET_MOVE_B_IMMED_to_A3_PreDec;
    table[04474] = INTERPRET_MOVE_B_IMMED_to_A4_PreDec;
    table[05474] = INTERPRET_MOVE_B_IMMED_to_A5_PreDec;
    table[06474] = INTERPRET_MOVE_B_IMMED_to_A6_PreDec;
    table[07474] = INTERPRET_MOVE_B_IMMED_to_A7_PreDec;

    return table;
}

static constexpr auto InsnTable_L = BuildInsnTable_Long();
static constexpr auto InsnTable_W = BuildInsnTable_Word();
static constexpr auto InsnTable_B = BuildInsnTable_Byte();

__attribute__((optimize("no-optimize-sibling-calls")))
void INTERPRET_line1(uint32_t opcode)
{
    InsnTable_B[opcode & 0xfff](opcode);
}

__attribute__((optimize("no-optimize-sibling-calls")))
void INTERPRET_line2(uint32_t opcode)
{
    InsnTable_L[opcode & 0xfff](opcode);
}

__attribute__((optimize("no-optimize-sibling-calls")))
void INTERPRET_line3(uint32_t opcode)
{
    InsnTable_W[opcode & 0xfff](opcode);
}