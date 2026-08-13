#include <cstdint>
#include <arm_neon.h>
#include <array>

#include <emu68/m68k/interpreter.hpp>

#include "RegisterMapping.h"


extern "C" {

void M68K_LoadContext(struct M68KState *ctx);
void M68K_SaveContext(struct M68KState *ctx);

}

namespace Emu68::M68k::Interpreter::Line6 {

static inline struct M68KState *getCTX()
{
    struct M68KState *ctx;
    __asm__ volatile("mov %0, " CTX_POINTER_ASM:"=r"(ctx));
    return ctx;
}

template<class Type>
void BSR(uint32_t opcode)
{
    int32_t bra_off = (int8_t)(opcode & 0xff);
    uint32_t pc = PC + 2;
    uint32_t ret_pc = pc;
    uint32_t ptr = getA<7, uint32_t>() - 4;

    if constexpr (sizeof(Type) == 2) {
        bra_off = *(int16_t*)(uintptr_t)pc;
        ret_pc += 2;
    } else if constexpr (sizeof(Type) == 4) {
        bra_off = *(int32_t*)(uintptr_t)pc;
        ret_pc += 4;
    }

    pc += bra_off;
    PC = pc;
    *(uint32_t *)(uintptr_t)ptr = ret_pc;
    setA<7>(ptr);
}

template<uint8_t InstCC, class Type>
void Bcc(uint32_t opcode)
{
    int32_t bra_off = (int8_t)(opcode & 0xff);
    uint32_t pc = PC + 2;
    uint32_t next_pc = pc;

    if constexpr (sizeof(Type) == 2) {
        bra_off = *(int16_t*)(uintptr_t)pc;
        next_pc += 2;
    } else if constexpr (sizeof(Type) == 4) {
        bra_off = *(int32_t*)(uintptr_t)pc;
        next_pc += 4;
    }

    if (evalCond<InstCC>()) {
        pc += bra_off;
    } else {
        pc = next_pc;
    }

    PC = pc;
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

template <class Type, template<class> class Op, class ImmF>
constexpr void fillImm(std::array<INTERPRET_Function, 4096>& table, int base)
{
    auto fillOne = [&]<std::size_t I>() {
        constexpr unsigned immV      = I;
        int idx = base + (immV << ImmF::bitOffset);
        table[idx] = Op<Type>::value;
    };
    [&]<std::size_t... Is>(std::index_sequence<Is...>) {
        (fillOne.template operator()<Is>(), ...);
    }(std::make_index_sequence<ImmF::size>{});
}

template<uint8_t InstCC, class Type>
struct  Bcc_Op { static constexpr auto value = Bcc<InstCC, Type>; };

template<class Type>
struct  BSR_Op { static constexpr auto value = BSR<Type>; };

static constexpr std::array<INTERPRET_Function, 4096> buildInsnTable()
{
    std::array<INTERPRET_Function, 4096> table{};
    for (auto& e : table) e = ILLEGAL;

    fillRegImm<BYTE, Bcc_Op, RegField<8,4>, ImmField<0,8>>(table, 0x000);
    fillImm<BYTE, BSR_Op, ImmField<0,8>>(table, 0x100);

    /* Special fill - SHORT branches, **must** follow after generic fill */
    table[0x000] = Bcc<M_CC_T, WORD>;
    table[0x100] = BSR<WORD>;
    table[0x200] = Bcc<M_CC_HI, WORD>;
    table[0x300] = Bcc<M_CC_LS, WORD>;
    table[0x400] = Bcc<M_CC_CC, WORD>;
    table[0x500] = Bcc<M_CC_CS, WORD>;
    table[0x600] = Bcc<M_CC_NE, WORD>;
    table[0x700] = Bcc<M_CC_EQ, WORD>;
    table[0x800] = Bcc<M_CC_VC, WORD>;
    table[0x900] = Bcc<M_CC_VS, WORD>;
    table[0xa00] = Bcc<M_CC_PL, WORD>;
    table[0xb00] = Bcc<M_CC_MI, WORD>;
    table[0xc00] = Bcc<M_CC_GE, WORD>;
    table[0xd00] = Bcc<M_CC_LT, WORD>;
    table[0xe00] = Bcc<M_CC_GT, WORD>;
    table[0xf00] = Bcc<M_CC_LE, WORD>;

    /* Special fill - LONG branches, **must** follow after generic fill */
    table[0x0ff] = Bcc<M_CC_T, WORD>;
    table[0x1ff] = BSR<LONG>;
    table[0x2ff] = Bcc<M_CC_HI, LONG>;
    table[0x3ff] = Bcc<M_CC_LS, LONG>;
    table[0x4ff] = Bcc<M_CC_CC, LONG>;
    table[0x5ff] = Bcc<M_CC_CS, LONG>;
    table[0x6ff] = Bcc<M_CC_NE, LONG>;
    table[0x7ff] = Bcc<M_CC_EQ, LONG>;
    table[0x8ff] = Bcc<M_CC_VC, LONG>;
    table[0x9ff] = Bcc<M_CC_VS, LONG>;
    table[0xaff] = Bcc<M_CC_PL, LONG>;
    table[0xbff] = Bcc<M_CC_MI, LONG>;
    table[0xcff] = Bcc<M_CC_GE, LONG>;
    table[0xdff] = Bcc<M_CC_LT, LONG>;
    table[0xeff] = Bcc<M_CC_GT, LONG>;
    table[0xfff] = Bcc<M_CC_LE, LONG>;

    return table;
}

constexpr auto InsnTable __attribute__((used,aligned(4096),section(".int.jumptable.6"))) = buildInsnTable();

} // Emu68::M68k::Interpreter::Line6
