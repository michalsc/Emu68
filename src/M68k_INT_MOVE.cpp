#include <cstdint>
#include <arm_neon.h>
#include <array>
#include <utility>

#include <emu68/m68k/interpreter>

#include "RegisterMapping.h"

namespace Emu68::M68k::Interpreter {

template<uint8_t SrcMode, uint8_t SrcReg, uint8_t DstMode, uint8_t DstReg, class Type>
void MOVE(uint32_t opcode)
{
    Type value;

    PC += 2;
    
    // Fetch data from source, either using generic function or templated fetch
    if constexpr (SrcMode == DEFAULT_EA || SrcReg == DEFAULT_EA) {
        const uint8_t srcMode = (SrcMode == DEFAULT_EA) ? (opcode >> 3) & 7 : SrcMode;
        const uint8_t srcReg  = (SrcReg == DEFAULT_EA) ? (opcode) & 7 : SrcReg;
        value = loadFromEA<Type>(srcMode, srcReg);
    } else {
        value = loadFromEA<SrcMode, SrcReg, Type>();
    }
    
    bool is_movea;

    // Store data to destination, either generic or templated store
    if constexpr (DstMode == DEFAULT_EA || DstReg == DEFAULT_EA) {
        const uint8_t dstMode = (DstMode == DEFAULT_EA) ? (opcode >> 6) & 7 : DstMode;
        const uint8_t dstReg  = (DstReg == DEFAULT_EA) ? (opcode >> 9) & 7 : DstReg;
        storeToEA<Type>(dstMode, dstReg, value);
        is_movea = (dstMode == 1);
    } else {
        storeToEA<DstMode, DstReg, Type>(value);
        is_movea = (DstMode == 1);
    }

    // If instruction was not MOVEA, update flags
    if (!is_movea) {
        uint32_t sr = SR & ~SR_NZVC;

        if (value == 0) {
            sr |= SR_Z;
        } else if (value < 0) {
            sr |= SR_N;
        }

        SR = sr;
    }
}

// hot on the source side: registers, simple indirects, d16(An), and the
// mode-7 forms that show up constantly in Kickstart/AmigaOS code —
// abs.W ("4.w"), abs.L, d16(PC), immediate.
inline constexpr uint32_t SrcSpecializeMask = 
      classBit(EAClass::Dn)      | classBit(EAClass::An)
    | classBit(EAClass::Ind)     | classBit(EAClass::IndPost)
    | classBit(EAClass::IndPre)  | classBit(EAClass::D16An)
    | classBit(EAClass::AbsW)    | classBit(EAClass::AbsL)
    | classBit(EAClass::D16PC)   | classBit(EAClass::Imm);
    // D8AnXn / D8PCXn omitted: extension-word decode is rare enough
    // to leave on the MOVE_Generic path.

inline constexpr uint32_t DstSpecializeMask = 
      classBit(EAClass::Dn)      | classBit(EAClass::An)
    | classBit(EAClass::Ind)     | classBit(EAClass::IndPost)
    | classBit(EAClass::IndPre)  | classBit(EAClass::D16An)
    | classBit(EAClass::AbsL);
    // AbsW omitted in stores as this is most likely rare enough.
    // D8AnXn omitted; D16PC/D8PCXn/Imm are not valid destinations anyway.

template <class Type, uint32_t SrcMask, uint32_t DstMask>
consteval void fillMoveTable(std::array<INTERPRET_Function, 4096>& table)
{
    auto fillOne = [&]<std::size_t I>() {
        constexpr std::size_t DstI = I / 61;
        constexpr std::size_t SrcI = I % 61;
        constexpr std::size_t DstMode = DstI >> 3;
        constexpr std::size_t DstReg  = DstI & 7;
        constexpr std::size_t SrcMode = SrcI >> 3;
        constexpr std::size_t SrcReg  = SrcI & 7;

        /* Automatically skip An as source or destination mode if size is 8 bit */
        if constexpr (sizeof(Type) > 1 || (SrcMode != 1 && DstMode != 1)) {
            constexpr int base = int((DstI & 7) << 9) | int(DstMode << 6);

            constexpr bool hotSrc =
                (SrcMask >> static_cast<unsigned>(classifyEA(SrcMode, SrcReg))) & 1u;
            constexpr bool hotDst =
                (DstMask >> static_cast<unsigned>(classifyEA(DstMode, DstReg))) & 1u;

            constexpr uint8_t S  = hotSrc ? SrcMode : DEFAULT_EA;
            constexpr uint8_t Sr = hotSrc ? SrcReg  : DEFAULT_EA;
            constexpr uint8_t D  = hotDst ? DstMode : DEFAULT_EA;
            constexpr uint8_t Dr = hotDst ? DstReg  : DEFAULT_EA;

            table[base + SrcI] = MOVE<S, Sr, D, Dr, Type>;
        }
    };

    [&]<std::size_t... Is>(std::index_sequence<Is...>) {
        (fillOne.template operator()<Is>(), ...);
    }(std::make_index_sequence<58 * 61>{});
}

template<class type>
static consteval std::array<INTERPRET_Function, 4096> buildInsnTable()
{
    std::array<INTERPRET_Function, 4096> table{};
    for (auto& e : table) e = ILLEGAL;

    fillMoveTable<type, SrcSpecializeMask, DstSpecializeMask>(table);

    return table;
}

static constexpr auto InsnTable_L __attribute__((used,section(".int.jumptable.2"))) = buildInsnTable<LONG>();
static constexpr auto InsnTable_W __attribute__((used,section(".int.jumptable.3"))) = buildInsnTable<WORD>();
static constexpr auto InsnTable_B __attribute__((used,section(".int.jumptable.1"))) = buildInsnTable<BYTE>();

} // Emu68::M68k::Interpreter
