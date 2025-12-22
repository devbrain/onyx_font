# Examples for onyx_font text rendering

# Basic text rendering demo (no external dependencies)
add_executable(text_rendering_demo
        text_rendering_demo.cc
)

target_link_libraries(text_rendering_demo
        PRIVATE
        onyx_font
)

# SDL demos (ImGui demo, text scroller) - require SDL2 or SDL3
if(ONYX_FONT_BUILD_DEMOS)
    add_subdirectory(imgui_demo)
    add_subdirectory(text_scroller)
endif()
