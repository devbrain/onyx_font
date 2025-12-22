/**
 * @file raster_target.hh
 * @brief Raster target concept and implementations for text rendering.
 *
 * This file defines the raster_target concept and provides several
 * built-in target implementations for receiving rasterized glyph pixels.
 * Targets abstract the destination surface, allowing the same text
 * rendering code to work with different output formats.
 *
 * @section target_overview Overview
 *
 * A raster target is any type that can receive pixel data during
 * glyph rasterization. The concept requires three operations:
 * - put_pixel(x, y, alpha): Write a single pixel
 * - width(): Get target width
 * - height(): Get target height
 *
 * @section target_implementations Built-in Implementations
 *
 * | Class | Description |
 * |-------|-------------|
 * | grayscale_target | Write to 8-bit grayscale buffer (overwrite) |
 * | grayscale_max_target | Write to grayscale with max-blending |
 * | rgba_blend_target | Alpha-blend to 32-bit RGBA buffer |
 * | callback_target | Call user function for each pixel |
 * | null_target | Discard pixels (for measurement only) |
 * | owned_grayscale_target | Grayscale with owned buffer |
 *
 * @section target_usage Usage Examples
 *
 * @subsection target_grayscale Rendering to Grayscale Buffer
 *
 * @code{.cpp}
 * std::vector<uint8_t> buffer(width * height, 0);
 * grayscale_target target(buffer.data(), width, height);
 *
 * rasterizer.rasterize_text("Hello", target, 10, 30);
 * @endcode
 *
 * @subsection target_rgba Rendering to RGBA with Color
 *
 * @code{.cpp}
 * std::vector<uint32_t> pixels(width * height, 0);
 * rgba_blend_target target(pixels.data(), width, height, 0xFFFFFF);
 *
 * rasterizer.rasterize_text("Hello", target, 10, 30);
 * @endcode
 *
 * @subsection target_custom Custom Callback Target
 *
 * @code{.cpp}
 * auto target = callback_target(width, height,
 *     [](int x, int y, uint8_t alpha) {
 *         my_surface.blend_pixel(x, y, text_color, alpha);
 *     });
 *
 * rasterizer.rasterize_text("Hello", target, 10, 30);
 * @endcode
 *
 * @author Igor
 * @date 21/12/2025
 */

#pragma once

#include <onyx_font/export.h>
#include <concepts>
#include <cstdint>
#include <cstring>
#include <functional>
#include <algorithm>
#include <vector>

namespace onyx_font {
    // ============================================================================
    // Raster target concepts
    // ============================================================================

    /**
     * @brief Concept for types that can receive rasterized glyph pixels.
     *
     * A raster target provides a surface for writing glyph pixels during
     * text rendering. The minimum requirements are:
     * - put_pixel(x, y, alpha): Write a pixel at (x, y) with given alpha
     * - width(): Return the target width
     * - height(): Return the target height
     *
     * @tparam T Type to check against the concept
     *
     * @section concept_example Implementing a Custom Target
     *
     * @code{.cpp}
     * class my_target {
     * public:
     *     void put_pixel(int x, int y, uint8_t alpha) {
     *         // Write pixel to your surface
     *     }
     *     int width() const { return m_width; }
     *     int height() const { return m_height; }
     * };
     *
     * static_assert(raster_target<my_target>);
     * @endcode
     */
    template<typename T>
    concept raster_target = requires(T& target, int x, int y, uint8_t alpha)
    {
        { target.put_pixel(x, y, alpha) } -> std::same_as<void>;
        { target.width() } -> std::convertible_to<int>;
        { target.height() } -> std::convertible_to<int>;
    };

    /**
     * @brief Extended concept for targets with optimized horizontal span support.
     *
     * Targets that implement put_span can receive a horizontal run of
     * pixels more efficiently than individual put_pixel calls. This is
     * especially useful for TTF glyph bitmaps.
     *
     * @tparam T Type to check against the concept
     */
    template<typename T>
    concept raster_target_with_span = raster_target<T> &&
                                      requires(T& target, int x, int y, const uint8_t* alphas, int count)
                                      {
                                          { target.put_span(x, y, alphas, count) } -> std::same_as<void>;
                                      };

