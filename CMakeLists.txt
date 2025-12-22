cmake_minimum_required(VERSION 3.20)

# =============================================================================
# Project Definition
# =============================================================================
project(onyx_font
        VERSION 1.0.0
        DESCRIPTION "Font loading and text rendering library for bitmap, vector, and TrueType fonts"
        LANGUAGES CXX
)

# =============================================================================
# Build Options
# =============================================================================

# Determine if this is a top-level project or included via add_subdirectory/FetchContent
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(ONYX_FONT_MAIN_PROJECT ON)
else()
    set(ONYX_FONT_MAIN_PROJECT OFF)
endif()

# Build configuration options
option(ONYX_FONT_BUILD_TESTS "Build unit tests" ${ONYX_FONT_MAIN_PROJECT})
option(ONYX_FONT_BUILD_EXAMPLES "Build example applications" ${ONYX_FONT_MAIN_PROJECT})
option(ONYX_FONT_BUILD_DEMOS "Build SDL demos (ImGui demo, text scroller)" ${ONYX_FONT_MAIN_PROJECT})
option(ONYX_FONT_BUILD_DOCS "Build API documentation with Doxygen" OFF)
option(ONYX_FONT_BUILD_SHARED "Build shared library (ON) or static library (OFF)" ON)

# =============================================================================
# Configuration Summary
# =============================================================================
message(STATUS "")
message(STATUS "onyx_font configuration:")
message(STATUS "  Version:          ${PROJECT_VERSION}")
message(STATUS "  Build type:       ${CMAKE_BUILD_TYPE}")
message(STATUS "  Main project:     ${ONYX_FONT_MAIN_PROJECT}")
message(STATUS "  Build tests:      ${ONYX_FONT_BUILD_TESTS}")
message(STATUS "  Build examples:   ${ONYX_FONT_BUILD_EXAMPLES}")
message(STATUS "  Build demos:      ${ONYX_FONT_BUILD_DEMOS}")
message(STATUS "  Build docs:       ${ONYX_FONT_BUILD_DOCS}")
message(STATUS "  Shared library:   ${ONYX_FONT_BUILD_SHARED}")
message(STATUS "")

# =============================================================================
# C++ Standard
# =============================================================================
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# =============================================================================
# Library Type Configuration
# =============================================================================
if(ONYX_FONT_BUILD_SHARED)
    set(BUILD_SHARED_LIBS ON)
else()
    set(BUILD_SHARED_LIBS OFF)
endif()

# =============================================================================
# Subdirectories
# =============================================================================

# Core library (always built)
add_subdirectory(src/onyx_font)

# Unit tests (optional)
if(ONYX_FONT_BUILD_TESTS)
    add_subdirectory(unittest)
endif()

# Examples (optional)
if(ONYX_FONT_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# =============================================================================
# Documentation (Doxygen)
# =============================================================================
if(ONYX_FONT_BUILD_DOCS)
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
        message(WARNING "ONYX_FONT_BUILD_DOCS is ON but Doxygen was not found")
    endif()
endif()
