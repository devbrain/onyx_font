# Windows Raster and Vector Font File Format

This document describes the Windows .FNT font file format used in Windows 1.0, 2.0, and 3.0. These fonts are typically embedded as RT_FONT resources within NE (New Executable) files with the .FON extension.

## Overview

Windows font files come in two main types:
- **Raster fonts**: Bitmap-based fonts where each glyph is stored as a pixel grid
- **Vector fonts**: Stroke-based fonts where each glyph is defined by pen movements

The format evolved across Windows versions:
- **Version 1.0 (0x0100)**: Original format with 117-byte header
- **Version 2.0 (0x0200)**: Extended format with 118-byte header
- **Version 3.0 (0x0300)**: Further extended with additional fields

## File Structure

```
+------------------+
| Font Header      |  117 bytes (v1.0) or 118 bytes (v2.0/3.0)
+------------------+
| Glyph Table      |  Variable size
+------------------+
| Face Name        |  Null-terminated string
+------------------+
| Device Name      |  Null-terminated string (optional)
+------------------+
| Bitmap/Stroke    |  Glyph bitmap or vector data
| Data             |
+------------------+
```

## Font Header

### Common Header (All Versions)

| Offset | Size | Field | Description |
|--------|------|-------|-------------|
| 0 | 2 | dfVersion | Font version: 0x0100 (1.0), 0x0200 (2.0), 0x0300 (3.0) |
| 2 | 4 | dfSize | Total file size in bytes |
| 6 | 60 | dfCopyright | Copyright string (null-padded) |
| 66 | 2 | dfType | Font type flags (see below) |
| 68 | 2 | dfPoints | Nominal point size |
| 70 | 2 | dfVertRes | Vertical resolution (DPI) |
| 72 | 2 | dfHorizRes | Horizontal resolution (DPI) |
| 74 | 2 | dfAscent | Distance from baseline to top of cell |
| 76 | 2 | dfInternalLeading | Space for accent marks within cell |
| 78 | 2 | dfExternalLeading | Extra line spacing (outside cell) |
| 80 | 1 | dfItalic | 1 = italic font |
| 81 | 1 | dfUnderline | 1 = underlined font |
| 82 | 1 | dfStrikeOut | 1 = strikeout font |
| 83 | 2 | dfWeight | Font weight (100-900, 400=normal, 700=bold) |
| 85 | 1 | dfCharSet | Character set (0=ANSI, 255=OEM) |
| 86 | 2 | dfPixWidth | Character width (0 = variable pitch) |
| 88 | 2 | dfPixHeight | Character height in pixels |
| 90 | 1 | dfPitchAndFamily | Pitch (low nibble) and family (high nibble) |
| 91 | 2 | dfAvgWidth | Average character width |
| 93 | 2 | dfMaxWidth | Maximum character width |
| 95 | 1 | dfFirstChar | First character code in font |
| 96 | 1 | dfLastChar | Last character code in font |
| 97 | 1 | dfDefaultChar | Default character (relative to dfFirstChar) |
| 98 | 1 | dfBreakChar | Break character (relative to dfFirstChar) |
| 99 | 2 | dfWidthBytes | Bytes per row in bitmap (raster only) |
| 101 | 4 | dfDevice | Offset to device name string (0 = generic) |
| 105 | 4 | dfFace | Offset to face name string |
| 109 | 4 | dfBitsPointer | Reserved (set by GDI at load time) |
| 113 | 4 | dfBitsOffset | Offset to bitmap/stroke data |

**Total: 117 bytes**

### Version 2.0/3.0 Additional Field

| Offset | Size | Field | Description |
|--------|------|-------|-------------|
| 117 | 1 | dfReserved | Reserved byte |

**Total: 118 bytes**

### Version 3.0 Extended Header

Version 3.0 adds additional fields after the glyph table for extended character metrics (ABC spacing).

### dfType Field Bits

| Bit | Meaning |
|-----|---------|
| 0 | 0 = Raster font, 1 = Vector font |
| 1 | Reserved (must be 0) |
| 2 | 1 = Bitmap in memory at dfBitsOffset address |
| 7 | 1 = Font realized by device |
| 8-15 | Reserved for device use |

### dfPitchAndFamily Field

**Low nibble (Pitch):**

| Value | Meaning |
|-------|---------|
| 0x00 | Default pitch |
| 0x01 | Fixed pitch (monospace) |
| 0x02 | Variable pitch (proportional) |

**High nibble (Family):**

| Value | Meaning |
|-------|---------|
| 0x00 | FF_DONTCARE |
| 0x10 | FF_ROMAN (serif, variable stroke) |
| 0x20 | FF_SWISS (sans-serif, variable stroke) |
| 0x30 | FF_MODERN (fixed stroke, serif or sans-serif) |
| 0x40 | FF_SCRIPT (cursive) |
| 0x50 | FF_DECORATIVE (novelty fonts) |

