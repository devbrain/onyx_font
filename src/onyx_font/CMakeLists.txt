# =============================================================================
# onyx_font Library
# =============================================================================

include(GenerateExportHeader)
include(FetchContent)

# =============================================================================
# Dependencies
# =============================================================================

# mz-explode - Windows executable format library
include(${NEUTRINO_CMAKE_DIR}/deps/mz-explode.cmake)
neutrino_fetch_mzexplode()

# failsafe - Error handling library
include(${NEUTRINO_CMAKE_DIR}/deps/failsafe.cmake)
neutrino_fetch_failsafe()

# DataScript - Binary format parser generator
include(${NEUTRINO_CMAKE_DIR}/deps/datascript.cmake)
neutrino_fetch_datascript()

# euler - Line rasterization library (for vector fonts)
include(${NEUTRINO_CMAKE_DIR}/deps/euler.cmake)
neutrino_fetch_euler()

# stb - Single-file public domain libraries (stb_truetype for TTF parsing)
if(NOT TARGET stb::stb)
    include(FetchContent)
    FetchContent_Declare(stb
        GIT_REPOSITORY https://github.com/nothings/stb.git
        GIT_TAG master
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(stb)

    # Create an interface library for stb headers
    add_library(stb INTERFACE)
    add_library(stb::stb ALIAS stb)
    target_include_directories(stb INTERFACE ${stb_SOURCE_DIR})
    neutrino_suppress_warnings(stb)
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

neutrino_target_warnings(onyx_font)

# =============================================================================
# Installation
# =============================================================================

if(NEUTRINO_ONYX_FONT_INSTALL)
    include(NeutrinoInstall)

    # Install generated export header
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/include/onyx_font/export.h
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/onyx_font
    )

    neutrino_install_library(onyx_font
        NAMESPACE neutrino::
        COMPATIBILITY SameMajorVersion
    )
endif()