    // ============================================================================
    // Helper for emitting spans
    // ============================================================================

    /**
     * @brief Emit a horizontal span of pixels to a target.
     *
     * Uses put_span when the target supports it, otherwise falls back
     * to a put_pixel loop. This allows optimal performance without
     * requiring all targets to implement span support.
     *
     * @tparam Target Raster target type
     * @param target Target to write to
     * @param x Starting X coordinate
     * @param y Y coordinate
     * @param alphas Array of alpha values
     * @param count Number of pixels
     */
    template<raster_target Target>
    inline void emit_span(Target& target, int x, int y,
                          const uint8_t* alphas, int count) {
        if constexpr (raster_target_with_span<Target>) {
            target.put_span(x, y, alphas, count);
        } else {
            for (int i = 0; i < count; ++i) {
                if (alphas[i] > 0) {
                    target.put_pixel(x + i, y, alphas[i]);
                }
            }
        }
    }

    // ============================================================================
    // Built-in target implementations
    // ============================================================================

    /**
     * @brief Grayscale buffer target (single channel, 8-bit).
     *
     * Writes pixels directly to a grayscale buffer, overwriting
     * existing values. This is the simplest target implementation.
     *
     * @note Does not own the buffer - caller must ensure the buffer
     *       remains valid for the target's lifetime.
     *
     * @section grayscale_usage Usage
     *
     * @code{.cpp}
     * std::vector<uint8_t> buffer(640 * 480, 0);
     * grayscale_target target(buffer.data(), 640, 480);
     *
     * // Render text
     * rasterizer.rasterize_text("Hello", target, 10, 50);
     *
     * // Buffer now contains rendered text
     * @endcode
     */
    class ONYX_FONT_EXPORT grayscale_target {
    public:
        /**
         * @brief Construct target wrapping an existing buffer.
         *
         * @param buffer Pointer to buffer (must be at least stride * height bytes)
         * @param width Width in pixels
         * @param height Height in pixels
         * @param stride Bytes per row (defaults to width if 0)
         */
        grayscale_target(uint8_t* buffer, int width, int height, int stride = 0)
            : m_buffer(buffer)
              , m_width(width)
              , m_height(height)
              , m_stride(stride > 0 ? stride : width) {
        }

        /**
         * @brief Write a single pixel.
         *
         * @param x X coordinate
         * @param y Y coordinate
         * @param alpha Alpha value (0-255)
         */
        void put_pixel(int x, int y, uint8_t alpha) {
            if (x >= 0 && x < m_width && y >= 0 && y < m_height) {
                m_buffer[y * m_stride + x] = alpha;
            }
        }

        /**
         * @brief Write a horizontal span (optimized).
         *
         * @param x Starting X coordinate
         * @param y Y coordinate
         * @param alphas Array of alpha values
         * @param count Number of pixels
         */
        void put_span(int x, int y, const uint8_t* alphas, int count) {
            if (y < 0 || y >= m_height) return;
            int x0 = std::max(0, x);
            int x1 = std::min(m_width, x + count);
            if (x0 < x1) {
                std::memcpy(m_buffer + y * m_stride + x0,
                            alphas + (x0 - x),
                            static_cast<std::size_t>(x1 - x0));
            }
        }

        [[nodiscard]] int width() const noexcept { return m_width; }
        [[nodiscard]] int height() const noexcept { return m_height; }
        [[nodiscard]] int stride() const noexcept { return m_stride; }

        /// Get pointer to buffer
        [[nodiscard]] uint8_t* data() noexcept { return m_buffer; }
        [[nodiscard]] const uint8_t* data() const noexcept { return m_buffer; }

    private:
        uint8_t* m_buffer;
        int m_width;
        int m_height;
        int m_stride;
    };

