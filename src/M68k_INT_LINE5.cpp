#include <cstdint>
#include <arm_neon.h>
#include <array>
#include <utility>

#include <emu68/m68k/interpreter>

#include "RegisterMapping.h"

namespace Emu68::M68k::Interpreter::Line5 {

template<unsigned reg, uint8_t InstCC>
void DBcc(uint32_t)
{
    uint32_t pc_continue = getPC<uint32_t>(4);
    uint32_t pc_loop = getPC<uint32_t>(2) + *getPC<int16_t*>(2);
    
    if (evalCond<InstCC>()) {
        setPC(pc_continue);
    } else {
        int16_t cnt = getD<reg, int16_t>() - 1;
        setD<reg, int16_t>(cnt);
        if (unlikely(cnt == -1)) {
            setPC(pc_continue);
        } else {
            setPC(pc_loop);
        }
    }
}

template<unsigned reg, uint8_t InstCC>
void Scc_Dn(uint32_t)
{    
    if (evalCond<InstCC>()) {
        setD<reg, uint8_t>(-1);
    } else {
        setD<reg, uint8_t>(0);
    }
    advancePC(2);
}

template<unsigned reg, uint8_t InstCC>
void Scc_An_Addr(uint32_t)
{
    uintptr_t addr = getA<reg, uintptr_t>();
    if (evalCond<InstCC>()) {
        *(uint8_t *)addr = 0xff;
    } else {
        *(uint8_t *)addr = 0x00;
    }
    advancePC(2);
}

template<unsigned reg, uint8_t InstCC>
void Scc_An_Addr_PostInc(uint32_t)
{
    uintptr_t addr = getA<reg, uintptr_t>();
    if (evalCond<InstCC>()) {
        *(uint8_t *)addr = 0xff;
    } else {
        *(uint8_t *)addr = 0x00;
    }
    
    if (reg != 7) { addr += 1; }
    else          { addr += 2; }

    setA<reg, uint32_t>(addr);
    advancePC(2);
}

template<unsigned reg, uint8_t InstCC>
void Scc_An_Addr_PreDec(uint32_t)
{
    uint32_t addr = getA<reg, uintptr_t>();
    if (reg != 7) { addr -= 1; }
    else          { addr -= 2; }

    if (evalCond<InstCC>()) {
        *(uint8_t *)(uintptr_t)addr = 0xff;
    } else {
        *(uint8_t *)(uintptr_t)addr = 0x00;
    }

    setA<reg, uint32_t>(addr);
    advancePC(2);
}

template<uint8_t InstCC>
void Scc_Generic(uint32_t opcode)
{
    uint8_t dst_reg = opcode & 7;
    uint8_t mode = (opcode >> 3) & 7;

    advancePC(2);

    if (evalCond<InstCC>()) {
        storeToEA<UBYTE>(mode, dst_reg, 0xff);
    } else {
        storeToEA<UBYTE>(mode, dst_reg, 0x00);
    }
}

template<uint8_t Mode, uint8_t Reg, class Type>
void ADDQ(uint32_t opcode)
{
    uint32_t immediate = (opcode >> 9) & 7;
    if (immediate == 0) { immediate = 8; }

    advancePC(2);

    if constexpr (Mode == 1) {
        /* An destination: full 32-bit add, CCR unaffected */
        setA<Reg, uint32_t>(getA<Reg, uint32_t>() + immediate);
    } else {
        readModifyWriteEA<Mode, Reg, Type>([&](Type v) -> Type {
            auto [result, ccr] = arithWithFlags<Type, false>(v, immediate);
            SR = (SR & ~(SR_X | SR_NZVC)) | ccr | ((ccr & SR_Calt) ? SR_X : 0);
            return result;
        });
    }
}

template<uint8_t Mode, uint8_t Reg, class Type>
void SUBQ(uint32_t opcode)
{
    uint32_t immediate = (opcode >> 9) & 7;
    if (immediate == 0) { immediate = 8; }

    advancePC(2);

    if constexpr (Mode == 1) {
        /* An destination: full 32-bit add, CCR unaffected */
        setA<Reg, uint32_t>(getA<Reg, uint32_t>() - immediate);
    } else {
        readModifyWriteEA<Mode, Reg, Type>([&](Type v) -> Type {
            auto [result, ccr] = arithWithFlags<Type, true>(v, immediate);
            SR = (SR & ~(SR_X | SR_NZVC)) | ccr | ((ccr & SR_Calt) ? SR_X : 0);
            return result;
        });
    }
}

void TRAPcc(uint32_t opcode)
{
    switch (opcode & 7) {
        case 2: advancePC(4); break;
        case 3: advancePC(6); break;
        case 4: advancePC(2); break;
        default: __builtin_unreachable();
    }
    commitPC();

    if (evalCond((opcode >> 8) & 15)) {
        raiseException(VECTOR_TRAPcc, ExceptionFrameFormat::FORMAT_0, 0, 0);
    }
}

#define FILL_ALL_WR_EAs(base_offset, name, size) \
    [&]<std::size_t... Is>(int base, std::index_sequence<Is...>) { \
        ((table[base + EA((Is >> 3), Is & 7)] = \
             name<(Is >> 3), Is & 7, size>), ...); \
    }((base_offset), std::make_index_sequence<58>{});

#define FILL_ALL_WR_EAs_no_An(base_offset, name, size) \
    [&]<std::size_t... Dreg>(int base, std::index_sequence<Dreg...>) { \
        ((table[base + EA(0, Dreg)] = \
            name<0, Dreg, size>), ...); \
    }((base_offset), std::make_index_sequence<8>{}); \
    [&]<std::size_t... Is>(int base, std::index_sequence<Is...>) { \
        ((table[base + EA(2 + (Is >> 3), Is & 7)] = \
             name<2 + (Is >> 3), Is & 7, size>), ...); \
    }((base_offset), std::make_index_sequence<42>{});

template <uint8_t InstCC, class Type>
struct TRAPcc_Op { static constexpr auto value = TRAPcc; };

template <class Type, template<uint8_t,class> class Op, class F>
constexpr void fillReg(std::array<INTERPRET_Function, 4096>& table, int base)
{
    auto fillOne = [&]<std::size_t I>() {
        if constexpr (F::valid(I)) {
            int idx = base + (I << F::bitOffset);
            table[idx] = Op<F::arg(I), Type>::value;
        }
    };
    [&]<std::size_t... Is>(std::index_sequence<Is...>) {
        (fillOne.template operator()<Is>(), ...);
    }(std::make_index_sequence<F::size>{});
}

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
    
