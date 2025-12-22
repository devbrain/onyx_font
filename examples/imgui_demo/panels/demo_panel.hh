//
// Demo Panel Base - Interface for modular UI panels
//
// Each panel is self-contained and demonstrates a specific aspect
// of the onyx_font library.
//

#pragma once

namespace imgui_demo {

class demo_app;

/// Base interface for demo panels
class demo_panel {
public:
    virtual ~demo_panel() = default;

    /// Render the panel UI
    /// @param app Reference to main application for shared state
    virtual void render(demo_app& app) = 0;

    /// Get panel title for window
    [[nodiscard]] virtual const char* title() const = 0;
};

} // namespace imgui_demo
