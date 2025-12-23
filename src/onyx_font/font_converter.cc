//
// Created by igor on 22/12/2025.
//
// Font conversion implementation
//

#include <onyx_font/font_converter.hh>
#include <../../include/onyx_font/utils/stb_truetype_font.hh>
#include <onyx_font/text/glyph_rasterizer.hh>
#include <onyx_font/text/raster_target.hh>
#include <cmath>
#include <algorithm>

namespace onyx_font {

namespace {

/// Calculate bounding box of a vector glyph when rendered
struct glyph_bounds {
    int min_x = 0;
    int max_x = 0;
    int min_y = 0;
    int max_y = 0;
    int width() const { return max_x - min_x; }
    int height() const { return max_y - min_y; }
};

glyph_bounds calculate_vector_glyph_bounds(const vector_glyph& glyph, float scale) {
    glyph_bounds bounds;
    float pen_x = 0, pen_y = 0;
    bool first = true;

    for (const auto& cmd : glyph.strokes) {
        float dx = static_cast<float>(cmd.dx) * scale;
        float dy = static_cast<float>(cmd.dy) * scale;

        if (cmd.type == stroke_type::MOVE_TO || cmd.type == stroke_type::LINE_TO) {
            pen_x += dx;
            pen_y += dy;

            int ix = static_cast<int>(std::floor(pen_x));
            int iy = static_cast<int>(std::floor(pen_y));

            if (first) {
                bounds.min_x = bounds.max_x = ix;
                bounds.min_y = bounds.max_y = iy;
                first = false;
            } else {
                bounds.min_x = std::min(bounds.min_x, ix);
                bounds.max_x = std::max(bounds.max_x, ix);
                bounds.min_y = std::min(bounds.min_y, iy);
                bounds.max_y = std::max(bounds.max_y, iy);
            }
        }
    }

    // Add padding for antialiased rendering
    bounds.min_x -= 1;
    bounds.min_y -= 1;
    bounds.max_x += 2;
    bounds.max_y += 2;

    return bounds;
}

} // anonymous namespace

bitmap_font font_converter::from_vector(
    const vector_font& font,
    float pixel_height,
    const conversion_options& options)
{
    bitmap_font result;

    // Get source font info
    const auto& src_metrics = font.get_metrics();
    uint8_t first_char = options.first_char;
    uint8_t last_char = options.last_char;

    // Clamp to font's actual range
    if (first_char < font.get_first_char()) {
        first_char = font.get_first_char();
    }
    if (last_char > font.get_last_char()) {
        last_char = font.get_last_char();
    }

    if (first_char > last_char) {
        return result; // Empty font
    }

    // Calculate scale factor
    float scale = pixel_height / static_cast<float>(src_metrics.pixel_height);

    // Calculate scaled metrics
    result.m_name = font.get_name() + " (bitmap)";
    result.m_first_char = first_char;
    result.m_last_char = last_char;
    result.m_default_char = font.get_default_char();
    result.m_break_char = ' ';

    // Convert font metrics
    result.m_metrics.pixel_height = static_cast<uint16_t>(std::ceil(pixel_height));
    result.m_metrics.ascent = static_cast<uint16_t>(std::ceil(
        static_cast<float>(src_metrics.ascent) * scale));
    result.m_metrics.internal_leading = 0;
    result.m_metrics.external_leading = 0;
    result.m_metrics.avg_width = static_cast<uint16_t>(std::ceil(
        static_cast<float>(src_metrics.avg_width) * scale));
    result.m_metrics.max_width = static_cast<uint16_t>(std::ceil(
        static_cast<float>(src_metrics.max_width) * scale));

    // Prepare spacing and storage
    size_t char_count = static_cast<size_t>(last_char - first_char + 1);
    result.m_spacing.resize(char_count);

    bitmap_builder builder(bit_order::msb_first);
    builder.reserve_glyphs(char_count);

    // Select raster mode
    auto raster_mode = options.antialias
        ? internal::raster_mode::antialiased
        : internal::raster_mode::aliased;

    // Convert each glyph
    for (uint8_t ch = first_char; ch <= last_char; ++ch) {
        size_t idx = ch - first_char;
        const vector_glyph* glyph = font.get_glyph(ch);

        if (!glyph || glyph->strokes.empty()) {
            // Empty glyph - create 1x1 placeholder
            (void)builder.reserve_glyph(1, 1);
            result.m_spacing[idx].b_space = std::uint16_t{1};
            continue;
        }

        // Calculate scaled advance
        uint16_t advance = static_cast<uint16_t>(std::ceil(
            static_cast<float>(glyph->width) * scale));

        // Calculate glyph bounds
        auto bounds = calculate_vector_glyph_bounds(*glyph, scale);

        // Ensure positive dimensions
        int glyph_w = std::max(1, bounds.width());
        int glyph_h = std::max(1, bounds.height());

        // Rasterize to grayscale buffer
        std::vector<uint8_t> buffer(static_cast<size_t>(glyph_w * glyph_h), 0);
        grayscale_max_target target(buffer.data(), glyph_w, glyph_h);

        // Render glyph at offset position
        internal::rasterize_vector_glyph(*glyph, scale, target,
                                         -bounds.min_x, -bounds.min_y, raster_mode);

        // Convert grayscale to 1-bit using threshold
        auto writer = builder.reserve_glyph(static_cast<uint16_t>(glyph_w),
                                            static_cast<uint16_t>(glyph_h));

        for (int y = 0; y < glyph_h; ++y) {
            for (int x = 0; x < glyph_w; ++x) {
                if (buffer[static_cast<size_t>(y * glyph_w + x)] >= options.threshold) {
                    writer.set_pixel(static_cast<uint16_t>(x),
                                    static_cast<uint16_t>(y), true);
                }
            }
        }

        // Set spacing info
        result.m_spacing[idx].a_space = static_cast<int16_t>(bounds.min_x);
        result.m_spacing[idx].b_space = static_cast<uint16_t>(glyph_w);
        result.m_spacing[idx].c_space = static_cast<int16_t>(
            advance - glyph_w - bounds.min_x);
    }

    result.m_storage = std::move(builder).build();
    return result;
}

bitmap_font font_converter::from_ttf(
    const ttf_font& font,
    float pixel_height,
    const conversion_options& options)
{
    bitmap_font result;

    if (!font.is_valid()) {
        return result;
    }

    // Create rasterizer from font data
    stb_truetype_font rasterizer(font.data(), font.font_index());
    if (!rasterizer.is_valid()) {
        return result;
    }

    uint8_t first_char = options.first_char;
    uint8_t last_char = options.last_char;

    // Find actual character range in font
    while (first_char < last_char && !font.has_glyph(first_char)) {
        ++first_char;
    }
    while (last_char > first_char && !font.has_glyph(last_char)) {
        --last_char;
    }

    if (first_char > last_char) {
        return result; // No glyphs found
    }

    // Get font metrics
    auto metrics = font.get_metrics(pixel_height);

    // Set up result font
    result.m_name = "TTF Font (bitmap)";
    result.m_first_char = first_char;
    result.m_last_char = last_char;
    result.m_default_char = '?'; // TTF fonts typically use '?' as default
    result.m_break_char = ' ';

    // Convert metrics
    result.m_metrics.pixel_height = static_cast<uint16_t>(std::ceil(pixel_height));
    result.m_metrics.ascent = static_cast<uint16_t>(std::ceil(metrics.ascent));
    result.m_metrics.internal_leading = 0;
    result.m_metrics.external_leading = static_cast<uint16_t>(
        std::max(0.0f, std::ceil(metrics.line_gap)));

    // Calculate average and max width by sampling common characters
    float total_width = 0;
    float max_width = 0;
    int sample_count = 0;
    for (char c = 'a'; c <= 'z'; ++c) {
        if (auto gm = font.get_glyph_metrics(static_cast<uint32_t>(c), pixel_height)) {
            total_width += gm->advance_x;
            max_width = std::max(max_width, gm->advance_x);
            ++sample_count;
        }
    }
    if (sample_count > 0) {
        result.m_metrics.avg_width = static_cast<uint16_t>(std::ceil(total_width / static_cast<float>(sample_count)));
    }
    result.m_metrics.max_width = static_cast<uint16_t>(std::ceil(max_width));

    // Prepare spacing and storage
    size_t char_count = static_cast<size_t>(last_char - first_char + 1);
    result.m_spacing.resize(char_count);

    bitmap_builder builder(bit_order::msb_first);
    builder.reserve_glyphs(char_count);

    // Convert each glyph
    for (uint8_t ch = first_char; ch <= last_char; ++ch) {
        size_t idx = ch - first_char;

        // Try to rasterize the glyph
        auto bitmap = rasterizer.rasterize(static_cast<uint32_t>(ch), pixel_height);

        if (!bitmap || bitmap->width == 0 || bitmap->height == 0) {
            // Empty glyph - create 1x1 placeholder
            (void)builder.reserve_glyph(1, 1);

            // Get advance if available
            if (auto gm = font.get_glyph_metrics(static_cast<uint32_t>(ch), pixel_height)) {
                result.m_spacing[idx].b_space = static_cast<uint16_t>(std::ceil(gm->advance_x));
            } else {
                result.m_spacing[idx].b_space = std::uint16_t{1};
            }
            continue;
        }

        int glyph_w = bitmap->width;
        int glyph_h = bitmap->height;

        // Convert grayscale to 1-bit using threshold
        auto writer = builder.reserve_glyph(static_cast<uint16_t>(glyph_w),
                                            static_cast<uint16_t>(glyph_h));

        for (int y = 0; y < glyph_h; ++y) {
            for (int x = 0; x < glyph_w; ++x) {
                if (bitmap->bitmap[static_cast<size_t>(y * glyph_w + x)] >= options.threshold) {
                    writer.set_pixel(static_cast<uint16_t>(x),
                                    static_cast<uint16_t>(y), true);
                }
            }
        }

        // Set spacing info based on STB offsets
        result.m_spacing[idx].a_space = static_cast<int16_t>(bitmap->offset_x);
        result.m_spacing[idx].b_space = static_cast<uint16_t>(glyph_w);

        // Calculate c_space from advance
        int c_space_int = static_cast<int>(std::ceil(bitmap->advance_x))
                        - bitmap->offset_x
                        - glyph_w;
        result.m_spacing[idx].c_space = static_cast<int16_t>(c_space_int);
    }

    result.m_storage = std::move(builder).build();
    return result;
}

} // namespace onyx_font
