//
// Created by igor on 30/12/2025.
//

#include <onyx_font/utils/freetype_font.hh>
#include "freetype_library.hh"

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H
#include FT_SYNTHESIS_H

#include <cmath>
#include <algorithm>

namespace onyx_font {

    namespace {
        // FT_LOAD_TARGET_MONO, without the macro's cast (a useless cast here)
        constexpr FT_Int32 load_target_mono = (static_cast<FT_Int32>(FT_RENDER_MODE_MONO) & 15) << 16;
    }

    struct freetype_font::impl {
        FT_Face face = nullptr;
        std::vector<uint8_t> data_copy;  // Keep a copy of font data
        bool valid = false;
        bool monochrome = false;         // hinted for and rendered to 1 bit (set_monochrome)

        impl(std::span<const uint8_t> data, int font_index) {
            if (data.empty()) {
                return;
            }

            // FreeType requires data to persist, so make a copy
            data_copy.assign(data.begin(), data.end());

            FT_Library lib = detail::freetype_library::get();
            if (!lib) {
                return;
            }

            FT_Error error = FT_New_Memory_Face(
                lib,
                data_copy.data(),
                static_cast<FT_Long>(data_copy.size()),
                font_index,
                &face
            );

            valid = (error == 0 && face != nullptr);
            if (valid && face->charmap == nullptr) {
                // A symbol font (Marlett, Wingdings) has only a Microsoft Symbol charmap, which FreeType
                // doesn't select by itself
                (void)FT_Select_Charmap(face, FT_ENCODING_MS_SYMBOL);
            }
        }

        ~impl() {
            if (face) {
                FT_Done_Face(face);
            }
        }

        void set_pixel_size(float pixel_height) const {
            if (face) {
                FT_Set_Pixel_Sizes(face, 0, static_cast<FT_UInt>(std::round(pixel_height)));
            }
        }
    };

    freetype_font::freetype_font(std::span<const uint8_t> data, int font_index)
        : m_impl(std::make_unique<impl>(data, font_index)) {
    }

    freetype_font::~freetype_font() = default;

    freetype_font::freetype_font(freetype_font&& other) noexcept = default;

    freetype_font& freetype_font::operator=(freetype_font&& other) noexcept = default;

    bool freetype_font::is_valid() const {
        return m_impl && m_impl->valid;
    }

    std::optional<ft_glyph_bitmap> freetype_font::rasterize(
        uint32_t codepoint,
        float pixel_height
    ) const {
        return rasterize_styled(codepoint, pixel_height, {});
    }

