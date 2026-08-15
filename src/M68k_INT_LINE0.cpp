#include <cstdint>
#include <arm_neon.h>
#include <array>

#include <emu68/m68k/interpreter.hpp>

#include "RegisterMapping.h"

namespace Emu68::M68k::Interpreter::Line0 {

template<uint8_t Mode, uint8_t Reg, class Type>
void ANDI(uint32_t opcode)
{
    Type immediate;

    PC += 2;

    if constexpr (sizeof(Type) == 2 || sizeof(Type) == 4) {
        immediate = *(Type *)(uintptr_t)PC;
        PC += sizeof(Type);
    } else {
        immediate = (Type)*(uint16_t *)(uintptr_t)PC; // byte imm still occupies a full word
        PC += 2;
    }

    auto oper = [&](Type v) -> Type {
        uint32_t sr = SR & ~SR_NZVC;

        v &= immediate;

        if (v == 0) {
            sr |= SR_Z;
        } else if (v < 0) {
            sr |= SR_N;
        }
        SR = sr;

        return v;
    };

    if constexpr (Mode == DEFAULT_EA || Reg == DEFAULT_EA) {
        uint32_t mode = (Mode == DEFAULT_EA) ? (opcode >> 3) & 7 : Mode;
        uint32_t reg = (Reg == DEFAULT_EA) ? (opcode & 7) : Reg;
        readModifyWriteEA<Type>(mode, reg, oper);
    } else {
        readModifyWriteEA<Mode, Reg, Type>(oper);
    }
}

template<uint8_t Mode, uint8_t Reg, class Type>
void ORI(uint32_t opcode)
{
    Type immediate;

    PC += 2;

    if constexpr (sizeof(Type) == 2 || sizeof(Type) == 4) {
        immediate = *(Type *)(uintptr_t)PC;
        PC += sizeof(Type);
    } else {
        immediate = (Type)*(uint16_t *)(uintptr_t)PC; // byte imm still occupies a full word
        PC += 2;
    }

    auto oper = [&](Type v) -> Type {
        uint32_t sr = SR & ~SR_NZVC;

        v |= immediate;

        if (v == 0) {
            sr |= SR_Z;
        } else if (v < 0) {
            sr |= SR_N;
        }
        SR = sr;

        return v;
    };
    
    if constexpr (Mode == DEFAULT_EA || Reg == DEFAULT_EA) {
        uint32_t mode = (Mode == DEFAULT_EA) ? (opcode >> 3) & 7 : Mode;
        uint32_t reg = (Reg == DEFAULT_EA) ? (opcode & 7) : Reg;
        readModifyWriteEA<Type>(mode, reg, oper);
    } else {
        readModifyWriteEA<Mode, Reg, Type>(oper);
    }
}

template<uint8_t Mode, uint8_t Reg, class Type>
void EORI(uint32_t opcode)
{
    Type immediate;

    PC += 2;

    if constexpr (sizeof(Type) == 2 || sizeof(Type) == 4) {
        immediate = *(Type *)(uintptr_t)PC;
        PC += sizeof(Type);
    } else {
        immediate = (Type)*(uint16_t *)(uintptr_t)PC; // byte imm still occupies a full word
        PC += 2;
    }

    auto oper = [&](Type v) -> Type {
        uint32_t sr = SR & ~SR_NZVC;

        v ^= immediate;

        if (v == 0) {
            sr |= SR_Z;
        } else if (v < 0) {
            sr |= SR_N;
        }
        SR = sr;

        return v;
    };

    if constexpr (Mode == DEFAULT_EA || Reg == DEFAULT_EA) {
        uint32_t mode = (Mode == DEFAULT_EA) ? (opcode >> 3) & 7 : Mode;
        uint32_t reg = (Reg == DEFAULT_EA) ? (opcode & 7) : Reg;
        readModifyWriteEA<Type>(mode, reg, oper);
    } else {
        readModifyWriteEA<Mode, Reg, Type>(oper);
    }
}

template<uint8_t Mode, uint8_t Reg, class Type>
void ADDI(uint32_t opcode)
{
    Type immediate;
    PC += 2;
    if constexpr (sizeof(Type) == 2 || sizeof(Type) == 4) {
        immediate = *(Type *)(uintptr_t)PC;
        PC += sizeof(Type);
    } else {
        immediate = (Type)*(uint16_t *)(uintptr_t)PC;
        PC += 2;
    }

    auto oper = [&](Type v) -> Type {
        auto [result, ccr] = arithWithFlags<Type, false>(v, immediate);
        SR = (SR & ~(SR_X | SR_NZVC)) | ccr | ((ccr & SR_Calt) ? SR_X : 0);
        return result;
    };

    if constexpr (Mode == DEFAULT_EA || Reg == DEFAULT_EA) {
        uint32_t mode = (Mode == DEFAULT_EA) ? (opcode >> 3) & 7 : Mode;
        uint32_t reg = (Reg == DEFAULT_EA) ? (opcode & 7) : Reg;
        readModifyWriteEA<Type>(mode, reg, oper);
    } else {
        readModifyWriteEA<Mode, Reg, Type>(oper);
    }
}

template<uint8_t Mode, uint8_t Reg, class Type>
void SUBI(uint32_t opcode)
{
    Type immediate;
    PC += 2;
    if constexpr (sizeof(Type) == 2 || sizeof(Type) == 4) {
        immediate = *(Type *)(uintptr_t)PC;
        PC += sizeof(Type);
    } else {
        immediate = (Type)*(uint16_t *)(uintptr_t)PC;
        PC += 2;
    }

    auto oper = [&](Type v) -> Type {
        auto [result, ccr] = arithWithFlags<Type, true>(v, immediate);
        SR = (SR & ~(SR_X | SR_NZVC)) | ccr | ((ccr & SR_Calt) ? SR_X : 0);
        return result;
    };

    if constexpr (Mode == DEFAULT_EA || Reg == DEFAULT_EA) {
        uint32_t mode = (Mode == DEFAULT_EA) ? (opcode >> 3) & 7 : Mode;
        uint32_t reg = (Reg == DEFAULT_EA) ? (opcode & 7) : Reg;
        readModifyWriteEA<Type>(mode, reg, oper);
    } else {
        readModifyWriteEA<Mode, Reg, Type>(oper);
    }
}

template<uint8_t Mode, uint8_t Reg, class Type>
void CMPI(uint32_t opcode)
{
    Type immediate;
    PC += 2;
    if constexpr (sizeof(Type) == 2 || sizeof(Type) == 4) {
        immediate = *(Type *)(uintptr_t)PC;
        PC += sizeof(Type);
    } else {
        immediate = (Type)*(uint16_t *)(uintptr_t)PC;
        PC += 2;
    }

    Type v;

    if constexpr (Mode == DEFAULT_EA || Reg == DEFAULT_EA) {
        uint32_t mode = (Mode == DEFAULT_EA) ? (opcode >> 3) & 7 : Mode;
        uint32_t reg = (Reg == DEFAULT_EA) ? (opcode & 7) : Reg;
        v = loadFromEA<Type>(mode, reg);
    } else {
        v = loadFromEA<Mode, Reg, Type>();
    }

    auto [result, ccr] = arithWithFlags<Type, true>(v, immediate);
    (void)result;                          // discarded — CMPI keeps only the flags
    SR = (SR & ~SR_NZVC) | ccr;            // X untouched
}

void ORI_to_CCR(uint32_t)
{
    uint16_t immed = swapVC(*(uint8_t *)(uintptr_t)(PC + 3) & SR_CCR);
    SR = SR | immed;
    PC += 4;
}

void ORI_to_SR(uint32_t)
{
    uint16_t immed = swapVC(*(uint16_t *)(uintptr_t)(PC + 2) & SR_ALL);
    uint16_t sr = SR;
    uint16_t changed = sr;
    
    /* Modifying SR requires supervisor rights */
    if (sr & SR_S) {
        sr |= immed;
        changed ^= sr;

        handleChangedSR(sr, changed);

        SR = sr;
        PC += 4;
    } else {
        raiseException(VECTOR_PRIVILEGE_VIOLATION, ExceptionFrameFormat::FORMAT_0, 0, 0);
    }
}

void ANDI_to_CCR(uint32_t)
{
	uint16_t immed = swapVC(*(uint8_t *)(uintptr_t)(PC + 3) & SR_CCR);
    SR = (SR & ~SR_CCR) | (SR & immed);
	PC += 4;
}

void ANDI_to_SR(uint32_t)
{
    uint16_t immed = swapVC(*(uint16_t *)(uintptr_t)(PC + 2) & SR_ALL);
    uint16_t sr = SR;
    uint16_t changed = sr;
    
    /* Modifying SR requires supervisor rights */
    if (sr & SR_S) {
        sr &= immed;
        changed ^= sr;

        handleChangedSR(sr, changed);

        SR = sr;
        PC += 4;
    } else {
        raiseException(VECTOR_PRIVILEGE_VIOLATION, ExceptionFrameFormat::FORMAT_0, 0, 0);
    }
}

void EORI_to_CCR(uint32_t)
{
    uint16_t immed = swapVC(*(uint8_t *)(uintptr_t)(PC + 3) & SR_CCR);
    SR = SR ^ immed;
    PC += 4;
}

void EORI_to_SR(uint32_t)
{
    uint16_t immed = swapVC(*(uint16_t *)(uintptr_t)(PC + 2) & SR_ALL);
    uint16_t sr = SR;
    uint16_t changed = sr;
    
    /* Modifying SR requires supervisor rights */
    if (sr & SR_S) {
        sr ^= immed;
        changed ^= sr;

        handleChangedSR(sr, changed);
        
        SR = sr;
        PC += 4;
    } else {
        raiseException(VECTOR_PRIVILEGE_VIOLATION, ExceptionFrameFormat::FORMAT_0, 0, 0);
    }
}

template<uint8_t Dn>
void BTST_IMM_Dn(uint32_t)
{
    uint32_t mask = 1 << (*(uint8_t *)(uintptr_t)(PC + 3) & 31);
    PC += 4;

    if (getD<Dn, uint32_t>() & mask) {
        SR &= ~SR_Z;
    } else {
        SR |= SR_Z;
    }
}

template<uint8_t Dn>
void BSET_IMM_Dn(uint32_t)
{
    uint32_t mask = 1 << (*(uint8_t *)(uintptr_t)(PC + 3) & 31);
    PC += 4;
    
    if (getD<Dn, uint32_t>() & mask) {
        SR &= ~SR_Z;
    } else {
        SR |= SR_Z;
    }
    setD<Dn, uint32_t>(getD<Dn, uint32_t>() | mask);
}

template<uint8_t Dn>
void BCLR_IMM_Dn(uint32_t)
{
    uint32_t mask = 1 << (*(uint8_t *)(uintptr_t)(PC + 3) & 31);
    PC += 4;
    
    if (getD<Dn, uint32_t>() & mask) {
        SR &= ~SR_Z;
    } else {
        SR |= SR_Z;
    }
    setD<Dn, uint32_t>(getD<Dn, uint32_t>() & ~mask);
}

template<uint8_t Dn>
void BCHG_IMM_Dn(uint32_t)
{
    uint32_t mask = 1 << (*(uint8_t *)(uintptr_t)(PC + 3) & 31);
    PC += 4;
    
    if (getD<Dn, uint32_t>() & mask) {
        SR &= ~SR_Z;
    } else {
        SR |= SR_Z;
    }
    setD<Dn, uint32_t>(getD<Dn, uint32_t>() ^ mask);
}

template<uint8_t Mode, uint8_t Reg> requires (Mode > 1)
void BTST_IMM_EA(uint32_t)
{
    uint32_t mask = 1 << (*(uint8_t *)(uintptr_t)(PC + 3) & 7);
    uint8_t v;
    
    PC += 4;
    v = loadFromEA<Mode, Reg, uint8_t>();

    if (v & mask) {
        SR &= ~SR_Z;
    } else {
        SR |= SR_Z;
    }
}

template<uint8_t Mode, uint8_t Reg> requires (Mode > 1)
void BSET_IMM_EA(uint32_t)
{
    uint32_t mask = 1 << (*(uint8_t *)(uintptr_t)(PC + 3) & 7);
    PC += 4;
    
    readModifyWriteEA<Mode, Reg, uint8_t>([&](uint8_t v) -> uint8_t {
        if (v & mask) {
            SR &= ~SR_Z;
        } else {
            SR |= SR_Z;
        }
        return v | mask;
    });
}

template<uint8_t Mode, uint8_t Reg> requires (Mode > 1)
void BCLR_IMM_EA(uint32_t)
{
    uint32_t mask = 1 << (*(uint8_t *)(uintptr_t)(PC + 3) & 7);
    PC += 4;
    
    readModifyWriteEA<Mode, Reg, uint8_t>([&](uint8_t v) -> uint8_t {
        if (v & mask) {
            SR &= ~SR_Z;
        } else {
            SR |= SR_Z;
        }
        return v & ~mask;
    });
}

template<uint8_t Mode, uint8_t Reg> requires (Mode > 1)
void BCHG_IMM_EA(uint32_t)
{
    uint32_t mask = 1 << (*(uint8_t *)(uintptr_t)(PC + 3) & 7);
    PC += 4;
    
    readModifyWriteEA<Mode, Reg, uint8_t>([&](uint8_t v) -> uint8_t {
        if (v & mask) {
            SR &= ~SR_Z;
        } else {
            SR |= SR_Z;
        }
        return v ^ mask;
    });
}

template<uint8_t Mode, uint8_t Reg, uint8_t Dn> requires (Mode > 1)
void BTST_REG(uint32_t)
{
    uint8_t mask = 1 << (getD<Dn, uint32_t>() & 7);
    uint8_t v;
    
    PC += 2;
    v = loadFromEA<Mode, Reg, uint8_t>();

    if (v & mask) {
        SR &= ~SR_Z;
    } else {
        SR |= SR_Z;
    }
}

template<uint8_t Mode, uint8_t Reg, uint8_t Dn> requires (Mode > 1)
void BSET_REG(uint32_t)
{
    uint8_t mask = 1 << (getD<Dn, uint32_t>() & 7);
    PC += 2;

    readModifyWriteEA<Mode, Reg, uint8_t>([&](uint8_t v) -> uint8_t {
        if (v & mask) {
            SR &= ~SR_Z;
        } else {
            SR |= SR_Z;
        }
        return v | mask;
    });
}

template<uint8_t Mode, uint8_t Reg, uint8_t Dn> requires (Mode > 1)
void BCLR_REG(uint32_t)
{
    uint8_t mask = 1 << (getD<Dn, uint32_t>() & 7);
    PC += 2;

    readModifyWriteEA<Mode, Reg, uint8_t>([&](uint8_t v) -> uint8_t {
        if (v & mask) {
            SR &= ~SR_Z;
        } else {
            SR |= SR_Z;
        }
        return v & ~mask;
    });
}

template<uint8_t Mode, uint8_t Reg, uint8_t Dn> requires (Mode > 1)
void BCHG_REG(uint32_t)
{
    uint8_t mask = 1 << (getD<Dn, uint32_t>() & 7);
    PC += 2;

    readModifyWriteEA<Mode, Reg, uint8_t>([&](uint8_t v) -> uint8_t {
        if (v & mask) {
            SR &= ~SR_Z;
        } else {
            SR |= SR_Z;
        }
        return v ^ mask;
    });
}

template<uint8_t Reg, uint8_t Dn>
void BTST_REG_Dn(uint32_t)
{
    uint32_t mask = 1 << (getD<Dn, uint32_t>() & 31);
    if (getD<Reg, uint32_t>() & mask) {
        SR &= ~SR_Z;
    } else {
        SR |= SR_Z;
    }
    PC += 2;
}

template<uint8_t Reg, uint8_t Dn>
void BSET_REG_Dn(uint32_t)
{
    uint32_t mask = 1 << (getD<Dn, uint32_t>() & 31);
    if (getD<Reg, uint32_t>() & mask) {
        SR &= ~SR_Z;
    } else {
        SR |= SR_Z;
    }
    setD<Reg, uint32_t>(getD<Reg, uint32_t>() | mask);
    PC += 2;
}

template<uint8_t Reg, uint8_t Dn>
void BCLR_REG_Dn(uint32_t)
{
    uint32_t mask = 1 << (getD<Dn, uint32_t>() & 31);
    if (getD<Reg, uint32_t>() & mask) {
        SR &= ~SR_Z;
    } else {
        SR |= SR_Z;
    }
    setD<Reg, uint32_t>(getD<Reg, uint32_t>() & ~mask);
    PC += 2;
}

template<uint8_t Reg, uint8_t Dn>
void BCHG_REG_Dn(uint32_t)
{
    uint32_t mask = 1 << (getD<Dn, uint32_t>() & 31);
    if (getD<Reg, uint32_t>() & mask) {
        SR &= ~SR_Z;
    } else {
        SR |= SR_Z;
    }
    setD<Reg, uint32_t>(getD<Reg, uint32_t>() ^ mask);
    PC += 2;
}

#define FILL_Bxxx_Dn(base_offset, name) \
    [&]<std::size_t... Is>(int base, std::index_sequence<Is...>) { \
        ((table[base + ((Is >> 3) << 9) + EA(0, Is & 7)] = \
             name<(Is & 7), (Is >> 3)>), ...); \
    }((base_offset), std::make_index_sequence<64>{});

#define FILL_Bxxx_Dn_EA(base_offset, name, dn) \
    [&]<std::size_t... Is>(int base, std::index_sequence<Is...>) { \
        ((table[base + (dn << 9) + EA(2 + ((Is >> 3) & 7), Is & 7)] = \
             name<2 + ((Is >> 3) & 7), (Is & 7), dn>), ...); \
    }((base_offset), std::make_index_sequence<42>{});

#define FILL_Bxxx_EA(base_offset, name) \
    FILL_Bxxx_Dn_EA(base_offset, name, 0) \
    FILL_Bxxx_Dn_EA(base_offset, name, 1) \
    FILL_Bxxx_Dn_EA(base_offset, name, 2) \
    FILL_Bxxx_Dn_EA(base_offset, name, 3) \
    FILL_Bxxx_Dn_EA(base_offset, name, 4) \
    FILL_Bxxx_Dn_EA(base_offset, name, 5) \
    FILL_Bxxx_Dn_EA(base_offset, name, 6) \
    FILL_Bxxx_Dn_EA(base_offset, name, 7)

#define FILL_Bxxx_IMM(base_offset, name) \
    [&]<std::size_t... Dreg>(int base, std::index_sequence<Dreg...>) { \
        ((table[base + EA(0, Dreg & 7)] = \
             name<Dreg>), ...); \
    }((base_offset), std::make_index_sequence<8>{});

#define FILL_Bxxx_IMM_EA(base_offset, name) \
    [&]<std::size_t... Is>(int base, std::index_sequence<Is...>) { \
        ((table[base + EA(2 + ((Is >> 3) & 7), Is & 7)] = \
             name<2 + ((Is >> 3) & 7), (Is & 7)>), ...); \
    }((base_offset), std::make_index_sequence<42>{});


template <class Type, template<uint8_t,uint8_t,class> class Op,
          class EAF>
constexpr void fillImmedOpEA(std::array<INTERPRET_Function, 4096>& table, int base)
{
    auto fillOne = [&]<std::size_t I>() {
        constexpr unsigned eaV  = I;

        if constexpr (EAF::template valid<eaV>() && (EAF::modeArg(eaV) >= 2 || EAF::modeArg(eaV) == 0)) {
            int idx = base + (eaV << EAF::bitOffset);
            table[idx] = Op<EAF::modeArg(eaV), EAF::regArg(eaV), Type>::value;
        }
    };
    [&]<std::size_t... Is>(std::index_sequence<Is...>) {
        (fillOne.template operator()<Is>(), ...);
    }(std::make_index_sequence<EAF::size>{});
}

inline constexpr uint32_t ImmedOpSpecializeMask = 
      classBit(EAClass::Dn)
    | classBit(EAClass::Ind)     | classBit(EAClass::IndPost)
    | classBit(EAClass::IndPre)  | classBit(EAClass::D16An);

template <uint8_t Mode, uint8_t Reg, class Type>
struct ORI_Op { static constexpr auto value = ORI<Mode, Reg, Type>; };

template <uint8_t Mode, uint8_t Reg, class Type>
struct ANDI_Op { static constexpr auto value = ANDI<Mode, Reg, Type>; };

template <uint8_t Mode, uint8_t Reg, class Type>
struct SUBI_Op { static constexpr auto value = SUBI<Mode, Reg, Type>; };

template <uint8_t Mode, uint8_t Reg, class Type>
struct ADDI_Op { static constexpr auto value = ADDI<Mode, Reg, Type>; };

template <uint8_t Mode, uint8_t Reg, class Type>
struct EORI_Op { static constexpr auto value = EORI<Mode, Reg, Type>; };

template <uint8_t Mode, uint8_t Reg, class Type>
struct CMPI_Op { static constexpr auto value = CMPI<Mode, Reg, Type>; };

// EA field combines Register and EA mode in one, gets
template <unsigned BitOffset, class Type, bool Write, uint32_t HotMask>
struct EAField : FieldBase<BitOffset, 6> {
    template <unsigned V>
    static constexpr bool valid() {
        constexpr unsigned mode = V >> 3, reg = V & 7;
        if constexpr (Write) return DestEA7<mode, reg> && ByteCompatibleMode<mode, Type>;
        else                 return SourceEA7<mode, reg> && ByteCompatibleMode<mode, Type>;
    }

