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
