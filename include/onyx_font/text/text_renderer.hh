/**
 * @file text_renderer.hh
 * @brief High-level text renderer with alignment and wrapping.
 *
 * This file provides the text_renderer class template, which builds
 * on glyph_cache to provide high-level text rendering features like
 * horizontal alignment, word wrapping, and baseline positioning.
 *
 * @section renderer_overview Overview
 *
 * text_renderer is the highest-level API for text rendering:
 * - Automatic glyph caching via glyph_cache
 * - Horizontal alignment (left, center, right)
 * - Word wrapping in bounding boxes
 * - Kerning between adjacent characters
 * - Callback-based rendering for GPU integration
 *
 * @section renderer_architecture Architecture
 *
 * @code
 *                    +----------------+
 *                    | text_renderer  |  <- High-level API
 *                    +-------+--------+
 *                            |
 *                            v
 *                    +-------+--------+
 *                    |  glyph_cache   |  <- Caching layer
 *                    +-------+--------+
 *                            |
 *                            v
 *                    +-------+--------+
 *                    | text_rasterizer|  <- Rasterization
 *                    +----------------+
 * @endcode
 *
 * @section renderer_usage Usage Examples
 *
 * @subsection renderer_basic Basic Text Drawing
 *
 * @code{.cpp}
 * ttf_font font(data);
 * glyph_cache<memory_atlas> cache(font_source::from_ttf(font), 24.0f);
 * text_renderer renderer(cache);
 *
 * // Draw text at position (using blit callback)
 * renderer.draw("Hello World", 100, 100,
 *     [](const memory_atlas& atlas, glyph_rect src, float x, float y) {
 *         // Copy from atlas to destination
 *         draw_sprite(atlas.data(), src.x, src.y, src.w, src.h, x, y);
 *     });
 * @endcode
 *
 * @subsection renderer_aligned Aligned Text
 *
 * @code{.cpp}
 * // Center text in a 300px wide area
 * renderer.draw_aligned("Centered", 100, 100, 300, text_align::center, blit);
 *
 * // Right-align
 * renderer.draw_aligned("Right", 100, 100, 300, text_align::right, blit);
 * @endcode
 *
 * @subsection renderer_wrapped Word-Wrapped Text
 *
 * @code{.cpp}
 * text_box box{100, 100, 200, 300};  // 200x300 text area
 *
 * int lines = renderer.draw_wrapped(
 *     "Long text that needs wrapping...",
 *     box,
 *     text_align::left,
 *     blit
 * );
 *
 * std::cout << "Rendered " << lines << " lines\n";
 * @endcode
 *
 * @author Igor
 * @date 21/12/2025
 */

#pragma once

#include <onyx_font/export.h>
#include <onyx_font/text/types.hh>
#include <onyx_font/text/glyph_cache.hh>
#include <onyx_font/text/utf8.hh>
#include <concepts>
#include <vector>
#include <string_view>

namespace onyx_font {
    /**
     * @brief Concept for blit callback functions.
     *
     * A blit callback receives atlas data and coordinates for
     * rendering a glyph from the atlas to the destination surface.
     *
     * @tparam F Callback type
     * @tparam Surface Atlas surface type
     *
     * The callback signature is:
     * `void callback(const Surface& atlas, glyph_rect src, float dst_x, float dst_y)`
     */
    template<typename F, typename Surface>
    concept blit_callback = requires(F func, const Surface& atlas,
                                     glyph_rect src, float dst_x, float dst_y)
    {
        { func(atlas, src, dst_x, dst_y) } -> std::same_as<void>;
    };

    /**
     * @brief High-level text renderer with caching, alignment, and wrapping.
     *
     * Provides a convenient API for rendering text using a glyph_cache.
     * Handles text layout, kerning, alignment, and word wrapping.
     *
     * @tparam Surface Atlas surface type (must satisfy atlas_surface concept)
     *
     * @section text_renderer_features Features
     *
     * - **Cached rendering**: Uses glyph_cache for efficient rendering
     * - **Alignment**: Left, center, or right alignment
     * - **Word wrapping**: Automatic line breaking at word boundaries
     * - **Kerning**: Proper spacing between character pairs
     * - **Flexible output**: Uses blit callback for GPU integration
     *
     * @section text_renderer_threading Thread Safety
     *
     * text_renderer is NOT thread-safe because it uses glyph_cache,
     * which may modify internal state when caching new glyphs.
     * External synchronization is required for multi-threaded use.
     */
    template<atlas_surface Surface>
    class text_renderer {
    public:
        /**
         * @brief Create renderer with glyph cache.
         *
         * @param cache Glyph cache to use (must outlive renderer)
         */
        explicit text_renderer(glyph_cache<Surface>& cache)
            : m_cache(&cache) {
        }

