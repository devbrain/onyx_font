/**
 * @file text_rasterizer.hh
 * @brief Low-level text measurement and rasterization with styling support.
 *
 * This file provides the text_rasterizer class for measuring and
 * rendering text strings. It handles UTF-8 decoding, glyph metrics,
 * kerning, styling (bold, italic, underline, strikethrough), and
 * direct rasterization to targets.
 *
 * @section rasterizer_overview Overview
 *
 * text_rasterizer provides the core text rendering functionality:
 * - Text measurement (single line, wrapped)
 * - Glyph rasterization with proper positioning
 * - Kerning between adjacent characters
 * - Text styling (bold, italic, underline, strikethrough)
 * - Size-independent operation via font_source
 *
 * @section rasterizer_usage Usage Examples
 *
 * @subsection rasterizer_measure Measuring Text
 *
 * @code{.cpp}
 * ttf_font font(data);
 * text_rasterizer rasterizer(font_source::from_ttf(font));
 * rasterizer.set_size(24.0f);
 *
 * // Measure a string
 * text_extents extents = rasterizer.measure_text("Hello World");
 * std::cout << "Width: " << extents.width << "px\n";
 * std::cout << "Height: " << extents.height << "px\n";
 *
 * // Measure with wrapping
 * text_extents wrapped = rasterizer.measure_wrapped("Long text...", 200.0f);
 * @endcode
 *
 * @subsection rasterizer_render Rendering Text
 *
 * @code{.cpp}
 * std::vector<uint8_t> buffer(width * height, 0);
 * grayscale_target target(buffer.data(), width, height);
 *
 * // Render at baseline position
 * float width = rasterizer.rasterize_text("Hello", target, 10, 40);
 * @endcode
 *
 * @subsection rasterizer_styled Styled Text
 *
 * @code{.cpp}
 * // Simple style flags
 * rasterizer.set_style(text_style::bold | text_style::underline);
 * rasterizer.rasterize_text("Bold & Underlined", target, 10, 40);
 *
 * // Detailed style control
 * render_style style;
 * style.style = text_style::bold | text_style::italic;
 * style.bold_strength = 2;
 * style.italic_shear = 0.25f;
 * rasterizer.set_style(style);
 * @endcode
 *
 * @author Igor
 * @date 21/12/2025
 */

#pragma once

#include <onyx_font/export.h>
#include <onyx_font/text/types.hh>
#include <onyx_font/text/font_source.hh>
#include <onyx_font/text/raster_target.hh>
#include <onyx_font/text/text_style.hh>
#include <onyx_font/text/utf8.hh>
#include <string_view>
#include <cmath>
#include <vector>
#include <algorithm>

namespace onyx_font {

    /**
     * @brief Temporary buffer for glyph transforms (italic shear, etc.).
     *
     * Used internally by text_rasterizer when applying transforms that
     * require an intermediate buffer.
     */
    class glyph_buffer {
    public:
        glyph_buffer(int width, int height)
            : m_width(width), m_height(height)
            , m_data(static_cast<size_t>(width * height), 0) {}

        void put_pixel(int x, int y, uint8_t alpha) {
            if (x >= 0 && x < m_width && y >= 0 && y < m_height) {
                auto& existing = m_data[static_cast<size_t>(y * m_width + x)];
                existing = std::max(existing, alpha);
            }
        }

        [[nodiscard]] uint8_t get_pixel(int x, int y) const {
            if (x >= 0 && x < m_width && y >= 0 && y < m_height) {
                return m_data[static_cast<size_t>(y * m_width + x)];
            }
            return 0;
        }

        [[nodiscard]] int width() const { return m_width; }
        [[nodiscard]] int height() const { return m_height; }

        void clear() {
            std::fill(m_data.begin(), m_data.end(), uint8_t{0});
        }

    private:
        int m_width;
        int m_height;
        std::vector<uint8_t> m_data;
    };

    /**
     * @brief Low-level text measurement and rasterization.
     *
     * Provides direct text rendering using a font_source. Handles
     * UTF-8 decoding, glyph positioning with kerning, and rasterization
     * to any conforming raster target.
     *
     * @section text_rasterizer_features Features
     *
     * - UTF-8 text input
     * - Precise text measurement
     * - Kerning support
     * - Glyph-by-glyph callback option
     * - Size control for scalable fonts
     *
     * @see glyph_cache For cached rendering (better for GPU)
     * @see text_renderer For high-level features (alignment, wrapping)
     */
    class ONYX_FONT_EXPORT text_rasterizer {
    public:
        /**
         * @brief Create rasterizer from font source.
         *
         * @param source Font source (takes ownership via move)
         */
        explicit text_rasterizer(font_source source);

