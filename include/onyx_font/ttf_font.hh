/**
 * @file ttf_font.hh
 * @brief TrueType/OpenType font access and outline data.
 *
 * This file provides the ttf_font class for working with TrueType (.ttf),
 * OpenType (.otf), and TrueType Collection (.ttc) fonts. These fonts use
 * bezier curves to define scalable glyph outlines.
 *
 * @section ttf_overview Overview
 *
 * Unlike bitmap_font (pixels) or vector_font (strokes), TTF fonts store
 * glyph shapes as bezier curves. This provides:
 * - Perfect scaling to any size
 * - High-quality rendering with hinting
 * - Support for complex scripts and large character sets
 *
 * For rasterization (converting outlines to pixels), use:
 * - stb_truetype_font for direct bitmap rendering
 * - font_converter to create a bitmap_font at a specific size
 *
 * @section ttf_coords Coordinate System
 *
 * - Font units are scaled to pixels using get_metrics(pixel_height)
 * - Y increases upward in font coordinates (baseline at y=0)
 * - Ascending glyphs have positive Y, descending have negative Y
 *
 * @section ttf_outlines Outline Data
 *
 * Glyph shapes are defined as a series of vertices:
 * - **MOVE_TO**: Start a new contour at (x, y)
 * - **LINE_TO**: Draw a straight line to (x, y)
 * - **CURVE_TO**: Quadratic bezier to (x, y) with control point (cx, cy)
 * - **CUBIC_TO**: Cubic bezier to (x, y) with control points (cx, cy), (cx1, cy1)
 *
 * @section ttf_usage Usage Example
 *
 * @code{.cpp}
 * // Load font
 * auto font_data = read_file("arial.ttf");
 * ttf_font font(font_data);
 *
 * if (!font.is_valid()) {
 *     // Handle error
 *     return;
 * }
 *
 * // Get metrics for 24px size
 * auto metrics = font.get_metrics(24.0f);
 * float line_height = metrics.ascent - metrics.descent + metrics.line_gap;
 *
 * // Get glyph outline for path rendering
 * auto shape = font.get_glyph_shape('A', 24.0f);
 * if (shape) {
 *     for (const auto& v : shape->vertices) {
 *         switch (v.type) {
 *             case ttf_vertex_type::MOVE_TO:
 *                 path.move_to(v.x, v.y);
 *                 break;
 *             case ttf_vertex_type::LINE_TO:
 *                 path.line_to(v.x, v.y);
 *                 break;
 *             case ttf_vertex_type::CURVE_TO:
 *                 path.quad_to(v.cx, v.cy, v.x, v.y);
 *                 break;
 *             case ttf_vertex_type::CUBIC_TO:
 *                 path.cubic_to(v.cx, v.cy, v.cx1, v.cy1, v.x, v.y);
 *                 break;
 *         }
 *     }
 * }
 * @endcode
 *
 * @section ttf_layout Text Layout Example
 *
 * @code{.cpp}
 * float pen_x = start_x;
 * float font_size = 24.0f;
 *
 * for (size_t i = 0; i < text.size(); ++i) {
 *     auto glyph_metrics = font.get_glyph_metrics(text[i], font_size);
 *     if (!glyph_metrics) continue;
 *
 *     // Render glyph at pen_x
 *     render_glyph(text[i], pen_x, pen_y);
 *
 *     // Advance pen position
 *     pen_x += glyph_metrics->advance_x;
 *
 *     // Apply kerning between adjacent characters
 *     if (i + 1 < text.size()) {
 *         pen_x += font.get_kerning(text[i], text[i + 1], font_size);
 *     }
 * }
 * @endcode
 *
 * @author Igor
 * @date 11/12/2025
 */

#pragma once

#include <onyx_font/export.h>
#include <string>
#include <cstdint>
#include <memory>
#include <vector>
#include <span>
#include <optional>

namespace onyx_font {
    /**
     * @brief Font-wide metrics scaled to a specific pixel size.
     *
     * These metrics describe the overall vertical dimensions of the font
     * and are used for line spacing calculations.
     */
    struct ONYX_FONT_EXPORT ttf_font_metrics {
        /**
         * @brief Distance from baseline to top of tallest glyph.
         *
         * Positive value. Characters like 'b', 'd', 'h' extend up to
         * approximately this height above the baseline.
         */
        float ascent;

        /**
         * @brief Distance from baseline to bottom of deepest descender.
         *
         * Negative value. Characters like 'g', 'p', 'y' extend down to
         * approximately this depth below the baseline.
         */
        float descent;

        /**
         * @brief Recommended extra spacing between lines.
         *
         * Add this to (ascent - descent) to get the recommended
         * line height for multi-line text.
         */
        float line_gap;
    };