    /**
     * @brief Grayscale target with max-blending.
     *
     * Keeps the maximum alpha value at each pixel, preventing
     * overlapping glyphs from darkening each other. Useful when
     * rendering text with kerning or overlapping characters.
     */
    class ONYX_FONT_EXPORT grayscale_max_target {
    public:
        /**
         * @brief Construct target with max-blending behavior.
         *
         * @param buffer Pointer to grayscale buffer
         * @param width Width in pixels
         * @param height Height in pixels
         * @param stride Bytes per row (defaults to width if 0)
         */
        grayscale_max_target(uint8_t* buffer, int width, int height, int stride = 0)
            : m_buffer(buffer)
              , m_width(width)
              , m_height(height)
              , m_stride(stride > 0 ? stride : width) {
        }

        /**
         * @brief Write a single pixel with max-blending.
         *
         * The resulting alpha is max(existing, new).
         *
         * @param x X coordinate
         * @param y Y coordinate
         * @param alpha Alpha value (0-255)
         */
        void put_pixel(int x, int y, uint8_t alpha) {
            if (x >= 0 && x < m_width && y >= 0 && y < m_height) {
                uint8_t& dst = m_buffer[y * m_stride + x];
                dst = std::max(dst, alpha);
            }
        }

        [[nodiscard]] int width() const noexcept { return m_width; }
        [[nodiscard]] int height() const noexcept { return m_height; }

    private:
        uint8_t* m_buffer;
        int m_width;
        int m_height;
        int m_stride;
    };

    /**
     * @brief Alpha-blending RGBA buffer target.
     *
     * Blends text with a specified color onto an RGBA buffer using
     * the Porter-Duff "over" compositing operation. This produces
     * proper antialiased text on any background.
     *
     * @section rgba_format Pixel Format
     *
     * The buffer uses 32-bit ARGB format (8 bits per channel):
     * - Bits 24-31: Alpha
     * - Bits 16-23: Red
     * - Bits 8-15: Green
     * - Bits 0-7: Blue
     *
     * @section rgba_usage Usage
     *
     * @code{.cpp}
     * std::vector<uint32_t> pixels(width * height, 0);
     *
     * // White text
     * rgba_blend_target target(pixels.data(), width, height, 0xFFFFFF);
     * rasterizer.rasterize_text("Hello", target, 10, 50);
     *
     * // Change color for next render
     * target.set_color(0xFF0000);  // Red
     * rasterizer.rasterize_text("World", target, 10, 80);
     * @endcode
     */
    class ONYX_FONT_EXPORT rgba_blend_target {
    public:
        /**
         * @brief Construct target wrapping an RGBA buffer.
         *
         * @param buffer Pointer to RGBA buffer (4 bytes per pixel)
         * @param width Width in pixels
         * @param height Height in pixels
         * @param color Text color (RGB, 0xRRGGBB)
         */
        rgba_blend_target(uint32_t* buffer, int width, int height, uint32_t color)
            : m_buffer(buffer)
              , m_width(width)
              , m_height(height)
              , m_color_r(static_cast<uint8_t>((color >> 16) & 0xFF))
              , m_color_g(static_cast<uint8_t>((color >> 8) & 0xFF))
              , m_color_b(static_cast<uint8_t>(color & 0xFF)) {
        }

