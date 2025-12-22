/**
 * @file font_source.hh
 * @brief Unified font source abstraction for all font types.
 *
 * This file provides the font_source class, which wraps bitmap_font,
 * vector_font, or ttf_font to provide a consistent interface for
 * accessing metrics and rasterizing glyphs. This abstraction allows
 * the text rendering system to work with any font type seamlessly.
 *
 * @section source_overview Overview
 *
 * font_source is the bridge between low-level font data and the
 * high-level text rendering pipeline. It provides:
 * - Unified glyph metrics access
 * - Font-type-independent rasterization
 * - Kerning support (when available)
 * - Automatic fallback character handling
 *
 * @section source_architecture Architecture
 *
 * @code
 * +---------------+     +---------------+     +----------------+
 * | bitmap_font   |     | vector_font   |     | ttf_font       |
 * +-------+-------+     +-------+-------+     +--------+-------+
 *         |                     |                      |
 *         v                     v                      v
 *     +---+---------------------+----------------------+---+
 *     |              font_source (unified interface)       |
 *     +---+---------------------+----------------------+---+
 *                               |
 *                               v
 *                    +----------+-----------+
 *                    |   text_rasterizer    |
 *                    |   glyph_cache        |
 *                    |   text_renderer      |
 *                    +----------------------+
 * @endcode
 *
 * @section source_usage Usage Examples
 *
 * @code{.cpp}
 * // From bitmap font
 * bitmap_font bitmap = font_factory::load_bitmap(data, 0);
 * font_source src1 = font_source::from_bitmap(bitmap);
 *
 * // From TTF font
 * ttf_font ttf(ttf_data);
 * font_source src2 = font_source::from_ttf(ttf);
 *
 * // Use the same interface for both
 * auto metrics = src1.get_scaled_metrics(16.0f);
 * auto glyph_m = src1.get_glyph_metrics('A', 16.0f);
 * @endcode
 *
 * @author Igor
 * @date 21/12/2025
 */

#pragma once

#include <onyx_font/export.h>
#include <onyx_font/text/types.hh>
#include <onyx_font/text/raster_target.hh>
#include <onyx_font/bitmap_font.hh>
#include <onyx_font/vector_font.hh>
#include <onyx_font/ttf_font.hh>
#include <onyx_font/utils/stb_truetype_font.hh>
#include <memory>
#include <variant>

namespace onyx_font {
    /**
     * @brief Unified font wrapper for all font types.
     *
     * Provides consistent access to metrics, glyph information, and
     * rasterization across bitmap, vector, and TTF fonts. This abstraction
     * is move-only (not copyable) due to internal resource ownership.
     *
     * @section font_source_features Features
     *
     * - **Unified metrics**: Same interface for all font types
     * - **Type-aware scaling**: Handles scalable vs fixed-size fonts
     * - **Kerning support**: When font provides kerning data
     * - **Template rasterization**: Works with any raster_target
     *
     * @section font_source_lifetime Lifetime Considerations
     *
     * @warning The underlying font (bitmap_font, vector_font, or ttf_font)
     *          must remain valid for the lifetime of the font_source.
     *          font_source stores a pointer to the source font.
     *
     * @see text_rasterizer For text measurement and rendering
     * @see glyph_cache For cached glyph rendering
     */
    class ONYX_FONT_EXPORT font_source {
    public:
        /// @cond
        font_source(const font_source&) = delete;
        font_source& operator=(const font_source&) = delete;
        /// @endcond

        /**
         * @brief Move constructor.
         */
        font_source(font_source&&) noexcept;

        /**
         * @brief Move assignment operator.
         */
        font_source& operator=(font_source&&) noexcept;

        /**
         * @brief Destructor.
         */
        ~font_source();

        /**
         * @brief Create from bitmap font.
         *
         * @param font Bitmap font (must remain valid)
         * @return font_source wrapping the bitmap font
         *
         * @note For bitmap fonts, the size parameter in scaling methods
         *       is ignored - the native size is always used.
         */
        static font_source from_bitmap(const bitmap_font& font);

        /**
         * @brief Create from vector font.
         *
         * @param font Vector font (must remain valid)
         * @return font_source wrapping the vector font
         */
        static font_source from_vector(const vector_font& font);

        /**
         * @brief Create from TTF font.
         *
         * Creates internal rasterizer for TTF glyph rendering.
         *
         * @param font TTF font (must remain valid)
         * @return font_source wrapping the TTF font
         */
        static font_source from_ttf(const ttf_font& font);

        /**
         * @brief Get the underlying font type.
         * @return Font type enum
         */
        [[nodiscard]] font_source_type type() const;

