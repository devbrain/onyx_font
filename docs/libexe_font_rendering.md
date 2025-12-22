# Rendering Fonts with libexe

This document describes how to render glyphs from Windows .FON/.FNT bitmap fonts using the libexe library API.

## Overview

The libexe library provides font parsing through the `font_parser` class and `font_data` structure. This document shows how to use these APIs to:

1. Extract fonts from executable resources
2. Access font metadata and metrics
3. Convert column-major bitmap data to standard row-major format
4. Render individual glyphs and text strings
5. Handle glyph positioning with baseline alignment

## Quick Start

```cpp
#include <libexe/libexe.hpp>
#include <libexe/resources/parsers/font_parser.hpp>

// Load executable and access resources
auto exe = libexe::load("MYFONT.FON");
auto resources = exe->resources();

// Find and parse a font resource
auto font_entry = resources->find_resource(libexe::resource_type::RT_FONT, 1);
if (font_entry) {
    auto font = font_entry->as_font();  // Convenience method
    // Or: auto font = libexe::font_parser::parse(font_entry->data());

    if (font) {
        std::cout << "Font: " << font->face_name << "\n";
        std::cout << "Size: " << font->points << " pt\n";
        std::cout << "Height: " << font->pixel_height << " px\n";
    }
}
```

## Font Data Structure

The `libexe::font_data` structure contains all parsed font information:

```cpp
namespace libexe {

struct font_data {
    // =========================================================================
    // Font Metadata
    // =========================================================================
    uint16_t version;             // 0x0200 (Windows 2.x) or 0x0300 (Windows 3.x)
    uint32_t size;                // Total font file size
    std::string copyright;        // Copyright string (up to 60 chars)
    font_type type;               // RASTER, VECTOR, MEMORY, or DEVICE

    // =========================================================================
    // Font Metrics (for positioning)
    // =========================================================================
    uint16_t points;              // Nominal point size
    uint16_t vertical_res;        // Vertical resolution (DPI)
    uint16_t horizontal_res;      // Horizontal resolution (DPI)
    uint16_t ascent;              // Distance from baseline to top
    uint16_t internal_leading;    // Space for accent marks
    uint16_t external_leading;    // Extra line spacing

    // =========================================================================
    // Font Appearance
    // =========================================================================
    bool italic;                  // Italic style
    bool underline;               // Underlined style
    bool strikeout;               // Strikeout style
    uint16_t weight;              // Weight (400=normal, 700=bold)
    uint8_t charset;              // Character set ID

    // =========================================================================
    // Character Dimensions
    // =========================================================================
    uint16_t pixel_width;         // Char width (0 = variable-pitch)
    uint16_t pixel_height;        // Character height (ALL glyphs)
    uint16_t avg_width;           // Average character width
    uint16_t max_width;           // Maximum character width

    // =========================================================================
    // Character Range
    // =========================================================================
    uint8_t first_char;           // First character code in font
    uint8_t last_char;            // Last character code in font
    uint8_t default_char;         // Default character for missing chars
    uint8_t break_char;           // Word break character

    // =========================================================================
    // Font Family & Pitch
    // =========================================================================
    font_pitch pitch;             // FIXED or VARIABLE
    font_family family;           // ROMAN, SWISS, MODERN, SCRIPT, DECORATIVE
    std::string face_name;        // Font face name (e.g., "Courier", "Helv")

    // =========================================================================
    // Glyph Data
    // =========================================================================
    std::vector<glyph_entry> glyphs;    // Glyph table
    std::vector<uint8_t> bitmap_data;   // Raw bitmap data

    // =========================================================================
    // Helper Methods
    // =========================================================================
    size_t character_count() const;
    bool is_fixed_pitch() const;
    bool is_variable_pitch() const;
    std::span<const uint8_t> get_char_bitmap(uint8_t c) const;
};

struct glyph_entry {
    uint16_t width;               // Character width in pixels
    uint32_t offset;              // Offset into bitmap data

    // Optional ABC spacing (Windows 3.0+)
    std::optional<int16_t> a_space;   // Left side bearing
    std::optional<uint16_t> b_space;  // Glyph width
    std::optional<int16_t> c_space;   // Right side bearing
};

} // namespace libexe
```

