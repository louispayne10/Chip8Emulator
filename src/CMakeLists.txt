add_executable(chip8 main.cpp Chip8Emulator.cpp)

target_compile_options(chip8 PRIVATE 
  $<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/permissive /W4 /WX>
  $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -pedantic -Werror>
)

target_link_libraries(chip8 PRIVATE SDL2::SDL2 SDL2::SDL2_mixer)

# we want to copy the assets directory to the same directory the executable is in post build
add_custom_command(TARGET chip8 POST_BUILD
  COMMAND "${CMAKE_COMMAND}" -E copy_directory
    "${CMAKE_SOURCE_DIR}/assets" "$<TARGET_FILE_DIR:chip8>/assets"
  VERBATIM
)
