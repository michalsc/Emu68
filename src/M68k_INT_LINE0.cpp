#include <cstdint>
#include <arm_neon.h>
#include <array>

#include <emu68/m68k/interpreter>

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
        SR = (SR & ~(SR_CCR)) | ccr | ((ccr & SR_Calt) ? SR_X : 0);
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
    
    if constexpr (sizeof(Type) == 2 || sizeof(Type) == 4) {
        immediate = *getPC<Type*>(2);
        advancePC(2 + sizeof(Type));
    } else {
        immediate = (Type)*getPC<uint16_t*>(2);
        advancePC(4);
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
    uint16_t immed = swapVC(*getPC<uint8_t*>(3) & SR_CCR);
    SR = SR | immed;
    advancePC(4);
}

void ORI_to_SR(uint32_t)
{
    uint16_t immed = swapVC(*getPC<uint16_t*>(2) & SR_ALL);
    uint16_t sr = SR;
    uint16_t changed = sr;
    
    /* Modifying SR requires supervisor rights */
    if (sr & SR_S) {
        sr |= immed;
        changed ^= sr;

        handleChangedSR(sr, changed);

        SR = sr;
        advancePC(4);
    } else {
        raiseException(VECTOR_PRIVILEGE_VIOLATION, ExceptionFrameFormat::FORMAT_0, 0, 0);
    }
}

void ANDI_to_CCR(uint32_t)
{
    uint16_t immed = swapVC(*getPC<uint8_t*>(3) & SR_CCR);
    SR = (SR & ~SR_CCR) | (SR & immed);
    advancePC(4);
}

void ANDI_to_SR(uint32_t)
{
    uint16_t immed = swapVC(*getPC<uint16_t*>(2) & SR_ALL);
    uint16_t sr = SR;
    uint16_t changed = sr;
    
    /* Modifying SR requires supervisor rights */
    if (sr & SR_S) {
        sr &= immed;
        changed ^= sr;

        handleChangedSR(sr, changed);

        SR = sr;
        advancePC(4);
    } else {
        raiseException(VECTOR_PRIVILEGE_VIOLATION, ExceptionFrameFormat::FORMAT_0, 0, 0);
    }
}

void EORI_to_CCR(uint32_t)
{
    uint16_t immed = swapVC(*getPC<uint8_t*>(3) & SR_CCR);
    SR = SR ^ immed;
    advancePC(4);
}

void EORI_to_SR(uint32_t)
{
    uint16_t immed = swapVC(*getPC<uint16_t*>(2) & SR_ALL);
    uint16_t sr = SR;
    uint16_t changed = sr;
    
    /* Modifying SR requires supervisor rights */
    if (sr & SR_S) {
        sr ^= immed;
        changed ^= sr;

        handleChangedSR(sr, changed);
        
        SR = sr;
        advancePC(4);
    } else {
        raiseException(VECTOR_PRIVILEGE_VIOLATION, ExceptionFrameFormat::FORMAT_0, 0, 0);
    }
}

template<uint8_t Dn>
void BTST_IMM_Dn(uint32_t)
{
    uint32_t mask = 1 << (*getPC<uint8_t*>(3) & 31);
    advancePC(4);

    if (getD<Dn, uint32_t>() & mask) {
        SR &= ~SR_Z;
    } else {
        SR |= SR_Z;
    }
}

template<uint8_t Dn>
void BSET_IMM_Dn(uint32_t)
{
    uint32_t mask = 1 << (*getPC<uint8_t*>(3) & 31);
    advancePC(4);
    
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
    uint32_t mask = 1 << (*getPC<uint8_t*>(3) & 31);
    advancePC(4);
    
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
    uint32_t mask = 1 << (*getPC<uint8_t*>(3) & 31);
    advancePC(4);
    
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
    uint32_t mask = 1 << (*getPC<uint8_t*>(3) & 7);
    uint8_t v;
    
    advancePC(4);
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
    uint32_t mask = 1 << (*getPC<uint8_t*>(3) & 7);
    advancePC(4);
    
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
    uint32_t mask = 1 << (*getPC<uint8_t*>(3) & 7);
    advancePC(4);
    
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
    uint32_t mask = 1 << (*getPC<uint8_t*>(3) & 7);
    advancePC(4);
    
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
    
    advancePC(2);
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
    advancePC(2);

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
    advancePC(2);

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
    advancePC(2);

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
    advancePC(2);
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
    advancePC(2);
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
    advancePC(2);
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
    advancePC(2);
}

