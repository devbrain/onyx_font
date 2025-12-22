//
// Diagnostic tool to dump font metadata for ground truth
//

#include <onyx_font/font_factory.hh>
#include <onyx_font/bitmap_font.hh>
#include <onyx_font/vector_font.hh>
#include <onyx_font/ttf_font.hh>
#include <../include/onyx_font/utils/stb_truetype_font.hh>
#include <iostream>
#include <iomanip>
#include <filesystem>

#include "test_data.hh"

using namespace onyx_font;
using namespace onyx_font::test;

void print_separator(const std::string& title) {
    std::cout << "\n" << std::string(70, '=') << "\n";
    std::cout << title << "\n";
    std::cout << std::string(70, '=') << "\n";
}

void print_subsection(const std::string& title) {
    std::cout << "\n--- " << title << " ---\n";
}

void dump_container_info(const std::string& name, const std::filesystem::path& path) {
    print_separator(name + " (" + path.filename().string() + ")");

    if (!test_data::file_exists(path)) {
        std::cout << "  FILE NOT FOUND: " << path << "\n";
        return;
    }

    auto data = test_data::load_file(path);
    std::cout << "File size: " << data.size() << " bytes\n";

    auto info = font_factory::analyze(data);
    std::cout << "Format: " << font_factory::format_name(info.format)
              << " (" << static_cast<int>(info.format) << ")\n";
    std::cout << "Font count: " << info.fonts.size() << "\n";

    for (size_t i = 0; i < info.fonts.size(); ++i) {
        const auto& f = info.fonts[i];
        std::cout << "\n  Font[" << i << "]:\n";
        std::cout << "    name: \"" << f.name << "\"\n";
        std::cout << "    type: " << font_factory::type_name(f.type)
                  << " (" << static_cast<int>(f.type) << ")\n";
        std::cout << "    pixel_height: " << f.pixel_height << "\n";
        std::cout << "    point_size: " << f.point_size << "\n";
        std::cout << "    weight: " << f.weight << "\n";
        std::cout << "    italic: " << (f.italic ? "true" : "false") << "\n";
    }
}

void dump_bitmap_fonts(const std::string& name, const std::filesystem::path& path) {
    if (!test_data::file_exists(path)) return;

    print_subsection("Bitmap Font Details");

    auto data = test_data::load_file(path);

    try {
        auto fonts = font_factory::load_all_bitmaps(data);
        std::cout << "Loaded " << fonts.size() << " bitmap font(s)\n";

        for (size_t i = 0; i < fonts.size(); ++i) {
            const auto& font = fonts[i];
            auto metrics = font.get_metrics();

            std::cout << "\n  BitmapFont[" << i << "]:\n";
            std::cout << "    name: \"" << font.get_name() << "\"\n";
            std::cout << "    first_char: " << static_cast<int>(font.get_first_char()) << "\n";
            std::cout << "    last_char: " << static_cast<int>(font.get_last_char()) << "\n";
            std::cout << "    default_char: " << static_cast<int>(font.get_default_char()) << "\n";
            std::cout << "    break_char: " << static_cast<int>(font.get_break_char()) << "\n";
            std::cout << "    metrics.pixel_height: " << metrics.pixel_height << "\n";
            std::cout << "    metrics.ascent: " << metrics.ascent << "\n";
            std::cout << "    metrics.internal_leading: " << metrics.internal_leading << "\n";
            std::cout << "    metrics.external_leading: " << metrics.external_leading << "\n";
            std::cout << "    metrics.max_width: " << metrics.max_width << "\n";
            std::cout << "    metrics.avg_width: " << metrics.avg_width << "\n";

            // Check a few specific glyphs
            std::cout << "    Glyph samples:\n";
            for (char ch : {'A', 'M', 'i', ' '}) {
                auto glyph = font.get_glyph(static_cast<uint8_t>(ch));
                if (glyph.width() > 0) {
                    std::cout << "      '" << ch << "': width=" << glyph.width()
                              << ", height=" << glyph.height() << "\n";
                }
            }
        }
    } catch (const std::exception& e) {
        std::cout << "  Error loading bitmap fonts: " << e.what() << "\n";
    }
}

void dump_vector_fonts(const std::string& name, const std::filesystem::path& path) {
    if (!test_data::file_exists(path)) return;

    print_subsection("Vector Font Details");

    auto data = test_data::load_file(path);

    try {
        auto fonts = font_factory::load_all_vectors(data);
        std::cout << "Loaded " << fonts.size() << " vector font(s)\n";

        for (size_t i = 0; i < fonts.size(); ++i) {
            const auto& font = fonts[i];
            const auto& metrics = font.get_metrics();

            std::cout << "\n  VectorFont[" << i << "]:\n";
            std::cout << "    name: \"" << font.get_name() << "\"\n";
            std::cout << "    first_char: " << static_cast<int>(font.get_first_char()) << "\n";
            std::cout << "    last_char: " << static_cast<int>(font.get_last_char()) << "\n";
            std::cout << "    default_char: " << static_cast<int>(font.get_default_char()) << "\n";
            std::cout << "    metrics.pixel_height: " << metrics.pixel_height << "\n";
            std::cout << "    metrics.ascent: " << metrics.ascent << "\n";
            std::cout << "    metrics.descent: " << metrics.descent << "\n";
            std::cout << "    metrics.baseline: " << metrics.baseline << "\n";
            std::cout << "    metrics.avg_width: " << metrics.avg_width << "\n";
            std::cout << "    metrics.max_width: " << metrics.max_width << "\n";

            // Count glyphs with strokes
            int glyphs_with_strokes = 0;
            int total_strokes = 0;
            for (uint8_t ch = font.get_first_char(); ch <= font.get_last_char(); ++ch) {
                auto glyph = font.get_glyph(ch);
                if (glyph && !glyph->strokes.empty()) {
                    glyphs_with_strokes++;
                    total_strokes += glyph->strokes.size();
                }
            }
            std::cout << "    glyphs_with_strokes: " << glyphs_with_strokes << "\n";
            std::cout << "    total_strokes: " << total_strokes << "\n";

            // Sample glyph 'A'
            auto glyph_A = font.get_glyph('A');
            if (glyph_A) {
                std::cout << "    Glyph 'A': width=" << glyph_A->width
                          << ", strokes=" << glyph_A->strokes.size() << "\n";
            }
        }
    } catch (const std::exception& e) {
        std::cout << "  Error loading vector fonts: " << e.what() << "\n";
    }
}

