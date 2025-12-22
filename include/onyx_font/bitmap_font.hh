/**
 * @file bitmap_font.hh
 * @brief Bitmap (raster) font representation and rendering support.
 *
 * This file provides the bitmap_font class for working with fixed-size
 * raster fonts. These fonts store glyphs as 1-bit per pixel bitmaps,
 * suitable for retro-style graphics, terminal emulators, and embedded systems.
 *
 * @section coord_system Coordinate System
 *
 * The coordinate system uses screen coordinates:
 * - Origin (0,0) is at top-left of the glyph bounding box
 * - X increases rightward
 * - Y increases downward
 * - Baseline is at Y = ascent (from top of cell)
 *
 * @code
 *          +------------------+  <- Y = 0 (top of cell)
 *          |   internal       |
 *          |   leading        |  <- Space for accents (diacritics)
 *          +------------------+  <- Y = internal_leading
 *          |                  |
 *          |   A   A   M M    |  <- Glyph body (ascender region)
 *          |  A A  A   MMM    |
 *          | AAAA  AAAA M M   |
 *          | A  A  A  A M M   |
 *          +------------------+  <- Y = ascent (BASELINE)
 *          |   g     p        |  <- Descender region (g, p, y, etc.)
 *          |   g     p        |
 *          +------------------+  <- Y = pixel_height
 * @endcode
 *
 * @section abc_model ABC Spacing Model
 *
 * Each glyph uses the ABC spacing model for precise character positioning:
 *
 * @code
 *   |<-- A -->|<---- B ---->|<-- C -->|
 *   |         |             |         |
 *   | bearing | glyph width | bearing |
 *   | (left)  |   (black)   | (right) |
 * @endcode
 *
 * - **A-space**: Distance from current pen position to left edge of glyph
 *   (can be negative for overhanging glyphs like 'j' or italic chars)
 * - **B-space**: Width of the glyph's black pixels (bitmap width)
 * - **C-space**: Distance from right edge of glyph to next character position
 *   (can be negative for kerning effects)
 *
 * Total character advance = A + B + C
 *
 * @section render_example Rendering Example
 *
 * @code{.cpp}
 * // Render a text string
 * int pen_x = start_x;
 * int pen_y = start_y;  // Top of text line
 *
 * for (char ch : text) {
 *     if (!font.has_glyph(ch)) {
 *         ch = font.get_default_char();
 *     }
 *
 *     const auto& spacing = font.get_spacing(ch);
 *     bitmap_view glyph = font.get_glyph(ch);
 *
 *     // Apply A-space (left side bearing)
 *     if (spacing.a_space) {
 *         pen_x += *spacing.a_space;
 *     }
 *
 *     // Draw the glyph
 *     for (uint16_t y = 0; y < glyph.height(); ++y) {
 *         for (uint16_t x = 0; x < glyph.width(); ++x) {
 *             if (glyph.pixel(x, y)) {
 *                 draw_pixel(pen_x + x, pen_y + y, color);
 *             }
 *         }
 *     }
 *
 *     // Advance pen position
 *     pen_x += spacing.b_space.value_or(glyph.width());
 *
 *     // Apply C-space (right side bearing)
 *     if (spacing.c_space) {
 *         pen_x += *spacing.c_space;
 *     }
 * }
 * @endcode
 *
 * @section line_spacing Line Spacing
 *
 * For multi-line text:
 * @code{.cpp}
 * int line_height = metrics.pixel_height + metrics.external_leading;
 * pen_y += line_height;  // Move to next line
 * @endcode
 *
 * @author Igor
 * @date 11/12/2025
 */

#pragma once

#include <onyx_font/export.h>
#include <string>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>
#include <onyx_font/utils/bitmap_glyphs_storage.hh>

namespace onyx_font {
    namespace internal {
        struct win_bitmap_fon_loader;
    }

    struct font_converter;
    struct font_factory;

