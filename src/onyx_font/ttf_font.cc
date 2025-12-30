//
// Created by igor on 11/12/2025.
//

#include <onyx_font/ttf_font.hh>
#include "utils/freetype_library.hh"

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_OUTLINE_H
#include FT_BBOX_H

namespace onyx_font {

    struct ttf_font::impl {
        FT_Face face = nullptr;
        std::span<const uint8_t> data;
        int index = 0;
        bool valid = false;

        impl(std::span<const uint8_t> font_data, int font_index)
            : data(font_data), index(font_index) {
            if (data.empty()) {
                return;
            }

            FT_Library lib = detail::freetype_library::get();
            if (!lib) {
                return;
            }

            FT_Error error = FT_New_Memory_Face(
                lib,
                data.data(),
                static_cast<FT_Long>(data.size()),
                font_index,
                &face
            );

            valid = (error == 0 && face != nullptr);
        }

        ~impl() {
            if (face) {
                FT_Done_Face(face);
            }
        }

        impl(const impl&) = delete;
        impl& operator=(const impl&) = delete;

        [[nodiscard]] float get_scale(float pixel_height) const {
            if (!face || face->units_per_EM == 0) {
                return 1.0f;
            }
            return pixel_height / static_cast<float>(face->units_per_EM);
        }

        // Set pixel size for glyph loading
        void set_pixel_size(float pixel_height) const {
            if (face) {
                FT_Set_Pixel_Sizes(face, 0, static_cast<FT_UInt>(pixel_height));
            }
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

        float scale = m_impl->get_scale(pixel_height);

        // FreeType stores metrics in font units
        metrics.ascent = static_cast<float>(m_impl->face->ascender) * scale;
        metrics.descent = static_cast<float>(m_impl->face->descender) * scale;

        // Line gap = height - (ascender - descender)
        float height = static_cast<float>(m_impl->face->height) * scale;
        metrics.line_gap = height - (metrics.ascent - metrics.descent);

        return metrics;
    }

    std::optional<ttf_glyph_metrics> ttf_font::get_glyph_metrics(
        uint32_t codepoint,
        float pixel_height
    ) const {
        if (!is_valid()) {
            return std::nullopt;
        }

        FT_UInt glyph_index = FT_Get_Char_Index(m_impl->face, codepoint);
        if (glyph_index == 0 && codepoint != 0) {
            return std::nullopt;
        }

        // Set size and load glyph
        m_impl->set_pixel_size(pixel_height);

        FT_Error error = FT_Load_Glyph(m_impl->face, glyph_index, FT_LOAD_DEFAULT);
        if (error) {
            return std::nullopt;
        }

        FT_GlyphSlot slot = m_impl->face->glyph;
        const FT_Glyph_Metrics& m = slot->metrics;

        ttf_glyph_metrics result{};

        // FreeType metrics are in 26.6 fixed point (divide by 64)
        result.width = static_cast<float>(m.width) / 64.0f;
        result.height = static_cast<float>(m.height) / 64.0f;
        result.bearing_x = static_cast<float>(m.horiBearingX) / 64.0f;
        result.bearing_y = static_cast<float>(m.horiBearingY) / 64.0f;
        result.advance_x = static_cast<float>(m.horiAdvance) / 64.0f;

        // Bounding box coordinates
        result.x0 = result.bearing_x;
        result.y0 = result.bearing_y - result.height;
        result.x1 = result.bearing_x + result.width;
        result.y1 = result.bearing_y;

        return result;
    }

