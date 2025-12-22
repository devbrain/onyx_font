//
// SDL Text Scroller Demo
// Classic demoscene-style horizontal text scroller using onyx_font
// Works with both SDL2 and SDL3
//

#include <onyx_font/font_factory.hh>
#include <../../include/onyx_font/utils/stb_truetype_font.hh>
#include <onyx_font/ttf_font.hh>
#include <onyx_font/text/font_source.hh>
#include <onyx_font/text/glyph_cache.hh>
#include <onyx_font/text/text_renderer.hh>
#include <onyx_font/text/atlas_surface.hh>

#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

// SDL2/SDL3 compatibility
#if defined(USE_SDL3)
    #include <SDL3/SDL.h>
    #define SDL_RENDERER_ACCELERATED 0  // SDL3 doesn't use this flag
    #define SDL_WINDOW_SHOWN 0          // SDL3 doesn't use this flag
#else
    #include <SDL2/SDL.h>
#endif

// ============================================================================
// Configuration
// ============================================================================

constexpr int WINDOW_WIDTH = 800;
constexpr int WINDOW_HEIGHT = 200;
constexpr float SCROLL_SPEED = 120.0f;  // pixels per second
constexpr float FONT_SIZE = 48.0f;

const char* SCROLL_TEXT =
    "Welcome to the ONYX_FONT text scroller demo!  :::  "
    "This library supports TrueType, Bitmap (Windows FON/FNT), "
    "Vector fonts (BGI CHR, Windows Vector FNT), and OS/2 fonts.  :::  "
    "Greetings to all demoscene enthusiasts!  :::  "
    "Press ESC to exit, LEFT/RIGHT to change speed, UP/DOWN to change font size...   ";

// ============================================================================
// Utility functions
// ============================================================================

std::vector<uint8_t> load_file(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        return {};
    }

    auto size = file.tellg();
    if (size <= 0) {
        return {};
    }

    std::vector<uint8_t> data(static_cast<size_t>(size));
    file.seekg(0);
    file.read(reinterpret_cast<char*>(data.data()), size);
    return data;
}

// ============================================================================
// Scroller class
// ============================================================================

class text_scroller {
public:
    text_scroller() = default;
    ~text_scroller() { shutdown(); }

    bool init();
    void run();
    void shutdown();

private:
    void handle_events();
    void update(float dt);
    void render();
    bool load_font(const std::filesystem::path& path);
    void rebuild_cache();
    void render_text_to_surface();

    // SDL resources
    SDL_Window* m_window = nullptr;
    SDL_Renderer* m_renderer = nullptr;
    SDL_Texture* m_texture = nullptr;

    // Text surface (we render to this, then upload to texture)
    std::vector<uint32_t> m_surface;
    int m_surface_width = 0;
    int m_surface_height = 0;

    // Font resources
    std::vector<uint8_t> m_font_data;
    std::optional<onyx_font::bitmap_font> m_bitmap_font;
    std::optional<onyx_font::vector_font> m_vector_font;
    std::optional<onyx_font::ttf_font> m_ttf_font;
    std::unique_ptr<onyx_font::font_source> m_source;
    std::unique_ptr<onyx_font::glyph_cache<onyx_font::memory_atlas>> m_cache;
    std::unique_ptr<onyx_font::text_renderer<onyx_font::memory_atlas>> m_text_renderer;

    // Scroller state
    std::string m_text = SCROLL_TEXT;
    float m_scroll_offset = 0.0f;
    float m_scroll_speed = SCROLL_SPEED;
    float m_font_size = FONT_SIZE;
    float m_text_width = 0.0f;
    uint32_t m_text_color = 0xFF00FFFF;  // Cyan (ARGB)
    bool m_running = true;
    bool m_cache_dirty = true;

    // Color cycling
    float m_hue = 0.0f;
};

bool text_scroller::init() {
    // Initialize SDL
#if defined(USE_SDL3)
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << "\n";
        return false;
    }
