/**
 * @file font_factory.hh
 * @brief Font loading and format detection for multiple font formats.
 *
 * This file provides the font_factory class, a unified interface for loading
 * fonts from various file formats including Windows .FON files, TrueType fonts,
 * Borland BGI fonts, and raw BIOS font dumps.
 *
 * @section factory_overview Overview
 *
 * The font_factory provides:
 * - **Format detection**: Automatically identify font file formats
 * - **Font enumeration**: List all fonts within a container file
 * - **Type-specific loading**: Load bitmap, vector, or TTF fonts
 * - **Bulk loading**: Load all fonts of a type from a container
 *
 * @section factory_formats Supported Formats
 *
 * | Format | Extension | Description |
 * |--------|-----------|-------------|
 * | TTF | .ttf | TrueType font (outline) |
 * | OTF | .otf | OpenType font (outline) |
 * | TTC | .ttc | TrueType Collection (multiple fonts) |
 * | FON_NE | .fon | Windows 16-bit NE executable |
 * | FON_PE | .fon | Windows 32/64-bit PE executable |
 * | FON_LX | .fon | OS/2 LX executable |
 * | FNT | .fnt | Raw Windows font resource |
 * | BGI | .chr | Borland Graphics Interface stroke font |
 *
 * @section factory_usage Usage Examples
 *
 * @subsection factory_analyze Analyzing a Font File
 *
 * @code{.cpp}
 * auto data = read_file("fonts/system.fon");
 * auto info = font_factory::analyze(data);
 *
 * std::cout << "Format: " << font_factory::format_name(info.format) << "\n";
 * std::cout << "Contains " << info.fonts.size() << " fonts:\n";
 *
 * for (const auto& entry : info.fonts) {
 *     std::cout << "  - " << entry.name
 *               << " (" << font_factory::type_name(entry.type) << ")"
 *               << " " << entry.pixel_height << "px\n";
 * }
 * @endcode
 *
 * @subsection factory_load Loading Fonts
 *
 * @code{.cpp}
 * // Load a specific bitmap font by index
 * auto bitmap = font_factory::load_bitmap(data, 0);
 *
 * // Load all bitmap fonts from a container
 * auto all_bitmaps = font_factory::load_all_bitmaps(data);
 *
 * // Load a TrueType font
 * auto ttf_data = read_file("arial.ttf");
 * auto ttf = font_factory::load_ttf(ttf_data);
 *
 * // Load a raw BIOS font dump
 * auto bios_data = read_file("vga8x16.bin");
 * auto bios_font = font_factory::load_raw(bios_data, raw_font_options::vga_8x16());
 * @endcode
 *
 * @author Igor
 * @date 11/12/2025
 */

#pragma once

#include <filesystem>
#include <span>
#include <vector>
#include <string>
#include <cstdint>

#include <onyx_font/export.h>
#include <onyx_font/bitmap_font.hh>
#include <onyx_font/vector_font.hh>
#include <onyx_font/ttf_font.hh>

namespace onyx_font {
    /**
     * @brief Options for loading raw BIOS font dumps.
     *
     * Raw BIOS fonts are simple bitmap dumps where each character is stored
     * as consecutive bytes. Each byte represents one row of pixels, with
     * the bit order controlled by msb_first.
     *
     * Common formats:
     * - **8x8 VGA**: 2048 bytes (256 chars * 8 bytes per char)
     * - **8x14 EGA**: 3584 bytes (256 chars * 14 bytes per char)
     * - **8x16 VGA**: 4096 bytes (256 chars * 16 bytes per char)
     *
     * @section raw_example Example
     *
     * @code{.cpp}
     * // Load standard VGA 8x16 font
     * auto data = read_file("vga8x16.bin");  // 4096 bytes
     * auto font = font_factory::load_raw(data, raw_font_options::vga_8x16());
     *
     * // Load custom font with different parameters
     * raw_font_options opts;
     * opts.char_width = 8;
     * opts.char_height = 12;
     * opts.first_char = 32;
     * opts.char_count = 96;  // ASCII printable only
     * opts.name = "Custom 8x12";
     * auto custom_font = font_factory::load_raw(data, opts);
     * @endcode
     */
    struct ONYX_FONT_EXPORT raw_font_options {
        /**
         * @brief Character width in pixels.
         *
         * Typically 8 for BIOS fonts. Each row is stored in
         * (char_width + 7) / 8 bytes.
         */
        uint8_t char_width = 8;

        /**
         * @brief Character height in pixels.
         *
         * Common values: 8, 14, or 16 for VGA/EGA fonts.
         */
        uint8_t char_height = 16;

