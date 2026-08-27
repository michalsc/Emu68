#include <cstdint>
#include <arm_neon.h>
#include <array>

#include <emu68/m68k/interpreter>

#include "RegisterMapping.h"

namespace Emu68::M68k::Interpreter::LineE {

template<uint8_t Mode, uint8_t Reg>
void ROR_EA(uint32_t)
{
    advancePC(2);

    readModifyWriteEA<Mode, Reg, uint16_t>([&](uint16_t v) -> uint16_t {
        uint32_t sr = SR & ~SR_NZVC;
        
        v = (v >> 1) | (v << 15);
        
        if (v == 0) {
            sr |= SR_Z;
        } else if ((int16_t)v < 0) {
            sr |= SR_N;
            sr |= SR_Calt;
        }
        SR = sr;

        return v;
    });
}

template<uint8_t Mode, uint8_t Reg>
void ROL_EA(uint32_t)
{
    advancePC(2);

    readModifyWriteEA<Mode, Reg, uint16_t>([&](uint16_t v) -> uint16_t {
        uint32_t sr = SR & ~SR_NZVC;
        
        v = (v << 1) | (v >> 15);
        
        if (v == 0) {
            sr |= SR_Z;
        } else if ((int16_t)v < 0) {
            sr |= SR_N;
        }
        if (v & 1) {
            sr |= SR_Calt;
        }
        SR = sr;

        return v;
    });
}

template<uint8_t Dn, class Type>
void ROR_Reg(uint32_t opcode)
{
    uint32_t sr = SR & ~SR_NZVC;
    uint32_t count = getD<uint32_t>((opcode >> 9) & 7) & 63;
    constexpr uint32_t bitcount = sizeof(Type) * 8;

    Type v = getD<Dn, Type>();

    if (v == 0) {
        sr |= SR_Z;
    } else {
        if (count > 0) {
            count &= (bitcount - 1);

            v = (v >> count) | (v << (bitcount - count));

            if (v & (1 << (bitcount - 1))) {
                sr |= SR_N | SR_Calt;
            }

            setD<Dn, Type>(v);
        } else {
            if (v & (1 << (bitcount - 1))) {
                sr |= SR_N;
            }
        }
    }

    SR = sr;
    advancePC(2);
}


template<uint8_t Dn, class Type>
void ROR_IMM(uint32_t opcode)
{
    uint32_t sr = SR & ~SR_NZVC;
    uint32_t count = (opcode >> 9) & 7;
    constexpr uint32_t bitcount = sizeof(Type) * 8;

    if (count == 0) count = 8;

    Type v = getD<Dn, Type>();

    if (v == 0) {
        sr |= SR_Z;
    } else {
        v = (v >> count) | (v << (bitcount - count));

        if (v & (1 << (bitcount - 1))) {
            sr |= SR_N | SR_Calt;
        }

        setD<Dn, Type>(v);
    }

    SR = sr;
    advancePC(2);
}

template<uint8_t Dn, class Type>
void ROL_Reg(uint32_t opcode)
{
    uint32_t sr = SR & ~SR_NZVC;
    uint32_t count = getD<uint32_t>((opcode >> 9) & 7) & 63;
    constexpr uint32_t bitcount = sizeof(Type) * 8;

    Type v = getD<Dn, Type>();

    if (v == 0) {
        sr |= SR_Z;
    } else {
        if (count > 0) {
            count = count & (bitcount - 1);

            v = (v << count) | (v >> (bitcount - count));

            if (v & (1 << (bitcount - 1))) {
                sr |= SR_N;
            }
            if (v & 1) {
                sr |= SR_Calt;
            }

            setD<Dn, Type>(v);
        } else if (v & (1 << (bitcount - 1))) {
            sr |= SR_N;
        }
    }

    SR = sr;
    advancePC(2);
}

template<uint8_t Dn, class Type>
void ROL_IMM(uint32_t opcode)
{
    uint32_t sr = SR & ~SR_NZVC;
    uint32_t count = (opcode >> 9) & 7;
    constexpr uint32_t bitcount = sizeof(Type) * 8;

    if (count == 0) count = 8;

    Type v = getD<Dn, Type>();

    if (v == 0) {
        sr |= SR_Z;
    } else {
        v = (v << count) | (v >> (bitcount - count));

        if (v & (1 << (bitcount - 1))) {
            sr |= SR_N;
        }
        if (v & 1) {
            sr |= SR_Calt;
        }

        setD<Dn, Type>(v);
    }

    SR = sr;
    advancePC(2);
}

template<bool Left, bool RegCount, uint8_t CountOrReg, uint8_t Reg, class Type>
void ASx_Reg(uint32_t)
{
    Type v = getD<Reg, Type>();
    
    advancePC(2);


    int count;
    if constexpr (RegCount) { count = getD<CountOrReg, uint32_t>() & 63; }
    else                    { count = (CountOrReg == 0) ? 8 : CountOrReg; }
    
    commitPC();

    auto [result, ccr] = [&] {
        if constexpr (Left) { return aslWithFlags<Type>(v, count); }
        else                { return asrWithFlags<Type>(v, count); }
    }();

    if constexpr (!RegCount) {
        // immediate count is always 1..8 -- X always updated, same shape as ADD
        SR = (SR & ~(SR_X | SR_NZVC)) | ccr | ((ccr & SR_Calt) ? SR_X : 0);
    } else {
        uint16_t mask = SR_NZVC | (count != 0 ? SR_X : 0);
        uint16_t bits = ccr | ((count != 0 && (ccr & SR_Calt)) ? SR_X : 0);
        SR = (SR & ~mask) | bits;
    }

    setD<Reg, Type>(result);
}

template<uint8_t Mode, uint8_t Reg, bool Left, class Type>
void ASx_Mem(uint32_t)
{
    advancePC(2);

    readModifyWriteEA<Mode, Reg, Type>([&](Type v) -> Type {
        auto [result, ccr] = Left ? aslWithFlags<Type>(v, 1) : asrWithFlags<Type>(v, 1);
        SR = (SR & ~(SR_X | SR_NZVC)) | ccr | ((ccr & SR_Calt) ? SR_X : 0);
        return result;
    });
}


template<bool Left, bool RegCount, uint8_t CountOrReg, uint8_t Reg, class Type>
void LSx_Reg(uint32_t)
{
    Type v = getD<Reg, Type>();

    advancePC(2);
    commitPC();

    int count;
    if constexpr (RegCount) { count = getD<CountOrReg, uint32_t>() & 63; }
    else                    { count = (CountOrReg == 0) ? 8 : CountOrReg; }

    auto [result, ccr] = [&] {
        if constexpr (Left) { return lslWithFlags<Type>(v, count); }
        else                { return lsrWithFlags<Type>(v, count); }
    }();

    if constexpr (!RegCount) {
        // immediate count is always 1..8 -- X always updated, same shape as ADD
        SR = (SR & ~(SR_X | SR_NZVC)) | ccr | ((ccr & SR_Calt) ? SR_X : 0);
    } else {
        uint16_t mask = SR_NZVC | (count != 0 ? SR_X : 0);
        uint16_t bits = ccr | ((count != 0 && (ccr & SR_Calt)) ? SR_X : 0);
        SR = (SR & ~mask) | bits;
    }

    setD<Reg, Type>(result);
}

template<uint8_t Mode, uint8_t Reg, bool Left, class Type>
void LSx_Mem(uint32_t)
{
    advancePC(2);
    commitPC();

    readModifyWriteEA<Mode, Reg, Type>([&](Type v) -> Type {
        auto [result, ccr] = Left ? lslWithFlags<Type>(v, 1) : lsrWithFlags<Type>(v, 1);
        SR = (SR & ~(SR_X | SR_NZVC)) | ccr | ((ccr & SR_Calt) ? SR_X : 0);
        return result;
    });
}

template<class Type>
static inline constexpr std::pair<Type, bool> roxRotate(Type v, uint32_t count, bool x_in, bool left)
{
    constexpr uint32_t bitcount = sizeof(Type) * 8;
    constexpr uint32_t ringbits = bitcount + 1;
    constexpr uint64_t valmask = (uint64_t(1) << bitcount) - 1;
    constexpr uint64_t ringmask = (uint64_t(1) << ringbits) - 1;
 
    uint64_t ring = (uint64_t(x_in) << bitcount) | (v & valmask);
    uint32_t c = count % ringbits;

    uint64_t rotated = ring;
    if (c != 0) {
        rotated = left
            ? ((ring << c) | (ring >> (ringbits - c))) & ringmask
            : ((ring >> c) | (ring << (ringbits - c))) & ringmask;
    }

    bool new_x = (rotated >> bitcount) & 1;
    Type result = static_cast<Type>(rotated & valmask);
    return { result, new_x };
}

template<class Type>
static inline constexpr std::pair<Type, uint32_t> roxWithFlags(Type v, uint32_t count, bool x_in, bool left)
{
    constexpr uint32_t bitcount = sizeof(Type) * 8;
    auto [result, new_x] = roxRotate<Type>(v, count, x_in, left);
 
    uint32_t ccr = 0;
    if (result == 0) {
        ccr |= SR_Z;
    } else if (result & (Type(1) << (bitcount - 1))) {
        ccr |= SR_N;
    }
    if (new_x) {
        ccr |= SR_Calt;
    }
 
    return { result, ccr };
}

template<bool Left, bool RegCount, uint8_t CountOrReg, uint8_t Reg, class Type>
void ROXx_Reg(uint32_t)
{
    Type v = getD<Reg, Type>();
    bool x_in = (SR & SR_X) != 0;
 
    advancePC(2);
    commitPC();

    uint32_t count;
    if constexpr (RegCount) { count = getD<CountOrReg, uint32_t>() & 63; }
    else                    { count = (CountOrReg == 0) ? 8 : CountOrReg; }

    auto [result, ccr] = roxWithFlags<Type>(v, count, x_in, Left);

    if constexpr (!RegCount) {
        // immediate count is always 1..8 -- X always updated, same shape as ASx_Reg/LSx_Reg
        SR = (SR & ~(SR_X | SR_NZVC)) | ccr | ((ccr & SR_Calt) ? SR_X : 0);
    } else {
        uint32_t mask = SR_NZVC | (count != 0 ? SR_X : 0);
        uint32_t bits = ccr | ((count != 0 && (ccr & SR_Calt)) ? SR_X : 0);
        SR = (SR & ~mask) | bits;
    }
 
    setD<Reg, Type>(result);
}

template<uint8_t Mode, uint8_t Reg>
void ROXR_Mem(uint32_t)
{
    advancePC(2);
    commitPC();

    readModifyWriteEA<Mode, Reg, uint16_t>([&](uint16_t v) -> uint16_t {
        bool x_in = (SR & SR_X) != 0;
        auto [result, ccr] = roxWithFlags<uint16_t>(v, 1, x_in, false);
        SR = (SR & ~(SR_X | SR_NZVC)) | ccr | ((ccr & SR_Calt) ? SR_X : 0);
        return result;
    });
}

template<uint8_t Mode, uint8_t Reg>
void ROXL_Mem(uint32_t)
{
    advancePC(2);
    commitPC();
    
    readModifyWriteEA<Mode, Reg, uint16_t>([&](uint16_t v) -> uint16_t {
        bool x_in = (SR & SR_X) != 0;
        auto [result, ccr] = roxWithFlags<uint16_t>(v, 1, x_in, true);
        SR = (SR & ~(SR_X | SR_NZVC)) | ccr | ((ccr & SR_Calt) ? SR_X : 0);
        return result;
    });
}

struct BitFieldOperand {
    uint64_t aligned, raw;
    bool dn_mode;
    uint32_t ea, operand_reg;
    int32_t offset, bit_pos;   // offset: as decoded (BFFFO's result needs it); bit_pos: reduced for addressing
    int width, fetch_bytes, ext_reg;
    uint32_t sr;
};

template<uint8_t Mode, uint8_t Reg>
BitFieldOperand resolveBitField(uint32_t opcode)
{
    BitFieldOperand op{};
    uint16_t opcode2 = *getPC<uint16_t*>(2);

    advancePC(4);   // must precede any getEA call below - it consumes further extension words
    commitPC();

    op.ext_reg = (opcode2 >> 12) & 7;
    op.offset  = (opcode2 & (1 << 11)) ? getD<int32_t>((opcode2 >> 6) & 7) : (opcode2 >> 6) & 31;
    op.width   = (opcode2 & (1 << 5))  ? getD<uint32_t>(opcode2 & 7) & 31  : opcode2 & 31;
    if (op.width == 0) { op.width = 32; }
    op.bit_pos = op.offset;

    if constexpr (Mode == DEFAULT_EA || Reg == DEFAULT_EA) {
        int mode = (Mode == DEFAULT_EA) ? (opcode >> 3) & 7 : Mode;
        int reg = (Reg == DEFAULT_EA) ? opcode & 7 : Reg;
        if (mode == 0) {
            op.operand_reg = reg;
            op.raw = (uint64_t)getD<uint32_t>(reg) << 32;
            op.bit_pos &= 31;
            op.dn_mode = true;
        } else {
            op.ea = getEA<void>(mode, reg);
        }
    } else if constexpr (Mode == 0) {
        op.operand_reg = Reg;
        op.raw = (uint64_t)getD<Reg, uint32_t>() << 32;
        op.bit_pos &= 31;
        op.dn_mode = true;
    } else {
        op.ea = getEA<Mode, Reg, void>();
    }

    if (!op.dn_mode) {
        op.ea += op.bit_pos >> 3;
        op.bit_pos &= 7;
        int total = op.bit_pos + op.width;
        op.fetch_bytes = (total > 32) ? 8 : (total > 16) ? 4 : (total > 8) ? 2 : 1;
        switch (op.fetch_bytes) {
            case 8: op.raw = *(uint64_t*)(uintptr_t)op.ea; break;
            case 4: op.raw = (uint64_t)*(uint32_t*)(uintptr_t)op.ea << 32; break;
            case 2: op.raw = (uint64_t)*(uint16_t*)(uintptr_t)op.ea << 48; break;
            default: op.raw = (uint64_t)*(uint8_t*)(uintptr_t)op.ea << 56; break;
        }
    }

    op.aligned = op.dn_mode ? ((op.raw << op.bit_pos) | (op.raw >> (32 - op.bit_pos)))
                             : (op.raw << op.bit_pos);

    uint32_t sr = SR & ~SR_NZVC;
    if ((int64_t)op.aligned < 0) { sr |= SR_N; }
    if ((op.aligned >> (64 - op.width)) == 0) { sr |= SR_Z; }
    op.sr = sr;

    return op;
}

void commitBitField(const BitFieldOperand& op, uint32_t new_field)
{
    uint64_t mask = ~0ULL << (64 - op.width);
    uint64_t new_aligned = (uint64_t)new_field << (64 - op.width);

    if (op.dn_mode) {
        uint32_t rotated = (uint32_t)(op.aligned >> 32);
        uint32_t mask32 = (uint32_t)(mask >> 32);
        uint32_t merged = (rotated & ~mask32) | ((uint32_t)(new_aligned >> 32) & mask32);
        uint32_t result = (merged >> op.bit_pos) | (merged << ((32 - op.bit_pos) & 31));
        setD<uint32_t>(op.operand_reg, result);
        return;
    }

    uint64_t raw_mask = mask >> op.bit_pos;
    uint64_t merged = (op.raw & ~raw_mask) | ((new_aligned >> op.bit_pos) & raw_mask);
    switch (op.fetch_bytes) {
        case 8: *(uint64_t*)(uintptr_t)op.ea = merged; break;
        case 4: *(uint32_t*)(uintptr_t)op.ea = (uint32_t)(merged >> 32); break;
        case 2: *(uint16_t*)(uintptr_t)op.ea = (uint16_t)(merged >> 48); break;
        default: *(uint8_t*)(uintptr_t)op.ea = (uint8_t)(merged >> 56); break;
    }
}

template<uint8_t Mode, uint8_t Reg, bool Signed>
void BFEXT(uint32_t opcode)
{
    BitFieldOperand op = resolveBitField<Mode, Reg>(opcode);
    uint32_t result;

    if constexpr (Signed) {
        result = (uint32_t)((int64_t)op.aligned >> (64 - op.width));
    } else {
        result = (uint32_t)(op.aligned >> (64 - op.width));
    }

    setD<uint32_t>(op.ext_reg, result);
    SR = op.sr;
}

template<uint8_t Mode, uint8_t Reg>
void BFTST(uint32_t opcode)
{
    BitFieldOperand op = resolveBitField<Mode, Reg>(opcode);
    SR = op.sr;
}

template<uint8_t Mode, uint8_t Reg>
void BFCHG(uint32_t opcode)
{
    BitFieldOperand op = resolveBitField<Mode, Reg>(opcode);
    uint32_t old_field = (uint32_t)(op.aligned >> (64 - op.width));
    commitBitField(op, ~old_field);
    SR = op.sr;
}

template<uint8_t Mode, uint8_t Reg>
void BFCLR(uint32_t opcode)
{
    BitFieldOperand op = resolveBitField<Mode, Reg>(opcode);
    commitBitField(op, 0);
    SR = op.sr;
}

template<uint8_t Mode, uint8_t Reg>
void BFSET(uint32_t opcode)
{
    BitFieldOperand op = resolveBitField<Mode, Reg>(opcode);
    commitBitField(op, 0xFFFFFFFFu);
    SR = op.sr;
}

template<uint8_t Mode, uint8_t Reg>
void BFFFO(uint32_t opcode)
{
    BitFieldOperand op = resolveBitField<Mode, Reg>(opcode);
    uint64_t field_mask = ~0ULL << (64 - op.width);
    uint64_t masked = op.aligned & field_mask;

    uint32_t result;
    if (masked == 0) {
        result = (uint32_t)(op.offset + op.width);
    } else {
        int leading = __builtin_clzll(masked);
        result = (uint32_t)(op.offset + leading);
    }

    setD<uint32_t>(op.ext_reg, result);
    SR = op.sr;
}

template<uint8_t Mode, uint8_t Reg>
void BFINS(uint32_t opcode)
{
    BitFieldOperand op = resolveBitField<Mode, Reg>(opcode);
    uint32_t insert = getD<uint32_t>(op.ext_reg);

    /* N/Z come from the inserted value, not the field being overwritten */
    uint32_t sr = SR & ~SR_NZVC;
    if (op.width < 32) {
        uint32_t field = insert & ((1u << op.width) - 1);
        if (field & (1u << (op.width - 1))) { sr |= SR_N; }
        if (field == 0) { sr |= SR_Z; }
    } else {
        if (insert & 0x80000000u) { sr |= SR_N; }
        if (insert == 0) { sr |= SR_Z; }
    }

    commitBitField(op, insert);
    SR = sr;
}

#define FILL_MOD2_to_72(base_offset, name) \
    [&]<std::size_t... Is>(int base, std::index_sequence<Is...>) { \
        ((table[base + EA(2 + (Is >> 3), Is & 7)] = \
            name<2 + (Is >> 3), Is & 7>), ...); \
    }((base_offset), std::make_index_sequence<42>{});

#define FILL_MOD2_to_72_sz(base_offset, name, opt, size) \
    [&]<std::size_t... Is>(int base, std::index_sequence<Is...>) { \
        ((table[base + EA(2 + (Is >> 3), Is & 7)] = \
            name<2 + (Is >> 3), Is & 7, opt, size>), ...); \
    }((base_offset), std::make_index_sequence<42>{});

#define FILL_REG_CNT(base_offset, name, type) \
    [&]<std::size_t... Is>(int base, std::index_sequence<Is...>) { \
        ((table[base + (Is & 7) + ((Is >> 3) << 9)] = \
            name<Is & 7, type>), ...); \
    }((base_offset), std::make_index_sequence<64>{});

#define FILL_SHIFT_REG(base_offset, name, left, regcount, size) \
    [&]<std::size_t... Is>(int base, std::index_sequence<Is...>) { \
        ((table[base + ((Is >> 3) << 9) + (Is & 7)] = \
             name<left, regcount, (Is >> 3), Is & 7, size>), ...); \
    }((base_offset), std::make_index_sequence<64>{});

#define FILL_BFxxx_RO(base_offset, name, sign) \
    [&]<std::size_t... Dn>(int base, std::index_sequence<Dn...>) { \
        ((table[base + Dn] = \
             name<0, Dn, sign>), ...); \
    }((base_offset), std::make_index_sequence<8>{}); \
    [&]<std::size_t... An>(int base, std::index_sequence<An...>) { \
        ((table[base + (2 << 3) + An] = \
             name<2, An, sign>), ...); \
    }((base_offset), std::make_index_sequence<8>{}); \
    [&]<std::size_t... Is>(int base, std::index_sequence<Is...>) { \
        ((table[base + ((5 + (Is >> 3)) << 3) + (Is & 7)] = \
             name<5 + (Is >> 3), Is & 7, sign>), ...); \
    }((base_offset), std::make_index_sequence<20>{}); \

#define FILL_BFxxx_RW(base_offset, name, sign) \
    [&]<std::size_t... Dn>(int base, std::index_sequence<Dn...>) { \
        ((table[base + Dn] = \
             name<0, Dn, sign>), ...); \
    }((base_offset), std::make_index_sequence<8>{}); \
    [&]<std::size_t... An>(int base, std::index_sequence<An...>) { \
        ((table[base + (2 << 3) + An] = \
             name<2, An, sign>), ...); \
    }((base_offset), std::make_index_sequence<8>{}); \
    [&]<std::size_t... Is>(int base, std::index_sequence<Is...>) { \
        ((table[base + ((5 + (Is >> 3)) << 3) + (Is & 7)] = \
             name<5 + (Is >> 3), Is & 7, sign>), ...); \
    }((base_offset), std::make_index_sequence<18>{}); \

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

template <class Type, bool Write, template<uint8_t,uint8_t> class Op,
          class EAF>
constexpr void fillEA(std::array<INTERPRET_Function, 4096>& table, int base)
{
    auto fillOne = [&]<std::size_t I>() {
        constexpr unsigned eaV  = I;

        if constexpr (EAF::template valid<eaV>()) {
            int idx = base + (eaV << EAF::bitOffset);
            table[idx] = Op<EAF::modeArg(eaV), EAF::regArg(eaV)>::value;
        }
    };
    [&]<std::size_t... Is>(std::index_sequence<Is...>) {
        (fillOne.template operator()<Is>(), ...);
    }(std::make_index_sequence<EAF::size>{});
}

inline constexpr uint32_t BitFieldOpSpecializeMask = 
      classBit(EAClass::Dn);

inline constexpr uint32_t BitFieldValidMask = 
      classBit(EAClass::Dn)     | classBit(EAClass::Ind)
    | classBit(EAClass::D16An)  | classBit(EAClass::D8AnXn)
    | classBit(EAClass::AbsW)   | classBit(EAClass::AbsL)
    | classBit(EAClass::D16PC)  | classBit(EAClass::D8PCXn);

template <uint8_t Mode, uint8_t Reg>
struct BFEXTU_Op { static constexpr auto value = BFEXT<Mode, Reg, false>; };

template <uint8_t Mode, uint8_t Reg>
struct BFEXTS_Op { static constexpr auto value = BFEXT<Mode, Reg, true>; };

template <uint8_t Mode, uint8_t Reg>
struct BFTST_Op { static constexpr auto value = BFTST<Mode, Reg>; };

template <uint8_t Mode, uint8_t Reg>
struct BFCHG_Op { static constexpr auto value = BFCHG<Mode, Reg>; };

template <uint8_t Mode, uint8_t Reg>
struct BFCLR_Op { static constexpr auto value = BFCLR<Mode, Reg>; };

template <uint8_t Mode, uint8_t Reg>
struct BFSET_Op { static constexpr auto value = BFSET<Mode, Reg>; };

template <uint8_t Mode, uint8_t Reg>
struct BFFFO_Op { static constexpr auto value = BFFFO<Mode, Reg>; };

template <uint8_t Mode, uint8_t Reg>
struct BFINS_Op { static constexpr auto value = BFINS<Mode, Reg>; };

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

