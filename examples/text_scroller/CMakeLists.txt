# SDL Text Scroller Demo for onyx_font library
# Classic demoscene-style horizontal text scroller

cmake_minimum_required(VERSION 3.20)

# ============================================================================
# SDL3 Dependency (using neutrino-cmake)
# ============================================================================

include(${NEUTRINO_CMAKE_DIR}/deps/SDL3.cmake)
neutrino_fetch_sdl3()

message(STATUS "Text Scroller: Using SDL3")

# ============================================================================
# Text Scroller executable
# ============================================================================

add_executable(text_scroller
    main.cc
)

target_link_libraries(text_scroller PRIVATE
    onyx_font
    SDL3::SDL3
)

target_compile_definitions(text_scroller PRIVATE USE_SDL3=1)

target_compile_features(text_scroller PRIVATE cxx_std_20)

# Pass test data directory for fonts
target_compile_definitions(text_scroller PRIVATE
    TESTDATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../../test/testdata"
)

# Copy fonts to build directory
add_custom_command(TARGET text_scroller POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/../../test/testdata"
        "$<TARGET_FILE_DIR:text_scroller>/fonts"
    COMMENT "Copying test fonts to build directory"
)
