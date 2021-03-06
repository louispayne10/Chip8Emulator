#pragma once

#include <array>
#include <stdexcept>

class StaticStack {
public:
    void push(uint16_t val) {
        if (stack_ptr == 16)
            throw std::runtime_error("stack has reached maximum size (15)");

        stack[stack_ptr] = val;
        stack_ptr++;
    }

    void pop() {
        if (stack_ptr == 0)
            throw std::runtime_error("cannot pop from an empty stack");

        stack_ptr--;
    }

    uint16_t top() const noexcept {
        return stack[stack_ptr - 1];
    }

private:
    std::array<uint16_t, 16> stack{}; // stack size on chip8 is 16
    uint8_t stack_ptr = 0;
};