    FILL_MOD2_to_72(    03300,  ROR_EA);
    FILL_MOD2_to_72(    03700,  ROL_EA);

    FILL_REG_CNT(       00030,  ROR_IMM, uint8_t);
    FILL_REG_CNT(       00130,  ROR_IMM, uint16_t);
    FILL_REG_CNT(       00230,  ROR_IMM, uint32_t);
    FILL_REG_CNT(       00430,  ROL_IMM, uint8_t);
    FILL_REG_CNT(       00530,  ROL_IMM, uint16_t);
    FILL_REG_CNT(       00630,  ROL_IMM, uint32_t);

    FILL_REG_CNT(       00070,  ROR_Reg, uint8_t);
    FILL_REG_CNT(       00170,  ROR_Reg, uint16_t);
    FILL_REG_CNT(       00270,  ROR_Reg, uint32_t);
    FILL_REG_CNT(       00470,  ROL_Reg, uint8_t);
    FILL_REG_CNT(       00570,  ROL_Reg, uint16_t);
    FILL_REG_CNT(       00670,  ROL_Reg, uint32_t);

    FILL_SHIFT_REG(     00000,  ASx_Reg, false, false, BYTE);   // ASR.B #n,Dn
    FILL_SHIFT_REG(     00040,  ASx_Reg, false, true,  BYTE);   // ASR.B Dn,Dn
    FILL_SHIFT_REG(     00100,  ASx_Reg, false, false, WORD);
    FILL_SHIFT_REG(     00140,  ASx_Reg, false, true,  WORD);
    FILL_SHIFT_REG(     00200,  ASx_Reg, false, false, LONG);
    FILL_SHIFT_REG(     00240,  ASx_Reg, false, true,  LONG);
    FILL_SHIFT_REG(     00400,  ASx_Reg, true,  false, BYTE);   // ASL.B #n,Dn
    FILL_SHIFT_REG(     00440,  ASx_Reg, true,  true,  BYTE);
    FILL_SHIFT_REG(     00500,  ASx_Reg, true,  false, WORD);
    FILL_SHIFT_REG(     00540,  ASx_Reg, true,  true,  WORD);
    FILL_SHIFT_REG(     00600,  ASx_Reg, true,  false, LONG);
    FILL_SHIFT_REG(     00640,  ASx_Reg, true,  true,  LONG);

