//
// Metrics Panel - Display font metrics information
//
// Demonstrates:
// - Font metrics (ascent, descent, line height)
// - Glyph metrics (advance, bearing)
// - Text measurement
//

#pragma once

#include "demo_panel.hh"
#include <string>

namespace imgui_demo {

/// Panel for displaying font metrics
class metrics_panel : public demo_panel {
public:
    void render(demo_app& app) override;
    [[nodiscard]] const char* title() const override { return "Font Metrics"; }

private:
    std::string m_test_char = "A";
};

} // namespace imgui_demo
