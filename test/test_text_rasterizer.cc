//
// Created by igor on 21/12/2025.
//
// Unit tests for text_rasterizer
//

#include <doctest/doctest.h>
#include <onyx_font/text/text_rasterizer.hh>
#include <onyx_font/text/raster_target.hh>
#include <onyx_font/font_factory.hh>
#include "test_data.hh"
#include <cmath>

using namespace onyx_font;
using namespace onyx_font::test;

TEST_SUITE("text_rasterizer") {

    TEST_CASE("construction and size") {
        auto data = test_data::load_fon_helva();
        auto font = font_factory::load_bitmap(data, 0);
        auto source = font_source::from_bitmap(font);

        text_rasterizer raster(std::move(source));
        CHECK(raster.size() == 12.0f);  // Default size

        raster.set_size(24.0f);
        CHECK(raster.size() == 24.0f);
    }

    TEST_CASE("get_metrics bitmap") {
        auto data = test_data::load_fon_helva();
        auto font = font_factory::load_bitmap(data, 0);
        auto source = font_source::from_bitmap(font);

        text_rasterizer raster(std::move(source));

        // Setting different size shouldn't affect bitmap font metrics
        raster.set_size(24.0f);

        auto metrics = raster.get_metrics();
        CHECK(metrics.line_height > 0);
        CHECK(metrics.ascent > 0);
        // Bitmap fonts return native metrics
        CHECK(metrics.line_height == source.native_size());
    }

    TEST_CASE("get_metrics ttf") {
        if (!test_data::file_exists(test_data::ttf_arial())) {
            WARN("Arial TTF not available");
            return;
        }

        auto data = test_data::load_ttf_arial();
        ttf_font ttf(data);
        stb_truetype_font stb(data);
        auto source = font_source::from_ttf(ttf);

        text_rasterizer raster(std::move(source));
        raster.set_size(24.0f);

        auto metrics = raster.get_metrics();
        CHECK(metrics.line_height > 0);
        CHECK(metrics.ascent > 0);
    }

    TEST_CASE("measure_glyph") {
        auto data = test_data::load_fon_helva();
        auto font = font_factory::load_bitmap(data, 0);
        auto source = font_source::from_bitmap(font);

        text_rasterizer raster(std::move(source));

        auto metrics_a = raster.measure_glyph('A');
        auto metrics_i = raster.measure_glyph('i');

        CHECK(metrics_a.advance_x > 0);
        CHECK(metrics_i.advance_x > 0);
        CHECK(metrics_a.advance_x >= metrics_i.advance_x);  // 'A' is wider
    }

    TEST_CASE("measure_text basic") {
        auto data = test_data::load_fon_helva();
        auto font = font_factory::load_bitmap(data, 0);
        auto source = font_source::from_bitmap(font);

        text_rasterizer raster(std::move(source));

        auto hello = raster.measure_text("Hello");
        auto hello_world = raster.measure_text("Hello World");

        CHECK(hello.width > 0);
        CHECK(hello.height > 0);
        CHECK(hello_world.width > hello.width);
        CHECK(hello.height == hello_world.height);  // Same line height
    }

    TEST_CASE("measure_text empty") {
        auto data = test_data::load_fon_helva();
        auto font = font_factory::load_bitmap(data, 0);
        auto source = font_source::from_bitmap(font);

        text_rasterizer raster(std::move(source));

        auto empty = raster.measure_text("");
        CHECK(empty.width == 0);
        CHECK(empty.height == 0);
    }

    TEST_CASE("measure_text UTF-8") {
        if (!test_data::file_exists(test_data::ttf_arial())) {
            return;
        }

        auto data = test_data::load_ttf_arial();
        ttf_font ttf(data);
        stb_truetype_font stb(data);
        auto source = font_source::from_ttf(ttf);

        text_rasterizer raster(std::move(source));
        raster.set_size(24.0f);

        // Measure text with accented characters
        auto cafe = raster.measure_text("café");
        CHECK(cafe.width > 0);
        CHECK(cafe.height > 0);
    }

    TEST_CASE("line_height") {
        auto data = test_data::load_fon_helva();
        auto font = font_factory::load_bitmap(data, 0);
        auto source = font_source::from_bitmap(font);

        text_rasterizer raster(std::move(source));

        float lh = raster.line_height();
        CHECK(lh > 0);
        CHECK(lh == raster.get_metrics().line_height);
    }

    TEST_CASE("rasterize_glyph") {
        auto data = test_data::load_fon_helva();
        auto font = font_factory::load_bitmap(data, 0);
        auto source = font_source::from_bitmap(font);

        text_rasterizer raster(std::move(source));

        uint8_t buffer[30 * 20] = {0};
        grayscale_target target(buffer, 30, 20);

        auto metrics = raster.get_metrics();
        int baseline = static_cast<int>(metrics.ascent);

        raster.rasterize_glyph('A', target, 2, baseline);

        // Should have some non-zero pixels
        int nonzero = 0;
        for (auto b : buffer) {
            if (b > 0) ++nonzero;
        }
        CHECK(nonzero > 0);
    }

    TEST_CASE("rasterize_text bitmap") {
        auto data = test_data::load_fon_helva();
        auto font = font_factory::load_bitmap(data, 0);
        auto source = font_source::from_bitmap(font);

        text_rasterizer raster(std::move(source));

        auto extents = raster.measure_text("Hi");
        int w = static_cast<int>(std::ceil(extents.width)) + 10;  // Extra margin
        int h = static_cast<int>(std::ceil(extents.height)) + 4;

        std::vector<uint8_t> buffer(static_cast<size_t>(w * h), 0);
        grayscale_target target(buffer.data(), w, h);

        float width = raster.rasterize_text("Hi", target, 2,
                                             static_cast<int>(extents.ascent));

        CHECK(width > 0);
        CHECK(width == doctest::Approx(extents.width).epsilon(1.0));

        // Verify pixels were written
        int nonzero = 0;
        for (auto b : buffer) {
            if (b > 0) ++nonzero;
        }
        CHECK(nonzero > 0);
    }

    TEST_CASE("rasterize_text ttf") {
        if (!test_data::file_exists(test_data::ttf_arial())) {
            return;
        }

        auto data = test_data::load_ttf_arial();
        ttf_font ttf(data);
        stb_truetype_font stb(data);
        auto source = font_source::from_ttf(ttf);

        text_rasterizer raster(std::move(source));
        raster.set_size(24.0f);

        auto extents = raster.measure_text("Hello");
        int w = static_cast<int>(std::ceil(extents.width)) + 10;
        int h = static_cast<int>(std::ceil(extents.height)) + 4;

        std::vector<uint8_t> buffer(static_cast<size_t>(w * h), 0);
        grayscale_target target(buffer.data(), w, h);

        float width = raster.rasterize_text("Hello", target, 2,
                                             static_cast<int>(extents.ascent));

        CHECK(width > 0);

        // Verify pixels were written
        int nonzero = 0;
        for (auto b : buffer) {
            if (b > 0) ++nonzero;
        }
        CHECK(nonzero > 0);

        // TTF should have antialiased pixels
        bool has_aa = false;
        for (auto b : buffer) {
            if (b > 0 && b < 255) has_aa = true;
        }
        CHECK(has_aa);
    }

    TEST_CASE("rasterize_text empty") {
        auto data = test_data::load_fon_helva();
        auto font = font_factory::load_bitmap(data, 0);
        auto source = font_source::from_bitmap(font);

        text_rasterizer raster(std::move(source));

        uint8_t buffer[100] = {0};
        grayscale_target target(buffer, 10, 10);

        float width = raster.rasterize_text("", target, 0, 5);
        CHECK(width == 0.0f);

        // No pixels should be written
        for (auto b : buffer) {
            CHECK(b == 0);
        }
    }

    TEST_CASE("rasterize_text with callback") {
        auto data = test_data::load_fon_helva();
        auto font = font_factory::load_bitmap(data, 0);
        auto source = font_source::from_bitmap(font);

        text_rasterizer raster(std::move(source));

        struct callback_data {
            int count = 0;
            int last_x = 0;
        };
        callback_data cb_data;

        uint8_t buffer[100 * 20] = {0};
        grayscale_target target(buffer, 100, 20);

        auto callback = [](char32_t, int x, int, const glyph_metrics&, void* user) {
            auto* data = static_cast<callback_data*>(user);
            data->count++;
            data->last_x = x;
        };

        raster.rasterize_text("ABC", target, 10, 10, callback, &cb_data);

        CHECK(cb_data.count == 3);  // Three characters
        CHECK(cb_data.last_x > 10);  // Last character is to the right
    }

    TEST_CASE("measure_wrapped basic") {
        auto data = test_data::load_fon_helva();
        auto font = font_factory::load_bitmap(data, 0);
        auto source = font_source::from_bitmap(font);

        text_rasterizer raster(std::move(source));

        auto single = raster.measure_text("Hello World");
        auto wrapped = raster.measure_wrapped("Hello World", 30.0f);

        // Wrapped should have more height (multiple lines)
        CHECK(wrapped.height >= single.height);
        // Wrapped width should not exceed max_width (or close to it)
        CHECK(wrapped.width <= single.width);
    }

    TEST_CASE("measure_wrapped newlines") {
        auto data = test_data::load_fon_helva();
        auto font = font_factory::load_bitmap(data, 0);
        auto source = font_source::from_bitmap(font);

        text_rasterizer raster(std::move(source));

        auto two_lines = raster.measure_wrapped("Hello\nWorld", 1000.0f);
        auto one_line = raster.measure_text("Hello");

        // Two lines should have double the height
        CHECK(two_lines.height == doctest::Approx(2.0f * one_line.height).epsilon(0.1f));
    }

    TEST_CASE("kerning affects width") {
        if (!test_data::file_exists(test_data::ttf_arial())) {
            return;
        }

        auto data = test_data::load_ttf_arial();
        ttf_font ttf(data);
        stb_truetype_font stb(data);
        auto source = font_source::from_ttf(ttf);

        text_rasterizer raster(std::move(source));
        raster.set_size(48.0f);  // Larger size for more noticeable kerning

        auto av = raster.measure_text("AV");
        auto aa = raster.measure_text("AA");

        // AV typically has negative kerning, so should be narrower
        // (This depends on the font having kerning data)
        CHECK(av.width <= aa.width);
    }
}
