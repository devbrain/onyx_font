//
// Created by igor on 21/12/2025.
//
// Unit tests for vector_font
//

#include <doctest/doctest.h>
#include <onyx_font/font_factory.hh>
#include <onyx_font/vector_font.hh>
#include "test_data.hh"

using namespace onyx_font;
using namespace onyx_font::test;

TEST_SUITE("vector_font") {

    // Helper to load the BGI vector font
    vector_font load_bgi_font() {
        auto data = test_data::load_bgi_litt();
        return font_factory::load_vector(data, 0);
    }

    TEST_CASE("default constructor creates empty font") {
        vector_font font;

        CHECK(font.get_name().empty());
        CHECK(font.get_first_char() == 0);
        CHECK(font.get_last_char() == 0);
    }

    TEST_CASE("loaded BGI font has correct properties") {
        REQUIRE(test_data::file_exists(test_data::bgi_litt()));

        auto font = load_bgi_font();

        CHECK(font.get_name() == "LITT");
        CHECK(font.get_first_char() == 32);
        CHECK(font.get_last_char() == 254);
        CHECK(font.get_default_char() == 32);
    }

    TEST_CASE("loaded BGI font has correct metrics") {
        REQUIRE(test_data::file_exists(test_data::bgi_litt()));

        auto font = load_bgi_font();
        const auto& metrics = font.get_metrics();

        CHECK(metrics.pixel_height == 9);
        CHECK(metrics.ascent == 7);
        CHECK(metrics.descent == -2);
        CHECK(metrics.baseline == 0);
        CHECK(metrics.max_width == 10);  // Maximum advance width across all glyphs
    }

    TEST_CASE("get_glyph returns valid glyph for characters in range") {
        REQUIRE(test_data::file_exists(test_data::bgi_litt()));

        auto font = load_bgi_font();

        for (uint8_t ch = font.get_first_char(); ch <= font.get_last_char(); ++ch) {
            const auto* glyph = font.get_glyph(ch);
            CHECK(glyph != nullptr);

            if (glyph) {
                // Width should be reasonable
                CHECK(glyph->width <= font.get_metrics().max_width);
            }
        }
    }

    TEST_CASE("get_glyph returns nullptr for out of range characters") {
        REQUIRE(test_data::file_exists(test_data::bgi_litt()));

        auto font = load_bgi_font();

        // Characters before first_char
        if (font.get_first_char() > 0) {
            CHECK(font.get_glyph(font.get_first_char() - 1) == nullptr);
        }

        // Characters after last_char
        if (font.get_last_char() < 255) {
            CHECK(font.get_glyph(font.get_last_char() + 1) == nullptr);
        }
    }

    TEST_CASE("has_glyph returns correct values") {
        REQUIRE(test_data::file_exists(test_data::bgi_litt()));

        auto font = load_bgi_font();

        // Characters in range
        CHECK(font.has_glyph(font.get_first_char()));
        CHECK(font.has_glyph(font.get_last_char()));

        // Characters out of range
        if (font.get_first_char() > 0) {
            CHECK_FALSE(font.has_glyph(font.get_first_char() - 1));
        }
        if (font.get_last_char() < 255) {
            CHECK_FALSE(font.has_glyph(font.get_last_char() + 1));
        }
    }

    TEST_CASE("glyph 'A' has correct stroke count") {
        REQUIRE(test_data::file_exists(test_data::bgi_litt()));

        auto font = load_bgi_font();

        REQUIRE(font.has_glyph('A'));
        const auto* glyph = font.get_glyph('A');
        REQUIRE(glyph != nullptr);

        // LITT font glyph 'A': width=6 (advance), strokes=8 (includes MOVE_TO commands)
        CHECK(glyph->width == 6);
        CHECK(glyph->strokes.size() == 8);
    }

    TEST_CASE("stroke commands have valid types") {
        REQUIRE(test_data::file_exists(test_data::bgi_litt()));

        auto font = load_bgi_font();

        for (uint8_t ch = font.get_first_char(); ch <= font.get_last_char(); ++ch) {
            const auto* glyph = font.get_glyph(ch);
            if (glyph) {
                for (const auto& stroke : glyph->strokes) {
                    // Stroke type should be one of the valid values
                    CHECK((stroke.type == stroke_type::MOVE_TO ||
                           stroke.type == stroke_type::LINE_TO ||
                           stroke.type == stroke_type::END));
                }
            }
        }
    }

    TEST_CASE("BGI font has correct glyph and stroke counts") {
        REQUIRE(test_data::file_exists(test_data::bgi_litt()));

        auto font = load_bgi_font();

        // Count glyphs with strokes and total strokes
        int glyphs_with_strokes = 0;
        size_t total_strokes = 0;

        for (uint8_t ch = font.get_first_char(); ch <= font.get_last_char(); ++ch) {
            const auto* glyph = font.get_glyph(ch);
            if (glyph && !glyph->strokes.empty()) {
                glyphs_with_strokes++;
                total_strokes += glyph->strokes.size();
            }
        }

        // LITT font: 223 glyphs with strokes, 1936 total strokes (with proper parsing)
        CHECK(glyphs_with_strokes == 223);
        CHECK(total_strokes == 1936);
    }

    TEST_CASE("glyph bounding box calculation") {
        REQUIRE(test_data::file_exists(test_data::bgi_litt()));

        auto font = load_bgi_font();

        if (font.has_glyph('W')) {
            const auto* glyph = font.get_glyph('W');
            REQUIRE(glyph != nullptr);

            // Calculate bounding box by tracing strokes
            int pen_x = 0, pen_y = 0;
            int min_x = 0, max_x = 0, min_y = 0, max_y = 0;

            for (const auto& cmd : glyph->strokes) {
                pen_x += cmd.dx;
                pen_y += cmd.dy;
                min_x = std::min(min_x, pen_x);
                max_x = std::max(max_x, pen_x);
                min_y = std::min(min_y, pen_y);
                max_y = std::max(max_y, pen_y);
            }

            // Bounding box should be reasonable
            CHECK(max_x - min_x <= static_cast<int>(glyph->width) + 2);  // Allow small margin
        }
    }

    TEST_CASE("Windows FNT vector font (ROMANP) properties") {
        REQUIRE(test_data::file_exists(test_data::fnt_romanp()));

        auto data = test_data::load_fnt_romanp();
        auto font = font_factory::load_vector(data, 0);

        // Verify ROMANP properties
        CHECK(font.get_name() == "Hershey Roman Simplex Half");
        CHECK(font.get_first_char() == 32);
        CHECK(font.get_last_char() == 127);

        const auto& metrics = font.get_metrics();
        CHECK(metrics.pixel_height == 16);
        CHECK(metrics.ascent == 12);
        CHECK(metrics.max_width == 17);

        // Count glyphs and strokes
        int glyphs_with_strokes = 0;
        size_t total_strokes = 0;
        for (uint8_t ch = font.get_first_char(); ch <= font.get_last_char(); ++ch) {
            const auto* glyph = font.get_glyph(ch);
            if (glyph && !glyph->strokes.empty()) {
                glyphs_with_strokes++;
                total_strokes += glyph->strokes.size();
            }
        }

        // ROMANP: 95 glyphs with strokes, 1024 total strokes (includes PEN_UP markers)
        CHECK(glyphs_with_strokes == 95);
        CHECK(total_strokes == 1024);

        // Verify glyph 'A'
        const auto* glyph_A = font.get_glyph('A');
        REQUIRE(glyph_A != nullptr);
        CHECK(glyph_A->width == 10);
        CHECK(glyph_A->strokes.size() == 9);  // Includes leading PEN_UP + 3 polylines
    }

    TEST_CASE("stroke delta values are within int8_t range") {
        REQUIRE(test_data::file_exists(test_data::bgi_litt()));

        auto font = load_bgi_font();

        for (uint8_t ch = font.get_first_char(); ch <= font.get_last_char(); ++ch) {
            const auto* glyph = font.get_glyph(ch);
            if (glyph) {
                for (const auto& stroke : glyph->strokes) {
                    // dx and dy are int8_t, so they should be in range [-128, 127]
                    CHECK(stroke.dx >= -128);
                    CHECK(stroke.dx <= 127);
                    CHECK(stroke.dy >= -128);
                    CHECK(stroke.dy <= 127);
                }
            }
        }
    }

    TEST_CASE("all glyphs have reasonable width") {
        REQUIRE(test_data::file_exists(test_data::bgi_litt()));

        auto font = load_bgi_font();
        const auto& metrics = font.get_metrics();

        for (uint8_t ch = font.get_first_char(); ch <= font.get_last_char(); ++ch) {
            const auto* glyph = font.get_glyph(ch);
            if (glyph) {
                CHECK(glyph->width <= metrics.max_width);
            }
        }
    }

    TEST_CASE("get_default_char returns space character") {
        REQUIRE(test_data::file_exists(test_data::bgi_litt()));

        auto font = load_bgi_font();

        // LITT font uses space (32) as default char
        CHECK(font.get_default_char() == 32);
    }
}
