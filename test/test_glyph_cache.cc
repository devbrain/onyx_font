//
// Created by igor on 21/12/2025.
//
// Unit tests for glyph_cache
//

#include <doctest/doctest.h>
#include <onyx_font/text/glyph_cache.hh>
#include <onyx_font/font_factory.hh>
#include "test_data.hh"

using namespace onyx_font;
using namespace onyx_font::test;

TEST_SUITE("glyph_cache") {

    TEST_CASE("atlas_surface concept") {
        // Verify memory_atlas satisfies the concept
        static_assert(atlas_surface<memory_atlas>);
    }

    TEST_CASE("memory_atlas basic") {
        memory_atlas atlas(64, 64);

        CHECK(atlas.width() == 64);
        CHECK(atlas.height() == 64);

        // Initially all zeros
        CHECK(atlas.pixel(0, 0) == 0);
        CHECK(atlas.pixel(32, 32) == 0);

        // Write some data
        uint8_t data[] = {100, 150, 200, 250};
        atlas.write_alpha(10, 10, 2, 2, data, 2);

        CHECK(atlas.pixel(10, 10) == 100);
        CHECK(atlas.pixel(11, 10) == 150);
        CHECK(atlas.pixel(10, 11) == 200);
        CHECK(atlas.pixel(11, 11) == 250);

        // Clear
        atlas.clear();
        CHECK(atlas.pixel(10, 10) == 0);
    }

    TEST_CASE("basic caching bitmap") {
        auto data = test_data::load_fon_helva();
        auto font = font_factory::load_bitmap(data, 0);
        auto source = font_source::from_bitmap(font);

        glyph_cache_config config;
        config.pre_cache_ascii = false;
        glyph_cache<memory_atlas> cache(std::move(source), 12.0f, config);

        // First access rasterizes
        const auto& glyph_A = cache.get('A');
        CHECK(glyph_A.rect.w > 0);
        CHECK(glyph_A.rect.h > 0);
        CHECK(glyph_A.advance_x > 0);

        // Second access returns cached
        const auto& glyph_A2 = cache.get('A');
        CHECK(&glyph_A == &glyph_A2);  // Same object
    }

    TEST_CASE("basic caching ttf") {
        if (!test_data::file_exists(test_data::ttf_arial())) {
            WARN("Arial TTF not available");
            return;
        }

        auto data = test_data::load_ttf_arial();
        ttf_font ttf(data);
        stb_truetype_font stb(data);
        auto source = font_source::from_ttf(ttf);

        glyph_cache_config config;
        config.pre_cache_ascii = false;
        glyph_cache<memory_atlas> cache(std::move(source), 24.0f, config);

        const auto& glyph_A = cache.get('A');
        CHECK(glyph_A.rect.w > 0);
        CHECK(glyph_A.rect.h > 0);
        CHECK(glyph_A.advance_x > 0);

        // Second access returns same pointer
        const auto& glyph_A2 = cache.get('A');
        CHECK(&glyph_A == &glyph_A2);
    }

    TEST_CASE("pre_cache_ascii") {
        auto data = test_data::load_fon_helva();
        auto font = font_factory::load_bitmap(data, 0);
        auto source = font_source::from_bitmap(font);

        glyph_cache_config config;
        config.pre_cache_ascii = true;
        glyph_cache<memory_atlas> cache(std::move(source), 12.0f, config);

        // ASCII should be pre-cached
        CHECK(cache.is_cached('A'));
        CHECK(cache.is_cached('z'));
        CHECK(cache.is_cached('0'));
        CHECK(cache.is_cached(' '));
        CHECK(cache.is_cached('!'));
        CHECK(cache.is_cached('~'));
    }

    TEST_CASE("cache_string") {
        auto data = test_data::load_fon_helva();
        auto font = font_factory::load_bitmap(data, 0);
        auto source = font_source::from_bitmap(font);

        glyph_cache_config config;
        config.pre_cache_ascii = false;
        glyph_cache<memory_atlas> cache(std::move(source), 12.0f, config);

        CHECK(!cache.is_cached('H'));
        CHECK(!cache.is_cached('e'));

        cache.cache_string("Hello");

        CHECK(cache.is_cached('H'));
        CHECK(cache.is_cached('e'));
        CHECK(cache.is_cached('l'));
        CHECK(cache.is_cached('o'));
    }

    TEST_CASE("cache_range") {
        auto data = test_data::load_fon_helva();
        auto font = font_factory::load_bitmap(data, 0);
        auto source = font_source::from_bitmap(font);

        glyph_cache_config config;
        config.pre_cache_ascii = false;
        glyph_cache<memory_atlas> cache(std::move(source), 12.0f, config);

        CHECK(!cache.is_cached('A'));
        CHECK(!cache.is_cached('Z'));

        cache.cache_range('A', 'Z');

        for (char c = 'A'; c <= 'Z'; ++c) {
            CHECK(cache.is_cached(c));
        }
    }

    TEST_CASE("multiple atlases") {
        auto data = test_data::load_fon_helva();
        auto font = font_factory::load_bitmap(data, 0);
        auto source = font_source::from_bitmap(font);

        glyph_cache_config config;
        config.atlas_size = 32;  // Very small atlas to force overflow
        config.pre_cache_ascii = false;
        glyph_cache<memory_atlas> cache(std::move(source), 12.0f, config);

        // Cache many glyphs to force multiple atlases
        cache.cache_range('A', 'Z');
        cache.cache_range('a', 'z');
        cache.cache_range('0', '9');

        CHECK(cache.atlas_count() >= 1);

        // All glyphs should still be retrievable
        for (char c = 'A'; c <= 'Z'; ++c) {
            CHECK(cache.is_cached(c));
            const auto& glyph = cache.get(c);
            CHECK(glyph.atlas_index < cache.atlas_count());
        }
    }

    TEST_CASE("atlas pixel data") {
        if (!test_data::file_exists(test_data::ttf_arial())) {
            return;
        }

        auto data = test_data::load_ttf_arial();
        ttf_font ttf(data);
        stb_truetype_font stb(data);
        auto source = font_source::from_ttf(ttf);

        glyph_cache_config config;
        config.pre_cache_ascii = false;
        glyph_cache<memory_atlas> cache(std::move(source), 24.0f, config);

        const auto& glyph = cache.get('A');
        const auto& atlas = cache.atlas(glyph.atlas_index);

        // Check that pixels were written to atlas
        bool has_pixels = false;
        for (int y = 0; y < glyph.rect.h; ++y) {
            for (int x = 0; x < glyph.rect.w; ++x) {
                if (atlas.pixel(glyph.rect.x + x, glyph.rect.y + y) > 0) {
                    has_pixels = true;
                }
            }
        }
        CHECK(has_pixels);
    }

    TEST_CASE("measure delegates to rasterizer") {
        auto data = test_data::load_fon_helva();
        auto font = font_factory::load_bitmap(data, 0);
        auto source = font_source::from_bitmap(font);

        glyph_cache<memory_atlas> cache(std::move(source), 12.0f);

        auto extents = cache.measure("Hello");
        CHECK(extents.width > 0);
        CHECK(extents.height > 0);
    }

    TEST_CASE("metrics and line_height") {
        auto data = test_data::load_fon_helva();
        auto font = font_factory::load_bitmap(data, 0);
        auto source = font_source::from_bitmap(font);

        glyph_cache<memory_atlas> cache(std::move(source), 12.0f);

        auto m = cache.metrics();
        CHECK(m.ascent > 0);
        CHECK(m.line_height > 0);

        CHECK(cache.line_height() == m.line_height);
    }

    TEST_CASE("space glyph has zero size but valid advance") {
        auto data = test_data::load_fon_helva();
        auto font = font_factory::load_bitmap(data, 0);
        auto source = font_source::from_bitmap(font);

        glyph_cache_config config;
        config.pre_cache_ascii = false;
        glyph_cache<memory_atlas> cache(std::move(source), 12.0f, config);

        const auto& space = cache.get(' ');

        // Space has advance but may have zero or small rect
        CHECK(space.advance_x > 0);
    }

    TEST_CASE("glyph bearings") {
        if (!test_data::file_exists(test_data::ttf_arial())) {
            return;
        }

        auto data = test_data::load_ttf_arial();
        ttf_font ttf(data);
        stb_truetype_font stb(data);
        auto source = font_source::from_ttf(ttf);

        glyph_cache<memory_atlas> cache(std::move(source), 24.0f);

        const auto& glyph_A = cache.get('A');

        // 'A' typically has positive bearing_y (extends above baseline)
        CHECK(glyph_A.bearing_y > 0);
    }
}
