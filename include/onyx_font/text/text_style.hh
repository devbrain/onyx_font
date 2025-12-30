/**
 * @file text_style.hh
 * @brief Text styling flags and rendering options.
 *
 * This file provides text styling capabilities for onyx_font, including
 * bold, italic, underline, strikethrough, and outline effects.
 *
 * @section style_support Font Type Support
 *
 * | Feature | TTF/OTF | Vector (BGI) | Bitmap |
 * |---------|---------|--------------|--------|
 * | Bold | Horizontal spread | Thick strokes | Ignored |
 * | Italic | Shear transform | Shear strokes | Ignored |
 * | Underline | Yes | Yes | Yes |
 * | Strikethrough | Yes | Yes | Yes |
 * | Outline | Dilate + overlay | Double stroke | Ignored |
 *
 * @section style_usage Usage
 *
 * @code{.cpp}
 * text_rasterizer rasterizer(source, 24.0f);
 *
 * // Simple style flags
 * rasterizer.set_style(text_style::bold | text_style::underline);
 *
 * // Detailed control
 * render_style style;
 * style.style = text_style::bold | text_style::italic;
 * style.bold_strength = 2;
 * style.italic_shear = 0.25f;
 * rasterizer.set_style(style);
 * @endcode
 *
 * @author Igor
 * @date 30/12/2025
 */

#pragma once

#include <onyx_font/export.h>
#include <cstdint>
#include <algorithm>

namespace onyx_font {

// ============================================================================
// Text Style Flags
// ============================================================================

/**
 * @brief Text style flags (combinable with |).
 *
 * These flags control the visual appearance of rendered text.
 * Multiple flags can be combined using bitwise OR.
 *
 * @note For bitmap fonts, only underline and strikethrough are applied.
 *       Bold, italic, and outline are silently ignored to preserve
 *       pixel-perfect rendering.
 */
enum class text_style : std::uint8_t {
    normal        = 0,        ///< No styling (default)
    bold          = 1 << 0,   ///< Synthetic bold (emboldening)
    italic        = 1 << 1,   ///< Synthetic italic (shear transform)
    underline     = 1 << 2,   ///< Underline decoration
    strikethrough = 1 << 3    ///< Strikethrough decoration
};

/**
 * @brief Combine two style flags.
 */
[[nodiscard]] constexpr text_style operator|(text_style a, text_style b) noexcept {
    return static_cast<text_style>(
        static_cast<std::uint8_t>(a) | static_cast<std::uint8_t>(b));
}

/**
 * @brief Combine style flags (compound assignment).
 */
constexpr text_style& operator|=(text_style& a, text_style b) noexcept {
    a = a | b;
    return a;
}

/**
 * @brief Intersect two style flags.
 */
[[nodiscard]] constexpr text_style operator&(text_style a, text_style b) noexcept {
    return static_cast<text_style>(
        static_cast<std::uint8_t>(a) & static_cast<std::uint8_t>(b));
}

/**
 * @brief Intersect style flags (compound assignment).
 */
constexpr text_style& operator&=(text_style& a, text_style b) noexcept {
    a = a & b;
    return a;
}

/**
 * @brief Invert style flags.
 */
[[nodiscard]] constexpr text_style operator~(text_style a) noexcept {
    return static_cast<text_style>(~static_cast<std::uint8_t>(a));
}

/**
 * @brief Check if a style flag is set.
 *
 * @param flags The style flags to check
 * @param style The specific style to test for
 * @return true if the style is set
 *
 * @code{.cpp}
 * if (has_style(my_style, text_style::bold)) {
 *     // Apply bold rendering
 * }
 * @endcode
 */
[[nodiscard]] constexpr bool has_style(text_style flags, text_style style) noexcept {
    return (static_cast<std::uint8_t>(flags) & static_cast<std::uint8_t>(style)) != 0;
}

// ============================================================================
// Render Style Options
// ============================================================================

/**
 * @brief Detailed rendering style options.
 *
 * Provides fine-grained control over text styling beyond the basic
 * text_style flags. Use this when you need to customize the appearance
 * of styled text.
 *
 * @section defaults Default Values
 *
 * Default values are chosen to match SDL_ttf behavior:
 * - Bold strength: auto-calculated (~1px per 24pt)
 * - Italic shear: 0.21 (~12 degrees)
 * - Decoration thickness: auto-calculated from font size
 */
struct ONYX_FONT_EXPORT render_style {
    /**
     * @brief Style flags (bold, italic, underline, strikethrough).
     */
    text_style style = text_style::normal;

    /**
     * @brief Bold strength in pixels.
     *
     * Controls how much the glyphs are emboldened.
     * - 0: Auto-calculate from font size (recommended)
     * - 1+: Manual pixel amount
     *
     * Auto-calculation: max(1, size / 24)
     */
    int bold_strength = 0;