template<uint8_t mode>
void MOVEP(uint32_t opcode)
{
    int16_t offset = *getPC<int16_t*>(2);
    int dreg = (opcode >> 9) & 7;
    int areg = opcode & 7;
    
    advancePC(4);
    commitPC();

    uintptr_t addr = getA<uint32_t>(areg) + offset;

    /* Register to memory? */
    if constexpr (mode & 2) {
        uint32_t data = getD<uint32_t>(dreg);

        /* Longword transfer ? */
        if constexpr (mode & 1) {
            *(uint8_t *)(addr + 0) = (data >> 24) & 0xff;
            *(uint8_t *)(addr + 2) = (data >> 16) & 0xff;
            *(uint8_t *)(addr + 4) = (data >> 8) & 0xff;
            *(uint8_t *)(addr + 6) = data & 0xff;
        } else {
            *(uint8_t *)(addr + 0) = (data >> 8) & 0xff;
            *(uint8_t *)(addr + 2) = data & 0xff;
        }
    } else { /* Memory to register */
        uint32_t data;

        /* Longword transfer ? */
        if constexpr (mode & 1) {
            data = *(uint8_t *)(addr + 0);
            data = (data << 8) | *(uint8_t *)(addr + 2);
            data = (data << 8) | *(uint8_t *)(addr + 4);
            data = (data << 8) | *(uint8_t *)(addr + 6);
            setD<uint32_t>(dreg, data);
        } else {
            data = *(uint8_t *)(addr + 0);
            data = (data << 8) | *(uint8_t *)(addr + 2);
            setD<uint16_t>(dreg, data);
        }
    }
}

template<uint8_t Mode, uint8_t Reg, class Type>
void MOVES(uint32_t opcode)
{
    /* 
        MOVES shall use SFC and DFC but we do not support them (yet) - this turns MOVES effectively 
        into 8/16/32 bit load from EA or store to EA 
    */
    if (SR & SR_S) {
        uint16_t opcode2 = *getPC<uint16_t*>(2);
        const bool useAn = (opcode2 & 0x8000) != 0;
        const int reg = (opcode2 >> 12) & 7;
        const bool EAtoReg = (opcode2 & 0x0800) == 0;
        uint32_t ea = 0;

        advancePC(4);
        commitPC();

        if constexpr (Mode == DEFAULT_EA || Reg == DEFAULT_EA) {
            uint32_t mode = (Mode == DEFAULT_EA) ? (opcode >> 3) & 7 : Mode;
            uint32_t reg = (Reg == DEFAULT_EA) ? (opcode & 7) : Reg;
            ea = getEA<Type>(mode, reg);
        } else {
            ea = getEA<Mode, Reg, Type>();
        }

        if (EAtoReg) {
            if (useAn) {
                /* When An is destination, BYTE is still supported - it is sign-extended */
                setA<uint32_t>(reg, *(Type*)(uintptr_t)ea);
            } else {
                setD<Type>(reg, *(Type*)(uintptr_t)ea);
            }
        } else {
            Type v = 0;
            
            if (useAn) {
                if constexpr (sizeof(Type) > 1) {
                    v = getA<Type>(reg);
                } else {
                    v = getA<uint32_t>(reg);
                }
            } else {
                v = getD<Type>(reg);
            }

            *(Type*)(uintptr_t)ea = v;
        }
    } else {
        raiseException(VECTOR_PRIVILEGE_VIOLATION, ExceptionFrameFormat::FORMAT_0, 0, 0);
    }
}

