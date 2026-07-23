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

void INTERPRET_ORI_to_CCR(uint32_t)
{
    uint16_t immed = *(uint8_t *)(uintptr_t)(PC + 3) & SR_CCR;
    SR = SR | immed;
    PC += 4;
}

void INTERPRET_ORI_to_SR(uint32_t)
{
    uint16_t immed = *(uint16_t *)(uintptr_t)(PC + 2) & SR_ALL;
    uint16_t sr = SR;
    uint16_t changed = sr;
    
    /* Modifying SR requires supervisor rights */
    if (sr & SR_S) {
        sr |= immed;
        changed ^= sr;

        /* Check if M flag has changed its value */
        if (changed & SR_M) {
            /* M is set, store A7 to ISP and move MSP to A7, changing M to 0 cannot happen in ORI */
            if (sr & SR_M) {
                setISP(A7);
                A7 = getMSP();
            }
        }

        /* Check if IPL was altered */
        if (changed & SR_IPL) {
            /* IPL higher than 6? Disable ARM interrupts, otherwise enable them */
            if ((sr & SR_IPL) > 0x0600) {
                asm volatile("msr DAIFSet, 7":::"memory");
            } else {
                asm volatile("msr DAIFClr, 7":::"memory");
            }
        }
        SR = sr;
        PC += 4;
    } else {
        INTERPRET_Exception_F0(VECTOR_PRIVILEGE_VIOLATION);
    }
}

void INTERPRET_ANDI_to_CCR(uint32_t)
{
	uint16_t immed = *(uint8_t *)(uintptr_t)(PC + 3) & SR_CCR;
    SR = SR & immed;
	PC += 4;
}

void INTERPRET_ANDI_to_SR(uint32_t)
{
    uint16_t immed = *(uint16_t *)(uintptr_t)(PC + 2) & SR_ALL;
    uint16_t sr = SR;
    uint16_t changed = sr;
    
    /* Modifying SR requires supervisor rights */
    if (sr & SR_S) {
        sr &= immed;
        changed ^= sr;

        /* Check if M flag has changed its value */
        if (changed & SR_M) {
            /* M is set, store A7 to ISP and move MSP to A7 */
            if (sr & SR_M) {
                setISP(A7);
                A7 = getMSP();
            } else {
                /* This **CANNOT** happen */
                setMSP(A7);
                A7 = getISP();
            }
        }

        /*
            Check if S was cleared. If this is the case, move A7 to ISP or MSP and
            load USP into A7
        */
        if (changed & SR_S) {
            if ((sr & SR_S) == 0) {
                if (sr & SR_M) {
                    setMSP(A7);
                } else {
                    setISP(A7);
                }
                A7 = getUSP();
            }
        }

        /* Check if IPL was altered */
        if (changed & SR_IPL) {
            /* IPL higher than 6? Disable ARM interrupts, otherwise enable them */
            if ((sr & SR_IPL) > 0x0600) {
                asm volatile("msr DAIFSet, 7":::"memory");
            } else {
                asm volatile("msr DAIFClr, 7":::"memory");
            }
        }
        SR = sr;
        PC += 4;
    } else {
        INTERPRET_Exception_F0(VECTOR_PRIVILEGE_VIOLATION);
    }
}

void INTERPRET_EORI_to_CCR(uint32_t)
{
	uint16_t immed = *(uint8_t *)(uintptr_t)(PC + 3) & SR_CCR;
    SR = SR ^ immed;
	PC += 4;
}

