#pragma once

#include <array>
#include <stdexcept>

class StaticStack {
public:
    void push(uint16_t val) {
        if (full())
            throw std::runtime_error("stack has reached maximum size");

        stack[stack_ptr] = val;
        stack_ptr++;
    }

    void pop() {
        if (empty())
            throw std::runtime_error("cannot pop from an empty stack");

        stack_ptr--;
    }

    uint16_t top() const {
        if (empty())
            throw std::runtime_error("cannot get top from empty stack");

        return stack[stack_ptr - 1];
    }

    [[nodiscard]] bool empty() const noexcept {
        return stack_ptr == 0;
    }

    [[nodiscard]] bool full() const noexcept {
        return stack_ptr == stack.size();
    }

private:
    std::array<uint16_t, 16> stack{}; // stack size on chip8 is 16
    uint8_t stack_ptr = 0;
};
