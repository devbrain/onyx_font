/**
 * @file bitmap_glyphs_storage.hh
 * @brief Packed 1-bit bitmap storage for font glyph data.
 *
 * This file provides efficient storage and access for monochrome (1-bit per pixel)
 * glyph bitmaps. The system uses a builder pattern for construction and provides
 * immutable storage with lightweight views for reading.
 *
 * @section storage_overview Overview
 *
 * The bitmap storage system consists of three main components:
 *
 * | Class | Purpose |
 * |-------|---------|
 * | bitmap_view | Read-only view into a single glyph's bitmap |
 * | bitmap_storage | Immutable container holding all glyphs |
 * | bitmap_builder | Mutable builder for constructing storage |
 *
 * @section storage_memory Memory Layout
 *
 * Glyphs are stored as packed 1-bit bitmaps in a contiguous byte blob:
 *
 * @code
 *   +--------+--------+--------+--------+
 *   | Glyph0 | Glyph1 | Glyph2 | ...    |  <- m_blob (contiguous bytes)
 *   +--------+--------+--------+--------+
 *
 *   Each glyph:
 *   +---+---+---+---+---+---+---+---+  <- byte 0 (row 0)
 *   | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |  <- bit positions (MSB first)
 *   +---+---+---+---+---+---+---+---+
 *   +---+---+---+---+---+---+---+---+  <- byte 1 (row 1)
 *   | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
 *   +---+---+---+---+---+---+---+---+
 *   ...
 * @endcode
 *
 * @section storage_bit_order Bit Order
 *
 * Two bit orderings are supported:
 * - **msb_first**: Pixel x=0 is bit 7 of first byte (standard for BIOS fonts)
 * - **lsb_first**: Pixel x=0 is bit 0 of first byte (some hardware formats)
 *
 * @section storage_usage Usage Examples
 *
 * @subsection storage_build Building Storage
 *
 * @code{.cpp}
 * // Create a builder
 * bitmap_builder builder(bit_order::msb_first);
 * builder.reserve_glyphs(256);
 *
 * // Add glyphs using the writer proxy
 * for (int ch = 0; ch < 256; ++ch) {
 *     auto writer = builder.reserve_glyph(8, 16);
 *     for (int y = 0; y < 16; ++y) {
 *         for (int x = 0; x < 8; ++x) {
 *             if (should_be_set(ch, x, y)) {
 *                 writer.set_pixel(x, y);
 *             }
 *         }
 *     }
 * }
 *
 * // Finalize to immutable storage
 * bitmap_storage storage = std::move(builder).build();
 * @endcode
 *
 * @subsection storage_read Reading Glyphs
 *
 * @code{.cpp}
 * // Get a view for a specific glyph
 * bitmap_view glyph = storage.view(65);  // 'A'
 *
 * // Read individual pixels
 * for (uint16_t y = 0; y < glyph.height(); ++y) {
 *     for (uint16_t x = 0; x < glyph.width(); ++x) {
 *         if (glyph.pixel(x, y)) {
 *             draw_pixel(pen_x + x, pen_y + y, color);
 *         }
 *     }
 * }
 *
 * // Or use packed row access for faster blitting
 * for (uint16_t y = 0; y < glyph.height(); ++y) {
 *     auto row = glyph.row(y);
 *     blit_packed_row(pen_x, pen_y + y, row, glyph.width());
 * }
 * @endcode
 *
 * @author Igor
 * @date 14/12/2025
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include <onyx_font/export.h>

namespace onyx_font {
    /**
     * @brief Bit packing order within bitmap bytes.
     *
     * Controls how pixels map to bits within each byte of the packed bitmap.
     * This affects both reading and writing operations.
     *
     * @section bit_order_example Visual Example
     *
     * For an 8-pixel wide glyph row with pixels: `##....##`
     *
     * @code
     * msb_first: byte = 0b11000011 = 0xC3
     *            bit:   76543210
     *            pixel: 01234567
     *
     * lsb_first: byte = 0b11000011 = 0xC3
     *            bit:   01234567
     *            pixel: 01234567
     * @endcode
     */
    enum class bit_order : std::uint8_t {
        /**
         * @brief Most significant bit first (standard).
         *
         * Pixel x=0 maps to bit 7 (highest bit) of the first byte.
         * This is the standard format for BIOS fonts and most font files.
         */
        msb_first,

        /**
         * @brief Least significant bit first.
         *
         * Pixel x=0 maps to bit 0 (lowest bit) of the first byte.
         * Used by some hardware formats and display controllers.
         */
        lsb_first
    };

    /**
     * @brief Dimensions of a glyph bitmap.
     *
     * Simple struct holding width and height in pixels.
     * Used by bitmap_storage::dimensions() to query glyph sizes
     * without creating a full bitmap_view.
     */
    struct ONYX_FONT_EXPORT glyph_dimensions {
        std::uint16_t width = 0;   ///< Glyph width in pixels
        std::uint16_t height = 0;  ///< Glyph height in pixels
    };

    /**
     * @brief Read-only view into a single glyph's packed bitmap data.
     *
     * bitmap_view provides efficient access to glyph pixel data without
     * copying. It supports both individual pixel queries and fast row-based
     * access for optimized blitting.
     *
     * @section view_features Features
     *
     * - Lightweight (just a pointer + dimensions)
     * - Individual pixel access via pixel()
     * - Packed row access via row() for fast blitting
     * - Respects the storage's bit order
     *
     * @section view_pixel Pixel Access
     *
     * @code{.cpp}
     * bitmap_view glyph = storage.view(ch);
     *
     * // Individual pixel access
     * for (uint16_t y = 0; y < glyph.height(); ++y) {
     *     for (uint16_t x = 0; x < glyph.width(); ++x) {
     *         if (glyph.pixel(x, y)) {
     *             set_pixel(x, y);
     *         }
     *     }
     * }
     * @endcode
     *
     * @section view_row Row Access
     *
     * For performance-critical rendering, use packed row access:
     *
     * @code{.cpp}
     * for (uint16_t y = 0; y < glyph.height(); ++y) {
     *     auto row_bytes = glyph.row(y);
     *     // Process packed bytes directly
     *     for (size_t i = 0; i < row_bytes.size(); ++i) {
     *         uint8_t byte = static_cast<uint8_t>(row_bytes[i]);
     *         // Unpack 8 pixels at once...
     *     }
     * }
     * @endcode
     *
     * @warning bitmap_view does not own its data. The underlying
     *          bitmap_storage must remain valid while the view is in use.
     */
    class ONYX_FONT_EXPORT bitmap_view {
    public:
        /**
         * @brief Construct an empty/invalid view.
         *
         * Creates a view with zero dimensions pointing to no data.
         */
        bitmap_view();

        /**
         * @brief Get the glyph width in pixels.
         * @return Width in pixels
         */
        [[nodiscard]] std::uint16_t width() const noexcept;

        /**
         * @brief Get the glyph height in pixels.
         * @return Height in pixels (number of rows)
         */
        [[nodiscard]] std::uint16_t height() const noexcept;

        /**
         * @brief Get the number of bytes per row.
         *
         * Each row is packed into ceil(width/8) bytes.
         * This value is pre-computed for efficiency.
         *
         * @return Bytes per row (stride)
         */
        [[nodiscard]] std::uint16_t stride_bytes() const noexcept;

        /**
         * @brief Get the bit ordering used in this bitmap.
         * @return Bit order (msb_first or lsb_first)
         */
        [[nodiscard]] bit_order order() const noexcept;

        /**
         * @brief Get packed bytes for a single row.
         *
         * Returns a span of stride_bytes() bytes containing the packed
         * pixel data for the specified row. This is the fastest way
         * to access glyph data for blitting.
         *
         * @param y Row index (0 to height-1)
         * @return Span of packed bytes for the row
         *
         * @note Does not perform bounds checking on y.
         */
        [[nodiscard]] std::span<const std::byte> row(std::uint16_t y) const noexcept;

        /**
         * @brief Test if a specific pixel is set.
         *
         * Returns true if the pixel at (x, y) is "on" (foreground/ink).
         * Handles bit order automatically.
         *
         * @param x X coordinate (0 to width-1)
         * @param y Y coordinate (0 to height-1)
         * @return true if pixel is set, false otherwise
         * @throws std::out_of_range if coordinates are out of bounds
         */
        [[nodiscard]] bool pixel(std::uint16_t x, std::uint16_t y) const;

    private:
        friend class bitmap_storage;

        bitmap_view(std::span<const std::byte> bytes,
                    std::uint16_t w,
                    std::uint16_t h,
                    std::uint16_t stride,
                    bit_order order) noexcept;

        std::span<const std::byte> m_data{};
        std::uint16_t m_width = 0;
        std::uint16_t m_height = 0;
        std::uint16_t m_stride = 0;
        bit_order m_order = bit_order::msb_first;
    };

    /**
     * @brief Immutable storage for multiple packed glyph bitmaps.
     *
     * bitmap_storage holds all glyph data for a font in a contiguous memory
     * block. It's constructed via bitmap_builder and provides read-only
     * access through bitmap_view objects.
     *
     * @section storage_design Design
     *
     * The storage uses a single contiguous byte array (blob) containing all
     * glyph bitmaps concatenated together. A separate index tracks each
     * glyph's offset, dimensions, and stride within the blob.
     *
     * @code
     *   m_blob: [glyph0 bytes][glyph1 bytes][glyph2 bytes]...
     *   m_glyphs: [{offset=0, w=8, h=16}, {offset=16, w=8, h=16}, ...]
     * @endcode
     *
     * @section storage_access Accessing Glyphs
     *
     * @code{.cpp}
     * // Query dimensions without creating a view
     * auto dims = storage.dimensions(65);  // 'A'
     * std::cout << "Glyph is " << dims.width << "x" << dims.height << "\n";
     *
     * // Get a view for rendering
     * bitmap_view glyph = storage.view(65);
     * render_glyph(glyph);
     * @endcode
     *
     * @note bitmap_storage is move-only. It owns its data and provides
     *       views that reference into it.
     *
     * @see bitmap_builder For constructing storage
     * @see bitmap_view For accessing glyph pixel data
     */
    class ONYX_FONT_EXPORT bitmap_storage {
    public:
        /**
         * @brief Construct empty storage.
         *
         * Creates storage with no glyphs. Use bitmap_builder to
         * populate storage with glyph data.
         */
        bitmap_storage();

        /**
         * @brief Get the bit ordering used in this storage.
         * @return Bit order (msb_first or lsb_first)
         */
        [[nodiscard]] bit_order order() const noexcept;

        /**
         * @brief Get the number of glyphs in storage.
         * @return Number of glyphs
         */
        [[nodiscard]] std::size_t glyph_count() const noexcept;

        /**
         * @brief Get dimensions for a glyph without creating a view.
         *
         * Lightweight query for glyph size when you don't need
         * pixel data access.
         *
         * @param glyph_index Index of the glyph (0 to glyph_count-1)
         * @return Dimensions (width, height) in pixels
         *
         * @note Returns zero dimensions for out-of-range indices.
         */
        [[nodiscard]] glyph_dimensions dimensions(std::size_t glyph_index) const noexcept;

        /**
         * @brief Get a read-only view for a glyph.
         *
         * Returns a bitmap_view that provides access to the glyph's
         * packed pixel data. The view is valid as long as this
         * storage object exists.
         *
         * @param glyph_index Index of the glyph (0 to glyph_count-1)
         * @return View into the glyph's bitmap data
         *
         * @note Returns an empty view for out-of-range indices.
         */
        [[nodiscard]] bitmap_view view(std::size_t glyph_index) const noexcept;

        /**
         * @brief Get raw access to the underlying byte blob.
         *
         * Returns a span of the entire contiguous memory block
         * containing all glyph data. Useful for serialization
         * or low-level access.
         *
         * @return Span of all glyph bitmap bytes
         */
        [[nodiscard]] std::span<const std::byte> blob_bytes() const noexcept;

    private:
        friend class bitmap_builder;

        /// @cond INTERNAL
        struct glyph_internal {
            std::size_t offset = 0;     ///< Byte offset into m_blob
            std::uint16_t width = 0;    ///< Glyph width in pixels
            std::uint16_t height = 0;   ///< Glyph height in pixels
            std::uint16_t stride = 0;   ///< Bytes per row
        };
        /// @endcond

        bit_order m_order = bit_order::msb_first;
        std::vector<std::byte> m_blob;
        std::vector<glyph_internal> m_glyphs;
    };

    /**
     * @brief Mutable builder for constructing bitmap_storage.
     *
     * bitmap_builder provides a flexible API for populating glyph bitmap
     * data before finalizing it into an immutable bitmap_storage. It supports
     * multiple methods for adding glyphs: pixel-by-pixel, packed rows, or
     * via callback functions.
     *
     * @section builder_workflow Typical Workflow
     *
     * @code{.cpp}
     * // 1. Create builder with desired bit order
     * bitmap_builder builder(bit_order::msb_first);
     *
     * // 2. Optionally reserve space for efficiency
     * builder.reserve_glyphs(256);
     * builder.reserve_bytes(256 * 8 * 16);  // 8x16 font
     *
     * // 3. Add glyphs using preferred method
     * for (int ch = 0; ch < 256; ++ch) {
     *     auto writer = builder.reserve_glyph(8, 16);
     *     // ... populate pixels ...
     * }
     *
     * // 4. Finalize to immutable storage
     * bitmap_storage storage = std::move(builder).build();
     * @endcode
     *
     * @section builder_methods Adding Glyphs
     *
     * Three methods are available:
     *
     * 1. **reserve_glyph()** - Returns a glyph_writer proxy for pixel-by-pixel
     *    or row-by-row writing. Most flexible.
     *
     * 2. **add_glyph_packed()** - Copies pre-packed row data directly.
     *    Fastest when data is already in the correct format.
     *
     * 3. **add_glyph_from_fn()** - Uses a callback to query each pixel.
     *    Convenient for adapting from other representations.
     *
     * @see bitmap_storage For the immutable result
     * @see glyph_writer For the pixel writing proxy
     */
    class ONYX_FONT_EXPORT bitmap_builder {
    public:
        /**
         * @brief Construct a builder with specified bit order.
         *
         * @param order Bit packing order for all glyphs (default: msb_first)
         */
        explicit bitmap_builder(bit_order order = bit_order::msb_first);

        /**
         * @brief Get the bit ordering used by this builder.
         * @return Bit order (msb_first or lsb_first)
         */
        [[nodiscard]] bit_order order() const noexcept;

        /**
         * @brief Reserve space in the byte blob to reduce reallocations.
         *
         * Call this if you know the approximate total size of all
         * glyph data to avoid incremental vector growth.
         *
         * @param bytes Number of bytes to reserve
         */
        void reserve_bytes(std::size_t bytes);

        /**
         * @brief Reserve space in the glyph index to reduce reallocations.
         *
         * @param n Number of glyphs to reserve space for
         */
        void reserve_glyphs(std::size_t n);

        /**
         * @brief Calculate packed row stride for a given width.
         *
         * Returns the number of bytes needed per row for a glyph
         * of the specified width: `(width + 7) / 8`.
         *
         * @param width Glyph width in pixels
         * @return Bytes per row (stride)
         */
        static constexpr std::uint16_t packed_stride(std::uint16_t width) noexcept;

        /**
         * @brief Move-only proxy for writing pixels to a reserved glyph.
         *
         * glyph_writer provides methods for setting individual pixels or
         * entire rows in a glyph that was reserved with reserve_glyph().
         * The writer holds raw pointers into the builder's internal storage.
         *
         * @warning The writer becomes invalid when:
         * - The parent builder is moved
         * - build() is called on the builder
         * - Any operation causes vector reallocation (reserve_glyph, add_glyph_*)
         *
         * Best practice: Use the writer immediately after reserve_glyph()
         * returns, then let it go out of scope.
         *
         * @section writer_usage Usage
         *
         * @code{.cpp}
         * auto writer = builder.reserve_glyph(8, 16);
         *
         * // Set pixels individually
         * writer.set_pixel(0, 0, true);  // Top-left corner
         * writer.set_pixel(7, 15, true); // Bottom-right corner
         *
         * // Or set entire packed rows
         * std::byte row_data[1] = {std::byte{0xFF}};
         * writer.set_row_packed(0, row_data);
         *
         * // Writer goes out of scope here
         * @endcode
         */
        class ONYX_FONT_EXPORT glyph_writer {
        public:
            /// @cond
            glyph_writer() = delete;
            glyph_writer(const glyph_writer&) = delete;
            glyph_writer& operator=(const glyph_writer&) = delete;
            glyph_writer(glyph_writer&&) noexcept = default;
            glyph_writer& operator=(glyph_writer&&) noexcept = default;
            /// @endcond

            /**
             * @brief Get the index of this glyph in the storage.
             * @return Zero-based glyph index
             */
            [[nodiscard]] std::size_t glyph_index() const noexcept;

            /**
             * @brief Get the glyph width in pixels.
             * @return Width in pixels
             */
            [[nodiscard]] std::uint16_t width() const noexcept;

            /**
             * @brief Get the glyph height in pixels.
             * @return Height in pixels
             */
            [[nodiscard]] std::uint16_t height() const noexcept;

            /**
             * @brief Get the number of bytes per row.
             * @return Stride in bytes
             */
            [[nodiscard]] std::uint16_t stride_bytes() const noexcept;

            /**
             * @brief Get the bit ordering.
             * @return Bit order (msb_first or lsb_first)
             */
            [[nodiscard]] bit_order order() const noexcept;

            /**
             * @brief Set or clear a pixel.
             *
             * Sets the pixel at (x, y) to the specified state.
             * Respects the bit order of the storage.
             *
             * @param x X coordinate (0 to width-1)
             * @param y Y coordinate (0 to height-1)
             * @param on true to set (ink), false to clear (background)
             *
             * @note Bounds are checked in debug builds only.
             */
            void set_pixel(std::uint16_t x, std::uint16_t y, bool on = true) noexcept;

            /**
             * @brief Clear a pixel (set to background).
             *
             * Convenience method equivalent to set_pixel(x, y, false).
             *
             * @param x X coordinate
             * @param y Y coordinate
             */
            void clear_pixel(std::uint16_t x, std::uint16_t y) noexcept;

            /**
             * @brief Write an entire row from packed data.
             *
             * Copies packed row data directly. This is the fastest way
             * to populate a glyph when the source data is already in
             * the correct packed format.
             *
             * @param y Row index (0 to height-1)
             * @param row Packed row data (must be exactly stride_bytes() long)
             * @throws std::invalid_argument if row size doesn't match stride
             */
            void set_row_packed(std::uint16_t y, std::span<const std::byte> row);

        private:
            friend class bitmap_builder;

            glyph_writer(std::vector<std::byte>* blob,
                         std::size_t glyph_index,
                         std::size_t offset,
                         std::uint16_t w,
                         std::uint16_t h,
                         std::uint16_t stride,
                         bit_order order) noexcept;

            void set_bit(std::uint16_t x, std::uint16_t y, bool on) noexcept;

            std::vector<std::byte>* m_blob = nullptr;
            std::size_t m_glyph_index = 0;
            std::size_t m_offset = 0;
            std::uint16_t m_width = 0;
            std::uint16_t m_height = 0;
            std::uint16_t m_stride = 0;
            bit_order m_order = bit_order::msb_first;
        };

        /**
         * @brief Reserve space for a glyph and get a writer to populate it.
         *
         * Allocates space in the byte blob for a glyph of the specified
         * dimensions (initialized to all zeros/background) and returns
         * a glyph_writer proxy for setting pixels.
         *
         * @param width Glyph width in pixels
         * @param height Glyph height in pixels
         * @return Writer proxy for populating the glyph
         *
         * @code{.cpp}
         * auto writer = builder.reserve_glyph(8, 16);
         * for (int y = 0; y < 16; ++y) {
         *     for (int x = 0; x < 8; ++x) {
         *         if (font_data[ch][y] & (0x80 >> x)) {
         *             writer.set_pixel(x, y);
         *         }
         *     }
         * }
         * @endcode
         */
        [[nodiscard]] glyph_writer reserve_glyph(std::uint16_t width, std::uint16_t height);

        /**
         * @brief Add a glyph from pre-packed row data.
         *
         * Copies the provided packed bitmap data directly. The data
         * must be in the correct format: height rows of stride bytes each,
         * with bits ordered according to this builder's bit_order.
         *
         * @param width Glyph width in pixels
         * @param height Glyph height in pixels
         * @param rows Packed bitmap data (height * stride bytes)
         * @return Index of the added glyph
         *
         * @code{.cpp}
         * // Add glyph from external packed data
         * std::span<const std::byte> packed_data = get_packed_glyph(ch);
         * size_t idx = builder.add_glyph_packed(8, 16, packed_data);
         * @endcode
         */
        [[nodiscard]] std::size_t add_glyph_packed(std::uint16_t width,
                                                   std::uint16_t height,
                                                   std::span<const std::byte> rows);

        /**
         * @brief Add a glyph using a pixel callback function.
         *
         * Creates a glyph by calling the provided function for each pixel
         * position. The function should return true if the pixel is set.
         *
         * @tparam PixelOnFn Callable type: bool(uint16_t x, uint16_t y)
         * @param width Glyph width in pixels
         * @param height Glyph height in pixels
         * @param pixel_on Callback returning true if pixel (x,y) should be set
         * @return Index of the added glyph
         *
         * @code{.cpp}
         * // Add glyph from a 2D array
         * bool pixels[16][8] = { ... };
         * size_t idx = builder.add_glyph_from_fn(8, 16,
         *     [&](uint16_t x, uint16_t y) { return pixels[y][x]; });
         *
         * // Add glyph from a bitmap_view
         * bitmap_view src = other_storage.view(ch);
         * size_t idx = builder.add_glyph_from_fn(src.width(), src.height(),
         *     [&](uint16_t x, uint16_t y) { return src.pixel(x, y); });
         * @endcode
         */
        template<class PixelOnFn>
        [[nodiscard]] std::size_t add_glyph_from_fn(std::uint16_t width,
                                                    std::uint16_t height,
                                                    PixelOnFn&& pixel_on) {
            auto w = reserve_glyph(width, height);
            for (std::uint16_t y = 0; y < height; ++y) {
                for (std::uint16_t x = 0; x < width; ++x) {
                    if (static_cast<bool>(pixel_on(x, y))) {
                        w.set_pixel(x, y, true);
                    }
                }
            }
            return w.glyph_index();
        }

        /**
         * @brief Finalize the builder into immutable storage.
         *
         * Transfers ownership of the accumulated glyph data to a new
         * bitmap_storage object. The builder is left in a moved-from
         * state and should not be used afterward.
         *
         * @return Immutable bitmap_storage containing all added glyphs
         *
         * @note This method consumes the builder (rvalue reference).
         *
         * @code{.cpp}
         * bitmap_storage storage = std::move(builder).build();
         * // builder is now empty/invalid
         * @endcode
         */
        [[nodiscard]] bitmap_storage build() &&;

    private:
        bit_order m_order;
        std::vector<std::byte> m_blob;
        std::vector<bitmap_storage::glyph_internal> m_glyphs;
    };

    constexpr std::uint16_t bitmap_builder::packed_stride(std::uint16_t width) noexcept {
        return static_cast <std::uint16_t>((width + 7u) / 8u);
    }
}
