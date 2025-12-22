# SDL Text Scroller Demo for onyx_font library
# Classic demoscene-style horizontal text scroller
# Works with SDL2 or SDL3

cmake_minimum_required(VERSION 3.20)

# ============================================================================
# SDL Detection - Try SDL3 first, fall back to SDL2
# ============================================================================

set(SDL_FOUND FALSE)
set(SDL_VERSION "")

# Try SDL3 first
find_package(SDL3 QUIET)
if(SDL3_FOUND)
    set(SDL_FOUND TRUE)
    set(SDL_VERSION "3")
    set(SDL_LIBRARIES SDL3::SDL3)
    message(STATUS "Text Scroller: Found SDL3")
else()
    # Fall back to SDL2
    find_package(SDL2 QUIET)
    if(SDL2_FOUND)
        set(SDL_FOUND TRUE)
        set(SDL_VERSION "2")
        set(SDL_LIBRARIES SDL2::SDL2)
        message(STATUS "Text Scroller: Found SDL2")
    endif()
endif()

if(NOT SDL_FOUND)
    message(WARNING
        "Text Scroller: Neither SDL2 nor SDL3 found, skipping build.\n"
        "Please install SDL2 or SDL3 development packages:\n"
        "  Ubuntu/Debian: sudo apt install libsdl2-dev\n"
        "  Fedora: sudo dnf install SDL2-devel\n"
        "  macOS: brew install sdl2"
    )
    return()
endif()

# ============================================================================
# Text Scroller executable
# ============================================================================

add_executable(text_scroller
    main.cc
)

target_link_libraries(text_scroller PRIVATE
    onyx_font
    ${SDL_LIBRARIES}
)

# Set SDL version define
if(SDL_VERSION STREQUAL "3")
    target_compile_definitions(text_scroller PRIVATE USE_SDL3=1)
else()
    target_compile_definitions(text_scroller PRIVATE USE_SDL2=1)
endif()

target_compile_features(text_scroller PRIVATE cxx_std_20)

# Pass test data directory for fonts
target_compile_definitions(text_scroller PRIVATE
    TESTDATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../../unittest/testdata"
)

# Copy fonts to build directory
add_custom_command(TARGET text_scroller POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/../../unittest/testdata"
        "$<TARGET_FILE_DIR:text_scroller>/fonts"
    COMMENT "Copying test fonts to build directory"
)
