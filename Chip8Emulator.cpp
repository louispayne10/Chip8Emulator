#include "Chip8Emulator.h"

#include <cassert>
#include <iostream>
#include <random>

namespace
{

class RandomNumberGenerator {
public:
    RandomNumberGenerator()
        : gen(rd()),
          distrib(0, 255) {
    }

    uint8_t next() {
        return static_cast<uint8_t>(distrib(gen));
    }

private:
    std::random_device rd;
    std::mt19937 gen;
    std::uniform_int_distribution<uint16_t> distrib; // for some reason unsigned char is not a defined template parameter
};

RandomNumberGenerator rng;

constexpr uint8_t vf_index   = 15;
constexpr int clock_speed_hz = 540;

} // namespace

namespace
{

void handle_clear_screen(std::array<std::array<bool, 64>, 32>& pixel_memory) {
    for (auto& col : pixel_memory)
        std::fill(col.begin(), col.end(), false);
}

void handle_ret_subroutine(uint16_t& program_counter, StaticStack& stack) {
    const uint16_t new_pc = stack.top();
    stack.pop();
    program_counter = new_pc + 2;
}

void handle_goto_addr(uint16_t instruction, uint16_t& program_counter) {
    program_counter = instruction & 0x0FFF;
}

void handle_call_subroutine(uint16_t instruction, uint16_t& program_counter, StaticStack& stack) {
    stack.push(program_counter);
    program_counter = instruction & 0x0FFF;
}

void handle_skip_eq_nn(uint16_t instruction, uint16_t& program_counter, const std::array<uint8_t, 16>& data_registers) {
    const uint8_t nn        = instruction & 0xFF;
    const uint8_t reg_index = (instruction & 0x0F00) >> 8;
    if (data_registers[reg_index] == nn) {
        program_counter += 4;
    } else {
        program_counter += 2;
    }
}

void handle_skip_neq_nn(uint16_t instruction, uint16_t& program_counter, const std::array<uint8_t, 16>& data_registers) {
    const uint8_t nn        = instruction & 0xFF;
    const uint8_t reg_index = (instruction & 0x0F00) >> 8;
    if (data_registers[reg_index] != nn) {
        program_counter += 4;
    } else {
        program_counter += 2;
    }
}

void handle_skip_eq_vx_vy(uint16_t instruction, uint16_t& program_counter, const std::array<uint8_t, 16>& data_registers) {
    const uint8_t reg1_index = (instruction & 0x0F00) >> 8;
    const uint8_t reg2_index = (instruction & 0x00F0) >> 4;
    if (data_registers[reg1_index] == data_registers[reg2_index]) {
        program_counter += 4;
    } else {
        program_counter += 2;
    }
}

void handle_set_register_to_nn(uint16_t instruction, std::array<uint8_t, 16>& data_registers) {
    const uint8_t reg_index   = (instruction & 0x0F00) >> 8;
    const uint8_t nn          = instruction & 0xFF;
    data_registers[reg_index] = nn;
}

void handle_add_nn_to_register(uint16_t instruction, std::array<uint8_t, 16>& data_registers) {
    const uint8_t reg_index = (instruction & 0x0F00) >> 8;
    const uint8_t nn        = instruction & 0xFF;
    data_registers[reg_index] += nn;
}

void handle_math_bitop(uint16_t instruction, std::array<uint8_t, 16>& data_registers) {
    const uint8_t type       = instruction & 0x000F;
    const uint8_t regx_index = (instruction & 0x0F00) >> 8;
    const uint8_t regy_index = (instruction & 0x00F0) >> 4;
    uint8_t& vx              = data_registers[regx_index];
    uint8_t& vy              = data_registers[regy_index];
    uint8_t& vf              = data_registers[vf_index];

    switch (type) {
    case 0: // assign vx to vy
        vx = vy;
        break;
    case 1: // set vx to (vx | vy)
        vx |= vy;
        break;
    case 2: // set vx to (vx & vy)
        vx &= vy;
        break;
    case 3: // set vx to (vx ^ vy) (XOR)
        vx ^= vy;
        break;
    case 4: // add vx = vx + vy. set vf to 1 on carry. 0 on no carry.
    {
        const uint8_t old = vx;
        vx += vy;
        if (vx < old) // overflow
            vf = 1;
        else
            vf = 0;
    } break;
    case 5: // vx = vx - vy. vf to 0 on borrow. 1 on no borrow.
    {
        const uint8_t old = vx;
        vx -= vy;
        if (vx > old) { // borrow
            vf = 0;
        } else {
            vf = 1;
        }
    } break;
    case 6: // store least significant bit in vx to vf then shift vx to right by 1
        vf = vx & 0x0001;
        vx >>= 1;
        break;
    case 7: // set vx = vy - vx. set 0 if there's a borrow. 1 when there isnt.
    {
        const uint8_t old = vx;
        vx                = vy - vx;
        if (vx > old) { // borrow
            vf = 0;
        } else {
            vf = 1;
        }
    } break;
    case 0xE: // store most significant bit in vx to vf then shift vx to the left by 1
        vf = vx & 0b1000'0000;
        vx <<= 1;
        break;
    default:
        assert(false);
    }
}

void handle_skip_neq_vx_vy(uint16_t instruction, uint16_t& program_counter, const std::array<uint8_t, 16>& data_registers) {
    const uint8_t reg1_index = (instruction & 0x0F00) >> 8;
    const uint8_t reg2_index = (instruction & 0x00F0) >> 4;
    if (data_registers[reg1_index] != data_registers[reg2_index]) {
        program_counter += 4;
    } else {
        program_counter += 2;
    }
}

void handle_set_i(uint16_t instruction, uint16_t& index_register) {
    index_register = instruction & 0x0FFF;
}

void handle_jump_addr_offset(uint16_t instruction, uint16_t& program_counter, const std::array<uint8_t, 16>& registers) {
    const uint16_t addr = instruction & 0x0FFF;
    program_counter     = addr + registers[0];
}

void handle_rand(uint16_t instruction, std::array<uint8_t, 16>& data_registers) {
    const uint8_t vx_index = (instruction & 0x0F00) >> 8;
    uint8_t& vx            = data_registers[vx_index];
    const uint8_t nn       = instruction & 0xFF;
    vx                     = nn & rng.next();
}

void handle_display(uint16_t instruction,
                    std::array<std::array<bool, 64>, 32>& pixel_memory,
                    std::array<uint8_t, 4096>& memory,
                    std::array<uint8_t, 16>& registers,
                    uint16_t i_reg) {
    const uint8_t vx     = registers[(instruction & 0x0F00) >> 8];
    const uint8_t vy     = registers[(instruction & 0x00F0) >> 4];
    const uint8_t height = instruction & 0xF;

    bool any_flip = false;
    for (size_t i = 0; i < height; ++i) {
        for (size_t j = 0; j < 8; ++j) {
            const bool bit_set    = memory[i_reg + i] & (1 << (7 - j));
            const uint8_t x_coord = (vx + j) % 64;
            const uint8_t y_coord = (vy + i) % 32;
            bool& pixel           = pixel_memory[y_coord][x_coord];
            if (pixel && bit_set)
                any_flip = true;

            if (bit_set)
                pixel ^= bit_set;
        }
    }

    if (any_flip) {
        registers[vf_index] = 1;
    }
}

void handle_keyop_poll(uint16_t instruction,
                       const std::array<bool, 16>& input_state,
                       const std::array<uint8_t, 16>& registers,
                       uint16_t& program_counter) {
    const uint8_t reg_idx   = (instruction & 0x0F00) >> 8;
    const uint8_t input_idx = registers[reg_idx];
    assert(input_idx < 16);
    const bool is_pressed = input_state[input_idx];

    const uint8_t op = instruction & 0xFF;
    if ((op == 0x9E && is_pressed) || (op == 0xA1 && !is_pressed)) {
        program_counter += 4;
    } else if ((op == 0x9E && !is_pressed) || (op == 0xA1 && is_pressed)) {
        program_counter += 2;
    } else {
        assert(false);
    }
}

void handle_get_delay_timer(uint16_t instruction, uint8_t delay_timer, std::array<uint8_t, 16>& data_registers) {
    const uint8_t idx   = (instruction & 0x0F00) >> 8;
    data_registers[idx] = delay_timer;
}

void handle_wait_for_key(uint16_t instruction, uint8_t& wait_for_key_reg_idx) {
    wait_for_key_reg_idx = (instruction & 0x0F00) >> 8;
}

void handle_set_delay_timer(uint16_t instruction, uint8_t& delay_timer, const std::array<uint8_t, 16>& data_registers) {
    const uint8_t idx = (instruction & 0x0F00) >> 8;
    delay_timer       = data_registers[idx];
}

void handle_set_sound_timer(uint16_t instruction, uint8_t& sound_timer, const std::array<uint8_t, 16>& data_registers) {
    const uint8_t idx = (instruction & 0x0F00) >> 8;
    sound_timer       = data_registers[idx];
}

void handle_add_vx_to_i(uint16_t instruction, uint16_t& index_register, const std::array<uint8_t, 16>& data_registers) {
    const uint8_t idx = (instruction & 0x0F00) >> 8;
    index_register += data_registers[idx];
}

[[nodiscard]] bool handle_set_i_to_font(uint16_t instruction, uint16_t& index_register, const std::array<uint8_t, 16>& data_registers) {
    const uint8_t reg_index  = (instruction & 0x0F00) >> 8;
    const uint8_t val_to_get = data_registers[reg_index];
    if (val_to_get >= 16)
        return false;

    // each font is 5 bytes, they are loaded in at 0
    index_register = val_to_get * 5;
    return true;
}

void handle_set_bcd(uint16_t instruction, const std::array<uint8_t, 16>& data_registers, uint16_t index_register, std::array<uint8_t, 4096>& memory) {
    const uint8_t reg_index = (instruction & 0x0F00) >> 8;
    const uint8_t val       = data_registers[reg_index];

    const uint8_t hundreds_digit = val / 100;
    const uint8_t tens_digit     = (val % 100) / 10;
    const uint8_t single_digit   = (val % 100) % 10;

    memory[index_register]     = hundreds_digit;
    memory[index_register + 1] = tens_digit;
    memory[index_register + 2] = single_digit;
}

void handle_register_dump(uint16_t instruction, const std::array<uint8_t, 16>& data_registers, uint16_t index_register, std::array<uint8_t, 4096>& memory) {
    const uint8_t reg_index = (instruction & 0x0F00) >> 8;
    for (size_t i = 0; i <= reg_index; ++i) {
        memory[index_register + i] = data_registers[i];
    }
}

void handle_reg_load(uint16_t instruction, std::array<uint8_t, 16>& data_registers, uint16_t index_register, const std::array<uint8_t, 4096>& memory) {
    const uint8_t reg_index = (instruction & 0x0F00) >> 8;
    for (size_t i = 0; i <= reg_index; ++i) {
        data_registers[i] = memory[index_register + i];
    }
}

} // namespace

