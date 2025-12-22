//
// Created by igor on 21/12/2025.
//
// Text rendering demonstration using onyx_font
//
// This example demonstrates the complete text rendering pipeline:
// 1. Loading a font (TTF, bitmap, or vector)
// 2. Creating a glyph cache with texture atlas
// 3. Using the text renderer to draw text
// 4. Outputting to a PGM image file
//
// Usage: text_rendering_demo <font_file> [text]
//

#include <onyx_font/text/text_renderer.hh>
#include <onyx_font/text/glyph_cache.hh>
#include <onyx_font/text/font_source.hh>
#include <onyx_font/font_factory.hh>
#include <fstream>
#include <iostream>
#include <vector>
#include <algorithm>
#include <cctype>

using namespace onyx_font;

// Simple PGM output for demonstration
void write_pgm(const char* filename, const uint8_t* data, int w, int h) {
    std::ofstream f(filename, std::ios::binary);
    if (!f) {
        std::cerr << "Failed to create output file: " << filename << '\n';
        return;
    }
    f << "P5\n" << w << " " << h << "\n255\n";
    f.write(reinterpret_cast<const char*>(data), w * h);
    std::cout << "Wrote " << filename << " (" << w << "x" << h << ")\n";
}

// Detect font type from file extension
enum class detected_font_type { ttf, bitmap, vector, unknown };

