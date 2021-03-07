#include "Chip8Emulator.h"

#define SDL_MAIN_HANDLED
#include "SDL.h"
#include "SDL_mixer.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <thread> // this_thread
#include <vector>

using namespace std::chrono;

namespace
{
constexpr uint32_t sprite_scale    = 10;
constexpr auto time_between_cycles = system_clock::duration(seconds(1)) / Chip8Emulator::clock_speed_hz;
constexpr auto frames_per_second   = 60;
constexpr auto time_between_draws  = system_clock::duration(seconds(1)) / frames_per_second;

constexpr std::array<SDL_Scancode, 16> key_map = {
    SDL_SCANCODE_KP_0, // 0
    SDL_SCANCODE_KP_7, // 1
    SDL_SCANCODE_KP_8, // 2
    SDL_SCANCODE_KP_9, // 3
    SDL_SCANCODE_KP_4, // 4
    SDL_SCANCODE_KP_5, // 5
    SDL_SCANCODE_KP_6, // 6
    SDL_SCANCODE_KP_1, // 7
    SDL_SCANCODE_KP_2, // 8
    SDL_SCANCODE_KP_3, // 9
    SDL_SCANCODE_A,    // A
    SDL_SCANCODE_B,    // B
    SDL_SCANCODE_C,    // C
    SDL_SCANCODE_D,    // D
    SDL_SCANCODE_E,    // E
    SDL_SCANCODE_F,    // F
};

struct SdlWindowDeleter {
    void operator()(SDL_Window* wnd) {
        SDL_DestroyWindow(wnd);
    }
};
struct SdlRendererDeleter {
    void operator()(SDL_Renderer* rnd) { SDL_DestroyRenderer(rnd); }
};
struct SdlTextureDeleter {
    void operator()(SDL_Texture* txt) { SDL_DestroyTexture(txt); }
};
struct SdlMixChunkDeleter {
    void operator()(Mix_Chunk* chunk) { Mix_FreeChunk(chunk); }
};

} // namespace

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cout << "Usage: " << argv[0] << " path_to_rom\n";
        return -1;
    }

    std::ifstream file(argv[1], std::ios_base::binary);
    if (!file.is_open()) {
        std::cout << "Could not find rom " << argv[1] << '\n';
        return -1;
    }

    const std::vector<uint8_t> program_bytes{ std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>() };
    if (program_bytes.empty()) {
        std::cout << "rom is empty\n";
        return -1;
    }

    // checking for errors with these using raii is a bit of a PITA, if needed create a scopeguard for the cleanup functions
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_AUDIO) != 0) {
        std::cout << "SDL_Init failed. Error: " << SDL_GetError() << "\n";
        return -1;
    };
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) == -1) {
        std::cout << "Mix_OpenAudio: " << Mix_GetError() << "\n";
        return -1;
    }

    std::unique_ptr<SDL_Window, SdlWindowDeleter> window(SDL_CreateWindow("Chip8 Emulator", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 64 * sprite_scale, 32 * sprite_scale, SDL_WINDOW_OPENGL));
    if (!window) {
        std::cout << "Could not create window\n";
        return -1;
    }

    std::unique_ptr<SDL_Renderer, SdlRendererDeleter> renderer(SDL_CreateRenderer(window.get(), -1, 0));
    if (!renderer) {
        std::cout << "Could not create renderer\n";
        return -1;
    }
    if (SDL_RenderSetLogicalSize(renderer.get(), 64, 32) != 0) {
        std::cout << "Could not set renderer logical size: " << SDL_GetError() << "\n";
        return -1;
    }

    // This texture will act as our framebuffer
    std::unique_ptr<SDL_Texture, SdlTextureDeleter> texture(SDL_CreateTexture(renderer.get(), SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, 64, 32));
    if (!texture) {
        std::cout << "Could not create texture for framebuffer: " << SDL_GetError() << "\n";
        return -1;
    }

    std::unique_ptr<Mix_Chunk, SdlMixChunkDeleter> sound_effect(Mix_LoadWAV("assets/tone.wav"));
    if (!sound_effect) {
        std::cout << "Failed to load sound effect. SDL_Mixer Error:" << Mix_GetError() << "\n";
        return -1;
    }
    Mix_Volume(-1, MIX_MAX_VOLUME / 8);

    Chip8Emulator emulator(program_bytes.begin(), program_bytes.end());
    bool run                                 = true;
    steady_clock::time_point last_cycle_time = steady_clock::now();
    steady_clock::time_point last_draw_time  = steady_clock::now();
    bool need_redraw                         = false;
    while (run) {
        const auto time_passed = steady_clock::now() - last_cycle_time;
        if (time_between_cycles > time_passed)
            std::this_thread::sleep_for(time_between_cycles - time_passed);

        const Chip8Emulator::Action action = emulator.process_next_instruction();
        last_cycle_time                    = steady_clock::now();

        SDL_Event e;
        while (SDL_PollEvent(&e) != 0) {
            if (e.type == SDL_QUIT) {
                run = false;
            } else if (e.type == SDL_KEYDOWN) {
                const auto keyIt = std::find(key_map.begin(), key_map.end(), e.key.keysym.scancode);
                if (keyIt != key_map.end()) {
                    const auto idx                = std::distance(key_map.begin(), keyIt);
                    emulator.input_buttons()[idx] = true;
                }
            } else if (e.type == SDL_KEYUP) {
                const auto keyIt = std::find(key_map.begin(), key_map.end(), e.key.keysym.scancode);
                if (keyIt != key_map.end()) {
                    const auto idx                = std::distance(key_map.begin(), keyIt);
                    emulator.input_buttons()[idx] = false;
                }
            }
        }

        if (action == Chip8Emulator::Action::Crash) {
            std::cout << "Emulated program has crashed\n";
            return -1;
        } else if (action == Chip8Emulator::Action::WaitForInput) {
            while (true) {
                const int res = SDL_WaitEvent(&e);
                if (res == 0) {
                    std::cout << "Wait event error\n";
                    return -1;
                }
                if (e.type == SDL_KEYDOWN) {
                    const auto key_it = std::find(key_map.begin(), key_map.end(), e.key.keysym.scancode);
                    if (key_it != key_map.end()) {
                        const auto idx = std::distance(key_map.begin(), key_it);
                        emulator.key_pressed_upon_wait(static_cast<uint8_t>(idx));
                        last_cycle_time = steady_clock::now();
                        break;
                    }
                }
            }
        } else if (action == Chip8Emulator::Action::ReDraw) {
            need_redraw = true;
        }

        if (need_redraw && ((steady_clock::now() - last_draw_time) > time_between_draws)) {
            std::array<uint32_t, 64 * 32> sdl_pixel_data; // NOLINT - no need to initialise
            size_t index = 0;
            for (const auto& row : emulator.video_memory()) {
                for (const auto& pixel : row) {
                    if (pixel) {
                        sdl_pixel_data[index] = 0xFFFFFFFF;
                    } else {
                        sdl_pixel_data[index] = 0xFF000000;
                    }
                    index++;
                }
            }

            SDL_UpdateTexture(texture.get(), nullptr, sdl_pixel_data.data(), 64 * 4);
            SDL_RenderClear(renderer.get());
            SDL_RenderCopy(renderer.get(), texture.get(), nullptr, nullptr);
            SDL_RenderPresent(renderer.get());

            last_draw_time = steady_clock::now();
            need_redraw    = false;
        }

        if (emulator.should_play_sound()) {
            if (Mix_PlayChannelTimed(-1, sound_effect.get(), -1, -1) == -1) {
                std::cout << "Error playing sound. Error: " << Mix_GetError() << "\n";
            }
        } else {
            Mix_HaltChannel(-1);
        }
    }

    Mix_Quit();
    SDL_Quit();

    return 0;
}
