//
// Created by igor on 12/21/2025.
//
// Loader for Borland BGI vector fonts (.CHR format)
//

#include "loaders.hh"
#include <formats/bgi/bgi.hh>
#include <failsafe/failsafe.hh>
#include <cstring>

namespace onyx_font::internal {

    namespace {
        // Find the 0x1A terminator after "PK" header
        // Returns offset to the byte after 0x1A, or 0 if not found
        std::size_t find_header_info_offset(std::span<const uint8_t> data) {
            if (data.size() < 2 || data[0] != 'P' || data[1] != 'K') {
                return 0;
            }

            // Scan for 0x1A terminator (max 253 bytes after "PK")
            for (std::size_t i = 2; i < std::min(data.size(), std::size_t{255}); ++i) {
                if (data[i] == 0x1A) {
                    return i + 1;  // Return offset after 0x1A
                }
            }
            return 0;
        }

        // Decode BGI stroke command opcode from the high bits of both bytes
        // Formula: 2*(byte0 bit7) + (byte1 bit7)
        // Returns: opcode
        //   0: End of character
        //   1: Scan/width marker (skip)
        //   2: Move to (pen up, then position)
        //   3: Draw to (line)
        uint8_t decode_opcode(uint8_t byte0, uint8_t byte1) {
            return static_cast<uint8_t>(((byte0 >> 7) << 1) + (byte1 >> 7));
        }

        // Decode 7-bit signed coordinate with sign extension from bit 6
        // Bits 0-6: 7-bit value
        // Bit 6: sign bit (extended to bit 7 for full signed byte)
        // Bit 7: opcode bit (not part of coordinate)
        int8_t decode_coord(uint8_t byte) {
            // (byte & 0x7F) gets the 7-bit value
            // ((byte & 0x40) << 1) extends the sign bit to bit 7
            return static_cast<int8_t>((byte & 0x7F) | ((byte & 0x40) << 1));
        }
    }

    vector_font bgi_font_loader::load(std::span<const uint8_t> data) {
        // Find header info after 0x1A
        std::size_t header_info_offset = find_header_info_offset(data);
        THROW_IF(header_info_offset == 0, std::runtime_error,
                 "Invalid BGI font: missing PK signature or 0x1A terminator");

        // Parse header info
        const uint8_t* ptr = data.data() + header_info_offset;
        const uint8_t* end = data.data() + data.size();

        auto header_info = formats::bgi::bgi_header_info::read(ptr, end);

        THROW_IF(header_info.font_major == 0, std::runtime_error,
                 "Invalid BGI font: font_major version is 0");

        // Parse font data at offset header_size
        THROW_IF(header_info.header_size >= data.size(), std::runtime_error,
                 "Invalid BGI font: header_size exceeds file size");

        ptr = data.data() + header_info.header_size;
        auto font_data = formats::bgi::bgi_font_data::read(ptr, end);

        // Build vector_font
        vector_font result;

        // Extract name from header_info (4 bytes, null-padded)
        result.m_name = std::string(
            reinterpret_cast<const char*>(header_info.font_name.data()),
            strnlen(reinterpret_cast<const char*>(header_info.font_name.data()), 4)
        );

        result.m_first_char = font_data.header.start_char;
        result.m_last_char = static_cast<uint8_t>(
            font_data.header.start_char + font_data.header.char_count - 1
        );
        result.m_default_char = font_data.header.start_char;

        // Populate metrics
        result.m_metrics.ascent = font_data.header.origin_to_ascender;
        result.m_metrics.descent = font_data.header.origin_to_descender;
        result.m_metrics.baseline = font_data.header.origin_to_baseline;
        result.m_metrics.pixel_height = static_cast<uint16_t>(
            font_data.header.origin_to_ascender - font_data.header.origin_to_descender
        );
        result.m_metrics.avg_width = 0;  // BGI doesn't provide this
        result.m_metrics.max_width = 0;

        // Parse each glyph's stroke data
        result.m_glyphs.reserve(font_data.header.char_count);

        const std::size_t strokes_base = header_info.header_size + font_data.header.strokes_offset;

        for (uint16_t i = 0; i < font_data.header.char_count; ++i) {
            vector_glyph glyph;

            // Calculate stroke data offset
            std::size_t stroke_offset = strokes_base + font_data.char_offsets[i];
            THROW_IF(stroke_offset + 2 > data.size(), std::runtime_error,
                     "Invalid BGI font: stroke offset exceeds file size");

            ptr = data.data() + stroke_offset;

            // BGI coordinates are ABSOLUTE from base origin (0, height)
            // We convert them to relative deltas for vector_font format
            const int base_x = 0;
            const int base_y = font_data.header.origin_to_ascender -
                               font_data.header.origin_to_descender;

            // Current pen position (starts at base origin)
            int pen_x = base_x;
            int pen_y = base_y;

            // Track bounds for width calculation
            int min_x = 0, max_x = 0;

            // Read stroke commands until opcode == 0 (end)
            while (ptr + 2 <= end) {
                auto cmd = formats::bgi::bgi_stroke_command::read(ptr, end);

                uint8_t opcode = decode_opcode(cmd.byte0, cmd.byte1);

                if (opcode == 0) {
                    // End of glyph
                    break;
                }

                if (opcode == 1) {
                    // Scan marker - skip
                    continue;
                }

                // Decode absolute coordinates from base origin
                int abs_x = base_x + decode_coord(cmd.byte0);
                int abs_y = base_y + static_cast<int8_t>(-decode_coord(cmd.byte1));  // Y inverted

                // Convert to relative delta from current pen position
                stroke_command stroke_cmd{};
                stroke_cmd.dx = static_cast<int8_t>(abs_x - pen_x);
                stroke_cmd.dy = static_cast<int8_t>(abs_y - pen_y);

                if (opcode == 2) {
                    // Move to (pen up, then move)
                    stroke_cmd.type = stroke_type::MOVE_TO;
                } else {
                    // opcode == 3: Line to (draw)
                    stroke_cmd.type = stroke_type::LINE_TO;
                    // Track bounds for drawn segments
                    min_x = std::min(min_x, std::min(pen_x, abs_x));
                    max_x = std::max(max_x, std::max(pen_x, abs_x));
                }

                // Update pen position
                pen_x = abs_x;
                pen_y = abs_y;

                glyph.strokes.push_back(stroke_cmd);
            }

            // Glyph width = advance width = final pen X position
            // In BGI fonts, the last MOVE_TO positions the pen for the next character
            glyph.width = static_cast<uint16_t>(std::max(0, pen_x));

            result.m_glyphs.push_back(std::move(glyph));
        }

        // Calculate max_width from glyphs
        for (const auto& g : result.m_glyphs) {
            if (g.width > result.m_metrics.max_width) {
                result.m_metrics.max_width = g.width;
            }
        }

        return result;
    }

}  // namespace onyx_font::internal