## Extracting Fonts from Executables

### From NE (16-bit) .FON Files

```cpp
#include <libexe/formats/ne_file.hpp>

auto ne = libexe::ne_file::load("SYSTEM.FON");
auto resources = ne->resources();

// List all fonts
auto fonts = resources->resources_by_type(libexe::resource_type::RT_FONT);
std::cout << "Found " << fonts.size() << " fonts\n";

for (const auto& entry : fonts) {
    auto font = entry.as_font();
    if (font) {
        std::cout << "  " << font->face_name
                  << " " << font->points << "pt"
                  << " (" << font->character_count() << " chars)\n";
    }
}
```

### From PE (32-bit) .FON Files

```cpp
#include <libexe/formats/pe_file.hpp>

auto pe = libexe::pe_file::load("MODERN.FON");
auto resources = pe->resources();

// Find specific font by ID
auto font_entry = resources->find_resource(libexe::resource_type::RT_FONT, 1);
if (font_entry) {
    auto font = libexe::font_parser::parse(font_entry->data());
    // ...
}
```

### Using the Factory (Auto-Detection)

```cpp
#include <libexe/formats/executable_factory.hpp>

// Automatically detects NE vs PE format
auto exe = libexe::executable_factory::load("ANYFONT.FON");
auto resources = exe->resources();
// ...
```

## Bitmap Data Format

**Critical:** FNT bitmap data is stored in **column-major** format (not row-major). The `get_char_bitmap()` method returns raw column-major data that must be transposed for standard rendering.

### Column-Major Layout

```
For a glyph 8 pixels wide, 16 pixels tall:

Raw data layout (column-major):
  Byte 0:   Column 0, Rows 0-7   (bits: top to bottom)
  Byte 1:   Column 0, Rows 8-15
  Byte 2:   Column 1, Rows 0-7
  Byte 3:   Column 1, Rows 8-15
  ... and so on

Bit order within each byte:
  Bit 7 (MSB) = topmost pixel of the 8
  Bit 0 (LSB) = bottommost pixel of the 8
```

### Transposition Algorithm

Convert column-major (libexe) to row-major (standard bitmap):

```cpp
struct rendered_glyph {
    std::vector<uint8_t> pixels;  // Row-major, 1 bit per pixel
    uint32_t width;               // Width in pixels
    uint32_t height;              // Height in pixels
    uint32_t pitch;               // Bytes per row
};

rendered_glyph transpose_glyph(const libexe::font_data& font, uint8_t char_code) {
    // Get raw column-major bitmap
    auto raw_bitmap = font.get_char_bitmap(char_code);
    if (raw_bitmap.empty()) {
        return {};
    }

    // Get glyph metrics
    size_t glyph_index = char_code - font.first_char;
    if (glyph_index >= font.glyphs.size()) {
        return {};
    }

    const auto& glyph = font.glyphs[glyph_index];

    rendered_glyph result;
    result.width = glyph.width;
    result.height = font.pixel_height;
    result.pitch = (glyph.width + 7) / 8;  // Round up to byte boundary

    // Allocate output buffer (row-major format)
    result.pixels.resize(result.pitch * result.height, 0);

    // Transpose column-major to row-major
    const uint8_t* src = raw_bitmap.data();
    size_t src_pitch = font.pixel_height;  // Bytes per column in source

    for (uint32_t col = 0; col < result.pitch; col++) {
        uint8_t* dst_column = result.pixels.data() + col;
        const uint8_t* src_column = src + col * src_pitch;

        for (uint32_t row = 0; row < result.height; row++) {
            dst_column[row * result.pitch] = src_column[row];
        }
    }

    return result;
}
```

### Alternative: Pixel-by-Pixel Access

For rendering directly without full transposition:

