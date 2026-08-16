#include <cstdint>
#include <arm_neon.h>
#include <array>

#include <emu68/m68k/interpreter>

#include "RegisterMapping.h"

namespace Emu68::M68k::Interpreter::LineB {

template<uint8_t Mode, uint8_t Reg, bool IsAn, uint8_t DstReg, class Type>
void CMP(uint32_t)
{
    PC += 2;

    if constexpr (IsAn) {
        int32_t eaval  = loadFromEA<Mode, Reg, Type>();   // WORD->int32 sign-extends; no-op for LONG
        int32_t regval = getA<DstReg, int32_t>();
        auto [result, ccr] = arithWithFlags<int32_t, true>(regval, eaval);
        (void)result;
        SR = (SR & ~SR_NZVC) | ccr;
    } else {
        Type eaval  = loadFromEA<Mode, Reg, Type>();
        Type regval = getD<DstReg, Type>();
        auto [result, ccr] = arithWithFlags<Type, true>(regval, eaval);
        (void)result;
        SR = (SR & ~SR_NZVC) | ccr;
    }
}

template<uint8_t SrcReg, uint8_t DstReg, class Type>
void CMPM(uint32_t)
{
    PC += 2;
    Type src = loadFromEA<3, SrcReg, Type>();
    Type dst = loadFromEA<3, DstReg, Type>();

    auto [result, ccr] = arithWithFlags<Type, true>(dst, src);

    (void)result;
    SR = (SR & ~SR_NZVC) | ccr;
}

template<uint8_t Mode, uint8_t Reg, uint8_t Dn, class Type>
void EOR_Dn_to_EA(uint32_t)
{
    Type dval = getD<Dn, Type>();
    PC = PC + 2;

    readModifyWriteEA<Mode, Reg, Type>([&](Type v) -> Type {
        uint32_t sr = SR & ~SR_NZVC;

        v ^= dval;

        if (v == 0) {
            sr |= SR_Z;
        } else if (v < 0) {
            sr |= SR_N;
        }
        SR = sr;

        return v;
    });
}

#define FILL_ALL_CMP(size) \
[&]<std::size_t... Is>(std::index_sequence<Is...>) consteval { \
    auto fillOne = [&]<std::size_t I>() consteval { \
        constexpr std::size_t DstReg = (I >> 6) & 7; \
        constexpr std::size_t SrcMode = (I >> 3) & 7; \
        constexpr std::size_t SrcReg = I & 7; \
        if constexpr ((sizeof(size) > 1 || (SrcMode != 1)) && ((SrcMode < 7) || (SrcMode == 7 && SrcReg <= 4))) { \
            constexpr int base = ((DstReg & 7) << 9) | (sizeof(size) == 1 ? 0 : sizeof(size) == 2 ? 1 : 2) << 6; \
            table[base + EA(SrcMode, SrcReg)] = CMP<SrcMode, SrcReg, false, DstReg, size>; \
        } \
    }; \
    (fillOne.template operator()<Is>(), ...); \
}(std::make_index_sequence<8 * 64>{});

#define FILL_ALL_CMPA(size) \
[&]<std::size_t... Is>(std::index_sequence<Is...>) consteval { \
    auto fillOne = [&]<std::size_t I>() consteval { \
        constexpr std::size_t DstReg = (I >> 6) & 7; \
        constexpr std::size_t SrcMode = (I >> 3) & 7; \
        constexpr std::size_t SrcReg = I & 7; \
        if constexpr ((SrcMode < 7) || (SrcMode == 7 && SrcReg <= 4)) { \
            constexpr int base = ((DstReg & 7) << 9) | (sizeof(size) == 2 ? 3 : 7) << 6; \
            table[base + EA(SrcMode, SrcReg)] = CMP<SrcMode, SrcReg, true, DstReg, size>; \
        } \
    }; \
    (fillOne.template operator()<Is>(), ...); \
}(std::make_index_sequence<8 * 64>{});

#define FILL_ALL_WR_EAs(base_offset, name, reg, size) \
    [&]<std::size_t... Dreg>(int base, std::index_sequence<Dreg...>) { \
        ((table[base + EA(0, Dreg)] = \
            name<0, Dreg, reg, size>), ...); \
    }((base_offset), std::make_index_sequence<8>{}); \
    [&]<std::size_t... Is>(int base, std::index_sequence<Is...>) { \
        ((table[base + EA(2 + (Is >> 3), Is & 7)] = \
             name<2 + (Is >> 3), Is & 7, reg, size>), ...); \
    }((base_offset), std::make_index_sequence<42>{});

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

template <uint8_t Rx, uint8_t Ry, class Type>
struct CMPM_Op { static constexpr auto value = CMPM<Rx, Ry, Type>; };

static consteval std::array<INTERPRET_Function, 4096> buildInsnTable()
{
    std::array<INTERPRET_Function, 4096> table{};

    auto fill = [&table](int first, int last, INTERPRET_Function func) {
        for (int i = first; i <= last; ++i) {
            table[i] = func;
        }
    };

    auto EA = [](int mode, int reg) constexpr { return (mode << 3) | reg; };

    fill(00000, 07777, ILLEGAL);

    FILL_ALL_CMP(BYTE);
    FILL_ALL_CMP(WORD);
    FILL_ALL_CMP(LONG);

    FILL_ALL_CMPA(WORD);
    FILL_ALL_CMPA(LONG);

    fillRegReg<BYTE, CMPM_Op, RegField<0>, RegField<9>>(table, 00410);
    fillRegReg<WORD, CMPM_Op, RegField<0>, RegField<9>>(table, 00510);
    fillRegReg<LONG, CMPM_Op, RegField<0>, RegField<9>>(table, 00610);

    FILL_ALL_WR_EAs(        00400,  EOR_Dn_to_EA, 0,  BYTE);
    FILL_ALL_WR_EAs(        01400,  EOR_Dn_to_EA, 1,  BYTE);
    FILL_ALL_WR_EAs(        02400,  EOR_Dn_to_EA, 2,  BYTE);
    FILL_ALL_WR_EAs(        03400,  EOR_Dn_to_EA, 3,  BYTE);
    FILL_ALL_WR_EAs(        04400,  EOR_Dn_to_EA, 4,  BYTE);
    FILL_ALL_WR_EAs(        05400,  EOR_Dn_to_EA, 5,  BYTE);
    FILL_ALL_WR_EAs(        06400,  EOR_Dn_to_EA, 6,  BYTE);
    FILL_ALL_WR_EAs(        07400,  EOR_Dn_to_EA, 7,  BYTE);

    FILL_ALL_WR_EAs(        00500,  EOR_Dn_to_EA, 0,  WORD);
    FILL_ALL_WR_EAs(        01500,  EOR_Dn_to_EA, 1,  WORD);
    FILL_ALL_WR_EAs(        02500,  EOR_Dn_to_EA, 2,  WORD);
    FILL_ALL_WR_EAs(        03500,  EOR_Dn_to_EA, 3,  WORD);
    FILL_ALL_WR_EAs(        04500,  EOR_Dn_to_EA, 4,  WORD);
    FILL_ALL_WR_EAs(        05500,  EOR_Dn_to_EA, 5,  WORD);
    FILL_ALL_WR_EAs(        06500,  EOR_Dn_to_EA, 6,  WORD);
    FILL_ALL_WR_EAs(        07500,  EOR_Dn_to_EA, 7,  WORD);

    FILL_ALL_WR_EAs(        00600,  EOR_Dn_to_EA, 0,  LONG);
    FILL_ALL_WR_EAs(        01600,  EOR_Dn_to_EA, 1,  LONG);
    FILL_ALL_WR_EAs(        02600,  EOR_Dn_to_EA, 2,  LONG);
    FILL_ALL_WR_EAs(        03600,  EOR_Dn_to_EA, 3,  LONG);
    FILL_ALL_WR_EAs(        04600,  EOR_Dn_to_EA, 4,  LONG);
    FILL_ALL_WR_EAs(        05600,  EOR_Dn_to_EA, 5,  LONG);
    FILL_ALL_WR_EAs(        06600,  EOR_Dn_to_EA, 6,  LONG);
    FILL_ALL_WR_EAs(        07600,  EOR_Dn_to_EA, 7,  LONG);

    return table;
}

constexpr auto InsnTable __attribute__((used,aligned(4096),section(".int.jumptable.b"))) = buildInsnTable();

} // Emu68::M68k::Interpreter::LineB
