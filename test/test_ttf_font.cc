//
// Created by igor on 21/12/2025.
//
// Unit tests for ttf_font
//

#include <doctest/doctest.h>
#include <onyx_font/ttf_font.hh>
#include <onyx_font/font_factory.hh>
#include "test_data.hh"

using namespace onyx_font;
using namespace onyx_font::test;

TEST_SUITE("ttf_font") {

    TEST_CASE("load Arial TTF font") {
        REQUIRE(test_data::file_exists(test_data::ttf_arial()));

        auto data = test_data::load_ttf_arial();
        CHECK(data.size() == 65692);

        ttf_font font(data);
        CHECK(font.is_valid());
        CHECK(ttf_font::get_font_count(data) == 1);
    }

    TEST_CASE("get font metrics at 24px") {
        REQUIRE(test_data::file_exists(test_data::ttf_arial()));

        auto data = test_data::load_ttf_arial();
        ttf_font font(data);
        REQUIRE(font.is_valid());

        auto metrics = font.get_metrics(24.0f);

        // FreeType metrics from font units (ascender/descender/height)
        CHECK(metrics.ascent == doctest::Approx(21.7266f).epsilon(0.01));
        CHECK(metrics.descent == doctest::Approx(-5.0859f).epsilon(0.01));
        CHECK(metrics.line_gap == doctest::Approx(0.0f).epsilon(0.5));
    }

    TEST_CASE("get glyph metrics at 24px") {
        REQUIRE(test_data::file_exists(test_data::ttf_arial()));

        auto data = test_data::load_ttf_arial();
        ttf_font font(data);
        REQUIRE(font.is_valid());

        // Test glyph 'A' - metrics from FreeType (26.6 fixed point)
        auto glyph_A = font.get_glyph_metrics('A', 24.0f);
        REQUIRE(glyph_A.has_value());
        CHECK(glyph_A->advance_x == doctest::Approx(16.0f).epsilon(0.1));
        CHECK(glyph_A->width == doctest::Approx(18.0f).epsilon(0.1));
        CHECK(glyph_A->height == doctest::Approx(17.0f).epsilon(0.1));

        // Test glyph 'M'
        auto glyph_M = font.get_glyph_metrics('M', 24.0f);
        REQUIRE(glyph_M.has_value());
        CHECK(glyph_M->advance_x == doctest::Approx(20.0f).epsilon(0.1));

        // Test glyph 'g' (has descender)
        auto glyph_g = font.get_glyph_metrics('g', 24.0f);
        REQUIRE(glyph_g.has_value());
        CHECK(glyph_g->advance_x == doctest::Approx(13.0f).epsilon(0.1));
    }

    TEST_CASE("get glyph shape (outline)") {
        REQUIRE(test_data::file_exists(test_data::ttf_arial()));

        auto data = test_data::load_ttf_arial();
        ttf_font font(data);
        REQUIRE(font.is_valid());

        // Get shape for 'A'
        auto shape = font.get_glyph_shape('A', 24.0f);
        REQUIRE(shape.has_value());
        CHECK(!shape->vertices.empty());

        // Check that vertices have valid types
        for (const auto& v : shape->vertices) {
            CHECK((v.type == ttf_vertex_type::MOVE_TO ||
                   v.type == ttf_vertex_type::LINE_TO ||
                   v.type == ttf_vertex_type::CURVE_TO ||
                   v.type == ttf_vertex_type::CUBIC_TO));
        }
    }

    TEST_CASE("has_glyph returns correct value") {
        REQUIRE(test_data::file_exists(test_data::ttf_arial()));

        auto data = test_data::load_ttf_arial();
        ttf_font font(data);
        REQUIRE(font.is_valid());

        // ASCII characters should exist
        CHECK(font.has_glyph('A'));
        CHECK(font.has_glyph('z'));
        CHECK(font.has_glyph('0'));
    }

    TEST_CASE("get_kerning returns value") {
        REQUIRE(test_data::file_exists(test_data::ttf_arial()));

        auto data = test_data::load_ttf_arial();
        ttf_font font(data);
        REQUIRE(font.is_valid());

        // AV is a common kerning pair
        [[maybe_unused]] float kern = font.get_kerning('A', 'V', 24.0f);
    }

    TEST_CASE("get_font_count for single font") {
        REQUIRE(test_data::file_exists(test_data::ttf_arial()));

        auto data = test_data::load_ttf_arial();
        int count = ttf_font::get_font_count(data);

        CHECK(count == 1);
    }

    TEST_CASE("invalid font data") {
        std::vector<uint8_t> garbage = {0x00, 0x01, 0x02, 0x03};
        ttf_font font(garbage);

        CHECK_FALSE(font.is_valid());
    }

    TEST_CASE("empty font data") {
        std::vector<uint8_t> empty;
        ttf_font font(empty);

        CHECK_FALSE(font.is_valid());
    }
}