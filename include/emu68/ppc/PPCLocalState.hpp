#pragma once

#include <cstdint>

namespace Emu68::PPC {

struct PPCLocalState
{
    void *          pls_PPCPtr;
    uint32_t        pls_ARMOffset;
    uint8_t         pls_RegMap[38];
    int32_t         pls_PCRel;
};

} // namespace Emu68::PPC
