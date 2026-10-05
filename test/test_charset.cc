//
// Unit tests for character sets of 8-bit fonts
//

#include <doctest/doctest.h>
#include <onyx_font/charset.hh>
#include <onyx_font/bios_font.hh>
#include <onyx_font/font_factory.hh>
#include <onyx_font/text/font_source.hh>
#include <onyx_font/text/raster_target.hh>
#include "test_data.hh"

#include <vector>

using namespace onyx_font;
using namespace onyx_font::test;

namespace {
    // The pixels font_source draws for a code point, and the pixels of a glyph code, as a bitmap of 0/1
    std::vector<uint8_t> drawn(const bitmap_font& font, char32_t codepoint) {
        const auto& m = font.get_metrics();
        std::vector<uint8_t> buffer(static_cast<size_t>(16 * m.pixel_height), 0);
        grayscale_target target(buffer.data(), 16, m.pixel_height);
        font_source::from_bitmap(font).rasterize_glyph(codepoint, static_cast<float>(m.pixel_height), target, 0,
                                                       m.ascent);
        for (auto& p : buffer) p = p ? 1 : 0;
        return buffer;
    }

    std::vector<uint8_t> stored(const bitmap_font& font, uint8_t code) {
        const auto& m = font.get_metrics();
        std::vector<uint8_t> buffer(static_cast<size_t>(16 * m.pixel_height), 0);
        bitmap_view glyph = font.get_glyph(code);
        for (uint16_t y = 0; y < glyph.height(); ++y) {
            for (uint16_t x = 0; x < glyph.width(); ++x) {
                buffer[static_cast<size_t>(y * 16 + x)] = glyph.pixel(x, y) ? 1 : 0;
            }
        }
        return buffer;
    }
}

