# =============================================================================
# onyx_font Tests
# =============================================================================

# =============================================================================
# Fetch doctest via neutrino-cmake
# =============================================================================

include(${NEUTRINO_CMAKE_DIR}/deps/doctest.cmake)
neutrino_fetch_doctest()

# =============================================================================
# Test Executable
# =============================================================================

add_executable(onyxfont_unittest
    main.cc
    test_data.hh

    test_glyphs_bitmap_storage.cc
    test_font_factory.cc
    test_bitmap_font.cc
    test_vector_font.cc
    test_ttf_font.cc

    # Text rendering module tests
    test_utf8.cc
    test_raster_target.cc
    test_font_source.cc
    test_text_rasterizer.cc
    test_glyph_cache.cc
    test_text_renderer.cc
    test_glyph_rasterizer.cc
    test_text_rendering.cc
    test_font_converter.cc
)

target_link_libraries(onyxfont_unittest
    PRIVATE
        onyx_font
        doctest::doctest
)

target_include_directories(onyxfont_unittest
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# Define path to test data directory
target_compile_definitions(onyxfont_unittest PRIVATE
    TESTDATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/testdata"
)

# Set target properties
set_target_properties(onyxfont_unittest PROPERTIES
    FOLDER "Tests"
)

# Register tests with CTest
include(CTest)
add_test(NAME onyxfont_unittest COMMAND onyxfont_unittest)

# =============================================================================
# Diagnostic tool to dump font metadata (for ground truth)
# =============================================================================

add_executable(dump_font_metadata
    dump_font_metadata.cc
    test_data.hh
)

target_link_libraries(dump_font_metadata
    PRIVATE
        onyx_font
)

target_include_directories(dump_font_metadata
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_definitions(dump_font_metadata PRIVATE
    TESTDATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/testdata"
)
