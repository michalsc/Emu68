/*
    Copyright © 2019-2025 Michal Schulz <michal.schulz@gmx.de>
    https://github.com/michalsc

    This Source Code Form is subject to the terms of the
    Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed
    with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

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
