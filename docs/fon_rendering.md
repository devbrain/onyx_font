# Rendering Microsoft .FON Bitmap Fonts

This document describes how to render glyphs from Windows .FON/.FNT bitmap font files, based on the FreeType 2.14.1 implementation analysis.

## Overview

Windows .FON files are bitmap fonts stored either as:
1. **Standalone .FNT files** - Raw FNT binary data
2. **NE (New Executable) resources** - FNT embedded in 16-bit Windows executables
3. **PE (Portable Executable) resources** - FNT embedded in 32-bit Windows executables

The actual glyph data is stored in **column-major format** (not row-major), which requires transposition during rendering.

## FNT Header Structure

The FNT header contains all metadata needed for rendering:

```cpp
struct FNT_Header {
    uint16_t version;              // 0x0200 (v2) or 0x0300 (v3)
    uint32_t file_size;            // Total size of FNT data
    uint8_t  copyright[60];        // Copyright string
    uint16_t file_type;            // Bit 0: 0=bitmap, 1=vector (reject if vector)
    uint16_t nominal_point_size;   // Design point size
    uint16_t vertical_resolution;  // Vertical DPI (default 72 if 0)
    uint16_t horizontal_resolution;// Horizontal DPI (default 72 if 0)
    uint16_t ascent;               // Pixels from baseline to top
    uint16_t internal_leading;     // Internal leading
    uint16_t external_leading;     // External leading
    uint8_t  italic;               // Boolean: italic flag
    uint8_t  underline;            // Boolean: underline flag
    uint8_t  strike_out;           // Boolean: strikeout flag
    uint16_t weight;               // Font weight (400=normal, 700=bold)
    uint8_t  charset;              // Character set ID (see below)
    uint16_t pixel_width;          // Width in pixels (0 for variable width)
    uint16_t pixel_height;         // Height in pixels (ALL glyphs)
    uint8_t  pitch_and_family;     // Pitch and family flags
    uint16_t avg_width;            // Average character width
    uint16_t max_width;            // Maximum character width
    uint8_t  first_char;           // First character code in font
    uint8_t  last_char;            // Last character code in font
    uint8_t  default_char;         // Default/missing glyph character
    uint8_t  break_char;           // Word break character
    uint16_t bytes_per_row;        // Not used for bitmap fonts
    uint32_t device_offset;        // Device name offset (unused)
    uint32_t face_name_offset;     // Family name offset in file
    uint32_t bits_pointer;         // Pointer to bitmap data (unused)
    uint32_t bits_offset;          // Offset to bitmap data
    uint8_t  reserved;             // Reserved byte

    // Version 0x300 only (additional 30 bytes):
    uint32_t flags;                // Optional flags
    uint16_t A_space;              // A width spacing
    uint16_t B_space;              // B width spacing
    uint16_t C_space;              // C width spacing
    uint16_t color_table_offset;   // Color table offset
    uint32_t reserved1[4];         // Reserved fields
};
```

**Header sizes:**
- Version 0x200: 118 bytes
- Version 0x300: 148 bytes

## Glyph Table Structure

Immediately following the header is the glyph table. Each entry describes one glyph:

### Version 0x200 (4 bytes per entry):
```cpp
struct GlyphEntry_v2 {
    uint16_t width;    // Glyph width in pixels
    uint16_t offset;   // Offset to bitmap data (from file start)
};
```

### Version 0x300 (6 bytes per entry):
```cpp
struct GlyphEntry_v3 {
    uint16_t width;    // Glyph width in pixels
    uint32_t offset;   // Offset to bitmap data (from file start)
};
```

**Number of glyphs:** `last_char - first_char + 1`

## Character to Glyph Index Mapping

```cpp
uint32_t char_to_glyph_index(uint8_t char_code, const FNT_Header& header) {
    // Check if character is in range
    if (char_code < header.first_char || char_code > header.last_char) {
        return 0;  // Use default glyph (index 0 = .notdef)
    }

    // Glyph indices start at 1 (0 is reserved for .notdef)
    return (char_code - header.first_char) + 1;
}
```

**Important:** Glyph index 0 is reserved for the `.notdef` glyph, which uses `header.default_char` internally.

## Reading Glyph Entry

```cpp
struct GlyphInfo {
    uint16_t width;      // Bitmap width in pixels
    uint32_t offset;     // Offset to bitmap data
};

GlyphInfo read_glyph_entry(const uint8_t* fnt_data, uint32_t glyph_index,
                           uint16_t version) {
    GlyphInfo info;

    // Calculate header size and entry size based on version
    uint32_t header_size = (version == 0x300) ? 148 : 118;
    uint32_t entry_size = (version == 0x300) ? 6 : 4;

    // Handle .notdef glyph (index 0)
    if (glyph_index == 0) {
        // Use default_char from header
        glyph_index = fnt_data[/* offset to default_char */];
    } else {
        glyph_index--;  // Revert to 0-based index
    }

    // Calculate offset to glyph entry
    uint32_t entry_offset = header_size + (glyph_index * entry_size);
    const uint8_t* p = fnt_data + entry_offset;

    // Read width (always 2 bytes, little-endian)
    info.width = p[0] | (p[1] << 8);

    // Read offset (2 or 4 bytes depending on version)
    if (version == 0x300) {
        info.offset = p[2] | (p[3] << 8) | (p[4] << 16) | (p[5] << 24);
    } else {
        info.offset = p[2] | (p[3] << 8);
    }

    return info;
}
```

## Bitmap Data Format (Column-Major)

**Critical:** FNT bitmap data is stored in **column-major** format, not row-major!

For a glyph with:
- Width: W pixels
- Height: H pixels (always `header.pixel_height`)

The bitmap data is organized as:
- **Pitch (bytes per column):** `ceil(W / 8)` columns, each H bytes tall
- **Total size:** `pitch * H` bytes

### Column-Major Storage Layout