    FILL_MOD2_to_72_sz( 00300,  ASx_Mem, false, WORD);   // ASR <ea>
    FILL_MOD2_to_72_sz( 00700,  ASx_Mem, true,  WORD);   // ASL <ea>

    FILL_SHIFT_REG(     00010,  LSx_Reg, false, false, BYTE);   // ASR.B #n,Dn
    FILL_SHIFT_REG(     00050,  LSx_Reg, false, true,  BYTE);   // ASR.B Dn,Dn
    FILL_SHIFT_REG(     00110,  LSx_Reg, false, false, WORD);
    FILL_SHIFT_REG(     00150,  LSx_Reg, false, true,  WORD);
    FILL_SHIFT_REG(     00210,  LSx_Reg, false, false, LONG);
    FILL_SHIFT_REG(     00250,  LSx_Reg, false, true,  LONG);
    FILL_SHIFT_REG(     00410,  LSx_Reg, true,  false, BYTE);   // ASL.B #n,Dn
    FILL_SHIFT_REG(     00450,  LSx_Reg, true,  true,  BYTE);
    FILL_SHIFT_REG(     00510,  LSx_Reg, true,  false, WORD);
    FILL_SHIFT_REG(     00550,  LSx_Reg, true,  true,  WORD);
    FILL_SHIFT_REG(     00610,  LSx_Reg, true,  false, LONG);
    FILL_SHIFT_REG(     00650,  LSx_Reg, true,  true,  LONG);

