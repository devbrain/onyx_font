//
// Created by igor on 30/12/2025.
//
// Loader for GEM bitmap fonts (.gft, .fnt from Genus Microprogramming)
//
// Uses datascript-generated code for header parsing.
// Bitmap extraction follows FreeType's algorithm.
//

#include "loaders.hh"
#include <formats/gem/gem.hh>
#include <failsafe/failsafe.hh>
#include <cstring>

namespace onyx_font::internal {

    namespace {
        // Validate GEM font header
        bool validate_header(const formats::gem::gem_font_header& hdr, size_t file_size) {
            // fheight must be positive
            if (hdr.fheight <= 0) return false;

            // ascent must be non-negative
            if (hdr.ascent < 0) return false;

            // descent must be non-negative
            if (hdr.descent < 0) return false;

            // cellsize must be positive
            if (hdr.cellsize <= 0) return false;

            // maxch >= minch
            if (hdr.maxch < hdr.minch) return false;

            // Character codes should be reasonable (within 0-255 range)
            if (hdr.minch < 0 || hdr.minch > 255) return false;
            if (hdr.maxch < 0 || hdr.maxch > 255) return false;

            // hotptr must be valid (offset table)
            if (hdr.hotptr < 84 || static_cast<size_t>(hdr.hotptr) >= file_size) return false;

            // cotptr must be valid (bitmap data)
            if (hdr.cotptr < 84 || static_cast<size_t>(hdr.cotptr) >= file_size) return false;

            // fwidth must be positive
            if (hdr.fwidth <= 0) return false;

            // Check bitmap fits in file
            size_t bitmap_size = static_cast<size_t>(hdr.fwidth) * static_cast<size_t>(hdr.fheight);
            if (static_cast<size_t>(hdr.cotptr) + bitmap_size > file_size) return false;

            return true;
        }
    }