detected_font_type detect_font_type(const std::string& filename) {
    auto dot = filename.rfind('.');
    if (dot == std::string::npos) return detected_font_type::unknown;

    std::string ext = filename.substr(dot + 1);
    // Convert to lowercase
    for (char& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

    if (ext == "ttf" || ext == "otf") return detected_font_type::ttf;
    if (ext == "fon" || ext == "fnt") return detected_font_type::bitmap;
    if (ext == "chr") return detected_font_type::vector;
    return detected_font_type::unknown;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <font_file> [text]\n";
        std::cerr << "Supported formats:\n";
        std::cerr << "  .ttf, .otf - TrueType/OpenType fonts\n";
        std::cerr << "  .fon, .fnt - Windows bitmap fonts\n";
        std::cerr << "  .chr       - BGI vector fonts\n";
        return 1;
    }

    const char* font_path = argv[1];
    std::string text = (argc > 2) ? argv[2] : "Hello, World!";

    // Load font file
    std::ifstream file(font_path, std::ios::binary);
    if (!file) {
        std::cerr << "Failed to open font file: " << font_path << '\n';
        return 1;
    }

    std::vector<uint8_t> data((std::istreambuf_iterator<char>(file)),
                               std::istreambuf_iterator<char>());
    file.close();

    if (data.empty()) {
        std::cerr << "Font file is empty\n";
        return 1;
    }

    // Detect font type and create source
    detected_font_type type = detect_font_type(font_path);
    float font_size = 48.0f;

    // Variables to hold font objects (keep in scope)
    ttf_font ttf_obj({});
    bitmap_font bitmap_obj;
    vector_font vector_obj;

    font_source source = [&]() -> font_source {
        switch (type) {
            case detected_font_type::ttf: {
                ttf_obj = ttf_font(data);
                if (!ttf_obj.is_valid()) {
                    std::cerr << "Failed to parse TTF font\n";
                    std::exit(1);
                }
                std::cout << "Loaded TTF font from: " << font_path << '\n';
                return font_source::from_ttf(ttf_obj);
            }
            case detected_font_type::bitmap: {
                bitmap_obj = font_factory::load_bitmap(data, 0);
                auto metrics = bitmap_obj.get_metrics();
                if (metrics.pixel_height == 0) {
                    std::cerr << "Failed to parse bitmap font\n";
                    std::exit(1);
                }
                std::cout << "Loaded bitmap font: " << bitmap_obj.get_name()
                          << " (" << metrics.pixel_height << "px)\n";
                font_size = static_cast<float>(metrics.pixel_height);
                return font_source::from_bitmap(bitmap_obj);
            }
            case detected_font_type::vector: {
                vector_obj = font_factory::load_vector(data, 0);
                auto metrics = vector_obj.get_metrics();
                if (metrics.pixel_height == 0) {
                    std::cerr << "Failed to parse vector font\n";
                    std::exit(1);
                }
                std::cout << "Loaded vector font: " << vector_obj.get_name() << '\n';
                return font_source::from_vector(vector_obj);
            }
            default:
                std::cerr << "Unknown font format. Use .ttf, .fon, or .chr\n";
                std::exit(1);
        }
    }();

    std::cout << "Rendering: \"" << text << "\" at " << font_size << "px\n";

    // Create glyph cache and renderer
    glyph_cache<memory_atlas> cache(std::move(source), font_size);
    text_renderer renderer(cache);

    // Measure text
    auto extents = renderer.measure(text);

    // Create output buffer with padding
    int padding = 10;
    int w = static_cast<int>(std::ceil(extents.width)) + padding * 2;
    int h = static_cast<int>(std::ceil(extents.height)) + padding * 2;
    std::vector<uint8_t> output(static_cast<std::size_t>(w * h), 0);

    // Blit function that copies glyphs to output buffer
    auto blit = [&](const memory_atlas& atlas, glyph_rect src,
                    float dx, float dy) {
        int dst_x = static_cast<int>(dx) + padding;
        int dst_y = static_cast<int>(dy) + padding;

        for (int y = 0; y < src.h; ++y) {
            for (int x = 0; x < src.w; ++x) {
                int ox = dst_x + x;
                int oy = dst_y + y;
                if (ox >= 0 && ox < w && oy >= 0 && oy < h) {
                    uint8_t alpha = atlas.pixel(src.x + x, src.y + y);
                    auto idx = static_cast<std::size_t>(oy * w + ox);
                    // Max blend for overlapping pixels
                    output[idx] = std::max(output[idx], alpha);
                }
            }
        }
    };

    // Render text
    float rendered_width = renderer.draw(text, 0, 0, blit);

    std::cout << "Rendered width: " << rendered_width << " pixels\n";
    std::cout << "Atlas pages: " << cache.atlas_count() << '\n';

    // Save output
    write_pgm("output.pgm", output.data(), w, h);

    // Also demonstrate wrapped text
    std::string long_text = "The quick brown fox jumps over the lazy dog. "
                            "Pack my box with five dozen liquor jugs.";

    text_box box{0, 0, 200, 300};
    auto wrapped_ext = renderer.measure_wrapped(long_text, box.w);

    int wrap_w = static_cast<int>(box.w) + padding * 2;
    int wrap_h = static_cast<int>(wrapped_ext.height) + padding * 2;
    std::vector<uint8_t> wrapped_output(static_cast<std::size_t>(wrap_w * wrap_h), 0);

    auto wrap_blit = [&](const memory_atlas& atlas, glyph_rect src,
                         float dx, float dy) {
        int dst_x = static_cast<int>(dx) + padding;
        int dst_y = static_cast<int>(dy) + padding;

        for (int y = 0; y < src.h; ++y) {
            for (int x = 0; x < src.w; ++x) {
                int ox = dst_x + x;
                int oy = dst_y + y;
                if (ox >= 0 && ox < wrap_w && oy >= 0 && oy < wrap_h) {
                    uint8_t alpha = atlas.pixel(src.x + x, src.y + y);
                    auto idx = static_cast<std::size_t>(oy * wrap_w + ox);
                    wrapped_output[idx] = std::max(wrapped_output[idx], alpha);
                }
            }
        }
    };

    box.y = 0;
    box.h = static_cast<float>(wrap_h);
    int lines = renderer.draw_wrapped(long_text, box, text_align::left, wrap_blit);

    std::cout << "Wrapped text: " << lines << " lines\n";
    write_pgm("wrapped.pgm", wrapped_output.data(), wrap_w, wrap_h);

    return 0;
}