    /**
     * @brief Per-glyph metrics for text layout.
     *
     * These metrics describe the dimensions and positioning of a
     * single glyph, used for calculating text layout.
     */
    struct ONYX_FONT_EXPORT ttf_glyph_metrics {
        /**
         * @brief Horizontal advance to next glyph origin.
         *
         * The distance to move the pen rightward after rendering
         * this glyph. This is the primary value for text layout.
         */
        float advance_x;

        /**
         * @brief Left side bearing.
         *
         * Distance from the current pen position to the left edge
         * of the glyph's bounding box. Can be negative for
         * overhanging glyphs.
         */
        float bearing_x;

        /**
         * @brief Top side bearing.
         *
         * Distance from the baseline to the top of the glyph's
         * bounding box. Positive for glyphs that extend above
         * the baseline.
         */
        float bearing_y;

        float width;   ///< Glyph bounding box width
        float height;  ///< Glyph bounding box height
        float x0;      ///< Bounding box left edge
        float y0;      ///< Bounding box bottom edge
        float x1;      ///< Bounding box right edge
        float y1;      ///< Bounding box top edge
    };

    /**
     * @brief Vertex type for glyph outline paths.
     *
     * These types correspond to common path drawing commands
     * and can be mapped directly to most graphics APIs.
     */
    enum class ttf_vertex_type : uint8_t {
        /**
         * @brief Start a new contour at (x, y).
         *
         * Moves the pen to a new position without drawing.
         * Every closed shape begins with a MOVE_TO.
         */
        MOVE_TO,

        /**
         * @brief Draw a straight line to (x, y).
         *
         * Draws from the current position to the specified point.
         */
        LINE_TO,

        /**
         * @brief Quadratic bezier curve to (x, y).
         *
         * Draws a quadratic bezier curve from the current position
         * to (x, y), using (cx, cy) as the control point.
         * TrueType fonts use quadratic beziers.
         */
        CURVE_TO,

        /**
         * @brief Cubic bezier curve to (x, y).
         *
         * Draws a cubic bezier curve from the current position
         * to (x, y), using (cx, cy) and (cx1, cy1) as control points.
         * OpenType CFF fonts may use cubic beziers.
         */
        CUBIC_TO
    };

    /**
     * @brief Single vertex in a glyph outline path.
     *
     * Represents one drawing command in the glyph's outline.
     * The meaning of the coordinate fields depends on the vertex type.
     */
    struct ONYX_FONT_EXPORT ttf_vertex {
        ttf_vertex_type type;  ///< Command type

        float x;   ///< End point X coordinate
        float y;   ///< End point Y coordinate

        /**
         * @brief First control point X (CURVE_TO and CUBIC_TO).
         *
         * For quadratic beziers (CURVE_TO), this is the single control point.
         * For cubic beziers (CUBIC_TO), this is the first control point.
         */
        float cx;

        /**
         * @brief First control point Y (CURVE_TO and CUBIC_TO).
         */
        float cy;

        /**
         * @brief Second control point X (CUBIC_TO only).
         */
        float cx1;

        /**
         * @brief Second control point Y (CUBIC_TO only).
         */
        float cy1;
    };

    /**
     * @brief Complete glyph outline shape with bezier curves.
     *
     * Contains all the path data needed to render a glyph at a
     * specific size, including metrics for positioning.
     */
    struct ONYX_FONT_EXPORT ttf_glyph_shape {
        /**
         * @brief Outline vertices defining the glyph shape.
         *
         * Execute these commands in order to draw the glyph.
         * Contours are implicitly closed (last point connects to
         * the point after the last MOVE_TO).
         */
        std::vector<ttf_vertex> vertices;

        float advance_x;  ///< Horizontal advance to next glyph
        float bearing_x;  ///< Left side bearing
        float bearing_y;  ///< Top side bearing

        float x0;  ///< Bounding box left
        float y0;  ///< Bounding box bottom
        float x1;  ///< Bounding box right
        float y1;  ///< Bounding box top
    };

