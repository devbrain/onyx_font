# onyx_font

[![CI](https://github.com/devbrain/onyx_font/actions/workflows/ci.yml/badge.svg)](https://github.com/devbrain/onyx_font/actions/workflows/ci.yml)

A modern C++20 library for loading, manipulating, and rendering fonts from multiple formats including Windows FON/FNT, TrueType, OpenType, and Borland BGI files.

---

## Features

- **Multi-format support** - Load fonts from TTF, OTF, TTC, Windows FON/FNT, GEM, and Borland BGI files
- **Extensible architecture** - Add custom format decoders without modifying library source
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
| GEM | .fnt, .gft | GEM/Atari ST bitmap font |
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

### Using the Font Registry

The `font_registry` provides direct access to format decoders:

```cpp
#include <onyx_font/font_registry.hh>

auto& registry = onyx_font::font_registry::instance();

// Find decoder by sniffing file data
if (auto* decoder = registry.find_bitmap_decoder(file_data)) {
    auto entries = decoder->enumerate(file_data);
    auto font = decoder->load(file_data, 0);
}

// Find decoder by name
if (auto* decoder = registry.find_vector_decoder("bgi")) {
    auto font = decoder->load(bgi_data);
}

// List all registered decoders
for (std::size_t i = 0; i < registry.bitmap_decoder_count(); ++i) {
    std::cout << registry.bitmap_decoder_at(i)->name() << "\n";
}
```

### Adding Custom Format Support

Extend the library with custom font format decoders without modifying library source:

```cpp
#include <onyx_font/decoder.hh>
#include <onyx_font/font_registry.hh>

// Implement a custom bitmap font decoder (e.g., Linux PSF format)
class psf_decoder : public onyx_font::bitmap_font_decoder {
public:
    std::string_view name() const noexcept override {
        return "psf";
    }

    std::span<const std::string_view> extensions() const noexcept override {
        static constexpr std::string_view ext[] = {".psf", ".psfu"};
        return ext;
    }

    bool sniff(std::span<const std::uint8_t> data) const noexcept override {
        if (data.size() < 4) return false;
        // PSF2 magic: 0x72 0xb5 0x4a 0x86
        return data[0] == 0x72 && data[1] == 0xb5 &&
               data[2] == 0x4a && data[3] == 0x86;
    }

    std::vector<onyx_font::font_entry> enumerate(
        std::span<const std::uint8_t> data) const override {
        // Parse PSF header and return font metadata
        // ...
    }

    onyx_font::bitmap_font load(
        std::span<const std::uint8_t> data,
        std::size_t index) const override {
        // Parse and return the bitmap font
        // ...
    }
};

// Register at program startup
void init_custom_fonts() {
    onyx_font::font_registry::instance()
        .register_decoder(std::make_unique<psf_decoder>());
}
```

Three decoder base classes are available:
- `bitmap_font_decoder` - For raster formats (PSF, BDF, PCF, etc.)
- `vector_font_decoder` - For stroke-based formats (Hershey, etc.)
- `outline_font_decoder` - For outline formats (WOFF, Type1, etc.)

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
│   ├── font_registry.hh     # Decoder registry (extension API)
│   ├── decoder.hh           # Decoder base classes
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

- [FreeType](https://freetype.org/) - TrueType/OpenType font parsing and rasterization
- [mzexplode](https://github.com/devbrain/mz-explode) - NE/PE/LX executable parsing
- [datascript](https://github.com/devbrain/datascript) - Binary data structure parsing
- [euler](https://github.com/devbrain/euler) - Line rasterization algorithms
- [failsafe](https://github.com/devbrain/failsafe) - Error handling utilities

---