    /**
     * @brief Font-level metrics for text layout and rendering.
     *
     * These metrics describe the vertical dimensions and typical character
     * widths for a font. They are used for calculating line heights,
     * baseline positions, and text layout.
     *
     * @see bitmap_font::get_metrics()
     */
    struct ONYX_FONT_EXPORT font_metrics {
        /**
         * @brief Distance from top of character cell to baseline.
         *
         * The ascent includes internal leading (space for accents).
         * Characters like 'A', 'M', 'x' have their tops near Y = internal_leading
         * and their bottoms at Y = ascent (the baseline).
         */
        std::uint16_t ascent = 0;

        /**
         * @brief Total height of character cell in pixels.
         *
         * This is the complete height from top to bottom, including
         * space for ascenders, descenders, and any internal/external leading.
         * Use this for calculating text bounds and line heights.
         */
        std::uint16_t pixel_height = 0;

        /**
         * @brief Space reserved for accent marks within the ascent.
         *
         * This is the vertical space at the top of the character cell
         * reserved for diacritical marks (accents, umlauts, etc.).
         * The main glyph body starts at Y = internal_leading.
         */
        std::uint16_t internal_leading = 0;

        /**
         * @brief Recommended extra spacing between lines.
         *
         * This is additional vertical space to add between lines of text
         * for improved readability. Total line height should be:
         * `pixel_height + external_leading`
         */
        std::uint16_t external_leading = 0;

        /**
         * @brief Average character width in pixels.
         *
         * Typically calculated from lowercase letters. Useful for
         * estimating text width before rendering, or for fixed-width
         * layout calculations.
         */
        std::uint16_t avg_width = 0;

        /**
         * @brief Maximum character width in pixels.
         *
         * The width of the widest character in the font.
         * Useful for buffer allocation and worst-case layout calculations.
         */
        std::uint16_t max_width = 0;
    };

    /**
     * @brief Per-glyph ABC spacing for precise character positioning.
     *
     * The ABC spacing model defines how characters are positioned relative
     * to each other. Each component is optional - if not present, use
     * default values based on glyph dimensions.
     *
     * @see bitmap_font::get_spacing()
     */
    struct ONYX_FONT_EXPORT glyph_spacing {
        /**
         * @brief Left side bearing (A-space).
         *
         * Distance from current pen position to the left edge of the glyph bitmap.
         * - Positive: adds space before the glyph
         * - Negative: glyph overhangs to the left (e.g., italic 'f', 'j')
         * - nullopt: use 0 as default
         */
        std::optional<std::int16_t> a_space;

        /**
         * @brief Glyph bitmap width (B-space).
         *
         * The width of the actual glyph bitmap in pixels.
         * This is the "black width" - the area that may contain ink.
         * - nullopt: use the glyph's bitmap width
         */
        std::optional<std::uint16_t> b_space;

        /**
         * @brief Right side bearing (C-space).
         *
         * Distance from the right edge of the glyph bitmap to the next
         * character's starting position.
         * - Positive: adds space after the glyph
         * - Negative: next character overlaps (kerning effect)
         * - nullopt: use 0 as default
         */
        std::optional<std::int16_t> c_space;
    };

