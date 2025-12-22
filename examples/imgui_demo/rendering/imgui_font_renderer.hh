//
// ImGui Font Renderer - Bridge between onyx_font and ImGui
//
// This module demonstrates how to:
// - Render text from onyx_font into ImGui draw lists
// - Handle texture atlas updates
// - Integrate with ImGui's rendering pipeline
//

#pragma once

#include "gl_atlas.hh"
#include <onyx_font/text/glyph_cache.hh>
#include <onyx_font/text/text_renderer.hh>
#include <onyx_font/text/atlas_surface.hh>

#include <imgui.h>
#include <vector>
#include <cstdint>

namespace imgui_demo {

/// Renders onyx_font text to ImGui
///
/// This class manages the connection between onyx_font's glyph cache
/// and ImGui's rendering system. It uploads atlas textures to OpenGL
/// and provides methods to render text into ImGui draw lists.
///
/// Usage:
/// @code
/// imgui_font_renderer renderer;
/// renderer.set_cache(&cache);  // Set glyph cache
/// renderer.sync_textures();     // Upload any new atlas pages
///
/// // Draw text at position
/// renderer.draw_text("Hello World", 100.0f, 100.0f, 0xFFFFFFFF);
/// @endcode
class imgui_font_renderer {
public:
    imgui_font_renderer() = default;

    /// Set the glyph cache to render from
    /// @param cache Pointer to glyph cache (must remain valid)
    void set_cache(onyx_font::glyph_cache<onyx_font::memory_atlas>* cache);

    /// Synchronize GPU textures with atlas
    /// Call this each frame before rendering
    void sync_textures();

    /// Draw text at position using current draw list
    /// @param text UTF-8 text to render
    /// @param x X position
    /// @param y Y position (top of text)
    /// @param color ABGR color (ImGui format)
    /// @return Width of rendered text
    float draw_text(std::string_view text, float x, float y,
                    uint32_t color = 0xFFFFFFFF);

    /// Draw text with alignment
    /// @param text UTF-8 text
    /// @param x Left edge of alignment box
    /// @param y Top position
    /// @param width Width for alignment
    /// @param align Text alignment
    /// @param color ABGR color
    void draw_text_aligned(std::string_view text, float x, float y,
                           float width, onyx_font::text_align align,
                           uint32_t color = 0xFFFFFFFF);

    /// Draw wrapped text in a box
    /// @param text UTF-8 text
    /// @param box Bounding box
    /// @param align Alignment per line
    /// @param color ABGR color
    /// @return Number of lines drawn
    int draw_text_wrapped(std::string_view text,
                          const onyx_font::text_box& box,
                          onyx_font::text_align align,
                          uint32_t color = 0xFFFFFFFF);

    /// Get texture ID for atlas page (for display)
    /// @param index Atlas page index
    /// @return ImGui texture ID, or nullptr if invalid
    [[nodiscard]] ImTextureID get_texture(int index) const;

    /// Get number of atlas pages
    [[nodiscard]] int atlas_count() const;

    /// Get text renderer (for measurement)
    [[nodiscard]] onyx_font::text_renderer<onyx_font::memory_atlas>*
        get_text_renderer();

private:
    onyx_font::glyph_cache<onyx_font::memory_atlas>* m_cache = nullptr;
    std::unique_ptr<onyx_font::text_renderer<onyx_font::memory_atlas>> m_renderer;
    std::vector<gl_atlas> m_textures;

    /// Blit callback for ImGui rendering
    struct imgui_blit_context {
        imgui_font_renderer* self;
        ImDrawList* draw_list;
        uint32_t color;
        float offset_x;
        float offset_y;
    };

    void blit_glyph(const imgui_blit_context& ctx,
                    const onyx_font::memory_atlas& atlas,
                    onyx_font::glyph_rect src,
                    float dst_x, float dst_y);
};

} // namespace imgui_demo
