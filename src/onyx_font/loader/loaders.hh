//
// Created by igor on 12/21/2025.
//
// Internal font loader declarations
//

#pragma once

#include <onyx_font/bitmap_font.hh>
#include <onyx_font/vector_font.hh>
#include <libexe/resources/parsers/font_parser.hpp>
#include <span>

namespace onyx_font::internal {

    /// Load bitmap font from Windows FNT data
    struct win_bitmap_fon_loader {
        static bitmap_font load(const libexe::font_data& fd);
    };

    /// Load vector font from Windows FNT data
    struct win_vector_fon_loader {
        static vector_font load(const libexe::font_data& fd);
    };

    /// Load vector font from BGI CHR data
    struct bgi_font_loader {
        static vector_font load(std::span<const uint8_t> data);
    };

}  // namespace onyx_font::internal
