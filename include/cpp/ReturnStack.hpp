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
