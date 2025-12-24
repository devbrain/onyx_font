//
// Created by igor on 21/12/2025.
//
// Unit tests for glyph_rasterizer
//

#include <doctest/doctest.h>
#include <onyx_font/text/glyph_rasterizer.hh>
#include <onyx_font/text/raster_target.hh>
#include <onyx_font/font_factory.hh>
#include "test_data.hh"

using namespace onyx_font;
using namespace onyx_font::internal;
using namespace onyx_font::test;

TEST_SUITE("glyph_rasterizer") {

    TEST_CASE("rasterize bitmap glyph") {
        auto data = test_data::load_fon_helva();
        auto font = font_factory::load_bitmap(data, 0);

        auto glyph = font.get_glyph('A');
        REQUIRE(glyph.width() > 0);

        uint8_t buffer[20 * 10] = {0};
        grayscale_target target(buffer, 20, 10);

        rasterize_bitmap_glyph(glyph, target, 0, 0);

        // Should have pixels (all 255 for bitmap)
        int count = 0;
        for (auto b : buffer) {
            if (b == 255) count++;
        }
        CHECK(count > 0);
    }

    TEST_CASE("rasterize vector glyph aliased") {
        auto data = test_data::load_bgi_litt();
        auto font = font_factory::load_vector(data, 0);

        const auto* glyph = font.get_glyph('A');
        REQUIRE(glyph != nullptr);

        uint8_t buffer[30 * 30] = {0};
        grayscale_target target(buffer, 30, 30);

        rasterize_vector_glyph(*glyph, 2.0f, target, 5, 20, raster_mode::aliased);

        int count = 0;
        for (auto b : buffer) {
            if (b == 255) count++;
        }
        CHECK(count > 0);
    }

    TEST_CASE("rasterize vector glyph antialiased") {
        auto data = test_data::load_bgi_litt();
        auto font = font_factory::load_vector(data, 0);

        const auto* glyph = font.get_glyph('A');
        REQUIRE(glyph != nullptr);

        uint8_t buffer[30 * 30] = {0};
        grayscale_target target(buffer, 30, 30);

        // Use non-integer scale to ensure coordinates don't land on pixel boundaries,
        // which produces intermediate coverage values in Wu's algorithm
        rasterize_vector_glyph(*glyph, 2.3f, target, 5, 20, raster_mode::antialiased);

        // Should have antialiased pixels (intermediate values)
        bool has_aa = false;
        bool has_pixels = false;
        for (auto b : buffer) {
            if (b > 0 && b < 255) has_aa = true;
            if (b > 0) has_pixels = true;
        }
        CHECK(has_pixels);
        // Anti-aliased output should have intermediate values
        CHECK(has_aa);
    }

    TEST_CASE("rasterize TTF glyph") {
        if (!test_data::file_exists(test_data::ttf_arial())) {
            WARN("Arial TTF not available");
            return;
        }

        auto data = test_data::load_ttf_arial();
        stb_truetype_font font(data);
        REQUIRE(font.is_valid());

        uint8_t buffer[30 * 30] = {0};
        grayscale_target target(buffer, 30, 30);

        bool success = rasterize_ttf_glyph(font, 'A', 24.0f, target, 5, 25);

        CHECK(success);

        // stb_truetype produces antialiased output
        bool has_aa = false;
        for (auto b : buffer) {
            if (b > 0 && b < 255) has_aa = true;
        }
        CHECK(has_aa);
    }

    TEST_CASE("scaling vector glyph") {
        auto data = test_data::load_bgi_litt();
        auto font = font_factory::load_vector(data, 0);

        const auto* glyph = font.get_glyph('A');
        REQUIRE(glyph != nullptr);

        // Rasterize at 1x scale
        uint8_t buffer1[50 * 50] = {0};
        grayscale_target target1(buffer1, 50, 50);
        rasterize_vector_glyph(*glyph, 1.0f, target1, 5, 20, raster_mode::aliased);

        // Rasterize at 3x scale
        uint8_t buffer3[50 * 50] = {0};
        grayscale_target target3(buffer3, 50, 50);
        rasterize_vector_glyph(*glyph, 3.0f, target3, 5, 40, raster_mode::aliased);

        // 3x scale should have more pixels
        int count1 = 0, count3 = 0;
        for (auto b : buffer1) if (b > 0) count1++;
        for (auto b : buffer3) if (b > 0) count3++;
        CHECK(count3 > count1);
    }

    TEST_CASE("vector glyph different characters") {
        auto data = test_data::load_bgi_litt();
        auto font = font_factory::load_vector(data, 0);

        // Test multiple characters
        for (char c : {'H', 'I', 'W', 'M'}) {
            const auto* glyph = font.get_glyph(static_cast<uint8_t>(c));
            if (!glyph) continue;

            uint8_t buffer[40 * 40] = {0};
            grayscale_target target(buffer, 40, 40);

            rasterize_vector_glyph(*glyph, 2.0f, target, 5, 30, raster_mode::antialiased);

            int count = 0;
            for (auto b : buffer) if (b > 0) count++;
            CHECK_MESSAGE(count > 0, "Character '", c, "' should have pixels");
        }
    }

    TEST_CASE("bitmap glyph positioning") {
        auto data = test_data::load_fon_helva();
        auto font = font_factory::load_bitmap(data, 0);

        auto glyph = font.get_glyph('H');
        REQUIRE(glyph.width() > 0);

        // Rasterize at different positions
        uint8_t buffer1[20 * 20] = {0};
        grayscale_target target1(buffer1, 20, 20);
        rasterize_bitmap_glyph(glyph, target1, 0, 0);

        uint8_t buffer2[20 * 20] = {0};
        grayscale_target target2(buffer2, 20, 20);
        rasterize_bitmap_glyph(glyph, target2, 5, 5);

        // Same number of pixels, different positions
        int count1 = 0, count2 = 0;
        for (auto b : buffer1) if (b > 0) count1++;
        for (auto b : buffer2) if (b > 0) count2++;
        CHECK(count1 == count2);

        // Buffers should be different (different positions)
        bool different = false;
        for (int i = 0; i < 400; ++i) {
            if (buffer1[i] != buffer2[i]) {
                different = true;
                break;
            }
        }
        CHECK(different);
    }
}