    bitmap_font gem_font_loader::load(std::span<const uint8_t> data) {
        THROW_IF(data.size() < 84, std::runtime_error,
                 "Invalid GEM font: file too small for header");

        // Parse header using datascript-generated code
        const uint8_t* ptr = data.data();
        const uint8_t* end = data.data() + data.size();
        auto header = formats::gem::gem_font_header::read(ptr, end);

        THROW_IF(!validate_header(header, data.size()), std::runtime_error,
                 "Invalid GEM font: header validation failed");

        int num_chars = header.maxch - header.minch + 1;

        // Read Character Offset Table from hotptr
        // Contains (num_chars + 1) entries of 16-bit bit offsets
        size_t offset_table_size = static_cast<size_t>(num_chars + 1) * 2;
        THROW_IF(static_cast<size_t>(header.hotptr) + offset_table_size > data.size(),
                 std::runtime_error, "Invalid GEM font: offset table extends beyond file");

        ptr = data.data() + header.hotptr;
        std::vector<uint16_t> offsets(static_cast<size_t>(num_chars + 1));
        for (int i = 0; i <= num_chars; ++i) {
            auto entry = formats::gem::gem_cot_entry::read(ptr, end);
            offsets[static_cast<size_t>(i)] = static_cast<uint16_t>(entry.offset);
        }

        // Get pointer to bitmap data (cotptr points to bitmap, despite the name)
        const uint8_t* bitmap = data.data() + header.cotptr;
        size_t bitmap_size = static_cast<size_t>(header.fwidth) * static_cast<size_t>(header.fheight);
        THROW_IF(static_cast<size_t>(header.cotptr) + bitmap_size > data.size(),
                 std::runtime_error, "Invalid GEM font: bitmap data extends beyond file");

        // Build bitmap font
        bitmap_font result;

        // Extract name (null-terminated, max 32 chars)
        result.m_name = std::string(
            reinterpret_cast<const char*>(header.fntname.data()),
            strnlen(reinterpret_cast<const char*>(header.fntname.data()), 32)
        );

        result.m_first_char = static_cast<uint8_t>(header.minch);
        result.m_last_char = static_cast<uint8_t>(header.maxch);
        result.m_default_char = '?';  // GEM doesn't specify default char
        result.m_break_char = ' ';

        // Populate metrics
        result.m_metrics.pixel_height = static_cast<uint16_t>(header.fheight);
        result.m_metrics.ascent = static_cast<uint16_t>(header.topline);  // topline = baseline to top
        result.m_metrics.internal_leading = 0;
        result.m_metrics.external_leading = 0;
        result.m_metrics.avg_width = 0;  // Will calculate
        result.m_metrics.max_width = static_cast<uint16_t>(header.maxwidth);

        // Build glyph storage
        bitmap_builder bb;
        bb.reserve_glyphs(static_cast<size_t>(num_chars));

        result.m_spacing.reserve(static_cast<size_t>(num_chars));

        int total_width = 0;
        int valid_chars = 0;

        for (int i = 0; i < num_chars; ++i) {
            // Offsets are BIT positions into each row of the bitmap
            uint16_t bit_offset = offsets[static_cast<size_t>(i)];
            uint16_t next_offset = offsets[static_cast<size_t>(i + 1)];
            int char_width = next_offset - bit_offset;

            // Ensure minimum width for empty glyphs
            int glyph_width = char_width > 0 ? char_width : 1;

            // Validate offset doesn't exceed bitmap width in bits
            THROW_IF(bit_offset + char_width > header.fwidth * 8,
                     std::runtime_error, "Invalid GEM font: character offset exceeds bitmap width");

            auto writer = bb.reserve_glyph(
                static_cast<uint16_t>(glyph_width),
                static_cast<uint16_t>(header.fheight)
            );

            // Extract glyph bitmap using FreeType's algorithm:
            // The bitmap is fwidth bytes per row, offsets are bit positions
            if (char_width > 0) {
                for (int y = 0; y < header.fheight; ++y) {
                    // Calculate byte position in bitmap row
                    const uint8_t* row_data = bitmap + header.fwidth * y + (bit_offset >> 3);
                    int bit_pos = bit_offset & 7;  // Starting bit within first byte
                    uint8_t in_mask = static_cast<uint8_t>(0x80 >> bit_pos);  // MSB first

                    for (int x = 0; x < char_width; ++x) {
                        bool pixel = (*row_data & in_mask) != 0;
                        writer.set_pixel(static_cast<uint16_t>(x),
                                         static_cast<uint16_t>(y), pixel);

                        // Advance to next bit
                        in_mask >>= 1;
                        if (in_mask == 0) {
                            in_mask = 0x80;
                            ++row_data;
                        }
                    }
                }
            } else {
                // Empty glyph - fill with zeros (already default)
                for (int y = 0; y < header.fheight; ++y) {
                    for (int x = 0; x < glyph_width; ++x) {
                        writer.set_pixel(static_cast<uint16_t>(x),
                                         static_cast<uint16_t>(y), false);
                    }
                }
            }

            // Set spacing (B-space is the character width)
            glyph_spacing spacing;
            spacing.b_space = static_cast<uint16_t>(glyph_width);
            result.m_spacing.push_back(spacing);

            if (char_width > 0) {
                total_width += char_width;
                ++valid_chars;
            }
        }

        // Calculate average width
        if (valid_chars > 0) {
            result.m_metrics.avg_width = static_cast<uint16_t>(total_width / valid_chars);
        }

        result.m_storage = std::move(bb).build();

        return result;
    }

    bool gem_font_loader::is_gem_font(std::span<const uint8_t> data) {
        if (data.size() < 84) {
            return false;
        }

        try {
            const uint8_t* ptr = data.data();
            const uint8_t* end = data.data() + data.size();
            auto header = formats::gem::gem_font_header::read(ptr, end);
            return validate_header(header, data.size());
        } catch (...) {
            return false;
        }
    }

}  // namespace onyx_font::internal