```cpp
bool get_pixel(const libexe::font_data& font,
               std::span<const uint8_t> raw_bitmap,
               uint16_t glyph_width,
               int x, int y) {
    if (x < 0 || x >= glyph_width || y < 0 || y >= font.pixel_height) {
        return false;
    }

    // Column-major: each column is pixel_height bytes tall
    size_t column_bytes = font.pixel_height;
    size_t col_index = x / 8;
    size_t bit_in_col = 7 - (x % 8);  // MSB = leftmost pixel

    // Offset into raw bitmap
    size_t byte_offset = col_index * column_bytes + y;

    if (byte_offset >= raw_bitmap.size()) {
        return false;
    }

    return (raw_bitmap[byte_offset] >> bit_in_col) & 1;
}
```

## Glyph Metrics and Positioning

### Font-Level Metrics

```cpp
struct font_metrics {
    int32_t ascender;        // Pixels above baseline
    int32_t descender;       // Pixels below baseline (negative)
    int32_t line_height;     // Total line height
    int32_t max_advance;     // Maximum glyph width
};

font_metrics get_font_metrics(const libexe::font_data& font) {
    font_metrics m;
    m.ascender = font.ascent;
    m.descender = -(static_cast<int32_t>(font.pixel_height) - font.ascent);
    m.line_height = font.pixel_height + font.external_leading;
    m.max_advance = font.max_width;
    return m;
}
```

### Glyph-Level Metrics

```cpp
struct glyph_metrics {
    int32_t width;           // Bitmap width in pixels
    int32_t height;          // Bitmap height in pixels
    int32_t bitmap_left;     // X offset from origin (always 0 for FNT)
    int32_t bitmap_top;      // Y offset from baseline (= ascent)
    int32_t advance_x;       // Horizontal advance
};

glyph_metrics get_glyph_metrics(const libexe::font_data& font, uint8_t char_code) {
    glyph_metrics m = {};

    if (char_code < font.first_char || char_code > font.last_char) {
        return m;  // Use default glyph
    }

    size_t idx = char_code - font.first_char;
    if (idx >= font.glyphs.size()) {
        return m;
    }

    const auto& glyph = font.glyphs[idx];

    m.width = glyph.width;
    m.height = font.pixel_height;
    m.bitmap_left = 0;
    m.bitmap_top = font.ascent;
    m.advance_x = glyph.width;

    // Use ABC spacing if available (Windows 3.0+)
    if (glyph.a_space && glyph.b_space && glyph.c_space) {
        m.bitmap_left = *glyph.a_space;
        m.advance_x = *glyph.a_space + *glyph.b_space + *glyph.c_space;
    }

    return m;
}
```

## Baseline-Aligned Text Rendering

### Understanding bitmap_top

The `bitmap_top` metric equals `font.ascent` for FNT fonts. It represents the distance from the **baseline** to the **top edge of the glyph bitmap**.

```
                    +-------------+
                    |             |  ^
                    |   Glyph     |  | bitmap_top (= ascent)
                    |   Bitmap    |  |
  baseline_y -------+-------------+--+---------------------
                    |  descender  |  ^
                    |    part     |  | (pixel_height - ascent)
                    +-------------+  v

  glyph_y (top-left) = baseline_y - bitmap_top
```

### Rendering at Baseline