```
For a glyph 8 pixels wide, 16 pixels tall:

Byte layout in file:
  Byte 0:   Column 0, Row 0-7   (8 pixels vertically)
  Byte 1:   Column 0, Row 8-15  (8 pixels vertically)
  Byte 2:   Column 1, Row 0-7
  Byte 3:   Column 1, Row 8-15
  ... and so on

Bit order within each byte:
  Bit 7 (MSB) = topmost pixel of the 8
  Bit 0 (LSB) = bottommost pixel of the 8
```

## Bitmap Extraction Algorithm

Convert from column-major (file format) to row-major (standard bitmap format):

```cpp
struct Bitmap {
    std::vector<uint8_t> buffer;
    uint32_t width;       // Pixels
    uint32_t height;      // Pixels
    uint32_t pitch;       // Bytes per row
};

Bitmap extract_glyph_bitmap(const uint8_t* fnt_data,
                            const GlyphInfo& glyph,
                            uint16_t pixel_height) {
    Bitmap bitmap;
    bitmap.width = glyph.width;
    bitmap.height = pixel_height;
    bitmap.pitch = (glyph.width + 7) / 8;  // Round up to byte boundary

    // Allocate output buffer (row-major format)
    bitmap.buffer.resize(bitmap.pitch * bitmap.height, 0);

    // Source pointer (column-major data)
    const uint8_t* src = fnt_data + glyph.offset;

    // Convert column-major to row-major
    uint8_t* column = bitmap.buffer.data();

    for (uint32_t col = 0; col < bitmap.pitch; col++, column++) {
        const uint8_t* src_col = src;
        const uint8_t* src_limit = src + bitmap.height;

        for (uint8_t* dst = column; src < src_limit; src++, dst += bitmap.pitch) {
            *dst = *src;
        }

        src = src_limit;  // Move to next column in source
    }

    return bitmap;
}
```

### Visual Explanation of Transposition

```
Source (column-major):          Output (row-major):
+---+---+---+                   +---+---+---+
| A | C | E |  Column 0         | A | B |     Row 0
+---+---+---+                   +---+---+---+
| B | D | F |  Column 1         | C | D |     Row 1
+---+---+---+                   +---+---+---+
                                | E | F |     Row 2
File order: A,B,C,D,E,F         +---+---+---+
```

## Glyph Metrics

```cpp
struct GlyphMetrics {
    int32_t width;           // Bitmap width in pixels
    int32_t height;          // Bitmap height in pixels
    int32_t bitmap_left;     // X offset from origin (always 0 for FNT)
    int32_t bitmap_top;      // Y offset from baseline (= ascent)
    int32_t advance_x;       // Horizontal advance (= width for FNT)
    int32_t advance_y;       // Vertical advance (0 for horizontal text)
};

GlyphMetrics calculate_metrics(const GlyphInfo& glyph,
                               const FNT_Header& header) {
    GlyphMetrics m;

    m.width = glyph.width;
    m.height = header.pixel_height;
    m.bitmap_left = 0;
    m.bitmap_top = header.ascent;
    m.advance_x = glyph.width;
    m.advance_y = 0;

    return m;
}
```

## Font-Level Metrics

```cpp
struct FontMetrics {
    int32_t ascender;        // Pixels above baseline
    int32_t descender;       // Pixels below baseline (negative)
    int32_t line_height;     // Total line height
    int32_t max_advance;     // Maximum glyph width
};

FontMetrics calculate_font_metrics(const FNT_Header& header) {
    FontMetrics m;

    m.ascender = header.ascent;
    m.descender = -(header.pixel_height - header.ascent);
    m.line_height = header.pixel_height + header.external_leading;
    m.max_advance = header.max_width;

    return m;
}
```

## Glyph Positioning with bitmap_top

The `bitmap_top` metric (equal to `header.ascent` for FNT fonts) represents the distance from the **baseline** to the **top edge of the glyph bitmap** in pixels. This is essential for correctly positioning glyphs when rendering text.

### Baseline Concept

```
                    ┌─────────────┐
                    │             │  ↑
                    │   Glyph     │  │ bitmap_top (= ascent)
                    │   Bitmap    │  │
  baseline_y ───────┼─────────────┼──┴─────────────────
                    │  descender  │  ↑
                    │    part     │  │ (pixel_height - ascent)
                    └─────────────┘  ↓

  glyph_y (top-left of bitmap) = baseline_y - bitmap_top
```

### Positioning Formula

When you have a baseline Y coordinate, position the glyph bitmap at:

```cpp
int glyph_screen_y = baseline_y - bitmap_top;
int glyph_screen_x = cursor_x + bitmap_left;  // bitmap_left is always 0 for FNT
```

### Rendering Text at a Baseline

```cpp
void render_text_at_baseline(Surface& surface,
                             const char* text,
                             int start_x,
                             int baseline_y,
                             const FNT_Header& header) {
    int cursor_x = start_x;

    // For FNT fonts, all glyphs share the same vertical positioning
    int glyph_top_y = baseline_y - header.ascent;

    for (const char* p = text; *p; p++) {
        // Get glyph bitmap
        Bitmap glyph = render_char(*p);

        // Blit at calculated position
        // X: cursor position (bitmap_left is always 0)
        // Y: baseline minus ascent
        blit_bitmap(surface, glyph, cursor_x, glyph_top_y);

        // Advance cursor horizontally
        cursor_x += glyph.width;
    }
}
```

### Multi-Line Text Rendering

```cpp
void render_multiline_text(Surface& surface,
                           const std::vector<std::string>& lines,
                           int start_x,
                           int start_y,
                           const FNT_Header& header) {
    // Calculate line height
    int line_height = header.pixel_height + header.external_leading;

    // First baseline position
    // If start_y is the top of the text area, baseline is at:
    int baseline_y = start_y + header.ascent;

    for (const auto& line : lines) {
        render_text_at_baseline(surface, line.c_str(), start_x, baseline_y, header);

        // Move to next line
        baseline_y += line_height;
    }
}
```

