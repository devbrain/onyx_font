//
// Created by igor on 11/12/2025.
//

#include <onyx_font/bitmap_font.hh>
#include <failsafe/exception.hh>

namespace onyx_font {
    bitmap_font::bitmap_font() = default;

    bitmap_font::bitmap_font(std::string name,
                             uint8_t first_char,
                             uint8_t last_char,
                             uint8_t default_char,
                             uint8_t break_char,
                             font_metrics metrics,
                             std::vector<glyph_spacing> spacing,
                             bitmap_storage storage)
        : m_name(std::move(name)),
          m_first_char(first_char),
          m_last_char(last_char),
          m_default_char(default_char),
          m_break_char(break_char),
          m_metrics(metrics),
          m_spacing(std::move(spacing)),
          m_storage(std::move(storage)) {
    }

    std::string bitmap_font::get_name() const {
        return m_name;
    }

    uint8_t bitmap_font::get_first_char() const {
        return m_first_char;
    }

    uint8_t bitmap_font::get_last_char() const {
        return m_last_char;
    }

    uint8_t bitmap_font::get_default_char() const {
        return m_default_char;
    }

    uint8_t bitmap_font::get_break_char() const {
        return m_break_char;
    }

    const font_metrics& bitmap_font::get_metrics() const {
        return m_metrics;
    }

    const glyph_spacing& bitmap_font::get_spacing(uint8_t ch) const {
        if (ch < m_first_char || ch > m_last_char) {
            THROW_OUT_OF_RANGE("Character is out of range for font", m_name);
        }
        return m_spacing[ch - m_first_char];
    }

    bitmap_view bitmap_font::get_glyph(uint8_t ch) const {
        if (ch < m_first_char || ch > m_last_char) {
            THROW_OUT_OF_RANGE("Character is out of range for font", m_name);
        }
        const std::size_t idx = ch - m_first_char;
        return m_storage.view(idx);
    }
}
