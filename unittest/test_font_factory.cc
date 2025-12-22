//
// Created by igor on 21/12/2025.
//
// Unit tests for font_factory
//

#include <doctest/doctest.h>
#include <onyx_font/font_factory.hh>
#include "test_data.hh"

using namespace onyx_font;
using namespace onyx_font::test;

TEST_SUITE("font_factory") {

    TEST_CASE("format_name returns valid strings") {
        CHECK(font_factory::format_name(container_format::UNKNOWN) == "Unknown");
        CHECK(font_factory::format_name(container_format::TTF) == "TrueType");
        CHECK(font_factory::format_name(container_format::OTF) == "OpenType");
        CHECK(font_factory::format_name(container_format::TTC) == "TrueType Collection");
        CHECK(font_factory::format_name(container_format::FNT) == "Windows Font");
        CHECK(font_factory::format_name(container_format::FON_NE) == "Windows 16-bit Font Resource");
        CHECK(font_factory::format_name(container_format::FON_PE) == "Windows 32/64-bit Font Resource");
        CHECK(font_factory::format_name(container_format::FON_LX) == "OS/2 Font Resource");
        CHECK(font_factory::format_name(container_format::BGI) == "Borland Graphics Interface");
    }

    TEST_CASE("type_name returns valid strings") {
        CHECK(font_factory::type_name(font_type::UNKNOWN) == "Unknown");
        CHECK(font_factory::type_name(font_type::BITMAP) == "Bitmap");
        CHECK(font_factory::type_name(font_type::VECTOR) == "Vector");
        CHECK(font_factory::type_name(font_type::OUTLINE) == "Outline");
    }

    TEST_CASE("analyze Windows FNT file (version 1.0)") {
        REQUIRE(test_data::file_exists(test_data::fnt_romanp()));

        auto data = test_data::load_fnt_romanp();
        CHECK(data.size() == 4748);

        auto info = font_factory::analyze(data);

        CHECK(info.format == container_format::FNT);
        REQUIRE(info.fonts.size() == 1);

        const auto& font = info.fonts[0];
        CHECK(font.name == "Hershey Roman Simplex Half");
        CHECK(font.type == font_type::VECTOR);
        CHECK(font.pixel_height == 16);
        CHECK(font.point_size == 36);
        CHECK(font.weight == 400);
        CHECK(font.italic == false);
    }

    TEST_CASE("analyze Windows FON file (NE)") {
        REQUIRE(test_data::file_exists(test_data::fon_helva()));

        auto data = test_data::load_fon_helva();
        CHECK(data.size() == 8032);

        auto info = font_factory::analyze(data);

        CHECK(info.format == container_format::FON_NE);
        REQUIRE(info.fonts.size() == 3);

        // All fonts in HELVA.FON are "Helv" bitmap fonts at different sizes
        for (const auto& font : info.fonts) {
            CHECK(font.name == "Helv");
            CHECK(font.type == font_type::BITMAP);
            CHECK(font.weight == 400);
            CHECK(font.italic == false);
        }

        // Verify specific sizes: 6px, 8px, 10px
        CHECK(info.fonts[0].pixel_height == 6);
        CHECK(info.fonts[0].point_size == 8);
        CHECK(info.fonts[1].pixel_height == 8);
        CHECK(info.fonts[1].point_size == 10);
        CHECK(info.fonts[2].pixel_height == 10);
        CHECK(info.fonts[2].point_size == 12);
    }

    TEST_CASE("analyze BGI font file") {
        REQUIRE(test_data::file_exists(test_data::bgi_litt()));

        auto data = test_data::load_bgi_litt();
        CHECK(data.size() == 5131);

        auto info = font_factory::analyze(data);

        CHECK(info.format == container_format::BGI);
        REQUIRE(info.fonts.size() == 1);

        const auto& font = info.fonts[0];
        CHECK(font.type == font_type::VECTOR);
        CHECK(font.weight == 400);
        CHECK(font.italic == false);
    }

    TEST_CASE("analyze empty data returns UNKNOWN") {
        std::vector<uint8_t> empty_data;
        auto info = font_factory::analyze(empty_data);

        CHECK(info.format == container_format::UNKNOWN);
        CHECK(info.fonts.empty());
    }

    TEST_CASE("analyze invalid data returns UNKNOWN") {
        std::vector<uint8_t> garbage = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05};
        auto info = font_factory::analyze(garbage);

        CHECK(info.format == container_format::UNKNOWN);
    }

    TEST_CASE("load_bitmap from FON file") {
        REQUIRE(test_data::file_exists(test_data::fon_helva()));

        auto data = test_data::load_fon_helva();
        auto info = font_factory::analyze(data);

        REQUIRE(!info.fonts.empty());
        REQUIRE(info.fonts[0].type == font_type::BITMAP);

        auto font = font_factory::load_bitmap(data, 0);

        CHECK(!font.get_name().empty());
        CHECK(font.get_first_char() <= font.get_last_char());
        CHECK(font.get_metrics().pixel_height > 0);
    }

    TEST_CASE("load_all_bitmaps from FON file") {
        REQUIRE(test_data::file_exists(test_data::fon_helva()));

        auto data = test_data::load_fon_helva();
        auto fonts = font_factory::load_all_bitmaps(data);

        CHECK(!fonts.empty());

        for (const auto& font : fonts) {
            CHECK(!font.get_name().empty());
            CHECK(font.get_metrics().pixel_height > 0);
        }
    }

    TEST_CASE("load_vector from FNT file (version 1.0)") {
        REQUIRE(test_data::file_exists(test_data::fnt_romanp()));

        auto data = test_data::load_fnt_romanp();
        auto font = font_factory::load_vector(data, 0);

        CHECK(font.get_first_char() <= font.get_last_char());
        CHECK(font.get_metrics().pixel_height > 0);
    }

    TEST_CASE("load_vector from BGI file") {
        REQUIRE(test_data::file_exists(test_data::bgi_litt()));

        auto data = test_data::load_bgi_litt();
        auto font = font_factory::load_vector(data, 0);

        CHECK(font.get_first_char() <= font.get_last_char());
        CHECK(font.get_metrics().pixel_height > 0);

        // Check that we have some glyphs with strokes
        bool has_strokes = false;
        for (uint8_t ch = font.get_first_char(); ch <= font.get_last_char(); ++ch) {
            const auto* glyph = font.get_glyph(ch);
            if (glyph && !glyph->strokes.empty()) {
                has_strokes = true;
                break;
            }
        }
        CHECK(has_strokes);
    }

    TEST_CASE("load_all_vectors from BGI file") {
        REQUIRE(test_data::file_exists(test_data::bgi_litt()));

        auto data = test_data::load_bgi_litt();
        auto fonts = font_factory::load_all_vectors(data);

        REQUIRE(fonts.size() == 1);
        CHECK(fonts[0].get_first_char() <= fonts[0].get_last_char());
    }

    TEST_CASE("load_bitmap with invalid index throws") {
        REQUIRE(test_data::file_exists(test_data::fon_helva()));

        auto data = test_data::load_fon_helva();
        auto info = font_factory::analyze(data);

        // Try to load an index beyond the available fonts
        CHECK_THROWS(font_factory::load_bitmap(data, info.fonts.size() + 10));
    }

    TEST_CASE("analyze via file path") {
        REQUIRE(test_data::file_exists(test_data::fon_helva()));

        auto info = font_factory::analyze(test_data::fon_helva());

        CHECK(info.format == container_format::FON_NE);
        CHECK(!info.fonts.empty());
    }

    TEST_CASE("analyze TrueType font file") {
        REQUIRE(test_data::file_exists(test_data::ttf_arial()));

        auto data = test_data::load_ttf_arial();
        CHECK(data.size() == 65692);

        auto info = font_factory::analyze(data);

        CHECK(info.format == container_format::TTF);
        REQUIRE(info.fonts.size() == 1);

        const auto& font = info.fonts[0];
        CHECK(font.type == font_type::OUTLINE);
        CHECK(font.weight == 400);
        CHECK(font.italic == false);
    }

    TEST_CASE("analyze VGAOEM FON file") {
        REQUIRE(test_data::file_exists(test_data::fon_vgaoem()));

        auto data = test_data::load_fon_vgaoem();
        CHECK(data.size() == 5168);

        auto info = font_factory::analyze(data);

        CHECK(info.format == container_format::FON_NE);
        REQUIRE(info.fonts.size() == 1);

        const auto& font = info.fonts[0];
        CHECK(font.name == "Terminal");
        CHECK(font.type == font_type::BITMAP);
        CHECK(font.pixel_height == 12);
        CHECK(font.point_size == 12);
        CHECK(font.weight == 400);
        CHECK(font.italic == false);
    }

    TEST_CASE("load_bitmap from VGAOEM FON file") {
        REQUIRE(test_data::file_exists(test_data::fon_vgaoem()));

        auto data = test_data::load_fon_vgaoem();
        auto fonts = font_factory::load_all_bitmaps(data);

        REQUIRE(fonts.size() == 1);

        const auto& font = fonts[0];
        CHECK(font.get_name() == "Terminal");
        CHECK(font.get_first_char() == 1);
        CHECK(font.get_last_char() == 254);
        CHECK(font.get_default_char() == 31);
        CHECK(font.get_break_char() == 31);

        auto metrics = font.get_metrics();
        CHECK(metrics.pixel_height == 12);
        CHECK(metrics.ascent == 10);
        CHECK(metrics.max_width == 8);
        CHECK(metrics.avg_width == 8);
    }

    TEST_CASE("load_ttf from TrueType file") {
        REQUIRE(test_data::file_exists(test_data::ttf_arial()));

        auto data = test_data::load_ttf_arial();
        auto font = font_factory::load_ttf(data, 0);

        CHECK(font.is_valid());
    }

    TEST_CASE("analyze FOT file") {
        REQUIRE(test_data::file_exists(test_data::fot_arial()));

        auto data = test_data::load_fot_arial();
        CHECK(data.size() == 1306);

        auto info = font_factory::analyze(data);

        // FOT files are NE executables but contain no actual font resources
        // They just point to a TTF file
        CHECK(info.format == container_format::FON_NE);
        CHECK(info.fonts.empty());
    }

    TEST_CASE("analyze OS/2 system font (NE)") {
        REQUIRE(test_data::file_exists(test_data::fon_os2_sysfont()));

        auto data = test_data::load_fon_os2_sysfont();
        CHECK(data.size() == 45568);

        auto info = font_factory::analyze(data);

        // OS/2 1.x uses NE format
        CHECK(info.format == container_format::FON_NE);

        // SYSFONT.DLL contains 5 bitmap fonts (IDs 101-105)
        REQUIRE(info.fonts.size() == 5);

        // Verify font names
        CHECK(info.fonts[0].name == "Mirrors-EGA");
        CHECK(info.fonts[1].name == "Mirrors-VGA");
        CHECK(info.fonts[2].name == "Mirrors-XGA");
        CHECK(info.fonts[3].name == "Mirrors-OEM-EGA");
        CHECK(info.fonts[4].name == "Mirrors-OEM-VGA");

        // Verify sizes: 10px, 15px, 20px, 10px, 15px
        CHECK(info.fonts[0].pixel_height == 10);
        CHECK(info.fonts[1].pixel_height == 15);
        CHECK(info.fonts[2].pixel_height == 20);
        CHECK(info.fonts[3].pixel_height == 10);
        CHECK(info.fonts[4].pixel_height == 15);

        // All fonts should be bitmap fonts
        for (const auto& font : info.fonts) {
            CHECK(font.type == font_type::BITMAP);
        }
    }

    TEST_CASE("load_bitmap from OS/2 system font") {
        REQUIRE(test_data::file_exists(test_data::fon_os2_sysfont()));

        auto data = test_data::load_fon_os2_sysfont();
        auto fonts = font_factory::load_all_bitmaps(data);

        // Should load 5 fonts
        REQUIRE(fonts.size() == 5);

        // Verify first font (Mirrors-EGA)
        CHECK(fonts[0].get_name() == "Mirrors-EGA");
        CHECK(fonts[0].get_first_char() == 32);
        CHECK(fonts[0].get_last_char() == 223);
        CHECK(fonts[0].get_metrics().pixel_height == 10);
        CHECK(fonts[0].get_metrics().ascent == 9);
        CHECK(fonts[0].get_metrics().max_width == 8);

        // Verify XGA font (largest)
        CHECK(fonts[2].get_name() == "Mirrors-XGA");
        CHECK(fonts[2].get_metrics().pixel_height == 20);
        CHECK(fonts[2].get_metrics().max_width == 10);

        // Verify OEM fonts have full character range
        CHECK(fonts[3].get_first_char() == 0);
        CHECK(fonts[3].get_last_char() == 255);
        CHECK(fonts[4].get_first_char() == 0);
        CHECK(fonts[4].get_last_char() == 255);
    }

    // =========================================================================
    // Raw BIOS Font Loading Tests
    // =========================================================================

    TEST_CASE("load_raw with synthetic 8x8 font") {
        // Create a simple 8x8 font with 2 characters
        // Character 0: empty
        // Character 1: solid block
        std::vector<uint8_t> data(16);  // 2 chars * 8 bytes
        // Character 0: all zeros (already initialized)
        // Character 1: all ones (solid block)
        for (int i = 8; i < 16; ++i) {
            data[static_cast<size_t>(i)] = 0xFF;
        }

        raw_font_options options;
        options.char_width = 8;
        options.char_height = 8;
        options.first_char = 0;
        options.char_count = 2;
        options.name = "Test 8x8";

        auto font = font_factory::load_raw(data, options);

        CHECK(font.get_name() == "Test 8x8");
        CHECK(font.get_first_char() == 0);
        CHECK(font.get_last_char() == 1);
        CHECK(font.get_metrics().pixel_height == 8);
        CHECK(font.get_metrics().avg_width == 8);

        // Check empty glyph
        auto glyph0 = font.get_glyph(0);
        CHECK(glyph0.width() == 8);
        CHECK(glyph0.height() == 8);
        bool has_pixels_0 = false;
        for (uint16_t y = 0; y < 8; ++y) {
            for (uint16_t x = 0; x < 8; ++x) {
                if (glyph0.pixel(x, y)) has_pixels_0 = true;
            }
        }
        CHECK_FALSE(has_pixels_0);

        // Check solid glyph
        auto glyph1 = font.get_glyph(1);
        CHECK(glyph1.width() == 8);
        CHECK(glyph1.height() == 8);
        bool all_pixels_1 = true;
        for (uint16_t y = 0; y < 8; ++y) {
            for (uint16_t x = 0; x < 8; ++x) {
                if (!glyph1.pixel(x, y)) all_pixels_1 = false;
            }
        }
        CHECK(all_pixels_1);
    }

    TEST_CASE("load_raw with synthetic 8x16 font") {
        // Create a simple 8x16 font with letter 'A' pattern
        std::vector<uint8_t> data(256 * 16);  // 256 chars * 16 bytes

        // Set up 'A' at character 65
        size_t offset = 65 * 16;
        data[offset + 0]  = 0b00011000;  // Row 0
        data[offset + 1]  = 0b00111100;  // Row 1
        data[offset + 2]  = 0b01100110;  // Row 2
        data[offset + 3]  = 0b01100110;  // Row 3
        data[offset + 4]  = 0b11000011;  // Row 4
        data[offset + 5]  = 0b11000011;  // Row 5
        data[offset + 6]  = 0b11111111;  // Row 6
        data[offset + 7]  = 0b11111111;  // Row 7
        data[offset + 8]  = 0b11000011;  // Row 8
        data[offset + 9]  = 0b11000011;  // Row 9
        data[offset + 10] = 0b11000011;  // Row 10
        data[offset + 11] = 0b11000011;  // Row 11
        data[offset + 12] = 0b00000000;  // Row 12
        data[offset + 13] = 0b00000000;  // Row 13
        data[offset + 14] = 0b00000000;  // Row 14
        data[offset + 15] = 0b00000000;  // Row 15

        auto font = font_factory::load_raw(data, raw_font_options::vga_8x16());

        CHECK(font.get_name() == "VGA 8x16");
        CHECK(font.get_first_char() == 0);
        CHECK(font.get_last_char() == 255);
        CHECK(font.get_metrics().pixel_height == 16);

        // Verify 'A' glyph has pixels
        auto glyph_a = font.get_glyph('A');
        CHECK(glyph_a.width() == 8);
        CHECK(glyph_a.height() == 16);

        // Check specific pixels
        CHECK(glyph_a.pixel(3, 0) == true);   // Part of top of A
        CHECK(glyph_a.pixel(4, 0) == true);
        CHECK(glyph_a.pixel(0, 0) == false);  // Outside A
        CHECK(glyph_a.pixel(0, 6) == true);   // Left side of crossbar
        CHECK(glyph_a.pixel(7, 6) == true);   // Right side of crossbar
    }

    TEST_CASE("load_raw with vga_8x8 preset") {
        // Create 8x8 font data
        std::vector<uint8_t> data(256 * 8, 0);

        // Set up a checkerboard pattern for character 'X'
        size_t offset = 'X' * 8;
        data[offset + 0] = 0b10101010;
        data[offset + 1] = 0b01010101;
        data[offset + 2] = 0b10101010;
        data[offset + 3] = 0b01010101;
        data[offset + 4] = 0b10101010;
        data[offset + 5] = 0b01010101;
        data[offset + 6] = 0b10101010;
        data[offset + 7] = 0b01010101;

        auto font = font_factory::load_raw(data, raw_font_options::vga_8x8());

        CHECK(font.get_name() == "VGA 8x8");
        CHECK(font.get_metrics().pixel_height == 8);

        auto glyph = font.get_glyph('X');
        // Check checkerboard pattern
        CHECK(glyph.pixel(0, 0) == true);
        CHECK(glyph.pixel(1, 0) == false);
        CHECK(glyph.pixel(0, 1) == false);
        CHECK(glyph.pixel(1, 1) == true);
    }

    TEST_CASE("load_raw with ega_8x14 preset") {
        // Create 8x14 font data
        std::vector<uint8_t> data(256 * 14, 0);

        auto font = font_factory::load_raw(data, raw_font_options::ega_8x14());

        CHECK(font.get_name() == "EGA 8x14");
        CHECK(font.get_metrics().pixel_height == 14);
        CHECK(font.get_first_char() == 0);
        CHECK(font.get_last_char() == 255);
    }

    TEST_CASE("load_raw with custom first_char") {
        // Create font starting at character 32 (space)
        std::vector<uint8_t> data(96 * 8, 0);  // 96 chars (32-127)

        raw_font_options options;
        options.char_width = 8;
        options.char_height = 8;
        options.first_char = 32;
        options.char_count = 96;
        options.name = "ASCII Only";

        auto font = font_factory::load_raw(data, options);

        CHECK(font.get_first_char() == 32);
        CHECK(font.get_last_char() == 127);
    }

    TEST_CASE("load_raw with lsb_first bit order") {
        // Create a simple 8x1 font with one char to test bit order
        std::vector<uint8_t> data(1);
        data[0] = 0b10000000;  // In MSB: leftmost pixel on
                               // In LSB: rightmost pixel on

        raw_font_options msb_options;
        msb_options.char_width = 8;
        msb_options.char_height = 1;
        msb_options.char_count = 1;
        msb_options.msb_first = true;

        raw_font_options lsb_options;
        lsb_options.char_width = 8;
        lsb_options.char_height = 1;
        lsb_options.char_count = 1;
        lsb_options.msb_first = false;

        auto msb_font = font_factory::load_raw(data, msb_options);
        auto lsb_font = font_factory::load_raw(data, lsb_options);

        auto msb_glyph = msb_font.get_glyph(0);
        auto lsb_glyph = lsb_font.get_glyph(0);

        // MSB: bit 7 = pixel 0 (leftmost), so pixel(0,0) should be on
        CHECK(msb_glyph.pixel(0, 0) == true);
        CHECK(msb_glyph.pixel(7, 0) == false);

        // LSB: bit 0 = pixel 0 (leftmost), so pixel(7,0) should be on
        CHECK(lsb_glyph.pixel(0, 0) == false);
        CHECK(lsb_glyph.pixel(7, 0) == true);
    }

    TEST_CASE("load_raw throws on insufficient data") {
        std::vector<uint8_t> data(100);  // Too small for 256 * 16 = 4096 bytes

        CHECK_THROWS(font_factory::load_raw(data, raw_font_options::vga_8x16()));
    }

    TEST_CASE("load_raw spacing is uniform") {
        std::vector<uint8_t> data(256 * 8, 0);

        auto font = font_factory::load_raw(data, raw_font_options::vga_8x8());

        // All characters should have the same width (8)
        // Use int to avoid uint8_t overflow when iterating to 255
        for (int ch = font.get_first_char(); ch <= font.get_last_char(); ++ch) {
            auto spacing = font.get_spacing(static_cast<uint8_t>(ch));
            REQUIRE(spacing.b_space.has_value());
            CHECK(*spacing.b_space == 8);
        }
    }
}