    /**
     * @brief Italic shear factor.
     *
     * Horizontal displacement per vertical pixel.
     * - 0.0: No slant
     * - 0.21: Standard italic (~12 degrees, matches SDL_ttf)
     * - 0.36: Heavy slant (~20 degrees)
     *
     * Positive values slant right (top moves right).
     */
    float italic_shear = 0.21f;

    /**
     * @brief Outline thickness in pixels.
     *
     * - 0: No outline (default)
     * - 1+: Outline thickness in pixels
     *
     * @note Outline is only supported for TTF fonts.
     *       Vector fonts use double-stroke technique.
     *       Bitmap fonts ignore outline.
     */
    int outline_thickness = 0;

    /**
     * @brief Underline thickness in pixels.
     *
     * - 0: Auto-calculate from font size
     * - 1+: Manual thickness
     *
     * Auto-calculation: max(1, size / 14)
     */
    int underline_thickness = 0;

    /**
     * @brief Underline vertical offset from baseline.
     *
     * - 0: Auto-calculate (typically descent / 2)
     * - Positive: Below baseline
     * - Negative: Above baseline
     */
    int underline_offset = 0;

    /**
     * @brief Strikethrough vertical offset from baseline.
     *
     * - 0: Auto-calculate (typically -ascent * 0.35)
     * - Positive: Below baseline
     * - Negative: Above baseline
     */
    int strikethrough_offset = 0;

    /**
     * @brief Strikethrough thickness in pixels.
     *
     * - 0: Auto-calculate (same as underline)
     * - 1+: Manual thickness
     */
    int strikethrough_thickness = 0;

    // ========================================================================
    // Convenience Methods
    // ========================================================================

    /**
     * @brief Check if bold style is enabled.
     */
    [[nodiscard]] bool is_bold() const noexcept {
        return has_style(style, text_style::bold);
    }

    /**
     * @brief Check if italic style is enabled.
     */
    [[nodiscard]] bool is_italic() const noexcept {
        return has_style(style, text_style::italic);
    }

    /**
     * @brief Check if underline is enabled.
     */
    [[nodiscard]] bool is_underlined() const noexcept {
        return has_style(style, text_style::underline);
    }

    /**
     * @brief Check if strikethrough is enabled.
     */
    [[nodiscard]] bool is_strikethrough() const noexcept {
        return has_style(style, text_style::strikethrough);
    }

    /**
     * @brief Check if any styling is applied.
     */
    [[nodiscard]] bool has_any_style() const noexcept {
        return style != text_style::normal || outline_thickness > 0;
    }

    /**
     * @brief Check if any glyph transforms are needed.
     *
     * Returns true if bold, italic, or outline is enabled.
     * Underline/strikethrough don't require glyph transforms.
     */
    [[nodiscard]] bool needs_glyph_transform() const noexcept {
        return is_bold() || is_italic() || outline_thickness > 0;
    }

    /**
     * @brief Check if decorations (underline/strikethrough) are needed.
     */
    [[nodiscard]] bool needs_decorations() const noexcept {
        return is_underlined() || is_strikethrough();
    }

    /**
     * @brief Reset to default (no styling).
     */
    void reset() noexcept {
        *this = render_style{};
    }
};

// ============================================================================
// Style Calculation Helpers
// ============================================================================

/**
 * @brief Calculate automatic bold strength for a font size.
 *
 * @param font_size Font size in pixels
 * @return Bold strength in pixels
 */
[[nodiscard]] inline int calc_bold_strength(float font_size) noexcept {
    return std::max(1, static_cast<int>(font_size / 24.0f + 0.5f));
}

/**
 * @brief Calculate automatic underline/strikethrough thickness.
 *
 * @param font_size Font size in pixels
 * @return Line thickness in pixels
 */
[[nodiscard]] inline int calc_decoration_thickness(float font_size) noexcept {
    return std::max(1, static_cast<int>(font_size / 14.0f + 0.5f));
}

/**
 * @brief Calculate automatic underline offset from baseline.
 *
 * @param descent Font descent (positive value)
 * @return Offset in pixels (positive = below baseline)
 */
[[nodiscard]] inline int calc_underline_offset(float descent) noexcept {
    return static_cast<int>(descent * 0.5f + 0.5f);
}

/**
 * @brief Calculate automatic strikethrough offset from baseline.
 *
 * @param ascent Font ascent
 * @return Offset in pixels (negative = above baseline)
 */
[[nodiscard]] inline int calc_strikethrough_offset(float ascent) noexcept {
    return -static_cast<int>(ascent * 0.35f + 0.5f);
}

} // namespace onyx_font
