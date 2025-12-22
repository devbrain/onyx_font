/**
 * @file stb_truetype_font.hh
 * @brief Low-level stb_truetype wrapper for TTF/OTF rasterization.
 *
 * This file provides the stb_truetype_font class, a lightweight wrapper
 * around the stb_truetype library for rasterizing TrueType and OpenType
 * font glyphs to grayscale bitmaps.
 *
 * @section stb_overview Overview
 *
 * stb_truetype_font provides direct access to stb_truetype's rasterization
 * capabilities. For most use cases, prefer the higher-level ttf_font class
 * which provides additional features like outline access and kerning.
 *
 * This class is primarily used internally by font_source for glyph
 * rasterization, but can be used directly for low-level control.
 *
 * @section stb_usage Usage Examples
 *
 * @subsection stb_basic Basic Rasterization
 *
 * @code{.cpp}
 * // Load font
 * auto font_data = read_file("arial.ttf");
 * stb_truetype_font font(font_data);
 *
 * if (!font.is_valid()) {
 *     // Handle error
 *     return;
 * }
 *
 * // Rasterize 'A' at 24 pixels
 * if (auto glyph = font.rasterize('A', 24.0f)) {
 *     // glyph->bitmap contains grayscale pixels
 *     for (int y = 0; y < glyph->height; ++y) {
 *         for (int x = 0; x < glyph->width; ++x) {
 *             uint8_t alpha = glyph->bitmap[y * glyph->width + x];
 *             blend_pixel(pen_x + x + glyph->offset_x,
 *                         pen_y + y + glyph->offset_y,
 *                         color, alpha);
 *         }
 *     }
 *     pen_x += static_cast<int>(glyph->advance_x);
 * }
 * @endcode
 *
 * @subsection stb_subpixel Subpixel Positioning
 *
 * @code{.cpp}
 * // For better quality at small sizes, use subpixel positioning
 * float x = 10.5f;  // Fractional position
 * float frac_x = x - std::floor(x);
 *
 * if (auto glyph = font.rasterize_subpixel('e', 12.0f, frac_x, 0.0f)) {
 *     // Render at integer position
 *     render_glyph(glyph, static_cast<int>(x), y);
 * }
 * @endcode
 *
 * @author Igor
 * @date 21/12/2025
 */

#pragma once

#include <onyx_font/export.h>
#include <cstdint>
#include <memory>
#include <vector>
#include <span>
#include <optional>

namespace onyx_font {
    /**
     * @brief Rasterized glyph bitmap from stb_truetype.
     *
     * Contains the grayscale bitmap data and positioning information
     * needed to render the glyph correctly.
     */
    struct ONYX_FONT_EXPORT stb_glyph_bitmap {
        /**
         * @brief Grayscale bitmap data (0-255 alpha values).
         *
         * Stored in row-major order. Size is width * height bytes.
         * 0 = transparent, 255 = fully opaque.
         */
        std::vector<uint8_t> bitmap;

        int width;     ///< Bitmap width in pixels
        int height;    ///< Bitmap height in pixels

        /**
         * @brief X offset from pen position to glyph left edge.
         *
         * Add this to the pen X position when rendering.
         * Can be negative for glyphs that extend left of the origin.
         */
        int offset_x;

        /**
         * @brief Y offset from baseline to glyph top edge.
         *
         * Add this to the baseline Y position when rendering.
         * Typically negative (glyphs extend above baseline).
         */
        int offset_y;

        /**
         * @brief Horizontal advance to next glyph origin.
         *
         * The distance to move the pen after rendering this glyph.
         */
        float advance_x;
    };

