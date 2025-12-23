# =============================================================================
# Fetch doctest if not already available
# =============================================================================

# Guard: check if doctest target already exists (provided by parent project)
if (NOT TARGET doctest::doctest AND NOT TARGET doctest)
    find_package(doctest QUIET)
endif ()

if (NOT doctest_FOUND AND NOT TARGET doctest::doctest AND NOT TARGET doctest)
    message(STATUS "doctest not found, fetching from GitHub...")

    include(FetchContent)
    set(FETCHCONTENT_QUIET OFF)

    # Allow FetchContent_Populate to be used (suppress CMP0169 warning)
    if (POLICY CMP0169)
        cmake_policy(SET CMP0169 OLD)
    endif ()

    # Configure doctest options
    set(DOCTEST_WITH_TESTS OFF CACHE INTERNAL "")
    set(DOCTEST_WITH_MAIN_IN_STATIC_LIB OFF CACHE INTERNAL "")
    set(DOCTEST_NO_INSTALL ON CACHE INTERNAL "")

    FetchContent_Declare(
            doctest
            GIT_REPOSITORY https://github.com/doctest/doctest.git
            GIT_TAG v2.4.12
            GIT_PROGRESS TRUE
    )

    # Fetch the content first
    FetchContent_GetProperties(doctest)
    if (NOT doctest_POPULATED)
        FetchContent_Populate(doctest)

        # Patch the CMakeLists.txt to fix deprecation warning
        file(READ ${doctest_SOURCE_DIR}/CMakeLists.txt DOCTEST_CMAKE_CONTENT)
        # Replace any version less than 3.10 with 3.10
        string(REGEX REPLACE "cmake_minimum_required\\(VERSION [0-9]+\\.[0-9]+\\)"
                "cmake_minimum_required(VERSION 3.10)"
                DOCTEST_CMAKE_CONTENT "${DOCTEST_CMAKE_CONTENT}")
        file(WRITE ${doctest_SOURCE_DIR}/CMakeLists.txt "${DOCTEST_CMAKE_CONTENT}")

        # Now add the subdirectory
        add_subdirectory(${doctest_SOURCE_DIR} ${doctest_BINARY_DIR} EXCLUDE_FROM_ALL)
    endif ()

    # Verify doctest::doctest target was created
    if (TARGET doctest::doctest)
        message(STATUS "doctest::doctest target created successfully")
    elseif (TARGET doctest)
        # Create alias if only 'doctest' target exists
        add_library(doctest::doctest ALIAS doctest)
        message(STATUS "Created doctest::doctest alias")
    else ()
        message(FATAL_ERROR "doctest failed to create expected targets")
    endif ()
else ()
    # doctest already available (from system or parent project)
    if (TARGET doctest::doctest)
        message(STATUS "Using existing doctest::doctest target")
    elseif (TARGET doctest)
        message(STATUS "Using existing doctest target, creating alias")
        add_library(doctest::doctest ALIAS doctest)
    else ()
        message(STATUS "Using system-provided doctest")
    endif ()
endif ()

# Unit test executable
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

target_include_directories(onyxfont_unittest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# Define path to test data directory
target_compile_definitions(onyxfont_unittest PRIVATE
        TESTDATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/testdata"
)

# Register tests with CTest
include(CTest)
add_test(NAME onyxfont_unittest COMMAND onyxfont_unittest)

# Diagnostic tool to dump font metadata (for ground truth)
add_executable(dump_font_metadata
        dump_font_metadata.cc
        test_data.hh
)

target_link_libraries(dump_font_metadata
        PRIVATE
        onyx_font
)

target_include_directories(dump_font_metadata PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_compile_definitions(dump_font_metadata PRIVATE
        TESTDATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/testdata"
)
