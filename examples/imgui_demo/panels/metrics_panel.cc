//
// Metrics Panel Implementation
//

#include "metrics_panel.hh"
#include "../demo_app.hh"
#include "../font_manager.hh"

#include <onyx_font/text/utf8.hh>

#include <imgui.h>

namespace imgui_demo {

void metrics_panel::render(demo_app& app) {
    auto* cache = app.fonts().get_cache(app.active_font());
    if (!cache) {
        ImGui::TextDisabled("No font loaded");
        return;
    }

    // Font metrics section
    ImGui::SeparatorText("Font Metrics");

    auto metrics = cache->metrics();

    // Visual representation of metrics
    ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
    float canvas_w = 200.0f;
    float canvas_h = 100.0f;
    float baseline_y = canvas_pos.y + 60.0f;

    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    // Background
    draw_list->AddRectFilled(canvas_pos,
                              ImVec2(canvas_pos.x + canvas_w, canvas_pos.y + canvas_h),
                              IM_COL32(30, 30, 30, 255));

    // Baseline
    draw_list->AddLine(
        ImVec2(canvas_pos.x, baseline_y),
        ImVec2(canvas_pos.x + canvas_w, baseline_y),
        IM_COL32(255, 255, 0, 255), 1.0f);
    draw_list->AddText(ImVec2(canvas_pos.x + canvas_w + 5, baseline_y - 7),
                       IM_COL32(255, 255, 0, 255), "baseline");

    // Ascent line
    float ascent_y = baseline_y - metrics.ascent;
    draw_list->AddLine(
        ImVec2(canvas_pos.x, ascent_y),
        ImVec2(canvas_pos.x + canvas_w, ascent_y),
        IM_COL32(0, 255, 0, 255), 1.0f);
    draw_list->AddText(ImVec2(canvas_pos.x + canvas_w + 5, ascent_y - 7),
                       IM_COL32(0, 255, 0, 255), "ascent");

    // Descent line
    float descent_y = baseline_y + metrics.descent;
    draw_list->AddLine(
        ImVec2(canvas_pos.x, descent_y),
        ImVec2(canvas_pos.x + canvas_w, descent_y),
        IM_COL32(255, 100, 100, 255), 1.0f);
    draw_list->AddText(ImVec2(canvas_pos.x + canvas_w + 5, descent_y - 7),
                       IM_COL32(255, 100, 100, 255), "descent");

    ImGui::Dummy(ImVec2(canvas_w + 80, canvas_h + 10));

    // Numeric values
    ImGui::Columns(2, "metrics_cols", false);
    ImGui::Text("Ascent:"); ImGui::NextColumn();
    ImGui::Text("%.2f px", metrics.ascent); ImGui::NextColumn();
    ImGui::Text("Descent:"); ImGui::NextColumn();
    ImGui::Text("%.2f px", metrics.descent); ImGui::NextColumn();
    ImGui::Text("Line Gap:"); ImGui::NextColumn();
    ImGui::Text("%.2f px", metrics.line_gap); ImGui::NextColumn();
    ImGui::Text("Line Height:"); ImGui::NextColumn();
    ImGui::Text("%.2f px", metrics.line_height); ImGui::NextColumn();
    ImGui::Columns(1);

    // Glyph metrics section
    ImGui::SeparatorText("Glyph Metrics");

    ImGui::InputText("Character", m_test_char.data(), m_test_char.capacity() + 1);
    m_test_char.resize(std::strlen(m_test_char.c_str()));

    if (!m_test_char.empty()) {
        // Decode the first codepoint
        auto [codepoint, bytes] = onyx_font::utf8_decode_one(m_test_char);

        if (bytes > 0) {
            // Get the cached glyph
            const auto& glyph = cache->get(codepoint);

            ImGui::Text("Codepoint: U+%04X", static_cast<unsigned>(codepoint));

            ImGui::Columns(2, "glyph_cols", false);
            ImGui::Text("Width:"); ImGui::NextColumn();
            ImGui::Text("%d px", glyph.rect.w); ImGui::NextColumn();
            ImGui::Text("Height:"); ImGui::NextColumn();
            ImGui::Text("%d px", glyph.rect.h); ImGui::NextColumn();
            ImGui::Text("Bearing X:"); ImGui::NextColumn();
            ImGui::Text("%.2f px", glyph.bearing_x); ImGui::NextColumn();
            ImGui::Text("Bearing Y:"); ImGui::NextColumn();
            ImGui::Text("%.2f px", glyph.bearing_y); ImGui::NextColumn();
            ImGui::Text("Advance X:"); ImGui::NextColumn();
            ImGui::Text("%.2f px", glyph.advance_x); ImGui::NextColumn();
            ImGui::Text("Atlas Page:"); ImGui::NextColumn();
            ImGui::Text("%d", glyph.atlas_index); ImGui::NextColumn();
            ImGui::Text("Atlas Position:"); ImGui::NextColumn();
            ImGui::Text("(%d, %d)", glyph.rect.x, glyph.rect.y); ImGui::NextColumn();
            ImGui::Columns(1);
        }
    }

    // Text measurement section
    ImGui::SeparatorText("Text Measurement");

    static char measure_text[256] = "Hello World";
    ImGui::InputText("Measure Text", measure_text, sizeof(measure_text));

    if (measure_text[0] != '\0') {
        auto extents = cache->measure(measure_text);

        ImGui::Columns(2, "measure_cols", false);
        ImGui::Text("Width:"); ImGui::NextColumn();
        ImGui::Text("%.2f px", extents.width); ImGui::NextColumn();
        ImGui::Text("Height:"); ImGui::NextColumn();
        ImGui::Text("%.2f px", extents.height); ImGui::NextColumn();
        ImGui::Columns(1);
    }

    // Tips
    ImGui::SeparatorText("About Metrics");
    ImGui::TextWrapped(
        "Font metrics describe the vertical dimensions of a font:\n"
        "- Ascent: Height above baseline (e.g., top of 'A')\n"
        "- Descent: Depth below baseline (e.g., bottom of 'g')\n"
        "- Line Height: Total height plus gap for line spacing\n\n"
        "Glyph metrics describe individual characters:\n"
        "- Bearing: Offset from pen position to glyph origin\n"
        "- Advance: How far to move pen after drawing glyph");
}

} // namespace imgui_demo
