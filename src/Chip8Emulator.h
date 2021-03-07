#pragma once

#include <algorithm>
#include <array>

#include "RandomNumberGenerator.h"
#include "StaticStack.h"

constexpr uint16_t load_address                  = 0x200;
constexpr std::array<uint8_t, 80> dec_pixel_data = {
    0xF0, 0x90, 0x90, 0x90, 0xF0, //0
    0x20, 0x60, 0x20, 0x20, 0x70, //1
    0xF0, 0x10, 0xF0, 0x80, 0xF0, //2
    0xF0, 0x10, 0xF0, 0x10, 0xF0, //3
    0x90, 0x90, 0xF0, 0x10, 0x10, //4
    0xF0, 0x80, 0xF0, 0x10, 0xF0, //5
    0xF0, 0x80, 0xF0, 0x90, 0xF0, //6
    0xF0, 0x10, 0x20, 0x40, 0x40, //7
    0xF0, 0x90, 0xF0, 0x90, 0xF0, //8
    0xF0, 0x90, 0xF0, 0x10, 0xF0, //9
    0xF0, 0x90, 0xF0, 0x90, 0x90, //A
    0xE0, 0x90, 0xE0, 0x90, 0xE0, //B
    0xF0, 0x80, 0x80, 0x80, 0xF0, //C
    0xE0, 0x90, 0x90, 0x90, 0xE0, //D
    0xF0, 0x80, 0xF0, 0x80, 0xF0, //E
    0xF0, 0x80, 0xF0, 0x80, 0x80  //F
};

class Chip8Emulator {
public:
    static constexpr int clock_speed_hz = 540;

    template <typename InputIt>
    Chip8Emulator(InputIt start, InputIt end)
        : program_counter(load_address) {
        if (std::distance(start, end) > static_cast<int64_t>(memory.size())) {
            throw std::runtime_error("Not enough memory to load program");
        }

        // copy number fonts into memory at address 0 and then the actual program at the load address
        std::copy(dec_pixel_data.begin(), dec_pixel_data.end(), memory.data());
        std::copy(start, end, memory.data() + load_address);
    }

    enum class Action {
        DoNothing,
        ReDraw,
        WaitForInput,
        Crash
    };
    [[nodiscard]] Action process_next_instruction();

    std::array<bool, 16>& input_buttons() noexcept { return input_state; }
    const std::array<std::array<bool, 64>, 32>& video_memory() const noexcept { return pixel_memory; }
    void key_pressed_upon_wait(uint8_t key) noexcept;

    [[nodiscard]] bool should_play_sound() const noexcept {
        return sound_timer != 0;
    }

private:
    std::array<uint8_t, 4096> memory{};
    std::array<std::array<bool, 64>, 32> pixel_memory{};
    StaticStack stack{};
    std::array<uint8_t, 16> data_registers{};
    uint16_t index_register{};
    uint16_t program_counter{};

    std::array<bool, 16> input_state{};
    uint8_t delay_timer{};
    uint8_t sound_timer{};
    uint8_t wait_for_key_reg_idx = 0; // When the opcode to wait for a keypress is used we use this to "remember" which reg to put it in

    uint64_t cycle_count = 0;
    RandomNumberGenerator rng;
};