### Alternative: Top-Left Origin Positioning

If your coordinate system uses top-left as the origin (common in graphics APIs):

```cpp
void render_text_topleft(Surface& surface,
                         const char* text,
                         int x,           // Left edge of text
                         int y,           // TOP edge of text (not baseline!)
                         const FNT_Header& header) {
    int cursor_x = x;

    // When y is the top edge, glyphs are placed directly at y
    // (no baseline adjustment needed since y IS the top)

    for (const char* p = text; *p; p++) {
        Bitmap glyph = render_char(*p);
        blit_bitmap(surface, glyph, cursor_x, y);
        cursor_x += glyph.width;
    }
}
```

### Why bitmap_top Matters

For FNT fonts, `bitmap_top` equals `ascent` for all glyphs. However, understanding this metric is important because:

1. **Baseline alignment** - Multiple fonts of different sizes align on the baseline
2. **Mixed content** - Text mixed with images or other elements needs baseline reference
3. **Vertical centering** - Proper centering requires knowing where the baseline sits within the glyph height

```
Font A (ascent=10, descent=4):    Font B (ascent=8, descent=2):

    ████                              ████
    █  █                              █  █
    ████      ← baseline →            ████
    █
    █
```

Both fonts align on the same baseline despite different heights.

## Character Set / Encoding

The `charset` field indicates the character encoding:

| Value | Name | Description |
|-------|------|-------------|
| 0 | CP1252 | ANSI (Western European) |
| 1 | DEFAULT | Unspecified |
| 2 | SYMBOL | Symbol font |
| 77 | MAC | Macintosh Roman |
| 128 | CP932 | Japanese Shift-JIS |
| 129 | CP949 | Korean |
| 130 | CP1361 | Korean (Johab) |
| 134 | CP936 | Simplified Chinese |
| 136 | CP950 | Traditional Chinese |
| 161 | CP1253 | Greek |
| 162 | CP1254 | Turkish |
| 163 | CP1258 | Vietnamese |
| 177 | CP1255 | Hebrew |
| 178 | CP1256 | Arabic |
| 186 | CP1257 | Baltic |
| 204 | CP1251 | Cyrillic |
| 222 | CP874 | Thai |
| 238 | CP1250 | Central European |
| 255 | OEM | OEM code page (system-dependent) |

## Complete Rendering Example

```cpp
#include <vector>
#include <cstdint>
#include <stdexcept>

class FNTRenderer {
public:
    FNTRenderer(const uint8_t* data, size_t size)
        : fnt_data_(data), fnt_size_(size) {
        parse_header();
    }

    // Render a single character to a bitmap
    Bitmap render_char(uint8_t char_code) {
        // 1. Map character to glyph index
        uint32_t glyph_idx = char_to_glyph_index(char_code);

        // 2. Read glyph entry
        GlyphInfo glyph = read_glyph_entry(glyph_idx);

        // 3. Extract and transpose bitmap
        return extract_glyph_bitmap(glyph);
    }

    // Render a string to a bitmap
    Bitmap render_string(const char* text) {
        // Calculate total width
        uint32_t total_width = 0;
        for (const char* p = text; *p; p++) {
            uint32_t idx = char_to_glyph_index(*p);
            GlyphInfo glyph = read_glyph_entry(idx);
            total_width += glyph.width;
        }

        // Create output bitmap
        Bitmap result;
        result.width = total_width;
        result.height = header_.pixel_height;
        result.pitch = (total_width + 7) / 8;
        result.buffer.resize(result.pitch * result.height, 0);

        // Render each character
        uint32_t x_offset = 0;
        for (const char* p = text; *p; p++) {
            Bitmap glyph_bmp = render_char(*p);
            blit_glyph(result, glyph_bmp, x_offset, 0);
            x_offset += glyph_bmp.width;
        }

        return result;
    }

private:
    const uint8_t* fnt_data_;
    size_t fnt_size_;
    FNT_Header header_;

    void parse_header() {
        // Read and validate header
        // (implementation as shown above)
    }

    uint32_t char_to_glyph_index(uint8_t char_code) {
        if (char_code < header_.first_char ||
            char_code > header_.last_char) {
            return 0;  // .notdef
        }
        return (char_code - header_.first_char) + 1;
    }

    GlyphInfo read_glyph_entry(uint32_t glyph_index) {
        // (implementation as shown above)
    }

    Bitmap extract_glyph_bitmap(const GlyphInfo& glyph) {
        // (implementation as shown above)
    }

    void blit_glyph(Bitmap& dst, const Bitmap& src,
                    uint32_t x, uint32_t y) {
        // Bit-level blitting for monochrome bitmaps
        for (uint32_t row = 0; row < src.height; row++) {
            for (uint32_t col = 0; col < src.width; col++) {
                // Get source pixel
                uint32_t src_byte = col / 8;
                uint32_t src_bit = 7 - (col % 8);
                bool pixel = (src.buffer[row * src.pitch + src_byte] >> src_bit) & 1;

                if (pixel) {
                    // Set destination pixel
                    uint32_t dst_col = x + col;
                    uint32_t dst_byte = dst_col / 8;
                    uint32_t dst_bit = 7 - (dst_col % 8);
                    dst.buffer[(y + row) * dst.pitch + dst_byte] |= (1 << dst_bit);
                }
            }
        }
    }
};
```

## Font Type Flags (dfType field)

The `dfType` field is a 16-bit value where different bits indicate font characteristics:

```cpp
// Bit definitions for dfType (low byte used by GDI)
#define FNT_TYPE_RASTER             0x0000  // Bit 0 clear = raster/bitmap font
#define FNT_TYPE_VECTOR             0x0001  // Bit 0 set = vector/stroke font
#define FNT_TYPE_RESERVED           0x0002  // Bit 1 reserved (must be 0)
#define FNT_TYPE_MEMORY             0x0004  // Bit 2 = bits at fixed memory address
#define FNT_TYPE_DEVICE             0x0080  // Bit 7 = font realized by device
```

