# lynxrecomp - static-recompilation toolkit for Atari Lynx games.
#
# Builds two things:
#   * lynxrecomp        - the runtime library recompiled game code links against
#                         (CPU state, memory dispatch, Suzy/Mikey peripherals,
#                          timers, input). No SDL dependency at this layer.
#   * m65c02recomp      - the recompiler CLI (decode/analyze/emit).
#
# A game repo consumes this via `add_subdirectory(lynxrecomp)` (git submodule)
# and links `lynxrecomp`. See README.md.
cmake_minimum_required(VERSION 3.16)
project(lynxrecomp C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# --- runtime library -------------------------------------------------------
add_library(lynxrecomp STATIC
    src/cpu.c
    src/mem.c
    src/suzy.c
    src/mikey.c
    src/timer.c
    src/input.c
    src/recomp_rt.c
    src/recomp_exec.c
    src/video.c
)
target_include_directories(lynxrecomp PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# --- recompiler CLI --------------------------------------------------------
add_subdirectory(tools/m65c02recomp)

# --- tests (only when lynxrecomp is the top-level project, not a submodule) --
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    enable_testing()
    add_subdirectory(tests)
endif()
