/*
    Copyright © 2019 Michal Schulz <michal.schulz@gmx.de>
    https://github.com/michalsc

    This Source Code Form is subject to the terms of the
    Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed
    with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#ifndef _EMU64_M68KSTATE_H
#define _EMU64_M68KSTATE_H

#include <stdint.h>

struct M68KState
{
    /* Integer part */

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
    } A[8];
    union {
        uint16_t u16[2];
        int16_t s16[2];
        uint32_t u32;
        int32_t s32;
    } USP, MSP, ISP;

    uint32_t PC;
    uint16_t SR;

    /* FPU Part */
    uint32_t FPSR;
    uint32_t FPIAR;
    uint16_t FPCR;
    double FP[8];   // Double precision! Extended is "emulated" in load/store only
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

#define SRB_C    0
#define SRB_V    1
#define SRB_Z    2
#define SRB_N    3
#define SRB_X    4
#define SRB_IPL  8
#define SRB_M    12
#define SRB_S    13
#define SRB_T0   14
#define SRB_T1   15

#define FPSR_N      0x08000000
#define FPSR_Z      0x04000000
#define FPSR_I      0x02000000
#define FPSR_NAN    0x01000000
#define FPSRB_N     27
#define FPSRB_Z     26
#define FPSRB_I     25
#define FPSRB_NAN   24

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

#define F_CC_EQ     0x01
#define F_CC_NE     0x0e
#define F_CC_GT     0x12
#define F_CC_NGT    0x1d
#define F_CC_GE     0x13
#define F_CC_NGE    0x1c
#define F_CC_LT     0x14
#define F_CC_NLT    0x1b
#define F_CC_LE     0x15
#define F_CC_NLE    0x1a
#define F_CC_GL     0x16
#define F_CC_NGL    0x19
#define F_CC_GLE    0x17
#define F_CC_NGLE   0x18
#define F_CC_OGT    0x02
#define F_CC_ULE    0x0d
#define F_CC_OGE    0x03
#define F_CC_ULT    0x0c
#define F_CC_OLT    0x04
#define F_CC_UGE    0x0b
#define F_CC_OLE    0x05
#define F_CC_UGT    0x0a
#define F_CC_OGL    0x06
#define F_CC_UEQ    0x09
#define F_CC_OR     0x07
#define F_CC_UN     0x08
#define F_CC_F      0x00
#define F_CC_T      0x0f
#define F_CC_SF     0x10
#define F_CC_ST     0x1f
#define F_CC_SEQ    0x11
#define F_CC_SNE    0x1e

#endif /* _EMU64_M68KSTATE_H */