#else
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << "\n";
        return false;
    }
#endif

    // Create window
#if defined(USE_SDL3)
    m_window = SDL_CreateWindow("onyx_font Text Scroller",
                                 WINDOW_WIDTH, WINDOW_HEIGHT,
                                 0);
#else
    m_window = SDL_CreateWindow("onyx_font Text Scroller",
                                 SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                 WINDOW_WIDTH, WINDOW_HEIGHT,
                                 SDL_WINDOW_SHOWN);
#endif
    if (!m_window) {
        std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << "\n";
        return false;
    }

    // Create renderer
#if defined(USE_SDL3)
    m_renderer = SDL_CreateRenderer(m_window, nullptr);
#else
    m_renderer = SDL_CreateRenderer(m_window, -1, SDL_RENDERER_ACCELERATED);
#endif
    if (!m_renderer) {
        std::cerr << "SDL_CreateRenderer failed: " << SDL_GetError() << "\n";
        return false;
    }

    // Create texture for text rendering
    m_surface_width = WINDOW_WIDTH * 2;  // Extra width for seamless scrolling
    m_surface_height = WINDOW_HEIGHT;
    m_surface.resize(static_cast<size_t>(m_surface_width * m_surface_height));

#if defined(USE_SDL3)
    m_texture = SDL_CreateTexture(m_renderer,
                                   SDL_PIXELFORMAT_ARGB8888,
                                   SDL_TEXTUREACCESS_STREAMING,
                                   m_surface_width, m_surface_height);
#else
    m_texture = SDL_CreateTexture(m_renderer,
                                   SDL_PIXELFORMAT_ARGB8888,
                                   SDL_TEXTUREACCESS_STREAMING,
                                   m_surface_width, m_surface_height);
#endif
    if (!m_texture) {
        std::cerr << "SDL_CreateTexture failed: " << SDL_GetError() << "\n";
        return false;
    }

    // Enable alpha blending
#if defined(USE_SDL3)
    SDL_SetTextureBlendMode(m_texture, SDL_BLENDMODE_BLEND);
#else
    SDL_SetTextureBlendMode(m_texture, SDL_BLENDMODE_BLEND);
#endif

    // Try to load a font
#ifdef TESTDATA_DIR
    std::filesystem::path testdata = TESTDATA_DIR;

    // Try vector font first (looks great when scaled)
    if (load_font(testdata / "bgi" / "LITT.CHR")) {
        std::cout << "Loaded BGI vector font\n";
    } else if (load_font(testdata / "winfonts" / "ARIAL.TTF")) {
        std::cout << "Loaded Arial TTF\n";
    } else if (load_font(testdata / "winfonts" / "HELVA.FON")) {
        std::cout << "Loaded Helv bitmap font\n";
    } else {
        std::cerr << "Failed to load any font!\n";
        return false;
    }
#else
    std::cerr << "No TESTDATA_DIR defined!\n";
    return false;
#endif

    return true;
}

bool text_scroller::load_font(const std::filesystem::path& path) {
    m_font_data = load_file(path);
    if (m_font_data.empty()) {
        return false;
    }

    auto analysis = onyx_font::font_factory::analyze(m_font_data);
    if (analysis.fonts.empty()) {
        return false;
    }

    const auto& info = analysis.fonts[0];

    // Clear previous font
    m_bitmap_font.reset();
    m_vector_font.reset();
    m_ttf_font.reset();
    m_source.reset();

    switch (info.type) {
        case onyx_font::font_type::BITMAP:
            m_bitmap_font = onyx_font::font_factory::load_bitmap(m_font_data, 0);
            if (m_bitmap_font) {
                m_source = std::make_unique<onyx_font::font_source>(
                    onyx_font::font_source::from_bitmap(*m_bitmap_font));
            }
            break;

        case onyx_font::font_type::VECTOR:
            m_vector_font = onyx_font::font_factory::load_vector(m_font_data, 0);
            if (m_vector_font) {
                m_source = std::make_unique<onyx_font::font_source>(
                    onyx_font::font_source::from_vector(*m_vector_font));
            }
            break;

        case onyx_font::font_type::OUTLINE:
            m_ttf_font.emplace(m_font_data);
            if (m_ttf_font->is_valid()) {
                m_source = std::make_unique<onyx_font::font_source>(
                    onyx_font::font_source::from_ttf(*m_ttf_font));
            }
            break;

        default:
            return false;
    }

    if (!m_source) {
        return false;
    }

    m_cache_dirty = true;
    return true;
}

