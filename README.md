# onyx_font

A modern C++20 library for loading, manipulating, and rendering fonts from multiple formats including Windows FON/FNT, TrueType, OpenType, and Borland BGI files.

---

## Features

- **Multi-format support** - Load fonts from TTF, OTF, TTC, Windows FON/FNT, and Borland BGI files
- **Automatic format detection** - Analyze font containers and enumerate embedded fonts
- **High-quality rendering** - Antialiased text rasterization with subpixel positioning
- **GPU-friendly architecture** - Texture atlas with glyph caching for hardware-accelerated rendering
- **Full Unicode support** - UTF-8 text handling with complete codepoint iteration
- **Text layout** - Measurement, alignment, and word wrapping
- **Font conversion** - Convert vector and TrueType fonts to bitmap format
- **Header-only dependencies** - All dependencies are fetched automatically via CMake

## Supported Formats

| Format | Extension | Description |
|--------|-----------|-------------|
| TrueType | .ttf | Scalable outline font |
| OpenType | .otf | Scalable outline font with advanced features |
| TrueType Collection | .ttc | Multiple fonts in a single file |
| Windows NE FON | .fon | 16-bit Windows executable with font resources |
| Windows PE FON | .fon | 32/64-bit Windows executable with font resources |
| Windows FNT | .fnt | Raw Windows font resource |
| Borland BGI | .chr | Borland Graphics Interface stroke font |
| Raw BIOS | .bin | VGA/EGA font dumps (8x8, 8x14, 8x16) |

## Requirements

- C++20 compatible compiler (GCC 10+, Clang 12+, MSVC 19.29+)
- CMake 3.20 or higher

## Installation

### Using CMake FetchContent (Recommended)

```cmake
include(FetchContent)

FetchContent_Declare(
    onyx_font
    GIT_REPOSITORY https://github.com/user/onyx_font.git
    GIT_TAG        v1.0.0
)

FetchContent_MakeAvailable(onyx_font)

target_link_libraries(your_target PRIVATE onyx_font::onyx_font)
```

### Building from Source

```bash
git clone https://github.com/user/onyx_font.git
cd onyx_font
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .
```

## Quick Start

```cpp
#include <onyx_font/font_factory.hh>
#include <onyx_font/text/font_source.hh>
#include <onyx_font/text/text_rasterizer.hh>
#include <onyx_font/text/raster_target.hh>

using namespace onyx_font;

// Load a TrueType font
std::vector<uint8_t> font_data = read_file("arial.ttf");
ttf_font ttf = font_factory::load_ttf(font_data);

// Create a font source at 24px
font_source font = font_source::from_ttf(ttf, 24.0f);

// Measure text
text_rasterizer rasterizer(font);
text_extents extents = rasterizer.measure_text("Hello, World!");

// Render to a grayscale buffer
std::vector<uint8_t> pixels(extents.width * extents.height, 0);
owned_grayscale_target target(pixels.data(), extents.width, extents.height);

rasterizer.rasterize_text("Hello, World!", target, 0, font.get_metrics().ascent);
```

### Analyzing Font Files

```cpp
#include <onyx_font/font_factory.hh>

auto data = read_file("system.fon");
container_info info = font_factory::analyze(data);

std::cout << "Format: " << font_factory::format_name(info.format) << "\n";
std::cout << "Contains " << info.fonts.size() << " fonts\n";

for (const auto& entry : info.fonts) {
    std::cout << "  " << entry.name
              << " (" << font_factory::type_name(entry.type) << ")"
              << " " << entry.pixel_height << "px\n";
}
```

### GPU Text Rendering with Glyph Cache

```cpp
#include <onyx_font/text/glyph_cache.hh>
#include <onyx_font/text/text_renderer.hh>

// Create glyph cache with texture atlas
glyph_cache_config config;
config.atlas_width = 512;
config.atlas_height = 512;
config.pre_cache_ascii = true;

glyph_cache<my_gpu_atlas> cache(font, config);
text_renderer<my_gpu_atlas> renderer(cache);

// Render with custom blit callback
auto blit = [&](int dst_x, int dst_y, int src_x, int src_y, int w, int h) {
    draw_textured_quad(dst_x, dst_y, src_x, src_y, w, h, cache.atlas().texture());
};

renderer.draw("Hello, GPU!", 100, 100, blit);
```

## CMake Options

| Option | Default | Description |
|--------|---------|-------------|
| `ONYX_FONT_BUILD_TESTS` | ON* | Build unit tests |
| `ONYX_FONT_BUILD_EXAMPLES` | ON* | Build example applications |
| `ONYX_FONT_BUILD_DEMOS` | ON* | Build SDL/ImGui demos |
| `ONYX_FONT_BUILD_DOCS` | OFF | Build Doxygen documentation |
| `ONYX_FONT_BUILD_SHARED` | ON | Build shared library (OFF for static) |

*Enabled by default when building as the main project, disabled when included via FetchContent.

## Documentation

- **[Programmer's Guide](docs/programmers_guide.md)** - Comprehensive usage guide with examples
- **API Reference** - Generated with Doxygen (build with `-DONYX_FONT_BUILD_DOCS=ON`)

### Building Documentation

```bash
cmake .. -DONYX_FONT_BUILD_DOCS=ON
cmake --build . --target docs
```

## Project Structure

```
onyx_font/
├── include/onyx_font/       # Public headers
│   ├── bitmap_font.hh       # Raster font class
│   ├── vector_font.hh       # Stroke-based font class
│   ├── ttf_font.hh          # TrueType/OpenType font class
│   ├── font_factory.hh      # Font loading and format detection
│   ├── font_converter.hh    # Font conversion utilities
│   ├── text/                # Text rendering subsystem
│   │   ├── font_source.hh   # Unified font abstraction
│   │   ├── text_rasterizer.hh
│   │   ├── text_renderer.hh
│   │   ├── glyph_cache.hh   # Atlas-based caching
│   │   └── ...
│   └── utils/               # Utility classes
├── src/                     # Implementation
├── examples/                # Example applications
├── unittest/                # Unit tests
└── docs/                    # Documentation
```

## Examples

The `examples/` directory contains sample applications:

- **font_info** - Analyze and list fonts in a container file
- **render_text** - Basic text rendering to image
- **sdl_demo** - Interactive SDL2 demo with text rendering

Build examples with:

```bash
cmake .. -DONYX_FONT_BUILD_EXAMPLES=ON
cmake --build .
```

## Testing

```bash
cmake .. -DONYX_FONT_BUILD_TESTS=ON
cmake --build .
ctest --output-on-failure
```

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## Contributing

Contributions are welcome. Please ensure that:

1. Code follows the existing style (C++20, no exceptions in hot paths)
2. New features include appropriate tests
3. Public APIs are documented with Doxygen comments
4. Commits are atomic and have descriptive messages

## Acknowledgments

This library uses the following open-source components:

- [stb_truetype](https://github.com/nothings/stb) - TrueType font rasterization
- [mzexplode](https://github.com/devbrain/mz-explode) - NE/PE/LX executable parsing
- [datascript](https://github.com/devbrain/datascript) - Binary data structure parsing
- [euler](https://github.com/devbrain/euler) - Line rasterization algorithms
- [failsafe](https://github.com/devbrain/failsafe) - Error handling utilities

---

*onyx_font is part of the Ares project.*