Chip8Emulator::Action Chip8Emulator::process_next_instruction() {
    const uint8_t hi           = memory[program_counter];
    const uint8_t lo           = memory[program_counter + 1];
    const uint16_t instruction = (static_cast<uint16_t>(hi) << 8 | lo);
    cycle_count++;

    std::cout << "instruction: " << std::hex << instruction << std::endl;

    constexpr auto cycles_for_decrement = clock_speed_hz / 60;
    if (cycle_count % cycles_for_decrement == 0) {
        if (delay_timer != 0)
            delay_timer--;
        if (sound_timer != 0)
            sound_timer--;
    }

    switch (instruction & 0xF000) {
    case 0x0000:
        if (instruction == 0x00E0) { // clear display
            handle_clear_screen(pixel_memory);
            program_counter += 2;
            return Action::ReDraw;
        } else if (instruction == 0x00EE) { // return from subroutine
            handle_ret_subroutine(program_counter, stack);
        } else {                      // call machine code routine on old implementations, we ignore it
            assert(instruction != 0); // this would almost certainley be an error
            program_counter += 2;
        }
        break;
    case 0x1000: // goto address
        handle_goto_addr(instruction, program_counter);
        break;
    case 0x2000: // call subroutine
        handle_call_subroutine(instruction, program_counter, stack);
        break;
    case 0x3000: // skip next instruction vx equal to NN
        handle_skip_eq_nn(instruction, program_counter, data_registers);
        break;
    case 0x4000: // skip next instruction vx not equal to NN
        handle_skip_neq_nn(instruction, program_counter, data_registers);
        break;
    case 0x5000: // skip next if vx equals vy
        handle_skip_eq_vx_vy(instruction, program_counter, data_registers);
        break;
    case 0x6000: // set register to NN
        handle_set_register_to_nn(instruction, data_registers);
        program_counter += 2;
        break;
    case 0x7000: // add nn to register
        handle_add_nn_to_register(instruction, data_registers);
        program_counter += 2;
        break;
    case 0x8000: // family of math and bitop instruction
        handle_math_bitop(instruction, data_registers);
        program_counter += 2;
        break;
    case 0x9000: // skips next instruction if vx doesnt equal vy
        handle_skip_neq_vx_vy(instruction, program_counter, data_registers);
        break;
    case 0xA000: // sets i to the address nnn
        handle_set_i(instruction, index_register);
        program_counter += 2;
        break;
    case 0xB000: // jumps to address nnn + v0
        handle_jump_addr_offset(instruction, program_counter, data_registers);
        break;
    case 0xC000: // set vx to result of a bitwise + random
        handle_rand(instruction, data_registers);
        program_counter += 2;
        break;
    case 0xD000: // display a sprite at a coordinate
        handle_display(instruction, pixel_memory, memory, data_registers, index_register);
        program_counter += 2;
        return Action::ReDraw;
    case 0xE000: // input polling
        handle_keyop_poll(instruction, input_state, data_registers, program_counter);
        break;
    case 0xF000: {
        switch (instruction & 0xFF) {
        case 0x07: // sets vx to the value of the delay timer
            handle_get_delay_timer(instruction, delay_timer, data_registers);
            program_counter += 2;
            break;
        case 0x0A: // wait for key press
            handle_wait_for_key(instruction, wait_for_key_reg_idx);
            program_counter += 2;
            return Action::WaitForInput;
        case 0x15: // set delay timer to vx
            handle_set_delay_timer(instruction, delay_timer, data_registers);
            program_counter += 2;
            break;
        case 0x18: // set sound timer to vx
            handle_set_sound_timer(instruction, sound_timer, data_registers);
            program_counter += 2;
            break;
        case 0x1E: // adds vx to I. does not affect VF
            handle_add_vx_to_i(instruction, index_register, data_registers);
            program_counter += 2;
            break;
        case 0x29: // set i register to location of the sprite for the character in VX. chars (0-F) represented by a 4x5
            if (!handle_set_i_to_font(instruction, index_register, data_registers))
                return Action::Crash;
            program_counter += 2;
            break;
        case 0x33: // store binary-coded decimal representation of VX
            handle_set_bcd(instruction, data_registers, index_register, memory);
            program_counter += 2;
            break;
        case 0x55: // register dumnp
            handle_register_dump(instruction, data_registers, index_register, memory);
            program_counter += 2;
            break;
        case 0x65: // register load
            handle_reg_load(instruction, data_registers, index_register, memory);
            program_counter += 2;
            break;
        default:
            return Action::Crash;
        }
    } break;
    }

    return Action::DoNothing;
}

void Chip8Emulator::key_pressed_upon_wait(uint8_t key) noexcept {
    data_registers[wait_for_key_reg_idx] = key;
}
