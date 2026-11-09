/**
 * @file font_entry.hh
 * @brief Metadata of one font in a container: what decoders' enumerate() returns.
 *
 * Kept apart from font_factory.hh so that decoder.hh is complete on its own: a custom
 * decoder needs font_entry, not the factory.
 *
 * @author Igor
 * @date 09/11/2026
 */

#pragma once

#include <cstdint>
#include <string>

#include <onyx_font/export.h>

namespace onyx_font {
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
        std::uint16_t pixel_height;  ///< Native pixel height (0 for scalable fonts)
        std::uint16_t point_size;    ///< Design point size (0 if unknown)
        std::uint16_t weight;        ///< Font weight (400=normal, 700=bold)
        bool italic;            ///< True if italic/oblique
    };

}
