//
// GL Atlas - OpenGL texture wrapper for memory_atlas
//
// This module demonstrates how to:
// - Upload glyph atlas data to GPU texture
// - Efficiently update textures when new glyphs are cached
//

#pragma once

#include <onyx_font/text/atlas_surface.hh>
#include <cstdint>

namespace imgui_demo {

/// OpenGL texture wrapper for glyph atlas
///
/// Converts grayscale memory_atlas data to an OpenGL texture
/// that can be used with ImGui or other rendering.
///
/// Usage:
/// @code
/// gl_atlas texture;
/// texture.upload(cache.atlas(0));  // Upload atlas page 0
/// glBindTexture(GL_TEXTURE_2D, texture.id());
/// @endcode
class gl_atlas {
public:
    gl_atlas() = default;
    ~gl_atlas();

    // Non-copyable, movable
    gl_atlas(const gl_atlas&) = delete;
    gl_atlas& operator=(const gl_atlas&) = delete;
    gl_atlas(gl_atlas&& other) noexcept;
    gl_atlas& operator=(gl_atlas&& other) noexcept;

    /// Upload atlas data to GPU
    /// @param atlas Source atlas (grayscale)
    void upload(const onyx_font::memory_atlas& atlas);

    /// Get OpenGL texture ID
    [[nodiscard]] uint32_t id() const noexcept { return m_texture_id; }

    /// Check if texture is valid
    [[nodiscard]] bool valid() const noexcept { return m_texture_id != 0; }

    /// Get texture dimensions
    [[nodiscard]] int width() const noexcept { return m_width; }
    [[nodiscard]] int height() const noexcept { return m_height; }

    /// Delete the texture
    void destroy();

private:
    uint32_t m_texture_id = 0;
    int m_width = 0;
    int m_height = 0;
};

} // namespace imgui_demo
