//
// ImGui Font Renderer Implementation
//

#include "imgui_font_renderer.hh"

namespace imgui_demo {

void imgui_font_renderer::set_cache(
    onyx_font::glyph_cache<onyx_font::memory_atlas>* cache) {
    m_cache = cache;

    if (m_cache) {
        m_renderer = std::make_unique<
            onyx_font::text_renderer<onyx_font::memory_atlas>>(*m_cache);
    } else {
        m_renderer.reset();
    }

    // Clear textures - will be rebuilt on next sync
    m_textures.clear();
}

void imgui_font_renderer::sync_textures() {
    if (!m_cache) {
        return;
    }

    int count = m_cache->atlas_count();

    // Add new textures if atlas grew
    while (static_cast<int>(m_textures.size()) < count) {
        m_textures.emplace_back();
    }

    // Upload all atlas pages
    // In production, you might want to track dirty pages
    for (int i = 0; i < count; ++i) {
        m_textures[static_cast<std::size_t>(i)].upload(m_cache->atlas(i));
    }
}

float imgui_font_renderer::draw_text(std::string_view text, float x, float y,
                                      uint32_t color) {
    if (!m_renderer || text.empty()) {
        return 0.0f;
    }

    sync_textures();

    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    imgui_blit_context ctx{this, draw_list, color, 0.0f, 0.0f};

    return m_renderer->draw(text, x, y,
        [&ctx](const onyx_font::memory_atlas& atlas,
               onyx_font::glyph_rect src, float dst_x, float dst_y) {
            ctx.self->blit_glyph(ctx, atlas, src, dst_x, dst_y);
        });
}

void imgui_font_renderer::draw_text_aligned(std::string_view text,
                                             float x, float y,
                                             float width,
                                             onyx_font::text_align align,
                                             uint32_t color) {
    if (!m_renderer || text.empty()) {
        return;
    }

    sync_textures();

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    imgui_blit_context ctx{this, draw_list, color, 0.0f, 0.0f};

    m_renderer->draw_aligned(text, x, y, width, align,
        [&ctx](const onyx_font::memory_atlas& atlas,
               onyx_font::glyph_rect src, float dst_x, float dst_y) {
            ctx.self->blit_glyph(ctx, atlas, src, dst_x, dst_y);
        });
}

int imgui_font_renderer::draw_text_wrapped(std::string_view text,
                                            const onyx_font::text_box& box,
                                            onyx_font::text_align align,
                                            uint32_t color) {
    if (!m_renderer || text.empty()) {
        return 0;
    }

    sync_textures();

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    imgui_blit_context ctx{this, draw_list, color, 0.0f, 0.0f};

    return m_renderer->draw_wrapped(text, box, align,
        [&ctx](const onyx_font::memory_atlas& atlas,
               onyx_font::glyph_rect src, float dst_x, float dst_y) {
            ctx.self->blit_glyph(ctx, atlas, src, dst_x, dst_y);
        });
}

ImTextureID imgui_font_renderer::get_texture(int index) const {
    if (index < 0 || static_cast<std::size_t>(index) >= m_textures.size()) {
        return static_cast<ImTextureID>(0);
    }

    const auto& tex = m_textures[static_cast<std::size_t>(index)];
    if (!tex.valid()) {
        return static_cast<ImTextureID>(0);
    }

    // Convert OpenGL texture ID to ImTextureID
    return static_cast<ImTextureID>(tex.id());
}

int imgui_font_renderer::atlas_count() const {
    return m_cache ? m_cache->atlas_count() : 0;
}

onyx_font::text_renderer<onyx_font::memory_atlas>*
imgui_font_renderer::get_text_renderer() {
    return m_renderer.get();
}

void imgui_font_renderer::blit_glyph(
    const imgui_blit_context& ctx,
    const onyx_font::memory_atlas& atlas,
    onyx_font::glyph_rect src,
    float dst_x, float dst_y) {

    // Skip empty glyphs (like space)
    if (src.w <= 0 || src.h <= 0) {
        return;
    }

    // Find the texture for this atlas
    // We use the address to find the correct texture
    int atlas_idx = -1;
    if (m_cache) {
        for (int i = 0; i < m_cache->atlas_count(); ++i) {
            if (&m_cache->atlas(i) == &atlas) {
                atlas_idx = i;
                break;
            }
        }
    }

    if (atlas_idx < 0 ||
        static_cast<std::size_t>(atlas_idx) >= m_textures.size()) {
        return;
    }

    const auto& tex = m_textures[static_cast<std::size_t>(atlas_idx)];
    if (!tex.valid()) {
        return;
    }

    // Calculate UV coordinates
    float inv_w = 1.0f / static_cast<float>(tex.width());
    float inv_h = 1.0f / static_cast<float>(tex.height());

    ImVec2 uv0(static_cast<float>(src.x) * inv_w,
               static_cast<float>(src.y) * inv_h);
    ImVec2 uv1(static_cast<float>(src.x + src.w) * inv_w,
               static_cast<float>(src.y + src.h) * inv_h);

    // Destination rectangle
    ImVec2 p0(dst_x + ctx.offset_x, dst_y + ctx.offset_y);
    ImVec2 p1(p0.x + static_cast<float>(src.w),
              p0.y + static_cast<float>(src.h));

    // Draw textured quad
    ImTextureID tex_id = static_cast<ImTextureID>(tex.id());

    ctx.draw_list->AddImage(tex_id, p0, p1, uv0, uv1, ctx.color);
}

} // namespace imgui_demo