        /**
         * @brief Blend a single pixel onto the buffer.
         *
         * Uses Porter-Duff "over" compositing with improved rounding.
         *
         * @param x X coordinate
         * @param y Y coordinate
         * @param alpha Glyph alpha value (0-255)
         */
        void put_pixel(int x, int y, uint8_t alpha) {
            if (x >= 0 && x < m_width && y >= 0 && y < m_height && alpha > 0) {
                uint32_t& dst = m_buffer[y * m_width + x];

                if (alpha == 255) {
                    // Fully opaque - direct write
                    dst = (255u << 24) | (static_cast<uint32_t>(m_color_r) << 16) |
                          (static_cast<uint32_t>(m_color_g) << 8) | m_color_b;
                } else {
                    // Alpha blend
                    auto dst_a = static_cast<uint8_t>((dst >> 24) & 0xFFu);
                    auto dst_r = static_cast<uint8_t>((dst >> 16) & 0xFFu);
                    auto dst_g = static_cast<uint8_t>((dst >> 8) & 0xFFu);
                    auto dst_b = static_cast<uint8_t>(dst & 0xFFu);

                    // Porter-Duff "over" operation with improved precision
                    uint32_t inv_alpha = 255 - alpha;
                    uint32_t out_a = alpha + (dst_a * inv_alpha + 127) / 255;
                    uint32_t out_r = (m_color_r * alpha + dst_r * inv_alpha + 127) / 255;
                    uint32_t out_g = (m_color_g * alpha + dst_g * inv_alpha + 127) / 255;
                    uint32_t out_b = (m_color_b * alpha + dst_b * inv_alpha + 127) / 255;

                    dst = (out_a << 24) | (out_r << 16) | (out_g << 8) | out_b;
                }
            }
        }

        [[nodiscard]] int width() const noexcept { return m_width; }
        [[nodiscard]] int height() const noexcept { return m_height; }

        /**
         * @brief Set the text color for subsequent rendering.
         * @param color RGB color (0xRRGGBB)
         */
        void set_color(uint32_t color) {
            m_color_r = static_cast<uint8_t>((color >> 16) & 0xFF);
            m_color_g = static_cast<uint8_t>((color >> 8) & 0xFF);
            m_color_b = static_cast<uint8_t>(color & 0xFF);
        }

    private:
        uint32_t* m_buffer;
        int m_width;
        int m_height;
        uint8_t m_color_r, m_color_g, m_color_b;
    };

    /**
     * @brief Callback-based target - calls user function for each pixel.
     *
     * Provides maximum flexibility by invoking a user-supplied callback
     * for each non-zero pixel. Useful for integrating with custom
     * rendering systems.
     *
     * @tparam Callback Callable type with signature void(int, int, uint8_t)
     *
     * @section callback_usage Usage
     *
     * @code{.cpp}
     * auto target = callback_target(640, 480,
     *     [&surface, color](int x, int y, uint8_t alpha) {
     *         surface.blend_pixel(x, y, color, alpha);
     *     });
     *
     * rasterizer.rasterize_text("Custom rendering!", target, 10, 50);
     * @endcode
     */
    template<typename Callback>
    class callback_target {
    public:
        /**
         * @brief Construct callback target.
         *
         * @param width Target width (for bounds checking)
         * @param height Target height (for bounds checking)
         * @param callback Function to call for each pixel
         */
        callback_target(int width, int height, Callback callback)
            : m_width(width)
              , m_height(height)
              , m_callback(std::move(callback)) {
        }

        /**
         * @brief Process a single pixel through the callback.
         *
         * @param x X coordinate
         * @param y Y coordinate
         * @param alpha Alpha value (0-255)
         */
        void put_pixel(int x, int y, uint8_t alpha) {
            if (x >= 0 && x < m_width && y >= 0 && y < m_height && alpha > 0) {
                m_callback(x, y, alpha);
            }
        }

        [[nodiscard]] int width() const noexcept { return m_width; }
        [[nodiscard]] int height() const noexcept { return m_height; }

    private:
        int m_width;
        int m_height;
        Callback m_callback;
    };

    /// Deduction guide for callback_target
    template<typename Callback>
    callback_target(int, int, Callback) -> callback_target<Callback>;

    /**
     * @brief Null target for measurement only.
     *
     * Discards all pixels, useful when you only need to measure
     * text extents without actually rendering. This is more efficient
     * than rendering to a real buffer.
     *
     * @section null_usage Usage
     *
     * @code{.cpp}
     * null_target target(9999, 9999);  // Infinite virtual size
     * float width = rasterizer.rasterize_text("Measure me", target, 0, 0);
     * @endcode
     */
    class ONYX_FONT_EXPORT null_target {
    public:
        /**
         * @brief Construct null target with given dimensions.
         *
         * @param width Virtual width
         * @param height Virtual height
         */
        null_target(int width, int height) noexcept
            : m_width(width), m_height(height) {
        }