### FNT_TYPE_RASTER (0x0000) and FNT_TYPE_VECTOR (0x0001)

These are mutually exclusive - bit 0 determines if the font contains bitmap glyphs or vector stroke data:

```cpp
bool is_vector_font(uint16_t type) {
    return (type & 0x0001) != 0;
}

bool is_raster_font(uint16_t type) {
    return (type & 0x0001) == 0;
}
```

### FNT_TYPE_MEMORY (0x0004) - ROM/Fixed Address Fonts

When bit 2 is set, the glyph bitmap data is **not stored in the font file**. Instead:
- The `dfBitsOffset` field contains an **absolute memory address** (not a file offset)
- The bitmap data is expected to be located at that fixed address (typically in ROM)
- This was used for hardware fonts burned into ROM chips

```cpp
bool is_memory_font(uint16_t type) {
    return (type & 0x0004) != 0;
}

// For memory fonts:
// - dfBitsOffset = absolute memory address where bitmaps reside
// - Glyph offsets in the table are relative to this address
// - No bitmap data follows in the file
```

**Practical implications:**
- These fonts cannot be rendered from the file alone
- They require the ROM hardware to be present
- Modern software should reject or skip these fonts
- Extremely rare in practice

### FNT_TYPE_DEVICE (0x0080) - Device-Realized Fonts

When bit 7 is set, the font was "realized by a device" (e.g., a printer or display adapter):
- The font is device-specific and may not be usable by GDI directly
- The high byte of `dfType` can be used by the device to describe itself
- GDI never inspects the high byte for device fonts

```cpp
bool is_device_font(uint16_t type) {
    return (type & 0x0080) != 0;
}

// For device fonts:
// - High byte (type >> 8) may contain device-specific information
// - Font may require specific hardware/driver to render
// - Generic rendering may not work correctly
```

**Practical implications:**
- Device fonts were used for printer-resident fonts
- The font file may only contain metrics, not actual glyph data
- Modern software typically ignores the device flag
- If bitmap data is present, it can usually still be rendered

### Combined Type Checking

```cpp
enum class FontType {
    RasterNormal,      // Standard bitmap font in file
    RasterMemory,      // Bitmap in ROM at fixed address
    RasterDevice,      // Device-specific raster font
    VectorNormal,      // Standard vector font in file
    VectorMemory,      // Vector data in ROM
    VectorDevice,      // Device-specific vector font
    Invalid
};

FontType classify_font(uint16_t type) {
    bool is_vector = (type & 0x0001) != 0;
    bool is_memory = (type & 0x0004) != 0;
    bool is_device = (type & 0x0080) != 0;

    // Check reserved bit
    if (type & 0x0002) {
        return FontType::Invalid;
    }

    if (is_vector) {
        if (is_memory) return FontType::VectorMemory;
        if (is_device) return FontType::VectorDevice;
        return FontType::VectorNormal;
    } else {
        if (is_memory) return FontType::RasterMemory;
        if (is_device) return FontType::RasterDevice;
        return FontType::RasterNormal;
    }
}
```

### Support Status

| Type | Description | Renderable from File |
|------|-------------|---------------------|
| `0x0000` | Raster, normal | Yes |
| `0x0001` | Vector, normal | Yes (with stroke renderer) |
| `0x0004` | Raster, ROM | No (requires hardware) |
| `0x0005` | Vector, ROM | No (requires hardware) |
| `0x0080` | Raster, device | Usually yes |
| `0x0081` | Vector, device | Usually yes |
| `0x0084` | Raster, ROM+device | No |
| `0x0085` | Vector, ROM+device | No |

**Recommendation:** For file-based rendering, only process fonts where `(type & 0x0004) == 0`. The memory/ROM flag indicates the bitmap data isn't in the file.

## Validation Checklist

Before rendering, validate:

1. **Version:** Must be 0x0200 or 0x0300
2. **File type:** Bit 0 must be 0 for bitmap fonts (or 1 for vector with appropriate renderer)
3. **Memory flag:** Bit 2 must be 0 (data in file, not ROM)
4. **Pixel height:** Must be > 0
5. **Character range:** `last_char >= first_char`
6. **Glyph offsets:** Must be within file bounds
7. **Bitmap data bounds:** `offset + (pitch * height) <= file_size`

## DataScript Schema

The libexe library includes a DataScript schema for parsing FNT fonts. See `src/libexe/formats/resources/fonts.ds`.

