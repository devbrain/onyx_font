//
// Font Manager Implementation
//

#include "font_manager.hh"

#include <fstream>
#include <algorithm>

namespace imgui_demo {

namespace {

/// Read entire file into vector
std::vector<uint8_t> read_file(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        return {};
    }

    auto size = file.tellg();
    if (size <= 0) {
        return {};
    }

    std::vector<uint8_t> data(static_cast<std::size_t>(size));
    file.seekg(0, std::ios::beg);
    file.read(reinterpret_cast<char*>(data.data()),
              static_cast<std::streamsize>(size));

    return data;
}

} // anonymous namespace

bool font_manager::load_font(const std::filesystem::path& path, int font_index) {
    auto data = read_file(path);
    if (data.empty()) {
        return false;
    }

    auto fd = std::make_unique<font_data>();
    fd->file_data = std::move(data);
    fd->info.path = path.string();
    fd->info.name = path.stem().string();

    if (!load_font_data(*fd, font_index)) {
        return false;
    }

    m_fonts.push_back(std::move(fd));
    return true;
}

bool font_manager::load_font_memory(const std::string& name,
                                     std::span<const uint8_t> data,
                                     int font_index) {
    if (data.empty()) {
        return false;
    }

    auto fd = std::make_unique<font_data>();
    fd->file_data.assign(data.begin(), data.end());
    fd->info.name = name;
    fd->info.path = "(memory)";

    if (!load_font_data(*fd, font_index)) {
        return false;
    }

    m_fonts.push_back(std::move(fd));
    return true;
}

bool font_manager::load_font_data(font_data& fd, int font_index) {
    // Analyze the font file
    auto analysis = onyx_font::font_factory::analyze(fd.file_data);

    if (analysis.fonts.empty()) {
        return false;
    }

    // Validate font index
    if (font_index < 0 ||
        static_cast<std::size_t>(font_index) >= analysis.fonts.size()) {
        font_index = 0;
    }

    const auto& font_info = analysis.fonts[static_cast<std::size_t>(font_index)];

    // Load based on detected type
    switch (font_info.type) {
        case onyx_font::font_type::BITMAP: {
            fd.bitmap = onyx_font::font_factory::load_bitmap(
                fd.file_data, font_index);
            if (!fd.bitmap) {
                return false;
            }
            fd.source = std::make_unique<onyx_font::font_source>(
                onyx_font::font_source::from_bitmap(*fd.bitmap));
            fd.info.type = onyx_font::font_source_type::bitmap;
            fd.info.scalable = false;
            // Bitmap fonts have fixed size
            fd.info.current_size = fd.source->native_size();
            break;
        }

        case onyx_font::font_type::VECTOR: {
            fd.vector = onyx_font::font_factory::load_vector(
                fd.file_data, font_index);
            if (!fd.vector) {
                return false;
            }
            fd.source = std::make_unique<onyx_font::font_source>(
                onyx_font::font_source::from_vector(*fd.vector));
            fd.info.type = onyx_font::font_source_type::vector;
            fd.info.scalable = true;
            break;
        }

        case onyx_font::font_type::OUTLINE: {
            fd.ttf.emplace(fd.file_data);

            if (!fd.ttf->is_valid()) {
                return false;
            }

            fd.source = std::make_unique<onyx_font::font_source>(
                onyx_font::font_source::from_ttf(*fd.ttf));
            fd.info.type = onyx_font::font_source_type::outline;
            fd.info.scalable = true;
            break;
        }

        default:
            return false;
    }

    // Update name with font info if available
    if (!font_info.name.empty()) {
        fd.info.name = font_info.name;
    }

    // Build the renderer
    rebuild_renderer(fd);

    return fd.renderer != nullptr;
}

void font_manager::rebuild_renderer(font_data& fd) {
    if (!fd.source) {
        return;
    }

    // Create glyph cache with current size
    onyx_font::glyph_cache_config config;
    config.atlas_size = 512;
    config.pre_cache_ascii = true;

    fd.cache = std::make_unique<onyx_font::glyph_cache<onyx_font::memory_atlas>>(
        std::move(*fd.source), fd.info.current_size, config);

    // Create text renderer
    fd.renderer = std::make_unique<onyx_font::text_renderer<onyx_font::memory_atlas>>(
        *fd.cache);
}

void font_manager::remove_font(std::size_t index) {
    if (index < m_fonts.size()) {
        m_fonts.erase(m_fonts.begin() + static_cast<std::ptrdiff_t>(index));
    }
}

const loaded_font_info* font_manager::get_info(std::size_t index) const {
    if (index < m_fonts.size()) {
        return &m_fonts[index]->info;
    }
    return nullptr;
}

onyx_font::text_renderer<onyx_font::memory_atlas>*
font_manager::get_renderer(std::size_t index) {
    if (index < m_fonts.size()) {
        return m_fonts[index]->renderer.get();
    }
    return nullptr;
}

onyx_font::glyph_cache<onyx_font::memory_atlas>*
font_manager::get_cache(std::size_t index) {
    if (index < m_fonts.size()) {
        return m_fonts[index]->cache.get();
    }
    return nullptr;
}

bool font_manager::set_font_size(std::size_t index, float size) {
    if (index >= m_fonts.size()) {
        return false;
    }

    auto& fd = *m_fonts[index];

    // Bitmap fonts can't be scaled
    if (!fd.info.scalable) {
        return false;
    }

    // Clamp size
    size = std::clamp(size, 8.0f, 200.0f);

    if (std::abs(fd.info.current_size - size) < 0.5f) {
        return false; // No significant change
    }

    fd.info.current_size = size;
    rebuild_renderer(fd);
    return true;
}

std::vector<std::pair<std::string, std::string>> font_manager::get_test_fonts() {
    std::vector<std::pair<std::string, std::string>> fonts;

#ifdef TESTDATA_DIR
    std::filesystem::path testdata = TESTDATA_DIR;

    // TTF fonts
    if (auto p = testdata / "winfonts" / "ARIAL.TTF"; std::filesystem::exists(p)) {
        fonts.emplace_back("Arial (TTF)", p.string());
    }
    if (auto p = testdata / "ttf" / "marlett.ttf"; std::filesystem::exists(p)) {
        fonts.emplace_back("Marlett Symbol (TTF)", p.string());
    }

    // Bitmap fonts (Windows FON)
    if (auto p = testdata / "winfonts" / "HELVA.FON"; std::filesystem::exists(p)) {
        fonts.emplace_back("Helv (Bitmap FON)", p.string());
    }
    if (auto p = testdata / "winfonts" / "VGAOEM.FON"; std::filesystem::exists(p)) {
        fonts.emplace_back("VGA OEM (Bitmap FON)", p.string());
    }

    // Vector fonts (Windows FNT)
    if (auto p = testdata / "winfonts" / "ROMANP.FNT"; std::filesystem::exists(p)) {
        fonts.emplace_back("Roman Simplex (Vector FNT)", p.string());
    }

    // Vector fonts (BGI)
    if (auto p = testdata / "bgi" / "LITT.CHR"; std::filesystem::exists(p)) {
        fonts.emplace_back("Little (Vector BGI)", p.string());
    }

    // OS/2 fonts
    if (auto p = testdata / "os2" / "SYSFONT.DLL"; std::filesystem::exists(p)) {
        fonts.emplace_back("OS/2 System Font", p.string());
    }
#endif

    return fonts;
}

} // namespace imgui_demo
