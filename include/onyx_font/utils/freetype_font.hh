//
// Created by igor on 30/12/2025.
//
// FreeType wrapper for high-quality TTF/OTF rendering with proper
// hinting, bold synthesis, and oblique transforms.
//

#pragma once

#include <onyx_font/export.h>
#include <cstdint>
#include <memory>
#include <vector>
#include <span>
#include <optional>

namespace onyx_font {

    /**
     * @brief Rasterized glyph bitmap from FreeType.
     */
    struct ONYX_FONT_EXPORT ft_glyph_bitmap {
        std::vector<uint8_t> bitmap;  ///< Grayscale bitmap (0-255)
        int width = 0;                ///< Bitmap width in pixels
        int height = 0;               ///< Bitmap height in pixels
        int offset_x = 0;             ///< X offset from pen position
        int offset_y = 0;             ///< Y offset from baseline (negative = above)
        float advance_x = 0;          ///< Horizontal advance
    };

    /**
     * @brief Style options for FreeType rendering.
     */
    struct ONYX_FONT_EXPORT ft_render_style {
        bool bold = false;            ///< Apply synthetic bold
        bool italic = false;          ///< Apply synthetic italic (oblique)
        float italic_skew = 0.2f;     ///< Skew factor for italic (tan of angle)
        int bold_strength = 0;        ///< Bold strength (0 = auto based on size)
    };

    /**
     * @brief FreeType-based font rasterizer.
     *
     * Provides high-quality font rendering with proper hinting,
     * synthetic bold, and oblique transforms.
     */
    class ONYX_FONT_EXPORT freetype_font {
    public:
        /**
         * @brief Construct from font file data.
         *
         * @param data Font file contents (must remain valid)
         * @param font_index Index within TTC collection (0 for regular TTF/OTF)
         */
        explicit freetype_font(std::span<const uint8_t> data, int font_index = 0);

        ~freetype_font();

        freetype_font(freetype_font&& other) noexcept;
        freetype_font& operator=(freetype_font&& other) noexcept;

        freetype_font(const freetype_font&) = delete;
        freetype_font& operator=(const freetype_font&) = delete;

        /**
         * @brief Check if font was loaded successfully.
         */
        [[nodiscard]] bool is_valid() const;

        /**
         * @brief Rasterize a glyph to a grayscale bitmap.
         *
         * @param codepoint Unicode codepoint
         * @param pixel_height Desired height in pixels
         * @return Rasterized glyph data, or nullopt if glyph not found
         */
        [[nodiscard]] std::optional<ft_glyph_bitmap> rasterize(
            uint32_t codepoint,
            float pixel_height
        ) const;

        /**
         * @brief Rasterize a glyph with styling (bold/italic).
         *
         * Uses FreeType's FT_GlyphSlot_Embolden for bold and
         * FT_Set_Transform for italic skew.
         *
         * @param codepoint Unicode codepoint
         * @param pixel_height Desired height in pixels
         * @param style Rendering style options
         * @return Rasterized glyph data, or nullopt if glyph not found
         */
        [[nodiscard]] std::optional<ft_glyph_bitmap> rasterize_styled(
            uint32_t codepoint,
            float pixel_height,
            const ft_render_style& style
        ) const;

        /**
         * @brief Get scale factor for a given pixel height.
         */
        [[nodiscard]] float get_scale_for_pixel_height(float pixel_height) const;

        /**
         * @brief Get font ascender in font units.
         */
        [[nodiscard]] int get_ascender() const;

        /**
         * @brief Get font descender in font units (negative).
         */
        [[nodiscard]] int get_descender() const;

        /**
         * @brief Get font height in font units.
         */
        [[nodiscard]] int get_height() const;

        /**
         * @brief Get units per EM.
         */
        [[nodiscard]] int get_units_per_em() const;

        /**
         * @brief Get kerning between two characters.
         *
         * @param left Left character codepoint
         * @param right Right character codepoint
         * @param pixel_height Pixel height for scaling
         * @return Kerning adjustment in pixels
         */
        [[nodiscard]] float get_kerning(uint32_t left, uint32_t right, float pixel_height) const;

        /**
         * @brief Get number of fonts in a TTC collection.
         */
        [[nodiscard]] static int get_font_count(std::span<const uint8_t> data);

    private:
        struct impl;
        std::unique_ptr<impl> m_impl;
    };

} // namespace onyx_font