```datascript
package formats.resources.fonts;

little;  // Little-endian byte order

// =============================================================================
// Enums
// =============================================================================

enum uint16 font_type {
    FNT_TYPE_RASTER   = 0x0000,  // Raster (bitmap) font
    FNT_TYPE_VECTOR   = 0x0001,  // Vector font
    FNT_TYPE_MEMORY   = 0x0004,  // Bits in memory at fixed address
    FNT_TYPE_DEVICE   = 0x0080   // Font realized by device
};

enum uint8 font_family {
    FF_DONTCARE   = 0x00,  // Don't care or don't know
    FF_ROMAN      = 0x10,  // Proportional with serifs (Times New Roman)
    FF_SWISS      = 0x20,  // Proportional without serifs (Arial, Helvetica)
    FF_MODERN     = 0x30,  // Fixed-pitch (Courier, Console)
    FF_SCRIPT     = 0x40,  // Script fonts (cursive)
    FF_DECORATIVE = 0x50   // Decorative fonts (ornamental)
};

enum uint8 font_pitch {
    FIXED_PITCH    = 0x00,  // Fixed pitch (all characters same width)
    VARIABLE_PITCH = 0x01   // Variable pitch (proportional spacing)
};

enum uint32 font_flags {
    DFF_FIXED           = 0x0001,  // Fixed pitch font
    DFF_PROPORTIONAL    = 0x0002,  // Proportional pitch font
    DFF_ABCFIXED        = 0x0004,  // ABC fixed font (advanced spacing)
    DFF_ABCPROPORTIONAL = 0x0008,  // ABC proportional font
    DFF_1COLOR          = 0x0010,  // 1 color (8 pixels per byte, monochrome)
    DFF_16COLOR         = 0x0020,  // 16 color (2 pixels per byte)
    DFF_256COLOR        = 0x0040,  // 256 color (1 pixel per byte)
    DFF_RGBCOLOR        = 0x0080   // RGB color (RGBquads)
};

// =============================================================================
// Font Header (common fields)
// =============================================================================

struct font_header {
    uint16 version;              // 0x0200 or 0x0300
    uint32 size;                 // Total file size in bytes
    uint8  copyright[60];        // Copyright string
    uint16 type;                 // Font type (font_type enum)
    uint16 points;               // Nominal point size
    uint16 vert_res;             // Vertical resolution in DPI
    uint16 horiz_res;            // Horizontal resolution in DPI
    uint16 ascent;               // Distance from top to baseline
    uint16 internal_leading;     // Leading inside character cell
    uint16 external_leading;     // Extra leading between rows
    uint8  italic;               // Italic flag (0 or 1)
    uint8  underline;            // Underline flag (0 or 1)
    uint8  strike_out;           // Strikeout flag (0 or 1)
    uint16 weight;               // Weight (400=normal, 700=bold)
    uint8  char_set;             // Character set ID
    uint16 pix_width;            // Char width in pixels (0=variable)
    uint16 pix_height;           // Character height in pixels
    uint8  pitch_and_family;     // Low 4 bits=pitch, high 4 bits=family
    uint16 avg_width;            // Average character width
    uint16 max_width;            // Maximum character width
    uint8  first_char;           // First character code
    uint8  last_char;            // Last character code
    uint8  default_char;         // Default character (offset from first_char)
    uint8  break_char;           // Word break character
    uint16 width_bytes;          // Bytes per row of glyph bitmap
    uint32 device;               // Offset to device name string
    uint32 face;                 // Offset to typeface name string
    uint32 bits_pointer;         // Absolute address (set at load time)
    uint32 bits_offset;          // Offset to glyph bitmap data
    uint8  reserved;             // Reserved byte
    // Version 0x0300 only:
    uint32 flags;                // Font flags (font_flags enum)
    uint16 aspace;               // Global A space
    uint16 bspace;               // Global B space
    uint16 cspace;               // Global C space
    uint32 color_pointer;        // Offset to color table
    uint8  reserved1[16];        // Reserved
};

// =============================================================================
// Glyph Entry Structures
// =============================================================================

// Windows 2.x (version 0x0200) - 4 bytes per entry
struct glyph_entry_2x {
    uint16 width;                // Glyph width in pixels
    uint16 offset;               // Offset to bitmap data (16-bit)
};

// Windows 3.x (version 0x0300) - 6 bytes per entry
struct glyph_entry_30 {
    uint16 width;                // Glyph width in pixels
    uint32 offset;               // Offset to bitmap data (32-bit)
};

// ABC spacing format (DFF_ABCFIXED or DFF_ABCPROPORTIONAL)
struct glyph_entry_abc {
    uint16 width;                // Glyph width in pixels
    uint32 offset;               // Offset to bitmap data
    uint32 aspace;               // A space (16.16 fixed point)
    uint32 bspace;               // B space (16.16 fixed point)
    uint32 cspace;               // C space (16.16 fixed point)
};

// Color font format (DFF_16COLOR, DFF_256COLOR, DFF_RGBCOLOR)
struct glyph_entry_color {
    uint16 width;                // Glyph width in pixels
    uint32 offset;               // Offset to bitmap data
    uint16 height;               // Glyph height in pixels
    uint32 aspace;               // A space (16.16 fixed point)
    uint32 bspace;               // B space (16.16 fixed point)
    uint32 cspace;               // C space (16.16 fixed point)
};

// =============================================================================
// Complete Font Structures
// =============================================================================

// Windows 2.x font file
struct font_2x {
    uint16 version : version == 0x0200;
    uint32 size;
    uint8  copyright[60];
    uint16 type;
    uint16 points;
    uint16 vert_res;
    uint16 horiz_res;
    uint16 ascent;
    uint16 internal_leading;
    uint16 external_leading;
    uint8  italic;
    uint8  underline;
    uint8  strike_out;
    uint16 weight;
    uint8  char_set;
    uint16 pix_width;
    uint16 pix_height;
    uint8  pitch_and_family;
    uint16 avg_width;
    uint16 max_width;
    uint8  first_char;
    uint8  last_char;
    uint8  default_char;
    uint8  break_char;
    uint16 width_bytes;
    uint32 device;
    uint32 face;
    uint32 bits_pointer;
    uint32 bits_offset;
    uint8  reserved;
    // Glyph table: (last_char - first_char + 2) entries
    glyph_entry_2x glyphs[last_char - first_char + 2];
};

// Windows 3.x font file
struct font_3x {
    uint16 version : version == 0x0300;
    uint32 size;
    uint8  copyright[60];
    uint16 type;
    uint16 points;
    uint16 vert_res;
    uint16 horiz_res;
    uint16 ascent;
    uint16 internal_leading;
    uint16 external_leading;
    uint8  italic;
    uint8  underline;
    uint8  strike_out;
    uint16 weight;
    uint8  char_set;
    uint16 pix_width;
    uint16 pix_height;
    uint8  pitch_and_family;
    uint16 avg_width;
    uint16 max_width;
    uint8  first_char;
    uint8  last_char;
    uint8  default_char;
    uint8  break_char;
    uint16 width_bytes;
    uint32 device;
    uint32 face;
    uint32 bits_pointer;
    uint32 bits_offset;
    uint8  reserved;
    uint32 flags;
    uint16 aspace;
    uint16 bspace;
    uint16 cspace;
    uint32 color_pointer;
    uint8  reserved1[16];
    // Glyph table: (last_char - first_char + 2) entries
    glyph_entry_30 glyphs[last_char - first_char + 2];
};
```

