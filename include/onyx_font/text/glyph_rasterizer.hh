/**
 * @file glyph_rasterizer.hh
 * @brief Low-level glyph rasterization using euler library.
 *
 * This file provides internal rasterization functions for bitmap
 * and vector fonts. It uses the euler library for line rasterization
 * (Bresenham and Wu's algorithm) for vector fonts.
 *
 * @section raster_overview Overview
 *
 * The rasterization functions are templated on the target type,
 * allowing them to work with any surface that satisfies the
 * raster_target concept.
 *
 * @section raster_modes Rasterization Modes
 *
 * For vector fonts, two modes are available:
 * - **aliased**: Fast, 1-bit output using Bresenham's algorithm
 * - **antialiased**: Smooth edges using Wu's line algorithm
 *
 * @section raster_internal Internal Use
 *
 * These functions are primarily used by font_source and not intended
 * for direct use. The higher-level APIs (text_rasterizer, glyph_cache)
 * provide more convenient interfaces.
 *
 * @note TTF font rasterization is handled by font_source using FreeType.
 *
 * @author Igor
 * @date 21/12/2025
 */

#pragma once

#include <onyx_font/export.h>
#include <onyx_font/text/raster_target.hh>
#include <onyx_font/bitmap_font.hh>
#include <onyx_font/vector_font.hh>
#include <euler/dda/line_iterator.hh>
#include <euler/dda/aa_line_iterator.hh>
#include <euler/coordinates/point2.hh>
#include <cmath>

namespace onyx_font::internal {
    /**
     * @brief Rasterization mode for vector fonts.
     *
     * Controls the quality/speed tradeoff for vector font rendering.
     */
    enum class raster_mode {
        /**
         * @brief Fast, 1-bit output (Bresenham's algorithm).
         *
         * Produces hard pixel edges. Fast but may look jagged
         * at larger sizes or on high-DPI displays.
         */
        aliased,

        /**
         * @brief Smooth, coverage-based output (Wu's algorithm).
         *
         * Produces antialiased lines with alpha gradients.
         * Slower but looks better, especially at larger sizes.
         */
        antialiased
    };

    // ============================================================================
    // Bitmap font rasterization
    // ============================================================================

    /**
     * @brief Rasterize a bitmap glyph to a target.
     *
     * Copies 1-bit bitmap pixels to the target as 0 or 255 alpha values.
     *
     * @tparam Target Raster target type
     * @param glyph Bitmap glyph view
     * @param target Raster target
     * @param x X offset in target
     * @param y Y offset in target
     */
    template<raster_target Target>
    void rasterize_bitmap_glyph(const bitmap_view& glyph,
                                Target& target, int x, int y) {
        for (uint16_t gy = 0; gy < glyph.height(); ++gy) {
            for (uint16_t gx = 0; gx < glyph.width(); ++gx) {
                if (glyph.pixel(gx, gy)) {
                    target.put_pixel(x + gx, y + gy, 255);
                }
            }
        }
    }

    // ============================================================================
    // Vector font rasterization
    // ============================================================================

    /**
     * @brief Rasterize a vector glyph (stroke-based) to a target.
     *
     * Executes the glyph's stroke commands using the euler library
     * for line rasterization. Supports both aliased and antialiased modes.
     *
     * @tparam Target Raster target type
     * @param glyph Vector glyph to rasterize
     * @param scale Scale factor (desired_size / native_height)
     * @param target Raster target
     * @param x X offset in target
     * @param y Y offset in target (baseline)
     * @param mode Rasterization mode (aliased or antialiased)
     */
    template<raster_target Target>
    void rasterize_vector_glyph(const vector_glyph& glyph, float scale,
                                Target& target, int x, int y,
                                raster_mode mode = raster_mode::antialiased) {
        float pen_x = static_cast<float>(x);
        float pen_y = static_cast<float>(y);
        float last_x = pen_x;
        float last_y = pen_y;
        // Start with pen down - some fonts start with LINE_TO from origin
        bool pen_down = true;

        for (const auto& cmd : glyph.strokes) {
            float dx = static_cast<float>(cmd.dx) * scale;
            float dy = static_cast<float>(cmd.dy) * scale;

            switch (cmd.type) {
                case stroke_type::MOVE_TO:
                    pen_x += dx;
                    pen_y += dy;
                    last_x = pen_x;
                    last_y = pen_y;
                    pen_down = true;
                    break;

                case stroke_type::LINE_TO: {
                    float new_x = pen_x + dx;
                    float new_y = pen_y + dy;

                    if (pen_down) {
                        if (mode == raster_mode::antialiased) {
                            // Use euler's Wu's algorithm for antialiased lines
                            euler::point2f start{last_x, last_y};
                            euler::point2f end{new_x, new_y};

                            auto line = euler::dda::make_aa_line_iterator(start, end);
                            while (line != euler::dda::aa_line_iterator<float>::end()) {
                                auto pixel = *line;
                                uint8_t alpha = static_cast<uint8_t>(
                                    std::clamp(pixel.coverage * 255.0f, 0.0f, 255.0f));
                                if (alpha > 0) {
                                    target.put_pixel(static_cast<int>(pixel.pos.x),
                                                     static_cast<int>(pixel.pos.y),
                                                     alpha);
                                }
                                ++line;
                            }
                        } else {
                            // Use euler's Bresenham for aliased lines
                            euler::point2i start{
                                static_cast<int>(std::round(last_x)),
                                static_cast<int>(std::round(last_y))
                            };
                            euler::point2i end{
                                static_cast<int>(std::round(new_x)),
                                static_cast<int>(std::round(new_y))
                            };

                            for (auto pixel : euler::dda::line_pixels(start, end)) {
                                target.put_pixel(pixel.pos.x, pixel.pos.y, 255);
                            }
                        }
                    }

                    pen_x = new_x;
                    pen_y = new_y;
                    last_x = pen_x;
                    last_y = pen_y;
                    break;
                }

                case stroke_type::END:
                    pen_down = false;
                    break;
            }
        }
    }

} // namespace onyx_font::internal
