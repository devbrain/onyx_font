//
// Font Manager - Handles loading and managing fonts
//
// This module demonstrates how to:
// - Load fonts from files using font_factory
// - Create font_source from different font types
// - Manage font data lifetime
//

#pragma once

#include <onyx_font/font_factory.hh>
#include <onyx_font/text/font_source.hh>
#include <onyx_font/text/glyph_cache.hh>
#include <onyx_font/text/atlas_surface.hh>
#include <onyx_font/text/text_renderer.hh>

#include <memory>
#include <string>
#include <vector>
#include <optional>
#include <filesystem>

namespace imgui_demo {

/// Information about a loaded font
struct loaded_font_info {
    std::string name;                           ///< Display name
    std::string path;                           ///< File path
    onyx_font::font_source_type type;           ///< Font type (bitmap/vector/outline)
    float current_size = 24.0f;                 ///< Current rendering size
    bool scalable = true;                       ///< Whether font can be scaled
};

/// Manages font loading and provides access to renderers
///
/// Usage example:
/// @code
/// font_manager mgr;
/// mgr.load_font("/path/to/font.ttf");
/// if (auto* renderer = mgr.get_renderer(0)) {
///     renderer->draw("Hello", x, y, blit_callback);
/// }
/// @endcode
class font_manager {
public:
    font_manager() = default;

    /// Load a font from file
    /// @param path Path to font file (.ttf, .otf, .fon, .fnt, .chr)
    /// @param font_index Index within file (for collections)
    /// @return true if loaded successfully
    bool load_font(const std::filesystem::path& path, int font_index = 0);

    /// Load font from memory
    /// @param name Display name for the font
    /// @param data Font file data (will be copied)
    /// @param font_index Index within file
    /// @return true if loaded successfully
    bool load_font_memory(const std::string& name,
                          std::span<const uint8_t> data,
                          int font_index = 0);

    /// Remove a loaded font
    void remove_font(std::size_t index);

    /// Get number of loaded fonts
    [[nodiscard]] std::size_t font_count() const noexcept {
        return m_fonts.size();
    }

    /// Get font info
    [[nodiscard]] const loaded_font_info* get_info(std::size_t index) const;

    /// Get text renderer for a font
    [[nodiscard]] onyx_font::text_renderer<onyx_font::memory_atlas>*
        get_renderer(std::size_t index);

    /// Get glyph cache for a font (for atlas access)
    [[nodiscard]] onyx_font::glyph_cache<onyx_font::memory_atlas>*
        get_cache(std::size_t index);

    /// Update font size (recreates cache/renderer)
    /// @param index Font index
    /// @param size New size in pixels
    /// @return true if size was changed
    bool set_font_size(std::size_t index, float size);

    /// Get list of available test fonts
    [[nodiscard]] static std::vector<std::pair<std::string, std::string>>
        get_test_fonts();

private:
    /// Internal font data holder
    struct font_data {
        // Font file data (must stay alive)
        std::vector<uint8_t> file_data;

        // Font objects (type depends on format)
        std::optional<onyx_font::bitmap_font> bitmap;
        std::optional<onyx_font::vector_font> vector;
        std::optional<onyx_font::ttf_font> ttf;

        // Rendering pipeline
        std::unique_ptr<onyx_font::font_source> source;
        std::unique_ptr<onyx_font::glyph_cache<onyx_font::memory_atlas>> cache;
        std::unique_ptr<onyx_font::text_renderer<onyx_font::memory_atlas>> renderer;

        // Metadata
        loaded_font_info info;
    };

    std::vector<std::unique_ptr<font_data>> m_fonts;

    /// Helper to detect font type and load
    bool load_font_data(font_data& fd, int font_index);

    /// Rebuild cache and renderer after size change
    void rebuild_renderer(font_data& fd);
};

} // namespace imgui_demo
