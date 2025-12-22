//
// Created by igor on 21/12/2025.
//

#include <onyx_font/text/text_rasterizer.hh>
#include <algorithm>

namespace onyx_font {

text_rasterizer::text_rasterizer(font_source source)
    : m_source(std::move(source)) {}

void text_rasterizer::set_size(float pixels) {
    m_size = pixels;
}

scaled_metrics text_rasterizer::get_metrics() const {
    // For bitmap fonts, return native metrics regardless of set size
    if (m_source.type() == font_source_type::bitmap) {
        float native = m_source.native_size();
        if (native > 0) {
            return m_source.get_scaled_metrics(native);
        }
    }
    return m_source.get_scaled_metrics(m_size);
}

glyph_metrics text_rasterizer::measure_glyph(char32_t codepoint) const {
    return m_source.get_glyph_metrics(codepoint, m_size);
}

text_extents text_rasterizer::measure_text(std::string_view text) const {
    text_extents result;

    if (text.empty()) {
        return result;
    }

    auto metrics = get_metrics();
    result.ascent = metrics.ascent;
    result.descent = metrics.descent;
    result.height = metrics.line_height;

    float pen_x = 0.0f;
    char32_t prev_codepoint = 0;

    for (char32_t codepoint : utf8_view(text)) {
        // Apply kerning
        if (prev_codepoint != 0) {
            pen_x += m_source.get_kerning(prev_codepoint, codepoint, m_size);
        }

        // Get glyph metrics and advance
        auto glyph = m_source.get_glyph_metrics(codepoint, m_size);
        pen_x += glyph.advance_x;

        prev_codepoint = codepoint;
    }

    result.width = pen_x;
    return result;
}

text_extents text_rasterizer::measure_wrapped(std::string_view text, float max_width) const {
    text_extents result;

    if (text.empty()) {
        return result;
    }

    auto metrics = get_metrics();
    result.ascent = metrics.ascent;
    result.descent = metrics.descent;

    float line_width = 0.0f;
    float max_line_width = 0.0f;
    int line_count = 1;

    char32_t prev_codepoint = 0;

    for (char32_t codepoint : utf8_view(text)) {
        // Check for newline
        if (codepoint == '\n') {
            max_line_width = std::max(max_line_width, line_width);
            line_width = 0.0f;
            ++line_count;
            prev_codepoint = 0;
            continue;
        }

        // Apply kerning
        float kern = 0.0f;
        if (prev_codepoint != 0) {
            kern = m_source.get_kerning(prev_codepoint, codepoint, m_size);
        }

        // Get glyph advance
        auto glyph = m_source.get_glyph_metrics(codepoint, m_size);
        float advance = kern + glyph.advance_x;

        // Check if we need to wrap (simple word wrap at any character)
        if (max_width > 0 && line_width + advance > max_width && line_width > 0) {
            max_line_width = std::max(max_line_width, line_width);
            line_width = glyph.advance_x;  // Start new line with current char
            ++line_count;
            prev_codepoint = 0;
        } else {
            line_width += advance;
            prev_codepoint = codepoint;
        }
    }

    // Account for last line
    max_line_width = std::max(max_line_width, line_width);

    result.width = max_line_width;
    result.height = metrics.line_height * static_cast<float>(line_count);

    return result;
}

float text_rasterizer::line_height() const {
    return get_metrics().line_height;
}

} // namespace onyx_font