    /**
     * @brief Bitmap font containing raster glyph data.
     *
     * A bitmap font stores characters as fixed-size raster images (1-bit per pixel).
     * Each glyph is stored as a packed bitmap where pixels are represented as bits.
     *
     * Bitmap fonts are:
     * - Fast to render (simple bit operations)
     * - Memory efficient for small sizes
     * - Pixel-perfect at their native size
     * - Not scalable (appear blocky when enlarged)
     *
     * Common sources:
     * - Windows .FON files (NE/PE executables with font resources)
     * - Raw BIOS font dumps (VGA 8x8, 8x14, 8x16)
     * - Converted from vector or TrueType fonts via font_converter
     *
     * @section usage_example Usage Example
     *
     * @code{.cpp}
     * // Load a bitmap font
     * auto data = read_file("vgasys.fon");
     * auto fonts = font_factory::load_all_bitmaps(data);
     *
     * if (!fonts.empty()) {
     *     const auto& font = fonts[0];
     *
     *     // Get font information
     *     std::cout << "Font: " << font.get_name() << "\n";
     *     std::cout << "Height: " << font.get_metrics().pixel_height << "\n";
     *
     *     // Render a character
     *     bitmap_view glyph = font.get_glyph('A');
     *     for (uint16_t y = 0; y < glyph.height(); ++y) {
     *         for (uint16_t x = 0; x < glyph.width(); ++x) {
     *             std::cout << (glyph.pixel(x, y) ? '#' : ' ');
     *         }
     *         std::cout << '\n';
     *     }
     * }
     * @endcode
     *
     * @see font_factory For loading bitmap fonts from various formats
     * @see font_converter For converting vector/TTF fonts to bitmap
     * @see bitmap_view For accessing glyph pixel data
     */
    class ONYX_FONT_EXPORT bitmap_font {
        friend struct internal::win_bitmap_fon_loader;
        friend struct font_converter;
        friend struct font_factory;

    public:
        /**
         * @brief Construct an empty bitmap font.
         *
         * Creates an uninitialized font with no glyphs.
         * Use font_factory to load fonts from files.
         */
        bitmap_font();

        /**
         * @brief Get the font's display name.
         * @return Font name (e.g., "System", "Courier", "Terminal")
         */
        [[nodiscard]] std::string get_name() const;

        /**
         * @brief Get the first character code in the font.
         * @return First valid character code (typically 0 or 32)
         */
        [[nodiscard]] uint8_t get_first_char() const;

        /**
         * @brief Get the last character code in the font.
         * @return Last valid character code (typically 255)
         */
        [[nodiscard]] uint8_t get_last_char() const;

        /**
         * @brief Get the default character for missing glyphs.
         *
         * When a requested character is not in the font's range,
         * this character should be rendered instead.
         *
         * @return Default character code (typically '?' or a box glyph)
         */
        [[nodiscard]] uint8_t get_default_char() const;

        /**
         * @brief Get the word break character.
         * @return Break character code (typically space, 0x20)
         */
        [[nodiscard]] uint8_t get_break_char() const;

        /**
         * @brief Get font-level metrics.
         *
         * Returns metrics describing the font's vertical dimensions
         * and typical character widths.
         *
         * @return Reference to font metrics
         */
        [[nodiscard]] const font_metrics& get_metrics() const;

        /**
         * @brief Get spacing information for a character.
         *
         * Returns the ABC spacing values for positioning the character.
         * If the character is outside the font's range, returns spacing
         * for the default character.
         *
         * @param ch Character code
         * @return Reference to glyph spacing data
         */
        [[nodiscard]] const glyph_spacing& get_spacing(uint8_t ch) const;

        /**
         * @brief Get the bitmap view for a character's glyph.
         *
         * Returns a view into the glyph's bitmap data. The view provides
         * access to individual pixels and row data.
         *
         * If the character is outside the font's range, returns the
         * glyph for the default character.
         *
         * @param ch Character code
         * @return Bitmap view for accessing glyph pixels
         *
         * @see bitmap_view For pixel access methods
         */
        [[nodiscard]] bitmap_view get_glyph(uint8_t ch) const;

    private:
        std::string m_name;              ///< Font display name
        uint8_t m_first_char{};          ///< First character in font
        uint8_t m_last_char{};           ///< Last character in font
        uint8_t m_default_char{};        ///< Fallback character
        uint8_t m_break_char{};          ///< Word break character

        font_metrics m_metrics;          ///< Font-level metrics
        std::vector<glyph_spacing> m_spacing;  ///< Per-glyph spacing
        bitmap_storage m_storage;        ///< Glyph bitmap storage
    };
}
