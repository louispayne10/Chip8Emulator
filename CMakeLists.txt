cmake_minimum_required(VERSION 3.7)

project(
    "Chip8 Emulator"
    VERSION 1.0
    LANGUAGES CXX
)

if(PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
  message(
    FATAL_ERROR
      "In-source builds not allowed. Please make a new directory (called a build directory) and run CMake from there."
  )
endif()

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

option(ENABLE_IWYU "Enable include-what-you-use" OFF)
if(ENABLE_IWYU)
  find_program(iwyu_path NAMES include-what-you-use iwyu)
  if(iwyu_path)
    message(STATUS "include-what-you-use found and enabled")
    set(CMAKE_CXX_INCLUDE_WHAT_YOU_USE ${iwyu_path})
  else()
    message(FATAL_ERROR "include-what-you-use requested but executable not found")
  endif()
endif()

find_package(SDL2 REQUIRED)
find_package(sdl2-mixer REQUIRED)

add_subdirectory(src)
