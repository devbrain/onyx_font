//
// Character sets of 8-bit fonts
//

#include <onyx_font/charset.hh>
#include <array>

namespace onyx_font {
    namespace {
        // Code Page 437, bytes 0x80-0xFF
        constexpr std::array<char32_t, 128> k_cp437_high = {
            0x00C7, 0x00FC, 0x00E9, 0x00E2, 0x00E4, 0x00E0, 0x00E5, 0x00E7, // 0x80
            0x00EA, 0x00EB, 0x00E8, 0x00EF, 0x00EE, 0x00EC, 0x00C4, 0x00C5,
            0x00C9, 0x00E6, 0x00C6, 0x00F4, 0x00F6, 0x00F2, 0x00FB, 0x00F9, // 0x90
            0x00FF, 0x00D6, 0x00DC, 0x00A2, 0x00A3, 0x00A5, 0x20A7, 0x0192,
            0x00E1, 0x00ED, 0x00F3, 0x00FA, 0x00F1, 0x00D1, 0x00AA, 0x00BA, // 0xA0
            0x00BF, 0x2310, 0x00AC, 0x00BD, 0x00BC, 0x00A1, 0x00AB, 0x00BB,
            0x2591, 0x2592, 0x2593, 0x2502, 0x2524, 0x2561, 0x2562, 0x2556, // 0xB0
            0x2555, 0x2563, 0x2551, 0x2557, 0x255D, 0x255C, 0x255B, 0x2510,
            0x2514, 0x2534, 0x252C, 0x251C, 0x2500, 0x253C, 0x255E, 0x255F, // 0xC0
            0x255A, 0x2554, 0x2569, 0x2566, 0x2560, 0x2550, 0x256C, 0x2567,
            0x2568, 0x2564, 0x2565, 0x2559, 0x2558, 0x2552, 0x2553, 0x256B, // 0xD0
            0x256A, 0x2518, 0x250C, 0x2588, 0x2584, 0x258C, 0x2590, 0x2580,
            0x03B1, 0x00DF, 0x0393, 0x03C0, 0x03A3, 0x03C3, 0x00B5, 0x03C4, // 0xE0
            0x03A6, 0x0398, 0x03A9, 0x03B4, 0x221E, 0x03C6, 0x03B5, 0x2229,
            0x2261, 0x00B1, 0x2265, 0x2264, 0x2320, 0x2321, 0x00F7, 0x2248, // 0xF0
            0x00B0, 0x2219, 0x00B7, 0x221A, 0x207F, 0x00B2, 0x25A0, 0x00A0,
        };

        // Code Page 437, the pictures of the control bytes 0x00-0x1F (0x00 shows nothing)
        constexpr std::array<char32_t, 32> k_cp437_low = {
            0x0000, 0x263A, 0x263B, 0x2665, 0x2666, 0x2663, 0x2660, 0x2022, // 0x00
            0x25D8, 0x25CB, 0x25D9, 0x2642, 0x2640, 0x266A, 0x266B, 0x263C,
            0x25BA, 0x25C4, 0x2195, 0x203C, 0x00B6, 0x00A7, 0x25AC, 0x21A8, // 0x10
            0x2191, 0x2193, 0x2192, 0x2190, 0x221F, 0x2194, 0x25B2, 0x25BC,
        };

        constexpr char32_t k_cp437_house = 0x2302; // the picture of 0x7F

        // Windows-1252, bytes 0x80-0x9F (unassigned bytes keep their C1 control code point)
        constexpr std::array<char32_t, 32> k_cp1252_c1 = {
            0x20AC, 0x0081, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021, // 0x80
            0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0x008D, 0x017D, 0x008F,
            0x0090, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014, // 0x90
            0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0x009D, 0x017E, 0x0178,
        };

        template<std::size_t N>
        std::optional<std::uint8_t> find(const std::array<char32_t, N>& table, char32_t codepoint,
                                         unsigned first_code) {
            for (std::size_t i = 0; i < N; ++i) {
                if (table[i] == codepoint) {
                    return static_cast<std::uint8_t>(first_code + i);
                }
            }
            return std::nullopt;
        }
    }

    std::optional<std::uint8_t> encode_char(charset set, char32_t codepoint) {
        if (codepoint < 0x80) {
            return static_cast<std::uint8_t>(codepoint);
        }
        switch (set) {
            case charset::latin1:
                break;
            case charset::cp437:
                if (auto code = find(k_cp437_high, codepoint, 0x80)) {
                    return code;
                }
                if (codepoint == k_cp437_house) {
                    return std::uint8_t{0x7F};
                }
                return find(k_cp437_low, codepoint, 0x00);
            case charset::cp1252:
                if (auto code = find(k_cp1252_c1, codepoint, 0x80)) {
                    return code;
                }
                if (codepoint >= 0x80 && codepoint < 0xA0) {
                    return std::nullopt; // a C1 control that Windows-1252 replaced
                }
                break;
        }
        if (codepoint <= 0xFF) {
            return static_cast<std::uint8_t>(codepoint);
        }
        return std::nullopt;
    }

    char32_t decode_char(charset set, std::uint8_t code) {
        if (code < 0x80) {
            return code;
        }
        switch (set) {
            case charset::latin1:
                break;
            case charset::cp437:
                return k_cp437_high[code - 0x80u];
            case charset::cp1252:
                if (code < 0xA0) {
                    return k_cp1252_c1[code - 0x80u];
                }
                break;
        }
        return code;
    }

    std::optional<std::uint8_t> encode_char(const code_page& page, char32_t codepoint) {
        if (codepoint < 0x80) {
            return static_cast<std::uint8_t>(codepoint);
        }
        return find(page.high, codepoint, 0x80);
    }

    char32_t decode_char(const code_page& page, std::uint8_t code) {
        return code < 0x80 ? char32_t{code} : page.high[code - 0x80u];
    }

    charset charset_from_windows(std::uint8_t windows_charset) {
        switch (windows_charset) {
            case 0:   return charset::cp1252; // ANSI_CHARSET
            case 255: return charset::cp437;  // OEM_CHARSET
            default:  return charset::latin1;
        }
    }

} // namespace onyx_font
