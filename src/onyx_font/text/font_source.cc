//
// Created by igor on 21/12/2025.
//

#include <onyx_font/text/font_source.hh>
#include <euler/dda/line_iterator.hh>
#include <euler/dda/aa_line_iterator.hh>
#include <euler/coordinates/point2.hh>
#include <algorithm>
#include <cmath>

namespace onyx_font {

font_source font_source::from_bitmap(const bitmap_font& font) {
    font_source source;
    source.m_font = bitmap_ref{&font};
    return source;
}

font_source font_source::from_vector(const vector_font& font) {
    font_source source;
    source.m_font = vector_ref{&font};
    return source;
}

font_source font_source::from_ttf(const ttf_font& font) {
    font_source source;
    source.m_font = ttf_ref{&font};
    // Create owned rasterizer from the font's data
    source.m_rasterizer = std::make_unique<stb_truetype_font>(
        font.data(), font.font_index());
    return source;
}

font_source::~font_source() = default;

font_source::font_source(font_source&&) noexcept = default;

font_source& font_source::operator=(font_source&&) noexcept = default;

font_source_type font_source::type() const {
    if (std::holds_alternative<bitmap_ref>(m_font)) {
        return font_source_type::bitmap;
    } else if (std::holds_alternative<vector_ref>(m_font)) {
        return font_source_type::vector;
    } else {
        return font_source_type::outline;
    }
}

bool font_source::has_glyph(char32_t codepoint) const {
    // For bitmap and vector fonts, only support 8-bit codepoints
    if (std::holds_alternative<bitmap_ref>(m_font)) {
        if (codepoint > 255) return false;
        const auto& font = *std::get<bitmap_ref>(m_font).font;
        auto ch = static_cast<uint8_t>(codepoint);
        return ch >= font.get_first_char() && ch <= font.get_last_char();
    } else if (std::holds_alternative<vector_ref>(m_font)) {
        if (codepoint > 255) return false;
        const auto& font = *std::get<vector_ref>(m_font).font;
        return font.has_glyph(static_cast<uint8_t>(codepoint));
    } else {
        const auto& ref = std::get<ttf_ref>(m_font);
        return ref.font->has_glyph(static_cast<uint32_t>(codepoint));
    }
}

char32_t font_source::default_char() const {
    if (std::holds_alternative<bitmap_ref>(m_font)) {
        return std::get<bitmap_ref>(m_font).font->get_default_char();
    } else if (std::holds_alternative<vector_ref>(m_font)) {
        return std::get<vector_ref>(m_font).font->get_default_char();
    } else {
        // TTF fonts typically use space or question mark as fallback
        return '?';
    }
}

scaled_metrics font_source::get_scaled_metrics(float size) const {
    scaled_metrics result;

    if (std::holds_alternative<bitmap_ref>(m_font)) {
        const auto& metrics = std::get<bitmap_ref>(m_font).font->get_metrics();
        result.ascent = static_cast<float>(metrics.ascent);
        result.descent = static_cast<float>(metrics.pixel_height - metrics.ascent);
        result.line_gap = static_cast<float>(metrics.external_leading);
        result.line_height = static_cast<float>(metrics.pixel_height + metrics.external_leading);
    } else if (std::holds_alternative<vector_ref>(m_font)) {
        const auto& metrics = std::get<vector_ref>(m_font).font->get_metrics();
        // Vector fonts need scaling from native size
        float scale = size / static_cast<float>(metrics.pixel_height);
        result.ascent = static_cast<float>(metrics.ascent) * scale;
        result.descent = static_cast<float>(-metrics.descent) * scale;  // descent is negative
        result.line_gap = 0;
        result.line_height = size;
    } else {
        const auto& ref = std::get<ttf_ref>(m_font);
        auto ttf_metrics = ref.font->get_metrics(size);
        result.ascent = ttf_metrics.ascent;
        result.descent = -ttf_metrics.descent;  // Convert from negative to positive
        result.line_gap = ttf_metrics.line_gap;
        result.line_height = result.ascent + result.descent + result.line_gap;
    }

    return result;
}

glyph_metrics font_source::get_glyph_metrics(char32_t codepoint, float size) const {
    glyph_metrics result;

    if (std::holds_alternative<bitmap_ref>(m_font)) {
        if (codepoint > 255) return result;
        const auto& font = *std::get<bitmap_ref>(m_font).font;
        auto ch = static_cast<uint8_t>(codepoint);

        if (ch < font.get_first_char() || ch > font.get_last_char()) {
            ch = font.get_default_char();
            // Validate default char is also within range
            if (ch < font.get_first_char() || ch > font.get_last_char()) {
                return result;  // Invalid default char, return empty metrics
            }
        }

        const auto& spacing = font.get_spacing(ch);
        bitmap_view glyph = font.get_glyph(ch);

        result.width = static_cast<float>(glyph.width());
        result.height = static_cast<float>(glyph.height());
        result.bearing_x = spacing.a_space ? static_cast<float>(*spacing.a_space) : 0.0f;
        result.bearing_y = static_cast<float>(font.get_metrics().ascent);

        // Calculate advance from ABC spacing
        float advance = 0;
        if (spacing.a_space) advance += static_cast<float>(*spacing.a_space);
        if (spacing.b_space) {
            advance += static_cast<float>(*spacing.b_space);
        } else {
            advance += static_cast<float>(glyph.width());
        }
        if (spacing.c_space) advance += static_cast<float>(*spacing.c_space);
        result.advance_x = advance;

    } else if (std::holds_alternative<vector_ref>(m_font)) {
        if (codepoint > 255) return result;
        const auto& font = *std::get<vector_ref>(m_font).font;
        auto ch = static_cast<uint8_t>(codepoint);

        const vector_glyph* glyph = font.get_glyph(ch);
        if (!glyph) {
            glyph = font.get_glyph(font.get_default_char());
            if (!glyph) return result;
        }

        const auto& metrics = font.get_metrics();
        float scale = size / static_cast<float>(metrics.pixel_height);

        result.advance_x = static_cast<float>(glyph->width) * scale;
        result.bearing_x = 0;

        // Calculate bounding box from strokes first
        int min_x = 0, min_y = 0, max_x = 0, max_y = 0;
        int pen_x = 0, pen_y = 0;
        for (const auto& cmd : glyph->strokes) {
            if (cmd.type != stroke_type::END) {
                pen_x += cmd.dx;
                pen_y += cmd.dy;
                min_x = std::min(min_x, pen_x);
                min_y = std::min(min_y, pen_y);
                max_x = std::max(max_x, pen_x);
                max_y = std::max(max_y, pen_y);
            }
        }

        // Width and height include the origin point (0,0) plus all stroke endpoints
        // Add 1 to include the origin point in the bounding box
        result.width = static_cast<float>(max_x - min_x + 1) * scale;
        result.height = static_cast<float>(max_y - min_y + 1) * scale;
        // bearing_y = distance from top of glyph bounding box to the rendering origin
        // Since strokes are relative to origin and min_y is typically negative (above origin),
        // bearing_y = -min_y so that rendering at y=bearing_y places strokes starting at y=0
        result.bearing_y = static_cast<float>(-min_y) * scale;

    } else {
        const auto& ref = std::get<ttf_ref>(m_font);
        auto ttf_metrics = ref.font->get_glyph_metrics(
            static_cast<uint32_t>(codepoint), size);

        if (ttf_metrics) {
            result.advance_x = ttf_metrics->advance_x;
            result.bearing_x = ttf_metrics->bearing_x;
            result.bearing_y = ttf_metrics->bearing_y;
            result.width = ttf_metrics->width;
            result.height = ttf_metrics->height;
        }
    }

    return result;
}

float font_source::get_kerning(char32_t first, char32_t second, float size) const {
    // Only TTF fonts support kerning
    if (std::holds_alternative<ttf_ref>(m_font)) {
        const auto& ref = std::get<ttf_ref>(m_font);
        return ref.font->get_kerning(
            static_cast<uint32_t>(first),
            static_cast<uint32_t>(second),
            size);
    }
    return 0.0f;
}

float font_source::native_size() const {
    if (std::holds_alternative<bitmap_ref>(m_font)) {
        return static_cast<float>(
            std::get<bitmap_ref>(m_font).font->get_metrics().pixel_height);
    }
    // Scalable fonts have no native size
    return 0.0f;
}

void font_source::rasterize_bitmap_glyph(char32_t codepoint, float /*size*/,
                                          void* target, int x, int y,
                                          void (*put_pixel)(void*, int, int, uint8_t)) const {
    if (codepoint > 255) return;

    const auto& font = *std::get<bitmap_ref>(m_font).font;
    auto ch = static_cast<uint8_t>(codepoint);

    if (ch < font.get_first_char() || ch > font.get_last_char()) {
        ch = font.get_default_char();
        // Validate default char is also within range
        if (ch < font.get_first_char() || ch > font.get_last_char()) {
            return;  // Invalid default char, nothing to render
        }
    }

    const auto& spacing = font.get_spacing(ch);
    bitmap_view glyph = font.get_glyph(ch);

    // Apply left side bearing
    int glyph_x = x;
    if (spacing.a_space) {
        glyph_x += *spacing.a_space;
    }

    // Adjust y for baseline (y is baseline, top of glyph is y - ascent)
    int glyph_y = y - static_cast<int>(font.get_metrics().ascent);

    // Convert 1-bit to 8-bit and write to target
    for (uint16_t gy = 0; gy < glyph.height(); ++gy) {
        for (uint16_t gx = 0; gx < glyph.width(); ++gx) {
            if (glyph.pixel(gx, gy)) {
                put_pixel(target, glyph_x + gx, glyph_y + gy, 255);
            }
        }
    }
}

// Line drawing using euler library
namespace {

void draw_line_aa(void* target, float x0, float y0, float x1, float y1,
                  void (*put_pixel)(void*, int, int, uint8_t),
                  int width, int height) {
    // Use euler's Wu's algorithm for antialiased lines
    euler::point2f start{x0, y0};
    euler::point2f end{x1, y1};

    auto line = euler::dda::make_aa_line_iterator(start, end);
    while (line != euler::dda::aa_line_iterator<float>::end()) {
        auto pixel = *line;
        int px = static_cast<int>(pixel.pos.x);
        int py = static_cast<int>(pixel.pos.y);
        if (px >= 0 && px < width && py >= 0 && py < height) {
            uint8_t alpha = static_cast<uint8_t>(
                std::clamp(pixel.coverage * 255.0f, 0.0f, 255.0f));
            if (alpha > 0) {
                put_pixel(target, px, py, alpha);
            }
        }
        ++line;
    }
}

} // anonymous namespace

void font_source::rasterize_vector_glyph(char32_t codepoint, float size,
                                          void* target, int x, int y,
                                          void (*put_pixel)(void*, int, int, uint8_t),
                                          int width, int height) const {
    if (codepoint > 255) return;

    const auto& font = *std::get<vector_ref>(m_font).font;
    auto ch = static_cast<uint8_t>(codepoint);

    const vector_glyph* glyph = font.get_glyph(ch);
    if (!glyph) {
        glyph = font.get_glyph(font.get_default_char());
        if (!glyph) return;
    }

    const auto& metrics = font.get_metrics();
    float scale = size / static_cast<float>(metrics.pixel_height);

    // Use y directly as the pen origin (matches glyph_rasterizer.hh behavior)
    // The caller (glyph_cache) already positions y correctly based on bearing_y
    float origin_x = static_cast<float>(x);
    float origin_y = static_cast<float>(y);

    float pen_x = origin_x;
    float pen_y = origin_y;
    // Start with pen down - some fonts (like BGI) start with LINE_TO from origin
    bool pen_down = true;

    for (const auto& cmd : glyph->strokes) {
        switch (cmd.type) {
            case stroke_type::MOVE_TO: {
                // Move without drawing, start new polyline
                pen_x += static_cast<float>(cmd.dx) * scale;
                pen_y += static_cast<float>(cmd.dy) * scale;
                pen_down = true;
                break;
            }

            case stroke_type::LINE_TO: {
                float new_x = pen_x + static_cast<float>(cmd.dx) * scale;
                float new_y = pen_y + static_cast<float>(cmd.dy) * scale;

                if (pen_down) {
                    draw_line_aa(target, pen_x, pen_y, new_x, new_y,
                                 put_pixel, width, height);
                }

                pen_x = new_x;
                pen_y = new_y;
                break;
            }

            case stroke_type::END:
                pen_down = false;
                break;
        }
    }
}

void font_source::rasterize_ttf_glyph(char32_t codepoint, float size,
                                       void* target, int x, int y,
                                       void (*put_pixel)(void*, int, int, uint8_t)) const {
    if (!m_rasterizer) return;
    auto bitmap = m_rasterizer->rasterize(static_cast<uint32_t>(codepoint), size);

    if (!bitmap) return;

    // y is baseline, offset_y is negative (distance from baseline to top)
    int glyph_x = x + bitmap->offset_x;
    int glyph_y = y + bitmap->offset_y;

    for (int gy = 0; gy < bitmap->height; ++gy) {
        for (int gx = 0; gx < bitmap->width; ++gx) {
            uint8_t alpha = bitmap->bitmap[static_cast<std::size_t>(gy * bitmap->width + gx)];
            if (alpha > 0) {
                put_pixel(target, glyph_x + gx, glyph_y + gy, alpha);
            }
        }
    }
}

} // namespace onyx_font
