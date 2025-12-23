//
// Created by igor on 12/21/2025.
//

#include <../../../include/onyx_font/utils/stb_truetype_font.hh>

// Disable warnings for stb_truetype (third-party header)
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wold-style-cast"
#pragma GCC diagnostic ignored "-Wdouble-promotion"
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic ignored "-Wuseless-cast"
#endif
#ifdef __clang__
#pragma clang diagnostic ignored "-Wdeprecated-anon-enum-enum-conversion"
#endif
#endif

#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

namespace onyx_font {

    struct stb_truetype_font::impl {
        stbtt_fontinfo font_info{};
        std::span<const uint8_t> data;
        bool valid = false;

        impl(std::span<const uint8_t> font_data, int font_index)
            : data(font_data) {
            if (data.empty()) {
                return;
            }

            int offset = stbtt_GetFontOffsetForIndex(
                data.data(),
                font_index
            );

            if (offset < 0) {
                return;
            }

            valid = stbtt_InitFont(
                &font_info,
                data.data(),
                offset
            ) != 0;
        }

        [[nodiscard]] float get_scale(float pixel_height) const {
            return stbtt_ScaleForPixelHeight(&font_info, pixel_height);
        }
    };

    stb_truetype_font::stb_truetype_font(std::span<const uint8_t> data, int font_index)
        : m_impl(std::make_unique<impl>(data, font_index)) {
    }

    stb_truetype_font::~stb_truetype_font() = default;

    stb_truetype_font::stb_truetype_font(stb_truetype_font&& other) noexcept = default;

    stb_truetype_font& stb_truetype_font::operator=(stb_truetype_font&& other) noexcept = default;

    bool stb_truetype_font::is_valid() const {
        return m_impl && m_impl->valid;
    }

    std::optional<stb_glyph_bitmap> stb_truetype_font::rasterize(
        uint32_t codepoint,
        float pixel_height
    ) const {
        return rasterize_subpixel(codepoint, pixel_height, 0.0f, 0.0f);
    }

    std::optional<stb_glyph_bitmap> stb_truetype_font::rasterize_subpixel(
        uint32_t codepoint,
        float pixel_height,
        float shift_x,
        float shift_y
    ) const {
        if (!is_valid()) {
            return std::nullopt;
        }

        int glyph_index = stbtt_FindGlyphIndex(&m_impl->font_info, static_cast<int>(codepoint));
        if (glyph_index == 0 && codepoint != 0) {
            return std::nullopt;
        }

        float scale = m_impl->get_scale(pixel_height);

        int width, height, x_off, y_off;
        unsigned char* bitmap = stbtt_GetGlyphBitmapSubpixel(
            &m_impl->font_info,
            scale, scale,
            shift_x, shift_y,
            glyph_index,
            &width, &height,
            &x_off, &y_off
        );

        stb_glyph_bitmap result;
        result.width = width;
        result.height = height;
        result.offset_x = x_off;
        result.offset_y = y_off;

        if (bitmap && width > 0 && height > 0) {
            // Cast to size_t before multiplication to prevent integer overflow
            std::size_t size = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
            result.bitmap.assign(bitmap, bitmap + size);
            stbtt_FreeBitmap(bitmap, nullptr);
        }

        // Get advance width
        int advance_width, left_bearing;
        stbtt_GetGlyphHMetrics(&m_impl->font_info, glyph_index, &advance_width, &left_bearing);
        result.advance_x = static_cast<float>(advance_width) * scale;

        return result;
    }

    float stb_truetype_font::get_scale_for_pixel_height(float pixel_height) const {
        if (!is_valid()) {
            return 0.0f;
        }
        return m_impl->get_scale(pixel_height);
    }

    int stb_truetype_font::get_font_count(std::span<const uint8_t> data) {
        if (data.empty()) {
            return 0;
        }
        return stbtt_GetNumberOfFonts(data.data());
    }

}  // namespace onyx_font