        /**
         * @brief Draw text at position.
         *
         * Renders text starting at the given position. The Y coordinate
         * is the top of the text, not the baseline.
         *
         * @tparam BlitFn Blit callback type
         * @param text UTF-8 text
         * @param x X position
         * @param y Y position (top of text, not baseline)
         * @param blit Callback to draw each glyph
         * @return Width of drawn text
         */
        template<blit_callback<Surface> BlitFn>
        float draw(std::string_view text, float x, float y, BlitFn&& blit) {
            if (text.empty()) return 0.0f;

            float baseline_y = y + m_cache->metrics().ascent;
            return draw_baseline(text, x, baseline_y, std::forward<BlitFn>(blit));
        }

        /**
         * @brief Draw text with horizontal alignment.
         *
         * Aligns text within a specified width. The text is positioned
         * according to the alignment setting.
         *
         * @tparam BlitFn Blit callback type
         * @param text UTF-8 text
         * @param x X position (left edge of alignment box)
         * @param y Y position (top of text)
         * @param width Width for alignment calculation
         * @param align Horizontal alignment
         * @param blit Callback to draw each glyph
         */
        template<blit_callback<Surface> BlitFn>
        void draw_aligned(std::string_view text, float x, float y,
                          float width, text_align align, BlitFn&& blit) {
            if (text.empty()) return;

            float text_width = m_cache->measure(text).width;
            float offset_x = 0.0f;

            switch (align) {
                case text_align::left:
                    offset_x = 0.0f;
                    break;
                case text_align::center:
                    offset_x = (width - text_width) / 2.0f;
                    break;
                case text_align::right:
                    offset_x = width - text_width;
                    break;
            }

            draw(text, x + offset_x, y, std::forward<BlitFn>(blit));
        }

        /**
         * @brief Draw word-wrapped text in a box.
         *
         * Wraps text at word boundaries to fit within the box width.
         * Lines that would exceed the box height are not drawn.
         *
         * @tparam BlitFn Blit callback type
         * @param text UTF-8 text
         * @param box Bounding box for text
         * @param align Horizontal alignment for each line
         * @param blit Callback to draw each glyph
         * @return Number of lines drawn
         */
        template<blit_callback<Surface> BlitFn>
        int draw_wrapped(std::string_view text, const text_box& box,
                         text_align align, BlitFn&& blit) {
            if (text.empty()) return 0;

            auto lines = wrap_lines(text, box.w);
            float line_h = m_cache->line_height();
            float current_y = box.y;
            int lines_drawn = 0;

            for (const auto& line : lines) {
                // Check if line fits in box
                if (current_y + line_h > box.y + box.h) {
                    break;
                }

                draw_aligned(line, box.x, current_y, box.w, align,
                             std::forward<BlitFn>(blit));

                current_y += line_h;
                ++lines_drawn;
            }

            return lines_drawn;
        }

        /**
         * @brief Draw text with baseline positioning.
         *
         * Renders text with Y as the baseline position. Use this
         * for precise typographic control.
         *
         * @tparam BlitFn Blit callback type
         * @param text UTF-8 text
         * @param x X position
         * @param y Baseline Y position
         * @param blit Callback to draw each glyph
         * @return Width of drawn text
         */
        template<blit_callback<Surface> BlitFn>
        float draw_baseline(std::string_view text, float x, float y, BlitFn&& blit) {
            if (text.empty()) return 0.0f;

            float pen_x = x;
            char32_t prev_codepoint = 0;

            for (char32_t codepoint : utf8_view(text)) {
                // Apply kerning between previous and current character
                if (prev_codepoint != 0) {
                    pen_x += m_cache->rasterizer().get_kerning(prev_codepoint, codepoint);
                }

                const auto& glyph = m_cache->get(codepoint);

                if (glyph.rect.w > 0 && glyph.rect.h > 0) {
                    // Calculate destination position
                    // bearing_x is offset from pen to left edge of glyph
                    // bearing_y is offset from baseline to top of glyph
                    float dst_x = pen_x + glyph.bearing_x;
                    float dst_y = y - glyph.bearing_y;

                    blit(m_cache->atlas(glyph.atlas_index),
                         glyph.rect, dst_x, dst_y);
                }

                // Advance pen by this glyph's advance
                pen_x += glyph.advance_x;
                prev_codepoint = codepoint;
            }

            return pen_x - x;
        }