template<uint8_t Mode, uint8_t Reg, class Type>
void CAS(uint32_t opcode)
{
    uint16_t opcode2 = *getPC<uint16_t*>(2);
    uint8_t du = (opcode2 >> 6) & 7;
    uint8_t dc = opcode2 & 7;

    advancePC(4);
    commitPC();

    uint32_t addr;
    if constexpr (Mode == DEFAULT_EA || Reg == DEFAULT_EA) {
        uint32_t mode = (Mode == DEFAULT_EA) ? (opcode >> 3) & 7 : Mode;
        uint32_t reg = (Reg == DEFAULT_EA) ? (opcode & 7) : Reg;
        addr = getEA<Type>(mode, reg);
    } else {
        addr = getEA<Mode, Reg, Type>();
    }

    Type* ptr = (Type*)(uintptr_t)addr;
    Type dc_val = getD<Type>(dc);
    Type du_val = getD<Type>(du);
    Type expected = dc_val;
    bool matched;

    if ((addr & (sizeof(Type) - 1)) == 0) {
        /* Atomic operation allowed on properly aligned address, only */
        matched = __atomic_compare_exchange_n(ptr, &expected, du_val, false,
                                               __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
    } else {
        Type v = *ptr;
        if (v == dc_val) {
            *ptr = du_val;
            matched = true;
        } else {
            expected = v;
            matched = false;
        }
    }

    auto [result, ccr] = arithWithFlags<Type, true>(expected, dc_val);
    (void)result;
    SR = (SR & ~SR_NZVC) | ccr;

    if (!matched) {
        setD<Type>(dc, expected);
    }
}

template<class Type>
void CAS2(uint32_t)
{
    uint16_t opcode2 = *getPC<uint16_t*>(2);
    uint16_t opcode3 = *getPC<uint16_t*>(4);

    advancePC(6);
    commitPC();

    uint32_t rn1 = (opcode2 & 0x8000) ? getA<uint32_t>((opcode2 >> 12) & 7) : getD<uint32_t>((opcode2 >> 12) & 7);
    uint32_t rn2 = (opcode3 & 0x8000) ? getA<uint32_t>((opcode3 >> 12) & 7) : getD<uint32_t>((opcode3 >> 12) & 7);

    uint8_t du1 = (opcode2 >> 6) & 7;
    uint8_t du2 = (opcode3 >> 6) & 7;
    uint8_t dc1 = opcode2 & 7;
    uint8_t dc2 = opcode3 & 7;

    Type* p1 = (Type*)(uintptr_t)rn1;
    Type* p2 = (Type*)(uintptr_t)rn2;

    Type v1 = *p1;
    Type v2 = *p2;

    auto [result1, ccr1] = arithWithFlags<Type, true>(v1, getD<Type>(dc1));
    (void)result1;

    if (ccr1 & SR_Z) {
        auto [result2, ccr2] = arithWithFlags<Type, true>(v2, getD<Type>(dc2));
        (void)result2;

        if (ccr2 & SR_Z) {
            /* 68040 stores Du2 first, then Du1 */
            *p2 = getD<Type>(du2);
            *p1 = getD<Type>(du1);
        } else {
            setD<Type>(dc1, v1);
            setD<Type>(dc2, v2);
        }

        SR = (SR & ~SR_NZVC) | ccr2;
    } else {
        setD<Type>(dc1, v1);
        SR = (SR & ~SR_NZVC) | ccr1;
    }
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

#define FILL_BTST_Dn_EA(base_offset, name, dn) \
    [&]<std::size_t... Is>(int base, std::index_sequence<Is...>) { \
        ((table[base + (dn << 9) + EA(2 + ((Is >> 3) & 7), Is & 7)] = \
             name<2 + ((Is >> 3) & 7), (Is & 7), dn>), ...); \
    }((base_offset), std::make_index_sequence<45>{});

#define FILL_Bxxx_EA(base_offset, name) \
    FILL_Bxxx_Dn_EA(base_offset, name, 0) \
    FILL_Bxxx_Dn_EA(base_offset, name, 1) \
    FILL_Bxxx_Dn_EA(base_offset, name, 2) \
    FILL_Bxxx_Dn_EA(base_offset, name, 3) \
    FILL_Bxxx_Dn_EA(base_offset, name, 4) \
    FILL_Bxxx_Dn_EA(base_offset, name, 5) \
    FILL_Bxxx_Dn_EA(base_offset, name, 6) \
    FILL_Bxxx_Dn_EA(base_offset, name, 7)

#define FILL_BTST_EA(base_offset, name) \
    FILL_BTST_Dn_EA(base_offset, name, 0) \
    FILL_BTST_Dn_EA(base_offset, name, 1) \
    FILL_BTST_Dn_EA(base_offset, name, 2) \
    FILL_BTST_Dn_EA(base_offset, name, 3) \
    FILL_BTST_Dn_EA(base_offset, name, 4) \
    FILL_BTST_Dn_EA(base_offset, name, 5) \
    FILL_BTST_Dn_EA(base_offset, name, 6) \
    FILL_BTST_Dn_EA(base_offset, name, 7)

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

#define FILL_BTST_IMM_EA(base_offset, name) \
    [&]<std::size_t... Is>(int base, std::index_sequence<Is...>) { \
        ((table[base + EA(2 + ((Is >> 3) & 7), Is & 7)] = \
             name<2 + ((Is >> 3) & 7), (Is & 7)>), ...); \
    }((base_offset), std::make_index_sequence<44>{});


template <class Type, template<uint8_t,uint8_t,class> class Op, class F1, class F2>
constexpr void fillRegReg(std::array<INTERPRET_Function, 4096>& table, int base)
{
    auto fillOne = [&]<std::size_t I>() {
        constexpr unsigned v1 = I / F2::size;
        constexpr unsigned v2 = I % F2::size;
        if constexpr (F1::valid(v1) && F2::valid(v2)) {
            int idx = base + (v1 << F1::bitOffset) + (v2 << F2::bitOffset);
            table[idx] = Op<F1::arg(v1), F2::arg(v2), Type>::value;
        }
    };
    [&]<std::size_t... Is>(std::index_sequence<Is...>) {
        (fillOne.template operator()<Is>(), ...);
    }(std::make_index_sequence<F1::size * F2::size>{});
}

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

template <unsigned BitOffset, class Type, bool Write, uint32_t ValidMask, uint32_t HotMask>
struct EAField : FieldBase<BitOffset, 6> {
    template <unsigned V>
    static constexpr bool valid() {
        constexpr unsigned mode = V >> 3, reg = V & 7;
        if constexpr (Write) return DestEA7<mode, reg> 
                                 && ByteCompatibleMode<mode, Type>
                                 && ValidMask >> static_cast<unsigned>(classifyEA(mode, reg)) & 1u;
        else                 return SourceEA7<mode, reg> 
                                 && ByteCompatibleMode<mode, Type>
                                 && ValidMask >> static_cast<unsigned>(classifyEA(mode, reg)) & 1u;
    }

    static constexpr bool hot(unsigned v) {
        return (HotMask >> static_cast<unsigned>(classifyEA(v >> 3, v & 7))) & 1u;
    }

    static constexpr uint8_t modeArg(unsigned v) { return hot(v) ? uint8_t(v >> 3) : DEFAULT_EA; }
    static constexpr uint8_t regArg(unsigned v)  { return hot(v) ? uint8_t(v & 7)  : DEFAULT_EA; }
};

template <class Type, bool Write, template<uint8_t,uint8_t,class> class Op,
          class EAF>
constexpr void fillEA(std::array<INTERPRET_Function, 4096>& table, int base)
{
    auto fillOne = [&]<std::size_t I>() {
        constexpr unsigned eaV  = I;

        if constexpr (EAF::template valid<eaV>()) {
            int idx = base + (eaV << EAF::bitOffset);
            table[idx] = Op<EAF::modeArg(eaV), EAF::regArg(eaV), Type>::value;
        }
    };
    [&]<std::size_t... Is>(std::index_sequence<Is...>) {
        (fillOne.template operator()<Is>(), ...);
    }(std::make_index_sequence<EAF::size>{});
}

inline constexpr uint32_t MOVESEnabledMask = 
      classBit(EAClass::Ind)     | classBit(EAClass::IndPost)
    | classBit(EAClass::IndPre)  | classBit(EAClass::D16An)
    | classBit(EAClass::D8AnXn)  | classBit(EAClass::AbsW)
    | classBit(EAClass::AbsL);

inline constexpr uint32_t MOVESSpecializeMask = 0;

inline constexpr uint32_t CASEnabledMask = 
      classBit(EAClass::Ind)     | classBit(EAClass::IndPost)
    | classBit(EAClass::IndPre)  | classBit(EAClass::D16An)
    | classBit(EAClass::D8AnXn)  | classBit(EAClass::AbsW)
    | classBit(EAClass::AbsL);

inline constexpr uint32_t CASSpecializeMask = 
      classBit(EAClass::Ind)     | classBit(EAClass::D16An)
    | classBit(EAClass::AbsW)    | classBit(EAClass::AbsL);

inline constexpr uint32_t ImmedOpSpecializeMask = 
      classBit(EAClass::Dn)
    | classBit(EAClass::Ind)     | classBit(EAClass::IndPost)
    | classBit(EAClass::IndPre)  | classBit(EAClass::D16An)
    | classBit(EAClass::Imm);

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

template <uint8_t F1, uint8_t F2, class Type>
struct MOVEP_D32toM_Op { static constexpr auto value = MOVEP<7>; };

template <uint8_t F1, uint8_t F2, class Type>
struct MOVEP_D16toM_Op { static constexpr auto value = MOVEP<6>; };

template <uint8_t F1, uint8_t F2, class Type>
struct MOVEP_MtoD32_Op { static constexpr auto value = MOVEP<5>; };

template <uint8_t F1, uint8_t F2, class Type>
struct MOVEP_MtoD16_Op { static constexpr auto value = MOVEP<4>; };

template<uint8_t Mode, uint8_t Reg, class Type>
struct MOVES_Op { static constexpr auto value = MOVES<Mode, Reg, Type>; };

template<uint8_t Mode, uint8_t Reg, class Type>
struct CAS_Op { static constexpr auto value = CAS<Mode, Reg, Type>; };

// EA field combines Register and EA mode in one, gets
template <unsigned BitOffset, class Type, bool Write, uint32_t HotMask>
struct EAFieldDef : FieldBase<BitOffset, 6> {
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
    
    fillImmedOpEA<BYTE, ORI_Op, EAFieldDef<0,BYTE,true,ImmedOpSpecializeMask> >(table, 00000);
    fillImmedOpEA<WORD, ORI_Op, EAFieldDef<0,WORD,true,ImmedOpSpecializeMask> >(table, 00100);
    fillImmedOpEA<LONG, ORI_Op, EAFieldDef<0,LONG,true,ImmedOpSpecializeMask> >(table, 00200);

    fillImmedOpEA<BYTE, ANDI_Op, EAFieldDef<0,BYTE,true,ImmedOpSpecializeMask> >(table, 01000);
    fillImmedOpEA<WORD, ANDI_Op, EAFieldDef<0,WORD,true,ImmedOpSpecializeMask> >(table, 01100);
    fillImmedOpEA<LONG, ANDI_Op, EAFieldDef<0,LONG,true,ImmedOpSpecializeMask> >(table, 01200);

    fillImmedOpEA<BYTE, SUBI_Op, EAFieldDef<0,BYTE,true,ImmedOpSpecializeMask> >(table, 02000);
    fillImmedOpEA<WORD, SUBI_Op, EAFieldDef<0,WORD,true,ImmedOpSpecializeMask> >(table, 02100);
    fillImmedOpEA<LONG, SUBI_Op, EAFieldDef<0,LONG,true,ImmedOpSpecializeMask> >(table, 02200);

    fillImmedOpEA<BYTE, ADDI_Op, EAFieldDef<0,BYTE,true,ImmedOpSpecializeMask> >(table, 03000);
    fillImmedOpEA<WORD, ADDI_Op, EAFieldDef<0,WORD,true,ImmedOpSpecializeMask> >(table, 03100);
    fillImmedOpEA<LONG, ADDI_Op, EAFieldDef<0,LONG,true,ImmedOpSpecializeMask> >(table, 03200);

    fillImmedOpEA<BYTE, EORI_Op, EAFieldDef<0,BYTE,true,ImmedOpSpecializeMask> >(table, 05000);
    fillImmedOpEA<WORD, EORI_Op, EAFieldDef<0,WORD,true,ImmedOpSpecializeMask> >(table, 05100);
    fillImmedOpEA<LONG, EORI_Op, EAFieldDef<0,LONG,true,ImmedOpSpecializeMask> >(table, 05200);

    fillImmedOpEA<BYTE, CMPI_Op, EAFieldDef<0,BYTE,false,ImmedOpSpecializeMask> >(table, 06000);
    fillImmedOpEA<WORD, CMPI_Op, EAFieldDef<0,WORD,false,ImmedOpSpecializeMask> >(table, 06100);
    fillImmedOpEA<LONG, CMPI_Op, EAFieldDef<0,LONG,false,ImmedOpSpecializeMask> >(table, 06200);

    fillRegReg<WORD, MOVEP_MtoD16_Op, RegField<9>, RegField<0> >(table, 00410);
    fillRegReg<WORD, MOVEP_MtoD32_Op, RegField<9>, RegField<0> >(table, 00510);
    fillRegReg<WORD, MOVEP_D16toM_Op, RegField<9>, RegField<0> >(table, 00610);
    fillRegReg<WORD, MOVEP_D32toM_Op, RegField<9>, RegField<0> >(table, 00710);

    fillEA<BYTE, true, MOVES_Op, EAField<0, BYTE, true, MOVESEnabledMask, MOVESSpecializeMask> >(table, 07000);
    fillEA<WORD, true, MOVES_Op, EAField<0, WORD, true, MOVESEnabledMask, MOVESSpecializeMask> >(table, 07100);
    fillEA<LONG, true, MOVES_Op, EAField<0, LONG, true, MOVESEnabledMask, MOVESSpecializeMask> >(table, 07200);

    fillEA<BYTE, true, CAS_Op, EAField<0, BYTE, true, CASEnabledMask, CASSpecializeMask> >(table, 05300);
    fillEA<WORD, true, CAS_Op, EAField<0, WORD, true, CASEnabledMask, CASSpecializeMask> >(table, 06300);
    fillEA<LONG, true, CAS_Op, EAField<0, LONG, true, CASEnabledMask, CASSpecializeMask> >(table, 07300);

    table[06374] = CAS2<WORD>;
    table[07374] = CAS2<LONG>;

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
    FILL_BTST_EA(00400, BTST_REG);
    FILL_Bxxx_EA(00500, BCHG_REG);
    FILL_Bxxx_EA(00600, BCLR_REG);
    FILL_Bxxx_EA(00700, BSET_REG);

    FILL_Bxxx_IMM(04000, BTST_IMM_Dn);
    FILL_Bxxx_IMM(04100, BCHG_IMM_Dn);
    FILL_Bxxx_IMM(04200, BCLR_IMM_Dn);
    FILL_Bxxx_IMM(04300, BSET_IMM_Dn);
    FILL_BTST_IMM_EA(04000, BTST_IMM_EA);
    FILL_Bxxx_IMM_EA(04100, BCHG_IMM_EA);
    FILL_Bxxx_IMM_EA(04200, BCLR_IMM_EA);
    FILL_Bxxx_IMM_EA(04300, BSET_IMM_EA);

    #if 0

	[0xcfc]			  = { EMIT_CAS2, NULL, 0, SR_NZVC, 3, 0, 2 },
	[0xefc]			  = { EMIT_CAS2, NULL, 0, SR_NZVC, 3, 0, 4 },

	[00320 ... 00327] = { EMIT_CMP2, NULL, SR_CCR, SR_NZVC, 2, 0, 1 },
	[00350 ... 00373] = { EMIT_CMP2, NULL, SR_CCR, SR_NZVC, 2, 1, 1 },
	[01320 ... 01327] = { EMIT_CMP2, NULL, SR_CCR, SR_NZVC, 2, 0, 2 },
	[01350 ... 01373] = { EMIT_CMP2, NULL, SR_CCR, SR_NZVC, 2, 1, 2 },
	[02320 ... 02327] = { EMIT_CMP2, NULL, SR_CCR, SR_NZVC, 2, 0, 4 },
	[02350 ... 02373] = { EMIT_CMP2, NULL, SR_CCR, SR_NZVC, 2, 1, 4 },
    #endif

    return table;
}

constexpr auto InsnTable __attribute__((used,aligned(4096),section(".int.jumptable.0"))) = buildInsnTable();

} // Emu68::M68k::Interpreter::Line0
