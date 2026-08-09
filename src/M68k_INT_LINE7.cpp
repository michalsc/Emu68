#include <cstdint>
#include <arm_neon.h>
#include <array>

#include <emu68/m68k/interpreter.hpp>

#include "RegisterMapping.h"

namespace Emu68::M68k::Interpreter::Line7 {

template<uint8_t Dn>
void MOVEQ(uint32_t opcode)
{
    uint32_t sr = SR & 0xfff0;
    LONG value = (BYTE)(opcode & 0xff);
    setD<Dn, LONG>(value);
    if (value == 0) {
        sr |= SR_Z;
    } else if (value < 0) {
        sr |= SR_N;
    }
    SR = sr;
    PC += 2;
}

template <class Type, template<uint8_t, class> class Op, class DispatchF, class ImmF>
constexpr void fillRegImm(std::array<INTERPRET_Function, 4096>& table, int base)
{
    auto fillOne = [&]<std::size_t I>() {
        constexpr unsigned dispatchV = I / ImmF::size;
        constexpr unsigned immV      = I % ImmF::size;  // unused for dispatch, just widens the loop

        if constexpr (DispatchF::valid(dispatchV)) {
            int idx = base + (dispatchV << DispatchF::bitOffset) + (immV << ImmF::bitOffset);
            table[idx] = Op<DispatchF::arg(dispatchV), Type>::value;
        }
    };
    [&]<std::size_t... Is>(std::index_sequence<Is...>) {
        (fillOne.template operator()<Is>(), ...);
    }(std::make_index_sequence<DispatchF::size * ImmF::size>{});
}

template<uint8_t Dn, class Type>
struct  MOVEQ_Op { static constexpr auto value = MOVEQ<Dn>; };

static constexpr std::array<INTERPRET_Function, 4096> buildInsnTable()
{
    std::array<INTERPRET_Function, 4096> table{};
    for (auto& e : table) e = ILLEGAL;

    fillRegImm<BYTE, MOVEQ_Op, RegField<9,3>, ImmField<0,8>>(table, 0x000);

    return table;
}

constexpr auto InsnTable __attribute__((aligned(4096),section(".int.jumptable.7"))) = buildInsnTable();

#if 0
static constexpr std::array<INTERPRET_Function, 16> InsnTable = {
    MOVEQ<0>,
    ILLEGAL,
    MOVEQ<1>,
    ILLEGAL,
    MOVEQ<2>,
    ILLEGAL,
    MOVEQ<3>,
    ILLEGAL,
    MOVEQ<4>,
    ILLEGAL,
    MOVEQ<5>,
    ILLEGAL,
    MOVEQ<6>,
    ILLEGAL,
    MOVEQ<7>,
    ILLEGAL
};
#endif

} // Emu68::M68k::Interpreter::Line7

__attribute__((optimize("no-optimize-sibling-calls")))
void INTERPRET_line7(uint32_t opcode)
{
    Emu68::M68k::Interpreter::Line7::InsnTable[opcode & 4095](opcode);
}
