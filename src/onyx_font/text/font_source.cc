//
// Created by igor on 21/12/2025.
// Rewritten 2026-05-11 to PIMPL the TTF/FreeType dependency.
//

#include <onyx_font/text/font_source.hh>

#if defined(ONYX_FONT_HAS_LOADER_TTF)
#include <onyx_font/ttf_font.hh>
#include <onyx_font/utils/freetype_font.hh>
#endif

#include <euler/dda/line_iterator.hh>
#include <euler/dda/aa_line_iterator.hh>
#include <euler/dda/thick_line_iterator.hh>
#include <euler/coordinates/point2.hh>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <utility>

namespace onyx_font {

// ---------------------------------------------------------------------------
// Internal storage.
//
// `kind` discriminates the three legal shapes. Borrowed cases hold raw
// pointers; the owned TTF case holds the byte buffer + ttf_font + freetype
// rasterizer. This is the only place in the library that names freetype_font
// directly, so disabling LOADER_TTF removes the entire dependency chain
// without touching public headers.
// ---------------------------------------------------------------------------
struct font_source::impl {
    enum class kind { none, bitmap, vector, ttf };

    kind k = kind::none;

    const bitmap_font* bm = nullptr;
    const vector_font* vec = nullptr;

#if defined(ONYX_FONT_HAS_LOADER_TTF)
    const ttf_font* tt = nullptr;                  ///< borrowing variant
    std::vector<std::uint8_t> owned_bytes;         ///< owning variant: TTF bytes
    std::unique_ptr<ttf_font> owned_ttf;           ///< owning variant: parsed TTF
    std::unique_ptr<freetype_font> rasterizer;     ///< present iff k == ttf
#endif
};

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------
font_source::font_source() : m_impl(std::make_unique<impl>()) {}
font_source::~font_source() = default;
font_source::font_source(font_source&&) noexcept = default;
font_source& font_source::operator=(font_source&&) noexcept = default;

font_source font_source::from_bitmap(const bitmap_font& font) {
    font_source source;
    source.m_impl->k = impl::kind::bitmap;
    source.m_impl->bm = &font;
    return source;
}

font_source font_source::from_vector(const vector_font& font) {
    font_source source;
    source.m_impl->k = impl::kind::vector;
    source.m_impl->vec = &font;
    return source;
}

#if defined(ONYX_FONT_HAS_LOADER_TTF)
font_source font_source::from_ttf(const ttf_font& font) {
    font_source source;
    source.m_impl->k = impl::kind::ttf;
    source.m_impl->tt = &font;
    source.m_impl->rasterizer = std::make_unique<freetype_font>(
        font.data(), font.font_index());
    return source;
}

font_source font_source::from_ttf_bytes(std::vector<std::uint8_t> bytes,
                                        int font_index) {
    font_source source;
    if (bytes.empty()) {
        return source;  // is_valid() == false
    }
    try {
        source.m_impl->owned_bytes = std::move(bytes);
        std::span<const std::uint8_t> data(source.m_impl->owned_bytes.data(),
                                           source.m_impl->owned_bytes.size());
        source.m_impl->owned_ttf = std::make_unique<ttf_font>(data, font_index);
        if (!source.m_impl->owned_ttf->is_valid()) {
            source.m_impl->owned_ttf.reset();
            source.m_impl->owned_bytes.clear();
            return source;
        }
        source.m_impl->rasterizer = std::make_unique<freetype_font>(
            data, font_index);
        source.m_impl->tt = source.m_impl->owned_ttf.get();
        source.m_impl->k = impl::kind::ttf;
    } catch (...) {
        source.m_impl->owned_ttf.reset();
        source.m_impl->rasterizer.reset();
        source.m_impl->owned_bytes.clear();
        source.m_impl->k = impl::kind::none;
    }
    return source;
}
#endif  // ONYX_FONT_HAS_LOADER_TTF

bool font_source::is_valid() const {
    return m_impl && m_impl->k != impl::kind::none;
}

font_source_type font_source::type() const {
    switch (m_impl->k) {
        case impl::kind::bitmap: return font_source_type::bitmap;
        case impl::kind::vector: return font_source_type::vector;
        case impl::kind::ttf:    return font_source_type::outline;
        case impl::kind::none:   break;
    }
    return font_source_type::bitmap;  // arbitrary default; is_valid() is the gate
}

// ---------------------------------------------------------------------------
// Metrics / queries
// ---------------------------------------------------------------------------
bool font_source::has_glyph(char32_t codepoint) const {
    switch (m_impl->k) {
        case impl::kind::bitmap: {
            if (codepoint > 255) return false;
            const auto& font = *m_impl->bm;
            auto ch = static_cast<std::uint8_t>(codepoint);
            return ch >= font.get_first_char() && ch <= font.get_last_char();
        }
        case impl::kind::vector: {
            if (codepoint > 255) return false;
            return m_impl->vec->has_glyph(static_cast<std::uint8_t>(codepoint));
        }
        case impl::kind::ttf:
#if defined(ONYX_FONT_HAS_LOADER_TTF)
            return m_impl->tt->has_glyph(static_cast<std::uint32_t>(codepoint));
#else
            return false;
#endif
        case impl::kind::none:
            return false;
    }
    return false;
}

char32_t font_source::default_char() const {
    switch (m_impl->k) {
        case impl::kind::bitmap: return m_impl->bm->get_default_char();
        case impl::kind::vector: return m_impl->vec->get_default_char();
        case impl::kind::ttf:    return U'?';
        case impl::kind::none:   return U'?';
    }
    return U'?';
}

scaled_metrics font_source::get_scaled_metrics(float size) const {
    scaled_metrics result;

    switch (m_impl->k) {
        case impl::kind::bitmap: {
            const auto& metrics = m_impl->bm->get_metrics();
            result.ascent = static_cast<float>(metrics.ascent);
            result.descent = static_cast<float>(metrics.pixel_height - metrics.ascent);
            result.line_gap = static_cast<float>(metrics.external_leading);
            result.line_height = static_cast<float>(metrics.pixel_height + metrics.external_leading);
            break;
        }
        case impl::kind::vector: {
            const auto& metrics = m_impl->vec->get_metrics();
            float scale = size / static_cast<float>(metrics.pixel_height);
            result.ascent = static_cast<float>(metrics.ascent) * scale;
            result.descent = static_cast<float>(-metrics.descent) * scale;
            result.line_gap = 0;
            result.line_height = size;
            break;
        }
        case impl::kind::ttf: {
#if defined(ONYX_FONT_HAS_LOADER_TTF)
            auto ttf_metrics = m_impl->tt->get_metrics(size);
            result.ascent = ttf_metrics.ascent;
            result.descent = -ttf_metrics.descent;
            result.line_gap = ttf_metrics.line_gap;
            result.line_height = result.ascent + result.descent + result.line_gap;
#endif
            break;
        }
        case impl::kind::none:
            break;
    }

    return result;
}

glyph_metrics font_source::get_glyph_metrics(char32_t codepoint, float size) const {
    glyph_metrics result;

    switch (m_impl->k) {
        case impl::kind::bitmap: {
            if (codepoint > 255) return result;
            const auto& font = *m_impl->bm;
            auto ch = static_cast<std::uint8_t>(codepoint);

            if (ch < font.get_first_char() || ch > font.get_last_char()) {
                ch = font.get_default_char();
                if (ch < font.get_first_char() || ch > font.get_last_char()) {
                    return result;
                }
            }

            const auto& spacing = font.get_spacing(ch);
            bitmap_view glyph = font.get_glyph(ch);

            result.width = static_cast<float>(glyph.width());
            result.height = static_cast<float>(glyph.height());
            result.bearing_x = spacing.a_space ? static_cast<float>(*spacing.a_space) : 0.0f;
            result.bearing_y = static_cast<float>(font.get_metrics().ascent);

            float advance = 0;
            if (spacing.a_space) advance += static_cast<float>(*spacing.a_space);
            if (spacing.b_space) {
                advance += static_cast<float>(*spacing.b_space);
            } else {
                advance += static_cast<float>(glyph.width());
            }
            if (spacing.c_space) advance += static_cast<float>(*spacing.c_space);
            result.advance_x = advance;
            break;
        }
        case impl::kind::vector: {
            if (codepoint > 255) return result;
            const auto& font = *m_impl->vec;
            auto ch = static_cast<std::uint8_t>(codepoint);

            const vector_glyph* glyph = font.get_glyph(ch);
            if (!glyph) {
                glyph = font.get_glyph(font.get_default_char());
                if (!glyph) return result;
            }

            const auto& metrics = font.get_metrics();
            float scale = size / static_cast<float>(metrics.pixel_height);

            result.advance_x = static_cast<float>(glyph->width) * scale;
            result.bearing_x = 0;

            int min_x = 0, min_y = 0, max_x = 0, max_y = 0;
            int pen_x = 0, pen_y = 0;
            for (const auto& cmd : glyph->strokes) {
                if (cmd.type != stroke_type::END) {
                    pen_x += cmd.dx;
                    pen_y += cmd.dy;
                    min_x = std::min(min_x, pen_x);
                    min_y = std::min(min_y, pen_y);
                    max_x = std::max(max_x, pen_x);
                    max_y = std::max(max_y, pen_y);
                }
            }

            result.width = static_cast<float>(max_x - min_x + 1) * scale;
            result.height = static_cast<float>(max_y - min_y + 1) * scale;
            result.bearing_y = static_cast<float>(-min_y) * scale;
            break;
        }
        case impl::kind::ttf: {
#if defined(ONYX_FONT_HAS_LOADER_TTF)
            auto ttf_metrics = m_impl->tt->get_glyph_metrics(
                static_cast<std::uint32_t>(codepoint), size);
            if (ttf_metrics) {
                result.advance_x = ttf_metrics->advance_x;
                result.bearing_x = ttf_metrics->bearing_x;
                result.bearing_y = ttf_metrics->bearing_y;
                result.width = ttf_metrics->width;
                result.height = ttf_metrics->height;
            }
#endif
            break;
        }
        case impl::kind::none:
            break;
    }

    return result;
}

float font_source::get_kerning(char32_t first, char32_t second, float size) const {
#if defined(ONYX_FONT_HAS_LOADER_TTF)
    if (m_impl->k == impl::kind::ttf) {
        return m_impl->tt->get_kerning(
            static_cast<std::uint32_t>(first),
            static_cast<std::uint32_t>(second),
            size);
    }
#else
    (void)first; (void)second; (void)size;
#endif
    return 0.0f;
}

float font_source::native_size() const {
    if (m_impl->k == impl::kind::bitmap) {
        return static_cast<float>(m_impl->bm->get_metrics().pixel_height);
    }
    return 0.0f;
}

// ---------------------------------------------------------------------------
// Rasterization
// ---------------------------------------------------------------------------
namespace {

void draw_line_aa(void* target, float x0, float y0, float x1, float y1,
                  void (*put_pixel)(void*, int, int, std::uint8_t),
                  int width, int height) {
    euler::point2f start{x0, y0};
    euler::point2f end{x1, y1};

    auto line = euler::dda::make_aa_line_iterator(start, end);
    while (line != euler::dda::aa_line_iterator<float>::end()) {
        auto pixel = *line;
        int px = static_cast<int>(pixel.pos.x);
        int py = static_cast<int>(pixel.pos.y);
        if (px >= 0 && px < width && py >= 0 && py < height) {
            std::uint8_t alpha = static_cast<std::uint8_t>(
                std::clamp(pixel.coverage * 255.0f, 0.0f, 255.0f));
            if (alpha > 0) {
                put_pixel(target, px, py, alpha);
            }
        }
        ++line;
    }
}

void draw_line_thick(void* target, float x0, float y0, float x1, float y1,
                     float thickness,
                     void (*put_pixel)(void*, int, int, std::uint8_t),
                     int width, int height) {
    euler::point2f start{x0, y0};
    euler::point2f end{x1, y1};

    auto line = euler::dda::thick_line_iterator<float>(start, end, thickness);
    while (line != euler::dda::thick_line_iterator<float>::end()) {
        auto pixel = *line;
        int px = pixel.pos.x;
        int py = pixel.pos.y;
        if (px >= 0 && px < width && py >= 0 && py < height) {
            put_pixel(target, px, py, 255);
        }
        ++line;
    }
}

// shear_x = x + shear * (origin_y - y); points above baseline shift right
inline float apply_shear(float x, float y, float origin_y, float shear) {
    return x + shear * (origin_y - y);
}

void rasterize_bitmap(const bitmap_font& font, char32_t codepoint,
                      void* target, int x, int y,
                      void (*put_pixel)(void*, int, int, std::uint8_t)) {
    if (codepoint > 255) return;
    auto ch = static_cast<std::uint8_t>(codepoint);

    if (ch < font.get_first_char() || ch > font.get_last_char()) {
        ch = font.get_default_char();
        if (ch < font.get_first_char() || ch > font.get_last_char()) {
            return;
        }
    }

    const auto& spacing = font.get_spacing(ch);
    bitmap_view glyph = font.get_glyph(ch);

    int glyph_x = x;
    if (spacing.a_space) {
        glyph_x += *spacing.a_space;
    }

    int glyph_y = y - static_cast<int>(font.get_metrics().ascent);

    for (std::uint16_t gy = 0; gy < glyph.height(); ++gy) {
        for (std::uint16_t gx = 0; gx < glyph.width(); ++gx) {
            if (glyph.pixel(gx, gy)) {
                put_pixel(target, glyph_x + gx, glyph_y + gy, 255);
            }
        }
    }
}

void rasterize_vector(const vector_font& font, char32_t codepoint, float size,
                      void* target, int x, int y,
                      void (*put_pixel)(void*, int, int, std::uint8_t),
                      int width, int height) {
    if (codepoint > 255) return;
    auto ch = static_cast<std::uint8_t>(codepoint);

    const vector_glyph* glyph = font.get_glyph(ch);
    if (!glyph) {
        glyph = font.get_glyph(font.get_default_char());
        if (!glyph) return;
    }

    const auto& metrics = font.get_metrics();
    float scale = size / static_cast<float>(metrics.pixel_height);

    float origin_x = static_cast<float>(x);
    float origin_y = static_cast<float>(y);
    (void)origin_x;

    float pen_x = origin_x;
    float pen_y = origin_y;
    bool pen_down = true;

    for (const auto& cmd : glyph->strokes) {
        switch (cmd.type) {
            case stroke_type::MOVE_TO: {
                pen_x += static_cast<float>(cmd.dx) * scale;
                pen_y += static_cast<float>(cmd.dy) * scale;
                pen_down = true;
                break;
            }
            case stroke_type::LINE_TO: {
                float new_x = pen_x + static_cast<float>(cmd.dx) * scale;
                float new_y = pen_y + static_cast<float>(cmd.dy) * scale;
                if (pen_down) {
                    draw_line_aa(target, pen_x, pen_y, new_x, new_y,
                                 put_pixel, width, height);
                }
                pen_x = new_x;
                pen_y = new_y;
                break;
            }
            case stroke_type::END:
                pen_down = false;
                break;
        }
    }
}

void rasterize_styled_vector(const vector_font& font, char32_t codepoint, float size,
                             void* target, int x, int y,
                             void (*put_pixel)(void*, int, int, std::uint8_t),
                             int width, int height,
                             const render_style& style) {
    if (codepoint > 255) return;
    auto ch = static_cast<std::uint8_t>(codepoint);

    const vector_glyph* glyph = font.get_glyph(ch);
    if (!glyph) {
        glyph = font.get_glyph(font.get_default_char());
        if (!glyph) return;
    }

    const auto& metrics = font.get_metrics();
    float scale = size / static_cast<float>(metrics.pixel_height);

    float origin_x = static_cast<float>(x);
    float origin_y = static_cast<float>(y);

    float thickness = 1.0f;
    if (style.is_bold()) {
        int bold_strength = style.bold_strength;
        if (bold_strength <= 0) {
            bold_strength = calc_bold_strength(size);
        }
        thickness = 1.0f + static_cast<float>(bold_strength);
    }

    float shear = style.is_italic() ? style.italic_shear : 0.0f;

    float pen_x = origin_x;
    float pen_y = origin_y;
    bool pen_down = true;

    for (const auto& cmd : glyph->strokes) {
        switch (cmd.type) {
            case stroke_type::MOVE_TO: {
                pen_x += static_cast<float>(cmd.dx) * scale;
                pen_y += static_cast<float>(cmd.dy) * scale;
                pen_down = true;
                break;
            }
            case stroke_type::LINE_TO: {
                float new_x = pen_x + static_cast<float>(cmd.dx) * scale;
                float new_y = pen_y + static_cast<float>(cmd.dy) * scale;
                if (pen_down) {
                    float x0 = shear != 0.0f ? apply_shear(pen_x, pen_y, origin_y, shear) : pen_x;
                    float y0 = pen_y;
                    float x1 = shear != 0.0f ? apply_shear(new_x, new_y, origin_y, shear) : new_x;
                    float y1 = new_y;

                    if (thickness > 1.0f) {
                        draw_line_thick(target, x0, y0, x1, y1, thickness,
                                        put_pixel, width, height);
                    } else {
                        draw_line_aa(target, x0, y0, x1, y1,
                                     put_pixel, width, height);
                    }
                }
                pen_x = new_x;
                pen_y = new_y;
                break;
            }
            case stroke_type::END:
                pen_down = false;
                break;
        }
    }
}

#if defined(ONYX_FONT_HAS_LOADER_TTF)
void rasterize_ttf(const freetype_font& rast, char32_t codepoint, float size,
                   void* target, int x, int y,
                   void (*put_pixel)(void*, int, int, std::uint8_t)) {
    if (!rast.is_valid()) return;

    auto bitmap = rast.rasterize(static_cast<std::uint32_t>(codepoint), size);
    if (!bitmap) return;

    int glyph_y = y + bitmap->offset_y;

    for (int gy = 0; gy < bitmap->height; ++gy) {
        for (int gx = 0; gx < bitmap->width; ++gx) {
            std::uint8_t alpha = bitmap->bitmap[static_cast<std::size_t>(gy * bitmap->width + gx)];
            if (alpha > 0) {
                put_pixel(target, x + gx, glyph_y + gy, alpha);
            }
        }
    }
}

void rasterize_styled_ttf(const freetype_font& rast, char32_t codepoint, float size,
                          void* target, int x, int y,
                          void (*put_pixel)(void*, int, int, std::uint8_t),
                          const render_style& style) {
    if (!rast.is_valid()) return;

    ft_render_style ft_style;
    ft_style.bold = style.is_bold();
    ft_style.italic = style.is_italic();
    ft_style.italic_skew = style.italic_shear;
    ft_style.bold_strength = style.bold_strength;

    auto bitmap = rast.rasterize_styled(
        static_cast<std::uint32_t>(codepoint), size, ft_style);
    if (!bitmap) return;

    int glyph_y = y + bitmap->offset_y;

    for (int gy = 0; gy < bitmap->height; ++gy) {
        for (int gx = 0; gx < bitmap->width; ++gx) {
            std::uint8_t alpha = bitmap->bitmap[static_cast<std::size_t>(gy * bitmap->width + gx)];
            if (alpha > 0) {
                put_pixel(target, x + gx, glyph_y + gy, alpha);
            }
        }
    }
}
#endif  // ONYX_FONT_HAS_LOADER_TTF

} // anonymous namespace

void font_source::rasterize_dispatch(char32_t codepoint, float size,
                                     void* target, int x, int y,
                                     void (*put_pixel)(void*, int, int, std::uint8_t),
                                     int width, int height) const {
    switch (m_impl->k) {
        case impl::kind::bitmap:
            rasterize_bitmap(*m_impl->bm, codepoint, target, x, y, put_pixel);
            return;
        case impl::kind::vector:
            rasterize_vector(*m_impl->vec, codepoint, size,
                             target, x, y, put_pixel, width, height);
            return;
        case impl::kind::ttf:
#if defined(ONYX_FONT_HAS_LOADER_TTF)
            rasterize_ttf(*m_impl->rasterizer, codepoint, size,
                          target, x, y, put_pixel);
#endif
            return;
        case impl::kind::none:
            return;
    }
}

void font_source::rasterize_styled_dispatch(char32_t codepoint, float size,
                                            void* target, int x, int y,
                                            void (*put_pixel)(void*, int, int, std::uint8_t),
                                            int width, int height,
                                            const render_style& style) const {
    switch (m_impl->k) {
        case impl::kind::bitmap:
            // Bitmap fonts ignore styling.
            rasterize_bitmap(*m_impl->bm, codepoint, target, x, y, put_pixel);
            return;
        case impl::kind::vector:
            if (style.needs_glyph_transform()) {
                rasterize_styled_vector(*m_impl->vec, codepoint, size,
                                        target, x, y, put_pixel,
                                        width, height, style);
            } else {
                rasterize_vector(*m_impl->vec, codepoint, size,
                                 target, x, y, put_pixel, width, height);
            }
            return;
        case impl::kind::ttf:
#if defined(ONYX_FONT_HAS_LOADER_TTF)
            if (style.needs_glyph_transform()) {
                rasterize_styled_ttf(*m_impl->rasterizer, codepoint, size,
                                     target, x, y, put_pixel, style);
            } else {
                rasterize_ttf(*m_impl->rasterizer, codepoint, size,
                              target, x, y, put_pixel);
            }
#endif
            return;
        case impl::kind::none:
            return;
    }
}

} // namespace onyx_font
