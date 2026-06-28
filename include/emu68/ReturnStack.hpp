/*
    Copyright © 2019-2025 Michal Schulz <michal.schulz@gmx.de>
    https://github.com/michalsc

    This Source Code Form is subject to the terms of the
    Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed
    with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include <cstdint>

namespace Emu68 {

class ReturnStack {
    static constexpr int RTSTACK_SIZE = 32;
    uint32_t* stack[RTSTACK_SIZE];
    uint32_t stackDepth = 0;

public:
    ReturnStack() : stackDepth(0) { }
    uint32_t* pop(bool* success);
    void push(uint32_t* ret_addr);
    void reset() { stackDepth = 0; }
};

} // namespace Emu68
