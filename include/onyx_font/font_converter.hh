/**
 * @file font_converter.hh
 * @brief Font conversion utilities for rasterizing vector and TTF fonts.
 *
 * This file provides the font_converter class for converting scalable fonts
 * (vector_font and ttf_font) to fixed-size bitmap_font objects. This enables
 * using modern font formats in systems that require raster fonts.
 *
 * @section converter_overview Overview
 *
 * Font conversion is useful when:
 * - Rendering to systems that only support bitmap fonts
 * - Pre-rasterizing fonts for performance-critical applications
 * - Creating pixel-perfect fonts at specific sizes
 * - Generating font atlases for game engines
 *
 * @section converter_workflow Conversion Workflow
 *
 * 1. Load the source font (vector or TTF)
 * 2. Choose target pixel height
 * 3. Configure conversion options (threshold, character range)
 * 4. Convert to bitmap font
 * 5. Use the resulting bitmap_font for rendering
 *
 * @section converter_example Usage Examples
 *
 * @subsection converter_ttf Converting a TrueType Font
 *
 * @code{.cpp}
 * // Load TTF font
 * auto ttf_data = read_file("arial.ttf");
 * ttf_font ttf(ttf_data);
 *
 * // Convert to 16px bitmap font
 * conversion_options opts;
 * opts.threshold = 128;  // 50% alpha threshold
 * opts.first_char = 32;  // Start at space
 * opts.last_char = 126;  // End at tilde (printable ASCII)
 *
 * bitmap_font bitmap = font_converter::from_ttf(ttf, 16.0f, opts);
 *
 * // Use the bitmap font
 * for (char ch : "Hello World") {
 *     auto glyph = bitmap.get_glyph(ch);
 *     render_glyph(glyph, pen_x, pen_y);
 *     pen_x += glyph.width();
 * }
 * @endcode
 *
 * @subsection converter_vector Converting a Vector Font
 *
 * @code{.cpp}
 * // Load vector font from Windows .FON file
 * auto fon_data = read_file("modern.fon");
 * auto vectors = font_factory::load_all_vectors(fon_data);
 *
 * if (!vectors.empty()) {
 *     // Scale up the vector font to 24 pixels
 *     bitmap_font large = font_converter::from_vector(vectors[0], 24.0f);
 *
 *     // The bitmap font now has the vector font's glyphs
 *     // rendered at 24px height
 * }
 * @endcode
 *
 * @section converter_threshold Threshold Selection
 *
 * The threshold parameter controls how antialiased pixels are converted
 * to 1-bit values:
 *
 * | Threshold | Effect |
 * |-----------|--------|
 * | 0 | All non-transparent pixels become 1 (thickest) |
 * | 128 | Standard 50% cutoff (balanced) |
 * | 200 | Only mostly-opaque pixels become 1 (thinnest) |
 * | 255 | Only fully opaque pixels become 1 (very thin) |
 *
 * Lower thresholds produce bolder/thicker text, higher thresholds
 * produce thinner/lighter text.
 *
 * @author Igor
 * @date 22/12/2025
 */

#pragma once

#include <onyx_font/export.h>
#include <onyx_font/bitmap_font.hh>
#include <onyx_font/vector_font.hh>
#include <onyx_font/ttf_font.hh>
#include <cstdint>

namespace onyx_font {
    /**
     * @brief Options for font-to-bitmap conversion.
     *
     * Controls how scalable fonts are rasterized to fixed-size bitmaps.
     * The most important parameter is the threshold, which determines
     * how antialiased edges are converted to 1-bit pixels.
     *
     * @section options_example Example
     *
     * @code{.cpp}
     * conversion_options opts;
     *
     * // Create thin, crisp text
     * opts.threshold = 180;
     * opts.antialias = true;
     *
     * // Only convert printable ASCII
     * opts.first_char = 32;
     * opts.last_char = 126;
     *
     * auto bitmap = font_converter::from_ttf(ttf, 14.0f, opts);
     * @endcode
     */
    struct ONYX_FONT_EXPORT conversion_options {
        /**
         * @brief Threshold for converting grayscale to 1-bit.
         *
         * When rasterizing, each pixel has an alpha value from 0-255.
         * Pixels with alpha >= threshold become 1 (foreground),
         * pixels with alpha < threshold become 0 (background).
         *
         * - **128** (default): Balanced 50% cutoff
         * - **Lower values**: Thicker, bolder text
         * - **Higher values**: Thinner, lighter text
         *
         * @note Only meaningful when antialias is true. When antialias
         *       is false, pixels are either 0 or 255.
         */
        uint8_t threshold = 128;

        /**
         * @brief First character code to include in output.
         *
         * Characters below this code are not converted.
         * Set to 32 (space) to exclude control characters.
         * Set to 0 to include all characters from the source font.
         *
         * @note If greater than the source font's first_char,
         *       this value takes precedence.
         */
        uint8_t first_char = 0;

