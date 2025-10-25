#ifndef _CPP_RETURNSTACK_HPP
#define _CPP_RETURNSTACK_HPP

#include <cstdint>

class ReturnStack {
    static constexpr int RTSTACK_SIZE = 32;
    uint32_t *stack[RTSTACK_SIZE];
    uint32_t stackDepth = 0;

public:
    ReturnStack() : stackDepth(0) { }
    uint32_t *Pop(bool *success);
    void Push(uint32_t *ret_addr);
    void Reset() { stackDepth = 0; }
};

#endif /* _CPP_RETURNSTACK_HPP */
