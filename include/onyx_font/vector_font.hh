/**
 * @file vector_font.hh
 * @brief Stroke-based vector font representation and rendering support.
 *
 * This file provides the vector_font class for working with scalable
 * stroke-based fonts. These fonts store glyphs as sequences of drawing
 * commands (move, line, end), making them resolution-independent.
 *
 * @section vector_coord Coordinate System
 *
 * Vector fonts use relative coordinates:
 * - Coordinates are deltas (dx, dy) from current pen position
 * - Y increases downward (screen coordinates)
 * - Origin is at the left side of the baseline
 *
 * @code
 *                  Y
 *                  ^
 *      ascent  --> |.....***.....  <- top of character
 *                  |...*...*....
 *                  |..*.....*...
 *                  |..*******...
 *                  |..*.....*...
 *                  |..*.....*...
 * baseline (0) --> +-----------> X (origin)
 *      descent --> |..*.....    <- descenders (g, p, y)
 *                  |...*....
 * @endcode
 *
 * @section stroke_types Stroke Command Types
 *
 * - **MOVE_TO**: Move pen without drawing (starts new polyline)
 * - **LINE_TO**: Draw line from current to new position
 * - **END**: Lift pen (end polyline segment)
 *
 * @section vector_render Rendering Example
 *
 * @code{.cpp}
 * void render_glyph(const vector_glyph& glyph, int origin_x, int origin_y) {
 *     int pen_x = origin_x;
 *     int pen_y = origin_y;
 *     bool pen_down = false;
 *     int last_x = pen_x, last_y = pen_y;
 *
 *     for (const auto& cmd : glyph.strokes) {
 *         switch (cmd.type) {
 *             case stroke_type::MOVE_TO:
 *                 pen_x += cmd.dx;
 *                 pen_y += cmd.dy;
 *                 last_x = pen_x;
 *                 last_y = pen_y;
 *                 pen_down = true;
 *                 break;
 *
 *             case stroke_type::LINE_TO: {
 *                 int new_x = pen_x + cmd.dx;
 *                 int new_y = pen_y + cmd.dy;
 *                 if (pen_down) {
 *                     draw_line(last_x, last_y, new_x, new_y, color);
 *                 }
 *                 pen_x = new_x;
 *                 pen_y = new_y;
 *                 last_x = pen_x;
 *                 last_y = pen_y;
 *                 break;
 *             }
 *
 *             case stroke_type::END:
 *                 pen_down = false;
 *                 break;
 *         }
 *     }
 * }
 * @endcode
 *
 * @section vector_scaling Scaling
 *
 * Vector fonts scale naturally by multiplying coordinates:
 *
 * @code{.cpp}
 * float scale = 2.0f;
 * int sdx = static_cast<int>(cmd.dx * scale);
 * int sdy = static_cast<int>(cmd.dy * scale);
 * // Use sdx, sdy instead of cmd.dx, cmd.dy
 * @endcode
 *
 * @author Igor
 * @date 11/12/2025
 */

#pragma once

#include <onyx_font/export.h>
#include <string>
#include <cstdint>
#include <vector>

namespace onyx_font {
    namespace internal {
        struct win_vector_fon_loader;
        struct bgi_font_loader;
    }

    /**
     * @brief Stroke command types for vector font rendering.
     *
     * These commands form a simple path-based drawing language,
     * similar to PostScript or SVG path commands but simplified
     * to just moves and lines.
     */
    enum class stroke_type : uint8_t {
        /**
         * @brief Move pen without drawing.
         *
         * Repositions the pen to a new location without leaving a mark.
         * Use at the start of a glyph or to begin a disconnected stroke
         * (like the dot of 'i' or 'j').
         */
        MOVE_TO,

        /**
         * @brief Draw a line to the new position.
         *
         * Draws a straight line from the current pen position to
         * the new position (current + delta). Updates pen position
         * after drawing.
         */
        LINE_TO,

        /**
         * @brief End of current polyline segment.
         *
         * Lifts the "pen" - subsequent LINE_TO commands will move
         * without drawing until the next MOVE_TO. Use for disconnected
         * strokes within a single glyph.
         */
        END
    };

    /**
     * @brief Single stroke command with relative coordinates.
     *
     * Each command moves the pen by (dx, dy) relative to its current
     * position. For MOVE_TO and LINE_TO, the delta is applied.
     * For END, the delta values are typically (0, 0) but are ignored.
     *
     * @note Deltas are stored as int8_t for memory efficiency.
     *       This limits individual strokes to -128..+127 pixels,
     *       but multiple strokes can cover any distance.
     */
    struct ONYX_FONT_EXPORT stroke_command {
        stroke_type type;  ///< Command type (MOVE_TO, LINE_TO, END)
        int8_t dx;         ///< X delta (relative to current position)
        int8_t dy;         ///< Y delta (relative to current position)
    };

    /**
     * @brief Vector glyph containing stroke-based drawing commands.
     *
     * A glyph is defined as a sequence of stroke commands that,
     * when executed in order, draw the character shape. The width
     * field indicates how far to advance after drawing.
     *
     * @section glyph_example Example Glyph Data
     *
     * A simple 'L' character might have strokes like:
     * @code{.cpp}
     * strokes = {
     *     {MOVE_TO, 0, 0},      // Start at origin
     *     {LINE_TO, 0, 10},     // Draw down
     *     {LINE_TO, 6, 0},      // Draw right
     *     {END, 0, 0}           // End polyline
     * };
     * width = 8;  // Advance 8 pixels
     * @endcode
     */
    struct ONYX_FONT_EXPORT vector_glyph {
        /**
         * @brief Horizontal advance width.
         *
         * The distance to move the pen rightward after rendering
         * this glyph, before drawing the next character.
         */
        uint16_t width;