        /**
         * @brief First character code in the dump.
         *
         * Usually 0 for full 256-character sets.
         */
        uint8_t first_char = 0;

        /**
         * @brief Number of characters in the dump.
         *
         * Usually 256 for full character sets.
         */
        uint16_t char_count = 256;

        /**
         * @brief Bit order within each byte.
         *
         * - true (MSB first): Bit 7 = leftmost pixel (standard for BIOS fonts)
         * - false (LSB first): Bit 0 = leftmost pixel
         */
        bool msb_first = true;

        /**
         * @brief Display name for the font.
         */
        std::string name = "BIOS";

        /**
         * @brief Create options for standard 8x8 VGA font.
         * @return Options configured for 8x8 VGA font (2048 bytes)
         */
        static raw_font_options vga_8x8() {
            return {8, 8, 0, 256, true, "VGA 8x8"};
        }

        /**
         * @brief Create options for standard 8x14 EGA font.
         * @return Options configured for 8x14 EGA font (3584 bytes)
         */
        static raw_font_options ega_8x14() {
            return {8, 14, 0, 256, true, "EGA 8x14"};
        }

        /**
         * @brief Create options for standard 8x16 VGA font.
         * @return Options configured for 8x16 VGA font (4096 bytes)
         */
        static raw_font_options vga_8x16() {
            return {8, 16, 0, 256, true, "VGA 8x16"};
        }
    };

    /**
     * @brief Font container file format.
     *
     * Identifies the file format of a font container, which may
     * hold one or more fonts of various types.
     */
    enum class container_format {
        UNKNOWN,   ///< Unrecognized format

        TTF,       ///< TrueType font (.ttf) - single outline font
        OTF,       ///< OpenType font (.otf) - single outline font
        TTC,       ///< TrueType Collection (.ttc) - multiple outline fonts

        FNT,       ///< Raw Windows font resource (.fnt) - single font
        FON_NE,    ///< Windows 16-bit NE executable (.fon) - one or more fonts
        FON_PE,    ///< Windows 32/64-bit PE executable (.fon) - one or more fonts
        FON_LX,    ///< OS/2 LX executable with font resources

        BGI        ///< Borland Graphics Interface (.chr) - vector stroke font
    };

    /**
     * @brief Font rendering type.
     *
     * Categorizes fonts by how they represent glyph shapes.
     */
    enum class font_type {
        UNKNOWN,   ///< Unknown or unrecognized font type

        /**
         * @brief Raster/bitmap font.
         *
         * Glyphs are stored as fixed-size pixel bitmaps.
         * Best quality at native size, pixelated when scaled.
         */
        BITMAP,

        /**
         * @brief Stroke-based vector font.
         *
         * Glyphs are stored as sequences of line drawing commands.
         * Scalable but typically simpler than outline fonts.
         * Examples: Windows vector fonts, BGI fonts.
         */
        VECTOR,

        /**
         * @brief Bezier outline font (TrueType/OpenType).
         *
         * Glyphs are stored as bezier curve outlines.
         * Highest quality, fully scalable with hinting support.
         */
        OUTLINE
    };

    /**
     * @brief Description of a single font within a container.
     *
     * Provides metadata about a font without loading it fully.
     * Use with container_info::fonts to enumerate available fonts.
     */
    struct ONYX_FONT_EXPORT font_entry {
        std::string name;       ///< Font face name (e.g., "Arial", "System")
        font_type type;         ///< Rendering type (bitmap/vector/outline)
        uint16_t pixel_height;  ///< Native pixel height (0 for scalable fonts)
        uint16_t point_size;    ///< Design point size (0 if unknown)
        uint16_t weight;        ///< Font weight (400=normal, 700=bold)
        bool italic;            ///< True if italic/oblique
    };

    /**
     * @brief Analysis result for a font container file.
     *
     * Contains information about the container format and a list
     * of all fonts found within.
     */
    struct ONYX_FONT_EXPORT container_info {
        container_format format;         ///< Container file format
        std::vector<font_entry> fonts;   ///< List of fonts in container
    };

    /**
     * @brief Factory for analyzing and loading fonts from various formats.
     *
     * Provides a unified interface for working with multiple font formats.
     * All methods are static - no instance needed.
     *
     * @section factory_workflow Typical Workflow
     *
     * 1. **Analyze** the file to determine format and list fonts
     * 2. **Load** specific fonts by index or load all fonts of a type
     * 3. **Use** the loaded font objects for rendering
     *
     * @code{.cpp}
     * // Read file data
     * auto data = read_file("myfont.fon");
     *
     * // Analyze to see what's inside
     * auto info = font_factory::analyze(data);
     *
     * // Load based on what was found
     * if (info.fonts[0].type == font_type::BITMAP) {
     *     auto font = font_factory::load_bitmap(data, 0);
     *     // Use bitmap font...
     * }
     * @endcode
     */
    struct ONYX_FONT_EXPORT font_factory {
        /**
         * @name Container Analysis
         * @{
         */

