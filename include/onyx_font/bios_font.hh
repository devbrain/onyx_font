/**
 * @file bios_font.hh
 * @brief Embedded IBM BIOS fonts (CP437).
 *
 * Provides built-in bitmap fonts matching the standard IBM video BIOS
 * ROM fonts, covering all 256 code points of Code Page 437:
 *
 * - **8x8** — CGA/EGA compatible, compact (2048 bytes)
 * - **8x14** — EGA text mode font (3584 bytes)
 * - **8x16** — VGA text mode font (4096 bytes)
 *
 * The font data is public domain — extracted from IBM hardware ROMs
 * and widely reproduced in open-source projects.
 *
 * @code{.cpp}
 * #include <onyx_font/bios_font.hh>
 *
 * const auto& small = onyx_font::bios_font_8x8();
 * const auto& medium = onyx_font::bios_font_8x14();
 * const auto& large = onyx_font::bios_font_8x16();
 * @endcode
 */

#pragma once

#include <onyx_font/bitmap_font.hh>

namespace onyx_font {

    /**
     * @brief Get the embedded CGA/EGA 8x8 bitmap font.
     *
     * Compact font suitable for small displays and dense text.
     * 256 glyphs, 8 bytes per glyph, 2048 bytes total.
     *
     * @return Const reference to the 8x8 bitmap font
     */
    [[nodiscard]] ONYX_FONT_EXPORT const bitmap_font& bios_font_8x8();

    /**
     * @brief Get the embedded EGA 8x14 bitmap font.
     *
     * Medium-height font from the EGA text mode.
     * 256 glyphs, 14 bytes per glyph, 3584 bytes total.
     *
     * @return Const reference to the EGA 8x14 bitmap font
     */
    [[nodiscard]] ONYX_FONT_EXPORT const bitmap_font& bios_font_8x14();

    /**
     * @brief Get the embedded VGA 8x16 bitmap font.
     *
     * Standard VGA text mode font, the most commonly used size.
     * 256 glyphs, 16 bytes per glyph, 4096 bytes total.
     *
     * @return Const reference to the VGA 8x16 bitmap font
     */
    [[nodiscard]] ONYX_FONT_EXPORT const bitmap_font& bios_font_8x16();

} // namespace onyx_font
