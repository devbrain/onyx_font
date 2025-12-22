# ImGui Demo for onyx_font library
# Demonstrates font rendering with SDL2/SDL3 + OpenGL3 + ImGui

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
    message(STATUS "Found SDL3")
else()
    # Fall back to SDL2
    find_package(SDL2 QUIET)
    if(SDL2_FOUND)
        set(SDL_FOUND TRUE)
        set(SDL_VERSION "2")
        set(SDL_LIBRARIES SDL2::SDL2)
        message(STATUS "Found SDL2")
    endif()
endif()

if(NOT SDL_FOUND)
    message(FATAL_ERROR
        "Neither SDL2 nor SDL3 found!\n"
        "Please install SDL2 or SDL3 development packages:\n"
        "  Ubuntu/Debian: sudo apt install libsdl2-dev\n"
        "  Fedora: sudo dnf install SDL2-devel\n"
        "  macOS: brew install sdl2\n"
        "  Windows: vcpkg install sdl2"
    )
endif()

# ============================================================================
# OpenGL
# ============================================================================

find_package(OpenGL REQUIRED)

# ============================================================================
# Fetch ImGui from GitHub
# ============================================================================

include(FetchContent)

FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.91.6
    GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(imgui)

# Create ImGui library target
add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    # OpenGL3 backend
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)

# Add SDL backend based on version
if(SDL_VERSION STREQUAL "3")
    target_sources(imgui PRIVATE
        ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp
    )
    target_compile_definitions(imgui PUBLIC IMGUI_DEMO_USE_SDL3=1)
else()
    target_sources(imgui PRIVATE
        ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl2.cpp
    )
    target_compile_definitions(imgui PUBLIC IMGUI_DEMO_USE_SDL2=1)
endif()

target_include_directories(imgui PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)

target_link_libraries(imgui PUBLIC
    ${SDL_LIBRARIES}
    OpenGL::GL
)

# ============================================================================
# Fetch Native File Dialog (for file browser) - Optional
# ============================================================================

option(IMGUI_DEMO_USE_NFD "Use native file dialogs (requires GTK3 on Linux)" OFF)

if(IMGUI_DEMO_USE_NFD)
    FetchContent_Declare(
        nfd
        GIT_REPOSITORY https://github.com/btzy/nativefiledialog-extended.git
        GIT_TAG v1.2.1
        GIT_SHALLOW TRUE
    )

    FetchContent_MakeAvailable(nfd)
endif()

# ============================================================================
# ImGui Demo executable
# ============================================================================

add_executable(imgui_font_demo
    main.cc
    demo_app.cc
    font_manager.cc
    rendering/gl_atlas.cc
    rendering/imgui_font_renderer.cc
    panels/font_browser_panel.cc
    panels/text_editor_panel.cc
    panels/atlas_viewer_panel.cc
    panels/metrics_panel.cc
)

target_link_libraries(imgui_font_demo PRIVATE
    onyx_font
    imgui
)

if(IMGUI_DEMO_USE_NFD)
    target_link_libraries(imgui_font_demo PRIVATE nfd)
    target_compile_definitions(imgui_font_demo PRIVATE HAS_NFD=1)
endif()

target_compile_features(imgui_font_demo PRIVATE cxx_std_20)

# Pass test data directory for built-in fonts
target_compile_definitions(imgui_font_demo PRIVATE
    TESTDATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../../unittest/testdata"
)

# Copy fonts to build directory for convenience
add_custom_command(TARGET imgui_font_demo POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/../../unittest/testdata"
        "$<TARGET_FILE_DIR:imgui_font_demo>/fonts"
    COMMENT "Copying test fonts to build directory"
)
