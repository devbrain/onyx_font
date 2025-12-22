//
// Created by igor on 21/12/2025.
//
// Unit tests for raster target concept and implementations
//

#include <doctest/doctest.h>
#include <onyx_font/text/raster_target.hh>
#include <vector>
#include <tuple>
#include <set>

using namespace onyx_font;

TEST_SUITE("raster_target") {

    TEST_CASE("concept satisfaction") {
        // Verify all targets satisfy the raster_target concept
        static_assert(raster_target<grayscale_target>);
        static_assert(raster_target<grayscale_max_target>);
        static_assert(raster_target<rgba_blend_target>);
        static_assert(raster_target<null_target>);
        static_assert(raster_target<owned_grayscale_target>);
        static_assert(raster_target<callback_target<std::function<void(int,int,uint8_t)>>>);

        // Verify span support
        static_assert(raster_target_with_span<grayscale_target>);
        static_assert(raster_target_with_span<null_target>);
        static_assert(raster_target_with_span<owned_grayscale_target>);
        static_assert(!raster_target_with_span<grayscale_max_target>);
        static_assert(!raster_target_with_span<rgba_blend_target>);
    }

    TEST_CASE("grayscale_target basic operations") {
        uint8_t buffer[10 * 10] = {0};
        grayscale_target target(buffer, 10, 10);

        CHECK(target.width() == 10);
        CHECK(target.height() == 10);
        CHECK(target.stride() == 10);

        // Write a pixel
        target.put_pixel(5, 5, 128);
        CHECK(buffer[5 * 10 + 5] == 128);

        // Write another pixel
        target.put_pixel(0, 0, 255);
        CHECK(buffer[0] == 255);

        // Overwrite
        target.put_pixel(5, 5, 64);
        CHECK(buffer[5 * 10 + 5] == 64);
    }

    TEST_CASE("grayscale_target clipping") {
        uint8_t buffer[10 * 10] = {0};
        grayscale_target target(buffer, 10, 10);

        // Out of bounds should be ignored
        target.put_pixel(-1, 0, 255);
        target.put_pixel(10, 0, 255);
        target.put_pixel(0, -1, 255);
        target.put_pixel(0, 10, 255);

        // Verify no writes occurred
        for (int i = 0; i < 100; ++i) {
            CHECK(buffer[i] == 0);
        }
    }

    TEST_CASE("grayscale_target span") {
        uint8_t buffer[10 * 10] = {0};
        grayscale_target target(buffer, 10, 10);

        uint8_t span[] = {10, 20, 30, 40, 50};
        target.put_span(2, 3, span, 5);

        CHECK(buffer[3 * 10 + 2] == 10);
        CHECK(buffer[3 * 10 + 3] == 20);
        CHECK(buffer[3 * 10 + 4] == 30);
        CHECK(buffer[3 * 10 + 5] == 40);
        CHECK(buffer[3 * 10 + 6] == 50);
    }

    TEST_CASE("grayscale_target span clipping left") {
        uint8_t buffer[10 * 10] = {0};
        grayscale_target target(buffer, 10, 10);

        uint8_t span[] = {10, 20, 30, 40, 50};
        target.put_span(-2, 0, span, 5);  // Start 2 pixels left of buffer

        // First two pixels should be clipped
        CHECK(buffer[0] == 30);  // Third element of span
        CHECK(buffer[1] == 40);
        CHECK(buffer[2] == 50);
    }

    TEST_CASE("grayscale_target span clipping right") {
        uint8_t buffer[10 * 10] = {0};
        grayscale_target target(buffer, 10, 10);

        uint8_t span[] = {10, 20, 30, 40, 50};
        target.put_span(7, 0, span, 5);  // Extends past right edge

        CHECK(buffer[7] == 10);
        CHECK(buffer[8] == 20);
        CHECK(buffer[9] == 30);
        // 40 and 50 should be clipped
    }

    TEST_CASE("grayscale_target span out of bounds y") {
        uint8_t buffer[10 * 10] = {0};
        grayscale_target target(buffer, 10, 10);

        uint8_t span[] = {255, 255, 255};
        target.put_span(0, -1, span, 3);
        target.put_span(0, 10, span, 3);

        // No writes should have occurred
        for (int i = 0; i < 100; ++i) {
            CHECK(buffer[i] == 0);
        }
    }

    TEST_CASE("grayscale_target with stride") {
        // Stride larger than width (e.g., aligned buffer)
        uint8_t buffer[16 * 10] = {0};  // 16-byte aligned rows
        grayscale_target target(buffer, 10, 10, 16);

        CHECK(target.stride() == 16);

        target.put_pixel(5, 2, 128);
        CHECK(buffer[2 * 16 + 5] == 128);
    }

    TEST_CASE("grayscale_max_target") {
        uint8_t buffer[10 * 10] = {0};
        grayscale_max_target target(buffer, 10, 10);

        // First write
        target.put_pixel(5, 5, 100);
        CHECK(buffer[5 * 10 + 5] == 100);

        // Lower value should not overwrite
        target.put_pixel(5, 5, 50);
        CHECK(buffer[5 * 10 + 5] == 100);

        // Higher value should overwrite
        target.put_pixel(5, 5, 200);
        CHECK(buffer[5 * 10 + 5] == 200);
    }

    TEST_CASE("rgba_blend_target basic") {
        uint32_t buffer[4] = {0};
        rgba_blend_target target(buffer, 2, 2, 0xFF0000);  // Red

        CHECK(target.width() == 2);
        CHECK(target.height() == 2);

        // Full alpha should write direct color
        target.put_pixel(0, 0, 255);
        CHECK(buffer[0] == 0xFFFF0000);  // ARGB: fully opaque red
    }

    TEST_CASE("rgba_blend_target alpha blend") {
        uint32_t buffer[1] = {0xFF00FF00};  // Opaque green background
        rgba_blend_target target(buffer, 1, 1, 0xFF0000);  // Red text

        // 50% alpha blend
        target.put_pixel(0, 0, 128);

        // Result should be approximately 50% red + 50% green
        uint8_t r = (buffer[0] >> 16) & 0xFF;
        uint8_t g = (buffer[0] >> 8) & 0xFF;

        CHECK(r > 100);  // Should have significant red
        CHECK(g > 100);  // Should have significant green
    }

    TEST_CASE("rgba_blend_target zero alpha ignored") {
        uint32_t buffer[1] = {0xFF00FF00};  // Opaque green
        rgba_blend_target target(buffer, 1, 1, 0xFF0000);

        target.put_pixel(0, 0, 0);  // Zero alpha - should not affect

        CHECK(buffer[0] == 0xFF00FF00);  // Unchanged
    }

    TEST_CASE("rgba_blend_target set_color") {
        uint32_t buffer[1] = {0};
        rgba_blend_target target(buffer, 1, 1, 0xFF0000);

        target.put_pixel(0, 0, 255);
        CHECK(buffer[0] == 0xFFFF0000);  // Red

        buffer[0] = 0;
        target.set_color(0x0000FF);  // Change to blue
        target.put_pixel(0, 0, 255);
        CHECK(buffer[0] == 0xFF0000FF);  // Blue
    }

    TEST_CASE("callback_target") {
        std::vector<std::tuple<int, int, uint8_t>> pixels;

        callback_target target(100, 100, [&](int x, int y, uint8_t a) {
            pixels.emplace_back(x, y, a);
        });

        CHECK(target.width() == 100);
        CHECK(target.height() == 100);

        target.put_pixel(10, 20, 255);
        target.put_pixel(30, 40, 128);

        REQUIRE(pixels.size() == 2);
        CHECK(std::get<0>(pixels[0]) == 10);
        CHECK(std::get<1>(pixels[0]) == 20);
        CHECK(std::get<2>(pixels[0]) == 255);
        CHECK(std::get<0>(pixels[1]) == 30);
        CHECK(std::get<1>(pixels[1]) == 40);
        CHECK(std::get<2>(pixels[1]) == 128);
    }

    TEST_CASE("callback_target zero alpha filtered") {
        std::vector<std::tuple<int, int, uint8_t>> pixels;

        callback_target target(100, 100, [&](int x, int y, uint8_t a) {
            pixels.emplace_back(x, y, a);
        });

        target.put_pixel(10, 10, 0);  // Zero alpha

        CHECK(pixels.empty());
    }

    TEST_CASE("callback_target clipping") {
        std::vector<std::tuple<int, int, uint8_t>> pixels;

        callback_target target(10, 10, [&](int x, int y, uint8_t a) {
            pixels.emplace_back(x, y, a);
        });

        target.put_pixel(-1, 0, 255);
        target.put_pixel(10, 0, 255);
        target.put_pixel(0, -1, 255);
        target.put_pixel(0, 10, 255);

        CHECK(pixels.empty());
    }

    TEST_CASE("null_target") {
        null_target target(100, 100);

        CHECK(target.width() == 100);
        CHECK(target.height() == 100);

        // Should not crash
        target.put_pixel(50, 50, 255);
        target.put_pixel(-1, -1, 255);
        target.put_pixel(1000, 1000, 255);

        uint8_t span[] = {1, 2, 3};
        target.put_span(0, 0, span, 3);
    }

    TEST_CASE("owned_grayscale_target") {
        owned_grayscale_target target(10, 10);

        CHECK(target.width() == 10);
        CHECK(target.height() == 10);

        // Should be initialized to zero
        for (int y = 0; y < 10; ++y) {
            for (int x = 0; x < 10; ++x) {
                CHECK(target.pixel(x, y) == 0);
            }
        }

        target.put_pixel(5, 5, 200);
        CHECK(target.pixel(5, 5) == 200);

        target.clear();
        CHECK(target.pixel(5, 5) == 0);
    }

    TEST_CASE("owned_grayscale_target span") {
        owned_grayscale_target target(10, 10);

        uint8_t span[] = {10, 20, 30};
        target.put_span(2, 3, span, 3);

        CHECK(target.pixel(2, 3) == 10);
        CHECK(target.pixel(3, 3) == 20);
        CHECK(target.pixel(4, 3) == 30);
    }

    TEST_CASE("emit_span with span support") {
        uint8_t buffer[10 * 10] = {0};
        grayscale_target target(buffer, 10, 10);

        uint8_t span[] = {100, 150, 200};
        emit_span(target, 2, 2, span, 3);

        CHECK(buffer[2 * 10 + 2] == 100);
        CHECK(buffer[2 * 10 + 3] == 150);
        CHECK(buffer[2 * 10 + 4] == 200);
    }

    TEST_CASE("emit_span without span support") {
        uint8_t buffer[10 * 10] = {0};
        grayscale_max_target target(buffer, 10, 10);

        uint8_t span[] = {100, 0, 200};  // Middle one is zero
        emit_span(target, 2, 2, span, 3);

        CHECK(buffer[2 * 10 + 2] == 100);
        CHECK(buffer[2 * 10 + 3] == 0);   // Zero alpha skipped
        CHECK(buffer[2 * 10 + 4] == 200);
    }
}