        /**
         * @brief Sequence of stroke commands.
         *
         * Commands are executed in order to draw the glyph.
         * An empty vector indicates a blank glyph (like space).
         */
        std::vector<stroke_command> strokes;
    };

    /**
     * @brief Font-level metrics for vector fonts.
     *
     * These metrics describe the font's overall dimensions and
     * are used for text layout, line spacing, and positioning.
     */
    struct ONYX_FONT_EXPORT vector_font_metrics {
        /**
         * @brief Distance from baseline to top of tallest character.
         *
         * Positive value indicating how far above the baseline
         * the tallest characters (like 'b', 'd', 'h') extend.
         */
        int16_t ascent;

        /**
         * @brief Distance from baseline to bottom of deepest descender.
         *
         * Typically a negative value indicating how far below
         * the baseline characters like 'g', 'p', 'y' extend.
         */
        int16_t descent;

        /**
         * @brief Y offset from glyph origin to baseline.
         *
         * Add this to the starting Y position to place the
         * baseline correctly. For rendering at position (x, y),
         * use origin at (x, y + baseline).
         */
        int16_t baseline;

        /**
         * @brief Total character height (ascent - descent).
         *
         * The full vertical extent of the font, from top of
         * ascenders to bottom of descenders.
         */
        uint16_t pixel_height;

        /**
         * @brief Average character width.
         *
         * Typical width of characters in the font.
         * Useful for estimating text width.
         */
        uint16_t avg_width;

        /**
         * @brief Maximum character width.
         *
         * Width of the widest character in the font.
         */
        uint16_t max_width;
    };

    /**
     * @brief Stroke-based vector font with scalable glyph data.
     *
     * Vector fonts store characters as sequences of drawing commands
     * rather than pixel bitmaps. This makes them resolution-independent
     * and smoothly scalable to any size.
     *
     * Advantages:
     * - Scale to any size without pixelation
     * - Small file size (especially for simple fonts)
     * - Can be rendered with anti-aliasing for smooth edges
     * - Easy to modify stroke width for bold effects
     *
     * Limitations:
     * - Less suitable for complex, detailed glyphs
     * - May look thin at very small sizes
     * - No built-in hinting for pixel alignment
     *
     * Common sources:
     * - Windows .FON files (NE/PE executables with vector font resources)
     * - Borland BGI .CHR files (Turbo C/Pascal graphics fonts)
     *
     * @section vector_usage Usage Example
     *
     * @code{.cpp}
     * // Load a vector font
     * auto data = read_file("gothic.fon");
     * auto fonts = font_factory::load_all_vectors(data);
     *
     * if (!fonts.empty()) {
     *     const auto& font = fonts[0];
     *     const auto& metrics = font.get_metrics();
     *
     *     // Render text
     *     int pen_x = 100;
     *     int pen_y = 100 + metrics.baseline;
     *
     *     for (char ch : "Hello") {
     *         if (const auto* glyph = font.get_glyph(ch)) {
     *             render_glyph(*glyph, pen_x, pen_y);
     *             pen_x += glyph->width;
     *         }
     *     }
     * }
     * @endcode
     *
     * @see font_factory For loading vector fonts from files
     * @see font_converter For converting vector fonts to bitmap
     */
    class ONYX_FONT_EXPORT vector_font {
        friend struct internal::win_vector_fon_loader;
        friend struct internal::bgi_font_loader;

    public:
        /**
         * @brief Construct an empty vector font.
         *
         * Creates an uninitialized font with no glyphs.
         * Use font_factory to load fonts from files.
         */
        vector_font();

        /**
         * @brief Get the font's display name.
         * @return Font name (e.g., "Roman", "Gothic", "Script")
         */
        [[nodiscard]] std::string get_name() const;

        /**
         * @brief Get the first character code in the font.
         * @return First valid character code
         */
        [[nodiscard]] uint8_t get_first_char() const;

        /**
         * @brief Get the last character code in the font.
         * @return Last valid character code
         */
        [[nodiscard]] uint8_t get_last_char() const;

        /**
         * @brief Get the default character for missing glyphs.
         * @return Default character code (typically '?' or space)
         */
        [[nodiscard]] uint8_t get_default_char() const;

        /**
         * @brief Get font-level metrics.
         * @return Reference to font metrics
         */
        [[nodiscard]] const vector_font_metrics& get_metrics() const;

        /**
         * @brief Get glyph data for a character.
         *
         * Returns a pointer to the glyph's stroke commands.
         * Returns nullptr if the character is not in the font.
         *
         * @param ch Character code
         * @return Pointer to glyph, or nullptr if not found
         */
        [[nodiscard]] const vector_glyph* get_glyph(uint8_t ch) const;

        /**
         * @brief Check if a character exists in the font.
         * @param ch Character code
         * @return true if character is in font range
         */
        [[nodiscard]] bool has_glyph(uint8_t ch) const;

    private:
        std::string m_name;              ///< Font display name
        uint8_t m_first_char{};          ///< First character in font
        uint8_t m_last_char{};           ///< Last character in font
        uint8_t m_default_char{};        ///< Fallback character

        vector_font_metrics m_metrics{}; ///< Font-level metrics
        std::vector<vector_glyph> m_glyphs;  ///< Glyph data
    };
} // namespace onyx_font
