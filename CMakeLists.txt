cmake_minimum_required(VERSION 3.7)

project(
    "Chip8 Emulator"
    VERSION 1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(ENABLE_CLANG_TIDY "Enable static analysis with clang-tidy" OFF)
if(ENABLE_CLANG_TIDY)
  find_program(CLANGTIDY clang-tidy)
  if(CLANGTIDY)
    message(STATUS "clang-tidy found and enabled")
    set(CMAKE_CXX_CLANG_TIDY ${CLANGTIDY})
  else()
    message(SEND_ERROR "clang-tidy requested but executable not found")
  endif()
endif()

find_package(SDL2 REQUIRED)

add_subdirectory(src)
