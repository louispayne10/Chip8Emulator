# Chip8 Emulator
This is an implementation of a chip8 emulator. It is written using C++17 and SDL2. 

## Build and run
Requires `cmake` and `SDL2`. Example build using `vcpkg`:
```
mkdir build
cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake -- build . --config Release
```
Then to run:
```
./chip8 /path/to/rom
```
