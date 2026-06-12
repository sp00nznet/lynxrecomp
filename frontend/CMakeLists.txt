# lynxrecomp frontend - SDL2 + Dear ImGui interactive shell.
#
# OPT-IN: enabled with -DLYNXRECOMP_FRONTEND=ON. The core `lynxrecomp` runtime
# library stays dependency-free; everything here pulls in SDL2 (and, with the
# menu, Dear ImGui). A game links `lynxrecomp_frontend` to get a window, audio,
# input, save/load and the menu overlay.
#
# Configure with the vcpkg toolchain so find_package(SDL2) resolves, e.g.:
#   cmake -B build -DLYNXRECOMP_FRONTEND=ON \
#         -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake

option(LYNXRECOMP_MENU "Build the Dear ImGui menu overlay" ON)

find_package(SDL2 REQUIRED)

# --- Dear ImGui (vendored static lib), only when the menu is enabled ---
if(LYNXRECOMP_MENU)
    set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui)
    add_library(lynx_imgui STATIC
        ${IMGUI_DIR}/imgui.cpp
        ${IMGUI_DIR}/imgui_draw.cpp
        ${IMGUI_DIR}/imgui_tables.cpp
        ${IMGUI_DIR}/imgui_widgets.cpp
        ${IMGUI_DIR}/backends/imgui_impl_sdl2.cpp
        ${IMGUI_DIR}/backends/imgui_impl_sdlrenderer2.cpp
    )
    target_include_directories(lynx_imgui PUBLIC ${IMGUI_DIR})
    target_link_libraries(lynx_imgui PUBLIC SDL2::SDL2)
    if(MSVC)
        target_compile_options(lynx_imgui PRIVATE /W0)
    else()
        target_compile_options(lynx_imgui PRIVATE -w)
    endif()
    set(MENU_SRC src/menu_overlay.cpp)
else()
    set(MENU_SRC src/menu_overlay_stub.c)
endif()

add_library(lynxrecomp_frontend STATIC
    src/platform_sdl.c
    src/mp_session.c
    ${MENU_SRC}
)
target_include_directories(lynxrecomp_frontend PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(lynxrecomp_frontend
    PUBLIC lynxrecomp
    PUBLIC SDL2::SDL2
)
if(WIN32)
    target_link_libraries(lynxrecomp_frontend PUBLIC ws2_32)   # netplay sockets
endif()
if(LYNXRECOMP_MENU)
    target_include_directories(lynxrecomp_frontend PRIVATE ${IMGUI_DIR})
    target_link_libraries(lynxrecomp_frontend PRIVATE lynx_imgui)
    target_compile_definitions(lynxrecomp_frontend PRIVATE LYNXRECOMP_MENU=1)
endif()

if(MSVC)
    set_source_files_properties(src/platform_sdl.c src/mp_session.c
        PROPERTIES COMPILE_OPTIONS "/W3")
endif()
