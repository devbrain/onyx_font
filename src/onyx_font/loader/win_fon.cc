//
// Created by igor on 12/12/2025.
//

#include "loaders.hh"
#include <failsafe/failsafe.hh>

namespace onyx_font::internal {
    static void draw(const libexe::font_data& fd, bitmap_builder& bb, const libexe::glyph_entry& gi) {
        auto w = gi.width;
        auto h = fd.pixel_height;

        auto writer = bb.reserve_glyph(w, h);

        for (uint16_t y = 0; y < h; y++) {
            for (uint16_t x = 0; x < w; x++) {
                // Which byte-column does pixel x fall into?
                std::size_t byte_col = x / 8;
                // Within that byte-column, which bit? (MSB = leftmost)
                uint8_t bit_pos = 7 - (x % 8);

                // The byte for (row y, byte-column bc) is at: offset + bc * height + y
                std::size_t byte_idx = gi.offset + byte_col * h + y;

                if (byte_idx < fd.bitmap_data.size()) {
                    uint8_t byte_val = fd.bitmap_data[byte_idx];
                    if ((byte_val & (1 << bit_pos)) != 0) {
                        writer.set_pixel(x, y, true);
                    } else {
                        writer.set_pixel(x, y, false);
                    }
                } else {
                }
            }
        }
    }

    bitmap_font win_bitmap_fon_loader::load(const libexe::font_data& fd) {
        if (fd.type != libexe::font_type::RASTER) {
            THROW_INVALID_ARG(fd.face_name, " is not a raster font");
        }
        bitmap_font result;
        result.m_name = fd.face_name;
        result.m_break_char = fd.break_char;
        result.m_default_char = fd.default_char;
        result.m_first_char = fd.first_char;
        result.m_last_char = fd.last_char;

        // Populate font-level metrics
        result.m_metrics.ascent = fd.ascent;
        result.m_metrics.pixel_height = fd.pixel_height;
        result.m_metrics.internal_leading = fd.internal_leading;
        result.m_metrics.external_leading = fd.external_leading;
        result.m_metrics.avg_width = fd.avg_width;
        result.m_metrics.max_width = fd.max_width;

        bitmap_builder bb;

        // Reserve space for glyph spacing
        result.m_spacing.reserve(fd.glyphs.size());

        for (const auto& g : fd.glyphs) {
            draw(fd, bb, g);

            // Populate per-glyph spacing (ABC spacing)
            glyph_spacing spacing;
            spacing.a_space = g.a_space;
            spacing.b_space = g.b_space;
            spacing.c_space = g.c_space;
            result.m_spacing.push_back(spacing);
        }

        result.m_storage = std::move(bb).build();

        return result;
    }
}