    FILL_MOD2_to_72_sz( 01300,  LSx_Mem, false, WORD);          // ASR <ea>
    FILL_MOD2_to_72_sz( 01700,  LSx_Mem, true,  WORD);          // ASL <ea>

    FILL_SHIFT_REG(     00020,  ROXx_Reg, false, false, BYTE);  // ROXR.B #n,Dn
    FILL_SHIFT_REG(     00060,  ROXx_Reg, false, true,  BYTE);  // ROXR.B Dn,Dn
    FILL_SHIFT_REG(     00120,  ROXx_Reg, false, false, WORD);
    FILL_SHIFT_REG(     00160,  ROXx_Reg, false, true,  WORD);
    FILL_SHIFT_REG(     00220,  ROXx_Reg, false, false, LONG);
    FILL_SHIFT_REG(     00260,  ROXx_Reg, false, true,  LONG);
    FILL_SHIFT_REG(     00420,  ROXx_Reg, true,  false, BYTE);  // ROXL.B #n,Dn
    FILL_SHIFT_REG(     00460,  ROXx_Reg, true,  true,  BYTE);
    FILL_SHIFT_REG(     00520,  ROXx_Reg, true,  false, WORD);
    FILL_SHIFT_REG(     00560,  ROXx_Reg, true,  true,  WORD);
    FILL_SHIFT_REG(     00620,  ROXx_Reg, true,  false, LONG);
    FILL_SHIFT_REG(     00660,  ROXx_Reg, true,  true,  LONG);