void INTERPRET_EORI_to_SR(uint32_t)
{
    uint16_t immed = *(uint16_t *)(uintptr_t)(PC + 2) & SR_ALL;
    uint16_t sr = SR;
    uint16_t changed = sr;
    
    /* Modifying SR requires supervisor rights */
    if (sr & SR_S) {
        sr ^= immed;
        changed ^= sr;

        /* Check if M flag has changed its value */
        if (changed & SR_M) {
            /* M is set, store A7 to ISP and move MSP to A7 */
            if (sr & SR_M) {
                setISP(A7);
                A7 = getMSP();
            } else {
                /* This **CANNOT** happen */
                setMSP(A7);
                A7 = getISP();
            }
        }

        /*
            Check if S was cleared. If this is the case, move A7 to ISP or MSP and
            load USP into A7
        */
        if (changed & SR_S) {
            if ((sr & SR_S) == 0) {
                if (sr & SR_M) {
                    setMSP(A7);
                } else {
                    setISP(A7);
                }
                A7 = getUSP();
            }
        }

        /* Check if IPL was altered */
        if (changed & SR_IPL) {
            /* IPL higher than 6? Disable ARM interrupts, otherwise enable them */
            if ((sr & SR_IPL) > 0x0600) {
                asm volatile("msr DAIFSet, 7":::"memory");
            } else {
                asm volatile("msr DAIFClr, 7":::"memory");
            }
        }
        SR = sr;
        PC += 4;
    } else {
        INTERPRET_Exception_F0(VECTOR_PRIVILEGE_VIOLATION);
    }
}