    /**
     * @brief TrueType/OpenType font providing metrics and outline data.
     *
     * This class provides access to font metrics and glyph outlines from
     * TrueType (.ttf), OpenType (.otf), and TrueType Collection (.ttc) files.
     *
     * Key features:
     * - Scale-independent glyph shapes (bezier curves)
     * - Full Unicode support (codepoints up to U+10FFFF)
     * - Kerning support for improved text appearance
     * - TTC collection support (multiple fonts in one file)
     *
     * @note The font data passed to the constructor must remain valid
     *       for the entire lifetime of the ttf_font object.
     *
     * @section ttf_class_example Example
     *
     * @code{.cpp}
     * // Load and check font
     * auto data = read_file("font.ttf");
     * ttf_font font(data);
     *
     * if (font.is_valid()) {
     *     // Get metrics for desired size
     *     auto metrics = font.get_metrics(16.0f);
     *
     *     // Check for specific characters
     *     if (font.has_glyph(U''));  // Check for emoji
     *
     *     // Get outline for rendering
     *     if (auto shape = font.get_glyph_shape('A', 48.0f)) {
     *         // Use shape->vertices for path drawing
     *     }
     * }
     * @endcode
     *
     * @see font_factory For loading TTF fonts
     * @see font_converter For converting TTF to bitmap_font
     * @see stb_truetype_font For direct rasterization
     */
    class ONYX_FONT_EXPORT ttf_font {
    public:
        /**
         * @brief Construct from font file data.
         *
         * Initializes the font from TTF, OTF, or TTC file data.
         * The data must remain valid for the lifetime of this object.
         *
         * @param data Font file contents (must remain valid)
         * @param font_index Index within TTC collection (0 for regular TTF/OTF)
         *
         * @note Use is_valid() to check if loading succeeded.
         */
        explicit ttf_font(std::span<const uint8_t> data, int font_index = 0);

        /**
         * @brief Destructor.
         */
        ~ttf_font();

        /**
         * @brief Move constructor.
         */
        ttf_font(ttf_font&& other) noexcept;

        /**
         * @brief Move assignment operator.
         */
        ttf_font& operator=(ttf_font&& other) noexcept;

        /// @cond
        ttf_font(const ttf_font&) = delete;
        ttf_font& operator=(const ttf_font&) = delete;
        /// @endcond

        /**
         * @brief Check if font was loaded successfully.
         * @return true if font is valid and ready to use
         */
        [[nodiscard]] bool is_valid() const;

        /**
         * @brief Get font metrics scaled to a pixel height.
         *
         * Returns the font's vertical metrics (ascent, descent, line gap)
         * scaled to the specified pixel height.
         *
         * @param pixel_height Desired font size in pixels
         * @return Scaled font metrics
         */
        [[nodiscard]] ttf_font_metrics get_metrics(float pixel_height) const;

        /**
         * @brief Get glyph metrics for text layout.
         *
         * Returns positioning and sizing information for a single glyph.
         * Use advance_x for basic text layout, and bearing values for
         * precise glyph positioning.
         *
         * @param codepoint Unicode codepoint (e.g., 'A' or U'\u4E00')
         * @param pixel_height Desired font size in pixels
         * @return Glyph metrics, or nullopt if glyph not found
         */
        [[nodiscard]] std::optional<ttf_glyph_metrics> get_glyph_metrics(
            uint32_t codepoint,
            float pixel_height
        ) const;

        /**
         * @brief Get glyph outline shape.
         *
         * Returns the bezier curve data defining the glyph's shape.
         * Use this for path-based rendering with vector graphics APIs.
         *
         * @param codepoint Unicode codepoint
         * @param pixel_height Desired font size in pixels
         * @return Glyph shape with vertices, or nullopt if not found
         */
        [[nodiscard]] std::optional<ttf_glyph_shape> get_glyph_shape(
            uint32_t codepoint,
            float pixel_height
        ) const;

        /**
         * @brief Get kerning adjustment between two characters.
         *
         * Returns the horizontal adjustment to apply between two
         * adjacent characters for improved visual appearance.
         *
         * @param first First character codepoint
         * @param second Second character codepoint
         * @param pixel_height Font size in pixels
         * @return Kerning adjustment (add to advance_x), usually negative
         *
         * @code{.cpp}
         * // Apply kerning in text layout
         * float kern = font.get_kerning('A', 'V', 24.0f);
         * pen_x += glyph_advance + kern;  // kern is typically negative
         * @endcode
         */
        [[nodiscard]] float get_kerning(
            uint32_t first,
            uint32_t second,
            float pixel_height
        ) const;

        /**
         * @brief Check if font contains a specific glyph.
         *
         * @param codepoint Unicode codepoint
         * @return true if glyph exists in font
         */
        [[nodiscard]] bool has_glyph(uint32_t codepoint) const;

        /**
         * @brief Get number of fonts in a TTC collection.
         *
         * For regular TTF/OTF files, returns 1.
         * For TTC (TrueType Collection) files, returns the number
         * of fonts in the collection.
         *
         * @param data Font file contents
         * @return Number of fonts (1 for TTF/OTF, N for TTC)
         */
        [[nodiscard]] static int get_font_count(std::span<const uint8_t> data);

        /**
         * @brief Get the underlying font data.
         * @return Span of the font data passed to constructor
         */
        [[nodiscard]] std::span<const uint8_t> data() const;

        /**
         * @brief Get the font index within a TTC collection.
         * @return Font index (0 for regular TTF/OTF)
         */
        [[nodiscard]] int font_index() const;

    private:
        struct impl;
        std::unique_ptr<impl> m_impl;
    };
} // namespace onyx_font
