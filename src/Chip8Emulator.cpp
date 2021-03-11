#include "Chip8Emulator.h"

#include <cassert>

namespace
{

constexpr uint8_t vf_index = 15;

std::pair<uint8_t, uint8_t> get_regs_math_ops(int16_t instruction) {
    return {
        static_cast<uint8_t>((instruction & 0x0F00) >> 8),
        static_cast<uint8_t>((instruction & 0x00F0) >> 4)
    };
}

} // namespace

Chip8Emulator::Action Chip8Emulator::process_next_instruction() {
    if (size_t(program_counter - 1) >= memory.size()) // would be better to crash on the instruction that caused this
        return Action::Crash;
    const uint8_t hi           = memory[program_counter];
    const uint8_t lo           = memory[program_counter + 1];
    const uint16_t instruction = (static_cast<uint16_t>(hi) << 8 | lo);
    cycle_count++;

    constexpr auto cycles_for_decrement = clock_speed_hz / 60;
    if (cycle_count % cycles_for_decrement == 0) {
        if (delay_timer != 0)
            delay_timer--;
        if (sound_timer != 0)
            sound_timer--;
    }

    switch (instruction & 0xF000) {
    case 0x0000:
        if (instruction == 0x00E0) {
            return op_cls(instruction);
        } else if (instruction == 0x00EE) {
            return op_ret(instruction);
        } else {
            return op_sys(instruction);
        }
    case 0x1000: return op_jp(instruction);
    case 0x2000: return op_call(instruction);
    case 0x3000: return op_se_byte(instruction);
    case 0x4000: return op_sne(instruction);
    case 0x5000: return op_se_reg(instruction);
    case 0x6000: return op_ld_byte(instruction);
    case 0x7000: return op_add(instruction);
    case 0x8000: {
        switch (instruction & 0xF) {
        case 0x0: return op_ld_reg(instruction);
        case 0x1: return op_or(instruction);
        case 0x2: return op_and(instruction);
        case 0x3: return op_xor(instruction);
        case 0x4: return op_add_reg(instruction);
        case 0x5: return op_sub(instruction);
        case 0x6: return op_shr(instruction);
        case 0x7: return op_subn(instruction);
        case 0xE: return op_shl(instruction);
        default: return Action::Crash;
        }
    }
    case 0x9000: return op_sne_reg(instruction);
    case 0xA000: return op_ld_addr(instruction);
    case 0xB000: return op_jp_offset(instruction);
    case 0xC000: return op_rnd(instruction);
    case 0xD000: return op_drw(instruction);
    case 0xE000: {
        switch (instruction & 0xFF) {
        case 0x9E: return op_skp(instruction);
        case 0xA1: return op_sknp(instruction);
        default: return Action::Crash;
        }
    }
    case 0xF000: {
        switch (instruction & 0xFF) {
        case 0x07: return op_ld_dt(instruction);
        case 0x0A: return op_ld_wait_key(instruction);
        case 0x15: return op_ld_set_dt(instruction);
        case 0x18: return op_ld_st(instruction);
        case 0x1E: return op_add_idx_reg(instruction);
        case 0x29: return op_ld_font(instruction);
        case 0x33: return op_ld_bcd(instruction);
        case 0x55: return op_ld_reg_dump(instruction);
        case 0x65: return op_ld_reg_store(instruction);
        default: return Action::Crash;
        }
    }
    }
    return Action::Crash;
}

void Chip8Emulator::key_pressed_upon_wait(uint8_t key) noexcept {
    assert(key < 16);
    data_registers[wait_for_key_reg_idx] = key;
}

Chip8Emulator::Action Chip8Emulator::op_cls([[maybe_unused]] uint16_t instruction) {
    for (auto& col : pixel_memory)
        std::fill(col.begin(), col.end(), false);
    program_counter += 2;
    return Action::DoNothing;
}

Chip8Emulator::Action Chip8Emulator::op_ret([[maybe_unused]] uint16_t instruction) {
    if (stack.empty())
        return Action::Crash;
    const uint16_t new_pc = stack.top();
    stack.pop();
    program_counter = new_pc + 2;
    return Action::DoNothing;
}