    /* Fill all Dn and all CC combinations for DBcc Dn, offset */
    [&]<std::size_t... Is>(std::index_sequence<Is...>) {
        ((table[0x0c8 | (Is & 7) | ((Is >> 3) << 8)] = 
            DBcc<(Is & 7), (Is >> 3)>), ...);
    }(std::make_index_sequence<128>{});

    /* Fill all Dn and all CC combinations for Scc Dn */
    [&]<std::size_t... Is>(std::index_sequence<Is...>) {
        ((table[0x0c0 | (Is & 7) | ((Is >> 3) << 8)] = 
            Scc_Dn<(Is & 7), (Is >> 3)>), ...);
    }(std::make_index_sequence<128>{});

    /* Fill all Dn and all CC combinations for Scc (An) */
    [&]<std::size_t... Is>(std::index_sequence<Is...>) {
        ((table[0x0d0 | (Is & 7) | ((Is >> 3) << 8)] = 
            Scc_An_Addr<(Is & 7), (Is >> 3)>), ...);
    }(std::make_index_sequence<128>{});

    /* Fill all Dn and all CC combinations for Scc (An)+ */
    [&]<std::size_t... Is>(std::index_sequence<Is...>) {
        ((table[0x0d8 | (Is & 7) | ((Is >> 3) << 8)] = 
            Scc_An_Addr_PostInc<(Is & 7), (Is >> 3)>), ...);
    }(std::make_index_sequence<128>{});

    /* Fill all Dn and all CC combinations for Scc -(An) */
    [&]<std::size_t... Is>(std::index_sequence<Is...>) {
        ((table[0x0e0 | (Is & 7) | ((Is >> 3) << 8)] = 
            Scc_An_Addr_PreDec<(Is & 7), (Is >> 3)>), ...);
    }(std::make_index_sequence<128>{});

    /* Fill all other combinations for Scc using generic */
    [&]<std::size_t... Is>(std::index_sequence<Is...>) {
        ((table[0x0e8 | (Is & 7) | ((Is >> 3) << 8)] = 
            Scc_Generic<(Is >> 3)>), ...);
    }(std::make_index_sequence<128>{});

    [&]<std::size_t... Is>(std::index_sequence<Is...>) {
        ((table[0x0f0 | (Is & 7) | ((Is >> 3) << 8)] = 
            Scc_Generic<(Is >> 3)>), ...);
    }(std::make_index_sequence<128>{});

    [&]<std::size_t... Is>(std::index_sequence<Is...>) {
        ((table[0x0f8 | (Is & 1) | ((Is >> 1) << 8)] = 
            Scc_Generic<(Is >> 1)>), ...);
    }(std::make_index_sequence<32>{});