        /**
         * @brief Set the rendering size in pixels.
         *
         * Sets the target pixel height for text rendering and measurement.
         * For bitmap fonts, this may be ignored if the font is not scalable.
         *
         * @param pixels Font height in pixels
         */
        void set_size(float pixels);

        /**
         * @brief Get current size.
         * @return Current pixel height
         */
        [[nodiscard]] float size() const { return m_size; }

        // ====================================================================
        // Style Control
        // ====================================================================

        /**
         * @brief Set text style using simple flags.
         *
         * Convenience method for setting basic styles without fine-grained control.
         *
         * @param style Style flags (bold, italic, underline, strikethrough)
         *
         * @code{.cpp}
         * rasterizer.set_style(text_style::bold | text_style::underline);
         * @endcode
         */
        void set_style(text_style style);

        /**
         * @brief Set text style with detailed control.
         *
         * Provides fine-grained control over styling parameters like
         * bold strength, italic shear angle, and decoration thickness.
         *
         * @param style Detailed render style options
         *
         * @code{.cpp}
         * render_style style;
         * style.style = text_style::bold | text_style::italic;
         * style.bold_strength = 2;
         * style.italic_shear = 0.25f;
         * rasterizer.set_style(style);
         * @endcode
         */
        void set_style(const render_style& style);

        /**
         * @brief Get current style flags.
         * @return Current text style flags
         */
        [[nodiscard]] text_style style() const { return m_render_style.style; }

        /**
         * @brief Get current detailed render style.
         * @return Current render style with all parameters
         */
        [[nodiscard]] const render_style& get_render_style() const { return m_render_style; }

        /**
         * @brief Reset style to normal (no styling).
         */
        void reset_style() { m_render_style.reset(); }

        /**
         * @brief Get scaled font metrics at current size.
         * @return Font metrics (ascent, descent, line gap, line height)
         */
        [[nodiscard]] scaled_metrics get_metrics() const;

        /**
         * @brief Measure a single glyph.
         *
         * @param codepoint Unicode codepoint
         * @return Glyph metrics at current size
         */
        [[nodiscard]] glyph_metrics measure_glyph(char32_t codepoint) const;

        /**
         * @brief Measure a text string.
         *
         * Calculates the dimensions of the text when rendered.
         * Includes kerning in width calculation.
         *
         * @param text UTF-8 encoded text
         * @return Text extents (width, height, ascent, descent)
         */
        [[nodiscard]] text_extents measure_text(std::string_view text) const;

        /**
         * @brief Measure text with maximum width (for wrapping calculation).
         *
         * Calculates the dimensions if the text were word-wrapped
         * at the specified maximum width.
         *
         * @param text UTF-8 encoded text
         * @param max_width Maximum line width
         * @return Extents of wrapped text
         */
        [[nodiscard]] text_extents measure_wrapped(std::string_view text, float max_width) const;

        /**
         * @brief Get line height at current size.
         *
         * Returns ascent + descent + line_gap.
         *
         * @return Line height in pixels
         */
        [[nodiscard]] float line_height() const;

        /**
         * @brief Get kerning between two codepoints at current size.
         *
         * @param first First codepoint
         * @param second Second codepoint
         * @return Kerning adjustment (usually negative for tightening)
         */
        [[nodiscard]] float get_kerning(char32_t first, char32_t second) const {
            return m_source.get_kerning(first, second, m_size);
        }

        /**
         * @brief Rasterize a single glyph.
         *
         * Renders one character at the specified position.
         *
         * @tparam Target Raster target type
         * @param codepoint Unicode codepoint
         * @param target Raster target
         * @param x X position
         * @param y Y position (baseline)
         */
        template<raster_target Target>
        void rasterize_glyph(char32_t codepoint, Target& target, int x, int y) const;

        /**
         * @brief Rasterize a text string.
         *
         * Renders a complete string with proper glyph positioning
         * and kerning.
         *
         * @tparam Target Raster target type
         * @param text UTF-8 encoded text
         * @param target Raster target
         * @param x Starting X position
         * @param y Y position (baseline)
         * @return Width of rendered text
         */
        template<raster_target Target>
        float rasterize_text(std::string_view text, Target& target, int x, int y) const;