---

## Glyph Table

The glyph table immediately follows the header and contains entries for each character from dfFirstChar to dfLastChar, plus one sentinel entry.

**Number of entries:** `(dfLastChar - dfFirstChar + 1) + 1`

### Windows 1.0 Format

#### Variable-Pitch Raster Fonts (dfPixWidth == 0)

Each entry is **2 bytes**:

| Offset | Size | Field | Description |
|--------|------|-------|-------------|
| 0 | 2 | offset | Pixel offset into each bitmap row |

- **Width calculation:** `width = next_entry.offset - this_entry.offset`
- The offset is a **pixel position** (not byte position) within the row-major bitmap
- The sentinel entry provides the offset needed to calculate the last glyph's width

#### Fixed-Pitch Raster Fonts (dfPixWidth != 0)

No glyph table entries are needed. All glyphs have width = dfPixWidth.
Glyph offsets are calculated as: `offset = glyph_index * dfPixWidth`

#### Fixed-Pitch Vector Fonts (dfPixWidth != 0)

Each entry is **2 bytes**:

| Offset | Size | Field | Description |
|--------|------|-------|-------------|
| 0 | 2 | offset | Byte offset into stroke data |

- Width is dfPixWidth for all glyphs
- Stroke data length = `next_entry.offset - this_entry.offset`

#### Variable-Pitch Vector Fonts (dfPixWidth == 0)

Each entry is **4 bytes**:

| Offset | Size | Field | Description |
|--------|------|-------|-------------|
| 0 | 2 | offset | Byte offset into stroke data |
| 2 | 2 | width | Glyph width in pixels |

### Windows 2.0/3.0 Format

#### Raster Fonts

Each entry is **4 bytes**:

| Offset | Size | Field | Description |
|--------|------|-------|-------------|
| 0 | 2 | width | Glyph width in pixels |
| 2 | 2 | offset | Byte offset into bitmap data |

For Version 3.0 with wide offsets (fonts > 64KB), entries are **6 bytes**:

| Offset | Size | Field | Description |
|--------|------|-------|-------------|
| 0 | 2 | width | Glyph width in pixels |
| 2 | 4 | offset | Byte offset into bitmap data |

#### Vector Fonts

Same as Version 1.0 vector font format.

---

## Bitmap Data Format

### Windows 1.0 Raster Fonts (Row-Major)

Windows 1.0 stores **all glyphs in a single combined bitmap** using row-major order:

```
Row 0: [all pixels for row 0 of all glyphs]
Row 1: [all pixels for row 0 of all glyphs]
...
Row N: [all pixels for row N of all glyphs]
```

**Layout:**
- Each row is `dfWidthBytes` bytes (always even for word alignment)
- Total bitmap size: `dfWidthBytes * dfPixHeight` bytes
- Glyph table offsets are **pixel positions** within each row
- Within each byte: **MSB = leftmost pixel** (bit 7 = leftmost)

**Example:** For a font with dfWidthBytes=150, dfPixHeight=6:
```
Offset 0-149:   Row 0 (150 bytes)
Offset 150-299: Row 1 (150 bytes)
...
Offset 750-899: Row 5 (150 bytes)
```

To extract pixel (x, y) for a glyph starting at pixel offset `px_offset`:
```c
pixel_x = px_offset + x;
byte_offset = y * dfWidthBytes + (pixel_x / 8);
bit_position = 7 - (pixel_x % 8);  // MSB = leftmost
pixel_value = (bitmap[byte_offset] >> bit_position) & 1;
```

### Windows 2.0/3.0 Raster Fonts (Column-Major Per-Glyph)

Windows 2.0+ stores **each glyph separately** in column-major order:

```
Glyph N:
  Byte-column 0: [row 0, row 1, ..., row H-1]  (H bytes)
  Byte-column 1: [row 0, row 1, ..., row H-1]  (H bytes)
  ...
```

**Layout:**
- Each glyph is stored independently
- Glyph table offsets are **byte positions** from dfBitsOffset
- Glyphs are organized in "byte-columns" (groups of 8 horizontal pixels)
- Each byte-column contains one byte per row
- Within each byte: **MSB = leftmost pixel** of that 8-pixel segment

**Example:** For a glyph with width=10, height=16:
```
Bytes 0-15:   Byte-column 0 (pixels 0-7 of each row)
Bytes 16-31: Byte-column 1 (pixels 8-9 of each row, padded)
Total: 32 bytes per glyph
```

**Byte-column count:** `ceil(glyph_width / 8)`
**Bytes per glyph:** `byte_columns * dfPixHeight`

To extract pixel (x, y) from a glyph at offset `glyph_offset`:
```c
byte_column = x / 8;
bit_position = 7 - (x % 8);  // MSB = leftmost
byte_offset = glyph_offset + byte_column * dfPixHeight + y;
pixel_value = (bitmap[byte_offset] >> bit_position) & 1;
```