void text_scroller::rebuild_cache() {
    if (!m_source) {
        return;
    }

    onyx_font::glyph_cache_config config;
    config.atlas_size = 512;
    config.pre_cache_ascii = true;

    m_cache = std::make_unique<onyx_font::glyph_cache<onyx_font::memory_atlas>>(
        std::move(*m_source), m_font_size, config);

    m_text_renderer = std::make_unique<onyx_font::text_renderer<onyx_font::memory_atlas>>(
        *m_cache);

    // Calculate total text width
    m_text_width = m_text_renderer->measure(m_text).width;

    // Resize surface to fit the entire text
    int required_width = static_cast<int>(std::ceil(m_text_width)) + 10;  // +10 for padding
    if (required_width > m_surface_width) {
        m_surface_width = required_width;
        m_surface.resize(static_cast<size_t>(m_surface_width * m_surface_height));

        // Recreate texture with new size
        if (m_texture) {
            SDL_DestroyTexture(m_texture);
        }
#if defined(USE_SDL3)
        m_texture = SDL_CreateTexture(m_renderer,
                                       SDL_PIXELFORMAT_ARGB8888,
                                       SDL_TEXTUREACCESS_STREAMING,
                                       m_surface_width, m_surface_height);
        SDL_SetTextureBlendMode(m_texture, SDL_BLENDMODE_BLEND);
#else
        m_texture = SDL_CreateTexture(m_renderer,
                                       SDL_PIXELFORMAT_ARGB8888,
                                       SDL_TEXTUREACCESS_STREAMING,
                                       m_surface_width, m_surface_height);
        SDL_SetTextureBlendMode(m_texture, SDL_BLENDMODE_BLEND);
#endif
    }

    // Reset scroll position
    m_scroll_offset = static_cast<float>(WINDOW_WIDTH);

    m_cache_dirty = false;

    // Re-render text to surface
    render_text_to_surface();
}

void text_scroller::render_text_to_surface() {
    if (!m_text_renderer || !m_cache) {
        return;
    }

    // Clear surface
    std::fill(m_surface.begin(), m_surface.end(), 0);

    // Get font metrics for vertical centering
    const auto& metrics = m_cache->metrics();
    float baseline_y = (static_cast<float>(m_surface_height) + metrics.ascent - metrics.descent) / 2.0f;

    // Render text to our surface
    m_text_renderer->draw(m_text, 0.0f, baseline_y,
        [this](const onyx_font::memory_atlas& atlas,
               onyx_font::glyph_rect src, float dst_x, float dst_y) {
            // Blit glyph to our surface
            const uint8_t* atlas_data = atlas.data();
            int atlas_width = atlas.width();

            for (int y = 0; y < src.h; ++y) {
                int sy = src.y + y;
                int dy = static_cast<int>(dst_y) + y;

                if (dy < 0 || dy >= m_surface_height) continue;

                for (int x = 0; x < src.w; ++x) {
                    int sx = src.x + x;
                    int dx = static_cast<int>(dst_x) + x;

                    if (dx < 0 || dx >= m_surface_width) continue;

                    uint8_t alpha = atlas_data[sy * atlas_width + sx];
                    if (alpha > 0) {
                        // Store alpha in the surface (we'll colorize during render)
                        size_t idx = static_cast<size_t>(dy * m_surface_width + dx);
                        m_surface[idx] = (static_cast<uint32_t>(alpha) << 24) | 0x00FFFFFF;
                    }
                }
            }
        });
}

