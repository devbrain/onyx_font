/**
 * @file glyph_cache.hh
 * @brief Glyph cache with texture atlas for efficient real-time rendering.
 *
 * This file provides the glyph_cache class template, which manages a
 * texture atlas of pre-rasterized glyphs. This is the preferred approach
 * for GPU-accelerated text rendering where rasterizing on-the-fly is
 * too slow.
 *
 * @section cache_overview Overview
 *
 * The glyph cache provides:
 * - Automatic rasterization on first access
 * - Efficient atlas packing with row-based algorithm
 * - Multiple atlas pages when needed
 * - Pre-caching for ASCII and custom character sets
 * - Thread safety notes for multi-threaded applications
 *
 * @section cache_architecture Architecture
 *
 * @code
 * +-------------------+     +------------------+
 * | font_source       | --> | text_rasterizer  |
 * +-------------------+     +------------------+
 *                                   |
 *                                   v
 *                           +-------+--------+
 *                           |  glyph_cache   |
 *                           +-------+--------+
 *                                   |
 *              +--------------------+--------------------+
 *              |                    |                    |
 *              v                    v                    v
 *         +--------+           +--------+           +--------+
 *         | Atlas 0|           | Atlas 1|           | Atlas N|
 *         +--------+           +--------+           +--------+
 * @endcode
 *
 * @section cache_usage Usage Examples
 *
 * @subsection cache_create Creating a Cache
 *
 * @code{.cpp}
 * ttf_font font(data);
 * auto source = font_source::from_ttf(font);
 *
 * glyph_cache_config config;
 * config.atlas_size = 1024;  // Larger texture
 * config.pre_cache_ascii = true;
 *
 * glyph_cache<memory_atlas> cache(std::move(source), 24.0f, config);
 * @endcode
 *
 * @subsection cache_render Rendering Text
 *
 * @code{.cpp}
 * for (char32_t cp : utf8_view(text)) {
 *     const auto& glyph = cache.get(cp);
 *
 *     // Get source rectangle in atlas
 *     const auto& atlas = cache.atlas(glyph.atlas_index);
 *     glyph_rect src = glyph.rect;
 *
 *     // Draw (e.g., with GPU quad)
 *     draw_textured_quad(atlas.data(), src, dst_x, dst_y);
 *
 *     dst_x += glyph.advance_x;
 * }
 * @endcode
 *
 * @author Igor
 * @date 21/12/2025
 */

#pragma once

#include <onyx_font/export.h>
#include <onyx_font/text/types.hh>
#include <onyx_font/text/text_rasterizer.hh>
#include <onyx_font/text/atlas_surface.hh>
#include <onyx_font/text/utf8.hh>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <stdexcept>

namespace onyx_font {
    /**
     * @brief Information about a cached glyph.
     *
     * Contains all the data needed to render a glyph from the atlas:
     * position in atlas, bearings for positioning, and advance width.
     */
    struct ONYX_FONT_EXPORT cached_glyph {
        int atlas_index = 0;   ///< Index of atlas containing this glyph
        glyph_rect rect;       ///< Position and size within the atlas
        float bearing_x = 0;   ///< Left side bearing (pen to glyph left edge)
        float bearing_y = 0;   ///< Top side bearing (baseline to glyph top)
        float advance_x = 0;   ///< Horizontal advance to next glyph
    };

    /**
     * @brief Configuration for glyph cache.
     *
     * Controls atlas size, padding between glyphs, and pre-caching behavior.
     */
    struct ONYX_FONT_EXPORT glyph_cache_config {
        /**
         * @brief Atlas texture size (square).
         *
         * Larger atlases mean fewer texture switches but more memory.
         * Common values: 256, 512, 1024, 2048.
         */
        int atlas_size = 512;

        /**
         * @brief Pixels of padding between glyphs.
         *
         * Prevents texture bleeding when using bilinear filtering.
         * 1 pixel is usually sufficient; use 2 for extra safety.
         */
        int padding = 1;

        /**
         * @brief Pre-cache ASCII characters (32-126) on creation.
         *
         * Set to true for English/ASCII text to avoid cache misses
         * during rendering. Set to false for non-ASCII or when you
         * want to control pre-caching manually.
         */
        bool pre_cache_ascii = true;
    };

