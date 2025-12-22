//
// Demo Application Implementation
//

#include "demo_app.hh"

#include "panels/font_browser_panel.hh"
#include "panels/text_editor_panel.hh"
#include "panels/atlas_viewer_panel.hh"
#include "panels/metrics_panel.hh"

#include <imgui.h>

namespace imgui_demo {

demo_app::demo_app() {
    // Create all panels
    m_panels.push_back(std::make_unique<font_browser_panel>());
    m_panels.push_back(std::make_unique<text_editor_panel>());
    m_panels.push_back(std::make_unique<atlas_viewer_panel>());
    m_panels.push_back(std::make_unique<metrics_panel>());
}

void demo_app::load_default_fonts() {
    // Try to load first available test font
    auto fonts = font_manager::get_test_fonts();
    if (!fonts.empty()) {
        // Try to load a TTF first
        for (const auto& [name, path] : fonts) {
            if (path.find(".ttf") != std::string::npos ||
                path.find(".TTF") != std::string::npos) {
                if (m_font_manager.load_font(path)) {
                    return;
                }
            }
        }
        // Fall back to first available
        m_font_manager.load_font(fonts[0].second);
    }
}

void demo_app::render() {
    // Menu bar
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Quit", "Alt+F4")) {
                // Request quit - handled in main loop
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("ImGui Demo", nullptr, &m_show_demo_window);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Help")) {
            if (ImGui::MenuItem("About")) {
                ImGui::OpenPopup("About##popup");
            }
            ImGui::EndMenu();
        }

        // About popup
        if (ImGui::BeginPopupModal("About##popup", nullptr,
                                    ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("onyx_font ImGui Demo");
            ImGui::Separator();
            ImGui::Text("A demonstration of the onyx_font library.");
            ImGui::Text("Supports TTF, Windows bitmap (FON), and BGI vector fonts.");
            ImGui::Separator();
            if (ImGui::Button("Close")) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        ImGui::EndMainMenuBar();
    }

    // Show ImGui demo if requested
    if (m_show_demo_window) {
        ImGui::ShowDemoWindow(&m_show_demo_window);
    }

    // Render each panel in its own window
    // Set initial window positions
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    float panel_width = 350.0f;

    // Font Browser panel - top left
    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + 10, viewport->WorkPos.y + 10),
                            ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(panel_width, 300), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(m_panels[0]->title())) {
        m_panels[0]->render(*this);
    }
    ImGui::End();

    // Text Editor panel - center/right
    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + panel_width + 20, viewport->WorkPos.y + 10),
                            ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(viewport->WorkSize.x - panel_width - 40, 400),
                            ImGuiCond_FirstUseEver);
    if (ImGui::Begin(m_panels[1]->title())) {
        m_panels[1]->render(*this);
    }
    ImGui::End();

    // Atlas Viewer panel - bottom left
    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + 10, viewport->WorkPos.y + 320),
                            ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(panel_width, 300), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(m_panels[2]->title())) {
        m_panels[2]->render(*this);
    }
    ImGui::End();

    // Metrics panel - bottom center
    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + panel_width + 20, viewport->WorkPos.y + 420),
                            ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(400, 350), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(m_panels[3]->title())) {
        m_panels[3]->render(*this);
    }
    ImGui::End();

    // Status bar style info window
    ImGui::SetNextWindowPos(
        ImVec2(viewport->WorkPos.x,
               viewport->WorkPos.y + viewport->WorkSize.y - 30),
        ImGuiCond_Always);
    ImGui::SetNextWindowSize(
        ImVec2(viewport->WorkSize.x, 30),
        ImGuiCond_Always);

    ImGuiWindowFlags status_flags =
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoSavedSettings;

    if (ImGui::Begin("##StatusBar", nullptr, status_flags)) {
        if (m_font_manager.font_count() > 0) {
            if (const auto* info = m_font_manager.get_info(m_active_font)) {
                ImGui::Text("Font: %s | Size: %.1f px | Type: %s",
                    info->name.c_str(),
                    info->current_size,
                    info->type == onyx_font::font_source_type::bitmap ? "Bitmap" :
                    info->type == onyx_font::font_source_type::vector ? "Vector" :
                    "TTF");
            }
        } else {
            ImGui::TextDisabled("No font loaded - use Font Browser to load one");
        }
    }
    ImGui::End();
}

void demo_app::set_active_font(std::size_t index) {
    if (index != m_active_font) {
        m_active_font = index;
        m_renderer_dirty = true;
    }
}

imgui_font_renderer* demo_app::get_imgui_renderer() {
    update_renderer();
    return &m_renderer;
}

void demo_app::invalidate_renderer() {
    m_renderer_dirty = true;
}

void demo_app::update_renderer() {
    if (!m_renderer_dirty) {
        return;
    }

    auto* cache = m_font_manager.get_cache(m_active_font);
    m_renderer.set_cache(cache);
    m_renderer_dirty = false;
}

void demo_app::render_text_preview(const std::string& text,
                                    float x, float y,
                                    float width, float height,
                                    onyx_font::text_align align,
                                    bool wrap,
                                    uint32_t color) {
    update_renderer();

    if (text.empty()) {
        return;
    }

    auto* text_renderer = m_renderer.get_text_renderer();
    if (!text_renderer) {
        return;
    }

    m_renderer.sync_textures();

    if (wrap) {
        onyx_font::text_box box{x, y, width, height};
        m_renderer.draw_text_wrapped(text, box, align, color);
    } else if (align != onyx_font::text_align::left) {
        m_renderer.draw_text_aligned(text, x, y, width, align, color);
    } else {
        m_renderer.draw_text(text, x, y, color);
    }
}

} // namespace imgui_demo
