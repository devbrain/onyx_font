# onyx_font Programmer's Guide

**Version 1.0.0**

A comprehensive guide to using the onyx_font library for loading fonts and rendering text in C++ applications.

---

## Table of Contents

1. [Introduction](#introduction)
2. [Installation](#installation)
3. [Quick Start](#quick-start)
4. [Core Concepts](#core-concepts)
5. [Font Types](#font-types)
6. [Loading Fonts](#loading-fonts)
7. [Basic Text Rendering](#basic-text-rendering)
8. [Advanced Text Rendering](#advanced-text-rendering)
9. [Gradient Text Rendering](#gradient-text-rendering)
10. [SDL2 Integration](#sdl2-integration)
11. [Font Conversion](#font-conversion)
12. [UTF-8 Support](#utf-8-support)
13. [Performance Considerations](#performance-considerations)
14. [API Reference Summary](#api-reference-summary)

---

## Introduction

onyx_font is a C++20 library for loading, manipulating, and rendering fonts from various formats. It provides a unified interface for working with:

- **Bitmap fonts** - Fixed-size raster fonts (Windows .FON/.FNT, BIOS dumps)
- **Vector fonts** - Stroke-based scalable fonts (Windows vector .FON, Borland BGI .CHR)
- **TrueType/OpenType fonts** - Bezier outline fonts (.TTF, .OTF, .TTC)

### Key Features

- Multi-format font loading with automatic format detection
- High-quality text rasterization with antialiasing
- GPU-friendly texture atlas with glyph caching
- Full Unicode support via UTF-8
- Text measurement and word wrapping
- Font conversion (vector/TTF to bitmap)
- Zero external dependencies for core functionality

### Design Philosophy

The library is designed around several key principles:

1. **Type Safety** - Strong typing with C++20 concepts and enums
2. **Zero-Copy Where Possible** - Views and spans avoid unnecessary copies
3. **Separation of Concerns** - Loading, storage, and rendering are decoupled
4. **Flexibility** - Template-based rendering targets for any graphics API

---

## Installation

### Requirements

- C++20 compatible compiler (GCC 10+, Clang 12+, MSVC 19.29+)
- CMake 3.20 or higher

### Using CMake FetchContent

```cmake
include(FetchContent)

FetchContent_Declare(
    onyx_font
    GIT_REPOSITORY git@github.com:devbrain/onyx_font.git
    GIT_TAG v1.0.0
)

FetchContent_MakeAvailable(onyx_font)

target_link_libraries(your_target PRIVATE onyx_font::onyx_font)
```

### Building from Source

```bash
git clone git@github.com:devbrain/onyx_font.git
cd onyx_font
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .
```

### CMake Options

| Option | Default | Description |
|--------|---------|-------------|
| `ONYX_FONT_BUILD_TESTS` | ON (main) | Build unit tests |
| `ONYX_FONT_BUILD_EXAMPLES` | ON (main) | Build example applications |
| `ONYX_FONT_BUILD_DEMOS` | ON (main) | Build SDL/ImGui demos |
| `ONYX_FONT_BUILD_DOCS` | OFF | Build Doxygen documentation |
| `ONYX_FONT_BUILD_SHARED` | ON | Build shared library (OFF for static) |

---

## Quick Start

Here's a minimal example that loads a TrueType font and renders text to a buffer:

```cpp
#include <onyx_font/font_factory.hh>
#include <onyx_font/text/font_source.hh>
#include <onyx_font/text/text_rasterizer.hh>
#include <onyx_font/text/raster_target.hh>
#include <fstream>
#include <vector>

int main() {
    using namespace onyx_font;

    // 1. Load font file
    std::ifstream file("arial.ttf", std::ios::binary);
    std::vector<uint8_t> data((std::istreambuf_iterator<char>(file)),
                               std::istreambuf_iterator<char>());

    // 2. Create font source
    auto ttf = font_factory::load_ttf(data);
    font_source font = font_source::from_ttf(ttf, 24.0f);  // 24px size

    // 3. Measure text
    text_rasterizer rasterizer(font);
    auto extents = rasterizer.measure_text("Hello, World!");
    // extents.width, extents.height contain pixel dimensions

    // 4. Create render target
    std::vector<uint8_t> pixels(extents.width * extents.height, 0);
    owned_grayscale_target target(pixels.data(), extents.width, extents.height);

    // 5. Render text
    rasterizer.rasterize_text("Hello, World!", target, 0, font.get_metrics().ascent);

    // pixels now contains grayscale alpha values (0-255)
    return 0;
}
```

---

## Core Concepts

### Coordinate System

onyx_font uses screen coordinates:

```
     (0,0)
       +-------------------------> X
       |
       |  +-----------------+  <- top of cell (y=0)
       |  |  internal       |
       |  |  leading        |  <- space for accents
       |  +-----------------+
       |  |  A   M   H      |  <- glyph body
       |  |  A A MMM H H    |
       |  | AAAA M M HHH    |
       |  | A  A M M H H    |
       |  +-----------------+  <- baseline (y=ascent)
       |  |  g   p   y      |  <- descenders
       |  +-----------------+  <- bottom (y=pixel_height)
       v
       Y
```

- **Origin (0,0)** is top-left
- **Y increases downward**
- **Baseline** is at `y = ascent` from the top

### ABC Spacing Model

Bitmap fonts use the ABC spacing model for character positioning:

```
|<-- A -->|<---- B ---->|<-- C -->|
|         |             |         |
| bearing | glyph width | bearing |
| (left)  |   (black)   | (right) |
```

- **A-space**: Left side bearing (can be negative for overhangs)
- **B-space**: Width of the actual glyph bitmap
- **C-space**: Right side bearing (can be negative for kerning)

Total advance = A + B + C

### Font Metrics

Every font provides metrics for text layout:

```cpp
struct font_metrics {
    uint16_t ascent;          // Baseline position from top
    uint16_t pixel_height;    // Total character cell height
    uint16_t internal_leading; // Space for accents (within ascent)
    uint16_t external_leading; // Recommended inter-line spacing
    uint16_t avg_width;       // Average character width
    uint16_t max_width;       // Maximum character width
};
```

**Line height** = `pixel_height + external_leading`

---

## Font Types

### Bitmap Fonts (`bitmap_font`)

Fixed-size raster fonts where each glyph is a 1-bit per pixel bitmap.

**Characteristics:**
- Pixel-perfect at native size
- Blocky when scaled
- Fast rendering (simple bit operations)
- Small memory footprint

**Common sources:**
- Windows .FON files (System, Terminal, Fixedsys)
- Raw BIOS dumps (VGA 8x8, 8x14, 8x16)
- Converted from vector/TTF fonts

```cpp
#include <onyx_font/bitmap_font.hh>

bitmap_font font = font_factory::load_bitmap(data, 0);

// Get glyph bitmap
bitmap_view glyph = font.get_glyph('A');

// Access pixels
for (uint16_t y = 0; y < glyph.height(); ++y) {
    for (uint16_t x = 0; x < glyph.width(); ++x) {
        if (glyph.pixel(x, y)) {
            draw_pixel(pen_x + x, pen_y + y, color);
        }
    }
}

// Or use packed row access for speed
for (uint16_t y = 0; y < glyph.height(); ++y) {
    auto row = glyph.row(y);  // std::span<const std::byte>
    blit_packed_row(row, pen_x, pen_y + y);
}
```

### Vector Fonts (`vector_font`)

Stroke-based fonts where glyphs are sequences of drawing commands.

**Characteristics:**
- Scalable to any size
- Simpler than bezier fonts
- Good for technical/CAD applications
- May look thin at small sizes

**Common sources:**
- Windows vector .FON files (Modern, Roman, Script)
- Borland BGI .CHR files (TRIP, LITT, SANS, etc.)

```cpp
#include <onyx_font/vector_font.hh>

vector_font font = font_factory::load_vector(data, 0);

// Get glyph stroke commands
const vector_glyph* glyph = font.get_glyph('A');

int pen_x = origin_x, pen_y = origin_y;
bool pen_down = false;

for (const auto& cmd : glyph->strokes) {
    switch (cmd.type) {
        case stroke_type::MOVE_TO:
            pen_x += cmd.dx;
            pen_y += cmd.dy;
            pen_down = true;
            break;

        case stroke_type::LINE_TO: {
            int new_x = pen_x + cmd.dx;
            int new_y = pen_y + cmd.dy;
            if (pen_down) {
                draw_line(pen_x, pen_y, new_x, new_y, color);
            }
            pen_x = new_x;
            pen_y = new_y;
            break;
        }

        case stroke_type::END:
            pen_down = false;
            break;
    }
}
```

### TrueType Fonts (`ttf_font`)

Bezier curve outline fonts providing the highest quality at any size.

**Characteristics:**
- Perfect scaling with antialiasing
- Support for hinting
- Large character sets (full Unicode)
- Kerning support
- Larger file sizes

**Common sources:**
- TrueType fonts (.ttf)
- OpenType fonts (.otf)
- TrueType Collections (.ttc)

```cpp
#include <onyx_font/ttf_font.hh>

// Load font (data must remain valid!)
std::vector<uint8_t> font_data = read_file("arial.ttf");
ttf_font font(font_data);

if (!font.is_valid()) {
    // Handle error
    return;
}

// Get metrics for 24px rendering
auto metrics = font.get_metrics(24.0f);
float line_height = metrics.ascent - metrics.descent + metrics.line_gap;

// Get glyph outline for path rendering
auto shape = font.get_glyph_shape('A', 24.0f);
if (shape) {
    for (const auto& v : shape->vertices) {
        switch (v.type) {
            case ttf_vertex_type::MOVE_TO:
                path.move_to(v.x, v.y);
                break;
            case ttf_vertex_type::LINE_TO:
                path.line_to(v.x, v.y);
                break;
            case ttf_vertex_type::CURVE_TO:
                path.quad_bezier_to(v.cx, v.cy, v.x, v.y);
                break;
            case ttf_vertex_type::CUBIC_TO:
                path.cubic_bezier_to(v.cx, v.cy, v.cx1, v.cy1, v.x, v.y);
                break;
        }
    }
}

// Apply kerning
float kern = font.get_kerning('A', 'V', 24.0f);  // Usually negative
pen_x += glyph_advance + kern;
```

---

## Loading Fonts

### Using font_factory

The `font_factory` class provides a unified interface for loading fonts from various formats:

```cpp
#include <onyx_font/font_factory.hh>

std::vector<uint8_t> data = read_file("font.fon");

// Analyze file to discover format and contents
container_info info = font_factory::analyze(data);

std::cout << "Format: " << font_factory::format_name(info.format) << "\n";
std::cout << "Contains " << info.fonts.size() << " fonts:\n";

for (const auto& entry : info.fonts) {
    std::cout << "  - " << entry.name
              << " (" << font_factory::type_name(entry.type) << ")"
              << " " << entry.pixel_height << "px\n";
}
```

### Supported Container Formats

| Format | Extension | Description |
|--------|-----------|-------------|
| `TTF` | .ttf | TrueType font (single outline font) |
| `OTF` | .otf | OpenType font (single outline font) |
| `TTC` | .ttc | TrueType Collection (multiple fonts) |
| `FNT` | .fnt | Raw Windows font resource |
| `FON_NE` | .fon | Windows 16-bit NE executable |
| `FON_PE` | .fon | Windows 32/64-bit PE executable |
| `FON_LX` | .fon | OS/2 LX executable |
| `BGI` | .chr | Borland Graphics Interface font |

### Loading by Type

```cpp
// Load specific font types
bitmap_font bitmap = font_factory::load_bitmap(data, 0);
vector_font vector = font_factory::load_vector(data, 0);
ttf_font ttf = font_factory::load_ttf(data, 0);

// Load all fonts of a type
std::vector<bitmap_font> all_bitmaps = font_factory::load_all_bitmaps(data);
std::vector<vector_font> all_vectors = font_factory::load_all_vectors(data);
```

### Loading Raw BIOS Fonts

For raw BIOS font dumps (no header, just pixel data):

```cpp
// Standard VGA 8x16 font (4096 bytes)
auto font = font_factory::load_raw(data, raw_font_options::vga_8x16());

// Standard VGA 8x8 font (2048 bytes)
auto font = font_factory::load_raw(data, raw_font_options::vga_8x8());

// Custom format
raw_font_options opts;
opts.char_width = 8;
opts.char_height = 12;
opts.first_char = 32;
opts.char_count = 96;
opts.msb_first = true;
opts.name = "Custom 8x12";
auto font = font_factory::load_raw(data, opts);
```

---

## Basic Text Rendering

### Using font_source

`font_source` provides a unified abstraction over all font types:

```cpp
#include <onyx_font/text/font_source.hh>

// Create from different font types
font_source src1 = font_source::from_bitmap(bitmap_font);
font_source src2 = font_source::from_vector(vector_font, 16.0f);  // 16px
font_source src3 = font_source::from_ttf(ttf_font, 24.0f);        // 24px

// Get unified metrics
scaled_metrics metrics = src1.get_metrics();
// metrics.ascent, metrics.descent, metrics.line_height, etc.

// Rasterize glyphs to any target
grayscale_target target(buffer, width, height);
auto glyph_metrics = src1.rasterize_glyph<grayscale_target>('A', target, x, y);
```

### Using text_rasterizer

For rendering complete text strings:

```cpp
#include <onyx_font/text/text_rasterizer.hh>
#include <onyx_font/text/raster_target.hh>

font_source font = font_source::from_ttf(ttf, 24.0f);
text_rasterizer rasterizer(font);

// Measure text first
text_extents extents = rasterizer.measure_text("Hello, World!");
// extents.width, extents.height, extents.advance

// Create target buffer
std::vector<uint8_t> pixels(extents.width * extents.height, 0);
owned_grayscale_target target(pixels.data(), extents.width, extents.height);

// Render at position (0, baseline)
float baseline_y = font.get_metrics().ascent;
rasterizer.rasterize_text("Hello, World!", target, 0, baseline_y);
```

### Render Targets

The library provides several built-in render targets:

```cpp
#include <onyx_font/text/raster_target.hh>

// Grayscale (overwrites pixels)
grayscale_target target(buffer, width, height, stride);

// Grayscale with max blending (for bold simulation)
grayscale_max_target target(buffer, width, height, stride);

// RGBA with alpha blending
rgba_blend_target target(rgba_buffer, width, height, stride, r, g, b);

// Custom callback
callback_target target([](int x, int y, uint8_t alpha) {
    set_pixel(x, y, color_with_alpha(alpha));
}, width, height);

// Null target (measurement only, no rendering)
null_target target(width, height);

// Owned buffer (manages its own memory)
owned_grayscale_target target(buffer, width, height);
```

### Custom Render Targets

Create your own by satisfying the `raster_target` concept:

```cpp
class my_target {
public:
    void put_pixel(int x, int y, uint8_t alpha) {
        // Your rendering logic
    }

    int width() const { return m_width; }
    int height() const { return m_height; }

private:
    int m_width, m_height;
};

// Optional: implement put_span for performance
class my_fast_target {
public:
    void put_pixel(int x, int y, uint8_t alpha);
    void put_span(int x, int y, std::span<const uint8_t> alphas);
    int width() const;
    int height() const;
};
```

---

## Advanced Text Rendering

### Text Alignment and Wrapping

Use `text_renderer` for high-level text rendering with alignment:

```cpp
#include <onyx_font/text/text_renderer.hh>
#include <onyx_font/text/glyph_cache.hh>

// Create glyph cache with atlas
glyph_cache_config config;
config.atlas_width = 512;
config.atlas_height = 512;
glyph_cache<memory_atlas> cache(font_source, config);

// Create renderer
text_renderer<memory_atlas> renderer(cache);

// Define blit callback (copies from atlas to your surface)
auto blit = [&](int dst_x, int dst_y, int src_x, int src_y, int w, int h) {
    copy_rect(screen, atlas.data(), dst_x, dst_y, src_x, src_y, w, h);
};

// Draw with alignment
text_box box{100, 100, 300, 200};  // x, y, width, height
renderer.draw_aligned("Centered Text", box, text_align::CENTER, text_valign::MIDDLE, blit);

// Draw with word wrapping
renderer.draw_wrapped("This is a long text that will wrap...", box, text_align::LEFT, blit);
```

### Texture Atlas and Glyph Caching

For GPU rendering, use the glyph cache with a custom atlas surface:

```cpp
#include <onyx_font/text/glyph_cache.hh>
#include <onyx_font/text/atlas_surface.hh>

// Custom GPU texture atlas
class gpu_atlas {
public:
    gpu_atlas(uint16_t w, uint16_t h) : m_texture(create_texture(w, h)) {}

    uint16_t width() const { return m_texture.width(); }
    uint16_t height() const { return m_texture.height(); }

    void write_alpha(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                     std::span<const uint8_t> data) {
        m_texture.upload_region(x, y, w, h, data);
    }

private:
    gpu_texture m_texture;
};

// Create cache with GPU atlas
glyph_cache<gpu_atlas> cache(font_source, {512, 512});

// Get cached glyph (rasterizes on first access)
if (auto* glyph = cache.get(U'A')) {
    // glyph->atlas_x, glyph->atlas_y - position in atlas
    // glyph->width, glyph->height - glyph dimensions
    // glyph->bearing_x, glyph->bearing_y - positioning offsets
    // glyph->advance_x - horizontal advance

    draw_textured_quad(
        glyph->atlas_x, glyph->atlas_y,
        glyph->width, glyph->height,
        pen_x + glyph->bearing_x,
        pen_y - glyph->bearing_y
    );

    pen_x += glyph->advance_x;
}
```

### Pre-caching Glyphs

For predictable performance, pre-cache commonly used characters:

```cpp
glyph_cache_config config;
config.atlas_width = 1024;
config.atlas_height = 1024;
config.pre_cache_ascii = true;  // Cache ASCII 32-126

glyph_cache<memory_atlas> cache(font_source, config);

// Or cache specific ranges
cache.precache_range(U'А', U'я');  // Cyrillic
cache.precache_range(U'0', U'9');  // Digits
```

---

## Gradient Text Rendering

The library renders text as grayscale alpha values, which you can colorize with gradients using custom render targets. This section shows how to create horizontal, vertical, and multi-stop gradient effects.

### Color Utilities

First, define a simple color structure with linear interpolation:

```cpp
#include <cstdint>
#include <algorithm>

struct color_rgba {
    uint8_t r, g, b, a;

    static color_rgba lerp(const color_rgba& a, const color_rgba& b, float t) {
        t = std::clamp(t, 0.0f, 1.0f);
        return {
            static_cast<uint8_t>(a.r + (b.r - a.r) * t),
            static_cast<uint8_t>(a.g + (b.g - a.g) * t),
            static_cast<uint8_t>(a.b + (b.b - a.b) * t),
            static_cast<uint8_t>(a.a + (b.a - a.a) * t)
        };
    }
};
```

### Horizontal Gradient Target

A render target that applies a left-to-right color gradient:

```cpp
class horizontal_gradient_target {
public:
    horizontal_gradient_target(uint32_t* rgba_buffer, int width, int height, int stride,
                               color_rgba color_left, color_rgba color_right)
        : m_buffer(rgba_buffer), m_width(width), m_height(height), m_stride(stride),
          m_left(color_left), m_right(color_right) {}

    void put_pixel(int x, int y, uint8_t alpha) {
        if (x < 0 || x >= m_width || y < 0 || y >= m_height || alpha == 0)
            return;

        // Calculate gradient position (0.0 = left, 1.0 = right)
        float t = static_cast<float>(x) / static_cast<float>(m_width - 1);
        color_rgba color = color_rgba::lerp(m_left, m_right, t);

        // Alpha blend onto buffer
        uint32_t* pixel = &m_buffer[y * m_stride + x];
        blend_pixel(pixel, color, alpha);
    }

    int width() const { return m_width; }
    int height() const { return m_height; }

private:
    void blend_pixel(uint32_t* dst, color_rgba src, uint8_t alpha) {
        // Porter-Duff "over" compositing
        uint8_t* d = reinterpret_cast<uint8_t*>(dst);
        uint8_t sa = (src.a * alpha) / 255;
        uint8_t inv_sa = 255 - sa;

        d[0] = (src.r * sa + d[0] * inv_sa) / 255;
        d[1] = (src.g * sa + d[1] * inv_sa) / 255;
        d[2] = (src.b * sa + d[2] * inv_sa) / 255;
        d[3] = sa + (d[3] * inv_sa) / 255;
    }

    uint32_t* m_buffer;
    int m_width, m_height, m_stride;
    color_rgba m_left, m_right;
};
```

### Vertical Gradient Target

A render target that applies a top-to-bottom color gradient:

```cpp
class vertical_gradient_target {
public:
    vertical_gradient_target(uint32_t* rgba_buffer, int width, int height, int stride,
                             color_rgba color_top, color_rgba color_bottom)
        : m_buffer(rgba_buffer), m_width(width), m_height(height), m_stride(stride),
          m_top(color_top), m_bottom(color_bottom) {}

    void put_pixel(int x, int y, uint8_t alpha) {
        if (x < 0 || x >= m_width || y < 0 || y >= m_height || alpha == 0)
            return;

        // Calculate gradient position (0.0 = top, 1.0 = bottom)
        float t = static_cast<float>(y) / static_cast<float>(m_height - 1);
        color_rgba color = color_rgba::lerp(m_top, m_bottom, t);

        uint32_t* pixel = &m_buffer[y * m_stride + x];
        blend_pixel(pixel, color, alpha);
    }

    int width() const { return m_width; }
    int height() const { return m_height; }

private:
    void blend_pixel(uint32_t* dst, color_rgba src, uint8_t alpha) {
        uint8_t* d = reinterpret_cast<uint8_t*>(dst);
        uint8_t sa = (src.a * alpha) / 255;
        uint8_t inv_sa = 255 - sa;

        d[0] = (src.r * sa + d[0] * inv_sa) / 255;
        d[1] = (src.g * sa + d[1] * inv_sa) / 255;
        d[2] = (src.b * sa + d[2] * inv_sa) / 255;
        d[3] = sa + (d[3] * inv_sa) / 255;
    }

    uint32_t* m_buffer;
    int m_width, m_height, m_stride;
    color_rgba m_top, m_bottom;
};
```

### Usage Example

```cpp
#include <onyx_font/font_factory.hh>
#include <onyx_font/text/font_source.hh>
#include <onyx_font/text/text_rasterizer.hh>

using namespace onyx_font;

// Load font
auto ttf = font_factory::load_ttf(font_data);
font_source font = font_source::from_ttf(ttf, 48.0f);
text_rasterizer rasterizer(font);

// Measure text
auto extents = rasterizer.measure_text("Gradient Text");

// Create RGBA buffer
std::vector<uint32_t> pixels(extents.width * extents.height, 0);

// Horizontal gradient: red -> blue
horizontal_gradient_target h_target(
    pixels.data(), extents.width, extents.height, extents.width,
    {255, 0, 0, 255},    // Left: red
    {0, 0, 255, 255}     // Right: blue
);
rasterizer.rasterize_text("Gradient Text", h_target, 0, font.get_metrics().ascent);

// Or vertical gradient: yellow -> green
vertical_gradient_target v_target(
    pixels.data(), extents.width, extents.height, extents.width,
    {255, 255, 0, 255},  // Top: yellow
    {0, 255, 0, 255}     // Bottom: green
);
rasterizer.rasterize_text("Gradient Text", v_target, 0, font.get_metrics().ascent);
```

### Quick Gradients with callback_target

For quick prototyping without creating a new class, use `callback_target`:

```cpp
#include <onyx_font/text/raster_target.hh>

int text_width = extents.width;
int text_height = extents.height;
color_rgba left{255, 0, 0, 255};
color_rgba right{0, 0, 255, 255};

callback_target target(
    [&](int x, int y, uint8_t alpha) {
        float t = static_cast<float>(x) / (text_width - 1);
        color_rgba color = color_rgba::lerp(left, right, t);

        // Apply to your framebuffer
        set_pixel_rgba(x, y, color.r, color.g, color.b, (color.a * alpha) / 255);
    },
    text_width, text_height
);

rasterizer.rasterize_text("Hello", target, 0, baseline_y);
```

### Multi-Stop Gradient Target

For complex gradients with multiple color stops (rainbow, etc.):

```cpp
#include <vector>
#include <algorithm>

struct gradient_stop {
    float position;  // 0.0 to 1.0
    color_rgba color;
};

class multi_stop_gradient_target {
public:
    multi_stop_gradient_target(uint32_t* buffer, int width, int height, int stride,
                               std::vector<gradient_stop> stops, bool horizontal = true)
        : m_buffer(buffer), m_width(width), m_height(height), m_stride(stride),
          m_stops(std::move(stops)), m_horizontal(horizontal) {
        // Sort stops by position
        std::sort(m_stops.begin(), m_stops.end(),
                  [](const auto& a, const auto& b) { return a.position < b.position; });
    }

    void put_pixel(int x, int y, uint8_t alpha) {
        if (x < 0 || x >= m_width || y < 0 || y >= m_height || alpha == 0)
            return;

        float t = m_horizontal
            ? static_cast<float>(x) / (m_width - 1)
            : static_cast<float>(y) / (m_height - 1);

        color_rgba color = sample_gradient(t);
        blend_pixel(&m_buffer[y * m_stride + x], color, alpha);
    }

    int width() const { return m_width; }
    int height() const { return m_height; }

private:
    color_rgba sample_gradient(float t) const {
        if (m_stops.empty()) return {255, 255, 255, 255};
        if (t <= m_stops.front().position) return m_stops.front().color;
        if (t >= m_stops.back().position) return m_stops.back().color;

        // Find surrounding stops and interpolate
        for (size_t i = 0; i < m_stops.size() - 1; ++i) {
            if (t >= m_stops[i].position && t <= m_stops[i + 1].position) {
                float local_t = (t - m_stops[i].position) /
                               (m_stops[i + 1].position - m_stops[i].position);
                return color_rgba::lerp(m_stops[i].color, m_stops[i + 1].color, local_t);
            }
        }
        return m_stops.back().color;
    }

    void blend_pixel(uint32_t* dst, color_rgba src, uint8_t alpha) {
        uint8_t* d = reinterpret_cast<uint8_t*>(dst);
        uint8_t sa = (src.a * alpha) / 255;
        uint8_t inv_sa = 255 - sa;

        d[0] = (src.r * sa + d[0] * inv_sa) / 255;
        d[1] = (src.g * sa + d[1] * inv_sa) / 255;
        d[2] = (src.b * sa + d[2] * inv_sa) / 255;
        d[3] = sa + (d[3] * inv_sa) / 255;
    }

    uint32_t* m_buffer;
    int m_width, m_height, m_stride;
    std::vector<gradient_stop> m_stops;
    bool m_horizontal;
};
```

Rainbow gradient example:

```cpp
multi_stop_gradient_target rainbow_target(
    pixels.data(), width, height, width,
    {
        {0.0f, {255, 0, 0, 255}},     // Red
        {0.2f, {255, 165, 0, 255}},   // Orange
        {0.4f, {255, 255, 0, 255}},   // Yellow
        {0.6f, {0, 255, 0, 255}},     // Green
        {0.8f, {0, 0, 255, 255}},     // Blue
        {1.0f, {128, 0, 128, 255}}    // Purple
    },
    true  // horizontal
);
```

### GPU Shader Approach

When using `glyph_cache` and `text_renderer` with GPU rendering, apply gradients in the fragment shader:

```glsl
// Fragment shader for gradient text
uniform sampler2D u_atlas;
uniform vec4 u_color_start;
uniform vec4 u_color_end;
uniform vec2 u_text_bounds;      // (width, height) of text
uniform bool u_horizontal;       // true = horizontal, false = vertical

in vec2 v_texcoord;
in vec2 v_position;              // Position relative to text origin

out vec4 frag_color;

void main() {
    // Sample alpha from atlas
    float alpha = texture(u_atlas, v_texcoord).r;

    // Calculate gradient parameter
    float t = u_horizontal
        ? v_position.x / u_text_bounds.x
        : v_position.y / u_text_bounds.y;
    t = clamp(t, 0.0, 1.0);

    // Interpolate color
    vec4 color = mix(u_color_start, u_color_end, t);
    color.a *= alpha;

    frag_color = color;
}
```

The key insight is that the library produces **alpha values**, which you combine with any color logic - gradients, textures, patterns, or procedural effects.

---

## SDL2 Integration

This section shows how to use SDL2's `SDL_Texture` as a glyph atlas for hardware-accelerated text rendering with `glyph_cache` and `text_renderer`.

### SDL Texture Atlas

Create an atlas surface that wraps an `SDL_Texture`:

```cpp
#include <SDL2/SDL.h>
#include <onyx_font/text/atlas_surface.hh>
#include <vector>
#include <cstdint>

class sdl_texture_atlas {
public:
    sdl_texture_atlas(SDL_Renderer* renderer, uint16_t width, uint16_t height)
        : m_renderer(renderer), m_width(width), m_height(height) {
        // Create streaming texture with alpha channel
        m_texture = SDL_CreateTexture(
            renderer,
            SDL_PIXELFORMAT_RGBA8888,
            SDL_TEXTUREACCESS_STREAMING,
            width, height
        );

        if (m_texture) {
            // Enable alpha blending
            SDL_SetTextureBlendMode(m_texture, SDL_BLENDMODE_BLEND);

            // Clear to transparent
            clear();
        }
    }

    ~sdl_texture_atlas() {
        if (m_texture) {
            SDL_DestroyTexture(m_texture);
        }
    }

    // Move-only
    sdl_texture_atlas(sdl_texture_atlas&& other) noexcept
        : m_renderer(other.m_renderer), m_texture(other.m_texture),
          m_width(other.m_width), m_height(other.m_height) {
        other.m_texture = nullptr;
    }

    sdl_texture_atlas& operator=(sdl_texture_atlas&& other) noexcept {
        if (this != &other) {
            if (m_texture) SDL_DestroyTexture(m_texture);
            m_renderer = other.m_renderer;
            m_texture = other.m_texture;
            m_width = other.m_width;
            m_height = other.m_height;
            other.m_texture = nullptr;
        }
        return *this;
    }

    sdl_texture_atlas(const sdl_texture_atlas&) = delete;
    sdl_texture_atlas& operator=(const sdl_texture_atlas&) = delete;

    // Required by atlas_surface concept
    uint16_t width() const { return m_width; }
    uint16_t height() const { return m_height; }

    void write_alpha(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                     std::span<const uint8_t> alpha_data) {
        if (!m_texture || alpha_data.empty()) return;

        // Lock the texture region for writing
        SDL_Rect rect = {x, y, w, h};
        void* pixels = nullptr;
        int pitch = 0;

        if (SDL_LockTexture(m_texture, &rect, &pixels, &pitch) != 0) {
            return;  // Lock failed
        }

        // Convert alpha to RGBA (white with varying alpha)
        uint8_t* dst = static_cast<uint8_t*>(pixels);
        size_t src_idx = 0;

        for (uint16_t row = 0; row < h; ++row) {
            uint32_t* row_pixels = reinterpret_cast<uint32_t*>(dst + row * pitch);
            for (uint16_t col = 0; col < w; ++col) {
                uint8_t alpha = alpha_data[src_idx++];
                // RGBA8888: 0xRRGGBBAA or 0xAABBGGRR depending on endianness
                // Use SDL_MapRGBA for portability
                row_pixels[col] = (255u << 24) | (255u << 16) | (255u << 8) | alpha;
            }
        }

        SDL_UnlockTexture(m_texture);
    }

    // Access the underlying texture for rendering
    SDL_Texture* texture() const { return m_texture; }
    SDL_Renderer* renderer() const { return m_renderer; }

    void clear() {
        if (!m_texture) return;

        void* pixels = nullptr;
        int pitch = 0;

        if (SDL_LockTexture(m_texture, nullptr, &pixels, &pitch) == 0) {
            std::memset(pixels, 0, pitch * m_height);
            SDL_UnlockTexture(m_texture);
        }
    }

private:
    SDL_Renderer* m_renderer = nullptr;
    SDL_Texture* m_texture = nullptr;
    uint16_t m_width = 0;
    uint16_t m_height = 0;
};
```

### Using with glyph_cache

Create a glyph cache that uses the SDL texture atlas:

```cpp
#include <onyx_font/text/glyph_cache.hh>
#include <onyx_font/text/text_renderer.hh>
#include <onyx_font/font_factory.hh>

using namespace onyx_font;

// Setup
SDL_Window* window = SDL_CreateWindow("Text Demo", ...);
SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

// Load font
std::vector<uint8_t> font_data = read_file("arial.ttf");
ttf_font ttf = font_factory::load_ttf(font_data);
font_source font = font_source::from_ttf(ttf, 24.0f);

// Create glyph cache with SDL atlas
glyph_cache_config config;
config.atlas_width = 512;
config.atlas_height = 512;
config.pre_cache_ascii = true;

glyph_cache<sdl_texture_atlas> cache(font, config, renderer, 512, 512);
```

### Rendering Text with text_renderer

Use `text_renderer` with a blit callback that copies from the atlas to the screen:

```cpp
text_renderer<sdl_texture_atlas> text_render(cache);

// Get atlas texture for rendering
SDL_Texture* atlas_texture = cache.atlas().texture();

// Define blit callback
auto blit = [&](int dst_x, int dst_y, int src_x, int src_y, int w, int h) {
    SDL_Rect src_rect = {src_x, src_y, w, h};
    SDL_Rect dst_rect = {dst_x, dst_y, w, h};
    SDL_RenderCopy(renderer, atlas_texture, &src_rect, &dst_rect);
};

// Render loop
while (running) {
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    // Draw text at position (100, 100)
    text_render.draw("Hello, SDL2!", 100, 100, blit);

    // Draw aligned text in a box
    text_box box{50, 200, 300, 100};
    text_render.draw_aligned("Centered", box, text_align::CENTER, text_valign::MIDDLE, blit);

    // Draw wrapped text
    text_render.draw_wrapped("This is a long text that will wrap to multiple lines.",
                             box, text_align::LEFT, blit);

    SDL_RenderPresent(renderer);
}
```

### Colored Text with SDL

Apply color tinting using SDL's texture color modulation:

```cpp
void draw_colored_text(text_renderer<sdl_texture_atlas>& renderer,
                       SDL_Renderer* sdl_renderer,
                       SDL_Texture* atlas,
                       const std::string& text,
                       int x, int y,
                       uint8_t r, uint8_t g, uint8_t b) {
    // Set color modulation
    SDL_SetTextureColorMod(atlas, r, g, b);

    auto blit = [&](int dst_x, int dst_y, int src_x, int src_y, int w, int h) {
        SDL_Rect src_rect = {src_x, src_y, w, h};
        SDL_Rect dst_rect = {x + dst_x, y + dst_y, w, h};
        SDL_RenderCopy(sdl_renderer, atlas, &src_rect, &dst_rect);
    };

    renderer.draw(text, 0, 0, blit);

    // Reset color modulation
    SDL_SetTextureColorMod(atlas, 255, 255, 255);
}

// Usage
draw_colored_text(text_render, renderer, atlas_texture, "Red Text", 100, 100, 255, 0, 0);
draw_colored_text(text_render, renderer, atlas_texture, "Green Text", 100, 130, 0, 255, 0);
draw_colored_text(text_render, renderer, atlas_texture, "Blue Text", 100, 160, 0, 0, 255);
```

### Complete SDL2 Example

Here's a complete working example:

```cpp
#include <SDL2/SDL.h>
#include <onyx_font/font_factory.hh>
#include <onyx_font/text/font_source.hh>
#include <onyx_font/text/glyph_cache.hh>
#include <onyx_font/text/text_renderer.hh>
#include <fstream>
#include <vector>

// Include sdl_texture_atlas class from above

std::vector<uint8_t> read_file(const char* path) {
    std::ifstream file(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

int main(int argc, char* argv[]) {
    using namespace onyx_font;

    // Initialize SDL
    SDL_Init(SDL_INIT_VIDEO);
    SDL_Window* window = SDL_CreateWindow(
        "onyx_font SDL2 Demo",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        800, 600,
        SDL_WINDOW_SHOWN
    );
    SDL_Renderer* renderer = SDL_CreateRenderer(
        window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );

    // Load font
    auto font_data = read_file("arial.ttf");
    auto ttf = font_factory::load_ttf(font_data);
    font_source font = font_source::from_ttf(ttf, 32.0f);

    // Create cache and renderer
    glyph_cache_config config;
    config.atlas_width = 512;
    config.atlas_height = 512;
    config.pre_cache_ascii = true;

    glyph_cache<sdl_texture_atlas> cache(font, config, renderer, 512, 512);
    text_renderer<sdl_texture_atlas> text_render(cache);
    SDL_Texture* atlas = cache.atlas().texture();

    // Main loop
    bool running = true;
    SDL_Event event;

    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) running = false;
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE)
                running = false;
        }

        // Clear screen
        SDL_SetRenderDrawColor(renderer, 40, 40, 40, 255);
        SDL_RenderClear(renderer);

        // Blit callback
        auto blit = [&](int dst_x, int dst_y, int src_x, int src_y, int w, int h) {
            SDL_Rect src = {src_x, src_y, w, h};
            SDL_Rect dst = {dst_x, dst_y, w, h};
            SDL_RenderCopy(renderer, atlas, &src, &dst);
        };

        // Draw white text
        SDL_SetTextureColorMod(atlas, 255, 255, 255);
        text_render.draw("Hello, SDL2!", 50, 50, blit);

        // Draw colored text
        SDL_SetTextureColorMod(atlas, 255, 100, 100);
        text_render.draw("Colored text is easy!", 50, 100, blit);

        // Draw in a box with alignment
        SDL_SetTextureColorMod(atlas, 100, 200, 255);
        text_box box{50, 200, 400, 150};
        text_render.draw_wrapped(
            "This text will automatically wrap when it reaches the edge of the box.",
            box, text_align::LEFT, blit
        );

        // Draw box outline for visualization
        SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255);
        SDL_Rect box_rect = {50, 200, 400, 150};
        SDL_RenderDrawRect(renderer, &box_rect);

        SDL_RenderPresent(renderer);
    }

    // Cleanup
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
```

### Alternative: SDL_Surface Atlas

For software rendering or when you need CPU access to the atlas:

```cpp
class sdl_surface_atlas {
public:
    sdl_surface_atlas(uint16_t width, uint16_t height)
        : m_width(width), m_height(height) {
        m_surface = SDL_CreateRGBSurfaceWithFormat(
            0, width, height, 32, SDL_PIXELFORMAT_RGBA8888
        );
        if (m_surface) {
            SDL_FillRect(m_surface, nullptr, 0);
        }
    }

    ~sdl_surface_atlas() {
        if (m_texture) SDL_DestroyTexture(m_texture);
        if (m_surface) SDL_FreeSurface(m_surface);
    }

    uint16_t width() const { return m_width; }
    uint16_t height() const { return m_height; }

    void write_alpha(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                     std::span<const uint8_t> alpha_data) {
        if (!m_surface) return;

        SDL_LockSurface(m_surface);

        size_t src_idx = 0;
        for (uint16_t row = 0; row < h; ++row) {
            uint32_t* dst = reinterpret_cast<uint32_t*>(
                static_cast<uint8_t*>(m_surface->pixels) +
                (y + row) * m_surface->pitch
            ) + x;

            for (uint16_t col = 0; col < w; ++col) {
                uint8_t alpha = alpha_data[src_idx++];
                dst[col] = SDL_MapRGBA(m_surface->format, 255, 255, 255, alpha);
            }
        }

        SDL_UnlockSurface(m_surface);
        m_dirty = true;
    }

    // Convert to texture for rendering (call once per frame if dirty)
    SDL_Texture* get_texture(SDL_Renderer* renderer) {
        if (m_dirty || !m_texture) {
            if (m_texture) SDL_DestroyTexture(m_texture);
            m_texture = SDL_CreateTextureFromSurface(renderer, m_surface);
            SDL_SetTextureBlendMode(m_texture, SDL_BLENDMODE_BLEND);
            m_dirty = false;
        }
        return m_texture;
    }

    SDL_Surface* surface() const { return m_surface; }

private:
    SDL_Surface* m_surface = nullptr;
    SDL_Texture* m_texture = nullptr;
    uint16_t m_width, m_height;
    bool m_dirty = false;
};
```

### Performance Tips for SDL2

1. **Use streaming textures** for frequently updated atlases
2. **Pre-cache common characters** to avoid runtime texture updates
3. **Batch draw calls** by drawing all text with the same color together
4. **Reuse the blit callback** lambda to avoid allocations
5. **Consider multiple atlases** for very large character sets (CJK fonts)

```cpp
// Pre-cache common character ranges
cache.precache_range(U' ', U'~');   // ASCII printable
cache.precache_range(U'0', U'9');   // Digits
cache.precache_range(U'А', U'я');   // Cyrillic (if needed)
```

---

## Font Conversion

### Converting to Bitmap

Convert vector or TTF fonts to bitmap fonts at a specific size:

```cpp
#include <onyx_font/font_converter.hh>

// Convert TTF to bitmap
conversion_options opts;
opts.pixel_height = 16;
opts.first_char = 32;
opts.last_char = 126;
opts.threshold = 128;        // For aliased rendering
opts.antialias = false;      // 1-bit output

bitmap_font bitmap = font_converter::from_ttf(ttf_font, opts);

// Convert vector font to bitmap
bitmap_font bitmap = font_converter::from_vector(vector_font, opts);
```

### Antialiasing and Thresholding

```cpp
// Antialiased rendering (grayscale source)
opts.antialias = true;
opts.threshold = 128;  // Values >= 128 become "on" pixels

// Hard edges (lower threshold = thicker glyphs)
opts.threshold = 64;

// Thin rendering (higher threshold = thinner glyphs)
opts.threshold = 192;
```

---

## UTF-8 Support

The library fully supports Unicode via UTF-8 encoding:

### UTF-8 Iteration

```cpp
#include <onyx_font/text/utf8.hh>

std::string text = u8"Hello, 世界! 🌍";

// Using utf8_view for range-based iteration
for (char32_t codepoint : utf8_view(text)) {
    process_character(codepoint);
}

// Manual iteration
utf8_iterator it(text.data(), text.data() + text.size());
utf8_iterator end(text.data() + text.size(), text.data() + text.size());

while (it != end) {
    char32_t cp = *it++;
    // ...
}

// Get codepoint count
size_t char_count = utf8_length(text);

// Decode single codepoint
auto [codepoint, bytes_consumed] = utf8_decode_one(text.data(), text.size());
```

### Checking Glyph Support

```cpp
// TTF fonts
if (ttf_font.has_glyph(U'世')) {
    // Character is supported
}

// Bitmap fonts (check character range)
if (ch >= font.get_first_char() && ch <= font.get_last_char()) {
    // Character might be supported
}
```

---

## Performance Considerations

### Memory Management

1. **Keep font data alive**: `ttf_font` stores a reference to the font data, not a copy
   ```cpp
   std::vector<uint8_t> font_data = read_file("font.ttf");
   ttf_font font(font_data);  // font_data must outlive font!
   ```

2. **Use views where possible**: `bitmap_view` is lightweight (just a pointer + dimensions)

3. **Reserve space in builders**:
   ```cpp
   bitmap_builder builder;
   builder.reserve_glyphs(256);
   builder.reserve_bytes(256 * 8 * 16);
   ```

### Rendering Performance

1. **Use glyph caching** for repeated text rendering:
   ```cpp
   glyph_cache<memory_atlas> cache(font, config);
   // Glyphs are rasterized once, then reused
   ```

2. **Use packed row access** for bitmap fonts:
   ```cpp
   auto row = glyph.row(y);  // Faster than pixel-by-pixel
   ```

3. **Implement `put_span`** in custom render targets:
   ```cpp
   void put_span(int x, int y, std::span<const uint8_t> alphas) {
       // Process multiple pixels at once
       std::memcpy(&buffer[y * stride + x], alphas.data(), alphas.size());
   }
   ```

4. **Pre-cache common characters**:
   ```cpp
   config.pre_cache_ascii = true;
   cache.precache_range(U'0', U'9');
   ```

### Threading Considerations

- `font_factory` methods are thread-safe (stateless)
- `bitmap_font`, `vector_font`, `ttf_font` are immutable after construction
- `glyph_cache` is NOT thread-safe (use one per thread or add synchronization)
- `text_rasterizer` can be used concurrently if targets don't overlap

---

## API Reference Summary

### Core Types

| Header | Classes | Purpose |
|--------|---------|---------|
| `bitmap_font.hh` | `bitmap_font`, `font_metrics`, `glyph_spacing` | Raster font storage |
| `vector_font.hh` | `vector_font`, `vector_glyph`, `stroke_command` | Stroke-based fonts |
| `ttf_font.hh` | `ttf_font`, `ttf_glyph_shape`, `ttf_vertex` | TrueType/OpenType fonts |
| `font_factory.hh` | `font_factory`, `container_info`, `font_entry` | Font loading |
| `font_converter.hh` | `font_converter`, `conversion_options` | Font conversion |

### Text Rendering

| Header | Classes | Purpose |
|--------|---------|---------|
| `text/font_source.hh` | `font_source` | Unified font abstraction |
| `text/text_rasterizer.hh` | `text_rasterizer` | Low-level text rendering |
| `text/text_renderer.hh` | `text_renderer` | High-level text rendering |
| `text/glyph_cache.hh` | `glyph_cache`, `cached_glyph` | Glyph caching with atlas |
| `text/raster_target.hh` | Various target types | Render target implementations |
| `text/types.hh` | `text_extents`, `text_box`, enums | Common types |
| `text/utf8.hh` | `utf8_view`, `utf8_iterator` | UTF-8 utilities |

### Utilities

| Header | Classes | Purpose |
|--------|---------|---------|
| `utils/bitmap_glyphs_storage.hh` | `bitmap_storage`, `bitmap_view`, `bitmap_builder` | Packed bitmap storage |
| `utils/stb_truetype_font.hh` | `stb_truetype_font`, `stb_glyph_bitmap` | Low-level TTF rasterization |

---

## Examples

### Example 1: Simple Console Text Renderer

```cpp
#include <onyx_font/font_factory.hh>
#include <onyx_font/text/font_source.hh>
#include <onyx_font/text/text_rasterizer.hh>
#include <onyx_font/text/raster_target.hh>
#include <iostream>
#include <fstream>
#include <vector>

void print_to_console(const std::vector<uint8_t>& pixels, int width, int height) {
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            uint8_t alpha = pixels[y * width + x];
            if (alpha > 200) std::cout << '#';
            else if (alpha > 128) std::cout << '+';
            else if (alpha > 64) std::cout << '.';
            else std::cout << ' ';
        }
        std::cout << '\n';
    }
}

int main() {
    using namespace onyx_font;

    // Load font
    std::ifstream file("arial.ttf", std::ios::binary);
    std::vector<uint8_t> data((std::istreambuf_iterator<char>(file)),
                               std::istreambuf_iterator<char>());

    auto ttf = font_factory::load_ttf(data);
    font_source font = font_source::from_ttf(ttf, 20.0f);
    text_rasterizer rasterizer(font);

    // Measure and render
    auto extents = rasterizer.measure_text("Hello!");
    std::vector<uint8_t> pixels(extents.width * extents.height, 0);
    owned_grayscale_target target(pixels.data(), extents.width, extents.height);

    rasterizer.rasterize_text("Hello!", target, 0, font.get_metrics().ascent);
    print_to_console(pixels, extents.width, extents.height);

    return 0;
}
```

### Example 2: Loading All Fonts from a .FON File

```cpp
#include <onyx_font/font_factory.hh>
#include <iostream>

int main() {
    using namespace onyx_font;

    auto data = read_file("vgasys.fon");
    auto info = font_factory::analyze(data);

    std::cout << "File format: " << font_factory::format_name(info.format) << "\n\n";

    for (size_t i = 0; i < info.fonts.size(); ++i) {
        const auto& entry = info.fonts[i];
        std::cout << "[" << i << "] " << entry.name << "\n";
        std::cout << "    Type: " << font_factory::type_name(entry.type) << "\n";
        std::cout << "    Size: " << entry.pixel_height << "px\n";
        std::cout << "    Weight: " << entry.weight << "\n";
        std::cout << "    Italic: " << (entry.italic ? "yes" : "no") << "\n\n";
    }

    // Load all bitmap fonts
    auto fonts = font_factory::load_all_bitmaps(data);
    std::cout << "Loaded " << fonts.size() << " bitmap fonts\n";

    return 0;
}
```

### Example 3: GPU Text Rendering with OpenGL

```cpp
#include <onyx_font/text/text_renderer.hh>
#include <onyx_font/text/glyph_cache.hh>

class gl_atlas {
public:
    gl_atlas(uint16_t w, uint16_t h) : m_width(w), m_height(h) {
        glGenTextures(1, &m_texture);
        glBindTexture(GL_TEXTURE_2D, m_texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, w, h, 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }

    uint16_t width() const { return m_width; }
    uint16_t height() const { return m_height; }

    void write_alpha(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                     std::span<const uint8_t> data) {
        glBindTexture(GL_TEXTURE_2D, m_texture);
        glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, w, h, GL_RED, GL_UNSIGNED_BYTE, data.data());
    }

    GLuint texture() const { return m_texture; }

private:
    GLuint m_texture;
    uint16_t m_width, m_height;
};

void render_text_opengl(const std::string& text, float x, float y) {
    using namespace onyx_font;

    static auto ttf_data = read_file("font.ttf");
    static auto ttf = font_factory::load_ttf(ttf_data);
    static font_source font = font_source::from_ttf(ttf, 24.0f);
    static glyph_cache<gl_atlas> cache(font, {1024, 1024});
    static text_renderer<gl_atlas> renderer(cache);

    auto blit = [&](int dst_x, int dst_y, int src_x, int src_y, int w, int h) {
        // Draw textured quad from atlas to screen
        float u0 = src_x / 1024.0f, v0 = src_y / 1024.0f;
        float u1 = (src_x + w) / 1024.0f, v1 = (src_y + h) / 1024.0f;
        draw_quad(x + dst_x, y + dst_y, w, h, u0, v0, u1, v1);
    };

    glBindTexture(GL_TEXTURE_2D, cache.atlas().texture());
    renderer.draw(text, 0, 0, blit);
}
```

---

## Troubleshooting

### Common Issues

**Font fails to load**
- Check file format with `font_factory::analyze()`
- Ensure file is not corrupted
- For TTF: verify the file is a valid TrueType font

**Glyphs appear corrupted**
- Check bit order (`msb_first` vs `lsb_first`)
- Verify stride/pitch values
- Ensure font data remains valid for TTF fonts

**Text appears at wrong position**
- Remember Y=0 is top, not baseline
- Add `ascent` to Y for baseline positioning
- Check for negative A-space/C-space values

**Poor rendering quality**
- Use antialiasing for TTF fonts
- Try different threshold values for bitmap conversion
- Use subpixel positioning for small sizes

**Memory issues**
- Font data must outlive `ttf_font` objects
- Use `bitmap_view` instead of copying glyph data
- Consider atlas size vs. number of cached glyphs

---

## License

See the LICENSE file in the repository root for licensing information.

---

*This guide covers onyx_font version 1.0.0. For the latest documentation, see the Doxygen-generated API reference.*
