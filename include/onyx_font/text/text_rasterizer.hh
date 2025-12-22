/**
 * @file text_rasterizer.hh
 * @brief Low-level text measurement and rasterization.
 *
 * This file provides the text_rasterizer class for measuring and
 * rendering text strings. It handles UTF-8 decoding, glyph metrics,
 * kerning, and direct rasterization to targets.
 *
 * @section rasterizer_overview Overview
 *
 * text_rasterizer provides the core text rendering functionality:
 * - Text measurement (single line, wrapped)
 * - Glyph rasterization with proper positioning
 * - Kerning between adjacent characters
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
 * @author Igor
 * @date 21/12/2025
 */

#pragma once

#include <onyx_font/export.h>
#include <onyx_font/text/types.hh>
#include <onyx_font/text/font_source.hh>
#include <onyx_font/text/raster_target.hh>
#include <onyx_font/text/utf8.hh>
#include <string_view>
#include <cmath>

namespace onyx_font {
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
    };

    // Template implementations

    template<raster_target Target>
    void text_rasterizer::rasterize_glyph(char32_t codepoint, Target& target,
                                          int x, int y) const {
        m_source.rasterize_glyph(codepoint, m_size, target, x, y);
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

            // Rasterize glyph
            int glyph_x = static_cast<int>(std::round(pen_x));
            m_source.rasterize_glyph(codepoint, m_size, target, glyph_x, y);

            // Invoke callback if provided
            if (callback) {
                callback(codepoint, glyph_x, y, metrics, user_data);
            }

            // Advance
            pen_x += metrics.advance_x;
            prev_codepoint = codepoint;
        }

        return pen_x - static_cast<float>(x);
    }
} // namespace onyx_font
