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

    void M68K_LoadContext(struct M68KState *ctx);
    void M68K_SaveContext(struct M68KState *ctx);
}

namespace Emu68::M68k::Interpreter {

/*
    Template for MOVE.B/W/L Dsrc, Ddest with SR update. 
    getD covers sign-extension, setD makes sure the source is inserted
    into destination if only a portion of register needs to be updated
*/
template<unsigned src, unsigned dst, class type> requires (src < 8 && dst < 8 && std::is_signed<type>::value)
void INTERPRET_MOVE_Dn_to_Dn(uint32_t)
{
    uint32_t sr = SR & ~SR_NZVC; \
    type val;

    val = getD<src, type>();
    setD<dst, type>(val);

    if (val == 0) {
        sr |= SR_Z;
    } else if (val < 0) {
        sr |= SR_N;
    }

    SR = sr;
    PC += 2;
}

/*
    Template for MOVE.W/L Asrc, Ddest with SR update. 
*/
template<unsigned src, unsigned dst, class type> requires (src < 8 && dst < 8 && std::is_signed<type>::value)
void INTERPRET_MOVE_An_to_Dn(uint32_t)
{
    uint32_t sr = SR & ~SR_NZVC; \
    type val;

    val = getA<src, type>();
    setD<dst, type>(val);

    if (val == 0) {
        sr |= SR_Z;
    } else if (val < 0) {
        sr |= SR_N;
    }

    SR = sr;
    PC += 2;
}

/*
    Template for MOVEA.W/L Dsrc, Adest without SR update. 
*/
template<unsigned src, unsigned dst, class type> requires (src < 8 && dst < 8 && std::is_signed<type>::value)
void INTERPRET_MOVEA_Dn_to_An(uint32_t)
{
    type val;

    val = getD<src, type>();
    setA<dst, type>(val);

    PC += 2;
}

/*
    Template for MOVEA.W/L Dsrc, Adest without SR update. 
*/
template<unsigned src, unsigned dst, class type> requires (src < 8 && dst < 8 && std::is_signed<type>::value)
void INTERPRET_MOVEA_An_to_An(uint32_t)
{
    type val;

    val = getA<src, type>();
    setA<dst, type>(val);

    PC += 2;
}

/*
    MOVE.B/W/L Dn to Abs.L
*/
template<unsigned src, class type> requires (src < 8 && std::is_signed<type>::value)
void INTERPRET_MOVE_Dn_to_ABS_L(uint32_t)
{
    uint32_t sr = SR & ~SR_NZVC;
    uint32_t abs = *(uint32_t*)(uintptr_t)(PC + 2);
    type val = getD<src, type>();
    *(type*)(uintptr_t)abs = val;
    if (val == 0) {
        sr |= SR_Z; 
    } else if (val < 0) {
        sr |= SR_N;
    }
    SR = sr;
    PC += 6;
}

/*
    MOVE.B/W/L Dn to Abs.W
*/
template<unsigned src, class type> requires (src < 8 && std::is_signed<type>::value)
void INTERPRET_MOVE_Dn_to_ABS_W(uint32_t)
{
    uint32_t sr = SR & ~SR_NZVC;
    uint32_t abs = *(int16_t*)(uintptr_t)(PC + 2);
    type val = getD<src, type>();
    *(type*)(uintptr_t)abs = val;
    if (val == 0) {
        sr |= SR_Z; 
    } else if (val < 0) {
        sr |= SR_N;
    }
    SR = sr;
    PC += 4;
}

/*
    MOVE.W/L An to Abs.L
*/
template<unsigned src, class type> requires (src < 8 && std::is_signed<type>::value)
void INTERPRET_MOVE_An_to_ABS_L(uint32_t)
{
    uint32_t sr = SR & ~SR_NZVC;
    uint32_t abs = *(uint32_t*)(uintptr_t)(PC + 2);
    type val = getA<src, type>();
    *(type*)(uintptr_t)abs = val;
    if (val == 0) {
        sr |= SR_Z; 
    } else if (val < 0) {
        sr |= SR_N;
    }
    SR = sr;
    PC += 6;
}

/*
    MOVE.W/L An to Abs.W
*/
template<unsigned src, class type> requires (src < 8 && std::is_signed<type>::value)
void INTERPRET_MOVE_An_to_ABS_W(uint32_t)
{
    uint32_t sr = SR & ~SR_NZVC;
    uint32_t abs = *(int16_t*)(uintptr_t)(PC + 2);
    type val = getD<src, type>();
    *(type*)(uintptr_t)abs = val;
    if (val == 0) {
        sr |= SR_Z; 
    } else if (val < 0) {
        sr |= SR_N;
    }
    SR = sr;
    PC += 4;
}

/*
    MOVE.B/W/L #Immed to Dn
*/
template<unsigned dst, class type> requires (dst < 8 && std::is_signed<type>::value)
void INTERPRET_MOVE_IMM_to_Dn(uint32_t)
{
    uint32_t sr = SR & ~SR_NZVC;
    type val;
    if (sizeof(type) == 4) {
        val = *(type*)(uintptr_t)(PC + 2);
        PC = PC + 6;
    } else if (sizeof(type) == 2) {
        val = *(type*)(uintptr_t)(PC + 2);
        PC = PC + 4;
    } else if (sizeof(type) == 1) {
        val = *(type*)(uintptr_t)(PC + 3);
        PC = PC + 4;
    }
    setD<dst, type>(val);
    if (val == 0) {
        sr |= SR_Z;
    } else if (val < 0) {
        sr |= SR_N;
    }
    SR = sr;
}

/*
    MOVEA.W/L #Immed to An
*/
template<unsigned dst, class type> requires (dst < 8 && std::is_signed<type>::value)
void INTERPRET_MOVEA_IMM_to_An(uint32_t)
{
    type val;
    if (sizeof(type) == 4) {
        val = *(type*)(uintptr_t)(PC + 2);
        PC = PC + 6;
    } else if (sizeof(type) == 2) {
        val = *(type*)(uintptr_t)(PC + 2);
        PC = PC + 4;
    }
    setA<dst, type>(val);
}

template<unsigned areg, class type> requires (areg < 8 && std::is_signed<type>::value)
void INTERPRET_MOVE_IMM_to_An_Addr(uint32_t)
{
    uint32_t sr = SR & ~SR_NZVC;
    type val;
    if (sizeof(type) == 4) {
        val = *(type*)(uintptr_t)(PC + 2);
        PC = PC + 6;
    } else if (sizeof(type) == 2) {
        val = *(type*)(uintptr_t)(PC + 2);
        PC = PC + 4;
    } else if (sizeof(type) == 1) {
        val = *(type*)(uintptr_t)(PC + 3);
        PC = PC + 4;
    }

    *(type*)getA<areg, uintptr_t>() = val;
    
    if (val == 0) {
        sr |= SR_Z;
    } else if (val < 0) {
        sr |= SR_N;
    }
    SR = sr;
}

template<unsigned areg, class type> requires (areg < 8 && std::is_signed<type>::value)
void INTERPRET_MOVE_IMM_to_An_Addr_PostInc(uint32_t)
{
    uint32_t sr = SR & ~SR_NZVC;
    type val;
    if (sizeof(type) == 4) {
        val = *(type*)(uintptr_t)(PC + 2);
        PC = PC + 6;
    } else if (sizeof(type) == 2) {
        val = *(type*)(uintptr_t)(PC + 2);
        PC = PC + 4;
    } else if (sizeof(type) == 1) {
        val = *(type*)(uintptr_t)(PC + 3);
        PC = PC + 4;
    }

    uintptr_t addr = getA<areg, uintptr_t>();
    *(type*)addr = val;
    
    addr += sizeof(type);
    if (sizeof(type) == 1 && areg == 7) addr++;

    setA<areg, LONG>(addr);

    if (val == 0) {
        sr |= SR_Z;
    } else if (val < 0) {
        sr |= SR_N;
    }
    SR = sr;
}

template<unsigned areg, class type> requires (areg < 8 && std::is_signed<type>::value)
void INTERPRET_MOVE_IMM_to_An_Addr_PreDec(uint32_t)
{
    uint32_t sr = SR & ~SR_NZVC;
    type val;
    if (sizeof(type) == 4) {
        val = *(type*)(uintptr_t)(PC + 2);
        PC = PC + 6;
    } else if (sizeof(type) == 2) {
        val = *(type*)(uintptr_t)(PC + 2);
        PC = PC + 4;
    } else if (sizeof(type) == 1) {
        val = *(type*)(uintptr_t)(PC + 3);
        PC = PC + 4;
    }

    uintptr_t addr = getA<areg, uintptr_t>();
    addr -= sizeof(type);
    if (sizeof(type) == 1 && areg == 7) addr--;

    *(type*)addr = val;
    setA<areg, LONG>(addr);

    if (val == 0) {
        sr |= SR_Z;
    } else if (val < 0) {
        sr |= SR_N;
    }
    SR = sr;
}

static inline struct M68KState *getCTX()
{
    struct M68KState *ctx;
    __asm__ volatile("mov %0, " CTX_POINTER_ASM:"=r"(ctx));
    return ctx;
}

template<class type>
void INTERPRET_MOVE_Generic(uint32_t opcode)
{
    type value;
    int src_mode = (opcode >> 3) & 7;
    int src_reg = opcode & 7;
    int dst_mode = (opcode >> 6) & 7;
    int dst_reg = (opcode >> 9) & 7;

    /* Modes with An as source or destination are WORD/LONG only, otherwise throw exception */
    if (sizeof(type) < 2) {
        if (src_mode == 1 || dst_mode == 1) {
            UNIMPLEMENTED(opcode);
            return;
        }
    }

    /* Update PC to point either to next instruction or first extension word */
    PC += 2;

    LoadFromEffectiveAddress(src_reg, sizeof(type), &value, src_mode);

    M68K_SaveContext(getCTX());
    kprintf("MOVE generic from mode %d.%d to mode %d.%d size %d value %x\n", src_mode, src_reg, dst_mode, dst_reg, sizeof(type), value);
    M68K_LoadContext(getCTX());

    StoreToEffectiveAddress(dst_reg, value, sizeof(type), dst_mode);

    if (dst_mode != 1) {
        uint32_t sr = SR & ~SR_NZVC;

        if (value == 0) {
            sr |= SR_Z;
        } else if (value < 0) {
            sr |= SR_N;
        }

        SR = sr;
    }
}

template<uint8_t SrcMode, uint8_t SrcReg, uint8_t DstMode, uint8_t DstReg, class Type>
void MOVE(uint32_t)
{
    PC += 2;
    Type value = LoadFromEA<SrcMode, SrcReg, Type>();
    StoreToEA<DstMode, DstReg, Type>(value);

    if (DstMode != 1) {
        uint32_t sr = SR & ~SR_NZVC;

        if (value == 0) {
            sr |= SR_Z;
        } else if (value < 0) {
            sr |= SR_N;
        }

        SR = sr;
    }
}

#define FILL_ALL_MOVE(name) \
[&]<std::size_t... Is>(std::index_sequence<Is...>) { \
    auto fillOne = [&]<std::size_t I>() { \
        constexpr std::size_t DstI = I / 61; \
        constexpr std::size_t SrcI = I % 61; \
        constexpr std::size_t DstMode = DstI >> 3; \
        constexpr std::size_t SrcMode = SrcI >> 3; \
        if constexpr (sizeof(type) > 1 || (SrcMode != 1 && DstMode != 1)) { \
            constexpr int base = ((DstI & 7) << 9) | ((DstI >> 3) << 6); \
            table[base + SrcI] = name<(SrcI >> 3), SrcI & 7, (DstI >> 3), DstI & 7, type>; \
        } \
    }; \
    (fillOne.template operator()<Is>(), ...); \
}(std::make_index_sequence<58 * 61>{});

template<class type>
static constexpr std::array<INTERPRET_Function, 4096> BuildInsnTable()
{
    std::array<INTERPRET_Function, 4096> table{};

    auto fill = [&table](int first, int last, INTERPRET_Function func) {
        for (int i = first; i <= last; ++i)
            table[i] = func;
    };
#if 0
    auto SRC = [](int mode, int reg) constexpr { return (mode << 3) | reg; };
    auto DST = [](int mode, int reg) constexpr { return (mode << 6) | (reg << 9); };
#endif
    /* Make all entries unimplemented first */
    fill(00000, 07777, UNIMPLEMENTED);

    FILL_ALL_MOVE(MOVE)
#if 0
    #if 0
    /*
        Fill all allowed defaults with generic version:
        modes 0 to 6 allow all register combinations
        mode 7 allows only register 0 and 1

        Register and mode are decoded as reg:mod - the loop can therefore 
        start at octal 000 (mode 0, register 0) and iterate up to octal 071
        with mode<->reg swap (mode 7, register 1). Source decoded as mod:reg 
        can go always from 000 (mode 0, register 0) up to 074 (mode 7, register 4)
    */
   
    for (int dst_rm = 0; dst_rm <= 071; dst_rm++) {
        int dst_mode = dst_rm >> 3;
        int dst_reg  = dst_rm & 7;
        int base = DST(dst_mode, dst_reg);
        fill(base + SRC(0, 0), base + SRC(7, 4), INTERPRET_MOVE_Generic<type>);
    }
    #endif

    /* Fill MOVE Dn to Dn, all 64 combinations */
    [&]<std::size_t... Is>(int base, std::index_sequence<Is...>) {
        ((table[base + SRC(0, Is & 7) + DST(0, Is >> 3)] = 
            INTERPRET_MOVE_Dn_to_Dn<(Is & 7), (Is >> 3), type>), ...);
    }(0, std::make_index_sequence<64>{});

    /* Fill MOVE Dn to ABS.W */
    [&]<std::size_t... Dreg>(int base, std::index_sequence<Dreg...>) {
        ((table[base + SRC(0, Dreg)] =
             INTERPRET_MOVE_Dn_to_ABS_W<Dreg, type>), ...);
    }(DST(7, 0), std::make_index_sequence<8>{});

    /* Fill MOVE Dn to ABS.L */
    [&]<std::size_t... Dreg>(int base, std::index_sequence<Dreg...>) {
        ((table[base + SRC(0, Dreg)] =
             INTERPRET_MOVE_Dn_to_ABS_L<Dreg, type>), ...);
    }(DST(7, 1), std::make_index_sequence<8>{});

    /* Fill MOVE #IMM to Dn */
    [&]<std::size_t... Dreg>(int base, std::index_sequence<Dreg...>) {
        ((table[base + DST(0, Dreg)] =
             INTERPRET_MOVE_IMM_to_Dn<Dreg, type>), ...);
    }(SRC(7, 4), std::make_index_sequence<8>{});

    /* The MOVE variants reading from An and MOVEA exist for SHORT and LONG, only */
    if constexpr (sizeof(type) > 1)
    {
        /* Fill MOVE An to Dn, all 64 combinations */
        [&]<std::size_t... Is>(int base, std::index_sequence<Is...>) {
            ((table[base + SRC(1, Is & 7) + DST(0, Is >> 3)] = 
                INTERPRET_MOVE_An_to_Dn<(Is & 7), (Is >> 3), type>), ...);
        }(0, std::make_index_sequence<64>{});

        /* Fill MOVEA Dn to An, all 64 combinations */
        [&]<std::size_t... Is>(int base, std::index_sequence<Is...>) {
            ((table[base + SRC(0, Is & 7) + DST(1, Is >> 3)] =
                INTERPRET_MOVEA_Dn_to_An<(Is & 7), (Is >> 3), type>), ...);
        }(0, std::make_index_sequence<64>{});

        /* Fill MOVEA An to An, all 64 combinations */
        [&]<std::size_t... Is>(int base, std::index_sequence<Is...>) {
            ((table[base + SRC(1, Is & 7) + DST(1, Is >> 3)] =
                INTERPRET_MOVEA_An_to_An<(Is & 7), (Is >> 3), type>), ...);
        }(0, std::make_index_sequence<64>{});

        /* Fill MOVE An to ABS.W */
        [&]<std::size_t... Areg>(int base, std::index_sequence<Areg...>) {
            ((table[base + SRC(1, Areg)] =
                INTERPRET_MOVE_An_to_ABS_W<Areg, type>), ...);
        }(DST(7, 0), std::make_index_sequence<8>{});

        /* Fill MOVE An to ABS.L, invalid for BYTE */
        [&]<std::size_t... Areg>(int base, std::index_sequence<Areg...>) {
            ((table[base + SRC(1, Areg)] =
                INTERPRET_MOVE_An_to_ABS_L<Areg, type>), ...);
        }(DST(7, 1), std::make_index_sequence<8>{});

        /* Fill MOVEA #IMM to An, invalid for BYTE */
        [&]<std::size_t... Areg>(int base, std::index_sequence<Areg...>) {
            ((table[base + DST(1, Areg)] =
                INTERPRET_MOVEA_IMM_to_An<Areg, type>), ...);
        }(SRC(7, 4), std::make_index_sequence<8>{});
    }

    /* Fill MOVE #IMM to (An) */
    [&]<std::size_t... Areg>(int base, std::index_sequence<Areg...>) {
        ((table[base + DST(2, Areg)] =
             INTERPRET_MOVE_IMM_to_An_Addr<Areg, type>), ...);
    }(SRC(7, 4), std::make_index_sequence<8>{});

    /* Fill MOVE #IMM to (An)+ */
    [&]<std::size_t... Areg>(int base, std::index_sequence<Areg...>) {
        ((table[base + DST(3, Areg)] =
             INTERPRET_MOVE_IMM_to_An_Addr_PostInc<Areg, type>), ...);
    }(SRC(7, 4), std::make_index_sequence<8>{});

    /* Fill MOVE #IMM to -(An) */
    [&]<std::size_t... Areg>(int base, std::index_sequence<Areg...>) {
        ((table[base + DST(4, Areg)] =
             INTERPRET_MOVE_IMM_to_An_Addr_PreDec<Areg, type>), ...);
    }(SRC(7, 4), std::make_index_sequence<8>{});
#endif
    return table;
}

} // Emu68::M68k::Interpreter

static constexpr auto InsnTable_L = Emu68::M68k::Interpreter::BuildInsnTable<LONG>();
static constexpr auto InsnTable_W = Emu68::M68k::Interpreter::BuildInsnTable<WORD>();
static constexpr auto InsnTable_B = Emu68::M68k::Interpreter::BuildInsnTable<BYTE>();

__attribute__((optimize("no-optimize-sibling-calls")))
void INTERPRET_line1(uint32_t opcode)
{
    InsnTable_B[opcode & 0xfff](opcode);
}

__attribute__((optimize("no-optimize-sibling-calls")))
void INTERPRET_line2(uint32_t opcode)
{
    InsnTable_L[opcode & 0xfff](opcode);
}

__attribute__((optimize("no-optimize-sibling-calls")))
void INTERPRET_line3(uint32_t opcode)
{
    InsnTable_W[opcode & 0xfff](opcode);
}