Chip8Emulator::Action Chip8Emulator::op_sys([[maybe_unused]] uint16_t instruction) {
    // ignore this instruction
    program_counter += 2;
    return Action::DoNothing;
}

Chip8Emulator::Action Chip8Emulator::op_jp(uint16_t instruction) {
    program_counter = instruction & 0x0FFF;
    return Action::DoNothing;
}

Chip8Emulator::Action Chip8Emulator::op_call(uint16_t instruction) {
    if (stack.full())
        return Action::Crash;
    stack.push(program_counter);
    program_counter = instruction & 0x0FFF;
    return Action::DoNothing;
}

Chip8Emulator::Action Chip8Emulator::op_se_byte(uint16_t instruction) {
    const uint8_t val       = instruction & 0xFF;
    const uint8_t reg_index = (instruction & 0x0F00) >> 8;
    if (data_registers[reg_index] == val) {
        program_counter += 4;
    } else {
        program_counter += 2;
    }
    return Action::DoNothing;
}

Chip8Emulator::Action Chip8Emulator::op_sne(uint16_t instruction) {
    const uint8_t val       = instruction & 0xFF;
    const uint8_t reg_index = (instruction & 0x0F00) >> 8;
    if (data_registers[reg_index] != val) {
        program_counter += 4;
    } else {
        program_counter += 2;
    }
    return Action::DoNothing;
}

Chip8Emulator::Action Chip8Emulator::op_se_reg(uint16_t instruction) {
    const uint8_t reg1_index = (instruction & 0x0F00) >> 8;
    const uint8_t reg2_index = (instruction & 0x00F0) >> 4;
    if (data_registers[reg1_index] == data_registers[reg2_index]) {
        program_counter += 4;
    } else {
        program_counter += 2;
    }
    return Action::DoNothing;
}

Chip8Emulator::Action Chip8Emulator::op_ld_byte(uint16_t instruction) {
    const uint8_t reg_index   = (instruction & 0x0F00) >> 8;
    const uint8_t val         = instruction & 0xFF;
    data_registers[reg_index] = val;
    program_counter += 2;
    return Action::DoNothing;
}

Chip8Emulator::Action Chip8Emulator::op_add(uint16_t instruction) {
    const uint8_t reg_index = (instruction & 0x0F00) >> 8;
    const uint8_t val       = instruction & 0xFF;
    data_registers[reg_index] += val; // let the overflow happen - intended behaviour
    program_counter += 2;
    return Action::DoNothing;
}

Chip8Emulator::Action Chip8Emulator::op_ld_reg(uint16_t instruction) {
    const auto [reg_x_idx, reg_y_idx] = get_regs_math_ops(instruction);
    data_registers[reg_x_idx]         = data_registers[reg_y_idx];
    program_counter += 2;
    return Action::DoNothing;
}

Chip8Emulator::Action Chip8Emulator::op_or(uint16_t instruction) {
    const auto [reg_x_idx, reg_y_idx] = get_regs_math_ops(instruction);
    data_registers[reg_x_idx] |= data_registers[reg_y_idx];
    program_counter += 2;
    return Action::DoNothing;
}

Chip8Emulator::Action Chip8Emulator::op_and(uint16_t instruction) {
    const auto [reg_x_idx, reg_y_idx] = get_regs_math_ops(instruction);
    data_registers[reg_x_idx] &= data_registers[reg_y_idx];
    program_counter += 2;
    return Action::DoNothing;
}

Chip8Emulator::Action Chip8Emulator::op_xor(uint16_t instruction) {
    const auto [reg_x_idx, reg_y_idx] = get_regs_math_ops(instruction);
    data_registers[reg_x_idx] ^= data_registers[reg_y_idx];
    program_counter += 2;
    return Action::DoNothing;
}

Chip8Emulator::Action Chip8Emulator::op_add_reg(uint16_t instruction) {
    const auto [reg_x_idx, reg_y_idx] = get_regs_math_ops(instruction);
    const uint8_t old                 = data_registers[reg_x_idx];
    data_registers[reg_x_idx] += data_registers[reg_y_idx];
    if (data_registers[reg_x_idx] < old) // overflow
        data_registers[vf_index] = 1;
    else
        data_registers[vf_index] = 0;
    program_counter += 2;
    return Action::DoNothing;
}