### Glyph Entry Selection by Flags

The `flags` field (version 0x0300 only) determines which glyph entry format to use:

| Flags | Entry Format | Entry Size |
|-------|--------------|------------|
| `DFF_FIXED` or `DFF_PROPORTIONAL` | `glyph_entry_30` | 6 bytes |
| `DFF_ABCFIXED` or `DFF_ABCPROPORTIONAL` | `glyph_entry_abc` | 18 bytes |
| `DFF_1COLOR` | `glyph_entry_30` | 6 bytes |
| `DFF_16COLOR`, `DFF_256COLOR`, `DFF_RGBCOLOR` | `glyph_entry_color` | 20 bytes |

For version 0x0200 fonts, always use `glyph_entry_2x` (4 bytes).

### Glyph Count

The glyph table contains `(last_char - first_char + 2)` entries:
- One entry for each character in the range `[first_char, last_char]`
- One sentinel entry at the end (used for calculating bitmap sizes)

## Advanced Font Variants

The FNT format specification defines several advanced font variants beyond basic fixed/proportional bitmap fonts. **Important:** According to Windows NT 3.5 source code comments, these formats are **"not supported in Win 3.0, except maybe if someone has custom created such a font, using font editor or a similar tool"**.

### Font Flags Summary

```cpp
#define DFF_FIXED            0x0001  // Fixed pitch font (supported)
#define DFF_PROPORTIONAL     0x0002  // Proportional pitch font (supported)
#define DFF_ABCFIXED         0x0004  // ABC fixed font (rarely supported)
#define DFF_ABCPROPORTIONAL  0x0008  // ABC proportional font (rarely supported)
#define DFF_1COLOR           0x0010  // 1 color / 8 pixels per byte (rarely supported)
#define DFF_16COLOR          0x0020  // 16 color / 2 pixels per byte (rarely supported)
#define DFF_256COLOR         0x0040  // 256 color / 1 pixel per byte (rarely supported)
#define DFF_RGBCOLOR         0x0080  // RGB color / RGBquads (rarely supported)
```

### ABC Spacing Fonts (DFF_ABCFIXED, DFF_ABCPROPORTIONAL)

ABC spacing provides finer control over character positioning than simple width-based advance. Each character has three spacing values:

- **A space**: Distance from current position to left edge of glyph bitmap (can be negative for overhanging glyphs)
- **B space**: Width of the glyph bitmap itself
- **C space**: Distance from right edge of glyph to next character position (can be negative for kerning)

The total advance is: `A + B + C`

#### Global vs Per-Glyph ABC

- **Global ABC** (`DFF_FIXED` with header `aspace`/`bspace`/`cspace`): All glyphs share the same ABC values from the header
- **Per-Glyph ABC** (`DFF_ABCFIXED`/`DFF_ABCPROPORTIONAL`): Each glyph has its own ABC values in the glyph entry

#### ABC Glyph Entry Structure

```cpp
struct glyph_entry_abc {
    uint16_t width;     // Glyph width in pixels
    uint32_t offset;    // Offset to bitmap data
    int32_t  aspace;    // A space (16.16 fixed-point)
    int32_t  bspace;    // B space (16.16 fixed-point)
    int32_t  cspace;    // C space (16.16 fixed-point)
};
// Total: 18 bytes per entry
```

#### Fixed-Point Format

ABC spacing values use **16.16 fixed-point** format:
- High 16 bits: signed integer part
- Low 16 bits: fractional part (65536ths of a pixel)

```cpp
// Convert 16.16 fixed-point to float
float fixed_to_float(int32_t fixed) {
    return (float)fixed / 65536.0f;
}

// Convert float to 16.16 fixed-point
int32_t float_to_fixed(float f) {
    return (int32_t)(f * 65536.0f);
}
```

#### ABC Rendering Algorithm

```cpp
void render_text_abc(const char* text, int x, int baseline_y) {
    float cursor_x = (float)x;

    for (const char* p = text; *p; p++) {
        auto glyph = get_abc_glyph(*p);

        // A space: offset from cursor to glyph left edge
        float glyph_x = cursor_x + fixed_to_float(glyph.aspace);

        // Render glyph bitmap at (glyph_x, baseline_y - ascent)
        render_bitmap(glyph.bitmap, glyph_x, baseline_y - ascent);

        // Advance cursor: A + B + C
        cursor_x += fixed_to_float(glyph.aspace);
        cursor_x += fixed_to_float(glyph.bspace);
        cursor_x += fixed_to_float(glyph.cspace);
    }
}
```

### Color Fonts (DFF_1COLOR, DFF_16COLOR, DFF_256COLOR, DFF_RGBCOLOR)

Color fonts store glyph bitmaps with multiple bits per pixel instead of monochrome 1-bit bitmaps.

#### Color Depth Formats

| Flag | Bits/Pixel | Pixels/Byte | Description |
|------|------------|-------------|-------------|
| `DFF_1COLOR` | 1 | 8 | Monochrome (same as standard bitmap) |
| `DFF_16COLOR` | 4 | 2 | 16-color palette indexed |
| `DFF_256COLOR` | 8 | 1 | 256-color palette indexed |
| `DFF_RGBCOLOR` | 32 | 0.25 | Direct RGB (RGBQUAD per pixel) |

#### Color Glyph Entry Structure

```cpp
struct glyph_entry_color {
    uint16_t width;     // Glyph width in pixels
    uint32_t offset;    // Offset to bitmap data
    uint16_t height;    // Glyph height in pixels (may differ from font height)
    int32_t  aspace;    // A space (16.16 fixed-point)
    int32_t  bspace;    // B space (16.16 fixed-point)
    int32_t  cspace;    // C space (16.16 fixed-point)
};
// Total: 20 bytes per entry
```

**Note:** Color fonts include a per-glyph `height` field, allowing variable-height glyphs unlike standard bitmap fonts.

#### Color Table