        /**
         * @brief Measure text.
         *
         * @param text UTF-8 text
         * @return Text extents (width, height, ascent, descent)
         */
        [[nodiscard]] text_extents measure(std::string_view text) const {
            return m_cache->measure(text);
        }

        /**
         * @brief Measure wrapped text.
         *
         * @param text UTF-8 text
         * @param max_width Maximum line width
         * @return Extents of wrapped text
         */
        [[nodiscard]] text_extents measure_wrapped(std::string_view text, float max_width) const {
            return m_cache->rasterizer().measure_wrapped(text, max_width);
        }

        /**
         * @brief Get line height.
         * @return Line height in pixels
         */
        [[nodiscard]] float line_height() const {
            return m_cache->line_height();
        }

        /**
         * @brief Get font metrics.
         * @return Scaled font metrics
         */
        [[nodiscard]] scaled_metrics metrics() const {
            return m_cache->metrics();
        }

    private:
        glyph_cache<Surface>* m_cache;

        /**
         * @brief Word-wrap helper: split text into lines.
         *
         * Splits text at word boundaries (spaces) to fit within
         * the specified maximum width.
         *
         * @param text UTF-8 text to wrap
         * @param max_width Maximum line width
         * @return Vector of string_views for each line
         */
        [[nodiscard]] std::vector<std::string_view> wrap_lines(std::string_view text,
                                                                float max_width) const {
            std::vector<std::string_view> lines;

            if (text.empty() || max_width <= 0) {
                return lines;
            }

            const char* line_start = text.data();
            const char* word_start = text.data();
            const char* current = text.data();
            const char* text_end = text.data() + text.size();

            float line_width = 0.0f;
            float word_width = 0.0f;

            auto flush_line = [&]() {
                if (current > line_start) {
                    // Trim trailing whitespace
                    const char* line_end = current;
                    while (line_end > line_start && *(line_end - 1) == ' ') {
                        --line_end;
                    }
                    if (line_end > line_start) {
                        lines.emplace_back(line_start, static_cast<std::size_t>(line_end - line_start));
                    }
                }
                line_start = current;
                line_width = 0.0f;
            };

            while (current < text_end) {
                // Decode next codepoint
                auto [cp, bytes] = utf8_decode_one(
                    std::string_view(current, static_cast<std::size_t>(text_end - current)));
                if (bytes == 0) break;

                // Check for newline
                if (cp == '\n') {
                    current += bytes;
                    flush_line();
                    line_start = current;
                    word_start = current;
                    word_width = 0.0f;
                    continue;
                }

                // Get glyph advance
                float advance = m_cache->rasterizer().measure_glyph(cp).advance_x;

                // Check for word boundary (space)
                if (cp == ' ') {
                    word_start = current + bytes;
                    word_width = 0.0f;
                }

                // Check if we need to wrap
                if (line_width + advance > max_width && line_width > 0) {
                    // Try to break at word boundary
                    if (word_start > line_start && word_start <= current) {
                        // Wrap at word boundary
                        const char* saved_current = current;
                        current = word_start;
                        flush_line();
                        current = saved_current;
                        line_start = word_start;
                        line_width = word_width;
                    } else {
                        // No word boundary, break at current position
                        flush_line();
                        line_start = current;
                        line_width = 0.0f;
                    }
                }

                word_width += advance;
                line_width += advance;
                current += bytes;
            }

            // Flush remaining text
            if (current > line_start) {
                lines.emplace_back(line_start, static_cast<std::size_t>(current - line_start));
            }

            return lines;
        }
    };
} // namespace onyx_font