TEST_SUITE("charset") {

    TEST_CASE("ASCII is the same in every character set") {
        for (charset set : {charset::latin1, charset::cp437, charset::cp1252}) {
            for (char32_t c = 0; c < 0x80; ++c) {
                REQUIRE(encode_char(set, c) == static_cast<uint8_t>(c));
                REQUIRE(decode_char(set, static_cast<uint8_t>(c)) == c);
            }
            CHECK_FALSE(encode_char(set, U'\u4E2D').has_value());
        }
    }

    TEST_CASE("latin1 maps bytes as they are") {
        CHECK(encode_char(charset::latin1, U'\u00E9') == uint8_t{0xE9});
        CHECK(encode_char(charset::latin1, U'\u0080') == uint8_t{0x80});
        CHECK_FALSE(encode_char(charset::latin1, U'\u20AC').has_value());
        CHECK(decode_char(charset::latin1, 0xFF) == U'\u00FF');
    }

    TEST_CASE("cp437: accented letters, box drawing, shades and the control pictures") {
        CHECK(encode_char(charset::cp437, U'\u00E9') == uint8_t{0x82});
        CHECK(encode_char(charset::cp437, U'\u00C7') == uint8_t{0x80});
        CHECK(encode_char(charset::cp437, U'\u2591') == uint8_t{0xB0});
        CHECK(encode_char(charset::cp437, U'\u256C') == uint8_t{0xCE});
        CHECK(encode_char(charset::cp437, U'\u00DF') == uint8_t{0xE1});
        CHECK(encode_char(charset::cp437, U'\u00A0') == uint8_t{0xFF});
        CHECK(encode_char(charset::cp437, U'\u263A') == uint8_t{0x01});
        CHECK(encode_char(charset::cp437, U'\u25BC') == uint8_t{0x1F});
        CHECK(encode_char(charset::cp437, U'\u2302') == uint8_t{0x7F});
        CHECK_FALSE(encode_char(charset::cp437, U'\u00C0').has_value());   // A grave: not in CP437
        CHECK_FALSE(encode_char(charset::cp437, U'\u0082').has_value());
        CHECK(decode_char(charset::cp437, 0x82) == U'\u00E9');
        CHECK(decode_char(charset::cp437, 0xDB) == U'\u2588');
    }

    TEST_CASE("cp1252: the 0x80-0x9F block") {
        CHECK(encode_char(charset::cp1252, U'\u20AC') == uint8_t{0x80});
        CHECK(encode_char(charset::cp1252, U'\u2014') == uint8_t{0x97});
        CHECK(encode_char(charset::cp1252, U'\u0178') == uint8_t{0x9F});
        CHECK(encode_char(charset::cp1252, U'\u00E9') == uint8_t{0xE9});
        CHECK(encode_char(charset::cp1252, U'\u0081') == uint8_t{0x81}); // unassigned: kept
        CHECK_FALSE(encode_char(charset::cp1252, U'\u0080').has_value()); // replaced by the euro sign
        CHECK(decode_char(charset::cp1252, 0x80) == U'\u20AC');
        CHECK(decode_char(charset::cp1252, 0xE9) == U'\u00E9');
    }

    TEST_CASE("every byte decodes to a code point that encodes back to it") {
        for (charset set : {charset::latin1, charset::cp437, charset::cp1252}) {
            for (unsigned b = 0; b < 256; ++b) {
                const auto code = static_cast<uint8_t>(b);
                REQUIRE(encode_char(set, decode_char(set, code)) == code);
            }
        }
    }

    TEST_CASE("a caller's code page") {
        code_page page;
        for (unsigned i = 0; i < 64; ++i) {
            page.high[64 + i] = 0x0410 + i; // 0xC0-0xFF: Cyrillic A to ya, as Windows-1251
        }
        CHECK(encode_char(page, U'A') == uint8_t{'A'});
        CHECK(encode_char(page, U'\u0410') == uint8_t{0xC0});
        CHECK(encode_char(page, U'\u044F') == uint8_t{0xFF});
        CHECK_FALSE(encode_char(page, U'\u00E9').has_value()); // not in it
        CHECK(decode_char(page, 0xC1) == U'\u0411');
        CHECK(decode_char(page, 0x80) == char32_t{0}); // left empty
        CHECK(decode_char(page, 'z') == U'z');
    }

    TEST_CASE("font_source reads glyph codes through a caller's code page") {
        // A raw latin1 font whose glyph of byte b is b's bits in its first row
        std::vector<uint8_t> data(256 * 8, 0);
        for (unsigned b = 0; b < 256; ++b) {
            data[b * 8] = static_cast<uint8_t>(b);
        }
        raw_font_options opts;
        opts.char_height = 8;
        const bitmap_font font = font_factory::load_raw(data, opts);
        code_page page;
        page.high[0x40] = U'\u0410'; // byte 0xC0
        auto source = font_source::from_bitmap(font);
        CHECK(source.has_glyph(U'\u00C0'));
        CHECK_FALSE(source.has_glyph(U'\u0410'));
        source.set_code_page(page);
        CHECK(source.has_glyph(U'\u0410'));
        CHECK_FALSE(source.has_glyph(U'\u00C0'));
        CHECK(source.has_glyph(U'A'));
        std::vector<uint8_t> buffer(8 * 8, 0);
        grayscale_target target(buffer.data(), 8, 8);
        source.rasterize_glyph(U'\u0410', 8.0f, target, 0, font.get_metrics().ascent);
        std::vector<uint8_t> row(buffer.begin(), buffer.begin() + 8);
        for (auto& p : row) p = p ? 1 : 0;
        CHECK(row == std::vector<uint8_t>{1, 1, 0, 0, 0, 0, 0, 0}); // 0xC0's bits
        CHECK(source.get_glyph_metrics(U'\u0410', 8.0f).advance_x > 0);
        source.set_code_page(std::nullopt);
        CHECK(source.has_glyph(U'\u00C0'));
    }

    TEST_CASE("Windows charset values") {
        CHECK(charset_from_windows(0) == charset::cp1252);
        CHECK(charset_from_windows(255) == charset::cp437);
        CHECK(charset_from_windows(2) == charset::latin1);   // SYMBOL_CHARSET
        CHECK(charset_from_windows(204) == charset::latin1); // RUSSIAN_CHARSET: unsupported, bytes as they are
    }

    TEST_CASE("the BIOS fonts are CP437 and draw accents from their CP437 positions") {
        for (const bitmap_font* font : {&bios_font_8x8(), &bios_font_8x14(), &bios_font_8x16()}) {
            CAPTURE(font->get_name());
            CHECK(font->get_charset() == charset::cp437);
            const auto source = font_source::from_bitmap(*font);
            CHECK(source.has_glyph(U'\u00E9'));
            CHECK(source.has_glyph(U'\u2592'));
            CHECK_FALSE(source.has_glyph(U'\u00C0'));
            CHECK(drawn(*font, U'\u00E9') == stored(*font, 0x82));
            CHECK(drawn(*font, U'\u2588') == stored(*font, 0xDB));
            CHECK(drawn(*font, U'A') == stored(*font, 'A'));
            CHECK(drawn(*font, U'\u00E9') != stored(*font, 0xE9));
        }
    }

    TEST_CASE("raw fonts are latin1 unless asked") {
        std::vector<uint8_t> data(256 * 8, 0);
        raw_font_options opts;
        opts.char_height = 8;
        CHECK(font_factory::load_raw(data, opts).get_charset() == charset::latin1);
        opts.encoding = charset::cp437;
        CHECK(font_factory::load_raw(data, opts).get_charset() == charset::cp437);
    }

    TEST_CASE("Windows FON fonts take the charset of the file") {
        if (!test_data::file_exists(test_data::fon_helva()) || !test_data::file_exists(test_data::fon_vgaoem())) {
            return;
        }
        auto helva = font_factory::load_bitmap(test_data::load_fon_helva(), 0);
        CHECK(helva.get_charset() == charset::cp1252);
        auto vgaoem = font_factory::load_bitmap(test_data::load_fon_vgaoem(), 0);
        CHECK(vgaoem.get_charset() == charset::cp437);
        if (vgaoem.get_first_char() <= 0x82 && vgaoem.get_last_char() >= 0x82) {
            CHECK(drawn(vgaoem, U'\u00E9') == stored(vgaoem, 0x82));
        }
    }
}