// HSV to RGB conversion for color cycling
static uint32_t hsv_to_argb(float h, float s, float v) {
    float c = v * s;
    float x = c * (1.0f - std::fabs(std::fmod(h / 60.0f, 2.0f) - 1.0f));
    float m = v - c;

    float r, g, b;
    if (h < 60) { r = c; g = x; b = 0; }
    else if (h < 120) { r = x; g = c; b = 0; }
    else if (h < 180) { r = 0; g = c; b = x; }
    else if (h < 240) { r = 0; g = x; b = c; }
    else if (h < 300) { r = x; g = 0; b = c; }
    else { r = c; g = 0; b = x; }

    uint8_t rb = static_cast<uint8_t>((r + m) * 255);
    uint8_t gb = static_cast<uint8_t>((g + m) * 255);
    uint8_t bb = static_cast<uint8_t>((b + m) * 255);

    return 0xFF000000 | (rb << 16) | (gb << 8) | bb;
}

void text_scroller::run() {
    Uint64 last_time = SDL_GetTicks();

    while (m_running) {
        Uint64 current_time = SDL_GetTicks();
        float dt = static_cast<float>(current_time - last_time) / 1000.0f;
        last_time = current_time;

        handle_events();
        update(dt);
        render();

        SDL_Delay(16);  // ~60 FPS
    }
}

void text_scroller::handle_events() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
#if defined(USE_SDL3)
        if (event.type == SDL_EVENT_QUIT) {
            m_running = false;
        } else if (event.type == SDL_EVENT_KEY_DOWN) {
            switch (event.key.key) {
                case SDLK_ESCAPE:
                    m_running = false;
                    break;
                case SDLK_LEFT:
                    m_scroll_speed = std::max(20.0f, m_scroll_speed - 20.0f);
                    break;
                case SDLK_RIGHT:
                    m_scroll_speed = std::min(500.0f, m_scroll_speed + 20.0f);
                    break;
                case SDLK_UP:
                    m_font_size = std::min(128.0f, m_font_size + 4.0f);
                    m_cache_dirty = true;
                    break;
                case SDLK_DOWN:
                    m_font_size = std::max(12.0f, m_font_size - 4.0f);
                    m_cache_dirty = true;
                    break;
            }
        }
#else
        if (event.type == SDL_QUIT) {
            m_running = false;
        } else if (event.type == SDL_KEYDOWN) {
            switch (event.key.keysym.sym) {
                case SDLK_ESCAPE:
                    m_running = false;
                    break;
                case SDLK_LEFT:
                    m_scroll_speed = std::max(20.0f, m_scroll_speed - 20.0f);
                    break;
                case SDLK_RIGHT:
                    m_scroll_speed = std::min(500.0f, m_scroll_speed + 20.0f);
                    break;
                case SDLK_UP:
                    m_font_size = std::min(128.0f, m_font_size + 4.0f);
                    m_cache_dirty = true;
                    break;
                case SDLK_DOWN:
                    m_font_size = std::max(12.0f, m_font_size - 4.0f);
                    m_cache_dirty = true;
                    break;
            }
        }
#endif
    }
}

void text_scroller::update(float dt) {
    if (m_cache_dirty) {
        rebuild_cache();
    }

    // Update scroll position
    m_scroll_offset -= m_scroll_speed * dt;

    // Wrap around when text has scrolled off
    if (m_scroll_offset < -m_text_width) {
        m_scroll_offset = static_cast<float>(WINDOW_WIDTH);
    }

    // Update color cycling
    m_hue += 60.0f * dt;  // 60 degrees per second
    if (m_hue >= 360.0f) {
        m_hue -= 360.0f;
    }
    m_text_color = hsv_to_argb(m_hue, 1.0f, 1.0f);
}