        /**
         * @brief Check if font has a specific glyph.
         *
         * @param codepoint Unicode codepoint
         * @return true if glyph exists in font
         */
        [[nodiscard]] bool has_glyph(char32_t codepoint) const;

        /**
         * @brief Get the default/fallback character.
         *
         * Returns the character to use when a requested glyph
         * is not available in the font.
         *
         * @return Default character codepoint
         */
        [[nodiscard]] char32_t default_char() const;

        /**
         * @brief Get scaled metrics for a given pixel size.
         *
         * Returns font-wide metrics (ascent, descent, line gap) scaled
         * to the specified pixel height.
         *
         * @param size Pixel height (ignored for bitmap fonts)
         * @return Scaled font metrics
         */
        [[nodiscard]] scaled_metrics get_scaled_metrics(float size) const;

        /**
         * @brief Get metrics for a single glyph.
         *
         * Returns positioning and sizing information for a glyph
         * at the specified size.
         *
         * @param codepoint Unicode codepoint
         * @param size Pixel height
         * @return Glyph metrics (advance, bearing, size)
         */
        [[nodiscard]] glyph_metrics get_glyph_metrics(char32_t codepoint, float size) const;

        /**
         * @brief Get kerning between two characters.
         *
         * Returns the horizontal adjustment to apply between adjacent
         * characters for improved appearance.
         *
         * @param first First codepoint
         * @param second Second codepoint
         * @param size Pixel height
         * @return Kerning adjustment (add to advance_x)
         *
         * @note Returns 0 for fonts without kerning data.
         */
        [[nodiscard]] float get_kerning(char32_t first, char32_t second, float size) const;

        /**
         * @brief Get native pixel height for bitmap fonts.
         *
         * Returns the native size of the font. For bitmap fonts,
         * this is the only size that renders correctly. For scalable
         * fonts (vector, TTF), returns 0.
         *
         * @return Native size, or 0 for scalable fonts
         */
        [[nodiscard]] float native_size() const;

        /**
         * @brief Rasterize a single glyph to target.
         *
         * Renders the specified glyph at the given position using
         * the appropriate rendering method for the font type.
         *
         * @tparam Target Raster target type (must satisfy raster_target concept)
         * @param codepoint Unicode codepoint
         * @param size Pixel height for rendering
         * @param target Raster target to write to
         * @param x X position in target
         * @param y Y position (baseline)
         */
        template<raster_target Target>
        void rasterize_glyph(char32_t codepoint, float size,
                             Target& target, int x, int y) const;

    private:
        /// Internal representation for bitmap font reference
        struct bitmap_ref {
            const bitmap_font* font;
        };

        /// Internal representation for vector font reference
        struct vector_ref {
            const vector_font* font;
        };

        /// Internal representation for TTF font reference
        struct ttf_ref {
            const ttf_font* font;
        };

        std::variant<bitmap_ref, vector_ref, ttf_ref> m_font;

        /// Owned rasterizer for TTF fonts (created internally by from_ttf)
        std::unique_ptr<stb_truetype_font> m_rasterizer;

        font_source() = default;

        // Internal rasterization helpers (implemented in .cc)
        void rasterize_bitmap_glyph(char32_t codepoint, float size,
                                    void* target, int x, int y,
                                    void (*put_pixel)(void*, int, int, uint8_t)) const;

        void rasterize_vector_glyph(char32_t codepoint, float size,
                                    void* target, int x, int y,
                                    void (*put_pixel)(void*, int, int, uint8_t),
                                    int width, int height) const;

        void rasterize_ttf_glyph(char32_t codepoint, float size,
                                 void* target, int x, int y,
                                 void (*put_pixel)(void*, int, int, uint8_t)) const;
    };

    // Template implementation
    template<raster_target Target>
    void font_source::rasterize_glyph(char32_t codepoint, float size,
                                      Target& target, int x, int y) const {
        // Type-erased callback that wraps the target
        auto put_pixel = [](void* ctx, int px, int py, uint8_t alpha) {
            static_cast<Target*>(ctx)->put_pixel(px, py, alpha);
        };

        if (std::holds_alternative<bitmap_ref>(m_font)) {
            rasterize_bitmap_glyph(codepoint, size, &target, x, y, put_pixel);
        } else if (std::holds_alternative<vector_ref>(m_font)) {
            rasterize_vector_glyph(codepoint, size, &target, x, y, put_pixel,
                                   target.width(), target.height());
        } else {
            rasterize_ttf_glyph(codepoint, size, &target, x, y, put_pixel);
        }
    }
} // namespace onyx_font