    std::optional<ttf_glyph_shape> ttf_font::get_glyph_shape(
        uint32_t codepoint,
        float pixel_height
    ) const {
        if (!is_valid()) {
            return std::nullopt;
        }

        FT_UInt glyph_index = FT_Get_Char_Index(m_impl->face, codepoint);
        if (glyph_index == 0 && codepoint != 0) {
            return std::nullopt;
        }

        // Load glyph without hinting to get clean outlines
        // Use font units (not scaled) for outline data
        FT_Error error = FT_Load_Glyph(m_impl->face, glyph_index, FT_LOAD_NO_SCALE);
        if (error) {
            return std::nullopt;
        }

        FT_GlyphSlot slot = m_impl->face->glyph;

        if (slot->format != FT_GLYPH_FORMAT_OUTLINE) {
            return std::nullopt;  // Not an outline glyph
        }

        float scale = m_impl->get_scale(pixel_height);
        ttf_glyph_shape result{};

        FT_Outline& outline = slot->outline;

        // Process outline contours
        int point_idx = 0;
        for (int contour = 0; contour < outline.n_contours; ++contour) {
            int contour_end = outline.contours[contour];
            int contour_start = point_idx;
            bool first_point = true;

            while (point_idx <= contour_end) {
                FT_Vector& pt = outline.points[point_idx];
                char tag = FT_CURVE_TAG(outline.tags[point_idx]);

                float x = static_cast<float>(pt.x) * scale;
                float y = static_cast<float>(pt.y) * scale;

                if (first_point) {
                    // Start new contour with MOVE_TO
                    ttf_vertex v{};
                    v.type = ttf_vertex_type::MOVE_TO;
                    v.x = x;
                    v.y = y;
                    result.vertices.push_back(v);
                    first_point = false;
                    ++point_idx;
                    continue;
                }

                if (tag == FT_CURVE_TAG_ON) {
                    // On-curve point - LINE_TO
                    ttf_vertex v{};
                    v.type = ttf_vertex_type::LINE_TO;
                    v.x = x;
                    v.y = y;
                    result.vertices.push_back(v);
                } else if (tag == FT_CURVE_TAG_CONIC) {
                    // Quadratic bezier control point
                    // Need to find the next on-curve point
                    int next_idx = (point_idx < contour_end) ? point_idx + 1 : contour_start;
                    FT_Vector& next_pt = outline.points[next_idx];
                    char next_tag = FT_CURVE_TAG(outline.tags[next_idx]);

                    float cx = x;
                    float cy = y;
                    float ex, ey;

                    if (next_tag == FT_CURVE_TAG_ON) {
                        // Next point is on-curve - use it as endpoint
                        ex = static_cast<float>(next_pt.x) * scale;
                        ey = static_cast<float>(next_pt.y) * scale;
                        ++point_idx;  // Skip the endpoint, we've consumed it
                    } else {
                        // Next point is also off-curve - implied on-curve point at midpoint
                        ex = (x + static_cast<float>(next_pt.x) * scale) / 2.0f;
                        ey = (y + static_cast<float>(next_pt.y) * scale) / 2.0f;
                    }

                    ttf_vertex v{};
                    v.type = ttf_vertex_type::CURVE_TO;
                    v.x = ex;
                    v.y = ey;
                    v.cx = cx;
                    v.cy = cy;
                    result.vertices.push_back(v);
                } else if (tag == FT_CURVE_TAG_CUBIC) {
                    // Cubic bezier - need two control points
                    if (point_idx + 2 <= contour_end) {
                        FT_Vector& cp2 = outline.points[point_idx + 1];
                        FT_Vector& end_pt = outline.points[point_idx + 2];

                        ttf_vertex v{};
                        v.type = ttf_vertex_type::CUBIC_TO;
                        v.cx = x;
                        v.cy = y;
                        v.cx1 = static_cast<float>(cp2.x) * scale;
                        v.cy1 = static_cast<float>(cp2.y) * scale;
                        v.x = static_cast<float>(end_pt.x) * scale;
                        v.y = static_cast<float>(end_pt.y) * scale;
                        result.vertices.push_back(v);
                        point_idx += 2;  // Skip the extra points
                    }
                }

                ++point_idx;
            }
        }

        // Get bounding box from outline
        FT_BBox bbox;
        FT_Outline_Get_BBox(&outline, &bbox);
        result.x0 = static_cast<float>(bbox.xMin) * scale;
        result.y0 = static_cast<float>(bbox.yMin) * scale;
        result.x1 = static_cast<float>(bbox.xMax) * scale;
        result.y1 = static_cast<float>(bbox.yMax) * scale;

        // Get horizontal metrics (in font units since we used NO_SCALE)
        result.advance_x = static_cast<float>(slot->metrics.horiAdvance) * scale;
        result.bearing_x = static_cast<float>(slot->metrics.horiBearingX) * scale;
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

        // Check if font has kerning
        if (!FT_HAS_KERNING(m_impl->face)) {
            return 0.0f;
        }

        FT_UInt glyph1 = FT_Get_Char_Index(m_impl->face, first);
        FT_UInt glyph2 = FT_Get_Char_Index(m_impl->face, second);

        FT_Vector kerning;
        FT_Error error = FT_Get_Kerning(
            m_impl->face,
            glyph1,
            glyph2,
            FT_KERNING_UNSCALED,
            &kerning
        );

        if (error) {
            return 0.0f;
        }

        float scale = m_impl->get_scale(pixel_height);
        return static_cast<float>(kerning.x) * scale;
    }

    bool ttf_font::has_glyph(uint32_t codepoint) const {
        if (!is_valid()) {
            return false;
        }

        FT_UInt glyph_index = FT_Get_Char_Index(m_impl->face, codepoint);
        return glyph_index != 0 || codepoint == 0;
    }

    int ttf_font::get_font_count(std::span<const uint8_t> data) {
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
            // If error, try loading face 0 to check if it's a valid single font
            error = FT_New_Memory_Face(lib, data.data(),
                                       static_cast<FT_Long>(data.size()), 0, &face);
            if (error) {
                return 0;
            }
            FT_Long count = face->num_faces;
            FT_Done_Face(face);
            return static_cast<int>(count);
        }

        // When index is -1, num_faces contains the count
        FT_Long count = face->num_faces;
        FT_Done_Face(face);
        return static_cast<int>(count);
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
