#include <cstdint>
#include <arm_neon.h>
#include <array>

#include <emu68/m68k/interpreter>

#include "RegisterMapping.h"

namespace Emu68::M68k::Interpreter::LineD {

template<uint8_t Mode, uint8_t Reg, uint8_t An, class Type>
requires (sizeof(Type) > 1 && An < 8)
void ADDA(uint32_t opcode)
{
    Type val;

    PC = PC + 2;
    
    if constexpr (Mode == DEFAULT_EA || Reg == DEFAULT_EA) {
        uint8_t mode = (Mode == DEFAULT_EA) ? ((opcode >> 3) & 7) : Mode;
        uint8_t reg = (Reg == DEFAULT_EA) ? (opcode & 7) : Reg;
        val = loadFromEA<Type>(mode, reg);
    } else {
        val = loadFromEA<Mode, Reg, Type>(); 
    }

    setA<An, LONG>(getA<An, LONG>() + val);
}

template<uint8_t Mode, uint8_t Reg, uint8_t Dn, class Type>
requires (Mode > 1)
void ADD_Dn_to_EA(uint32_t opcode)
{
    Type addend;

    if constexpr (Dn == DEFAULT_EA) {
        addend = getD<Type>((opcode >> 9) & 7);
    } else {
        addend = getD<Dn, Type>();
    }

    PC = PC + 2;

    auto oper = [&](Type v) -> Type {
        auto [result, ccr] = arithWithFlags<Type, false>(v, addend);
        SR = (SR & ~(SR_X | SR_NZVC)) | ccr | ((ccr & SR_Calt) ? SR_X : 0);
        return result;
    };

    if constexpr (Mode == DEFAULT_EA || Reg == DEFAULT_EA) {
        uint8_t mode = (Mode == DEFAULT_EA) ? ((opcode >> 3) & 7) : Mode;
        uint8_t reg = (Reg == DEFAULT_EA) ? (opcode & 7) : Reg;
        readModifyWriteEA<Type>(mode, reg, oper);
    } else {
        readModifyWriteEA<Mode, Reg, Type>(oper);
    }
}

template<uint8_t Mode, uint8_t Reg, uint8_t Dn, class Type>
requires (Mode != 1 || (Mode == 1 && sizeof(Type) > 1))
void ADD_EA_to_Dn(uint32_t opcode)
{
    PC = PC + 2;
    Type addend;
    Type dval;
    
    if constexpr (Mode == DEFAULT_EA || Reg == DEFAULT_EA) {
        uint8_t mode = (Mode == DEFAULT_EA) ? ((opcode >> 3) & 7) : Mode;
        uint8_t reg = (Reg == DEFAULT_EA) ? (opcode & 7) : Reg;
        addend = loadFromEA<Type>(mode, reg);
    } else {
        addend = loadFromEA<Mode, Reg, Type>(); 
    }

    if constexpr (Dn == DEFAULT_EA) {
        dval = getD<Type>((opcode >> 9) & 7);
    } else {
        dval = getD<Dn, Type>();
    }

    auto [result, ccr] = arithWithFlags<Type, false>(dval, addend);
    SR = (SR & ~(SR_X | SR_NZVC)) | ccr | ((ccr & SR_Calt) ? SR_X : 0);

    if constexpr (Dn == DEFAULT_EA) {
        setD<Type>((opcode >> 9) & 7, result);
    } else {
        setD<Dn, Type>(result);
    }
}

template<bool MemForm, uint8_t Rx, uint8_t Ry, class Type>
void ADDX(uint32_t)
{
    PC += 2;
    uint8_t x_in = (SR & SR_X) ? 1 : 0;

    Type a, b;
    Type *dst;

    if constexpr (!MemForm) {
        a = getD<Ry, Type>();
        b = getD<Rx, Type>();
    } else {
        uint32_t srcAddr = getEA<4, Ry, Type>();
        uint32_t dstAddr = getEA<4, Rx, Type>();
        a = *(Type *)(uintptr_t)srcAddr;
        b = *(Type *)(uintptr_t)dstAddr;
        dst = (Type *)(uintptr_t)dstAddr;
    }

    auto [result, ccr] = arithXWithFlags<Type, false>(b, a, x_in);
    ccr = (ccr & ~SR_Z) | (ccr & SR & SR_Z);
    SR = (SR & ~(SR_X | SR_NZVC)) | ccr | ((ccr & SR_Calt) ? SR_X : 0);

    if constexpr (!MemForm) { setD<Rx, Type>(result); }
    else                    { *dst = result; }
}

template <class Type, template<uint8_t,uint8_t,uint8_t,class> class Op,
          class EAF, class RegF>
constexpr void fillEAReg(std::array<INTERPRET_Function, 4096>& table, int base)
{
    auto fillOne = [&]<std::size_t I>() {
        constexpr unsigned eaV  = I / RegF::size;
        constexpr unsigned regV = I % RegF::size;

        if constexpr (EAF::template valid<eaV>() && RegF::valid(regV)) {
            int idx = base + (eaV << EAF::bitOffset) + (regV << RegF::bitOffset);
            table[idx] = Op<EAF::modeArg(eaV), EAF::regArg(eaV), RegF::arg(regV), Type>::value;
        }
    };
    [&]<std::size_t... Is>(std::index_sequence<Is...>) {
        (fillOne.template operator()<Is>(), ...);
    }(std::make_index_sequence<EAF::size * RegF::size>{});
}

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

// hot on the source side: registers, simple indirects, d16(An), and the
// mode-7 forms that show up constantly in Kickstart/AmigaOS code —
// abs.W ("4.w"), abs.L, d16(PC), immediate.
inline constexpr uint32_t SrcSpecializeMask = 
      classBit(EAClass::Dn)      | classBit(EAClass::An)
    | classBit(EAClass::Ind)     | classBit(EAClass::IndPost)
    | classBit(EAClass::IndPre)  | classBit(EAClass::D16An)
    | classBit(EAClass::D16PC)   | classBit(EAClass::Imm);
    // D8AnXn / D8PCXn omitted: extension-word decode is rare enough
    // AbsW / AbsL omitted: adding a constant from RAM location is rare

inline constexpr uint32_t DstSpecializeMask = 
      classBit(EAClass::Dn)      | classBit(EAClass::An)
    | classBit(EAClass::Ind)     | classBit(EAClass::IndPost)
    | classBit(EAClass::IndPre)  | classBit(EAClass::D16An)
    | classBit(EAClass::AbsL);
    // AbsW / AbsL omitted: adding to a fixed memory location is rare.
    // D8AnXn omitted; D16PC/D8PCXn/Imm are not valid destinations anyway.

template <uint8_t Mode, uint8_t Reg, uint8_t Dn, class Type>
struct ADDA_Op { static constexpr auto value = ADDA<Mode, Reg, Dn, Type>; };

template <uint8_t Mode, uint8_t Reg, uint8_t Dn, class Type>
struct ADD_EA_to_Dn_Op { static constexpr auto value = ADD_EA_to_Dn<Mode, Reg, Dn, Type>; };

template <uint8_t Mode, uint8_t Reg, uint8_t Dn, class Type>
struct ADD_Dn_to_EA_Op { static constexpr auto value = ADD_Dn_to_EA<Mode, Reg, Dn, Type>; };

template <uint8_t Rx, uint8_t Ry, class Type>
struct ADDX_Reg_Op { static constexpr auto value = ADDX<false, Rx, Ry, Type>; };

template <uint8_t Rx, uint8_t Ry, class Type>
struct ADDX_Mem_Op { static constexpr auto value = ADDX<true, Rx, Ry, Type>; };

// EA field combines Register and EA mode in one, gets
template <unsigned BitOffset, class Type, bool Write, uint32_t HotMask>
struct EAField : FieldBase<BitOffset, 6> {
    template <unsigned V>
    static constexpr bool valid() {
        constexpr unsigned mode = V >> 3, reg = V & 7;
        if constexpr (Write) return MemoryMode<mode> && DestEA7<mode, reg> && ByteCompatibleMode<mode, Type>;
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
    for (auto& e : table) e = ILLEGAL;

    fillEAReg<WORD, ADDA_Op, EAField<0,WORD,false,SrcSpecializeMask>, RegField<9>>(table, 00300);
    fillEAReg<LONG, ADDA_Op, EAField<0,LONG,false,SrcSpecializeMask>, RegField<9>>(table, 00700);

    fillEAReg<BYTE, ADD_EA_to_Dn_Op, EAField<0,BYTE,false,SrcSpecializeMask>, RegField<9>>(table, 00000);
    fillEAReg<WORD, ADD_EA_to_Dn_Op, EAField<0,WORD,false,SrcSpecializeMask>, RegField<9>>(table, 00100);
    fillEAReg<LONG, ADD_EA_to_Dn_Op, EAField<0,LONG,false,SrcSpecializeMask>, RegField<9>>(table, 00200);

    fillEAReg<BYTE, ADD_Dn_to_EA_Op, EAField<0,BYTE,true,DstSpecializeMask>, RegField<9>>(table, 00400);
    fillEAReg<WORD, ADD_Dn_to_EA_Op, EAField<0,WORD,true,DstSpecializeMask>, RegField<9>>(table, 00500);
    fillEAReg<LONG, ADD_Dn_to_EA_Op, EAField<0,LONG,true,DstSpecializeMask>, RegField<9>>(table, 00600);

    fillRegReg<BYTE, ADDX_Reg_Op, RegField<9>, RegField<0>>(table, 00400);
    fillRegReg<WORD, ADDX_Reg_Op, RegField<9>, RegField<0>>(table, 00500);
    fillRegReg<LONG, ADDX_Reg_Op, RegField<9>, RegField<0>>(table, 00600);
    fillRegReg<BYTE, ADDX_Mem_Op, RegField<9>, RegField<0>>(table, 00410);
    fillRegReg<WORD, ADDX_Mem_Op, RegField<9>, RegField<0>>(table, 00510);
    fillRegReg<LONG, ADDX_Mem_Op, RegField<9>, RegField<0>>(table, 00610);

    return table;
}

constexpr auto InsnTable __attribute__((used,aligned(4096),section(".int.jumptable.d"))) = buildInsnTable();

} // Emu68::M68k::Interpreter::LineD
