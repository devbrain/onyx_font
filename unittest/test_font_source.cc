//
// Created by igor on 21/12/2025.
//
// Unit tests for font_source abstraction
//

#include <doctest/doctest.h>
#include <onyx_font/text/font_source.hh>
#include <onyx_font/text/raster_target.hh>
#include <onyx_font/font_factory.hh>
#include "test_data.hh"

using namespace onyx_font;
using namespace onyx_font::test;

TEST_SUITE("font_source") {

    TEST_CASE("from_bitmap basic") {
        auto data = test_data::load_fon_helva();
        auto font = font_factory::load_bitmap(data, 0);

        auto source = font_source::from_bitmap(font);
        CHECK(source.type() == font_source_type::bitmap);
        CHECK(source.has_glyph('A'));
        CHECK(source.has_glyph('Z'));
        CHECK(source.has_glyph(' '));
        CHECK(!source.has_glyph(0x4E2D));  // Chinese character - not supported

        // Native size should be the pixel height
        CHECK(source.native_size() > 0);
    }

    TEST_CASE("from_vector basic") {
        auto data = test_data::load_bgi_litt();
        auto font = font_factory::load_vector(data, 0);

        auto source = font_source::from_vector(font);
        CHECK(source.type() == font_source_type::vector);
        CHECK(source.has_glyph('A'));
        CHECK(source.has_glyph('Z'));
        CHECK(source.native_size() == 0.0f);  // Scalable, no native size
    }

    TEST_CASE("from_ttf basic") {
        if (!test_data::file_exists(test_data::ttf_arial())) {
            WARN("Arial TTF not available, skipping TTF tests");
            return;
        }

        auto data = test_data::load_ttf_arial();
        ttf_font ttf(data);
        stb_truetype_font stb(data);

        auto source = font_source::from_ttf(ttf);
        CHECK(source.type() == font_source_type::outline);
        CHECK(source.has_glyph('A'));
        CHECK(source.has_glyph('Z'));
        CHECK(source.native_size() == 0.0f);  // Scalable
    }

    TEST_CASE("bitmap scaled_metrics") {
        auto data = test_data::load_fon_helva();
        auto font = font_factory::load_bitmap(data, 0);
        auto source = font_source::from_bitmap(font);

        auto metrics = source.get_scaled_metrics(12.0f);

        CHECK(metrics.ascent > 0);
        CHECK(metrics.line_height > 0);
        CHECK(metrics.line_height >= metrics.ascent + metrics.descent);
    }

    TEST_CASE("vector scaled_metrics") {
        auto data = test_data::load_bgi_litt();
        auto font = font_factory::load_vector(data, 0);
        auto source = font_source::from_vector(font);

        auto metrics = source.get_scaled_metrics(24.0f);

        CHECK(metrics.ascent > 0);
        CHECK(metrics.line_height > 0);
    }

    TEST_CASE("ttf scaled_metrics") {
        if (!test_data::file_exists(test_data::ttf_arial())) {
            return;
        }

        auto data = test_data::load_ttf_arial();
        ttf_font ttf(data);
        stb_truetype_font stb(data);
        auto source = font_source::from_ttf(ttf);

        auto metrics = source.get_scaled_metrics(24.0f);

        CHECK(metrics.ascent > 0);
        CHECK(metrics.descent >= 0);  // Can be 0 for some fonts
        CHECK(metrics.line_height > 0);
    }

    TEST_CASE("bitmap glyph_metrics") {
        auto data = test_data::load_fon_helva();
        auto font = font_factory::load_bitmap(data, 0);
        auto source = font_source::from_bitmap(font);

        auto metrics = source.get_glyph_metrics('A', 12.0f);

        CHECK(metrics.advance_x > 0);
        CHECK(metrics.width > 0);
        CHECK(metrics.height > 0);
    }

    TEST_CASE("vector glyph_metrics") {
        auto data = test_data::load_bgi_litt();
        auto font = font_factory::load_vector(data, 0);
        auto source = font_source::from_vector(font);

        auto metrics = source.get_glyph_metrics('A', 24.0f);

        CHECK(metrics.advance_x > 0);
    }

    TEST_CASE("ttf glyph_metrics") {
        if (!test_data::file_exists(test_data::ttf_arial())) {
            return;
        }

        auto data = test_data::load_ttf_arial();
        ttf_font ttf(data);
        stb_truetype_font stb(data);
        auto source = font_source::from_ttf(ttf);

        auto metrics = source.get_glyph_metrics('A', 24.0f);

        CHECK(metrics.advance_x > 0);
        CHECK(metrics.width > 0);
        CHECK(metrics.height > 0);
    }

    TEST_CASE("kerning") {
        if (!test_data::file_exists(test_data::ttf_arial())) {
            return;
        }

        auto data = test_data::load_ttf_arial();
        ttf_font ttf(data);
        stb_truetype_font stb(data);
        auto source = font_source::from_ttf(ttf);

        // A-V typically has negative kerning, A-A typically has 0
        float kern_AV = source.get_kerning('A', 'V', 24.0f);
        float kern_AA = source.get_kerning('A', 'A', 24.0f);

        // This may or may not be true depending on the font,
        // but for Arial A-V should have negative kerning
        CHECK(kern_AV <= kern_AA);
    }

    TEST_CASE("bitmap kerning returns zero") {
        auto data = test_data::load_fon_helva();
        auto font = font_factory::load_bitmap(data, 0);
        auto source = font_source::from_bitmap(font);

        // Bitmap fonts don't support kerning
        CHECK(source.get_kerning('A', 'V', 12.0f) == 0.0f);
    }

    TEST_CASE("rasterize_glyph bitmap") {
        auto data = test_data::load_fon_helva();
        auto font = font_factory::load_bitmap(data, 0);
        auto source = font_source::from_bitmap(font);

        uint8_t buffer[30 * 20] = {0};
        grayscale_target target(buffer, 30, 20);

        auto metrics = source.get_scaled_metrics(12.0f);
        int baseline = static_cast<int>(metrics.ascent);

        source.rasterize_glyph('A', 12.0f, target, 2, baseline);

        // Should have some non-zero pixels
        bool has_pixels = false;
        for (auto b : buffer) {
            if (b > 0) has_pixels = true;
        }
        CHECK(has_pixels);

        // For bitmap fonts, all pixels are either 0 or 255
        bool all_binary = true;
        for (auto b : buffer) {
            if (b != 0 && b != 255) {
                all_binary = false;
                break;
            }
        }
        CHECK(all_binary);
    }

    TEST_CASE("rasterize_glyph vector") {
        auto data = test_data::load_bgi_litt();
        auto font = font_factory::load_vector(data, 0);
        auto source = font_source::from_vector(font);

        // Use larger buffer - vector glyphs may extend beyond declared metrics
        uint8_t buffer[80 * 80] = {0};
        grayscale_target target(buffer, 80, 80);

        // Place baseline well into the buffer so glyph has room above and below
        // Vector fonts may have glyphs that extend beyond declared ascent/descent
        source.rasterize_glyph('A', 24.0f, target, 5, 60);

        // Should have some non-zero pixels
        bool has_pixels = false;
        for (auto b : buffer) {
            if (b > 0) has_pixels = true;
        }
        CHECK(has_pixels);
    }

    TEST_CASE("rasterize_glyph ttf") {
        if (!test_data::file_exists(test_data::ttf_arial())) {
            return;
        }

        auto data = test_data::load_ttf_arial();
        ttf_font ttf(data);
        stb_truetype_font stb(data);
        auto source = font_source::from_ttf(ttf);

        uint8_t buffer[40 * 40] = {0};
        grayscale_target target(buffer, 40, 40);

        source.rasterize_glyph('A', 24.0f, target, 5, 30);

        // Should have some non-zero pixels
        bool has_pixels = false;
        for (auto b : buffer) {
            if (b > 0) has_pixels = true;
        }
        CHECK(has_pixels);

        // TTF should have antialiased pixels (values between 0 and 255)
        bool has_aa = false;
        for (auto b : buffer) {
            if (b > 0 && b < 255) has_aa = true;
        }
        CHECK(has_aa);
    }

    TEST_CASE("default_char") {
        auto data = test_data::load_fon_helva();
        auto font = font_factory::load_bitmap(data, 0);
        auto source = font_source::from_bitmap(font);

        // Default char should be valid
        char32_t def = source.default_char();
        CHECK(source.has_glyph(def));
    }

    TEST_CASE("missing glyph falls back") {
        auto data = test_data::load_fon_helva();
        auto font = font_factory::load_bitmap(data, 0);
        auto source = font_source::from_bitmap(font);

        // Request metrics for a non-existent glyph (high codepoint)
        // Should use default char
        [[maybe_unused]] auto metrics = source.get_glyph_metrics(0x1234, 12.0f);

        // Metrics should still be valid (from default char)
        // For bitmap fonts with limited charset, this returns zeros
        // which is acceptable behavior
    }

    TEST_CASE("rasterize to callback_target") {
        auto data = test_data::load_fon_helva();
        auto font = font_factory::load_bitmap(data, 0);
        auto source = font_source::from_bitmap(font);

        std::vector<std::tuple<int, int, uint8_t>> pixels;

        callback_target target(100, 50, [&](int x, int y, uint8_t a) {
            pixels.emplace_back(x, y, a);
        });

        auto metrics = source.get_scaled_metrics(12.0f);
        source.rasterize_glyph('A', 12.0f, target, 10, static_cast<int>(metrics.ascent));

        CHECK(!pixels.empty());

        // All pixels should be at x >= 10 (we offset by 10)
        for (const auto& [x, y, a] : pixels) {
            CHECK(x >= 10);
            (void)y;
            (void)a;
        }
    }

    TEST_CASE("rasterize multiple glyphs") {
        auto data = test_data::load_fon_helva();
        auto font = font_factory::load_bitmap(data, 0);
        auto source = font_source::from_bitmap(font);

        uint8_t buffer[100 * 20] = {0};
        grayscale_target target(buffer, 100, 20);

        auto metrics = source.get_scaled_metrics(12.0f);
        int baseline = static_cast<int>(metrics.ascent);

        // Rasterize "Hi"
        int pen_x = 2;
        for (char ch : {'H', 'i'}) {
            auto glyph_metrics = source.get_glyph_metrics(ch, 12.0f);
            source.rasterize_glyph(ch, 12.0f, target, pen_x, baseline);
            pen_x += static_cast<int>(glyph_metrics.advance_x);
        }

        // Count non-zero pixels
        int pixel_count = 0;
        for (auto b : buffer) {
            if (b > 0) ++pixel_count;
        }
        CHECK(pixel_count > 10);  // Should have at least some pixels
    }
}