Chip8Emulator::Action Chip8Emulator::op_sub(uint16_t instruction) {
    const auto [reg_x_idx, reg_y_idx] = get_regs_math_ops(instruction);
    const uint8_t old                 = data_registers[reg_x_idx];
    data_registers[reg_x_idx] -= data_registers[reg_y_idx];
    if (data_registers[reg_x_idx] > old) { // borrow
        data_registers[vf_index] = 0;
    } else {
        data_registers[vf_index] = 1;
    }
    program_counter += 2;
    return Action::DoNothing;
}

Chip8Emulator::Action Chip8Emulator::op_shr(uint16_t instruction) {
    const auto [reg_x_idx, reg_y_idx] = get_regs_math_ops(instruction);
    data_registers[vf_index]          = data_registers[reg_x_idx] & 0x0001;
    data_registers[reg_x_idx] >>= 1;
    program_counter += 2;
    return Action::DoNothing;
}

Chip8Emulator::Action Chip8Emulator::op_subn(uint16_t instruction) {
    const auto [reg_x_idx, reg_y_idx] = get_regs_math_ops(instruction);
    const uint8_t old                 = data_registers[reg_x_idx];
    data_registers[reg_x_idx]         = data_registers[reg_y_idx] - data_registers[reg_x_idx];
    if (data_registers[reg_x_idx] > old) { // borrow
        data_registers[vf_index] = 0;
    } else {
        data_registers[vf_index] = 1;
    }
    program_counter += 2;
    return Action::DoNothing;
}

Chip8Emulator::Action Chip8Emulator::op_shl(uint16_t instruction) {
    const auto [reg_x_idx, reg_y_idx] = get_regs_math_ops(instruction);
    data_registers[vf_index]          = data_registers[reg_x_idx] & 0b1000'0000;
    data_registers[reg_x_idx] <<= 1;
    program_counter += 2;
    return Action::DoNothing;
}

Chip8Emulator::Action Chip8Emulator::op_sne_reg(uint16_t instruction) {
    const uint8_t reg1_index = (instruction & 0x0F00) >> 8;
    const uint8_t reg2_index = (instruction & 0x00F0) >> 4;
    if (data_registers[reg1_index] != data_registers[reg2_index]) {
        program_counter += 4;
    } else {
        program_counter += 2;
    }
    return Action::DoNothing;
}

Chip8Emulator::Action Chip8Emulator::op_ld_addr(uint16_t instruction) {
    index_register = instruction & 0x0FFF;
    program_counter += 2;
    return Action::DoNothing;
}

Chip8Emulator::Action Chip8Emulator::op_jp_offset(uint16_t instruction) {
    program_counter = (instruction & 0x0FFF) + data_registers[0];
    return Action::DoNothing;
}

Chip8Emulator::Action Chip8Emulator::op_rnd(uint16_t instruction) {
    const uint8_t vx_index   = (instruction & 0x0F00) >> 8;
    const uint8_t val        = instruction & 0xFF;
    data_registers[vx_index] = val & rng.next();
    program_counter += 2;
    return Action::DoNothing;
}

