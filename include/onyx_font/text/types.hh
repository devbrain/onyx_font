/**
 * @file types.hh
 * @brief Core types for text rendering system.
 *
 * This file defines fundamental data structures used throughout the
 * text rendering subsystem. These types represent geometric primitives,
 * metrics, and alignment options for text layout and rendering.
 *
 * @section types_overview Overview
 *
 * The types are organized into several categories:
 * - **Geometric types**: glyph_rect, text_box
 * - **Metrics types**: glyph_metrics, text_extents, scaled_metrics
 * - **Alignment enums**: text_align, text_valign
 * - **Font identification**: font_source_type
 *
 * @section types_usage Usage
 *
 * These types are used by higher-level components like text_rasterizer,
 * glyph_cache, and text_renderer for text measurement and layout.
 *
 * @code{.cpp}
 * // Measure and align text
 * text_extents extents = rasterizer.measure_text("Hello World");
 *
 * // Position text with alignment
 * text_box box{100, 100, 300, 50};
 * text_align align = text_align::center;
 *
 * float offset = (box.w - extents.width) / 2.0f;
 * @endcode
 *
 * @author Igor
 * @date 21/12/2025
 */

#pragma once

#include <onyx_font/export.h>
#include <cstdint>

namespace onyx_font {
    /**
     * @brief Rectangle for glyph positioning in atlas or on screen.
     *
     * A simple axis-aligned rectangle used for specifying glyph
     * positions within texture atlases and on render targets.
     */
    struct ONYX_FONT_EXPORT glyph_rect {
        int x = 0;  ///< Left edge X coordinate
        int y = 0;  ///< Top edge Y coordinate
        int w = 0;  ///< Width in pixels
        int h = 0;  ///< Height in pixels
    };

    /**
     * @brief Metrics for a single glyph.
     *
     * Contains all the information needed to position and advance
     * past a single glyph during text layout. These values are
     * scaled to the current font size.
     *
     * @section glyph_metrics_diagram Layout Diagram
     *
     * @code
     *                 bearing_x
     *                 |<->|
     *                 +---+--- <- bearing_y (baseline to top)
     *                 |   |  |
     *                 | A |  | height
     *                 |   |  |
     * baseline -------|---|--+--
     *                 |<->|
     *                 width
     *
     * |<------- advance_x ------->|
     * @endcode
     */
    struct ONYX_FONT_EXPORT glyph_metrics {
        float advance_x = 0;  ///< Horizontal advance to next glyph position
        float bearing_x = 0;  ///< Left side bearing (pen position to glyph left edge)
        float bearing_y = 0;  ///< Top side bearing (baseline to glyph top, positive up)
        float width = 0;      ///< Glyph bounding box width
        float height = 0;     ///< Glyph bounding box height
    };

    /**
     * @brief Metrics for a text string.
     *
     * Describes the dimensions and vertical positioning of a
     * complete text string, used for layout calculations.
     */
    struct ONYX_FONT_EXPORT text_extents {
        float width = 0;   ///< Total horizontal extent of the text
        float height = 0;  ///< Total height (ascent + descent)
        float ascent = 0;  ///< Distance from top to baseline (positive down)
        float descent = 0; ///< Distance from baseline to bottom (positive down)
    };

    /**
     * @brief Font-wide metrics scaled to a specific pixel size.
     *
     * These metrics describe the vertical dimensions of a font
     * at a particular rendering size. Used for calculating line
     * spacing and vertical positioning.
     *
     * @section scaled_diagram Vertical Layout
     *
     * @code
     *   +----------------+  <- top of line
     *   |    ascent      |  <- tallest character top
     *   |                |
     *   +--- baseline ---+  <- y = 0 for glyph positioning
     *   |    descent     |  <- deepest descender bottom
     *   +----------------+
     *   |   line_gap     |  <- extra spacing
     *   +----------------+  <- next line top
     *
     *   line_height = ascent + descent + line_gap
     * @endcode
     */
    struct ONYX_FONT_EXPORT scaled_metrics {
        float ascent = 0;      ///< Baseline to top of tallest glyph (positive)
        float descent = 0;     ///< Baseline to bottom of deepest descender (positive)
        float line_gap = 0;    ///< Recommended extra spacing between lines
        float line_height = 0; ///< Total line height (ascent + descent + line_gap)
    };

    /**
     * @brief Horizontal text alignment options.
     *
     * Controls how text is positioned horizontally within a
     * container or relative to an anchor point.
     */
    enum class text_align {
        left,   ///< Align text to left edge (default)
        center, ///< Center text horizontally
        right   ///< Align text to right edge
    };

    /**
     * @brief Vertical text alignment options.
     *
     * Controls how text is positioned vertically within a
     * container or relative to an anchor point.
     */
    enum class text_valign {
        top,      ///< Align text to top edge
        middle,   ///< Center text vertically
        bottom,   ///< Align text to bottom edge
        baseline  ///< Position at baseline (for single-line)
    };

    /**
     * @brief Bounding box for text layout.
     *
     * Defines a rectangular region for text rendering with
     * wrapping and alignment. Used by text_renderer for
     * multi-line layout.
     */
    struct ONYX_FONT_EXPORT text_box {
        float x = 0;  ///< Left edge X coordinate
        float y = 0;  ///< Top edge Y coordinate
        float w = 0;  ///< Width (for alignment and wrapping)
        float h = 0;  ///< Height (for vertical alignment and clipping)
    };

    /**
     * @brief Font type identifier.
     *
     * Identifies the underlying font type for a font_source,
     * which affects available features and rendering behavior.
     */
    enum class font_source_type {
        bitmap,  ///< Bitmap/raster font (fixed-size pixels)
        vector,  ///< Stroke-based vector font (scalable lines)
        outline  ///< Bezier outline font (TTF/OTF, highest quality)
    };
} // namespace onyx_font
