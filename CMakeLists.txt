# =============================================================================
# onyx_font - Font loading and text rendering library
# =============================================================================

cmake_minimum_required(VERSION 3.20)

project(onyx_font
    VERSION 1.0.0
    DESCRIPTION "Font loading and text rendering library for bitmap, vector, and TrueType fonts"
    LANGUAGES CXX
)

# =============================================================================
# Neutrino CMake Integration
# =============================================================================

include(FetchContent)

if(NOT DEFINED NEUTRINO_CMAKE_DIR)
    FetchContent_Declare(
        neutrino_cmake
        GIT_REPOSITORY https://github.com/devbrain/neutrino-cmake.git
        GIT_TAG master
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(neutrino_cmake)
    set(NEUTRINO_CMAKE_DIR "${neutrino_cmake_SOURCE_DIR}/cmake")
    list(APPEND CMAKE_MODULE_PATH "${NEUTRINO_CMAKE_DIR}")
endif()

include(NeutrinoInit)

# =============================================================================
# Options
# =============================================================================

neutrino_define_library_options(onyx_font)

# Additional options
neutrino_is_top_level(_onyx_font_is_top_level)
if(_onyx_font_is_top_level)
    option(NEUTRINO_ONYX_FONT_BUILD_EXAMPLES "Build example applications" ON)
    option(NEUTRINO_ONYX_FONT_BUILD_DEMOS "Build SDL demos (ImGui demo, text scroller)" ON)
else()
    option(NEUTRINO_ONYX_FONT_BUILD_EXAMPLES "Build example applications" OFF)
    option(NEUTRINO_ONYX_FONT_BUILD_DEMOS "Build SDL demos (ImGui demo, text scroller)" OFF)
endif()

# =============================================================================
# C++ Standard
# =============================================================================

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Symbol visibility (hide all by default, like Windows)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN YES)

# =============================================================================
# Output directories
# =============================================================================

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# =============================================================================
# Main Library
# =============================================================================

add_subdirectory(src/onyx_font)

# =============================================================================
# Unit Tests
# =============================================================================

if(NEUTRINO_ONYX_FONT_BUILD_TESTS)
    enable_testing()
    add_subdirectory(test)
endif()

# =============================================================================
# Examples
# =============================================================================

if(NEUTRINO_ONYX_FONT_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# =============================================================================
# Documentation (Doxygen)
# =============================================================================

if(NEUTRINO_ONYX_FONT_BUILD_DOCS)
    find_package(Doxygen)

    if(DOXYGEN_FOUND)
        set(DOXYGEN_PROJECT_NAME "${PROJECT_NAME}")
        set(DOXYGEN_PROJECT_NUMBER "${PROJECT_VERSION}")
        set(DOXYGEN_PROJECT_BRIEF "${PROJECT_DESCRIPTION}")
        set(DOXYGEN_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/docs")
        set(DOXYGEN_GENERATE_HTML YES)
        set(DOXYGEN_GENERATE_LATEX NO)
        set(DOXYGEN_EXTRACT_ALL YES)
        set(DOXYGEN_EXTRACT_PRIVATE NO)
        set(DOXYGEN_EXTRACT_STATIC YES)
        set(DOXYGEN_RECURSIVE YES)
        set(DOXYGEN_USE_MDFILE_AS_MAINPAGE "${CMAKE_SOURCE_DIR}/README.md")

        doxygen_add_docs(docs
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/README.md
            COMMENT "Generating API documentation with Doxygen"
        )

        message(STATUS "Doxygen found: documentation target 'docs' available")
    else()
        message(WARNING "NEUTRINO_ONYX_FONT_BUILD_DOCS is ON but Doxygen was not found")
    endif()
endif()

# =============================================================================
# Summary
# =============================================================================

neutrino_is_top_level(_onyx_font_is_top_level)
if(_onyx_font_is_top_level)
    neutrino_print_options(onyx_font)
    message(STATUS "")
    message(STATUS "onyx_font ${PROJECT_VERSION} Configuration:")
    message(STATUS "  C++ Standard:    20")
    message(STATUS "  Library type:    ${NEUTRINO_ONYX_FONT_BUILD_SHARED}")
    message(STATUS "  Build tests:     ${NEUTRINO_ONYX_FONT_BUILD_TESTS}")
    message(STATUS "  Build examples:  ${NEUTRINO_ONYX_FONT_BUILD_EXAMPLES}")
    message(STATUS "  Build demos:     ${NEUTRINO_ONYX_FONT_BUILD_DEMOS}")
    message(STATUS "  Build docs:      ${NEUTRINO_ONYX_FONT_BUILD_DOCS}")
    message(STATUS "")
endif()