        /**
         * @brief Last character code to include in output.
         *
         * Characters above this code are not converted.
         * Set to 126 (~) for printable ASCII only.
         * Set to 255 to include all characters from the source font.
         *
         * @note If less than the source font's last_char,
         *       this value takes precedence.
         */
        uint8_t last_char = 255;

        /**
         * @brief Enable antialiasing during rasterization.
         *
         * When true (default), the font is rendered with grayscale
         * antialiasing, then converted to 1-bit using the threshold.
         * This typically produces smoother-looking results even in
         * 1-bit output because the threshold can capture sub-pixel
         * edge information.
         *
         * When false, the font is rendered without antialiasing,
         * producing hard pixel edges directly.
         */
        bool antialias = true;
    };

    /**
     * @brief Converter for rasterizing scalable fonts to bitmaps.
     *
     * Provides static methods to convert vector_font and ttf_font
     * objects to bitmap_font at a specified pixel height. The resulting
     * bitmap fonts can be used anywhere a bitmap_font is expected.
     *
     * All methods are static - no instance needed.
     *
     * @section converter_features Features
     *
     * - **Scale independence**: Convert fonts to any pixel size
     * - **Threshold control**: Fine-tune 1-bit conversion
     * - **Character filtering**: Select which characters to include
     * - **Antialiasing**: Better quality through intermediate grayscale
     *
     * @section converter_use_cases Use Cases
     *
     * | Scenario | Recommended Settings |
     * |----------|---------------------|
     * | Retro game UI | threshold=128, antialias=true |
     * | Terminal emulator | threshold=100, antialias=true |
     * | LCD display | threshold=150, antialias=true |
     * | Thermal printer | threshold=180, antialias=false |
     *
     * @section converter_memory Memory Considerations
     *
     * The resulting bitmap_font is self-contained and does not
     * reference the source font. The source font can be destroyed
     * after conversion.
     *
     * @see conversion_options For controlling the conversion process
     * @see bitmap_font For using the resulting font
     */
    struct ONYX_FONT_EXPORT font_converter {
        /**
         * @brief Convert a vector font to bitmap at a specific size.
         *
         * Rasterizes the vector font's stroke-based glyphs to
         * fixed-size pixel bitmaps. The vector font is scaled
         * to the specified pixel height.
         *
         * @param font Source vector font to convert
         * @param pixel_height Target height in pixels
         * @param options Conversion options (threshold, character range)
         * @return Newly created bitmap font
         *
         * @note Vector fonts scale linearly, so 2x the pixel height
         *       produces glyphs that are 2x taller and wider.
         *
         * @section from_vector_example Example
         *
         * @code{.cpp}
         * vector_font src = font_factory::load_vector(data, 0);
         *
         * // Create versions at different sizes
         * bitmap_font small = font_converter::from_vector(src, 12.0f);
         * bitmap_font medium = font_converter::from_vector(src, 16.0f);
         * bitmap_font large = font_converter::from_vector(src, 24.0f);
         * @endcode
         */
        static bitmap_font from_vector(
            const vector_font& font,
            float pixel_height,
            const conversion_options& options = {}
        );

        /**
         * @brief Convert a TTF font to bitmap at a specific size.
         *
         * Rasterizes the TrueType/OpenType font's bezier outlines to
         * fixed-size pixel bitmaps. This is useful for pre-rendering
         * fonts for systems that don't support TTF directly.
         *
         * @param font Source TTF font to convert
         * @param pixel_height Target height in pixels
         * @param options Conversion options (threshold, character range)
         * @return Newly created bitmap font
         *
         * @warning The ttf_font's underlying data buffer must remain
         *          valid during the conversion process. After conversion
         *          completes, the source data is no longer needed.
         *
         * @section from_ttf_example Example
         *
         * @code{.cpp}
         * // Load TTF
         * std::vector<uint8_t> ttf_data = read_file("consola.ttf");
         * ttf_font ttf(ttf_data);
         *
         * // Convert to bitmap for terminal use
         * conversion_options opts;
         * opts.first_char = 0;    // Include control chars for terminal
         * opts.last_char = 255;   // Include extended ASCII
         * opts.threshold = 100;   // Slightly bolder
         *
         * bitmap_font terminal_font = font_converter::from_ttf(ttf, 14.0f, opts);
         *
         * // ttf_data can now be freed if desired
         * @endcode
         *
         * @section from_ttf_metrics Metrics Preservation
         *
         * The converted bitmap font preserves the TTF font's metrics:
         * - Ascent and descent are scaled to pixel height
         * - Character widths match the TTF font's advance widths
         * - Line spacing follows the original font's design
         */
        static bitmap_font from_ttf(
            const ttf_font& font,
            float pixel_height,
            const conversion_options& options = {}
        );
    };
} // namespace onyx_font
