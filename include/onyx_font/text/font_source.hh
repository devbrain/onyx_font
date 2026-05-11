/**
 * @file font_source.hh
 * @brief Unified font source abstraction for all font types.
 *
 * `font_source` is the boundary between low-level font types (bitmap_font,
 * vector_font, ttf_font) and the rendering pipeline (text_rasterizer,
 * glyph_cache, text_renderer). Callers see a single uniform type and never
 * need to know which underlying font shape is in use.
 *
 * The class is PIMPL'd: the public header drags in the lightweight bitmap
 * and vector font headers (their public API is small and always present),
 * but TTF/FreeType state is hidden in the .cc. This means consumers that
 * never call `from_ttf` / `from_ttf_bytes` do not transitively include
 * `<onyx_font/ttf_font.hh>` or `<onyx_font/utils/freetype_font.hh>`, and the
 * `freetype_font` destructor is not synthesised in their translation units.
 *
 * Two ownership models:
 *   - **Borrowing**: `from_bitmap` / `from_vector` / `from_ttf(const ttf_font&)`
 *     store a non-owning pointer; the underlying font must outlive the
 *     font_source.
 *   - **Owning**: `from_ttf_bytes(std::vector<uint8_t>)` moves bytes into the
 *     font_source, which also owns its ttf_font + freetype_font instances.
 *     Use this when the bytes are loaded specifically for the font_source
 *     and have no other owner.
 *
 * @author Igor
 * @date 21/12/2025
 */

#pragma once

#include <onyx_font/export.h>
#include <onyx_font/text/types.hh>
#include <onyx_font/text/raster_target.hh>
#include <onyx_font/text/text_style.hh>
#include <onyx_font/bitmap_font.hh>
#include <onyx_font/vector_font.hh>
#include <cstdint>
#include <memory>
#include <vector>

namespace onyx_font {
    // Forward-declared so the header doesn't require <onyx_font/ttf_font.hh>.
    // Callers that actually invoke `from_ttf(const ttf_font&)` need to
    // include that header themselves.
    class ttf_font;

    /**
     * @brief Unified font wrapper for all font types.
     *
     * Move-only. Constructors are static factories. Access to the
     * underlying type is via `type()`; rasterization dispatches to the
     * right backend internally.
     *
     * Lifetime: for the borrowing factories the source font must outlive
     * the font_source. For the owning `from_ttf_bytes` overload the
     * font_source owns its data.
     */
    class ONYX_FONT_EXPORT font_source {
    public:
        font_source(const font_source&) = delete;
        font_source& operator=(const font_source&) = delete;

        font_source(font_source&&) noexcept;
        font_source& operator=(font_source&&) noexcept;
        ~font_source();

        /// Wrap a non-owning bitmap font reference. The bitmap_font must
        /// outlive this font_source.
        static font_source from_bitmap(const bitmap_font& font);

        /// Wrap a non-owning vector font reference. Same lifetime contract.
        static font_source from_vector(const vector_font& font);

#if defined(ONYX_FONT_HAS_LOADER_TTF)
        /// Wrap a non-owning TTF font reference. The ttf_font must outlive
        /// this font_source. Callers need `<onyx_font/ttf_font.hh>`.
        static font_source from_ttf(const ttf_font& font);

        /// Take ownership of TTF bytes and build the font_source on top.
        /// Bytes are moved into the font_source; no caller-side lifetime
        /// management required. Returns an empty `std::nullopt`-equivalent
        /// font_source on parse failure — check `is_valid()`.
        static font_source from_ttf_bytes(std::vector<std::uint8_t> bytes,
                                          int font_index = 0);
#endif

        /// True if this source was constructed successfully and can serve
        /// glyphs. Use after a fallible factory like `from_ttf_bytes`.
        [[nodiscard]] bool is_valid() const;

        [[nodiscard]] font_source_type type() const;
        [[nodiscard]] bool has_glyph(char32_t codepoint) const;
        [[nodiscard]] char32_t default_char() const;
        [[nodiscard]] scaled_metrics get_scaled_metrics(float size) const;
        [[nodiscard]] glyph_metrics get_glyph_metrics(char32_t codepoint, float size) const;
        [[nodiscard]] float get_kerning(char32_t first, char32_t second, float size) const;

        /// Native pixel height for bitmap fonts; 0 for scalable fonts.
        [[nodiscard]] float native_size() const;

        template<raster_target Target>
        void rasterize_glyph(char32_t codepoint, float size,
                             Target& target, int x, int y) const;

        template<raster_target Target>
        void rasterize_styled_glyph(char32_t codepoint, float size,
                                    Target& target, int x, int y,
                                    const render_style& style) const;

    private:
        struct impl;
        std::unique_ptr<impl> m_impl;

        font_source();

        // Type-erased dispatch (defined in .cc, branches on impl->kind).
        // Templates above wrap the typed raster_target into these.
        void rasterize_dispatch(char32_t codepoint, float size,
                                void* target, int x, int y,
                                void (*put_pixel)(void*, int, int, std::uint8_t),
                                int width, int height) const;

        void rasterize_styled_dispatch(char32_t codepoint, float size,
                                       void* target, int x, int y,
                                       void (*put_pixel)(void*, int, int, std::uint8_t),
                                       int width, int height,
                                       const render_style& style) const;
    };

    template<raster_target Target>
    void font_source::rasterize_glyph(char32_t codepoint, float size,
                                      Target& target, int x, int y) const {
        auto put_pixel = [](void* ctx, int px, int py, std::uint8_t alpha) {
            static_cast<Target*>(ctx)->put_pixel(px, py, alpha);
        };
        rasterize_dispatch(codepoint, size, &target, x, y, put_pixel,
                           target.width(), target.height());
    }

    template<raster_target Target>
    void font_source::rasterize_styled_glyph(char32_t codepoint, float size,
                                             Target& target, int x, int y,
                                             const render_style& style) const {
        auto put_pixel = [](void* ctx, int px, int py, std::uint8_t alpha) {
            static_cast<Target*>(ctx)->put_pixel(px, py, alpha);
        };
        rasterize_styled_dispatch(codepoint, size, &target, x, y, put_pixel,
                                  target.width(), target.height(), style);
    }
} // namespace onyx_font
