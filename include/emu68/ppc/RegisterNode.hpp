#pragma once

#include <cstdint>
#include <emu68/Node>

namespace Emu68::PPC {

struct RegisterNode : public Emu68::Node {
    uint8_t rn_RegNum;
    uint8_t rn_ARM;
    uint8_t rn_Dirty;
};

} // namespace Emu68::PPC