        /**
         * @brief Callback invoked for each rendered glyph.
         *
         * Provides position and metrics information for each glyph
         * as it is rendered.
         *
         * @param codepoint Unicode codepoint
         * @param x X position where glyph was rendered
         * @param y Y position (baseline)
         * @param metrics Glyph metrics
         * @param user_data User-provided context
         */
        using glyph_callback = void(*)(char32_t codepoint, int x, int y,
                                       const glyph_metrics& metrics, void* user_data);

        /**
         * @brief Rasterize text with callback for each glyph.
         *
         * Renders text and invokes a callback for each glyph,
         * providing position and metrics information.
         *
         * @tparam Target Raster target type
         * @param text UTF-8 encoded text
         * @param target Raster target
         * @param x Starting X position
         * @param y Y position (baseline)
         * @param callback Called for each glyph with position and metrics
         * @param user_data User data passed to callback
         * @return Width of rendered text
         */
        template<raster_target Target>
        float rasterize_text(std::string_view text, Target& target, int x, int y,
                             glyph_callback callback, void* user_data) const;

    private:
        font_source m_source;
        float m_size = 12.0f;
        render_style m_render_style;

        /**
         * @brief Get effective bold strength (auto-calculate if 0).
         */
        [[nodiscard]] int effective_bold_strength() const {
            if (m_render_style.bold_strength > 0) {
                return m_render_style.bold_strength;
            }
            return calc_bold_strength(m_size);
        }

        /**
         * @brief Rasterize a styled glyph (handles bold, italic).
         * @tparam Target Raster target type
         */
        template<raster_target Target>
        void rasterize_styled_glyph(char32_t codepoint, Target& target, int x, int y) const;

        /**
         * @brief Draw underline and strikethrough decorations.
         * @tparam Target Raster target type
         */
        template<raster_target Target>
        void draw_decorations(Target& target, int x, int y, float width) const;

    private:
        /**
         * @brief Draw decorations for a single glyph.
         *
         * Draws the decoration portion spanning this glyph's advance width.
         * @tparam Target Raster target type
         */
        template<raster_target Target>
        void draw_glyph_decorations(char32_t codepoint, Target& target, int x, int y) const;
    };

    // Template implementations

    template<raster_target Target>
    void text_rasterizer::rasterize_glyph(char32_t codepoint, Target& target,
                                          int x, int y) const {
        rasterize_styled_glyph(codepoint, target, x, y);
    }

    template<raster_target Target>
    float text_rasterizer::rasterize_text(std::string_view text, Target& target,
                                          int x, int y) const {
        return rasterize_text(text, target, x, y, nullptr, nullptr);
    }

    template<raster_target Target>
    float text_rasterizer::rasterize_text(std::string_view text, Target& target,
                                          int x, int y,
                                          glyph_callback callback, void* user_data) const {
        if (text.empty()) {
            return 0.0f;
        }

        float pen_x = static_cast<float>(x);
        char32_t prev_codepoint = 0;

        for (char32_t codepoint : utf8_view(text)) {
            // Apply kerning
            if (prev_codepoint != 0) {
                pen_x += m_source.get_kerning(prev_codepoint, codepoint, m_size);
            }

            // Get glyph metrics
            auto metrics = m_source.get_glyph_metrics(codepoint, m_size);

            // Rasterize styled glyph
            int glyph_x = static_cast<int>(std::round(pen_x));
            rasterize_styled_glyph(codepoint, target, glyph_x, y);

            // Invoke callback if provided
            if (callback) {
                callback(codepoint, glyph_x, y, metrics, user_data);
            }

            // Advance
            pen_x += metrics.advance_x;
            prev_codepoint = codepoint;
        }

        // Draw decorations (underline/strikethrough) for the full text width
        float total_width = pen_x - static_cast<float>(x);
        if (m_render_style.needs_decorations()) {
            draw_decorations(target, x, y, total_width);
        }

        return total_width;
    }