        /**
         * @brief Analyze a font container from memory.
         *
         * Examines the file data to determine the format and enumerate
         * all fonts within the container.
         *
         * @param data Raw file data
         * @return Container info with format and font list
         *
         * @note This is a lightweight operation that doesn't fully parse
         *       font data - use it to inspect files before loading.
         */
        static container_info analyze(std::span<const uint8_t> data);

        /**
         * @brief Analyze a font file from disk.
         *
         * Convenience overload that reads the file and analyzes it.
         *
         * @param path Path to font file
         * @return Container info with format and font list
         * @throws std::runtime_error if file cannot be read
         */
        static container_info analyze(const std::filesystem::path& path);

        /** @} */

        /**
         * @name Type-Specific Loading
         * @{
         */

        /**
         * @brief Load a bitmap font by index.
         *
         * Loads a single bitmap font from a container file.
         * Use analyze() first to determine available fonts and their indices.
         *
         * @param data Raw file data (NE/PE/LX FON or FNT)
         * @param index Font index within container (0-based)
         * @return Loaded bitmap font
         * @throws std::runtime_error if index invalid or font is not bitmap type
         */
        static bitmap_font load_bitmap(std::span<const uint8_t> data, size_t index = 0);

        /**
         * @brief Load a vector font by index.
         *
         * Loads a single vector (stroke-based) font from a container file.
         *
         * @param data Raw file data (NE/PE FON with vector font, or BGI CHR)
         * @param index Font index within container (0-based)
         * @return Loaded vector font
         * @throws std::runtime_error if index invalid or font is not vector type
         */
        static vector_font load_vector(std::span<const uint8_t> data, size_t index = 0);

        /**
         * @brief Load a TrueType/OpenType font.
         *
         * Loads a TTF, OTF, or font from a TTC collection.
         *
         * @param data Raw file data (must remain valid for ttf_font lifetime)
         * @param index Font index within TTC collection (0 for single TTF/OTF)
         * @return Loaded TTF font
         * @throws std::runtime_error if index invalid or not a TTF/OTF/TTC
         *
         * @warning The data span must remain valid for the entire lifetime
         *          of the returned ttf_font object.
         */
        static ttf_font load_ttf(std::span<const uint8_t> data, size_t index = 0);

        /**
         * @brief Load a raw BIOS font dump.
         *
         * Loads a raw bitmap font dump, such as those extracted from
         * VGA BIOS or created by font editors.
         *
         * @param data Raw font bitmap data
         * @param options Font format options (dimensions, character range, etc.)
         * @return Loaded bitmap font
         * @throws std::runtime_error if data size doesn't match expected size
         *
         * Expected data size: `(char_width + 7) / 8 * char_height * char_count`
         *
         * @code{.cpp}
         * // Load standard VGA font
         * auto data = read_file("vga8x16.bin");
         * auto font = font_factory::load_raw(data, raw_font_options::vga_8x16());
         * @endcode
         */
        static bitmap_font load_raw(std::span<const uint8_t> data,
                                    const raw_font_options& options = {});

        /** @} */

        /**
         * @name Bulk Loading
         * @{
         */

        /**
         * @brief Load all bitmap fonts from a container.
         *
         * Loads every bitmap font found in the container file.
         * Non-bitmap fonts are skipped.
         *
         * @param data Raw file data
         * @return Vector of all bitmap fonts (may be empty)
         */
        static std::vector<bitmap_font> load_all_bitmaps(std::span<const uint8_t> data);

        /**
         * @brief Load all vector fonts from a container.
         *
         * Loads every vector font found in the container file.
         * Non-vector fonts are skipped.
         *
         * @param data Raw file data
         * @return Vector of all vector fonts (may be empty)
         */
        static std::vector<vector_font> load_all_vectors(std::span<const uint8_t> data);

        /** @} */

        /**
         * @name Utility Functions
         * @{
         */

        /**
         * @brief Get human-readable name for a container format.
         * @param fmt Container format enum value
         * @return String like "TrueType", "Windows NE FON", etc.
         */
        static std::string_view format_name(container_format fmt);

        /**
         * @brief Get human-readable name for a font type.
         * @param type Font type enum value
         * @return String like "Bitmap", "Vector", "Outline"
         */
        static std::string_view type_name(font_type type);

        /** @} */
    };
} // namespace onyx_font
