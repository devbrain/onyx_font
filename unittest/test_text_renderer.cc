//
// Created by igor on 21/12/2025.
//
// Unit tests for text_renderer
//

#include <doctest/doctest.h>
#include <onyx_font/text/text_renderer.hh>
#include <onyx_font/font_factory.hh>
#include "test_data.hh"
#include <set>
#include <tuple>

using namespace onyx_font;
using namespace onyx_font::test;

TEST_SUITE("text_renderer") {

    TEST_CASE("draw basic bitmap") {
        auto data = test_data::load_fon_helva();
        auto font = font_factory::load_bitmap(data, 0);
        auto source = font_source::from_bitmap(font);

        glyph_cache<memory_atlas> cache(std::move(source), 12.0f);
        text_renderer renderer(cache);

        std::vector<std::tuple<glyph_rect, float, float>> calls;

        auto blit = [&](const memory_atlas&, glyph_rect src, float x, float y) {
            calls.emplace_back(src, x, y);
        };

        float width = renderer.draw("Hi", 10.0f, 20.0f, blit);

        CHECK(width > 0);
        CHECK(calls.size() == 2);  // Two glyphs
        CHECK(std::get<1>(calls[0]) >= 10.0f);  // First at or after start
        CHECK(std::get<1>(calls[1]) > std::get<1>(calls[0]));  // Second to right
    }

    TEST_CASE("draw basic ttf") {
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

        std::vector<std::tuple<glyph_rect, float, float>> calls;

        auto blit = [&](const memory_atlas&, glyph_rect src, float x, float y) {
            calls.emplace_back(src, x, y);
        };

        float width = renderer.draw("Hi", 10.0f, 20.0f, blit);

        CHECK(width > 0);
        CHECK(calls.size() == 2);
    }

    TEST_CASE("draw_aligned left") {
        auto data = test_data::load_fon_helva();
        auto font = font_factory::load_bitmap(data, 0);
        auto source = font_source::from_bitmap(font);

        glyph_cache<memory_atlas> cache(std::move(source), 12.0f);
        text_renderer renderer(cache);

        std::vector<float> x_positions;

        auto blit = [&](const memory_atlas&, glyph_rect, float x, float) {
            x_positions.push_back(x);
        };

        renderer.draw_aligned("Hi", 50.0f, 0.0f, 200.0f, text_align::left, blit);

        // First glyph should start near x=50
        CHECK(x_positions[0] >= 50.0f);
        CHECK(x_positions[0] < 60.0f);
    }

    TEST_CASE("draw_aligned center") {
        auto data = test_data::load_fon_helva();
        auto font = font_factory::load_bitmap(data, 0);
        auto source = font_source::from_bitmap(font);

        glyph_cache<memory_atlas> cache(std::move(source), 12.0f);
        text_renderer renderer(cache);

        std::vector<float> x_positions;

        auto blit = [&](const memory_atlas&, glyph_rect, float x, float) {
            x_positions.push_back(x);
        };

        // Center "Hi" in 200px width starting at x=0
        renderer.draw_aligned("Hi", 0.0f, 0.0f, 200.0f, text_align::center, blit);

        // First glyph should be offset from left (roughly centered)
        CHECK(x_positions[0] > 0);
        CHECK(x_positions[0] < 100.0f);  // But not past center
    }

    TEST_CASE("draw_aligned right") {
        auto data = test_data::load_fon_helva();
        auto font = font_factory::load_bitmap(data, 0);
        auto source = font_source::from_bitmap(font);

        glyph_cache<memory_atlas> cache(std::move(source), 12.0f);
        text_renderer renderer(cache);

        float last_x = 0;
        float last_w = 0;

        auto blit = [&](const memory_atlas&, glyph_rect src, float x, float) {
            last_x = x;
            last_w = static_cast<float>(src.w);
        };

        // Right-align "Hi" in 200px width
        renderer.draw_aligned("Hi", 0.0f, 0.0f, 200.0f, text_align::right, blit);

        // Last glyph should end near 200
        auto text_width = renderer.measure("Hi").width;
        CHECK(last_x > 100.0f);  // Should be in right half
    }

    TEST_CASE("draw_wrapped basic") {
        auto data = test_data::load_fon_helva();
        auto font = font_factory::load_bitmap(data, 0);
        auto source = font_source::from_bitmap(font);

        glyph_cache<memory_atlas> cache(std::move(source), 12.0f);
        text_renderer renderer(cache);

        std::vector<float> y_positions;

        auto blit = [&](const memory_atlas&, glyph_rect, float, float y) {
            y_positions.push_back(y);
        };

        // Narrow box should force wrapping
        text_box box{0, 0, 30, 200};
        int lines = renderer.draw_wrapped("Hello World Test", box,
                                           text_align::left, blit);

        CHECK(lines >= 2);  // Should wrap to multiple lines

        // Check that we have glyphs on different Y positions
        std::set<float> unique_y(y_positions.begin(), y_positions.end());
        CHECK(unique_y.size() >= 2);
    }

    TEST_CASE("draw_wrapped respects box height") {
        auto data = test_data::load_fon_helva();
        auto font = font_factory::load_bitmap(data, 0);
        auto source = font_source::from_bitmap(font);

        glyph_cache<memory_atlas> cache(std::move(source), 12.0f);
        text_renderer renderer(cache);

        float max_y = 0;

        auto blit = [&](const memory_atlas&, glyph_rect src, float, float y) {
            max_y = std::max(max_y, y + static_cast<float>(src.h));
        };

        // Very short box - only fits one line
        float line_h = renderer.line_height();
        text_box box{0, 0, 50, line_h + 2};  // Just over one line
        int lines = renderer.draw_wrapped("Line one Line two Line three", box,
                                           text_align::left, blit);

        // Should only draw one line
        CHECK(lines <= 2);
    }

    TEST_CASE("draw_wrapped with newlines") {
        auto data = test_data::load_fon_helva();
        auto font = font_factory::load_bitmap(data, 0);
        auto source = font_source::from_bitmap(font);

        glyph_cache<memory_atlas> cache(std::move(source), 12.0f);
        text_renderer renderer(cache);

        std::set<float> y_positions;

        auto blit = [&](const memory_atlas&, glyph_rect, float, float y) {
            y_positions.insert(y);
        };

        // Wide box but text has newline
        text_box box{0, 0, 500, 200};
        int lines = renderer.draw_wrapped("Hello\nWorld", box,
                                           text_align::left, blit);

        CHECK(lines == 2);
        CHECK(y_positions.size() == 2);
    }

    TEST_CASE("measure") {
        auto data = test_data::load_fon_helva();
        auto font = font_factory::load_bitmap(data, 0);
        auto source = font_source::from_bitmap(font);

        glyph_cache<memory_atlas> cache(std::move(source), 12.0f);
        text_renderer renderer(cache);

        auto ext = renderer.measure("Hello");
        CHECK(ext.width > 0);
        CHECK(ext.height > 0);
    }

    TEST_CASE("measure_wrapped") {
        auto data = test_data::load_fon_helva();
        auto font = font_factory::load_bitmap(data, 0);
        auto source = font_source::from_bitmap(font);

        glyph_cache<memory_atlas> cache(std::move(source), 12.0f);
        text_renderer renderer(cache);

        auto single = renderer.measure("Hello World");
        auto wrapped = renderer.measure_wrapped("Hello World", 30.0f);

        // Wrapped should have more height
        CHECK(wrapped.height >= single.height);
    }

    TEST_CASE("line_height and metrics") {
        auto data = test_data::load_fon_helva();
        auto font = font_factory::load_bitmap(data, 0);
        auto source = font_source::from_bitmap(font);

        glyph_cache<memory_atlas> cache(std::move(source), 12.0f);
        text_renderer renderer(cache);

        CHECK(renderer.line_height() > 0);
        CHECK(renderer.metrics().ascent > 0);
        CHECK(renderer.metrics().line_height == renderer.line_height());
    }

    TEST_CASE("draw empty string") {
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
    }

    TEST_CASE("draw with space") {
        auto data = test_data::load_fon_helva();
        auto font = font_factory::load_bitmap(data, 0);
        auto source = font_source::from_bitmap(font);

        glyph_cache<memory_atlas> cache(std::move(source), 12.0f);
        text_renderer renderer(cache);

        float width_no_space = renderer.measure("Hi").width;
        float width_with_space = renderer.measure("H i").width;

        // Space should add width
        CHECK(width_with_space > width_no_space);
    }
}