    static constexpr bool hot(unsigned v) {
        return (HotMask >> static_cast<unsigned>(classifyEA(v >> 3, v & 7))) & 1u;
    }
    static constexpr uint8_t modeArg(unsigned v) { return hot(v) ? uint8_t(v >> 3) : DEFAULT_EA; }
    static constexpr uint8_t regArg(unsigned v)  { return hot(v) ? uint8_t(v & 7)  : DEFAULT_EA; }
};

static constexpr std::array<INTERPRET_Function, 4096> buildInsnTable()
{
    std::array<INTERPRET_Function, 4096> table{};

    auto fill = [&table](int first, int last, INTERPRET_Function func) {
        for (int i = first; i <= last; ++i) {
            table[i] = func;
        }
    };

    auto EA = [](int mode, int reg) constexpr { return (mode << 3) | reg; };

    fill(00000, 07777, ILLEGAL);
    
    fillImmedOpEA<BYTE, ORI_Op, EAField<0,BYTE,true,ImmedOpSpecializeMask> >(table, 00000);
    fillImmedOpEA<WORD, ORI_Op, EAField<0,WORD,true,ImmedOpSpecializeMask> >(table, 00100);
    fillImmedOpEA<LONG, ORI_Op, EAField<0,LONG,true,ImmedOpSpecializeMask> >(table, 00200);

    fillImmedOpEA<BYTE, ANDI_Op, EAField<0,BYTE,true,ImmedOpSpecializeMask> >(table, 01000);
    fillImmedOpEA<WORD, ANDI_Op, EAField<0,WORD,true,ImmedOpSpecializeMask> >(table, 01100);
    fillImmedOpEA<LONG, ANDI_Op, EAField<0,LONG,true,ImmedOpSpecializeMask> >(table, 01200);

    fillImmedOpEA<BYTE, SUBI_Op, EAField<0,BYTE,true,ImmedOpSpecializeMask> >(table, 02000);
    fillImmedOpEA<WORD, SUBI_Op, EAField<0,WORD,true,ImmedOpSpecializeMask> >(table, 02100);
    fillImmedOpEA<LONG, SUBI_Op, EAField<0,LONG,true,ImmedOpSpecializeMask> >(table, 02200);

    fillImmedOpEA<BYTE, ADDI_Op, EAField<0,BYTE,true,ImmedOpSpecializeMask> >(table, 03000);
    fillImmedOpEA<WORD, ADDI_Op, EAField<0,WORD,true,ImmedOpSpecializeMask> >(table, 03100);
    fillImmedOpEA<LONG, ADDI_Op, EAField<0,LONG,true,ImmedOpSpecializeMask> >(table, 03200);

    fillImmedOpEA<BYTE, EORI_Op, EAField<0,BYTE,true,ImmedOpSpecializeMask> >(table, 05000);
    fillImmedOpEA<WORD, EORI_Op, EAField<0,WORD,true,ImmedOpSpecializeMask> >(table, 05100);
    fillImmedOpEA<LONG, EORI_Op, EAField<0,LONG,true,ImmedOpSpecializeMask> >(table, 05200);

    fillImmedOpEA<BYTE, CMPI_Op, EAField<0,BYTE,true,ImmedOpSpecializeMask> >(table, 06000);
    fillImmedOpEA<WORD, CMPI_Op, EAField<0,WORD,true,ImmedOpSpecializeMask> >(table, 06100);
    fillImmedOpEA<LONG, CMPI_Op, EAField<0,LONG,true,ImmedOpSpecializeMask> >(table, 06200);

    table[00074] = ORI_to_CCR;
    table[01074] = ANDI_to_CCR;
    table[05074] = EORI_to_CCR;

    table[00174] = ORI_to_SR;
    table[01174] = ANDI_to_SR;
    table[05174] = EORI_to_SR;

    FILL_Bxxx_Dn(00400, BTST_REG_Dn);
    FILL_Bxxx_Dn(00500, BCHG_REG_Dn);
    FILL_Bxxx_Dn(00600, BCLR_REG_Dn);
    FILL_Bxxx_Dn(00700, BSET_REG_Dn);
    FILL_Bxxx_EA(00400, BTST_REG);
    FILL_Bxxx_EA(00500, BCHG_REG);
    FILL_Bxxx_EA(00600, BCLR_REG);
    FILL_Bxxx_EA(00700, BSET_REG);

    FILL_Bxxx_IMM(04000, BTST_IMM_Dn);
    FILL_Bxxx_IMM(04100, BCHG_IMM_Dn);
    FILL_Bxxx_IMM(04200, BCLR_IMM_Dn);
    FILL_Bxxx_IMM(04300, BSET_IMM_Dn);
    FILL_Bxxx_IMM_EA(04000, BTST_IMM_EA);
    FILL_Bxxx_IMM_EA(04100, BCHG_IMM_EA);
    FILL_Bxxx_IMM_EA(04200, BCLR_IMM_EA);
    FILL_Bxxx_IMM_EA(04300, BSET_IMM_EA);

    #if 0

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

constexpr auto InsnTable __attribute__((used,aligned(4096),section(".int.jumptable.0"))) = buildInsnTable();

} // Emu68::M68k::Interpreter::Line0
