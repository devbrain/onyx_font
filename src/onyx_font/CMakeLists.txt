# =============================================================================
# onyx_font Library
# =============================================================================

include(GenerateExportHeader)
include(FetchContent)

# =============================================================================
# Dependencies
# =============================================================================

# mz-explode - Windows executable format library (only needed for FON loader)
if(NEUTRINO_ONYX_FONT_LOADER_FON)
    include(${NEUTRINO_CMAKE_DIR}/deps/mz-explode.cmake)
    neutrino_fetch_mzexplode()
endif()

# failsafe - Error handling library
include(${NEUTRINO_CMAKE_DIR}/deps/failsafe.cmake)
neutrino_fetch_failsafe()

# DataScript - Binary format parser generator
include(${NEUTRINO_CMAKE_DIR}/deps/datascript.cmake)
neutrino_fetch_datascript()

# euler - Line rasterization library (for vector fonts)
include(${NEUTRINO_CMAKE_DIR}/deps/euler.cmake)
neutrino_fetch_euler()

# FreeType - High-quality font rendering with proper hinting and styling
# (only needed for TTF / OTF / TTC support and the freetype-based converter)
if(NEUTRINO_ONYX_FONT_LOADER_TTF AND NOT TARGET freetype)
    include(FetchContent)

    # Disable all optional dependencies for minimal portable build
    set(FT_DISABLE_ZLIB ON CACHE BOOL "" FORCE)
    set(FT_DISABLE_BZIP2 ON CACHE BOOL "" FORCE)
    set(FT_DISABLE_PNG ON CACHE BOOL "" FORCE)
    set(FT_DISABLE_HARFBUZZ ON CACHE BOOL "" FORCE)
    set(FT_DISABLE_BROTLI ON CACHE BOOL "" FORCE)

    FetchContent_Declare(freetype
        GIT_REPOSITORY https://github.com/freetype/freetype.git
        GIT_TAG VER-2-13-3
        GIT_SHALLOW TRUE
        PATCH_COMMAND ${CMAKE_COMMAND} -P ${NEUTRINO_CMAKE_DIR}/patches/patch_cmake_minimum.cmake
    )
    FetchContent_MakeAvailable(freetype)
endif()


# =============================================================================
# DataScript Code Generation
# =============================================================================

datascript_generate(
    TARGET onyx_font_parsers
    SCHEMAS
        ${CMAKE_CURRENT_SOURCE_DIR}/formats/bgi.ds
        ${CMAKE_CURRENT_SOURCE_DIR}/formats/gem.ds
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

    font_factory.cc
    ${PROJECT_SOURCE_DIR}/include/onyx_font/font_factory.hh

    bios_font_8x8.cc
    bios_font_8x14.cc
    bios_font_8x16.cc
    ${PROJECT_SOURCE_DIR}/include/onyx_font/bios_font.hh

    font_registry.cc
    ${PROJECT_SOURCE_DIR}/include/onyx_font/font_registry.hh
    ${PROJECT_SOURCE_DIR}/include/onyx_font/decoder.hh

    loader/loaders.hh

    loader/bgi_fon.cc
    loader/gem_fon.cc

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
)

# FON (Windows .FON / .FNT, OS/2 LX font resources) loader — pulls libexe.
# Compile def is PUBLIC because loaders.hh (gated by ONYX_FONT_HAS_LOADER_FON)
# is included by the library's internal .cc files only, but consumers may
# want to #if on the same macro to know whether the loader code is present.
if(NEUTRINO_ONYX_FONT_LOADER_FON)
    target_sources(onyx_font PRIVATE
        loader/win_fon.cc
        loader/win_vector_fon.cc
    )
    target_compile_definitions(onyx_font PUBLIC ONYX_FONT_HAS_LOADER_FON=1)
endif()

# TTF / OTF / TTC loader and FreeType-backed converter — pulls FreeType.
# Compile def is PUBLIC because font_source.hh's `from_ttf` / `from_ttf_bytes`
# declarations are gated by ONYX_FONT_HAS_LOADER_TTF; consumers including
# the header must see the same macro to compile against those overloads.
if(NEUTRINO_ONYX_FONT_LOADER_TTF)
    target_sources(onyx_font PRIVATE
        ttf_font.cc
        ${PROJECT_SOURCE_DIR}/include/onyx_font/ttf_font.hh

        utils/freetype_font.cc
        ${PROJECT_SOURCE_DIR}/include/onyx_font/utils/freetype_font.hh

        font_converter.cc
        ${PROJECT_SOURCE_DIR}/include/onyx_font/font_converter.hh
    )
    target_compile_definitions(onyx_font PUBLIC ONYX_FONT_HAS_LOADER_TTF=1)
endif()

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
        euler::euler
    PRIVATE
        failsafe
        onyx_font_parsers
)

if(NEUTRINO_ONYX_FONT_LOADER_FON)
    target_link_libraries(onyx_font PRIVATE neutrino::libexe)
endif()

if(NEUTRINO_ONYX_FONT_LOADER_TTF)
    target_link_libraries(onyx_font PRIVATE freetype)
endif()

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
