# Chip8 Emulator
This is an implementation of a chip8 emulator. It is written using C++17 and SDL2. 

## Build and run
Requires `cmake`, `SDL2` and `SDL_Mixer 2.0`. Example build using `vcpkg`:
```
mkdir build
cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build . --config Release
```
Then to run:
```
./chip8 /path/to/rom
```

The controls are mapped to the numpad number keys `0-9` as well as the keys `A`, `B`, `C`, `D`, `E` and `F`. If you would like to change these you have to change these in the source file. This can be found in `main.cpp` in the array called `key_map`.

## Acknowledgements
The sound `tone.wav` in the assets directory is from [here](https://freesound.org/people/austin1234575/sounds/213795/).