    std::optional<ft_glyph_bitmap> freetype_font::rasterize_styled(
        uint32_t codepoint,
        float pixel_height,
        const ft_render_style& style
    ) const {
        if (!is_valid()) {
            return std::nullopt;
        }

        m_impl->set_pixel_size(pixel_height);

        // Set up transform for italic
        FT_Matrix matrix;
        FT_Vector pen;
        pen.x = 0;
        pen.y = 0;

        if (style.italic) {
            // Skew matrix for oblique effect
            matrix.xx = 0x10000L;  // 1.0 in 16.16 fixed point
            matrix.xy = static_cast<FT_Fixed>(style.italic_skew * 0x10000L);
            matrix.yx = 0;
            matrix.yy = 0x10000L;
            FT_Set_Transform(m_impl->face, &matrix, &pen);
        } else {
            FT_Set_Transform(m_impl->face, nullptr, nullptr);
        }

        // Load glyph
        FT_UInt glyph_index = detail::char_index(m_impl->face, codepoint);
        if (glyph_index == 0 && codepoint != 0) {
            return std::nullopt;
        }

        FT_Error error = FT_Load_Glyph(m_impl->face, glyph_index,
                                       m_impl->monochrome ? load_target_mono : FT_LOAD_DEFAULT);
        if (error) {
            return std::nullopt;
        }

        // Apply bold synthesis
        if (style.bold) {
            // Calculate bold strength based on size if not specified
            FT_Pos strength = style.bold_strength;
            if (strength <= 0) {
                // Auto-calculate: ~2.5% of em size is a good default
                strength = FT_MulFix(m_impl->face->units_per_EM,
                                     m_impl->face->size->metrics.y_scale) / 40;
            }
            FT_GlyphSlot_Embolden(m_impl->face->glyph);
        }

        // Render to bitmap
        error = FT_Render_Glyph(m_impl->face->glyph,
                                m_impl->monochrome ? FT_RENDER_MODE_MONO : FT_RENDER_MODE_NORMAL);
        if (error) {
            return std::nullopt;
        }

        FT_GlyphSlot slot = m_impl->face->glyph;
        FT_Bitmap& bitmap = slot->bitmap;

        ft_glyph_bitmap result;
        result.width = static_cast<int>(bitmap.width);
        result.height = static_cast<int>(bitmap.rows);
        result.offset_x = slot->bitmap_left;
        result.offset_y = -slot->bitmap_top;  // FreeType uses top-down, we use baseline-relative
        result.advance_x = static_cast<float>(slot->advance.x) / 64.0f;

        if (result.width > 0 && result.height > 0) {
            result.bitmap.resize(static_cast<std::size_t>(result.width) *
                                 static_cast<std::size_t>(result.height));

            // Copy bitmap data (a monochrome bitmap's bits, most significant first, become 0 or 255)
            for (int y = 0; y < result.height; ++y) {
                for (int x = 0; x < result.width; ++x) {
                    const unsigned char* row = bitmap.buffer + y * bitmap.pitch;
                    result.bitmap[static_cast<std::size_t>(y * result.width + x)] =
                        bitmap.pixel_mode == FT_PIXEL_MODE_MONO
                            ? static_cast<uint8_t>(((row[x / 8] >> (7 - x % 8)) & 1) != 0 ? 255 : 0)
                            : row[x];
                }
            }
        }

        return result;
    }

    void freetype_font::set_monochrome(bool on) {
        if (m_impl) {
            m_impl->monochrome = on;
        }
    }

    bool freetype_font::monochrome() const {
        return m_impl && m_impl->monochrome;
    }

    float freetype_font::get_scale_for_pixel_height(float pixel_height) const {
        if (!is_valid() || m_impl->face->units_per_EM == 0) {
            return 0.0f;
        }
        return pixel_height / static_cast<float>(m_impl->face->units_per_EM);
    }

    int freetype_font::get_ascender() const {
        if (!is_valid()) return 0;
        return m_impl->face->ascender;
    }

    int freetype_font::get_descender() const {
        if (!is_valid()) return 0;
        return m_impl->face->descender;
    }

    int freetype_font::get_height() const {
        if (!is_valid()) return 0;
        return m_impl->face->height;
    }

    int freetype_font::get_units_per_em() const {
        if (!is_valid()) return 0;
        return m_impl->face->units_per_EM;
    }

    float freetype_font::get_kerning(uint32_t left, uint32_t right, float pixel_height) const {
        if (!is_valid()) return 0.0f;

        if (!FT_HAS_KERNING(m_impl->face)) {
            return 0.0f;
        }

        m_impl->set_pixel_size(pixel_height);

        FT_UInt left_index = detail::char_index(m_impl->face, left);
        FT_UInt right_index = detail::char_index(m_impl->face, right);

        FT_Vector kerning;
        FT_Error error = FT_Get_Kerning(
            m_impl->face,
            left_index,
            right_index,
            FT_KERNING_DEFAULT,
            &kerning
        );

        if (error) {
            return 0.0f;
        }

        return static_cast<float>(kerning.x) / 64.0f;
    }

    int freetype_font::get_font_count(std::span<const uint8_t> data) {
        if (data.empty()) {
            return 0;
        }

        FT_Library lib = detail::freetype_library::get();
        if (!lib) {
            return 0;
        }

        FT_Face face;
        FT_Error error = FT_New_Memory_Face(
            lib,
            data.data(),
            static_cast<FT_Long>(data.size()),
            -1,  // Request face count
            &face
        );

        if (error) {
            return 0;
        }

        int count = static_cast<int>(face->num_faces);
        FT_Done_Face(face);
        return count;
    }

} // namespace onyx_font
