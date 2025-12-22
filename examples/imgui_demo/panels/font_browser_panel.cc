//
// Font Browser Panel Implementation
//

#include "font_browser_panel.hh"
#include "../demo_app.hh"
#include "../font_manager.hh"

#include <imgui.h>

#if defined(HAS_NFD)
#include <nfd.h>
#endif

namespace imgui_demo {

void font_browser_panel::render(demo_app& app) {
    auto& mgr = app.fonts();

    // Section: Load Font
    ImGui::SeparatorText("Load Font");

#if defined(HAS_NFD)
    // Button to open file dialog
    if (ImGui::Button("Browse...")) {
        nfdu8char_t* path = nullptr;
        nfdu8filteritem_t filters[] = {
            {"Font Files", "ttf,otf,fon,fnt,chr"},
            {"TrueType", "ttf,otf"},
            {"Windows Bitmap", "fon,fnt"},
            {"BGI Vector", "chr"}
        };

        nfdopendialogu8args_t args = {0};
        args.filterList = filters;
        args.filterCount = sizeof(filters) / sizeof(filters[0]);

        nfdresult_t result = NFD_OpenDialogU8_With(&path, &args);

        if (result == NFD_OKAY) {
            if (mgr.load_font(path)) {
                m_error_message.clear();
                app.set_active_font(mgr.font_count() - 1);
            } else {
                m_error_message = "Failed to load font: ";
                m_error_message += path;
            }
            NFD_FreePathU8(path);
        } else if (result == NFD_ERROR) {
            m_error_message = "File dialog error: ";
            m_error_message += NFD_GetError();
        }
    }
#else
    // Manual path input when NFD is not available
    static char path_buffer[512] = "";
    ImGui::InputText("Font Path", path_buffer, sizeof(path_buffer));
    ImGui::SameLine();
    if (ImGui::Button("Load")) {
        if (path_buffer[0] != '\0') {
            if (mgr.load_font(path_buffer)) {
                m_error_message.clear();
                app.set_active_font(mgr.font_count() - 1);
            } else {
                m_error_message = "Failed to load font: ";
                m_error_message += path_buffer;
            }
        }
    }
    ImGui::TextDisabled("(File browser requires GTK3 - build with -DIMGUI_DEMO_USE_NFD=ON)");
#endif

    // Show error if any
    if (!m_error_message.empty()) {
        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f),
                          "%s", m_error_message.c_str());
    }

    // Section: Test Fonts
    ImGui::SeparatorText("Built-in Test Fonts");

    auto test_fonts = font_manager::get_test_fonts();
    if (test_fonts.empty()) {
        ImGui::TextDisabled("No test fonts available");
    } else {
        for (const auto& [name, path] : test_fonts) {
            if (ImGui::Button(name.c_str())) {
                if (mgr.load_font(path)) {
                    m_error_message.clear();
                    app.set_active_font(mgr.font_count() - 1);
                } else {
                    m_error_message = "Failed to load: " + path;
                }
            }
        }
    }

    // Section: Loaded Fonts
    ImGui::SeparatorText("Loaded Fonts");

    if (mgr.font_count() == 0) {
        ImGui::TextDisabled("No fonts loaded");
    } else {
        for (std::size_t i = 0; i < mgr.font_count(); ++i) {
            const auto* info = mgr.get_info(i);
            if (!info) continue;

            ImGui::PushID(static_cast<int>(i));

            // Selectable font entry
            bool selected = (app.active_font() == i);
            if (ImGui::Selectable(info->name.c_str(), selected)) {
                app.set_active_font(i);
            }

            // Show font type and size
            if (ImGui::IsItemHovered()) {
                ImGui::BeginTooltip();
                ImGui::Text("Path: %s", info->path.c_str());
                ImGui::Text("Type: %s",
                    info->type == onyx_font::font_source_type::bitmap ? "Bitmap" :
                    info->type == onyx_font::font_source_type::vector ? "Vector" :
                    "Outline (TTF)");
                ImGui::Text("Size: %.1f px", info->current_size);
                ImGui::Text("Scalable: %s", info->scalable ? "Yes" : "No");
                ImGui::EndTooltip();
            }

            // Context menu
            if (ImGui::BeginPopupContextItem("font_ctx")) {
                if (ImGui::MenuItem("Remove")) {
                    mgr.remove_font(i);
                    if (app.active_font() >= mgr.font_count() && mgr.font_count() > 0) {
                        app.set_active_font(mgr.font_count() - 1);
                    }
                }
                ImGui::EndPopup();
            }

            ImGui::PopID();
        }
    }

    // Section: Font Settings (for active font)
    if (mgr.font_count() > 0 && app.active_font() < mgr.font_count()) {
        const auto* info = mgr.get_info(app.active_font());
        if (info && info->scalable) {
            ImGui::SeparatorText("Font Size");

            float size = info->current_size;
            if (ImGui::SliderFloat("Size (px)", &size, 8.0f, 100.0f, "%.1f")) {
                mgr.set_font_size(app.active_font(), size);
                app.invalidate_renderer();
            }
        }
    }
}

} // namespace imgui_demo
