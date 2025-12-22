//
// Text Editor Panel - Text input and rendering controls
//
// Demonstrates:
// - Rendering text with text_renderer
// - Text alignment (left, center, right)
// - Word wrapping in bounding boxes
// - Color and style options
//

#pragma once

#include "demo_panel.hh"
#include <onyx_font/text/types.hh>
#include <string>
#include <array>

namespace imgui_demo {

/// Panel for editing and rendering text
class text_editor_panel : public demo_panel {
public:
    text_editor_panel();

    void render(demo_app& app) override;
    [[nodiscard]] const char* title() const override { return "Text Editor"; }

    /// Get the current text
    [[nodiscard]] const std::string& text() const { return m_text; }

    /// Get text alignment
    [[nodiscard]] onyx_font::text_align alignment() const { return m_alignment; }

    /// Get text color (RGBA)
    [[nodiscard]] const std::array<float, 4>& color() const { return m_color; }

    /// Is word wrap enabled?
    [[nodiscard]] bool wrap_enabled() const { return m_wrap_enabled; }

    /// Get wrap width
    [[nodiscard]] float wrap_width() const { return m_wrap_width; }

private:
    std::string m_text;
    onyx_font::text_align m_alignment = onyx_font::text_align::left;
    std::array<float, 4> m_color = {1.0f, 1.0f, 1.0f, 1.0f};
    bool m_wrap_enabled = false;
    float m_wrap_width = 300.0f;
};

} // namespace imgui_demo