```cpp
void render_text_at_baseline(
    uint8_t* framebuffer,
    int fb_width,
    int fb_height,
    const libexe::font_data& font,
    const char* text,
    int start_x,
    int baseline_y,
    uint8_t color)
{
    int cursor_x = start_x;
    int glyph_top_y = baseline_y - font.ascent;

    for (const char* p = text; *p; p++) {
        uint8_t c = static_cast<uint8_t>(*p);

        // Get glyph info
        if (c < font.first_char || c > font.last_char) {
            c = font.default_char + font.first_char;  // Use default glyph
        }

        size_t idx = c - font.first_char;
        if (idx >= font.glyphs.size()) continue;

        const auto& glyph = font.glyphs[idx];
        auto raw_bitmap = font.get_char_bitmap(c);

        if (!raw_bitmap.empty()) {
            // Render each pixel (transposing on the fly)
            for (int y = 0; y < font.pixel_height; y++) {
                for (int x = 0; x < glyph.width; x++) {
                    // Get pixel from column-major source
                    size_t col_bytes = font.pixel_height;
                    size_t col_idx = x / 8;
                    size_t bit = 7 - (x % 8);
                    size_t byte_off = col_idx * col_bytes + y;

                    if (byte_off < raw_bitmap.size()) {
                        bool pixel = (raw_bitmap[byte_off] >> bit) & 1;

                        if (pixel) {
                            int screen_x = cursor_x + x;
                            int screen_y = glyph_top_y + y;

                            if (screen_x >= 0 && screen_x < fb_width &&
                                screen_y >= 0 && screen_y < fb_height) {
                                framebuffer[screen_y * fb_width + screen_x] = color;
                            }
                        }
                    }
                }
            }
        }

        cursor_x += glyph.width;
    }
}
```

### Multi-Line Text Rendering

```cpp
void render_multiline_text(
    uint8_t* framebuffer,
    int fb_width,
    int fb_height,
    const libexe::font_data& font,
    const std::vector<std::string>& lines,
    int start_x,
    int start_y,  // Top of text area
    uint8_t color)
{
    int line_height = font.pixel_height + font.external_leading;
    int baseline_y = start_y + font.ascent;  // First baseline

    for (const auto& line : lines) {
        render_text_at_baseline(framebuffer, fb_width, fb_height,
                                font, line.c_str(), start_x, baseline_y, color);
        baseline_y += line_height;
    }
}
```

## Complete Rendering Class

```cpp
#include <libexe/resources/parsers/font_parser.hpp>
#include <vector>
#include <cstdint>

class fnt_renderer {
public:
    explicit fnt_renderer(const libexe::font_data& font)
        : font_(font) {}

    // =========================================================================
    // Glyph Rendering
    // =========================================================================

    struct bitmap {
        std::vector<uint8_t> data;  // Row-major, 1 bit per pixel
        uint32_t width;
        uint32_t height;
        uint32_t pitch;             // Bytes per row
    };

    /**
     * Render a single character to a bitmap.
     * Returns row-major bitmap data (transposed from column-major source).
     */
    bitmap render_char(uint8_t char_code) const {
        bitmap result;

        // Map to glyph index
        uint8_t c = char_code;
        if (c < font_.first_char || c > font_.last_char) {
            c = font_.default_char + font_.first_char;
        }

        size_t idx = c - font_.first_char;
        if (idx >= font_.glyphs.size()) {
            return result;
        }

        const auto& glyph = font_.glyphs[idx];
        auto raw = font_.get_char_bitmap(c);

        if (raw.empty()) {
            return result;
        }

        result.width = glyph.width;
        result.height = font_.pixel_height;
        result.pitch = (glyph.width + 7) / 8;
        result.data.resize(result.pitch * result.height, 0);

        // Transpose column-major to row-major
        size_t src_pitch = font_.pixel_height;

        for (uint32_t col = 0; col < result.pitch; col++) {
            for (uint32_t row = 0; row < result.height; row++) {
                size_t src_offset = col * src_pitch + row;
                size_t dst_offset = row * result.pitch + col;

                if (src_offset < raw.size() && dst_offset < result.data.size()) {
                    result.data[dst_offset] = raw[src_offset];
                }
            }
        }

        return result;
    }

    /**
     * Render a string to a bitmap.
     */
    bitmap render_string(const char* text) const {
        // Calculate total width
        uint32_t total_width = 0;
        for (const char* p = text; *p; p++) {
            uint8_t c = static_cast<uint8_t>(*p);
            if (c < font_.first_char || c > font_.last_char) {
                c = font_.default_char + font_.first_char;
            }
            size_t idx = c - font_.first_char;
            if (idx < font_.glyphs.size()) {
                total_width += font_.glyphs[idx].width;
            }
        }

        bitmap result;
        result.width = total_width;
        result.height = font_.pixel_height;
        result.pitch = (total_width + 7) / 8;
        result.data.resize(result.pitch * result.height, 0);

        // Render each character
        uint32_t x_offset = 0;
        for (const char* p = text; *p; p++) {
            bitmap glyph_bmp = render_char(static_cast<uint8_t>(*p));
            blit_glyph(result, glyph_bmp, x_offset, 0);
            x_offset += glyph_bmp.width;
        }

        return result;
    }

    // =========================================================================
    // Metrics
    // =========================================================================

    uint16_t ascent() const { return font_.ascent; }
    uint16_t descent() const { return font_.pixel_height - font_.ascent; }
    uint16_t line_height() const {
        return font_.pixel_height + font_.external_leading;
    }

    uint16_t char_width(uint8_t c) const {
        if (c < font_.first_char || c > font_.last_char) {
            c = font_.default_char + font_.first_char;
        }
        size_t idx = c - font_.first_char;
        if (idx < font_.glyphs.size()) {
            return font_.glyphs[idx].width;
        }
        return 0;
    }

    uint32_t string_width(const char* text) const {
        uint32_t width = 0;
        for (const char* p = text; *p; p++) {
            width += char_width(static_cast<uint8_t>(*p));
        }
        return width;
    }

private:
    const libexe::font_data& font_;

    void blit_glyph(bitmap& dst, const bitmap& src, uint32_t x, uint32_t y) const {
        for (uint32_t row = 0; row < src.height && (y + row) < dst.height; row++) {
            for (uint32_t col = 0; col < src.width; col++) {
                // Get source pixel
                uint32_t src_byte = col / 8;
                uint32_t src_bit = 7 - (col % 8);
                bool pixel = (src.data[row * src.pitch + src_byte] >> src_bit) & 1;

                if (pixel) {
                    // Set destination pixel
                    uint32_t dst_col = x + col;
                    if (dst_col < dst.width) {
                        uint32_t dst_byte = dst_col / 8;
                        uint32_t dst_bit = 7 - (dst_col % 8);
                        dst.data[(y + row) * dst.pitch + dst_byte] |= (1 << dst_bit);
                    }
                }
            }
        }
    }
};
```

