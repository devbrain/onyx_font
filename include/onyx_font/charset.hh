/**
 * @file charset.hh
 * @brief Character sets of 8-bit fonts: Unicode code points to glyph codes and back.
 *
 * Bitmap and vector fonts index their glyphs with one byte. Which character
 * each byte shows depends on the font's character set: the IBM BIOS fonts
 * follow Code Page 437, Windows fonts usually Windows-1252. `font_source`
 * uses the font's charset to find the glyph for a Unicode code point, so
 * `U'\u00E9'` (e acute) draws byte 0x82 of a CP437 font and byte 0xE9 of a Windows-1252 one.
 *
 * @code{.cpp}
 * #include <onyx_font/charset.hh>
 *
 * auto byte = onyx_font::encode_char(onyx_font::charset::cp437, U'\u2591');  // light shade: 0xB0
 * char32_t cp = onyx_font::decode_char(onyx_font::charset::cp437, 0x82); // U'\u00E9'
 * @endcode
 */

#pragma once

#include <onyx_font/export.h>
#include <array>
#include <cstdint>
#include <optional>

namespace onyx_font {

    /**
     * @brief The character set of an 8-bit font.
     *
     * In all of them bytes 0x00-0x7F are ASCII.
     */
    enum class charset : std::uint8_t {
        /**
         * @brief Byte = code point (ISO 8859-1, U+0000-U+00FF).
         *
         * Also the choice for fonts whose character set isn't known or
         * supported (symbol fonts, other code pages): bytes map as they are.
         */
        latin1,

        /**
         * @brief IBM PC Code Page 437 (the BIOS fonts, DOS OEM fonts).
         *
         * 0x80-0xFF are accented letters, box drawing, shades and math
         * symbols. The pictures shown for the control bytes 0x01-0x1F and
         * 0x7F (smileys, card suits, arrows, the house, ...) are reached through their own code points;
         * the control code points themselves still map to those bytes.
         */
        cp437,

        /**
         * @brief Windows-1252 (Windows ANSI fonts).
         *
         * Latin-1, except 0x80-0x9F, which hold the euro sign, quotes, dashes, S/Z caron, OE
         * and others. The five unassigned bytes there map to the C1 control
         * code points of the same value.
         */
        cp1252
    };

    /**
     * @brief The glyph code of a code point in a character set.
     * @return The byte, or nullopt if the character set has no such character
     */
    [[nodiscard]] ONYX_FONT_EXPORT std::optional<std::uint8_t> encode_char(charset set, char32_t codepoint);

    /**
     * @brief The code point a glyph code shows in a character set.
     *
     * Bytes 0x00-0x7F decode to themselves, so control codes stay control codes.
     */
    [[nodiscard]] ONYX_FONT_EXPORT char32_t decode_char(charset set, std::uint8_t code);

    /**
     * @brief A code page the caller supplies, for fonts whose character set onyx_font doesn't know.
     *
     * Bytes 0x00-0x7F are ASCII; `high[i]` is the code point byte 0x80 + i shows, 0 for none.
     * A Windows font of charset 204 (Cyrillic) loads as charset::latin1; given Windows-1251's table, `font_source`
     * draws its Cyrillic letters (see `font_source::set_code_page`).
     */
    struct code_page {
        std::array<char32_t, 128> high{};
    };

    /**
     * @brief The glyph code of a code point in a caller's code page.
     * @return The byte, or nullopt if the code page has no such character
     */
    [[nodiscard]] ONYX_FONT_EXPORT std::optional<std::uint8_t> encode_char(const code_page& page, char32_t codepoint);

    /**
     * @brief The code point a glyph code shows in a caller's code page; 0 for a byte it leaves empty.
     */
    [[nodiscard]] ONYX_FONT_EXPORT char32_t decode_char(const code_page& page, std::uint8_t code);

    /**
     * @brief The character set named by a Windows font's `dfCharSet` field.
     *
     * 0 (ANSI_CHARSET) is Windows-1252 and 255 (OEM_CHARSET) is Code Page 437.
     * Any other value (symbol fonts, other code pages) maps bytes as they are
     * (charset::latin1); for another code page, give `font_source` a `code_page`.
     */
    [[nodiscard]] ONYX_FONT_EXPORT charset charset_from_windows(std::uint8_t windows_charset);

} // namespace onyx_font
