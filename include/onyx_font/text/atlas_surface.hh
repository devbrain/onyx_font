/**
 * @file atlas_surface.hh
 * @brief Atlas surface concept and implementations for glyph caching.
 *
 * This file defines the atlas_surface concept and provides a memory-based
 * implementation. Atlas surfaces store rasterized glyphs for efficient
 * real-time rendering, typically as textures in GPU-accelerated applications.
 *
 * @section atlas_overview Overview
 *
 * A texture atlas is a large image containing multiple glyphs packed together.
 * Instead of rasterizing glyphs on demand, the glyph_cache pre-rasterizes them
 * to an atlas and renders by copying from the atlas to the destination.
 *
 * @section atlas_concept The Concept
 *
 * An atlas_surface must support:
 * - Construction with (width, height)
 * - width() and height() queries
 * - write_alpha(x, y, w, h, pixels, stride) for writing glyph data
 *
 * @section atlas_usage Usage
 *
 * @code{.cpp}
 * // Create an atlas
 * memory_atlas atlas(512, 512);
 *
 * // Write some data to it
 * std::vector<uint8_t> glyph_data(16 * 16, 128);
 * atlas.write_alpha(10, 10, 16, 16, glyph_data.data(), 16);
 *
 * // Access the data
 * uint8_t pixel = atlas.pixel(15, 15);
 * @endcode
 *
 * @section atlas_custom Custom Implementation
 *
 * For GPU rendering, implement an atlas that uploads to texture:
 *
 * @code{.cpp}
 * class gl_atlas {
 * public:
 *     gl_atlas(int width, int height);
 *     int width() const;
 *     int height() const;
 *     void write_alpha(int x, int y, int w, int h,
 *                      const uint8_t* pixels, int stride) {
 *         // Upload to OpenGL texture
 *         glTextureSubImage2D(texture, 0, x, y, w, h,
 *                             GL_RED, GL_UNSIGNED_BYTE, pixels);
 *     }
 *     GLuint texture_id() const;
 * };
 * @endcode
 *
 * @author Igor
 * @date 21/12/2025
 */

#pragma once

#include <onyx_font/export.h>
#include <concepts>
#include <cstdint>
#include <vector>
#include <cstring>

namespace onyx_font {
    /**
     * @brief Concept for atlas surface types.
     *
     * Atlas surfaces store rasterized glyphs for efficient rendering.
     * They must support construction with dimensions and writing of
     * alpha data to rectangular regions.
     *
     * @tparam T Type to check against the concept
     *
     * @section atlas_concept_requirements Requirements
     *
     * - `T(int, int)` - Constructible with width and height
     * - `surface.width()` - Returns width as convertible to int
     * - `surface.height()` - Returns height as convertible to int
     * - `surface.write_alpha(x, y, w, h, pixels, stride)` - Writes alpha data
     */
    template<typename T>
    concept atlas_surface = requires(T& surface, int x, int y, int w, int h,
                                     const uint8_t* pixels, int stride)
    {
        { T(int{}, int{}) };
        { surface.width() } -> std::convertible_to<int>;
        { surface.height() } -> std::convertible_to<int>;
        { surface.write_alpha(x, y, w, h, pixels, stride) } -> std::same_as<void>;
    };

    /**
     * @brief Simple in-memory atlas surface.
     *
     * Stores glyphs in a CPU-side buffer. Suitable for testing,
     * software rendering, or as a staging area before GPU upload.
     *
     * @section memory_atlas_usage Usage
     *
     * @code{.cpp}
     * // Create 512x512 atlas
     * memory_atlas atlas(512, 512);
     *
     * // Write a glyph
     * atlas.write_alpha(0, 0, glyph.width, glyph.height,
     *                   glyph.data, glyph.stride);
     *
     * // Access data for GPU upload
     * glTexImage2D(GL_TEXTURE_2D, 0, GL_R8,
     *              atlas.width(), atlas.height(), 0,
     *              GL_RED, GL_UNSIGNED_BYTE, atlas.data());
     * @endcode
     */
    class ONYX_FONT_EXPORT memory_atlas {
    public:
        /**
         * @brief Construct atlas with given dimensions.
         *
         * @param width Atlas width in pixels
         * @param height Atlas height in pixels
         * @pre width >= 0 && height >= 0
         */
        memory_atlas(int width, int height)
            : m_width(width)
              , m_height(height)
              , m_data(static_cast<std::size_t>(width) * static_cast<std::size_t>(height), 0) {
        }

        /**
         * @brief Get atlas width.
         * @return Width in pixels
         */
        [[nodiscard]] int width() const noexcept { return m_width; }

        /**
         * @brief Get atlas height.
         * @return Height in pixels
         */
        [[nodiscard]] int height() const noexcept { return m_height; }

        /**
         * @brief Write alpha data to a region of the atlas.
         *
         * Copies glyph pixel data into the atlas at the specified position.
         * The source data is row-major, 8-bit alpha values.
         *
         * @param x X position in atlas (left edge)
         * @param y Y position in atlas (top edge)
         * @param w Width of region to write
         * @param h Height of region to write
         * @param pixels Source pixel data (row-major, 8-bit alpha)
         * @param stride Source row stride in bytes
         */
        void write_alpha(int x, int y, int w, int h,
                         const uint8_t* pixels, int stride) {
            if (!pixels || w <= 0 || h <= 0) return;

            for (int row = 0; row < h; ++row) {
                int dst_y = y + row;
                if (dst_y < 0 || dst_y >= m_height) continue;

                int src_offset = row * stride;

                int copy_start = 0;
                int copy_end = w;

                // Clip to atlas bounds
                if (x < 0) {
                    copy_start = -x;
                }
                if (x + w > m_width) {
                    copy_end = m_width - x;
                }

                if (copy_start < copy_end) {
                    std::size_t dst_offset = static_cast<std::size_t>(dst_y) *
                                             static_cast<std::size_t>(m_width) +
                                             static_cast<std::size_t>(x + copy_start);
                    std::memcpy(
                        m_data.data() + dst_offset,
                        pixels + src_offset + copy_start,
                        static_cast<std::size_t>(copy_end - copy_start)
                    );
                }
            }
        }

        /**
         * @brief Access pixel data (read-only).
         * @return Pointer to atlas data (row-major, 8-bit alpha)
         */
        [[nodiscard]] const uint8_t* data() const noexcept { return m_data.data(); }

        /**
         * @brief Access pixel data (read-write).
         * @return Pointer to atlas data
         */
        [[nodiscard]] uint8_t* data() noexcept { return m_data.data(); }

        /**
         * @brief Get pixel at position.
         *
         * @param x X coordinate
         * @param y Y coordinate
         * @return Pixel value, or 0 if out of bounds
         */
        [[nodiscard]] uint8_t pixel(int x, int y) const noexcept {
            if (x >= 0 && x < m_width && y >= 0 && y < m_height) {
                return m_data[static_cast<std::size_t>(y) * static_cast<std::size_t>(m_width) +
                              static_cast<std::size_t>(x)];
            }
            return 0;
        }

        /**
         * @brief Clear the atlas to zero.
         */
        void clear() noexcept {
            std::fill(m_data.begin(), m_data.end(), static_cast<uint8_t>(0));
        }

    private:
        int m_width;
        int m_height;
        std::vector<uint8_t> m_data;
    };

    // Verify memory_atlas satisfies atlas_surface concept
    static_assert(atlas_surface<memory_atlas>);
} // namespace onyx_font