void text_scroller::render() {
    // Clear screen with dark background
    SDL_SetRenderDrawColor(m_renderer, 20, 20, 40, 255);
    SDL_RenderClear(m_renderer);

    if (!m_text_renderer) {
        SDL_RenderPresent(m_renderer);
        return;
    }

    // Create a colored version of the text surface
    std::vector<uint32_t> colored_surface(m_surface.size());
    uint8_t r = (m_text_color >> 16) & 0xFF;
    uint8_t g = (m_text_color >> 8) & 0xFF;
    uint8_t b = m_text_color & 0xFF;

    for (size_t i = 0; i < m_surface.size(); ++i) {
        uint8_t alpha = (m_surface[i] >> 24) & 0xFF;
        if (alpha > 0) {
            colored_surface[i] = (static_cast<uint32_t>(alpha) << 24) |
                                 (static_cast<uint32_t>(r) << 16) |
                                 (static_cast<uint32_t>(g) << 8) |
                                 static_cast<uint32_t>(b);
        } else {
            colored_surface[i] = 0;
        }
    }

    // Upload to texture
#if defined(USE_SDL3)
    SDL_UpdateTexture(m_texture, nullptr, colored_surface.data(),
                      m_surface_width * sizeof(uint32_t));
#else
    SDL_UpdateTexture(m_texture, nullptr, colored_surface.data(),
                      m_surface_width * static_cast<int>(sizeof(uint32_t)));
#endif

    // Draw scrolling text
    SDL_Rect src_rect;
    src_rect.x = 0;
    src_rect.y = 0;
    src_rect.w = WINDOW_WIDTH;
    src_rect.h = WINDOW_HEIGHT;

#if defined(USE_SDL3)
    SDL_FRect dst_rect;
    dst_rect.x = m_scroll_offset;
    dst_rect.y = 0;
    dst_rect.w = static_cast<float>(m_surface_width);
    dst_rect.h = static_cast<float>(WINDOW_HEIGHT);

    // Draw main text
    SDL_RenderTexture(m_renderer, m_texture, nullptr, &dst_rect);

    // Draw wrapped copy for seamless scrolling
    if (m_scroll_offset < 0) {
        dst_rect.x = m_scroll_offset + m_text_width;
        SDL_RenderTexture(m_renderer, m_texture, nullptr, &dst_rect);
    }
#else
    SDL_Rect dst_rect;
    dst_rect.x = static_cast<int>(m_scroll_offset);
    dst_rect.y = 0;
    dst_rect.w = m_surface_width;
    dst_rect.h = WINDOW_HEIGHT;

    // Draw main text
    SDL_RenderCopy(m_renderer, m_texture, nullptr, &dst_rect);

    // Draw wrapped copy for seamless scrolling
    if (m_scroll_offset < 0) {
        dst_rect.x = static_cast<int>(m_scroll_offset + m_text_width);
        SDL_RenderCopy(m_renderer, m_texture, nullptr, &dst_rect);
    }
#endif

    SDL_RenderPresent(m_renderer);
}

void text_scroller::shutdown() {
    m_text_renderer.reset();
    m_cache.reset();
    m_source.reset();

    if (m_texture) {
        SDL_DestroyTexture(m_texture);
        m_texture = nullptr;
    }
    if (m_renderer) {
        SDL_DestroyRenderer(m_renderer);
        m_renderer = nullptr;
    }
    if (m_window) {
        SDL_DestroyWindow(m_window);
        m_window = nullptr;
    }

    SDL_Quit();
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    text_scroller scroller;

    if (!scroller.init()) {
        return 1;
    }

    std::cout << "Controls:\n";
    std::cout << "  LEFT/RIGHT - Change scroll speed\n";
    std::cout << "  UP/DOWN    - Change font size\n";
    std::cout << "  ESC        - Exit\n";

    scroller.run();

    return 0;
}
