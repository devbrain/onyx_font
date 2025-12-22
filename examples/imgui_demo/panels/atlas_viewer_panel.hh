//
// Atlas Viewer Panel - Visualize glyph texture atlas
//
// Demonstrates:
// - How glyphs are packed into texture atlases
// - Atlas page management
// - Texture coordinates for rendering
//

#pragma once

#include "demo_panel.hh"

namespace imgui_demo {

/// Panel for viewing glyph atlas textures
class atlas_viewer_panel : public demo_panel {
public:
    void render(demo_app& app) override;
    [[nodiscard]] const char* title() const override { return "Atlas Viewer"; }

private:
    int m_selected_page = 0;
    float m_zoom = 1.0f;
};

} // namespace imgui_demo
