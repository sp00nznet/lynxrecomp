# Tests for lynxrecomp. Self-contained — no game data (fixture.bin is authored
# public-domain 65SC02). Run with `ctest` after building.

# --- unit tests: decoder + runtime helpers ---
add_executable(test_units
    test_units.c
    ${PROJECT_SOURCE_DIR}/tools/m65c02recomp/decode.c
)
target_include_directories(test_units PRIVATE
    ${PROJECT_SOURCE_DIR}/tools/m65c02recomp
    ${PROJECT_SOURCE_DIR}/include
)
target_link_libraries(test_units PRIVATE lynxrecomp)
add_test(NAME units COMMAND test_units)

# --- hardware: timers/IRQ + video readout ---
add_executable(test_hw test_hw.c)
target_include_directories(test_hw PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(test_hw PRIVATE lynxrecomp)
add_test(NAME hw COMMAND test_hw)

# --- Suzy sprite blitter ---
add_executable(test_blit test_blit.c)
target_include_directories(test_blit PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(test_blit PRIVATE lynxrecomp)
add_test(NAME blit COMMAND test_blit)

# --- Suzy math unit ---
add_executable(test_math test_math.c)
target_include_directories(test_math PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(test_math PRIVATE lynxrecomp)
add_test(NAME math COMMAND test_math)

# --- Mikey audio ---
add_executable(test_audio test_audio.c)
target_include_directories(test_audio PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(test_audio PRIVATE lynxrecomp)
add_test(NAME audio COMMAND test_audio)

# --- Mikey UART / ComLynx serial ---
add_executable(test_serial test_serial.c)
target_include_directories(test_serial PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(test_serial PRIVATE lynxrecomp)
add_test(NAME serial COMMAND test_serial)

# --- save states ---
add_executable(test_state test_state.c)
target_include_directories(test_state PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(test_state PRIVATE lynxrecomp)
add_test(NAME state COMMAND test_state)

# --- end-to-end pipeline: recompile fixture.bin, build the C, run it ---
set(GEN ${CMAKE_CURRENT_BINARY_DIR}/gen)
file(MAKE_DIRECTORY ${GEN})

add_custom_command(
    OUTPUT ${GEN}/recomp_funcs.c ${GEN}/recomp_funcs.h
    COMMAND m65c02recomp recompbin ${CMAKE_CURRENT_SOURCE_DIR}/fixture.bin 0x0200 ${GEN}
    DEPENDS m65c02recomp ${CMAKE_CURRENT_SOURCE_DIR}/fixture.bin
    COMMENT "Recompiling tests/fixture.bin -> C"
    VERBATIM
)

add_executable(test_pipeline
    test_pipeline.c
    ${GEN}/recomp_funcs.c
)
target_include_directories(test_pipeline PRIVATE
    ${GEN}
    ${PROJECT_SOURCE_DIR}/include
)
target_link_libraries(test_pipeline PRIVATE lynxrecomp)
if(MSVC)
    target_compile_options(test_pipeline PRIVATE /wd4244)
endif()
add_test(NAME pipeline COMMAND test_pipeline)

# --- dispatch: recompiled C runs through a computed jump ---
set(GEN2 ${CMAKE_CURRENT_BINARY_DIR}/gen_jmp)
file(MAKE_DIRECTORY ${GEN2})
add_custom_command(
    OUTPUT ${GEN2}/recomp_funcs.c ${GEN2}/recomp_funcs.h
    COMMAND m65c02recomp recompbin ${CMAKE_CURRENT_SOURCE_DIR}/fixture_jmp.bin 0x0200 ${GEN2}
            0x0200 0x0220 0x0228 0x0230
    DEPENDS m65c02recomp ${CMAKE_CURRENT_SOURCE_DIR}/fixture_jmp.bin
    COMMENT "Recompiling tests/fixture_jmp.bin -> C"
    VERBATIM
)
add_executable(test_dispatch test_dispatch.c ${GEN2}/recomp_funcs.c)
target_include_directories(test_dispatch PRIVATE ${GEN2} ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(test_dispatch PRIVATE lynxrecomp)
if(MSVC)
    target_compile_options(test_dispatch PRIVATE /wd4244)
endif()
add_test(NAME dispatch COMMAND test_dispatch)
