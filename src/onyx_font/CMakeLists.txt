# =============================================================================
# onyx_font Library
# =============================================================================

include(GenerateExportHeader)
include(FetchContent)

# =============================================================================
# Dependencies
# =============================================================================

set(FETCHCONTENT_QUIET OFF)

# mz-explode - Windows executable format library
if(NOT TARGET neutrino::libexe)
    message(STATUS "libexe not found, fetching from GitHub...")

    set(NEUTRINO_MZEXPLODE_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(NEUTRINO_MZEXPLODE_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
    set(NEUTRINO_MZEXPLODE_BUILD_DOCS OFF CACHE BOOL "" FORCE)

    FetchContent_Declare(
        mzexplode
        GIT_REPOSITORY https://github.com/devbrain/mz-explode.git
        GIT_TAG master
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
    )

    FetchContent_MakeAvailable(mzexplode)
else()
    message(STATUS "Using existing libexe (provided by parent project)")
endif()

# failsafe - Error handling library
if(NOT TARGET failsafe AND NOT TARGET thirdparty::failsafe)
    find_package(failsafe QUIET)

    if(NOT failsafe_FOUND)
        message(STATUS "failsafe not found - fetching from GitHub...")

        set(FAILSAFE_BUILD_TESTS OFF CACHE INTERNAL "")
        set(FAILSAFE_BUILD_EXAMPLES OFF CACHE INTERNAL "")

        FetchContent_Declare(
            FAILSAFE
            GIT_REPOSITORY "https://github.com/devbrain/failsafe.git"
            GIT_TAG master
            GIT_PROGRESS TRUE
            UPDATE_COMMAND ""
            OVERRIDE_FIND_PACKAGE
        )
        FetchContent_MakeAvailable(FAILSAFE)
    endif()
endif()

# DataScript - Binary format parser generator
if(NOT TARGET ds)
    message(STATUS "DataScript not found, fetching from GitHub...")

    set(NEUTRINO_DATASCRIPT_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(NEUTRINO_DATASCRIPT_BUILD_HOST_TOOLS ON CACHE BOOL "" FORCE)

    FetchContent_Declare(
        datascript
        GIT_REPOSITORY https://github.com/devbrain/datascript.git
        GIT_TAG master
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
    )

    FetchContent_MakeAvailable(datascript)
else()
    message(STATUS "Using existing DataScript (provided by parent project)")
endif()

# euler - Line rasterization library (for vector fonts)
if(NOT TARGET euler::euler)
    find_package(euler QUIET)

    if(NOT euler_FOUND)
        message(STATUS "euler not found, fetching from GitHub...")

        set(NEUTRINO_EULER_BUILD_TESTS OFF CACHE BOOL "" FORCE)
        set(NEUTRINO_EULER_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
        set(NEUTRINO_EULER_BUILD_BENCHMARKS OFF CACHE BOOL "" FORCE)

        FetchContent_Declare(
            euler
            GIT_REPOSITORY https://github.com/devbrain/euler.git
            GIT_TAG master
            GIT_SHALLOW TRUE
            GIT_PROGRESS TRUE
        )

        FetchContent_MakeAvailable(euler)
    endif()
else()
    message(STATUS "Using existing euler (provided by parent project)")
endif()

# stb - Single-file public domain libraries (stb_truetype for TTF parsing)
if(NOT TARGET stb::stb)
    message(STATUS "stb not found, fetching from GitHub...")

    FetchContent_Declare(
        stb
        GIT_REPOSITORY https://github.com/nothings/stb.git
        GIT_TAG master
        GIT_SHALLOW TRUE
    )

    FetchContent_MakeAvailable(stb)

    # Create an interface library for stb headers
    add_library(stb INTERFACE)
    add_library(stb::stb ALIAS stb)
    target_include_directories(stb INTERFACE ${stb_SOURCE_DIR})
else()
    message(STATUS "Using existing stb (provided by parent project)")
endif()

# =============================================================================
# DataScript Code Generation
# =============================================================================

datascript_generate(
    TARGET onyx_font_parsers
    SCHEMAS
        ${CMAKE_CURRENT_SOURCE_DIR}/formats/bgi.ds
    OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated
    IMPORT_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
    INCLUDE_DIRS ${CMAKE_CURRENT_BINARY_DIR}
    PRESERVE_PACKAGE_DIRS ON
)

# =============================================================================
# Library Target
# =============================================================================

if(NEUTRINO_ONYX_FONT_BUILD_SHARED)
    add_library(onyx_font SHARED)
else()
    add_library(onyx_font STATIC)
endif()

# Create neutrino:: alias
add_library(neutrino::onyx_font ALIAS onyx_font)
# Legacy alias for compatibility
add_library(onyx::font ALIAS onyx_font)

target_sources(onyx_font PRIVATE
    bitmap_font.cc
    ${PROJECT_SOURCE_DIR}/include/onyx_font/bitmap_font.hh

    vector_font.cc
    ${PROJECT_SOURCE_DIR}/include/onyx_font/vector_font.hh

    ttf_font.cc
    ${PROJECT_SOURCE_DIR}/include/onyx_font/ttf_font.hh

    utils/stb_truetype_font.cc
    ${PROJECT_SOURCE_DIR}/include/onyx_font/utils/stb_truetype_font.hh

    font_factory.cc
    ${PROJECT_SOURCE_DIR}/include/onyx_font/font_factory.hh

    loader/win_fon.cc
    loader/loaders.hh

    loader/win_vector_fon.cc
    loader/bgi_fon.cc

    utils/bitmap_glyphs_storage.cc
    ${PROJECT_SOURCE_DIR}/include/onyx_font/utils/bitmap_glyphs_storage.hh

    # Text rendering module
    text/utf8.cc
    ${PROJECT_SOURCE_DIR}/include/onyx_font/text/utf8.hh
    ${PROJECT_SOURCE_DIR}/include/onyx_font/text/types.hh
    ${PROJECT_SOURCE_DIR}/include/onyx_font/text/raster_target.hh

    text/font_source.cc
    ${PROJECT_SOURCE_DIR}/include/onyx_font/text/font_source.hh

    text/text_rasterizer.cc
    ${PROJECT_SOURCE_DIR}/include/onyx_font/text/text_rasterizer.hh

    ${PROJECT_SOURCE_DIR}/include/onyx_font/text/atlas_surface.hh
    ${PROJECT_SOURCE_DIR}/include/onyx_font/text/glyph_cache.hh
    ${PROJECT_SOURCE_DIR}/include/onyx_font/text/glyph_rasterizer.hh
    ${PROJECT_SOURCE_DIR}/include/onyx_font/text/text_renderer.hh

    # Font conversion utilities
    font_converter.cc
    ${PROJECT_SOURCE_DIR}/include/onyx_font/font_converter.hh
)

# =============================================================================
# Target Configuration
# =============================================================================

target_compile_features(onyx_font PUBLIC cxx_std_20)

target_include_directories(onyx_font
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(onyx_font
    PUBLIC
        failsafe
        euler::euler
    PRIVATE
        neutrino::libexe
        onyx_font_parsers
        stb::stb
)

# Generate portable export header
generate_export_header(onyx_font
    BASE_NAME onyx_font
    EXPORT_FILE_NAME ${CMAKE_CURRENT_BINARY_DIR}/include/onyx_font/export.h
)

# =============================================================================
# Library Properties
# =============================================================================

set_target_properties(onyx_font PROPERTIES
    OUTPUT_NAME onyx_font
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    POSITION_INDEPENDENT_CODE ON
    FOLDER "Libraries"
)

# =============================================================================
# Compiler Warnings
# =============================================================================

if(MSVC)
    target_compile_options(onyx_font PRIVATE
        /W4
        /permissive-
        /wd4251       # Disable warning about DLL interface for STL types
    )
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(onyx_font PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Wshadow
        -Wconversion
        -Wsign-conversion
    )

    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_compile_options(onyx_font PRIVATE
            -Wduplicated-cond
            -Wlogical-op
            -Wnull-dereference
        )
    endif()

    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        target_compile_options(onyx_font PRIVATE
            -Wno-unknown-warning-option
        )
    endif()
endif()
