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

void text_rasterizer::set_style(text_style style) {
    m_render_style.style = style;
}

void text_rasterizer::set_style(const render_style& style) {
    m_render_style = style;
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
    auto metrics = m_source.get_glyph_metrics(codepoint, m_size);
    auto font_metrics = get_metrics();

    const bool is_styled = m_render_style.needs_glyph_transform();
    const bool is_ttf = (m_source.type() == font_source_type::outline);

    // Adjust metrics for bold style (increases width)
    if (m_render_style.is_bold() && m_source.type() != font_source_type::bitmap) {
        int bold_strength = m_render_style.bold_strength;
        if (bold_strength <= 0) {
            bold_strength = calc_bold_strength(m_size);
        }
        metrics.width += static_cast<float>(bold_strength);

        // FreeType's bold synthesis can also increase height slightly
        if (is_ttf) {
            metrics.height += static_cast<float>(bold_strength);
        }
    }

    // Adjust metrics for italic style (increases width due to shear)
    // Applies to both outline (TTF) and vector fonts
    if (m_render_style.is_italic() && m_source.type() != font_source_type::bitmap) {
        float shear = m_render_style.italic_shear;
        // Italic shear shifts the top of the glyph to the right
        // The extra width needed is approximately shear * ascent
        float shear_extra = shear * font_metrics.ascent;
        metrics.width += shear_extra;
    }

    // For TTF glyphs, add extra padding because FreeType's actual rendered
    // size may differ slightly from STB-based metrics due to hinting differences.
    // We add padding to both dimensions and adjust bearing_y so the glyph
    // has room at the top of the buffer.
    if (is_ttf) {
        // Base padding for hinting differences (1px top, 1px bottom, 2px sides)
        float top_padding = 1.0f;
        float bottom_padding = 1.0f;
        metrics.width += 2.0f;
        metrics.height += top_padding + bottom_padding;
        metrics.bearing_y += top_padding;  // Move baseline down to make room at top

        // Additional padding for styled glyphs
        if (is_styled) {
            float extra_padding = 2.0f;
            metrics.width += extra_padding;
            metrics.height += extra_padding;
            metrics.bearing_y += extra_padding / 2.0f;  // Center the extra space
        }
    }

    // If decorations are enabled, adjust metrics to include decoration space
    if (m_render_style.is_underlined() || m_render_style.is_strikethrough()) {
        // Width must be at least advance_x for decoration continuity
        if (metrics.width < metrics.advance_x) {
            metrics.width = metrics.advance_x;
        }

        // For underline, extend height below baseline
        if (m_render_style.is_underlined()) {
            int ul_offset = m_render_style.underline_offset;
            if (ul_offset == 0) {
                ul_offset = calc_underline_offset(font_metrics.descent);
            }
            int ul_thickness = m_render_style.underline_thickness;
            if (ul_thickness <= 0) {
                ul_thickness = calc_decoration_thickness(m_size);
            }

            // Underline extends below baseline by offset + thickness
            float ul_bottom = static_cast<float>(ul_offset + ul_thickness);
            float current_bottom = metrics.bearing_y - metrics.height;

            // If underline extends beyond current glyph bottom
            if (-ul_bottom < current_bottom) {
                // Extend height to include underline
                float extra = current_bottom - (-ul_bottom);
                metrics.height += extra;
            }
        }
    }

    return metrics;
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