    /**
     * @brief Glyph cache with texture atlas.
     *
     * Automatically rasterizes and caches glyphs on first access.
     * Provides efficient lookup for subsequent accesses using a
     * hash map from codepoint to cached glyph data.
     *
     * @tparam Surface Atlas surface type (must satisfy atlas_surface concept)
     *
     * @warning This class is NOT thread-safe. The get() method modifies
     *          internal state (caches new glyphs). If used from multiple
     *          threads, external synchronization is required.
     *
     * @section glyph_cache_example Complete Example
     *
     * @code{.cpp}
     * // Create cache
     * ttf_font font(data);
     * glyph_cache<memory_atlas> cache(
     *     font_source::from_ttf(font),
     *     24.0f,  // 24px
     *     {.atlas_size = 512, .pre_cache_ascii = true}
     * );
     *
     * // Pre-cache additional characters
     * cache.cache_string("éàü");  // Extended Latin
     * cache.cache_range(0x4E00, 0x4E20);  // Some CJK
     *
     * // Render text
     * float x = 10.0f;
     * for (char32_t cp : utf8_view("Hello")) {
     *     const auto& g = cache.get(cp);
     *     // Draw g.rect from cache.atlas(g.atlas_index) at (x + g.bearing_x, y - g.bearing_y)
     *     x += g.advance_x;
     * }
     * @endcode
     */
    template<atlas_surface Surface>
    class glyph_cache {
    public:
        /**
         * @brief Create cache for a font at a specific size.
         *
         * @param source Font source to rasterize from (moves ownership)
         * @param size Pixel height for rasterization
         * @param config Cache configuration
         */
        glyph_cache(font_source source, float size,
                    glyph_cache_config config = {})
            : m_rasterizer(std::move(source))
              , m_config(config) {
            m_rasterizer.set_size(size);

            // Create first atlas
            add_atlas();

            // Pre-cache ASCII if requested
            if (m_config.pre_cache_ascii) {
                cache_range(32, 126);
            }
        }

        /**
         * @brief Get cached glyph (rasterizes and caches if not present).
         *
         * Returns information about the glyph, rasterizing it to the
         * atlas if not already cached.
         *
         * @param codepoint Unicode codepoint
         * @return Reference to cached glyph info
         *
         * @warning Not thread-safe. May modify internal state.
         */
        const cached_glyph& get(char32_t codepoint) {
            auto it = m_cache.find(codepoint);
            if (it != m_cache.end()) {
                return it->second;
            }
            return cache_glyph(codepoint);
        }

        /**
         * @brief Check if glyph is already cached.
         *
         * @param codepoint Unicode codepoint
         * @return true if glyph is in cache
         */
        [[nodiscard]] bool is_cached(char32_t codepoint) const noexcept {
            return m_cache.find(codepoint) != m_cache.end();
        }

        /**
         * @brief Pre-cache a range of characters.
         *
         * @param first First codepoint (inclusive)
         * @param last Last codepoint (inclusive)
         */
        void cache_range(char32_t first, char32_t last) {
            for (char32_t cp = first; cp <= last; ++cp) {
                if (!is_cached(cp)) {
                    cache_glyph(cp);
                }
            }
        }

        /**
         * @brief Pre-cache all characters in a string.
         *
         * @param utf8_text UTF-8 encoded text
         */
        void cache_string(std::string_view utf8_text) {
            for (char32_t cp : utf8_view(utf8_text)) {
                if (!is_cached(cp)) {
                    cache_glyph(cp);
                }
            }
        }

        /**
         * @brief Get number of atlas pages.
         * @return Number of atlas surfaces
         */
        [[nodiscard]] int atlas_count() const noexcept {
            return static_cast<int>(m_atlases.size());
        }

        /**
         * @brief Get atlas surface by index.
         *
         * @param index Atlas index (0 to atlas_count() - 1)
         * @return Reference to atlas surface
         * @throws std::out_of_range if index is invalid
         */
        [[nodiscard]] const Surface& atlas(int index) const {
            if (index < 0 || static_cast<std::size_t>(index) >= m_atlases.size()) {
                throw std::out_of_range("atlas index out of range");
            }
            return m_atlases[static_cast<std::size_t>(index)];
        }

        /**
         * @brief Get the underlying rasterizer.
         *
         * Useful for measurement without accessing cached glyphs.
         *
         * @return Reference to text rasterizer
         */
        [[nodiscard]] const text_rasterizer& rasterizer() const noexcept {
            return m_rasterizer;
        }