## Character Set Handling

The `charset` field indicates the character encoding:

| Value | Constant | Codepage | Description |
|-------|----------|----------|-------------|
| 0 | ANSI_CHARSET | CP1252 | Western European |
| 1 | DEFAULT_CHARSET | - | System default |
| 2 | SYMBOL_CHARSET | - | Symbol font |
| 77 | MAC_CHARSET | - | Macintosh Roman |
| 128 | SHIFTJIS_CHARSET | CP932 | Japanese |
| 129 | HANGUL_CHARSET | CP949 | Korean |
| 134 | GB2312_CHARSET | CP936 | Simplified Chinese |
| 136 | CHINESEBIG5_CHARSET | CP950 | Traditional Chinese |
| 161 | GREEK_CHARSET | CP1253 | Greek |
| 162 | TURKISH_CHARSET | CP1254 | Turkish |
| 177 | HEBREW_CHARSET | CP1255 | Hebrew |
| 178 | ARABIC_CHARSET | CP1256 | Arabic |
| 186 | BALTIC_CHARSET | CP1257 | Baltic |
| 204 | RUSSIAN_CHARSET | CP1251 | Cyrillic |
| 222 | THAI_CHARSET | CP874 | Thai |
| 238 | EASTEUROPE_CHARSET | CP1250 | Central European |
| 255 | OEM_CHARSET | - | OEM (system-dependent) |

### Unicode Conversion

```cpp
#include <codecvt>
#include <locale>

// Convert UTF-8 to font's character set for rendering
std::string utf8_to_charset(const std::string& utf8, uint8_t charset) {
    // For ANSI (CP1252), simple conversion
    if (charset == 0) {
        // Use Windows-1252 conversion
        // (implementation depends on your platform)
    }

    // For OEM charset, use system OEM codepage
    if (charset == 255) {
        // Platform-specific OEM conversion
    }

    // For others, use appropriate iconv/ICU conversion
    return utf8;  // Placeholder
}
```