Chip8Emulator::Action Chip8Emulator::op_drw(uint16_t instruction) {
    const uint8_t vx     = data_registers[(instruction & 0x0F00) >> 8];
    const uint8_t vy     = data_registers[(instruction & 0x00F0) >> 4];
    const uint8_t height = instruction & 0xF;

    bool any_flip = false;
    for (size_t i = 0; i < height; ++i) {
        for (size_t j = 0; j < 8; ++j) {
            if (index_register + i >= memory.size())
                return Action::Crash;
            const bool bit_set    = memory[index_register + i] & (1 << (7 - j));
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
        data_registers[vf_index] = 1;
    }

    program_counter += 2;
    return Action::ReDraw;
}

Chip8Emulator::Action Chip8Emulator::op_skp(uint16_t instruction) {
    const uint8_t reg_idx   = (instruction & 0x0F00) >> 8;
    const uint8_t input_idx = data_registers[reg_idx];
    if (input_idx >= 16)
        return Action::Crash;

    if (input_state[input_idx]) {
        program_counter += 4;
    } else {
        program_counter += 2;
    }
    return Action::DoNothing;
}

Chip8Emulator::Action Chip8Emulator::op_sknp(uint16_t instruction) {
    const uint8_t reg_idx   = (instruction & 0x0F00) >> 8;
    const uint8_t input_idx = data_registers[reg_idx];
    if (input_idx >= 16)
        return Action::Crash;

    if (!input_state[input_idx]) {
        program_counter += 4;
    } else {
        program_counter += 2;
    }
    return Action::DoNothing;
}

Chip8Emulator::Action Chip8Emulator::op_ld_dt(uint16_t instruction) {
    const uint8_t idx   = (instruction & 0x0F00) >> 8;
    data_registers[idx] = delay_timer;
    program_counter += 2;
    return Action::DoNothing;
}

Chip8Emulator::Action Chip8Emulator::op_ld_wait_key(uint16_t instruction) {
    wait_for_key_reg_idx = (instruction & 0x0F00) >> 8;
    program_counter += 2;
    return Action::WaitForInput;
}

Chip8Emulator::Action Chip8Emulator::op_ld_set_dt(uint16_t instruction) {
    const uint8_t idx = (instruction & 0x0F00) >> 8;
    delay_timer       = data_registers[idx];
    program_counter += 2;
    return Action::DoNothing;
}

Chip8Emulator::Action Chip8Emulator::op_ld_st(uint16_t instruction) {
    const uint8_t idx = (instruction & 0x0F00) >> 8;
    sound_timer       = data_registers[idx];
    program_counter += 2;
    return Action::DoNothing;
}

Chip8Emulator::Action Chip8Emulator::op_add_idx_reg(uint16_t instruction) {
    const uint8_t idx = (instruction & 0x0F00) >> 8;
    index_register += data_registers[idx];
    program_counter += 2;
    return Action::DoNothing;
}

Chip8Emulator::Action Chip8Emulator::op_ld_font(uint16_t instruction) {
    const uint8_t reg_index  = (instruction & 0x0F00) >> 8;
    const uint8_t val_to_get = data_registers[reg_index];
    if (val_to_get >= 16)
        return Action::Crash;

    // each font is 5 bytes, they are loaded in at 0
    index_register = val_to_get * 5;
    program_counter += 2;
    return Action::DoNothing;
}

Chip8Emulator::Action Chip8Emulator::op_ld_bcd(uint16_t instruction) {
    const uint8_t reg_index = (instruction & 0x0F00) >> 8;
    const uint8_t val       = data_registers[reg_index];

    const uint8_t hundreds_digit = val / 100;
    const uint8_t tens_digit     = (val % 100) / 10;
    const uint8_t single_digit   = (val % 100) % 10;

    if (size_t(index_register + 2) >= memory.size())
        return Action::Crash;

    memory[index_register]     = hundreds_digit;
    memory[index_register + 1] = tens_digit;
    memory[index_register + 2] = single_digit;

    program_counter += 2;
    return Action::DoNothing;
}

Chip8Emulator::Action Chip8Emulator::op_ld_reg_dump(uint16_t instruction) {
    const uint8_t reg_index = (instruction & 0x0F00) >> 8;
    if (size_t(index_register + reg_index) >= memory.size())
        return Action::Crash;
    for (size_t i = 0; i <= reg_index; ++i) { // TODO: use std::copy
        memory[index_register + i] = data_registers[i];
    }
    program_counter += 2;
    return Action::DoNothing;
}

Chip8Emulator::Action Chip8Emulator::op_ld_reg_store(uint16_t instruction) {
    const uint8_t reg_index = (instruction & 0x0F00) >> 8;
    if (size_t(index_register + reg_index) >= memory.size())
        return Action::Crash;
    for (size_t i = 0; i <= reg_index; ++i) { // TODO: use std::copy
        data_registers[i] = memory[index_register + i];
    }
    program_counter += 2;
    return Action::DoNothing;
}