The `color_pointer` field in the header points to a color palette (if non-zero):
- **16-color fonts**: 16 RGBQUAD entries (64 bytes)
- **256-color fonts**: 256 RGBQUAD entries (1024 bytes)
- **RGB fonts**: No palette needed (direct color)

```cpp
struct RGBQUAD {
    uint8_t blue;
    uint8_t green;
    uint8_t red;
    uint8_t reserved;  // Always 0
};
```

#### Color Bitmap Layout

Color bitmaps use the same column-major storage as monochrome fonts, but with different pixel packing:

```cpp
// 16-color: 2 pixels per byte (high nibble = first pixel)
uint8_t get_pixel_16color(const uint8_t* bitmap, int x, int y, int pitch) {
    int byte_offset = (x / 2) * height + y;
    uint8_t byte_val = bitmap[byte_offset];
    if (x % 2 == 0) {
        return (byte_val >> 4) & 0x0F;  // High nibble
    } else {
        return byte_val & 0x0F;          // Low nibble
    }
}

// 256-color: 1 pixel per byte
uint8_t get_pixel_256color(const uint8_t* bitmap, int x, int y, int pitch) {
    return bitmap[x * height + y];
}

// RGB: 4 bytes per pixel (RGBQUAD)
RGBQUAD get_pixel_rgb(const uint8_t* bitmap, int x, int y, int height) {
    const RGBQUAD* pixels = (const RGBQUAD*)bitmap;
    return pixels[x * height + y];
}
```

### Practical Support Status

| Font Type | Windows 3.0 | Windows 3.1 | Windows NT | FreeType |
|-----------|-------------|-------------|------------|----------|
| `DFF_FIXED` | Yes | Yes | Yes | Yes |
| `DFF_PROPORTIONAL` | Yes | Yes | Yes | Yes |
| `DFF_ABCFIXED` | No | Partial | Yes | No |
| `DFF_ABCPROPORTIONAL` | No | Partial | Yes | No |
| `DFF_1COLOR` | No | No | No | No |
| `DFF_16COLOR` | No | No | No | No |
| `DFF_256COLOR` | No | No | No | No |
| `DFF_RGBCOLOR` | No | No | No | No |

**Recommendation:** For maximum compatibility, only use `DFF_FIXED` or `DFF_PROPORTIONAL` bitmap fonts. The advanced formats were specified but never widely implemented.

## Vector Font Format (FNT_TYPE_VECTOR)

Windows FNT files can also contain **vector (stroke) fonts** instead of bitmap fonts. These are identified by:
- `file_type & 1 == 1` (low bit set)
- Also called "plotter fonts" or "continuous scaling" fonts
- Examples: `MODERN.FON`, `ROMAN.FON`, `SCRIPT.FON` from Windows 3.x

**Note:** FreeType does NOT support vector FNT fonts - it explicitly rejects them. The information below is based on the **Windows NT 3.5 VTFD (Vector TrueType Font Driver)** source code, which is the authoritative implementation.

### Vector Font Identification

```cpp
bool is_vector_font(uint16_t file_type) {
    return (file_type & 0x0001) != 0;
}
```

### Pen-Up Command

The pen-up marker is defined as:

```cpp
#define PEN_UP (char)0x80  // -128 as signed char
```

When the X-coordinate byte equals `PEN_UP` (0x80), it signals the end of a polyline segment.

### Stroke Data Format

Vector glyphs are stored as a series of **signed byte coordinate pairs** (dx, dy). These are **relative deltas** that accumulate from the starting position.

```cpp
// Each coordinate pair is two signed bytes
struct stroke_delta {
    int8_t dx;    // Relative X delta
    int8_t dy;    // Relative Y delta
};
```

### Starting Position

The path starts from the **top-left corner** of the character cell. In the Windows GDI coordinate system:

```cpp
// Initial position (from NT 3.5 VTFD)
point.x = 0;
point.y = -ascender;  // Negative because GDI Y-axis points up
```

In screen coordinates (Y-axis down), the starting point is at `(0, 0)` relative to the character cell's top-left corner.

### Vector Glyph Rendering Algorithm (from NT 3.5)

The following algorithm is based on the `bCreatePath()` function from the Windows NT 3.5 VTFD driver:

```cpp
// PEN_UP marker
const int8_t PEN_UP = (int8_t)0x80;

struct Point {
    int32_t x;
    int32_t y;
};

void render_vector_glyph(const uint8_t* stroke_data,
                         size_t stroke_end,
                         int ascender,
                         float scale_x,
                         float scale_y) {
    std::vector<Point> points;

    // Starting position: top-left of character cell
    Point current;
    current.x = 0;
    current.y = 0;  // In screen coords; in GDI would be -ascender
    points.push_back(current);

    const int8_t* p = (const int8_t*)stroke_data;
    const int8_t* end = (const int8_t*)(stroke_data + stroke_end);

    while (p <= end) {
        if ((*p != PEN_UP) && (p != end)) {
            // Accumulate relative delta to get new absolute position
            int8_t dx = *p++;
            int8_t dy = *p++;

            Point next;
            next.x = points.back().x + dx;
            next.y = points.back().y + dy;
            points.push_back(next);
        } else {
            // PEN_UP or end of data: flush current polyline
            if (points.size() > 1) {
                // Move to first point
                move_to(points[0].x * scale_x, points[0].y * scale_y);

                // Draw lines through remaining points
                for (size_t i = 1; i < points.size(); i++) {
                    line_to(points[i].x * scale_x, points[i].y * scale_y);
                }
            }

            if (p >= end) break;

            // Skip past PEN_UP marker and read next starting position
            p++;  // Skip PEN_UP byte

            // Next coordinate pair is the new starting position (relative to last point)
            int8_t dx = *p++;
            int8_t dy = *p++;

            Point new_start;
            new_start.x = points.back().x + dx;
            new_start.y = points.back().y + dy;

            // Start new polyline
            points.clear();
            points.push_back(new_start);
        }
    }
}
```

