//
// Created by igor on 11/12/2025.
//

#include <onyx_font/vector_font.hh>

namespace onyx_font {

    vector_font::vector_font() = default;

    std::string vector_font::get_name() const {
        return m_name;
    }

    uint8_t vector_font::get_first_char() const {
        return m_first_char;
    }

    uint8_t vector_font::get_last_char() const {
        return m_last_char;
    }

    uint8_t vector_font::get_default_char() const {
        return m_default_char;
    }

    const vector_font_metrics& vector_font::get_metrics() const {
        return m_metrics;
    }

    bool vector_font::has_glyph(uint8_t ch) const {
        return ch >= m_first_char && ch <= m_last_char;
    }

    const vector_glyph* vector_font::get_glyph(uint8_t ch) const {
        if (!has_glyph(ch)) {
            return nullptr;
        }
        return &m_glyphs[ch - m_first_char];
    }

}  // namespace onyx_font
