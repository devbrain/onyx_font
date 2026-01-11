/**
 * @file decoder.hh
 * @brief Abstract base classes for font format decoders.
 *
 * This file provides the extension API for adding custom font format support
 * to onyx_font without modifying the library source.
 *
 * @section decoder_overview Overview
 *
 * Three decoder base classes are provided, one for each font type:
 * - bitmap_font_decoder - For raster/bitmap font formats
 * - vector_font_decoder - For stroke-based vector font formats
 * - outline_font_decoder - For Bezier outline fonts (TTF-style)
 *
 * @section decoder_usage Adding a Custom Format
 *
 * 1. Inherit from the appropriate decoder base class
 * 2. Implement the required virtual methods
 * 3. Register with font_registry::instance().register_decoder()
 *
 * @section decoder_example Example: PSF Font Decoder
 *
 * @code{.cpp}
 * #include <onyx_font/decoder.hh>
 * #include <onyx_font/font_registry.hh>
 *
 * class psf_decoder : public onyx_font::bitmap_font_decoder {
 * public:
 *     std::string_view name() const noexcept override {
 *         return "psf";
 *     }
 *
 *     std::span<const std::string_view> extensions() const noexcept override {
 *         static constexpr std::string_view ext[] = {".psf", ".psfu"};
 *         return ext;
 *     }
 *
 *     bool sniff(std::span<const std::uint8_t> data) const noexcept override {
 *         if (data.size() < 4) return false;
 *         // PSF2 magic
 *         return data[0] == 0x72 && data[1] == 0xb5 &&
 *                data[2] == 0x4a && data[3] == 0x86;
 *     }
 *
 *     std::vector<font_entry> enumerate(
 *         std::span<const std::uint8_t> data) const override {
 *         // Parse header, return font entries
 *     }
 *
 *     bitmap_font load(std::span<const std::uint8_t> data,
 *                      std::size_t index) const override {
 *         // Parse and return bitmap_font
 *     }
 * };
 *
 * // Register at startup
 * void init_custom_fonts() {
 *     onyx_font::font_registry::instance()
 *         .register_decoder(std::make_unique<psf_decoder>());
 * }
 * @endcode
 */

#pragma once

#include <cstdint>
#include <memory>
#include <span>
#include <string_view>
#include <vector>

#include <onyx_font/export.h>
#include <onyx_font/bitmap_font.hh>
#include <onyx_font/vector_font.hh>
#include <onyx_font/ttf_font.hh>

namespace onyx_font {

    // Forward declaration
    struct font_entry;

    /**
     * @brief Abstract base class for bitmap font format decoders.
     *
     * Implement this class to add support for a bitmap/raster font format.
     * Bitmap fonts store glyphs as fixed-size pixel arrays.
     *
     * Examples of bitmap formats: Windows FNT, GEM, PSF, BDF, PCF, raw BIOS.
     */
    class ONYX_FONT_EXPORT bitmap_font_decoder {
    public:
        virtual ~bitmap_font_decoder() = default;

        /**
         * @brief Short identifier for this format.
         *
         * Used for lookup by name and logging. Should be lowercase.
         * Examples: "gem", "psf", "bdf", "win_fnt"
         *
         * @return Format identifier
         */
        [[nodiscard]] virtual std::string_view name() const noexcept = 0;

        /**
         * @brief File extensions associated with this format.
         *
         * Extensions should include the leading dot and be lowercase.
         * Examples: {".psf", ".psfu"}, {".fnt", ".fon"}
         *
         * @return Span of extension strings
         */
        [[nodiscard]] virtual std::span<const std::string_view> extensions() const noexcept = 0;

        /**
         * @brief Quick format detection by examining file data.
         *
         * Check magic bytes or header structure to determine if this
         * decoder can handle the data. Should be fast - avoid full parsing.
         *
         * @param data Raw file data
         * @return true if this decoder recognizes the format
         */
        [[nodiscard]] virtual bool sniff(std::span<const std::uint8_t> data) const noexcept = 0;

        /**
         * @brief Enumerate fonts in the container.
         *
         * Parse the container structure and return metadata for each
         * font without fully decoding glyph data.
         *
         * @param data Raw file data
         * @return Vector of font entries describing available fonts
         * @throws std::runtime_error on parse error
         *
         * @pre sniff(data) returned true
         */
        [[nodiscard]] virtual std::vector<font_entry> enumerate(
            std::span<const std::uint8_t> data) const = 0;

        /**
         * @brief Load a bitmap font by index.
         *
         * Fully decode and return the font at the specified index.
         *
         * @param data Raw file data
         * @param index Font index within container (0-based)
         * @return Loaded bitmap font
         * @throws std::runtime_error on parse error or invalid index
         *
         * @pre sniff(data) returned true
         * @pre index < enumerate(data).size()
         */
        [[nodiscard]] virtual bitmap_font load(
            std::span<const std::uint8_t> data,
            std::size_t index) const = 0;

        /**
         * @brief Load the first bitmap font from the container.
         *
         * Convenience overload equivalent to load(data, 0).
         *
         * @param data Raw file data
         * @return Loaded bitmap font
         */
        [[nodiscard]] bitmap_font load(std::span<const std::uint8_t> data) const {
            return load(data, 0);
        }