void dump_ttf_font(const std::string& name, const std::filesystem::path& path) {
    if (!test_data::file_exists(path)) return;

    print_subsection("TTF Font Details");

    auto data = test_data::load_file(path);

    try {
        // ttf_font (outline)
        ttf_font font(data);
        std::cout << "ttf_font:\n";
        std::cout << "  is_valid: " << (font.is_valid() ? "true" : "false") << "\n";

        if (font.is_valid()) {
            std::cout << "  font_count: " << ttf_font::get_font_count(data) << "\n";

            float test_size = 24.0f;
            auto metrics = font.get_metrics(test_size);
            std::cout << "  metrics (at " << test_size << "px):\n";
            std::cout << "    ascent: " << metrics.ascent << "\n";
            std::cout << "    descent: " << metrics.descent << "\n";
            std::cout << "    line_gap: " << metrics.line_gap << "\n";

            // Test some glyphs
            std::cout << "  Glyph samples (at " << test_size << "px):\n";
            for (char ch : {'A', 'M', 'g', ' '}) {
                std::cout << "    '" << ch << "': has_glyph="
                          << (font.has_glyph(ch) ? "true" : "false");
                auto gm = font.get_glyph_metrics(ch, test_size);
                if (gm) {
                    std::cout << ", advance_x=" << gm->advance_x
                              << ", width=" << gm->width
                              << ", height=" << gm->height;
                }
                std::cout << "\n";
            }

            // Kerning
            float kern_AV = font.get_kerning('A', 'V', test_size);
            float kern_To = font.get_kerning('T', 'o', test_size);
            std::cout << "  kerning (at " << test_size << "px):\n";
            std::cout << "    A-V: " << kern_AV << "\n";
            std::cout << "    T-o: " << kern_To << "\n";

            // Glyph shape for 'A'
            auto shape = font.get_glyph_shape('A', test_size);
            if (shape) {
                std::cout << "  Glyph 'A' shape: " << shape->vertices.size() << " vertices\n";
            }
        }

        // stb_truetype_font (rasterization)
        stb_truetype_font stb_font(data);
        std::cout << "\nstb_truetype_font:\n";
        std::cout << "  is_valid: " << (stb_font.is_valid() ? "true" : "false") << "\n";

        if (stb_font.is_valid()) {
            float test_size = 24.0f;
            std::cout << "  scale_for_pixel_height(" << test_size << "): "
                      << stb_font.get_scale_for_pixel_height(test_size) << "\n";

            // Rasterize 'A'
            auto bitmap = stb_font.rasterize('A', test_size);
            if (bitmap) {
                std::cout << "  Rasterized 'A' (at " << test_size << "px):\n";
                std::cout << "    width: " << bitmap->width << "\n";
                std::cout << "    height: " << bitmap->height << "\n";
                std::cout << "    advance_x: " << bitmap->advance_x << "\n";
                std::cout << "    offset_x: " << bitmap->offset_x << "\n";
                std::cout << "    offset_y: " << bitmap->offset_y << "\n";
                std::cout << "    bitmap_size: " << bitmap->bitmap.size() << " bytes\n";
            }
        }
    } catch (const std::exception& e) {
        std::cout << "  Error: " << e.what() << "\n";
    }
}

int main() {
    std::cout << "Font Metadata Dump - Ground Truth Generator\n";
    std::cout << "Test data base path: " << test_data::base_path() << "\n";

    // Windows FNT (ROMANP - vector font)
    dump_container_info("Windows FNT (ROMANP)", test_data::fnt_romanp());
    dump_vector_fonts("ROMANP", test_data::fnt_romanp());

    // Windows FON (HELVA - bitmap fonts)
    dump_container_info("Windows FON (HELVA)", test_data::fon_helva());
    dump_bitmap_fonts("HELVA", test_data::fon_helva());

    // Windows FON (VGAOEM)
    dump_container_info("Windows FON (VGAOEM)", test_data::fon_vgaoem());
    dump_bitmap_fonts("VGAOEM", test_data::fon_vgaoem());

    // BGI font (LITT)
    dump_container_info("BGI Font (LITT)", test_data::bgi_litt());
    dump_vector_fonts("LITT", test_data::bgi_litt());

    // OS/2 font (SYSFONT)
    dump_container_info("OS/2 Font (SYSFONT)", test_data::fon_os2_sysfont());
    dump_bitmap_fonts("SYSFONT", test_data::fon_os2_sysfont());

    // TTF (Arial)
    dump_container_info("TrueType (ARIAL)", test_data::ttf_arial());
    dump_ttf_font("ARIAL", test_data::ttf_arial());

    // TTF (Marlett)
    dump_container_info("TrueType (Marlett)", test_data::ttf_marlett());
    dump_ttf_font("Marlett", test_data::ttf_marlett());

    // FOT file
    dump_container_info("FOT (ARIAL.FOT)", test_data::fot_arial());

    std::cout << "\n" << std::string(70, '=') << "\n";
    std::cout << "Done.\n";

    return 0;
}