    FILL_MOD2_to_72(    02300,  ROXR_Mem);
    FILL_MOD2_to_72(    02700,  ROXL_Mem);

    fillEA<LONG, false, BFEXTU_Op, EAField<0, LONG, false, BitFieldValidMask, BitFieldOpSpecializeMask>>(table, 04700);
    fillEA<LONG, false, BFEXTS_Op, EAField<0, LONG, false, BitFieldValidMask, BitFieldOpSpecializeMask>>(table, 05700);
    fillEA<LONG, false, BFTST_Op,  EAField<0, LONG, false, BitFieldValidMask, BitFieldOpSpecializeMask>>(table, 04300);
    fillEA<LONG, false, BFFFO_Op,  EAField<0, LONG, false, BitFieldValidMask, BitFieldOpSpecializeMask>>(table, 06700);

    fillEA<LONG, true,  BFCHG_Op,  EAField<0, LONG, true,  BitFieldValidMask, BitFieldOpSpecializeMask>>(table, 05300);
    fillEA<LONG, true,  BFCLR_Op,  EAField<0, LONG, true,  BitFieldValidMask, BitFieldOpSpecializeMask>>(table, 06300);
    fillEA<LONG, true,  BFSET_Op,  EAField<0, LONG, true,  BitFieldValidMask, BitFieldOpSpecializeMask>>(table, 07300);
    fillEA<LONG, true,  BFINS_Op,  EAField<0, LONG, true,  BitFieldValidMask, BitFieldOpSpecializeMask>>(table, 07700);

    return table;
}

constexpr auto InsnTable __attribute__((used,aligned(4096),section(".int.jumptable.e"))) = buildInsnTable();

} // Emu68::M68k::Interpreter::LineE
