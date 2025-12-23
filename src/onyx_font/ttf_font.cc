//
// Created by igor on 11/12/2025.
//

#include <onyx_font/ttf_font.hh>

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
#endif

#include <stb_truetype.h>

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

namespace onyx_font {

    struct ttf_font::impl {
        stbtt_fontinfo font_info{};
        std::span<const uint8_t> data;
        int index = 0;
        bool valid = false;

        impl(std::span<const uint8_t> font_data, int font_index)
            : data(font_data), index(font_index) {
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

    ttf_font::ttf_font(std::span<const uint8_t> data, int font_index)
        : m_impl(std::make_unique<impl>(data, font_index)) {
    }

    ttf_font::~ttf_font() = default;

    ttf_font::ttf_font(ttf_font&& other) noexcept = default;

    ttf_font& ttf_font::operator=(ttf_font&& other) noexcept = default;

    bool ttf_font::is_valid() const {
        return m_impl && m_impl->valid;
    }

    ttf_font_metrics ttf_font::get_metrics(float pixel_height) const {
        ttf_font_metrics metrics{};

        if (!is_valid()) {
            return metrics;
        }

        int ascent, descent, line_gap;
        stbtt_GetFontVMetrics(&m_impl->font_info, &ascent, &descent, &line_gap);

        float scale = m_impl->get_scale(pixel_height);
        metrics.ascent = static_cast<float>(ascent) * scale;
        metrics.descent = static_cast<float>(descent) * scale;
        metrics.line_gap = static_cast<float>(line_gap) * scale;

        return metrics;
    }

    std::optional<ttf_glyph_metrics> ttf_font::get_glyph_metrics(
        uint32_t codepoint,
        float pixel_height
    ) const {
        if (!is_valid()) {
            return std::nullopt;
        }

        int glyph_index = stbtt_FindGlyphIndex(&m_impl->font_info, static_cast<int>(codepoint));
        if (glyph_index == 0 && codepoint != 0) {
            return std::nullopt;
        }

        float scale = m_impl->get_scale(pixel_height);

        ttf_glyph_metrics result{};

        // Get bounding box (unscaled)
        int ix0, iy0, ix1, iy1;
        stbtt_GetGlyphBox(&m_impl->font_info, glyph_index, &ix0, &iy0, &ix1, &iy1);

        // Scale to pixels
        result.x0 = static_cast<float>(ix0) * scale;
        result.y0 = static_cast<float>(iy0) * scale;
        result.x1 = static_cast<float>(ix1) * scale;
        result.y1 = static_cast<float>(iy1) * scale;

        result.width = result.x1 - result.x0;
        result.height = result.y1 - result.y0;

        // Get horizontal metrics
        int advance_width, left_bearing;
        stbtt_GetGlyphHMetrics(&m_impl->font_info, glyph_index, &advance_width, &left_bearing);
        result.advance_x = static_cast<float>(advance_width) * scale;
        result.bearing_x = static_cast<float>(left_bearing) * scale;
        result.bearing_y = result.y1;  // Top of glyph relative to baseline

        return result;
    }

    std::optional<ttf_glyph_shape> ttf_font::get_glyph_shape(
        uint32_t codepoint,
        float pixel_height
    ) const {
        if (!is_valid()) {
            return std::nullopt;
        }

        int glyph_index = stbtt_FindGlyphIndex(&m_impl->font_info, static_cast<int>(codepoint));
        if (glyph_index == 0 && codepoint != 0) {
            return std::nullopt;
        }

        float scale = m_impl->get_scale(pixel_height);

        // Get glyph shape from stb_truetype
        stbtt_vertex* stb_vertices = nullptr;
        int num_vertices = stbtt_GetGlyphShape(&m_impl->font_info, glyph_index, &stb_vertices);

        ttf_glyph_shape result{};

        if (num_vertices > 0 && stb_vertices) {
            result.vertices.reserve(static_cast<size_t>(num_vertices));

            for (int i = 0; i < num_vertices; ++i) {
                const auto& sv = stb_vertices[i];
                ttf_vertex v{};

                // Scale coordinates
                v.x = static_cast<float>(sv.x) * scale;
                v.y = static_cast<float>(sv.y) * scale;
                v.cx = static_cast<float>(sv.cx) * scale;
                v.cy = static_cast<float>(sv.cy) * scale;
                v.cx1 = static_cast<float>(sv.cx1) * scale;
                v.cy1 = static_cast<float>(sv.cy1) * scale;

                // Map stb vertex type to our type
                switch (sv.type) {
                    case STBTT_vmove:
                        v.type = ttf_vertex_type::MOVE_TO;
                        break;
                    case STBTT_vline:
                        v.type = ttf_vertex_type::LINE_TO;
                        break;
                    case STBTT_vcurve:
                        v.type = ttf_vertex_type::CURVE_TO;
                        break;
                    case STBTT_vcubic:
                        v.type = ttf_vertex_type::CUBIC_TO;
                        break;
                    default:
                        continue;  // Skip unknown vertex types
                }

                result.vertices.push_back(v);
            }

            stbtt_FreeShape(&m_impl->font_info, stb_vertices);
        }

        // Get bounding box
        int ix0, iy0, ix1, iy1;
        stbtt_GetGlyphBox(&m_impl->font_info, glyph_index, &ix0, &iy0, &ix1, &iy1);
        result.x0 = static_cast<float>(ix0) * scale;
        result.y0 = static_cast<float>(iy0) * scale;
        result.x1 = static_cast<float>(ix1) * scale;
        result.y1 = static_cast<float>(iy1) * scale;

        // Get horizontal metrics
        int advance_width, left_bearing;
        stbtt_GetGlyphHMetrics(&m_impl->font_info, glyph_index, &advance_width, &left_bearing);
        result.advance_x = static_cast<float>(advance_width) * scale;
        result.bearing_x = static_cast<float>(left_bearing) * scale;
        result.bearing_y = result.y1;

        return result;
    }

    float ttf_font::get_kerning(
        uint32_t first,
        uint32_t second,
        float pixel_height
    ) const {
        if (!is_valid()) {
            return 0.0f;
        }

        float scale = m_impl->get_scale(pixel_height);
        int kern = stbtt_GetCodepointKernAdvance(
            &m_impl->font_info,
            static_cast<int>(first),
            static_cast<int>(second)
        );

        return static_cast<float>(kern) * scale;
    }

    bool ttf_font::has_glyph(uint32_t codepoint) const {
        if (!is_valid()) {
            return false;
        }

        int glyph_index = stbtt_FindGlyphIndex(&m_impl->font_info, static_cast<int>(codepoint));
        return glyph_index != 0 || codepoint == 0;
    }

    int ttf_font::get_font_count(std::span<const uint8_t> data) {
        if (data.empty()) {
            return 0;
        }

        return stbtt_GetNumberOfFonts(data.data());
    }

    std::span<const uint8_t> ttf_font::data() const {
        if (!m_impl) {
            return {};
        }
        return m_impl->data;
    }

    int ttf_font::font_index() const {
        if (!m_impl) {
            return 0;
        }
        return m_impl->index;
    }

}  // namespace onyx_font