### Key Implementation Details

1. **Coordinates are cumulative**: Each (dx, dy) pair is added to the previous point's position, not the origin.

2. **PEN_UP handling**: When `dx == 0x80` (-128), the current polyline is flushed (drawn), then the next coordinate pair defines a new starting point for the next polyline.

3. **Polyline-based rendering**: Glyphs are composed of multiple polylines. Each polyline is drawn with connected line segments (MoveTo + PolyLineTo).

4. **Bold simulation**: The NT driver duplicates the entire path with an X-offset for bold rendering:
   ```cpp
   // For bold simulation, draw the path twice:
   // 1. Original path at (x, y)
   // 2. Duplicate path at (x + bold_offset, y)
   ```

5. **Italic simulation**: Apply a shear transformation by adjusting X based on Y:
   ```cpp
   // For italic, transform each point:
   x_italic = x + (y * italic_factor);
   ```

### Coordinate Size

The coordinate storage size depends on font dimensions:

| Condition | Coordinate Size | Pen-Up Value |
|-----------|-----------------|--------------|
| `pixel_height <= 128` AND `max_width <= 128` | 1 byte (signed) | 0x80 (-128) |
| `pixel_height > 128` OR `max_width > 128` | 2 bytes (signed, LE) | 0x8000 (-32768) |

Most vector fonts use 1-byte coordinates.

### Glyph Entry for Vector Fonts

**Fixed-pitch vector fonts:** Each 2-byte entry specifies the offset from the start of stroke data.

**Proportionally-spaced vector fonts:** Each 4-byte entry:
```cpp
struct vector_glyph_entry {
    uint16_t offset;    // Offset to stroke data
    uint16_t width;     // Character width in pixels
};
```

### Scaling Vector Fonts

Vector fonts can be scaled to any size (hence "continuous scaling"):

```cpp
float scale = desired_height / (float)header.pixel_height;

// Apply scale uniformly to all coordinates
int scaled_x = (int)(x * scale);
int scaled_y = (int)(y * scale);
```

### DataScript for Vector Fonts

```datascript
// Vector font glyph entry (proportional)
struct vector_glyph_entry {
    uint16 offset;    // Offset to stroke data from dfBitsOffset
    uint16 width;     // Character width
};

// Stroke point (1-byte coordinates)
struct stroke_delta {
    int8 dx;
    int8 dy;
};

// Pen-up sentinel value: dx == 0x80 (-128)
```

### Complete Rendering Example

```cpp
class VectorFontRenderer {
public:
    void render_glyph(uint8_t char_code, int x, int y, float scale) {
        // Get glyph entry
        auto entry = get_glyph_entry(char_code);

        // Get stroke data pointer
        const uint8_t* strokes = font_data_ + header_.bits_offset + entry.offset;

        // Calculate stroke data size (difference to next glyph's offset)
        auto next_entry = get_glyph_entry(char_code + 1);
        size_t stroke_size = next_entry.offset - entry.offset;

        // Render with accumulated coordinates
        render_strokes(strokes, stroke_size, x, y, scale);
    }

private:
    void render_strokes(const uint8_t* data, size_t size,
                        int origin_x, int origin_y, float scale) {
        const int8_t PEN_UP = (int8_t)0x80;

        std::vector<std::pair<int,int>> polyline;
        int abs_x = 0, abs_y = 0;
        polyline.push_back({abs_x, abs_y});

        const int8_t* p = (const int8_t*)data;
        const int8_t* end = p + size;

        while (p < end) {
            int8_t dx = *p++;

            if (dx == PEN_UP) {
                // Flush current polyline
                draw_polyline(polyline, origin_x, origin_y, scale);
                polyline.clear();

                if (p >= end) break;

                // Read new start position (relative to last point)
                dx = *p++;
                int8_t dy = *p++;
                abs_x += dx;
                abs_y += dy;
                polyline.push_back({abs_x, abs_y});
            } else {
                // Normal point: accumulate delta
                int8_t dy = *p++;
                abs_x += dx;
                abs_y += dy;
                polyline.push_back({abs_x, abs_y});
            }
        }

        // Flush final polyline
        if (polyline.size() > 1) {
            draw_polyline(polyline, origin_x, origin_y, scale);
        }
    }

    void draw_polyline(const std::vector<std::pair<int,int>>& points,
                       int origin_x, int origin_y, float scale) {
        if (points.empty()) return;

        move_to(origin_x + points[0].first * scale,
                origin_y + points[0].second * scale);

        for (size_t i = 1; i < points.size(); i++) {
            line_to(origin_x + points[i].first * scale,
                    origin_y + points[i].second * scale);
        }
    }
};
```

### Vector Font Limitations

1. **No curves** - Only straight line segments (unlike TrueType/OpenType)
2. **No fill** - Stroke-based only, characters are outlines
3. **Limited software support** - FreeType, FontForge don't support them
4. **Obsolete** - Rarely used in modern applications

### Source Reference

The authoritative implementation is found in the Windows NT 3.5 source code:
- `PRIVATE/WINDOWS/GDI/FONDRV/VTFD/FONTFILE.H` - Defines `PEN_UP` as `(CHAR)0x80`
- `PRIVATE/WINDOWS/GDI/FONDRV/VTFD/FDQUERY.C` - Contains `bCreatePath()` function with the actual rendering algorithm

## References

- FreeType source: `src/winfonts/winfnt.c`
- FreeType header: `include/freetype/ftwinfnt.h`
- libexe DataScript: `src/libexe/formats/resources/fonts.ds`
- Microsoft FNT specification (historical)
- [Microsoft KB Q65123 - Font File Format](https://jeffpar.github.io/kbarchive/kb/065/Q65123/)
- [FontForge Issue #3841 - Vector FNT format discussion](https://github.com/fontforge/fontforge/issues/3841)
- [ModdingWiki - Microsoft Vector Font Format](https://moddingwiki.shikadi.net/wiki/Microsoft_Vector_Font_Format)
