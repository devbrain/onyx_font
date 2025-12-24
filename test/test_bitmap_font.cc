//
// Created by igor on 21/12/2025.
//
// Unit tests for bitmap_font
//

#include <doctest/doctest.h>
#include <onyx_font/font_factory.hh>
#include <onyx_font/bitmap_font.hh>
#include <algorithm>
#include "test_data.hh"

using namespace onyx_font;
using namespace onyx_font::test;

TEST_SUITE("bitmap_font") {

    // Helper to load the first bitmap font from HELVA.FON
    bitmap_font load_helva_font() {
        auto data = test_data::load_fon_helva();
        return font_factory::load_bitmap(data, 0);
    }

    TEST_CASE("default constructor creates empty font") {
        bitmap_font font;

        CHECK(font.get_name().empty());
        CHECK(font.get_first_char() == 0);
        CHECK(font.get_last_char() == 0);
    }

    TEST_CASE("loaded font has correct name and properties") {
        REQUIRE(test_data::file_exists(test_data::fon_helva()));

        auto font = load_helva_font();

        // First font in HELVA.FON is "Helv" at 6px
        CHECK(font.get_name() == "Helv");
        CHECK(font.get_first_char() == 32);
        CHECK(font.get_last_char() == 255);
        CHECK(font.get_default_char() == 96);
        CHECK(font.get_break_char() == 0);
    }

    TEST_CASE("loaded font has correct metrics") {
        REQUIRE(test_data::file_exists(test_data::fon_helva()));

        auto font = load_helva_font();
        const auto& metrics = font.get_metrics();

        // First font in HELVA.FON is 6px height
        CHECK(metrics.pixel_height == 6);
        CHECK(metrics.ascent == 5);
        CHECK(metrics.internal_leading == 0);
        CHECK(metrics.external_leading == 0);
        CHECK(metrics.avg_width == 7);
        CHECK(metrics.max_width == 14);
    }

    TEST_CASE("get_glyph returns valid bitmap for ASCII characters") {
        REQUIRE(test_data::file_exists(test_data::fon_helva()));

        auto font = load_helva_font();

        // Test some common ASCII characters
        for (uint8_t ch : {'A', 'Z', 'a', 'z', '0', '9', ' ', '!'}) {
            if (ch >= font.get_first_char() && ch <= font.get_last_char()) {
                auto glyph = font.get_glyph(ch);

                CHECK(glyph.width() > 0);
                CHECK(glyph.height() > 0);
                CHECK(glyph.height() == font.get_metrics().pixel_height);
            }
        }
    }

    TEST_CASE("glyph bitmap has correct dimensions") {
        REQUIRE(test_data::file_exists(test_data::fon_helva()));

        auto font = load_helva_font();

        // Verify specific glyph dimensions for 6px Helv font
        auto glyph_A = font.get_glyph('A');
        CHECK(glyph_A.width() == 7);
        CHECK(glyph_A.height() == 6);

        auto glyph_M = font.get_glyph('M');
        CHECK(glyph_M.width() == 10);
        CHECK(glyph_M.height() == 6);

        auto glyph_i = font.get_glyph('i');
        CHECK(glyph_i.width() == 2);
        CHECK(glyph_i.height() == 6);

        auto glyph_space = font.get_glyph(' ');
        CHECK(glyph_space.width() == 2);
        CHECK(glyph_space.height() == 6);
    }

    TEST_CASE("glyph bitmap pixel access works") {
        REQUIRE(test_data::file_exists(test_data::fon_helva()));

        auto font = load_helva_font();
        auto glyph = font.get_glyph('A');

        // Should be able to access any pixel within bounds without crashing
        for (uint16_t y = 0; y < glyph.height(); ++y) {
            for (uint16_t x = 0; x < glyph.width(); ++x) {
                // Just verify we can call pixel() without exception
                [[maybe_unused]] bool pixel = glyph.pixel(x, y);
            }
        }
    }

    TEST_CASE("glyph 'A' has some set pixels") {
        REQUIRE(test_data::file_exists(test_data::fon_helva()));

        auto font = load_helva_font();
        auto glyph = font.get_glyph('A');

        // 'A' should have some pixels set (it's not empty)
        bool has_set_pixels = false;
        for (uint16_t y = 0; y < glyph.height() && !has_set_pixels; ++y) {
            for (uint16_t x = 0; x < glyph.width() && !has_set_pixels; ++x) {
                if (glyph.pixel(x, y)) {
                    has_set_pixels = true;
                }
            }
        }
        CHECK(has_set_pixels);
    }

    TEST_CASE("space glyph has no set pixels") {
        REQUIRE(test_data::file_exists(test_data::fon_helva()));

        auto font = load_helva_font();

        // Space character (32) should typically have no set pixels
        if (' ' >= font.get_first_char() && ' ' <= font.get_last_char()) {
            auto glyph = font.get_glyph(' ');

            bool has_set_pixels = false;
            for (uint16_t y = 0; y < glyph.height() && !has_set_pixels; ++y) {
                for (uint16_t x = 0; x < glyph.width() && !has_set_pixels; ++x) {
                    if (glyph.pixel(x, y)) {
                        has_set_pixels = true;
                    }
                }
            }
            CHECK_FALSE(has_set_pixels);
        }
    }

    TEST_CASE("glyph row access works") {
        REQUIRE(test_data::file_exists(test_data::fon_helva()));

        auto font = load_helva_font();
        auto glyph = font.get_glyph('W');

        for (uint16_t y = 0; y < glyph.height(); ++y) {
            auto row = glyph.row(y);
            CHECK(row.size() == glyph.stride_bytes());
        }
    }

    TEST_CASE("get_spacing returns valid spacing info") {
        REQUIRE(test_data::file_exists(test_data::fon_helva()));

        auto font = load_helva_font();
        const auto& spacing = font.get_spacing('M');

        // B-space (black width) should match glyph width if set
        if (spacing.b_space.has_value()) {
            CHECK(*spacing.b_space > 0);
        }
    }

    TEST_CASE("multiple font sizes in FON file") {
        REQUIRE(test_data::file_exists(test_data::fon_helva()));

        auto data = test_data::load_fon_helva();
        auto fonts = font_factory::load_all_bitmaps(data);

        // HELVA.FON contains 3 sizes: 6px, 8px, 10px
        REQUIRE(fonts.size() == 3);

        // All fonts are named "Helv"
        for (const auto& font : fonts) {
            CHECK(font.get_name() == "Helv");
            CHECK(font.get_first_char() == 32);
            CHECK(font.get_last_char() == 255);
        }

        // Verify specific heights
        CHECK(fonts[0].get_metrics().pixel_height == 6);
        CHECK(fonts[1].get_metrics().pixel_height == 8);
        CHECK(fonts[2].get_metrics().pixel_height == 10);

        // Verify ascent values
        CHECK(fonts[0].get_metrics().ascent == 5);
        CHECK(fonts[1].get_metrics().ascent == 7);
        CHECK(fonts[2].get_metrics().ascent == 9);
    }

    TEST_CASE("out of range pixel access throws") {
        REQUIRE(test_data::file_exists(test_data::fon_helva()));

        auto font = load_helva_font();
        auto glyph = font.get_glyph('A');

        // Access beyond width should throw
        CHECK_THROWS((void)glyph.pixel(glyph.width(), 0));

        // Access beyond height should throw
        CHECK_THROWS((void)glyph.pixel(0, glyph.height()));
    }
}