    FILL_ALL_WR_EAs_no_An(  00000, ADDQ, BYTE);
    FILL_ALL_WR_EAs_no_An(  01000, ADDQ, BYTE);
    FILL_ALL_WR_EAs_no_An(  02000, ADDQ, BYTE);
    FILL_ALL_WR_EAs_no_An(  03000, ADDQ, BYTE);
    FILL_ALL_WR_EAs_no_An(  04000, ADDQ, BYTE);
    FILL_ALL_WR_EAs_no_An(  05000, ADDQ, BYTE);
    FILL_ALL_WR_EAs_no_An(  06000, ADDQ, BYTE);
    FILL_ALL_WR_EAs_no_An(  07000, ADDQ, BYTE);

    FILL_ALL_WR_EAs(        00100, ADDQ, WORD);
    FILL_ALL_WR_EAs(        01100, ADDQ, WORD);
    FILL_ALL_WR_EAs(        02100, ADDQ, WORD);
    FILL_ALL_WR_EAs(        03100, ADDQ, WORD);
    FILL_ALL_WR_EAs(        04100, ADDQ, WORD);
    FILL_ALL_WR_EAs(        05100, ADDQ, WORD);
    FILL_ALL_WR_EAs(        06100, ADDQ, WORD);
    FILL_ALL_WR_EAs(        07100, ADDQ, WORD);

    FILL_ALL_WR_EAs(        00200, ADDQ, LONG);
    FILL_ALL_WR_EAs(        01200, ADDQ, LONG);
    FILL_ALL_WR_EAs(        02200, ADDQ, LONG);
    FILL_ALL_WR_EAs(        03200, ADDQ, LONG);
    FILL_ALL_WR_EAs(        04200, ADDQ, LONG);
    FILL_ALL_WR_EAs(        05200, ADDQ, LONG);
    FILL_ALL_WR_EAs(        06200, ADDQ, LONG);
    FILL_ALL_WR_EAs(        07200, ADDQ, LONG);

    FILL_ALL_WR_EAs_no_An(  00400, SUBQ, BYTE);
    FILL_ALL_WR_EAs_no_An(  01400, SUBQ, BYTE);
    FILL_ALL_WR_EAs_no_An(  02400, SUBQ, BYTE);
    FILL_ALL_WR_EAs_no_An(  03400, SUBQ, BYTE);
    FILL_ALL_WR_EAs_no_An(  04400, SUBQ, BYTE);
    FILL_ALL_WR_EAs_no_An(  05400, SUBQ, BYTE);
    FILL_ALL_WR_EAs_no_An(  06400, SUBQ, BYTE);
    FILL_ALL_WR_EAs_no_An(  07400, SUBQ, BYTE);

    FILL_ALL_WR_EAs(        00500, SUBQ, WORD);
    FILL_ALL_WR_EAs(        01500, SUBQ, WORD);
    FILL_ALL_WR_EAs(        02500, SUBQ, WORD);
    FILL_ALL_WR_EAs(        03500, SUBQ, WORD);
    FILL_ALL_WR_EAs(        04500, SUBQ, WORD);
    FILL_ALL_WR_EAs(        05500, SUBQ, WORD);
    FILL_ALL_WR_EAs(        06500, SUBQ, WORD);
    FILL_ALL_WR_EAs(        07500, SUBQ, WORD);

    FILL_ALL_WR_EAs(        00600, SUBQ, LONG);
    FILL_ALL_WR_EAs(        01600, SUBQ, LONG);
    FILL_ALL_WR_EAs(        02600, SUBQ, LONG);
    FILL_ALL_WR_EAs(        03600, SUBQ, LONG);
    FILL_ALL_WR_EAs(        04600, SUBQ, LONG);
    FILL_ALL_WR_EAs(        05600, SUBQ, LONG);
    FILL_ALL_WR_EAs(        06600, SUBQ, LONG);
    FILL_ALL_WR_EAs(        07600, SUBQ, LONG);

    fillReg<void, TRAPcc_Op, RegField<8, 4>>(table, 00372);
    fillReg<void, TRAPcc_Op, RegField<8, 4>>(table, 00373);
    fillReg<void, TRAPcc_Op, RegField<8, 4>>(table, 00374);

    #if 0
    [0372]          = { EMIT_TRAPcc, NULL, SR_CCR, 0, 2, 0, 0 },
    [0373]          = { EMIT_TRAPcc, NULL, SR_CCR, 0, 3, 0, 0 },
    [0374]          = { EMIT_TRAPcc, NULL, SR_CCR, 0, 1, 0, 0 },

    [0772]          = { EMIT_TRAPcc, NULL, SR_CCR, 0, 2, 0, 0},
    [0773]          = { EMIT_TRAPcc, NULL, SR_CCR, 0, 3, 0, 0},
    [0774]          = { EMIT_TRAPcc, NULL, SR_CCR, 0, 1, 0, 0},
    #endif

    return table;
}

constexpr auto InsnTable __attribute__((used,section(".int.jumptable.5"))) = buildInsnTable();

} // Emu68::M68k::Interpreter::Line5