        /**
         * @brief Measure text (delegates to rasterizer).
         *
         * @param text UTF-8 encoded text
         * @return Text extents
         */
        [[nodiscard]] text_extents measure(std::string_view text) const {
            return m_rasterizer.measure_text(text);
        }

        /**
         * @brief Get font metrics.
         * @return Scaled font metrics
         */
        [[nodiscard]] scaled_metrics metrics() const noexcept {
            return m_rasterizer.get_metrics();
        }

        /**
         * @brief Get line height.
         * @return Line height in pixels
         */
        [[nodiscard]] float line_height() const noexcept {
            return m_rasterizer.line_height();
        }

    private:
        text_rasterizer m_rasterizer;
        glyph_cache_config m_config;
        std::vector<Surface> m_atlases;
        std::unordered_map<char32_t, cached_glyph> m_cache;

        // Packing state for current atlas
        int m_pack_x = 0;
        int m_pack_y = 0;
        int m_row_height = 0;

        /// Add a new atlas page
        void add_atlas() {
            m_atlases.emplace_back(m_config.atlas_size, m_config.atlas_size);
            m_pack_x = m_config.padding;
            m_pack_y = m_config.padding;
            m_row_height = 0;
        }

        /// Rasterize and cache a single glyph
        cached_glyph& cache_glyph(char32_t codepoint) {
            // Get glyph metrics
            auto metrics = m_rasterizer.measure_glyph(codepoint);

            // Validate and clamp dimensions
            int glyph_w = static_cast<int>(std::ceil(metrics.width));
            int glyph_h = static_cast<int>(std::ceil(metrics.height));

            // Clamp to reasonable bounds
            glyph_w = std::clamp(glyph_w, 0, m_config.atlas_size);
            glyph_h = std::clamp(glyph_h, 0, m_config.atlas_size);

            // Handle zero-size glyphs (like space)
            if (glyph_w <= 0 || glyph_h <= 0) {
                cached_glyph glyph;
                glyph.atlas_index = 0;
                glyph.rect = {0, 0, 0, 0};
                glyph.bearing_x = metrics.bearing_x;
                glyph.bearing_y = metrics.bearing_y;
                glyph.advance_x = metrics.advance_x;
                return m_cache.emplace(codepoint, glyph).first->second;
            }

            int padded_w = glyph_w + m_config.padding;
            int padded_h = glyph_h + m_config.padding;

            // Check if glyph fits in current row
            if (m_pack_x + padded_w > m_config.atlas_size) {
                // Move to next row
                m_pack_x = m_config.padding;
                m_pack_y += m_row_height + m_config.padding;
                m_row_height = 0;
            }

            // Check if we need a new atlas
            if (m_pack_y + padded_h > m_config.atlas_size) {
                add_atlas();
            }

            int atlas_index = static_cast<int>(m_atlases.size()) - 1;
            int glyph_x = m_pack_x;
            int glyph_y = m_pack_y;

            // Rasterize glyph to temporary buffer
            std::vector<uint8_t> buffer(
                static_cast<std::size_t>(glyph_w) * static_cast<std::size_t>(glyph_h), 0);
            grayscale_target target(buffer.data(), glyph_w, glyph_h);

            // Rasterize at position (0, bearing_y) so glyph is at top of buffer
            int baseline_y = static_cast<int>(std::ceil(metrics.bearing_y));
            m_rasterizer.rasterize_glyph(codepoint, target, 0, baseline_y);

            // Copy to atlas
            m_atlases[static_cast<std::size_t>(atlas_index)].write_alpha(
                glyph_x, glyph_y, glyph_w, glyph_h,
                buffer.data(), glyph_w
            );

            // Update packing state
            m_pack_x += padded_w;
            m_row_height = std::max(m_row_height, padded_h);

            // Create cached glyph entry
            cached_glyph glyph;
            glyph.atlas_index = atlas_index;
            glyph.rect = {glyph_x, glyph_y, glyph_w, glyph_h};
            glyph.bearing_x = metrics.bearing_x;
            glyph.bearing_y = metrics.bearing_y;
            glyph.advance_x = metrics.advance_x;

            return m_cache.emplace(codepoint, glyph).first->second;
        }
    };
} // namespace onyx_font
