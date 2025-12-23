# Include required modules
include(GenerateExportHeader)
include(FetchContent)

set(FETCHCONTENT_QUIET OFF)
if (NOT TARGET mzexplode::libexe)
    message(STATUS "libexe not found, fetching from GitHub...")
    set(MZEXPLODE_BUILD_TESTING OFF CACHE BOOL "" FORCE)
    set(MZEXPLODE_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
    set(MZEXPLODE_BUILD_DOCS OFF CACHE BOOL "" FORCE)

    FetchContent_Declare(
            mzexplode
            GIT_REPOSITORY https://github.com/devbrain/mz-explode.git
            GIT_TAG master
            GIT_SHALLOW TRUE
            GIT_PROGRESS TRUE
    )

    FetchContent_MakeAvailable(mzexplode)
else ()
    message(STATUS "Using existing mz-explode (provided by parent project)")
endif ()

# ===========================================================================
# Failsafe Dependency
# ===========================================================================
# Note: Failsafe is fetched via FetchContent (not ext/) due to OVERRIDE_FIND_PACKAGE

if (NOT TARGET failsafe AND NOT TARGET thirdparty::failsafe)
    find_package(failsafe QUIET)

    if (NOT failsafe_FOUND)
        message(STATUS "failsafe not found - fetching from GitHub...")

        # Disable failsafe tests and examples
        set(FAILSAFE_BUILD_TESTS OFF CACHE INTERNAL "")
        set(FAILSAFE_BUILD_EXAMPLES OFF CACHE INTERNAL "")

        fetchcontent_declare(
                FAILSAFE
                GIT_REPOSITORY "https://github.com/devbrain/failsafe.git"
                GIT_TAG master
                GIT_PROGRESS TRUE
                UPDATE_COMMAND ""
                OVERRIDE_FIND_PACKAGE
        )
        fetchcontent_makeavailable(FAILSAFE)
    endif ()
endif ()


# DataScript - binary format parser generator
# Guard: only fetch if not already available (could be provided by parent project)
if (NOT TARGET ds)
    message(STATUS "DataScript not found, fetching from GitHub...")

    set(DATASCRIPT_BUILD_TESTS OFF CACHE BOOL "" FORCE)

    FetchContent_Declare(
            datascript
            GIT_REPOSITORY https://github.com/devbrain/datascript.git
            GIT_TAG master
            GIT_SHALLOW TRUE
            GIT_PROGRESS TRUE
    )

    FetchContent_MakeAvailable(datascript)
else ()
    message(STATUS "Using existing DataScript (provided by parent project)")
endif ()

# ===========================================================================
# euler - Line rasterization library (for vector fonts)
# ===========================================================================
if (NOT TARGET euler::euler)
    find_package(euler QUIET)

    if (NOT euler_FOUND)
        message(STATUS "euler not found, fetching from GitHub...")

        set(EULER_BUILD_TESTS OFF CACHE BOOL "" FORCE)
        set(EULER_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
        set(EULER_BUILD_BENCHMARKS OFF CACHE BOOL "" FORCE)

        FetchContent_Declare(
                euler
                GIT_REPOSITORY https://github.com/devbrain/euler.git
                GIT_TAG master
                GIT_SHALLOW TRUE
                GIT_PROGRESS TRUE
        )

        FetchContent_MakeAvailable(euler)
    endif ()
else ()
    message(STATUS "Using existing euler (provided by parent project)")
endif ()

# ===========================================================================
# stb - Single-file public domain libraries (stb_truetype for TTF parsing)
# ===========================================================================
if (NOT TARGET stb::stb)
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
else ()
    message(STATUS "Using existing stb (provided by parent project)")
endif ()


# Symbol visibility (hide all by default, like Windows)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN YES)

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

# Suppress warnings for source files that include generated code (datascript-generated headers)
# Use compiler ID check to include clang-cl (which sets MSVC=TRUE but supports -Wno-xxx flags)
if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set_source_files_properties(
            loader/bgi_fon.cc
            PROPERTIES COMPILE_OPTIONS "-Wno-unused-variable;-Wno-sign-conversion;-Wno-implicit-int-conversion"
    )
    # Suppress warnings from third-party mzexplode library headers
    set_source_files_properties(
            font_factory.cc
            loader/win_fon.cc
            loader/win_vector_fon.cc
            PROPERTIES COMPILE_OPTIONS "-Wno-sign-conversion;-Wno-shadow;-Wno-conversion"
    )
endif ()

add_library(onyx_font
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

target_include_directories(onyx_font
        PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(onyx_font
        PUBLIC failsafe euler::euler
        PRIVATE mzexplode::libexe onyx_font_parsers stb::stb)

# Generate portable export header
generate_export_header(onyx_font
        BASE_NAME onyx_font
        EXPORT_FILE_NAME ${CMAKE_CURRENT_BINARY_DIR}/include/onyx_font/export.h
)

# Library properties
set_target_properties(onyx_font PROPERTIES
        OUTPUT_NAME onyx_font
        VERSION ${PROJECT_VERSION}
        SOVERSION ${PROJECT_VERSION_MAJOR}
)

# ===========================================================================
# Strict Compiler Warnings (MSVC, GCC, Clang)
# ===========================================================================
if (MSVC OR (WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "Clang"))
    # Suppress MSVC CRT deprecation warnings from third-party headers (failsafe uses gmtime)
    target_compile_definitions(onyx_font PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

if (MSVC)
    target_compile_options(onyx_font PRIVATE
            /W4           # Warning level 4 (very strict)
            /WX           # Treat warnings as errors
            /permissive-  # Strict standards conformance
            /w14242       # 'identifier': conversion, possible loss of data
            /w14254       # 'operator': conversion, possible loss of data
            /w14263       # Member function does not override any base class virtual member function
            /w14265       # Class has virtual functions, but destructor is not virtual
            /w14287       # 'operator': unsigned/negative constant mismatch
            /we4289       # Nonstandard extension used: 'variable': loop control variable is used outside the for-loop scope
            /w14296       # 'operator': expression is always 'boolean_value'
            /w14311       # 'variable': pointer truncation from 'type1' to 'type2'
            /w14545       # Expression before comma evaluates to a function which is missing an argument list
            /w14546       # Function call before comma missing argument list
            /w14547       # 'operator': operator before comma has no effect; expected operator with side-effect
            /w14549       # 'operator': operator before comma has no effect; did you intend 'operator'?
            /w14555       # Expression has no effect; expected expression with side-effect
            /w14619       # Pragma warning: there is no warning number 'number'
            /w14640       # Enable warning on thread unsafe static member initialization
            /w14826       # Conversion from 'type1' to 'type2' is sign-extended
            /w14905       # Wide string literal cast to 'LPSTR'
            /w14906       # String literal cast to 'LPWSTR'
            /w14928       # Illegal copy-initialization; more than one user-defined conversion has been implicitly applied
            /wd4251       # Disable warning about DLL interface for STL types (common in shared libraries)
            /wd4458       # Disable warning about declaration hiding class member (from mzexplode headers)
    )

    # Handle clang-cl (Clang frontend with MSVC backend) - needs Clang warning flags via /clang: prefix
    if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        target_compile_options(onyx_font PRIVATE
                /clang:-Wno-unused-parameter  # Suppress warnings from third-party euler library headers
        )
    endif ()
else ()
    # GCC and Clang common warnings
    target_compile_options(onyx_font PRIVATE
            -Wall                       # Enable most warnings
            -Wextra                     # Enable extra warnings
            -Wpedantic                  # Strict ISO C++ compliance
            -Werror                     # Treat warnings as errors
            -Wshadow                    # Warn when a variable shadows another
            -Wnon-virtual-dtor          # Warn when a class with virtual functions has non-virtual destructor
            -Wold-style-cast            # Warn on C-style casts
            -Wcast-align                # Warn on potential alignment issues
            -Wunused                    # Warn on unused entities
            -Woverloaded-virtual        # Warn when function hides virtual from base class
            -Wconversion                # Warn on implicit conversions that may alter value
            -Wsign-conversion           # Warn on sign conversions
            -Wnull-dereference          # Warn when dereferencing null pointer
            -Wdouble-promotion          # Warn when float is implicitly promoted to double
            -Wformat=2                  # Warn on format string issues
            -Wimplicit-fallthrough      # Warn on fallthrough in switch without annotation
            -Wmisleading-indentation    # Warn when indentation implies blocks that don't exist
    )

    # GCC-specific warnings
    if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_compile_options(onyx_font PRIVATE
                -Wduplicated-cond           # Warn on duplicated conditions in if-else chains
                # Note: -Wduplicated-branches disabled due to false positive in third-party failsafe library
                -Wlogical-op                # Warn on suspicious uses of logical operators
                -Wuseless-cast              # Warn on useless casts
                -Wsuggest-override          # Suggest override for virtual functions
        )
    endif ()

    # Clang-specific warnings
    if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        target_compile_options(onyx_font PRIVATE
                -Wmost                      # Enable most warnings
                -Wthread-safety             # Thread safety analysis (Clang)
                -Wno-c++98-compat           # Don't warn about C++98 incompatibility
                -Wno-c++98-compat-pedantic  # Don't warn about C++98 incompatibility (pedantic)
                -Wno-unused-parameter       # Suppress unused parameter warnings from third-party euler library
        )
    endif ()
endif ()

# Namespace alias for FetchContent compatibility
add_library(onyx::font ALIAS onyx_font)