# m65c02recomp - the static recompiler CLI (decode/analyze/emit).
add_executable(m65c02recomp
    main.c
    decode.c
    lnx.c
    analyze.c
    emit.c
)

set_target_properties(m65c02recomp PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
)

if(MSVC)
    target_compile_options(m65c02recomp PRIVATE /W3)
else()
    target_compile_options(m65c02recomp PRIVATE -Wall -Wextra)
endif()
