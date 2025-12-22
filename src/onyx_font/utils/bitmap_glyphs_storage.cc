//
// Created by igor on 14/12/2025.
//

#include <algorithm>
#include <utility>
#include <failsafe/enforce.hh>
#include <onyx_font/utils/bitmap_glyphs_storage.hh>

namespace onyx_font {
    bitmap_view::bitmap_view() = default;

    std::uint16_t bitmap_view::width() const noexcept { return m_width; }

    std::uint16_t bitmap_view::height() const noexcept { return m_height; }

    std::uint16_t bitmap_view::stride_bytes() const noexcept { return m_stride; }

    bit_order bitmap_view::order() const noexcept { return m_order; }

    std::span <const std::byte> bitmap_view::row(std::uint16_t y) const noexcept {
        ENFORCE(y < m_height);
        return {m_data.data() + static_cast <std::size_t>(y) * m_stride, m_stride};
    }

    bool bitmap_view::pixel(std::uint16_t x, std::uint16_t y) const {
        ENFORCE(x < m_width && y < m_height);
        const std::byte* r = m_data.data() + static_cast <std::size_t>(y) * m_stride;
        const auto b = std::to_integer <std::uint8_t>(r[x >> 3]);
        const std::uint8_t bit = (m_order == bit_order::msb_first)
                                     ? static_cast <std::uint8_t>(7u - (x & 7u))
                                     : static_cast <std::uint8_t>(x & 7u);
        return ((b >> bit) & 1u) != 0u;
    }

    bitmap_view::bitmap_view(std::span <const std::byte> bytes, std::uint16_t w, std::uint16_t h, std::uint16_t stride,
                             bit_order order) noexcept
        : m_data(bytes), m_width(w), m_height(h), m_stride(stride), m_order(order) {
    }

    // =============================================================================
    // Bitmap storage
    // =============================================================================
    bitmap_storage::bitmap_storage() = default;

    bit_order bitmap_storage::order() const noexcept { return m_order; }

    std::size_t bitmap_storage::glyph_count() const noexcept { return m_glyphs.size(); }

    glyph_dimensions bitmap_storage::dimensions(std::size_t glyph_index) const noexcept {
        ENFORCE(glyph_index < m_glyphs.size());
        const auto& g = m_glyphs[glyph_index];
        return {g.width, g.height};
    }

    bitmap_view bitmap_storage::view(std::size_t glyph_index) const noexcept {
        ENFORCE(glyph_index < m_glyphs.size());
        const auto& g = m_glyphs[glyph_index];
        const std::size_t bytes = static_cast <std::size_t>(g.stride) * g.height;
        ENFORCE(g.offset + bytes <= m_blob.size());

        return {
            std::span <const std::byte>(m_blob.data() + g.offset, bytes),
            g.width,
            g.height,
            g.stride,
            m_order
        };
    }

    std::span <const std::byte> bitmap_storage::blob_bytes() const noexcept {
        return {m_blob.data(), m_blob.size()};
    }

    // =================================================================================
    // Bitmap Bilder
    // =================================================================================
    bitmap_builder::bitmap_builder(bit_order order)
        : m_order(order) {
    }

    bit_order bitmap_builder::order() const noexcept { return m_order; }

    void bitmap_builder::reserve_bytes(std::size_t bytes) { m_blob.reserve(bytes); }

    void bitmap_builder::reserve_glyphs(std::size_t n) { m_glyphs.reserve(n); }

    std::size_t bitmap_builder::glyph_writer::glyph_index() const noexcept { return m_glyph_index; }

    std::uint16_t bitmap_builder::glyph_writer::width() const noexcept { return m_width; }

    std::uint16_t bitmap_builder::glyph_writer::height() const noexcept { return m_height; }

    std::uint16_t bitmap_builder::glyph_writer::stride_bytes() const noexcept { return m_stride; }

    bit_order bitmap_builder::glyph_writer::order() const noexcept { return m_order; }

    void bitmap_builder::glyph_writer::set_pixel(std::uint16_t x, std::uint16_t y, bool on) noexcept {
        ENFORCE(x < m_width && y < m_height);
        set_bit(x, y, on);
    }

    void bitmap_builder::glyph_writer::clear_pixel(std::uint16_t x, std::uint16_t y) noexcept {
        set_pixel(x, y, false);
    }

    void bitmap_builder::glyph_writer::set_row_packed(std::uint16_t y, std::span <const std::byte> row) {
        ENFORCE(y < m_height);
        ENFORCE(row.size() == m_stride);
        std::copy(row.begin(), row.end(),
                  m_blob->data() + m_offset + static_cast <std::size_t>(y) * m_stride);
    }

    bitmap_builder::glyph_writer::glyph_writer(std::vector <std::byte>* blob,
                                               std::vector <bitmap_storage::glyph_internal>* glyphs,
                                               std::size_t glyph_index, std::size_t offset,
                                               std::uint16_t w, std::uint16_t h, std::uint16_t stride,
                                               bit_order order) noexcept
        : m_blob(blob)
          , m_glyphs(glyphs)
          , m_glyph_index(glyph_index)
          , m_offset(offset)
          , m_width(w)
          , m_height(h)
          , m_stride(stride)
          , m_order(order) {
    }

    void bitmap_builder::glyph_writer::set_bit(std::uint16_t x, std::uint16_t y, bool on) noexcept {
        const std::size_t idx = m_offset + static_cast <std::size_t>(y) * m_stride + (x >> 3);
        const std::uint8_t bit = (m_order == bit_order::msb_first)
                                     ? static_cast <std::uint8_t>(7u - (x & 7u))
                                     : static_cast <std::uint8_t>(x & 7u);

        auto b = std::to_integer <std::uint8_t>((*m_blob)[idx]);
        const auto mask = static_cast <std::uint8_t>(1u << bit);
        b = on ? (b | mask) : (b & ~mask);
        (*m_blob)[idx] = std::byte{b};
    }

    bitmap_builder::glyph_writer bitmap_builder::reserve_glyph(std::uint16_t width, std::uint16_t height) {
        const std::uint16_t stride = packed_stride(width);
        const std::size_t bytes = static_cast <std::size_t>(height) * stride;

        const std::size_t offset = m_blob.size();
        m_blob.resize(offset + bytes, std::byte{0});

        bitmap_storage::glyph_internal gi;
        gi.width = width;
        gi.height = height;
        gi.offset = offset;
        gi.stride = stride;

        m_glyphs.push_back(gi);
        const std::size_t glyph_index = m_glyphs.size() - 1;

        return {&m_blob, &m_glyphs, glyph_index, offset, width, height, stride, m_order};
    }

    std::size_t bitmap_builder::add_glyph_packed(std::uint16_t width, std::uint16_t height,
                                                 std::span <const std::byte> rows) {
        const std::uint16_t stride = packed_stride(width);
        const std::size_t expected = static_cast <std::size_t>(height) * stride;
        ENFORCE(rows.size() == expected);

        auto w = reserve_glyph(width, height);
        for (std::uint16_t y = 0; y < height; ++y) {
            w.set_row_packed(y, rows.subspan(static_cast <std::size_t>(y) * stride, stride));
        }
        return w.glyph_index();
    }

    bitmap_storage bitmap_builder::build() && {
        bitmap_storage s;
        s.m_order = m_order;
        s.m_blob = std::move(m_blob);
        s.m_glyphs = std::move(m_glyphs);
        return s;
    }
}