static constexpr std::array<INTERPRET_Function, 4096> BuildInsnTable()
{
    std::array<INTERPRET_Function, 4096> table{};

    auto fill = [&table](int first, int last, INTERPRET_Function func) {
        for (int i = first; i <= last; ++i)
            table[i] = func;
    };

    fill(00000, 07777, INTERPRET_UNIMPLEMENTED);
    
    table[00074]      = INTERPRET_ORI_to_CCR;
    table[01074]      = INTERPRET_ANDI_to_CCR;
    table[05074]      = INTERPRET_EORI_to_CCR;

    table[00174]      = INTERPRET_ORI_to_SR;
    table[01174]      = INTERPRET_ANDI_to_SR;
    table[05174]      = INTERPRET_ORI_to_SR;

    #if 0
	[00000 ... 00007] = { EMIT_ORI, NULL, 0, SR_NZVC, 2, 0, 1 },
	[00020 ... 00047] = { EMIT_ORI, NULL, 0, SR_NZVC, 2, 0, 1 },
	[00050 ... 00071] = { EMIT_ORI, NULL, 0, SR_NZVC, 2, 1, 1 },
	[00100 ... 00107] = { EMIT_ORI, NULL, 0, SR_NZVC, 2, 0, 2 },
	[00120 ... 00147] = { EMIT_ORI, NULL, 0, SR_NZVC, 2, 0, 2 },
	[00150 ... 00171] = { EMIT_ORI, NULL, 0, SR_NZVC, 2, 1, 2 },
	[00200 ... 00207] = { EMIT_ORI, NULL, 0, SR_NZVC, 3, 0, 4 },
	[00220 ... 00247] = { EMIT_ORI, NULL, 0, SR_NZVC, 3, 0, 4 },
	[00250 ... 00271] = { EMIT_ORI, NULL, 0, SR_NZVC, 3, 1, 4 },

	[01000 ... 01007] = { EMIT_ANDI, NULL, 0, SR_NZVC, 2, 0, 1 },
	[01020 ... 01047] = { EMIT_ANDI, NULL, 0, SR_NZVC, 2, 0, 1 },
	[01050 ... 01071] = { EMIT_ANDI, NULL, 0, SR_NZVC, 2, 1, 1 },
	[01100 ... 01107] = { EMIT_ANDI, NULL, 0, SR_NZVC, 2, 0, 2 },
	[01120 ... 01147] = { EMIT_ANDI, NULL, 0, SR_NZVC, 2, 0, 2 },
	[01150 ... 01171] = { EMIT_ANDI, NULL, 0, SR_NZVC, 2, 1, 2 },
	[01200 ... 01207] = { EMIT_ANDI, NULL, 0, SR_NZVC, 3, 0, 4 },
	[01220 ... 01247] = { EMIT_ANDI, NULL, 0, SR_NZVC, 3, 0, 4 },
	[01250 ... 01271] = { EMIT_ANDI, NULL, 0, SR_NZVC, 3, 1, 4 },

	[02000 ... 02007] = { EMIT_SUBI, NULL, 0, SR_CCR, 2, 0, 1 },
	[02020 ... 02047] = { EMIT_SUBI, NULL, 0, SR_CCR, 2, 0, 1 },
	[02050 ... 02071] = { EMIT_SUBI, NULL, 0, SR_CCR, 2, 1, 1 },
	[02100 ... 02107] = { EMIT_SUBI, NULL, 0, SR_CCR, 2, 0, 2 },
	[02120 ... 02147] = { EMIT_SUBI, NULL, 0, SR_CCR, 2, 0, 2 },
	[02150 ... 02171] = { EMIT_SUBI, NULL, 0, SR_CCR, 2, 1, 2 },
	[02200 ... 02207] = { EMIT_SUBI, NULL, 0, SR_CCR, 3, 0, 4 },
	[02220 ... 02247] = { EMIT_SUBI, NULL, 0, SR_CCR, 3, 0, 4 },
	[02250 ... 02271] = { EMIT_SUBI, NULL, 0, SR_CCR, 3, 1, 4 },

	[03000 ... 03007] = { EMIT_ADDI, NULL, 0, SR_CCR, 2, 0, 1 },
	[03020 ... 03047] = { EMIT_ADDI, NULL, 0, SR_CCR, 2, 0, 1 },
	[03050 ... 03071] = { EMIT_ADDI, NULL, 0, SR_CCR, 2, 1, 1 },
	[03100 ... 03107] = { EMIT_ADDI, NULL, 0, SR_CCR, 2, 0, 2 },
	[03120 ... 03147] = { EMIT_ADDI, NULL, 0, SR_CCR, 2, 0, 2 },
	[03150 ... 03171] = { EMIT_ADDI, NULL, 0, SR_CCR, 2, 1, 2 },
	[03200 ... 03207] = { EMIT_ADDI, NULL, 0, SR_CCR, 3, 0, 4 },
	[03220 ... 03247] = { EMIT_ADDI, NULL, 0, SR_CCR, 3, 0, 4 },
	[03250 ... 03271] = { EMIT_ADDI, NULL, 0, SR_CCR, 3, 1, 4 },

	[04000 ... 04007] = { EMIT_BTST, NULL, 0, SR_Z, 2, 0, 4 },
	[04020 ... 04047] = { EMIT_BTST, NULL, 0, SR_Z, 2, 0, 1 },
	[04050 ... 04073] = { EMIT_BTST, NULL, 0, SR_Z, 2, 1, 1 },
	[04100 ... 04107] = { EMIT_BCHG, NULL, 0, SR_Z, 2, 0, 4 },
	[04120 ... 04147] = { EMIT_BCHG, NULL, 0, SR_Z, 2, 1, 1 },
	[04150 ... 04171] = { EMIT_BCHG, NULL, 0, SR_Z, 2, 1, 1 },
	[04200 ... 04207] = { EMIT_BCLR, NULL, 0, SR_Z, 2, 0, 4 },
	[04220 ... 04247] = { EMIT_BCLR, NULL, 0, SR_Z, 2, 0, 1 },
	[04250 ... 04271] = { EMIT_BCLR, NULL, 0, SR_Z, 2, 1, 1 },
	[04300 ... 04307] = { EMIT_BSET, NULL, 0, SR_Z, 2, 0, 4 },
	[04320 ... 04347] = { EMIT_BSET, NULL, 0, SR_Z, 2, 0, 1 },
	[04350 ... 04371] = { EMIT_BSET, NULL, 0, SR_Z, 2, 1, 1 },

	[05000 ... 05007] = { EMIT_EORI, NULL, 0, SR_NZVC, 2, 0, 1 },
	[05020 ... 05047] = { EMIT_EORI, NULL, 0, SR_NZVC, 2, 0, 1 },
	[05050 ... 05071] = { EMIT_EORI, NULL, 0, SR_NZVC, 2, 1, 1 },
	[05100 ... 05107] = { EMIT_EORI, NULL, 0, SR_NZVC, 2, 0, 2 },
	[05120 ... 05147] = { EMIT_EORI, NULL, 0, SR_NZVC, 2, 0, 2 },
	[05150 ... 05171] = { EMIT_EORI, NULL, 0, SR_NZVC, 2, 1, 2 },
	[05200 ... 05207] = { EMIT_EORI, NULL, 0, SR_NZVC, 3, 0, 4 },
	[05220 ... 05247] = { EMIT_EORI, NULL, 0, SR_NZVC, 3, 0, 4 },
	[05250 ... 05271] = { EMIT_EORI, NULL, 0, SR_NZVC, 3, 1, 4 },

	[06000 ... 06007] = { EMIT_CMPI, NULL, 0, SR_NZVC, 2, 0, 1 },
	[06020 ... 06047] = { EMIT_CMPI, NULL, 0, SR_NZVC, 2, 0, 1 },
	[06050 ... 06073] = { EMIT_CMPI, NULL, 0, SR_NZVC, 2, 1, 1 },
	[06100 ... 06107] = { EMIT_CMPI, NULL, 0, SR_NZVC, 2, 0, 2 },
	[06120 ... 06147] = { EMIT_CMPI, NULL, 0, SR_NZVC, 2, 0, 2 },
	[06150 ... 06173] = { EMIT_CMPI, NULL, 0, SR_NZVC, 2, 1, 2 },
	[06200 ... 06207] = { EMIT_CMPI, NULL, 0, SR_NZVC, 3, 0, 4 },
	[06220 ... 06247] = { EMIT_CMPI, NULL, 0, SR_NZVC, 3, 0, 4 },
	[06250 ... 06273] = { EMIT_CMPI, NULL, 0, SR_NZVC, 3, 1, 4 },

	[00400 ... 00407] = { EMIT_BTST, NULL, 0, SR_Z, 1, 0, 4 },
	[00420 ... 00447] = { EMIT_BTST, NULL, 0, SR_Z, 1, 0, 1 },
	[00450 ... 00474] = { EMIT_BTST, NULL, 0, SR_Z, 1, 1, 1 },
	[01400 ... 01407] = { EMIT_BTST, NULL, 0, SR_Z, 1, 0, 4 },
	[01420 ... 01447] = { EMIT_BTST, NULL, 0, SR_Z, 1, 0, 1 },
	[01450 ... 01474] = { EMIT_BTST, NULL, 0, SR_Z, 1, 1, 1 },
	[02400 ... 02407] = { EMIT_BTST, NULL, 0, SR_Z, 1, 0, 4 },
	[02420 ... 02447] = { EMIT_BTST, NULL, 0, SR_Z, 1, 0, 1 },
	[02450 ... 02474] = { EMIT_BTST, NULL, 0, SR_Z, 1, 1, 1 },
	[03400 ... 03407] = { EMIT_BTST, NULL, 0, SR_Z, 1, 0, 4 },
	[03420 ... 03447] = { EMIT_BTST, NULL, 0, SR_Z, 1, 0, 1 },
	[03450 ... 03474] = { EMIT_BTST, NULL, 0, SR_Z, 1, 1, 1 },
	[04400 ... 04407] = { EMIT_BTST, NULL, 0, SR_Z, 1, 0, 4 },
	[04420 ... 04447] = { EMIT_BTST, NULL, 0, SR_Z, 1, 0, 1 },
	[04450 ... 04474] = { EMIT_BTST, NULL, 0, SR_Z, 1, 1, 1 },
	[05400 ... 05407] = { EMIT_BTST, NULL, 0, SR_Z, 1, 0, 4 },
	[05420 ... 05447] = { EMIT_BTST, NULL, 0, SR_Z, 1, 1, 1 },
	[05450 ... 05474] = { EMIT_BTST, NULL, 0, SR_Z, 1, 1, 1 },
	[06400 ... 06407] = { EMIT_BTST, NULL, 0, SR_Z, 1, 0, 4 },
	[06420 ... 06447] = { EMIT_BTST, NULL, 0, SR_Z, 1, 0, 1 },
	[06450 ... 06474] = { EMIT_BTST, NULL, 0, SR_Z, 1, 1, 1 },
	[07400 ... 07407] = { EMIT_BTST, NULL, 0, SR_Z, 1, 0, 4 },
	[07420 ... 07447] = { EMIT_BTST, NULL, 0, SR_Z, 1, 0, 1 },
	[07450 ... 07474] = { EMIT_BTST, NULL, 0, SR_Z, 1, 1, 1 },

	[00500 ... 00507] = { EMIT_BCHG, NULL, 0, SR_Z, 1, 0, 4 },
	[00520 ... 00547] = { EMIT_BCHG, NULL, 0, SR_Z, 1, 0, 1 },
	[00550 ... 00571] = { EMIT_BCHG, NULL, 0, SR_Z, 1, 1, 1 },
	[01500 ... 01507] = { EMIT_BCHG, NULL, 0, SR_Z, 1, 0, 4 },
	[01520 ... 01547] = { EMIT_BCHG, NULL, 0, SR_Z, 1, 0, 1 },
	[01550 ... 01571] = { EMIT_BCHG, NULL, 0, SR_Z, 1, 1, 1 },
	[02500 ... 02507] = { EMIT_BCHG, NULL, 0, SR_Z, 1, 0, 4 },
	[02520 ... 02547] = { EMIT_BCHG, NULL, 0, SR_Z, 1, 0, 1 },
	[02550 ... 02571] = { EMIT_BCHG, NULL, 0, SR_Z, 1, 1, 1 },
	[03500 ... 03507] = { EMIT_BCHG, NULL, 0, SR_Z, 1, 0, 4 },
	[03520 ... 03547] = { EMIT_BCHG, NULL, 0, SR_Z, 1, 0, 1 },
	[03550 ... 03571] = { EMIT_BCHG, NULL, 0, SR_Z, 1, 1, 1 },
	[04500 ... 04507] = { EMIT_BCHG, NULL, 0, SR_Z, 1, 0, 4 },
	[04520 ... 04547] = { EMIT_BCHG, NULL, 0, SR_Z, 1, 0, 1 },
	[04550 ... 04571] = { EMIT_BCHG, NULL, 0, SR_Z, 1, 1, 1 },
	[05500 ... 05507] = { EMIT_BCHG, NULL, 0, SR_Z, 1, 0, 4 },
	[05520 ... 05547] = { EMIT_BCHG, NULL, 0, SR_Z, 1, 0, 1 },
	[05550 ... 05571] = { EMIT_BCHG, NULL, 0, SR_Z, 1, 1, 1 },
	[06500 ... 06507] = { EMIT_BCHG, NULL, 0, SR_Z, 1, 0, 4 },
	[06520 ... 06547] = { EMIT_BCHG, NULL, 0, SR_Z, 1, 0, 1 },
	[06550 ... 06571] = { EMIT_BCHG, NULL, 0, SR_Z, 1, 1, 1 },
	[07500 ... 07507] = { EMIT_BCHG, NULL, 0, SR_Z, 1, 0, 4 },
	[07520 ... 07547] = { EMIT_BCHG, NULL, 0, SR_Z, 1, 0, 1 },
	[07550 ... 07571] = { EMIT_BCHG, NULL, 0, SR_Z, 1, 1, 1 },

	[00600 ... 00607] = { EMIT_BCLR, NULL, 0, SR_Z, 1, 0, 4 },
	[00620 ... 00647] = { EMIT_BCLR, NULL, 0, SR_Z, 1, 0, 1 },
	[00650 ... 00671] = { EMIT_BCLR, NULL, 0, SR_Z, 1, 1, 1 },
	[01600 ... 01607] = { EMIT_BCLR, NULL, 0, SR_Z, 1, 0, 4 },
	[01620 ... 01647] = { EMIT_BCLR, NULL, 0, SR_Z, 1, 0, 1 },
	[01650 ... 01671] = { EMIT_BCLR, NULL, 0, SR_Z, 1, 1, 1 },
	[02600 ... 02607] = { EMIT_BCLR, NULL, 0, SR_Z, 1, 0, 4 },
	[02620 ... 02647] = { EMIT_BCLR, NULL, 0, SR_Z, 1, 0, 1 },
	[02650 ... 02671] = { EMIT_BCLR, NULL, 0, SR_Z, 1, 1, 1 },
	[03600 ... 03607] = { EMIT_BCLR, NULL, 0, SR_Z, 1, 0, 4 },
	[03620 ... 03647] = { EMIT_BCLR, NULL, 0, SR_Z, 1, 0, 1 },
	[03650 ... 03671] = { EMIT_BCLR, NULL, 0, SR_Z, 1, 1, 1 },
	[04600 ... 04607] = { EMIT_BCLR, NULL, 0, SR_Z, 1, 0, 4 },
	[04620 ... 04647] = { EMIT_BCLR, NULL, 0, SR_Z, 1, 0, 1 },
	[04650 ... 04671] = { EMIT_BCLR, NULL, 0, SR_Z, 1, 1, 1 },
	[05600 ... 05607] = { EMIT_BCLR, NULL, 0, SR_Z, 1, 0, 4 },
	[05620 ... 05647] = { EMIT_BCLR, NULL, 0, SR_Z, 1, 0, 1 },
	[05650 ... 05671] = { EMIT_BCLR, NULL, 0, SR_Z, 1, 1, 1 },
	[06600 ... 06607] = { EMIT_BCLR, NULL, 0, SR_Z, 1, 0, 4 },
	[06620 ... 06647] = { EMIT_BCLR, NULL, 0, SR_Z, 1, 0, 1 },
	[06650 ... 06671] = { EMIT_BCLR, NULL, 0, SR_Z, 1, 1, 1 },
	[07600 ... 07607] = { EMIT_BCLR, NULL, 0, SR_Z, 1, 0, 4 },
	[07620 ... 07647] = { EMIT_BCLR, NULL, 0, SR_Z, 1, 0, 1 },
	[07650 ... 07671] = { EMIT_BCLR, NULL, 0, SR_Z, 1, 1, 1 },

	[00700 ... 00707] = { EMIT_BSET, NULL, 0, SR_Z, 1, 0, 4 },
	[00720 ... 00747] = { EMIT_BSET, NULL, 0, SR_Z, 1, 0, 1 },
	[00750 ... 00771] = { EMIT_BSET, NULL, 0, SR_Z, 1, 1, 1 },
	[01700 ... 01707] = { EMIT_BSET, NULL, 0, SR_Z, 1, 0, 4 },
	[01720 ... 01747] = { EMIT_BSET, NULL, 0, SR_Z, 1, 0, 1 },
	[01750 ... 01771] = { EMIT_BSET, NULL, 0, SR_Z, 1, 1, 1 },
	[02700 ... 02707] = { EMIT_BSET, NULL, 0, SR_Z, 1, 0, 4 },
	[02720 ... 02747] = { EMIT_BSET, NULL, 0, SR_Z, 1, 0, 1 },
	[02750 ... 02771] = { EMIT_BSET, NULL, 0, SR_Z, 1, 1, 1 },
	[03700 ... 03707] = { EMIT_BSET, NULL, 0, SR_Z, 1, 0, 4 },
	[03720 ... 03747] = { EMIT_BSET, NULL, 0, SR_Z, 1, 0, 1 },
	[03750 ... 03771] = { EMIT_BSET, NULL, 0, SR_Z, 1, 1, 1 },
	[04700 ... 04707] = { EMIT_BSET, NULL, 0, SR_Z, 1, 0, 4 },
	[04720 ... 04747] = { EMIT_BSET, NULL, 0, SR_Z, 1, 0, 1 },
	[04750 ... 04771] = { EMIT_BSET, NULL, 0, SR_Z, 1, 1, 1 },
	[05700 ... 05707] = { EMIT_BSET, NULL, 0, SR_Z, 1, 0, 4 },
	[05720 ... 05747] = { EMIT_BSET, NULL, 0, SR_Z, 1, 0, 1 },
	[05750 ... 05771] = { EMIT_BSET, NULL, 0, SR_Z, 1, 1, 1 },
	[06700 ... 06707] = { EMIT_BSET, NULL, 0, SR_Z, 1, 0, 4 },
	[06720 ... 06747] = { EMIT_BSET, NULL, 0, SR_Z, 1, 0, 1 },
	[06750 ... 06771] = { EMIT_BSET, NULL, 0, SR_Z, 1, 1, 1 },
	[07700 ... 07707] = { EMIT_BSET, NULL, 0, SR_Z, 1, 0, 4 },
	[07720 ... 07747] = { EMIT_BSET, NULL, 0, SR_Z, 1, 0, 1 },
	[07750 ... 07771] = { EMIT_BSET, NULL, 0, SR_Z, 1, 1, 1 },

	[05320 ... 05347] = { EMIT_CAS, NULL, 0, SR_NZVC, 2, 0, 1 },
	[05350 ... 05371] = { EMIT_CAS, NULL, 0, SR_NZVC, 2, 1, 1 },
	[06320 ... 06347] = { EMIT_CAS, NULL, 0, SR_NZVC, 2, 0, 2 },
	[06350 ... 06371] = { EMIT_CAS, NULL, 0, SR_NZVC, 2, 1, 2 },
	[07320 ... 07347] = { EMIT_CAS, NULL, 0, SR_NZVC, 2, 0, 4 },
	[07350 ... 07371] = { EMIT_CAS, NULL, 0, SR_NZVC, 2, 1, 4 },

	[0xcfc]			  = { EMIT_CAS2, NULL, 0, SR_NZVC, 3, 0, 2 },
	[0xefc]			  = { EMIT_CAS2, NULL, 0, SR_NZVC, 3, 0, 4 },

	[00320 ... 00327] = { EMIT_CMP2, NULL, SR_CCR, SR_NZVC, 2, 0, 1 },
	[00350 ... 00373] = { EMIT_CMP2, NULL, SR_CCR, SR_NZVC, 2, 1, 1 },
	[01320 ... 01327] = { EMIT_CMP2, NULL, SR_CCR, SR_NZVC, 2, 0, 2 },
	[01350 ... 01373] = { EMIT_CMP2, NULL, SR_CCR, SR_NZVC, 2, 1, 2 },
	[02320 ... 02327] = { EMIT_CMP2, NULL, SR_CCR, SR_NZVC, 2, 0, 4 },
	[02350 ... 02373] = { EMIT_CMP2, NULL, SR_CCR, SR_NZVC, 2, 1, 4 },

	[00410 ... 00417] = { EMIT_MOVEP, NULL, 0, 0, 2, 0, 2 },
	[00510 ... 00517] = { EMIT_MOVEP, NULL, 0, 0, 2, 0, 4 },
	[00610 ... 00617] = { EMIT_MOVEP, NULL, 0, 0, 2, 0, 2 },
	[00710 ... 00717] = { EMIT_MOVEP, NULL, 0, 0, 2, 0, 4 },
	[01410 ... 01417] = { EMIT_MOVEP, NULL, 0, 0, 2, 0, 2 },
	[01510 ... 01517] = { EMIT_MOVEP, NULL, 0, 0, 2, 0, 4 },
	[01610 ... 01617] = { EMIT_MOVEP, NULL, 0, 0, 2, 0, 2 },
	[01710 ... 01717] = { EMIT_MOVEP, NULL, 0, 0, 2, 0, 4 },
	[02410 ... 02417] = { EMIT_MOVEP, NULL, 0, 0, 2, 0, 2 },
	[02510 ... 02517] = { EMIT_MOVEP, NULL, 0, 0, 2, 0, 4 },
	[02610 ... 02617] = { EMIT_MOVEP, NULL, 0, 0, 2, 0, 2 },
	[02710 ... 02717] = { EMIT_MOVEP, NULL, 0, 0, 2, 0, 4 },
	[03410 ... 03417] = { EMIT_MOVEP, NULL, 0, 0, 2, 0, 2 },
	[03510 ... 03517] = { EMIT_MOVEP, NULL, 0, 0, 2, 0, 4 },
	[03610 ... 03617] = { EMIT_MOVEP, NULL, 0, 0, 2, 0, 2 },
	[03710 ... 03717] = { EMIT_MOVEP, NULL, 0, 0, 2, 0, 4 },
	[04410 ... 04417] = { EMIT_MOVEP, NULL, 0, 0, 2, 0, 2 },
	[04510 ... 04517] = { EMIT_MOVEP, NULL, 0, 0, 2, 0, 4 },
	[04610 ... 04617] = { EMIT_MOVEP, NULL, 0, 0, 2, 0, 2 },
	[04710 ... 04717] = { EMIT_MOVEP, NULL, 0, 0, 2, 0, 4 },
	[05410 ... 05417] = { EMIT_MOVEP, NULL, 0, 0, 2, 0, 2 },
	[05510 ... 05517] = { EMIT_MOVEP, NULL, 0, 0, 2, 0, 4 },
	[05610 ... 05617] = { EMIT_MOVEP, NULL, 0, 0, 2, 0, 2 },
	[05710 ... 05717] = { EMIT_MOVEP, NULL, 0, 0, 2, 0, 4 },
	[06410 ... 06417] = { EMIT_MOVEP, NULL, 0, 0, 2, 0, 2 },
	[06510 ... 06517] = { EMIT_MOVEP, NULL, 0, 0, 2, 0, 4 },
	[06610 ... 06617] = { EMIT_MOVEP, NULL, 0, 0, 2, 0, 2 },
	[06710 ... 06717] = { EMIT_MOVEP, NULL, 0, 0, 2, 0, 4 },
	[07410 ... 07417] = { EMIT_MOVEP, NULL, 0, 0, 2, 0, 2 },
	[07510 ... 07517] = { EMIT_MOVEP, NULL, 0, 0, 2, 0, 4 },
	[07610 ... 07617] = { EMIT_MOVEP, NULL, 0, 0, 2, 0, 2 },
	[07710 ... 07717] = { EMIT_MOVEP, NULL, 0, 0, 2, 0, 4 },

	[07020 ... 07047] = { EMIT_MOVES, NULL, SR_S, 0, 2, 0, 1 },
	[07050 ... 07071] = { EMIT_MOVES, NULL, SR_S, 0, 2, 1, 1 },
	[07120 ... 07147] = { EMIT_MOVES, NULL, SR_S, 0, 2, 0, 2 },
	[07150 ... 07171] = { EMIT_MOVES, NULL, SR_S, 0, 2, 1, 2 },
	[07220 ... 07247] = { EMIT_MOVES, NULL, SR_S, 0, 2, 0, 4 },
	[07250 ... 07271] = { EMIT_MOVES, NULL, SR_S, 0, 2, 1, 4 },
    #endif

    return table;
}

static constexpr auto InsnTable = BuildInsnTable();

__attribute__((optimize("no-optimize-sibling-calls")))
void INTERPRET_line0(uint32_t opcode)
{
    InsnTable[opcode & 0xfff](opcode);
}