        /// Discard pixel (no-op)
        void put_pixel(int, int, uint8_t) noexcept {
        }

        /// Discard span (no-op)
        void put_span(int, int, const uint8_t*, int) noexcept {
        }

        [[nodiscard]] int width() const noexcept { return m_width; }
        [[nodiscard]] int height() const noexcept { return m_height; }

    private:
        int m_width;
        int m_height;
    };

    /**
     * @brief Owned grayscale buffer target (manages its own memory).
     *
     * A grayscale target that allocates and owns its buffer.
     * Useful for temporary rendering or when you don't have
     * a pre-allocated buffer.
     *
     * @section owned_usage Usage
     *
     * @code{.cpp}
     * owned_grayscale_target target(256, 64);
     *
     * rasterizer.rasterize_text("Self-contained", target, 10, 40);
     *
     * // Access the data
     * const uint8_t* data = target.data();
     * save_to_file(data, target.width(), target.height());
     * @endcode
     */
    class ONYX_FONT_EXPORT owned_grayscale_target {
    public:
        /**
         * @brief Construct target with owned buffer.
         *
         * @param width Width in pixels
         * @param height Height in pixels
         */
        owned_grayscale_target(int width, int height)
            : m_buffer(static_cast<std::size_t>(width) * static_cast<std::size_t>(height), 0)
              , m_width(width)
              , m_height(height) {
        }

        /**
         * @brief Write a single pixel.
         *
         * @param x X coordinate
         * @param y Y coordinate
         * @param alpha Alpha value (0-255)
         */
        void put_pixel(int x, int y, uint8_t alpha) {
            if (x >= 0 && x < m_width && y >= 0 && y < m_height) {
                m_buffer[static_cast<std::size_t>(y) * static_cast<std::size_t>(m_width) +
                         static_cast<std::size_t>(x)] = alpha;
            }
        }

        /**
         * @brief Write a horizontal span (optimized).
         *
         * @param x Starting X coordinate
         * @param y Y coordinate
         * @param alphas Array of alpha values
         * @param count Number of pixels
         */
        void put_span(int x, int y, const uint8_t* alphas, int count) {
            if (y < 0 || y >= m_height) return;
            int x0 = std::max(0, x);
            int x1 = std::min(m_width, x + count);
            if (x0 < x1) {
                std::size_t offset = static_cast<std::size_t>(y) * static_cast<std::size_t>(m_width) +
                                     static_cast<std::size_t>(x0);
                std::memcpy(m_buffer.data() + offset,
                            alphas + (x0 - x),
                            static_cast<std::size_t>(x1 - x0));
            }
        }

        [[nodiscard]] int width() const noexcept { return m_width; }
        [[nodiscard]] int height() const noexcept { return m_height; }

        [[nodiscard]] uint8_t* data() noexcept { return m_buffer.data(); }
        [[nodiscard]] const uint8_t* data() const noexcept { return m_buffer.data(); }

        /// Clear buffer to zero
        void clear() noexcept {
            std::fill(m_buffer.begin(), m_buffer.end(), static_cast<uint8_t>(0));
        }

        /**
         * @brief Get pixel value at position.
         *
         * @param x X coordinate
         * @param y Y coordinate
         * @return Pixel value, or 0 if out of bounds
         */
        [[nodiscard]] uint8_t pixel(int x, int y) const noexcept {
            if (x >= 0 && x < m_width && y >= 0 && y < m_height) {
                return m_buffer[static_cast<std::size_t>(y) * static_cast<std::size_t>(m_width) +
                                static_cast<std::size_t>(x)];
            }
            return 0;
        }

    private:
        std::vector<uint8_t> m_buffer;
        int m_width;
        int m_height;
    };
} // namespace onyx_font
