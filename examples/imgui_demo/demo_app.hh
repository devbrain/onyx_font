//
// Demo Application - Main controller
//
// Coordinates all panels and manages application state.
// This is the central hub that connects fonts, rendering, and UI.
//

#pragma once

#include "font_manager.hh"
#include "rendering/imgui_font_renderer.hh"
#include "panels/demo_panel.hh"

#include <memory>
#include <vector>
#include <cstddef>

namespace imgui_demo {

/// Main demo application
///
/// Manages fonts, panels, and rendering coordination.
/// This class ties everything together and demonstrates
/// how to integrate onyx_font into an ImGui application.
class demo_app {
public:
    demo_app();

    /// Load default test fonts
    void load_default_fonts();

    /// Render the complete UI
    void render();

    /// Get font manager
    [[nodiscard]] class font_manager& fonts() { return m_font_manager; }
    [[nodiscard]] const class font_manager& fonts() const { return m_font_manager; }

    /// Get/set active font index
    [[nodiscard]] std::size_t active_font() const { return m_active_font; }
    void set_active_font(std::size_t index);

    /// Get ImGui renderer for current font
    [[nodiscard]] imgui_font_renderer* get_imgui_renderer();

    /// Invalidate renderer (call after font changes)
    void invalidate_renderer();

    /// Render text preview (called from text editor panel)
    void render_text_preview(const std::string& text,
                             float x, float y,
                             float width, float height,
                             onyx_font::text_align align,
                             bool wrap,
                             uint32_t color);

private:
    class font_manager m_font_manager;
    std::size_t m_active_font = 0;
    imgui_font_renderer m_renderer;
    bool m_renderer_dirty = true;

    // Panels
    std::vector<std::unique_ptr<demo_panel>> m_panels;

    // UI state
    bool m_show_demo_window = false;

    /// Update renderer if needed
    void update_renderer();
};

} // namespace imgui_demo
