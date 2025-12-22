//
// Atlas Viewer Panel Implementation
//

#include "atlas_viewer_panel.hh"
#include "../demo_app.hh"

#include <imgui.h>

namespace imgui_demo {

void atlas_viewer_panel::render(demo_app& app) {
    auto* renderer = app.get_imgui_renderer();
    if (!renderer) {
        ImGui::TextDisabled("No font loaded");
        return;
    }

    int atlas_count = renderer->atlas_count();
    if (atlas_count == 0) {
        ImGui::TextDisabled("No atlas pages");
        return;
    }

    // Page selector
    ImGui::SeparatorText("Atlas Pages");

    if (atlas_count > 1) {
        ImGui::SliderInt("Page", &m_selected_page, 0, atlas_count - 1);
    } else {
        ImGui::Text("Page: 0 (single page)");
        m_selected_page = 0;
    }

    // Zoom control
    ImGui::SliderFloat("Zoom", &m_zoom, 0.25f, 4.0f, "%.2fx");
    ImGui::SameLine();
    if (ImGui::Button("Reset")) {
        m_zoom = 1.0f;
    }

    // Info
    ImGui::SeparatorText("Info");
    ImGui::Text("Total pages: %d", atlas_count);
    ImGui::Text("Zoom: %.0f%%", m_zoom * 100.0f);

    // Atlas texture display
    ImGui::SeparatorText("Texture");

    ImTextureID tex_id = renderer->get_texture(m_selected_page);
    if (tex_id) {
        // Get cache for dimensions
        auto* cache = app.fonts().get_cache(app.active_font());
        if (cache) {
            const auto& atlas = cache->atlas(m_selected_page);
            int tex_w = atlas.width();
            int tex_h = atlas.height();

            ImGui::Text("Size: %dx%d", tex_w, tex_h);

            // Display with scroll
            ImVec2 display_size(static_cast<float>(tex_w) * m_zoom,
                                static_cast<float>(tex_h) * m_zoom);

            ImGui::BeginChild("AtlasScroll", ImVec2(0, 0), true,
                              ImGuiWindowFlags_HorizontalScrollbar);

            ImVec2 pos = ImGui::GetCursorScreenPos();

            // Draw checkered background to show transparency
            ImDrawList* draw_list = ImGui::GetWindowDrawList();
            int checker_size = 16;
            for (int y = 0; y < static_cast<int>(display_size.y); y += checker_size) {
                for (int x = 0; x < static_cast<int>(display_size.x); x += checker_size) {
                    bool dark = ((x / checker_size) + (y / checker_size)) % 2 == 0;
                    ImU32 col = dark ? IM_COL32(40, 40, 40, 255)
                                     : IM_COL32(60, 60, 60, 255);
                    float x0 = pos.x + static_cast<float>(x);
                    float y0 = pos.y + static_cast<float>(y);
                    float x1 = std::min(x0 + static_cast<float>(checker_size),
                                        pos.x + display_size.x);
                    float y1 = std::min(y0 + static_cast<float>(checker_size),
                                        pos.y + display_size.y);
                    draw_list->AddRectFilled(ImVec2(x0, y0), ImVec2(x1, y1), col);
                }
            }

            // Draw the atlas texture
            ImGui::Image(tex_id, display_size);

            // Draw border
            draw_list->AddRect(pos,
                               ImVec2(pos.x + display_size.x, pos.y + display_size.y),
                               IM_COL32(128, 128, 128, 255));

            ImGui::EndChild();
        }
    } else {
        ImGui::TextDisabled("Texture not available");
    }

    // Tips
    ImGui::SeparatorText("About Glyph Atlases");
    ImGui::TextWrapped(
        "Glyph atlases pack rendered glyphs into textures for efficient GPU rendering. "
        "When a glyph is first requested, it's rasterized and added to the atlas. "
        "Multiple atlas pages are created if glyphs don't fit in a single texture.");
}

} // namespace imgui_demo