        /**
         * @brief Load all bitmap fonts from the container.
         *
         * Default implementation calls load() for each entry from enumerate().
         * Override for more efficient bulk loading.
         *
         * @param data Raw file data
         * @return Vector of all bitmap fonts
         */
        [[nodiscard]] virtual std::vector<bitmap_font> load_all(
            std::span<const std::uint8_t> data) const;
    };

    /**
     * @brief Abstract base class for vector font format decoders.
     *
     * Implement this class to add support for a stroke-based vector font format.
     * Vector fonts store glyphs as sequences of drawing commands (move, line).
     *
     * Examples of vector formats: Borland BGI, Windows vector FON, Hershey fonts.
     */
    class ONYX_FONT_EXPORT vector_font_decoder {
    public:
        virtual ~vector_font_decoder() = default;

        /**
         * @brief Short identifier for this format.
         * @return Format identifier
         */
        [[nodiscard]] virtual std::string_view name() const noexcept = 0;

        /**
         * @brief File extensions associated with this format.
         * @return Span of extension strings
         */
        [[nodiscard]] virtual std::span<const std::string_view> extensions() const noexcept = 0;

        /**
         * @brief Quick format detection by examining file data.
         * @param data Raw file data
         * @return true if this decoder recognizes the format
         */
        [[nodiscard]] virtual bool sniff(std::span<const std::uint8_t> data) const noexcept = 0;

        /**
         * @brief Enumerate fonts in the container.
         * @param data Raw file data
         * @return Vector of font entries describing available fonts
         */
        [[nodiscard]] virtual std::vector<font_entry> enumerate(
            std::span<const std::uint8_t> data) const = 0;

        /**
         * @brief Load a vector font by index.
         * @param data Raw file data
         * @param index Font index within container (0-based)
         * @return Loaded vector font
         */
        [[nodiscard]] virtual vector_font load(
            std::span<const std::uint8_t> data,
            std::size_t index) const = 0;

        /**
         * @brief Load the first vector font from the container.
         * @param data Raw file data
         * @return Loaded vector font
         */
        [[nodiscard]] vector_font load(std::span<const std::uint8_t> data) const {
            return load(data, 0);
        }

        /**
         * @brief Load all vector fonts from the container.
         * @param data Raw file data
         * @return Vector of all vector fonts
         */
        [[nodiscard]] virtual std::vector<vector_font> load_all(
            std::span<const std::uint8_t> data) const;
    };

    /**
     * @brief Abstract base class for outline font format decoders.
     *
     * Implement this class to add support for a Bezier outline font format.
     * Outline fonts store glyphs as curves that can be scaled to any size.
     *
     * Examples of outline formats: TrueType, OpenType, WOFF, Type 1.
     */
    class ONYX_FONT_EXPORT outline_font_decoder {
    public:
        virtual ~outline_font_decoder() = default;

        /**
         * @brief Short identifier for this format.
         * @return Format identifier
         */
        [[nodiscard]] virtual std::string_view name() const noexcept = 0;

        /**
         * @brief File extensions associated with this format.
         * @return Span of extension strings
         */
        [[nodiscard]] virtual std::span<const std::string_view> extensions() const noexcept = 0;

        /**
         * @brief Quick format detection by examining file data.
         * @param data Raw file data
         * @return true if this decoder recognizes the format
         */
        [[nodiscard]] virtual bool sniff(std::span<const std::uint8_t> data) const noexcept = 0;

        /**
         * @brief Enumerate fonts in the container.
         * @param data Raw file data
         * @return Vector of font entries describing available fonts
         */
        [[nodiscard]] virtual std::vector<font_entry> enumerate(
            std::span<const std::uint8_t> data) const = 0;

        /**
         * @brief Load an outline font by index.
         *
         * @param data Raw file data (must remain valid for font lifetime)
         * @param index Font index within container (0-based)
         * @return Loaded outline font
         *
         * @warning The data span must remain valid for the entire lifetime
         *          of the returned ttf_font object.
         */
        [[nodiscard]] virtual ttf_font load(
            std::span<const std::uint8_t> data,
            std::size_t index) const = 0;

        /**
         * @brief Load the first outline font from the container.
         * @param data Raw file data (must remain valid for font lifetime)
         * @return Loaded outline font
         */
        [[nodiscard]] ttf_font load(std::span<const std::uint8_t> data) const {
            return load(data, 0);
        }

        /**
         * @brief Load all outline fonts from the container.
         * @param data Raw file data (must remain valid for fonts' lifetime)
         * @return Vector of all outline fonts
         */
        [[nodiscard]] virtual std::vector<ttf_font> load_all(
            std::span<const std::uint8_t> data) const;
    };

    // Convenience type aliases
    using bitmap_font_decoder_ptr = std::unique_ptr<bitmap_font_decoder>;
    using vector_font_decoder_ptr = std::unique_ptr<vector_font_decoder>;
    using outline_font_decoder_ptr = std::unique_ptr<outline_font_decoder>;

} // namespace onyx_font
