/*
    Copyright © 2019-2025 Michal Schulz <michal.schulz@gmx.de>
    https://github.com/michalsc

    This Source Code Form is subject to the terms of the
    Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed
    with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <emu68/ReturnStack.hpp>

#include "config.h"

namespace Emu68 {

void ReturnStack::push(uint32_t* ret_addr)
{
    if (stackDepth >= RTSTACK_SIZE)
    {
        for (int i = 1; i < RTSTACK_SIZE; i++) {
            stack[i - 1] = stack[i];
        }
        
        stackDepth--;
    }

    stack[stackDepth++] = ret_addr;
}

uint32_t* ReturnStack::pop(bool* success)
{
    uint32_t* ptr;

    if (EMU68_USE_RETURN_STACK && stackDepth > 0)
    {
        ptr = stack[--stackDepth];

        if (success) {
            *success = true;
        }
    }
    else
    {
        ptr = (uint32_t*)0xffffffff;
        
        if (success) {
            *success = false;
        }
    }

    return ptr;
}

} // namespace Emu68
