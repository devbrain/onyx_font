//
// Created by igor on 22/12/2025.
//
// Unit tests for font_converter
//

#include <doctest/doctest.h>
#include <onyx_font/font_converter.hh>
#include <onyx_font/font_factory.hh>
#include "test_data.hh"

using namespace onyx_font;
using namespace onyx_font::test;

TEST_SUITE("font_converter") {

    TEST_CASE("convert vector font to bitmap") {
        auto data = test_data::load_bgi_litt();
        auto vector_font = font_factory::load_vector(data, 0);

        REQUIRE(vector_font.get_first_char() <= 'A');
        REQUIRE(vector_font.get_last_char() >= 'Z');

        // Convert at 2x native size
        float target_size = static_cast<float>(vector_font.get_metrics().pixel_height) * 2.0f;
        auto bitmap = font_converter::from_vector(vector_font, target_size);

        // Verify basic properties
        CHECK(bitmap.get_first_char() == vector_font.get_first_char());
        CHECK(bitmap.get_last_char() == vector_font.get_last_char());
        CHECK(bitmap.get_metrics().pixel_height > 0);

        // Verify we can get glyphs
        auto glyph_a = bitmap.get_glyph('A');
        CHECK(glyph_a.width() > 0);
        CHECK(glyph_a.height() > 0);

        // Verify glyph has actual pixels
        int pixel_count = 0;
        for (uint16_t y = 0; y < glyph_a.height(); ++y) {
            for (uint16_t x = 0; x < glyph_a.width(); ++x) {
                if (glyph_a.pixel(x, y)) {
                    ++pixel_count;
                }
            }
        }
        CHECK(pixel_count > 0);
    }

    TEST_CASE("convert vector font with custom options") {
        auto data = test_data::load_bgi_litt();
        auto vector_font = font_factory::load_vector(data, 0);

        conversion_options options;
        options.first_char = 'A';
        options.last_char = 'Z';
        options.threshold = 64;  // Lower threshold

        float target_size = 24.0f;
        auto bitmap = font_converter::from_vector(vector_font, target_size, options);

        CHECK(bitmap.get_first_char() == 'A');
        CHECK(bitmap.get_last_char() == 'Z');

        // All uppercase letters should have glyphs
        for (char c = 'A'; c <= 'Z'; ++c) {
            auto glyph = bitmap.get_glyph(static_cast<uint8_t>(c));
            CHECK(glyph.width() > 0);
        }
    }

    TEST_CASE("convert vector font aliased mode") {
        auto data = test_data::load_bgi_litt();
        auto vector_font = font_factory::load_vector(data, 0);

        conversion_options options;
        options.antialias = false;  // Use aliased (Bresenham) rendering

        float target_size = 20.0f;
        auto bitmap = font_converter::from_vector(vector_font, target_size, options);

        // Should still produce valid glyphs
        auto glyph = bitmap.get_glyph('H');
        CHECK(glyph.width() > 0);
        CHECK(glyph.height() > 0);
    }

    TEST_CASE("convert TTF font to bitmap") {
        if (!test_data::file_exists(test_data::ttf_arial())) {
            WARN("Arial TTF not available - skipping test");
            return;
        }

        auto data = test_data::load_ttf_arial();
        ttf_font ttf(data);
        REQUIRE(ttf.is_valid());

        // Use limited range for faster test
        conversion_options options;
        options.first_char = 'A';
        options.last_char = 'z';

        auto bitmap = font_converter::from_ttf(ttf, 24.0f, options);

        // Verify basic properties
        CHECK(bitmap.get_metrics().pixel_height > 0);
        CHECK(bitmap.get_metrics().ascent > 0);

        // Verify we can get glyphs
        auto glyph_a = bitmap.get_glyph('A');
        CHECK(glyph_a.width() > 0);
        CHECK(glyph_a.height() > 0);

        // Verify glyph has actual pixels
        int pixel_count = 0;
        for (uint16_t y = 0; y < glyph_a.height(); ++y) {
            for (uint16_t x = 0; x < glyph_a.width(); ++x) {
                if (glyph_a.pixel(x, y)) {
                    ++pixel_count;
                }
            }
        }
        CHECK(pixel_count > 0);
    }

    TEST_CASE("convert TTF font with custom character range") {
        if (!test_data::file_exists(test_data::ttf_arial())) {
            WARN("Arial TTF not available - skipping test");
            return;
        }

        auto data = test_data::load_ttf_arial();
        ttf_font ttf(data);
        REQUIRE(ttf.is_valid());

        conversion_options options;
        options.first_char = '0';
        options.last_char = '9';

        auto bitmap = font_converter::from_ttf(ttf, 16.0f, options);

        CHECK(bitmap.get_first_char() == '0');
        CHECK(bitmap.get_last_char() == '9');

        // All digits should have glyphs
        for (char c = '0'; c <= '9'; ++c) {
            auto glyph = bitmap.get_glyph(static_cast<uint8_t>(c));
            CHECK(glyph.width() > 0);
        }
    }

    TEST_CASE("convert TTF font different sizes") {
        if (!test_data::file_exists(test_data::ttf_arial())) {
            WARN("Arial TTF not available - skipping test");
            return;
        }

        auto data = test_data::load_ttf_arial();
        ttf_font ttf(data);
        REQUIRE(ttf.is_valid());

        conversion_options options;
        options.first_char = 'A';
        options.last_char = 'A';

        // Convert at different sizes
        auto bitmap_small = font_converter::from_ttf(ttf, 12.0f, options);
        auto bitmap_large = font_converter::from_ttf(ttf, 48.0f, options);

        auto glyph_small = bitmap_small.get_glyph('A');
        auto glyph_large = bitmap_large.get_glyph('A');

        // Larger size should have larger glyph
        CHECK(glyph_large.width() > glyph_small.width());
        CHECK(glyph_large.height() > glyph_small.height());
    }

    TEST_CASE("convert TTF font threshold effect") {
        if (!test_data::file_exists(test_data::ttf_arial())) {
            WARN("Arial TTF not available - skipping test");
            return;
        }

        auto data = test_data::load_ttf_arial();
        ttf_font ttf(data);
        REQUIRE(ttf.is_valid());

        conversion_options low_threshold;
        low_threshold.first_char = 'O';
        low_threshold.last_char = 'O';
        low_threshold.threshold = 32;  // Low threshold - more pixels

        conversion_options high_threshold;
        high_threshold.first_char = 'O';
        high_threshold.last_char = 'O';
        high_threshold.threshold = 224;  // High threshold - fewer pixels

        auto bitmap_low = font_converter::from_ttf(ttf, 32.0f, low_threshold);
        auto bitmap_high = font_converter::from_ttf(ttf, 32.0f, high_threshold);

        auto glyph_low = bitmap_low.get_glyph('O');
        auto glyph_high = bitmap_high.get_glyph('O');

        // Count pixels
        int count_low = 0, count_high = 0;
        for (uint16_t y = 0; y < glyph_low.height(); ++y) {
            for (uint16_t x = 0; x < glyph_low.width(); ++x) {
                if (glyph_low.pixel(x, y)) ++count_low;
            }
        }
        for (uint16_t y = 0; y < glyph_high.height(); ++y) {
            for (uint16_t x = 0; x < glyph_high.width(); ++x) {
                if (glyph_high.pixel(x, y)) ++count_high;
            }
        }

        // Low threshold should have more pixels
        CHECK(count_low > count_high);
    }

    TEST_CASE("converted bitmap font can render text") {
        auto data = test_data::load_bgi_litt();
        auto vector_font = font_factory::load_vector(data, 0);

        auto bitmap = font_converter::from_vector(vector_font, 24.0f);

        // Simulate rendering "HI"
        int pen_x = 0;
        std::string text = "HI";

        for (char c : text) {
            auto glyph = bitmap.get_glyph(static_cast<uint8_t>(c));
            auto spacing = bitmap.get_spacing(static_cast<uint8_t>(c));

            // Apply A-space
            if (spacing.a_space) {
                pen_x += *spacing.a_space;
            }

            // Advance using B-space or glyph width
            if (spacing.b_space) {
                pen_x += *spacing.b_space;
            } else {
                pen_x += glyph.width();
            }

            // Apply C-space
            if (spacing.c_space) {
                pen_x += *spacing.c_space;
            }
        }

        // Should have advanced some distance
        CHECK(pen_x > 0);
    }

    TEST_CASE("empty font conversion") {
        // Create an empty vector font scenario by using invalid range
        auto data = test_data::load_bgi_litt();
        auto vector_font = font_factory::load_vector(data, 0);

        conversion_options options;
        options.first_char = 250;  // Beyond typical range
        options.last_char = 255;

        auto bitmap = font_converter::from_vector(vector_font, 16.0f, options);

        // Should handle gracefully - may have placeholder glyphs
        CHECK(bitmap.get_metrics().pixel_height > 0);
    }
}
