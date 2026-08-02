#include <cstdint>
#include <arm_neon.h>
#include <array>
#include <utility>

#include <emu68/m68k/interpreter.hpp>

#include "RegisterMapping.h"

#define _REGLOCK_H
extern "C" {
    #include "M68k.h"
    #include "support.h"
}


extern "C" {

    void M68K_LoadContext(struct M68KState *ctx);
    void M68K_SaveContext(struct M68KState *ctx);

}

namespace Emu68::M68k::Interpreter {

static inline struct M68KState *getCTX()
{
    struct M68KState *ctx;
    __asm__ volatile("mov %0, " CTX_POINTER_ASM:"=r"(ctx));
    return ctx;
}


template<unsigned reg, uint8_t InstCC>
void DBcc(uint32_t)
{
    uint32_t pc_continue = PC + 4;
    uint32_t pc_loop = PC + 2 + *(int16_t *)(uintptr_t)(PC + 2);
    
    if (EvalCond<InstCC>()) {
        PC = pc_continue;
    } else {
        int16_t cnt = getD<reg, int16_t>() - 1;
        setD<reg, int16_t>(cnt);
        if (unlikely(cnt == -1)) {
            PC = pc_continue;
        } else {
            PC = pc_loop;
        }
    }
}

template<unsigned reg, uint8_t InstCC>
void Scc_Dn(uint32_t)
{    
    if (EvalCond<InstCC>()) {
        setD<reg, uint8_t>(-1);
    } else {
        setD<reg, uint8_t>(0);
    }
    PC += 2;
}

template<unsigned reg, uint8_t InstCC>
void Scc_An_Addr(uint32_t)
{
    uintptr_t addr = getA<reg, uintptr_t>();
    if (EvalCond<InstCC>()) {
        *(uint8_t *)addr = 0xff;
    } else {
        *(uint8_t *)addr = 0x00;
    }
    PC += 2;
}

template<unsigned reg, uint8_t InstCC>
void Scc_An_Addr_PostInc(uint32_t)
{
    uintptr_t addr = getA<reg, uintptr_t>();
    if (EvalCond<InstCC>()) {
        *(uint8_t *)addr = 0xff;
    } else {
        *(uint8_t *)addr = 0x00;
    }
    
    if (reg != 7) addr += 1;
    else addr += 2;

    setA<reg, uint32_t>(addr);
    PC += 2;
}

template<unsigned reg, uint8_t InstCC>
void Scc_An_Addr_PreDec(uint32_t)
{
    uint32_t addr = getA<reg, uintptr_t>();
    if (reg != 7) addr -= 1;
    else addr -= 2;

    if (EvalCond<InstCC>()) {
        *(uint8_t *)(uintptr_t)addr = 0xff;
    } else {
        *(uint8_t *)(uintptr_t)addr = 0x00;
    }

    setA<reg, uint32_t>(addr);
    PC += 2;
}

template<uint8_t InstCC>
void Scc_Generic(uint32_t opcode)
{
    uint8_t dst_reg = opcode & 7;
    uint8_t mode = (opcode >> 3) & 7;

    PC += 2;

    if (EvalCond<InstCC>()) {
        StoreToEffectiveAddress(dst_reg, 0xff, 1, mode);
    } else {
        StoreToEffectiveAddress(dst_reg, 0x00, 1, mode);
    }
}

template<uint8_t Mode, uint8_t Reg, class Type>
void ADDQ(uint32_t opcode)
{
    uint32_t immediate = (opcode >> 9) & 7;
    if (immediate == 0) immediate = 8;

    PC += 2;

    ReadModifyWriteEA<Mode, Reg, Type>([&](Type v) -> Type {
        auto [result, ccr] = Arith_WithFlags<Type, false>(v, immediate);
        SR = (SR & ~(SR_X | SR_NZVC)) | ccr | ((ccr & SR_Calt) ? SR_X : 0);
        return result;
    });
}


template<uint8_t Mode, uint8_t Reg, class Type>
void SUBQ(uint32_t opcode)
{
    uint32_t immediate = (opcode >> 9) & 7;
    if (immediate == 0) immediate = 8;

    PC += 2;

    ReadModifyWriteEA<Mode, Reg, Type>([&](Type v) -> Type {
        auto [result, ccr] = Arith_WithFlags<Type, true>(v, immediate);
        SR = (SR & ~(SR_X | SR_NZVC)) | ccr | ((ccr & SR_Calt) ? SR_X : 0);
        return result;
    });
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


static constexpr std::array<INTERPRET_Function, 4096> BuildInsnTable()
{
    std::array<INTERPRET_Function, 4096> table{};

    auto fill = [&table](int first, int last, INTERPRET_Function func) {
        for (int i = first; i <= last; ++i)
            table[i] = func;
    };

    auto EA = [](int mode, int reg) constexpr { return (mode << 3) | reg; };

    fill(00000, 07777, UNIMPLEMENTED);
    
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

} // Emu68::M68k::Interpreter

static constexpr auto InsnTable = Emu68::M68k::Interpreter::BuildInsnTable();

__attribute__((optimize("no-optimize-sibling-calls")))
void INTERPRET_line5(uint32_t opcode)
{
    InsnTable[opcode & 4095](opcode);
}