    /**
     * @brief Low-level stb_truetype wrapper for TTF/OTF rasterization.
     *
     * Provides direct access to stb_truetype's glyph rasterization.
     * This is a low-level utility; most applications should use
     * ttf_font or font_source instead.
     *
     * @section stb_font_features Features
     *
     * - TrueType (.ttf) and OpenType (.otf) support
     * - TrueType Collection (.ttc) support
     * - High-quality antialiased rasterization
     * - Subpixel positioning for improved quality
     *
     * @section stb_font_lifetime Lifetime Management
     *
     * @warning The font data span passed to the constructor must remain
     *          valid for the entire lifetime of this object. The class
     *          stores a reference to the data, not a copy.
     */
    class ONYX_FONT_EXPORT stb_truetype_font {
    public:
        /**
         * @brief Construct from font file data.
         *
         * Initializes the stb_truetype font info from TTF, OTF, or TTC data.
         * Use is_valid() to check if initialization succeeded.
         *
         * @param data Font file contents (must remain valid)
         * @param font_index Index within TTC collection (0 for regular TTF/OTF)
         */
        explicit stb_truetype_font(std::span<const uint8_t> data, int font_index = 0);

        /**
         * @brief Destructor.
         */
        ~stb_truetype_font();

        /**
         * @brief Move constructor.
         */
        stb_truetype_font(stb_truetype_font&& other) noexcept;

        /**
         * @brief Move assignment operator.
         */
        stb_truetype_font& operator=(stb_truetype_font&& other) noexcept;

        /// @cond
        stb_truetype_font(const stb_truetype_font&) = delete;
        stb_truetype_font& operator=(const stb_truetype_font&) = delete;
        /// @endcond

        /**
         * @brief Check if font was loaded successfully.
         * @return true if font is valid and ready to use
         */
        [[nodiscard]] bool is_valid() const;

        /**
         * @brief Rasterize a glyph to a grayscale bitmap.
         *
         * Renders the specified codepoint at the given pixel height.
         * The bitmap uses 8-bit grayscale (0-255 alpha).
         *
         * @param codepoint Unicode codepoint to render
         * @param pixel_height Desired height in pixels
         * @return Rasterized glyph data, or nullopt if glyph not found
         *
         * @code{.cpp}
         * if (auto g = font.rasterize('W', 32.0f)) {
         *     // Use g->bitmap, g->width, g->height, etc.
         * }
         * @endcode
         */
        [[nodiscard]] std::optional<stb_glyph_bitmap> rasterize(
            uint32_t codepoint,
            float pixel_height
        ) const;

        /**
         * @brief Rasterize a glyph with subpixel positioning.
         *
         * Renders the glyph with a fractional pixel offset for
         * improved positioning accuracy at small sizes.
         *
         * @param codepoint Unicode codepoint to render
         * @param pixel_height Desired height in pixels
         * @param shift_x Subpixel X offset (0.0 to 1.0)
         * @param shift_y Subpixel Y offset (0.0 to 1.0)
         * @return Rasterized glyph data, or nullopt if glyph not found
         *
         * @section subpixel_usage Usage
         *
         * @code{.cpp}
         * float pen_x = 100.3f;  // Fractional position
         * float frac = pen_x - std::floor(pen_x);
         *
         * auto g = font.rasterize_subpixel('A', 14.0f, frac, 0.0f);
         * // Render at integer position
         * @endcode
         */
        [[nodiscard]] std::optional<stb_glyph_bitmap> rasterize_subpixel(
            uint32_t codepoint,
            float pixel_height,
            float shift_x,
            float shift_y
        ) const;

        /**
         * @brief Get scale factor for a given pixel height.
         *
         * Calculates the scale to apply to font units to achieve
         * the specified pixel height. Useful for manual glyph
         * positioning calculations.
         *
         * @param pixel_height Desired height in pixels
         * @return Scale factor (multiply font units by this value)
         */
        [[nodiscard]] float get_scale_for_pixel_height(float pixel_height) const;

        /**
         * @brief Get number of fonts in a TTC collection.
         *
         * For regular TTF/OTF files, returns 1.
         * For TTC (TrueType Collection), returns the font count.
         *
         * @param data Font file contents
         * @return Number of fonts in the file
         */
        [[nodiscard]] static int get_font_count(std::span<const uint8_t> data);

    private:
        struct impl;
        std::unique_ptr<impl> m_impl;
    };
} // namespace onyx_font
