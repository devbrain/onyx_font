//
// GL Atlas Implementation
//

#include "gl_atlas.hh"

#if defined(__APPLE__)
    #include <OpenGL/gl3.h>
#else
    #include <GL/gl.h>
#endif

#include <utility>

namespace imgui_demo {

gl_atlas::~gl_atlas() {
    destroy();
}

gl_atlas::gl_atlas(gl_atlas&& other) noexcept
    : m_texture_id(other.m_texture_id)
    , m_width(other.m_width)
    , m_height(other.m_height) {
    other.m_texture_id = 0;
    other.m_width = 0;
    other.m_height = 0;
}

gl_atlas& gl_atlas::operator=(gl_atlas&& other) noexcept {
    if (this != &other) {
        destroy();
        m_texture_id = other.m_texture_id;
        m_width = other.m_width;
        m_height = other.m_height;
        other.m_texture_id = 0;
        other.m_width = 0;
        other.m_height = 0;
    }
    return *this;
}

void gl_atlas::upload(const onyx_font::memory_atlas& atlas) {
    // Create texture if needed
    if (m_texture_id == 0) {
        glGenTextures(1, &m_texture_id);
    }

    glBindTexture(GL_TEXTURE_2D, m_texture_id);

    // Set texture parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Upload grayscale data as RED channel
    // ImGui shaders use the red channel for font rendering
#if defined(__APPLE__)
    // macOS with core profile
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED,
                 atlas.width(), atlas.height(), 0,
                 GL_RED, GL_UNSIGNED_BYTE, atlas.data());

    // Swizzle so all channels read from red
    GLint swizzle[] = {GL_RED, GL_RED, GL_RED, GL_RED};
    glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swizzle);
#else
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    // Upload as GL_RED and use swizzle so color tinting works properly
    // ImGui multiplies texture color by vertex color, so we need:
    // R,G,B = 1.0 (from vertex color), A = texture value
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED,
                 atlas.width(), atlas.height(), 0,
                 GL_RED, GL_UNSIGNED_BYTE, atlas.data());

    // Set up swizzle: RGB = 1 (ONE), A = RED (the glyph data)
    // This way ImGui's color tinting works: final = vertex_color * tex
    // With tex = (1,1,1,glyph), result = (r,g,b,a*glyph)
    GLint swizzle[] = {GL_ONE, GL_ONE, GL_ONE, GL_RED};
    glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swizzle);
#endif

    m_width = atlas.width();
    m_height = atlas.height();

    glBindTexture(GL_TEXTURE_2D, 0);
}

void gl_atlas::destroy() {
    if (m_texture_id != 0) {
        glDeleteTextures(1, &m_texture_id);
        m_texture_id = 0;
        m_width = 0;
        m_height = 0;
    }
}

} // namespace imgui_demo
