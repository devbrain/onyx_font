//
// Font Browser Panel - Load and manage fonts
//
// Demonstrates:
// - Loading fonts from files using font_factory
// - Handling different font formats (TTF, bitmap, vector)
// - Managing multiple fonts
//

#pragma once

#include "demo_panel.hh"
#include <string>

namespace imgui_demo {

/// Panel for browsing and loading fonts
class font_browser_panel : public demo_panel {
public:
    void render(demo_app& app) override;
    [[nodiscard]] const char* title() const override { return "Font Browser"; }

private:
    std::string m_error_message;
    bool m_show_test_fonts = true;
};

} // namespace imgui_demo
