//
// Created by igor on 21/12/2025.
//
// Integration tests for the complete text rendering pipeline
//

#include <doctest/doctest.h>
#include <onyx_font/text/text_renderer.hh>
#include <onyx_font/text/glyph_cache.hh>
#include <onyx_font/text/font_source.hh>
#include <onyx_font/font_factory.hh>
#include "test_data.hh"
#include <algorithm>
#include <set>

using namespace onyx_font;
using namespace onyx_font::test;

TEST_SUITE("text_rendering_integration") {

    TEST_CASE("full pipeline with TTF") {
        if (!test_data::file_exists(test_data::ttf_arial())) {
            WARN("Arial TTF not available");
            return;
        }

        // Load font
        auto data = test_data::load_ttf_arial();
        ttf_font ttf(data);
        stb_truetype_font stb(data);
        auto source = font_source::from_ttf(ttf);

        // Create cache and renderer
        glyph_cache<memory_atlas> cache(std::move(source), 24.0f);
        text_renderer renderer(cache);

        // Measure
        auto extents = renderer.measure("Hello, World!");

        CHECK(extents.width > 0);
        CHECK(extents.height > 0);

        // Create output buffer
        int w = static_cast<int>(std::ceil(extents.width)) + 10;
        int h = static_cast<int>(std::ceil(extents.height)) + 10;
        std::vector<uint8_t> output(static_cast<std::size_t>(w * h), 0);

        // Create blit function that copies from atlas to output
        auto blit = [&](const memory_atlas& atlas, glyph_rect src,
                        float dx, float dy) {
            int dst_x = static_cast<int>(dx);
            int dst_y = static_cast<int>(dy);

            for (int y = 0; y < src.h; ++y) {
                for (int x = 0; x < src.w; ++x) {
                    int ox = dst_x + x;
                    int oy = dst_y + y;
                    if (ox >= 0 && ox < w && oy >= 0 && oy < h) {
                        uint8_t alpha = atlas.pixel(src.x + x, src.y + y);
                        // Simple max blend
                        auto idx = static_cast<std::size_t>(oy * w + ox);
                        output[idx] = std::max(output[idx], alpha);
                    }
                }
            }
        };

        // Render
        float width = renderer.draw("Hello, World!", 5, 5, blit);
        CHECK(width > 0);

        // Verify output has content
        int nonzero = 0;
        for (auto b : output) if (b > 0) nonzero++;
        CHECK(nonzero > 100);  // Should have many pixels
    }

    TEST_CASE("full pipeline with bitmap font") {
        auto data = test_data::load_fon_helva();
        auto font = font_factory::load_bitmap(data, 0);
        auto source = font_source::from_bitmap(font);

        glyph_cache<memory_atlas> cache(std::move(source), 12.0f);
        text_renderer renderer(cache);

        auto extents = renderer.measure("Test");
        CHECK(extents.width > 0);
        CHECK(extents.height > 0);

        int w = 100;
        int h = 20;
        std::vector<uint8_t> output(static_cast<std::size_t>(w * h), 0);

        auto blit = [&](const memory_atlas& atlas, glyph_rect src,
                        float dx, float dy) {
            int dst_x = static_cast<int>(dx);
            int dst_y = static_cast<int>(dy);

            for (int y = 0; y < src.h; ++y) {
                for (int x = 0; x < src.w; ++x) {
                    int ox = dst_x + x;
                    int oy = dst_y + y;
                    if (ox >= 0 && ox < w && oy >= 0 && oy < h) {
                        uint8_t alpha = atlas.pixel(src.x + x, src.y + y);
                        auto idx = static_cast<std::size_t>(oy * w + ox);
                        output[idx] = std::max(output[idx], alpha);
                    }
                }
            }
        };

        float width = renderer.draw("Test", 0, 0, blit);
        CHECK(width > 0);

        int nonzero = 0;
        for (auto b : output) if (b > 0) nonzero++;
        CHECK(nonzero > 0);
    }

    TEST_CASE("full pipeline with vector font") {
        auto data = test_data::load_bgi_litt();
        auto font = font_factory::load_vector(data, 0);
        auto source = font_source::from_vector(font);

        glyph_cache<memory_atlas> cache(std::move(source), 18.0f);  // 2x scale
        text_renderer renderer(cache);

        auto extents = renderer.measure("VECTOR");
        CHECK(extents.width > 0);
        CHECK(extents.height > 0);

        int w = 200;
        int h = 40;
        std::vector<uint8_t> output(static_cast<std::size_t>(w * h), 0);

        auto blit = [&](const memory_atlas& atlas, glyph_rect src,
                        float dx, float dy) {
            int dst_x = static_cast<int>(dx);
            int dst_y = static_cast<int>(dy);

            for (int y = 0; y < src.h; ++y) {
                for (int x = 0; x < src.w; ++x) {
                    int ox = dst_x + x;
                    int oy = dst_y + y;
                    if (ox >= 0 && ox < w && oy >= 0 && oy < h) {
                        uint8_t alpha = atlas.pixel(src.x + x, src.y + y);
                        auto idx = static_cast<std::size_t>(oy * w + ox);
                        output[idx] = std::max(output[idx], alpha);
                    }
                }
            }
        };

        float width = renderer.draw("VECTOR", 10, 10, blit);
        CHECK(width > 0);

        int nonzero = 0;
        for (auto b : output) if (b > 0) nonzero++;
        CHECK(nonzero > 0);
    }

    TEST_CASE("multiple fonts") {
        if (!test_data::file_exists(test_data::ttf_arial())) {
            WARN("Arial TTF not available");
            return;
        }

        auto ttf_data = test_data::load_ttf_arial();
        ttf_font ttf(ttf_data);
        stb_truetype_font stb(ttf_data);
        auto ttf_source = font_source::from_ttf(ttf);

        auto bitmap_data = test_data::load_fon_helva();
        auto bitmap = font_factory::load_bitmap(bitmap_data, 0);
        auto bitmap_source = font_source::from_bitmap(bitmap);

        // Each font gets its own cache
        glyph_cache<memory_atlas> ttf_cache(std::move(ttf_source), 24.0f);
        glyph_cache<memory_atlas> bitmap_cache(std::move(bitmap_source), 12.0f);

        text_renderer ttf_renderer(ttf_cache);
        text_renderer bitmap_renderer(bitmap_cache);

        auto ttf_ext = ttf_renderer.measure("Title");
        auto bitmap_ext = bitmap_renderer.measure("body text");

        CHECK(ttf_ext.height > bitmap_ext.height);
    }

    TEST_CASE("aligned text rendering") {
        auto data = test_data::load_fon_helva();
        auto font = font_factory::load_bitmap(data, 0);
        auto source = font_source::from_bitmap(font);

        glyph_cache<memory_atlas> cache(std::move(source), 12.0f);
        text_renderer renderer(cache);

        std::vector<float> left_x, center_x, right_x;

        auto collect_x = [&](std::vector<float>& positions) {
            return [&positions](const memory_atlas&, glyph_rect, float x, float) {
                positions.push_back(x);
            };
        };

        renderer.draw_aligned("Hi", 0, 0, 100, text_align::left, collect_x(left_x));
        renderer.draw_aligned("Hi", 0, 0, 100, text_align::center, collect_x(center_x));
        renderer.draw_aligned("Hi", 0, 0, 100, text_align::right, collect_x(right_x));

        REQUIRE(!left_x.empty());
        REQUIRE(!center_x.empty());
        REQUIRE(!right_x.empty());

        // Left should start near 0
        CHECK(left_x[0] < 10);

        // Center should start around 50 minus half text width
        CHECK(center_x[0] > left_x[0]);
        CHECK(center_x[0] < right_x[0]);

        // Right should start near 100 minus text width
        CHECK(right_x[0] > center_x[0]);
    }

    TEST_CASE("wrapped text rendering") {
        auto data = test_data::load_fon_helva();
        auto font = font_factory::load_bitmap(data, 0);
        auto source = font_source::from_bitmap(font);

        glyph_cache<memory_atlas> cache(std::move(source), 12.0f);
        text_renderer renderer(cache);

        std::vector<float> y_positions;

        auto blit = [&](const memory_atlas&, glyph_rect, float, float y) {
            y_positions.push_back(y);
        };

        text_box box{0, 0, 30, 100};  // Narrow box
        int lines = renderer.draw_wrapped("Hello World Test", box,
                                           text_align::left, blit);

        CHECK(lines >= 2);

        // Check multiple Y positions
        std::set<float> unique_y(y_positions.begin(), y_positions.end());
        CHECK(unique_y.size() >= 2);
    }

    TEST_CASE("empty text handling") {
        auto data = test_data::load_fon_helva();
        auto font = font_factory::load_bitmap(data, 0);
        auto source = font_source::from_bitmap(font);

        glyph_cache<memory_atlas> cache(std::move(source), 12.0f);
        text_renderer renderer(cache);

        int call_count = 0;
        auto blit = [&](const memory_atlas&, glyph_rect, float, float) {
            ++call_count;
        };

        float width = renderer.draw("", 0, 0, blit);
        CHECK(width == 0.0f);
        CHECK(call_count == 0);

        auto ext = renderer.measure("");
        CHECK(ext.width == 0.0f);
    }

    TEST_CASE("atlas reuse") {
        auto data = test_data::load_fon_helva();
        auto font = font_factory::load_bitmap(data, 0);
        auto source = font_source::from_bitmap(font);

        glyph_cache<memory_atlas> cache(std::move(source), 12.0f);
        text_renderer renderer(cache);

        // Render same text twice
        int calls1 = 0, calls2 = 0;

        auto counter1 = [&](const memory_atlas&, glyph_rect, float, float) {
            ++calls1;
        };
        auto counter2 = [&](const memory_atlas&, glyph_rect, float, float) {
            ++calls2;
        };

        renderer.draw("Test", 0, 0, counter1);
        int atlas_count_after_first = cache.atlas_count();

        renderer.draw("Test", 0, 0, counter2);
        int atlas_count_after_second = cache.atlas_count();

        // Should have same call count and no new atlases
        CHECK(calls1 == calls2);
        CHECK(atlas_count_after_first == atlas_count_after_second);
    }

    TEST_CASE("metrics consistency") {
        if (!test_data::file_exists(test_data::ttf_arial())) {
            WARN("Arial TTF not available");
            return;
        }

        auto data = test_data::load_ttf_arial();
        ttf_font ttf(data);
        stb_truetype_font stb(data);
        auto source = font_source::from_ttf(ttf);

        glyph_cache<memory_atlas> cache(std::move(source), 24.0f);
        text_renderer renderer(cache);

        // Metrics should be consistent
        auto metrics = renderer.metrics();
        float line_height = renderer.line_height();

        CHECK(metrics.line_height == line_height);
        CHECK(metrics.ascent > 0);
        CHECK(metrics.descent >= 0);
        CHECK(metrics.line_height >= metrics.ascent + metrics.descent);
    }
}