## Font Type Validation

Before rendering, validate the font type:

```cpp
bool can_render_font(const libexe::font_data& font) {
    uint16_t type_value = static_cast<uint16_t>(font.type);

    // Check for vector font (not supported)
    if (type_value & 0x0001) {
        return false;  // Vector font
    }

    // Check for ROM/memory font (bitmap not in file)
    if (type_value & 0x0004) {
        return false;  // Memory font
    }

    // Basic validation
    if (font.pixel_height == 0) {
        return false;
    }

    if (font.last_char < font.first_char) {
        return false;
    }

    if (font.glyphs.empty()) {
        return false;
    }

    return true;
}
```

## Converting to PNG

Example using stb_image_write (header-only library):

```cpp
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

void save_glyph_png(const fnt_renderer::bitmap& bmp, const char* filename) {
    // Convert 1-bit to 8-bit grayscale
    std::vector<uint8_t> pixels(bmp.width * bmp.height);

    for (uint32_t y = 0; y < bmp.height; y++) {
        for (uint32_t x = 0; x < bmp.width; x++) {
            uint32_t byte_idx = y * bmp.pitch + x / 8;
            uint32_t bit_idx = 7 - (x % 8);
            bool pixel = (bmp.data[byte_idx] >> bit_idx) & 1;
            pixels[y * bmp.width + x] = pixel ? 255 : 0;
        }
    }

    stbi_write_png(filename, bmp.width, bmp.height, 1, pixels.data(), bmp.width);
}

// Save entire font as sprite sheet
void save_font_sheet(const libexe::font_data& font, const char* filename) {
    fnt_renderer renderer(font);

    // Calculate grid size
    int chars_per_row = 16;
    int char_count = font.last_char - font.first_char + 1;
    int rows = (char_count + chars_per_row - 1) / chars_per_row;

    int cell_width = font.max_width;
    int cell_height = font.pixel_height;

    int sheet_width = chars_per_row * cell_width;
    int sheet_height = rows * cell_height;

    std::vector<uint8_t> sheet(sheet_width * sheet_height, 0);

    for (int i = 0; i < char_count; i++) {
        uint8_t c = font.first_char + i;
        auto glyph = renderer.render_char(c);

        int grid_x = (i % chars_per_row) * cell_width;
        int grid_y = (i / chars_per_row) * cell_height;

        // Copy glyph to sheet
        for (uint32_t y = 0; y < glyph.height; y++) {
            for (uint32_t x = 0; x < glyph.width; x++) {
                uint32_t src_byte = y * glyph.pitch + x / 8;
                uint32_t src_bit = 7 - (x % 8);
                bool pixel = (glyph.data[src_byte] >> src_bit) & 1;

                if (pixel) {
                    sheet[(grid_y + y) * sheet_width + grid_x + x] = 255;
                }
            }
        }
    }

    stbi_write_png(filename, sheet_width, sheet_height, 1, sheet.data(), sheet_width);
}
```

## Error Handling

```cpp
std::optional<libexe::font_data> load_font_safe(
    const libexe::resource_entry& entry)
{
    try {
        auto font = libexe::font_parser::parse(entry.data());

        if (!font) {
            std::cerr << "Failed to parse font resource\n";
            return std::nullopt;
        }

        if (!can_render_font(*font)) {
            std::cerr << "Font type not supported for rendering\n";
            return std::nullopt;
        }

        return font;
    }
    catch (const std::exception& e) {
        std::cerr << "Exception parsing font: " << e.what() << "\n";
        return std::nullopt;
    }
}
```

## See Also

- [fon_rendering.md](fon_rendering.md) - Detailed format specification and algorithms
- [libexe API Reference](../include/libexe/) - Complete API documentation
- `src/libexe/formats/resources/fonts.ds` - DataScript schema for FNT parsing
