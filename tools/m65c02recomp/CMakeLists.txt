# m65c02recomp - the static recompiler CLI (decode/analyze/emit).
add_executable(m65c02recomp
    main.c
    decode.c
    lnx.c
    analyze.c
    emit.c
    lynxdec.c
)

# lynxexec - boots a cart (real boot ROM + modeled cart hardware) far enough to
# snapshot the post-boot RAM image + game entry for the recompiler.
add_executable(lynxexec
    lynxexec.c
    decode.c
    lnx.c
)

foreach(t m65c02recomp lynxexec)
    set_target_properties(${t} PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON)
    if(MSVC)
        target_compile_options(${t} PRIVATE /W3)
    else()
        target_compile_options(${t} PRIVATE -Wall -Wextra)
    endif()
endforeach()