---

## Vector Font Stroke Data

Vector fonts define glyphs as a series of pen movements. The stroke data format is the same across all Windows versions.

### Coordinate Size

Coordinates are stored as either 1-byte or 2-byte signed values:
- **1-byte:** If both dfPixHeight ≤ 128 AND dfMaxWidth ≤ 128
- **2-byte:** Otherwise

### Stroke Commands

Strokes are encoded as coordinate pairs (x, y), optionally preceded by a pen-up marker:

| Value | Meaning |
|-------|---------|
| 0x80 (1-byte) or 0x8000 (2-byte) | Pen-up marker |
| Other | Signed relative coordinate |

### Stroke Sequence

```
[pen-up?] x0, y0    -> Move to (x0, y0) if after pen-up, else line to
[pen-up?] x1, y1    -> Move/line to (x1, y1)
...
```

- First coordinate after pen-up: **move** without drawing
- Subsequent coordinates: **line** to the point
- Pen-up marker: lift pen, next coordinate is a move

### Character Cell

- Origin is at the **upper-left corner** of the character cell
- Glyphs hang **down and to the right** from the origin
- X increases rightward, Y increases downward
- Coordinates are **relative** movements from the current position

### Decoding Algorithm

```c
bool pen_down = false;
int x = 0, y = 0;

while (data_remaining) {
    if (current_byte == PEN_UP_MARKER) {
        pen_down = false;
        advance();
        continue;
    }

    int8_t dx = read_coordinate();
    int8_t dy = read_coordinate();

    if (pen_down) {
        draw_line(x, y, x + dx, y + dy);
    }

    x += dx;
    y += dy;
    pen_down = true;
}
```

---

## Version Differences Summary

| Feature | Version 1.0 | Version 2.0 | Version 3.0 |
|---------|-------------|-------------|-------------|
| Header size | 117 bytes | 118 bytes | 118+ bytes |
| Raster bitmap | Row-major combined | Column-major per-glyph | Column-major per-glyph |
| Raster glyph table | 2-byte (offset only) | 4-byte (width + offset) | 4 or 6-byte |
| Vector glyph table | 2 or 4-byte | 2 or 4-byte | 2 or 4-byte |
| Offset meaning | Pixel position | Byte offset | Byte offset |
| Max font size | ~64KB | ~64KB | >64KB (6-byte entries) |
| ABC spacing | No | No | Optional |

---

## Character Sets

| Value | Name | Description |
|-------|------|-------------|
| 0 | ANSI_CHARSET | Windows ANSI (Western European) |
| 1 | DEFAULT_CHARSET | Default for current locale |
| 2 | SYMBOL_CHARSET | Symbol characters |
| 128 | SHIFTJIS_CHARSET | Japanese Shift-JIS |
| 129 | HANGUL_CHARSET | Korean Hangul |
| 134 | GB2312_CHARSET | Simplified Chinese |
| 136 | CHINESEBIG5_CHARSET | Traditional Chinese |
| 255 | OEM_CHARSET | OEM/DOS character set |

---

## Font Weights

| Value | Name |
|-------|------|
| 100 | FW_THIN |
| 200 | FW_EXTRALIGHT |
| 300 | FW_LIGHT |
| 400 | FW_NORMAL / FW_REGULAR |
| 500 | FW_MEDIUM |
| 600 | FW_SEMIBOLD |
| 700 | FW_BOLD |
| 800 | FW_EXTRABOLD |
| 900 | FW_BLACK |

---

## Implementation Notes

### Parsing Strategy

1. Read header (117 or 118 bytes based on version)
2. Check dfVersion to determine format
3. Calculate glyph count: `dfLastChar - dfFirstChar + 1`
4. Read glyph table based on version and font type
5. Read face name from dfFace offset
6. Process bitmap/stroke data from dfBitsOffset

### Version Detection

```c
uint16_t version = read_u16(data + 0);

switch (version) {
    case 0x0100: // Windows 1.0
        parse_1x_font(data);
        break;
    case 0x0200: // Windows 2.0
        parse_2x_font(data);
        break;
    case 0x0300: // Windows 3.0
        parse_3x_font(data);
        break;
}
```

### Font Type Detection

```c
uint16_t type = read_u16(data + 66);
bool is_vector = (type & 0x0001) != 0;
bool is_raster = !is_vector;
```

### Pitch Detection

```c
bool is_fixed_pitch = (dfPixWidth != 0);
bool is_variable_pitch = (dfPixWidth == 0);
```

---

## References

- Microsoft Windows 1.0 SDK Documentation, Appendix C: Font Files
- Microsoft Windows 3.1 SDK: Font File Format
- FreeType FNT driver source code
- Wine project font handling
