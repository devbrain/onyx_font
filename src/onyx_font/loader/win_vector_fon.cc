//
// Created by igor on 12/21/2025.
//
// Loader for Microsoft Windows vector fonts (.FNT format)
//

#include "loaders.hh"

namespace onyx_font::internal {

    vector_font win_vector_fon_loader::load(const libexe::font_data& fd) {
        vector_font result;

        result.m_name = fd.face_name;
        result.m_first_char = fd.first_char;
        result.m_last_char = fd.last_char;
        result.m_default_char = fd.default_char;

        // Populate metrics
        result.m_metrics.ascent = static_cast<int16_t>(fd.ascent);
        result.m_metrics.descent = 0;  // MS fonts don't have explicit descent
        result.m_metrics.baseline = 0;
        result.m_metrics.pixel_height = fd.pixel_height;
        result.m_metrics.avg_width = fd.avg_width;
        result.m_metrics.max_width = fd.max_width;

        // Convert vector glyphs
        result.m_glyphs.reserve(fd.vector_glyphs.size());

        for (const auto& src_glyph : fd.vector_glyphs) {
            vector_glyph dst_glyph;
            dst_glyph.width = src_glyph.width;
            dst_glyph.strokes.reserve(src_glyph.strokes.size());

            for (const auto& src_cmd : src_glyph.strokes) {
                stroke_command dst_cmd{};

                switch (src_cmd.cmd) {
                    case libexe::stroke_command::type::MOVE_TO:
                        dst_cmd.type = stroke_type::MOVE_TO;
                        dst_cmd.dx = src_cmd.x;
                        dst_cmd.dy = src_cmd.y;
                        dst_glyph.strokes.push_back(dst_cmd);
                        break;

                    case libexe::stroke_command::type::LINE_TO:
                        dst_cmd.type = stroke_type::LINE_TO;
                        dst_cmd.dx = src_cmd.x;
                        dst_cmd.dy = src_cmd.y;
                        dst_glyph.strokes.push_back(dst_cmd);
                        break;

                    case libexe::stroke_command::type::PEN_UP:
                        // PEN_UP becomes END (end of polyline segment)
                        dst_cmd.type = stroke_type::END;
                        dst_cmd.dx = 0;
                        dst_cmd.dy = 0;
                        dst_glyph.strokes.push_back(dst_cmd);
                        break;
                }
            }

            result.m_glyphs.push_back(std::move(dst_glyph));
        }

        return result;
    }

}  // namespace onyx_font::internal
