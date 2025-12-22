//
// Text Editor Panel Implementation
//

#include "text_editor_panel.hh"
#include "../demo_app.hh"

#include <imgui.h>

namespace imgui_demo {

text_editor_panel::text_editor_panel() {
    // Default sample text showing various features
    m_text = "Hello, onyx_font!\n\n"
             "This demo shows text rendering with:\n"
             "- TrueType fonts (TTF/OTF)\n"
             "- Bitmap fonts (Windows FON/FNT)\n"
             "- Vector fonts (Borland BGI CHR)\n\n"
             "Try different fonts and sizes!\n"
             "Unicode: cafe \xC3\xA9\xC3\xA8 \xE2\x9C\x93";  // UTF-8 encoded
}

void text_editor_panel::render(demo_app& app) {
    // Text input
    ImGui::SeparatorText("Text Input");

    // Multiline text editor
    ImGui::InputTextMultiline("##text", &m_text[0], m_text.capacity() + 1,
                               ImVec2(-1, 150),
                               ImGuiInputTextFlags_CallbackResize,
                               [](ImGuiInputTextCallbackData* data) {
                                   if (data->EventFlag == ImGuiInputTextFlags_CallbackResize) {
                                       auto* str = static_cast<std::string*>(data->UserData);
                                       str->resize(static_cast<std::size_t>(data->BufTextLen));
                                       data->Buf = str->data();
                                   }
                                   return 0;
                               },
                               &m_text);
    // Update string length if needed
    m_text.resize(std::strlen(m_text.c_str()));

    // Quick text presets
    if (ImGui::Button("Pangram")) {
        m_text = "The quick brown fox jumps over the lazy dog.";
    }
    ImGui::SameLine();
    if (ImGui::Button("Lorem")) {
        m_text = "Lorem ipsum dolor sit amet, consectetur adipiscing elit. "
                 "Sed do eiusmod tempor incididunt ut labore et dolore magna aliqua.";
    }
    ImGui::SameLine();
    if (ImGui::Button("Unicode")) {
        m_text = "Caf\xC3\xA9 \xE2\x98\x95 | \xC2\xA9 2025 | \xE2\x9C\x93 Check";
    }

    // Alignment
    ImGui::SeparatorText("Alignment");

    int align = static_cast<int>(m_alignment);
    ImGui::RadioButton("Left", &align, 0);
    ImGui::SameLine();
    ImGui::RadioButton("Center", &align, 1);
    ImGui::SameLine();
    ImGui::RadioButton("Right", &align, 2);
    m_alignment = static_cast<onyx_font::text_align>(align);

    // Word wrapping
    ImGui::SeparatorText("Word Wrap");

    ImGui::Checkbox("Enable Word Wrap", &m_wrap_enabled);
    if (m_wrap_enabled) {
        ImGui::SliderFloat("Wrap Width", &m_wrap_width, 100.0f, 600.0f, "%.0f px");
    }

    // Color picker
    ImGui::SeparatorText("Color");

    ImGui::ColorEdit4("Text Color", m_color.data(),
                      ImGuiColorEditFlags_AlphaPreview);

    // Rendering preview
    ImGui::SeparatorText("Preview");

    // Get rendering area
    ImVec2 preview_pos = ImGui::GetCursorScreenPos();
    ImVec2 preview_size = ImGui::GetContentRegionAvail();
    preview_size.y = std::max(preview_size.y, 200.0f);

    // Draw background
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    draw_list->AddRectFilled(preview_pos,
                              ImVec2(preview_pos.x + preview_size.x,
                                     preview_pos.y + preview_size.y),
                              IM_COL32(30, 30, 30, 255));

    // Draw the text using onyx_font
    app.render_text_preview(m_text, preview_pos.x + 10, preview_pos.y + 10,
                            m_wrap_enabled ? m_wrap_width : (preview_size.x - 20),
                            preview_size.y - 20,
                            m_alignment,
                            m_wrap_enabled,
                            ImGui::ColorConvertFloat4ToU32(
                                ImVec4(m_color[0], m_color[1], m_color[2], m_color[3])));

    // Reserve space for preview
    ImGui::Dummy(preview_size);

    // Draw wrap boundary indicator
    if (m_wrap_enabled) {
        float line_x = preview_pos.x + 10 + m_wrap_width;
        draw_list->AddLine(
            ImVec2(line_x, preview_pos.y),
            ImVec2(line_x, preview_pos.y + preview_size.y),
            IM_COL32(100, 100, 100, 128), 1.0f);
    }
}

} // namespace imgui_demo
