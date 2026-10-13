/*
    Copyright © 2019-2025 Michal Schulz <michal.schulz@gmx.de>
    https://github.com/michalsc

    This Source Code Form is subject to the terms of the
    Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed
    with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

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