    template<raster_target Target>
    void text_rasterizer::rasterize_styled_glyph(char32_t codepoint, Target& target,
                                                  int x, int y) const {
        const font_source_type src_type = m_source.type();

        // For bitmap fonts, skip glyph transforms (only decorations apply)
        if (src_type == font_source_type::bitmap) {
            m_source.rasterize_glyph(codepoint, m_size, target, x, y);
            // Draw decorations for this glyph
            draw_glyph_decorations(codepoint, target, x, y);
            return;
        }

        // Check if any glyph transforms are needed
        if (!m_render_style.needs_glyph_transform()) {
            // No transforms, just render normally
            m_source.rasterize_glyph(codepoint, m_size, target, x, y);
            // Draw decorations for this glyph
            draw_glyph_decorations(codepoint, target, x, y);
            return;
        }

        // Vector fonts use direct stroke styling (thick lines + shear)
        if (src_type == font_source_type::vector) {
            m_source.rasterize_styled_glyph(codepoint, m_size, target, x, y, m_render_style);
            // Draw decorations for this glyph
            draw_glyph_decorations(codepoint, target, x, y);
            return;
        }

        // TTF fonts use FreeType's proper bold/italic synthesis
        if (src_type == font_source_type::outline) {
            m_source.rasterize_styled_glyph(codepoint, m_size, target, x, y, m_render_style);
            // Draw decorations for this glyph
            draw_glyph_decorations(codepoint, target, x, y);
            return;
        }

        // Fallback: regular rendering
        m_source.rasterize_glyph(codepoint, m_size, target, x, y);

        // Draw decorations for this glyph
        draw_glyph_decorations(codepoint, target, x, y);
    }

    /**
     * @brief Draw underline/strikethrough decorations for a single glyph.
     *
     * Draws the decoration portion for this glyph's advance width.
     * When glyphs are rendered consecutively, decorations form continuous lines.
     */
    template<raster_target Target>
    void text_rasterizer::draw_glyph_decorations(char32_t codepoint, Target& target,
                                                  int x, int y) const {
        if (!m_render_style.is_underlined() && !m_render_style.is_strikethrough()) {
            return;
        }

        // Get glyph advance width for decoration span
        auto glyph_m = m_source.get_glyph_metrics(codepoint, m_size);
        int advance = static_cast<int>(std::ceil(glyph_m.advance_x));
        if (advance <= 0) {
            return;
        }

        auto metrics = get_metrics();

        // Calculate thickness
        int thickness = m_render_style.underline_thickness;
        if (thickness <= 0) {
            thickness = calc_decoration_thickness(m_size);
        }

        // Draw underline
        if (m_render_style.is_underlined()) {
            int offset = m_render_style.underline_offset;
            if (offset == 0) {
                offset = calc_underline_offset(metrics.descent);
            }

            int line_y = y + offset;
            for (int t = 0; t < thickness; ++t) {
                for (int px = x; px < x + advance; ++px) {
                    target.put_pixel(px, line_y + t, 255);
                }
            }
        }

        // Draw strikethrough
        if (m_render_style.is_strikethrough()) {
            int offset = m_render_style.strikethrough_offset;
            if (offset == 0) {
                offset = calc_strikethrough_offset(metrics.ascent);
            }

            int line_y = y + offset;
            for (int t = 0; t < thickness; ++t) {
                for (int px = x; px < x + advance; ++px) {
                    target.put_pixel(px, line_y + t, 255);
                }
            }
        }
    }

    template<raster_target Target>
    void text_rasterizer::draw_decorations(Target& target, int x, int y, float width) const {
        if (width <= 0) {
            return;
        }

        auto metrics = get_metrics();
        int line_end = x + static_cast<int>(std::round(width));

        // Calculate thickness (auto if 0)
        int thickness = m_render_style.underline_thickness;
        if (thickness <= 0) {
            thickness = calc_decoration_thickness(m_size);
        }

        // Draw underline
        if (m_render_style.is_underlined()) {
            int offset = m_render_style.underline_offset;
            if (offset == 0) {
                offset = calc_underline_offset(metrics.descent);
            }

            int line_y = y + offset;
            for (int t = 0; t < thickness; ++t) {
                for (int px = x; px < line_end; ++px) {
                    target.put_pixel(px, line_y + t, 255);
                }
            }
        }

        // Draw strikethrough
        if (m_render_style.is_strikethrough()) {
            int st_thickness = m_render_style.strikethrough_thickness;
            if (st_thickness <= 0) {
                st_thickness = thickness; // Same as underline
            }

            int offset = m_render_style.strikethrough_offset;
            if (offset == 0) {
                offset = calc_strikethrough_offset(metrics.ascent);
            }

            int line_y = y + offset;
            for (int t = 0; t < st_thickness; ++t) {
                for (int px = x; px < line_end; ++px) {
                    target.put_pixel(px, line_y + t, 255);
                }
            }
        }
    }

} // namespace onyx_font
