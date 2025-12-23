//
// Created by igor on 11/12/2025.
//

#include <onyx_font/font_factory.hh>
#include <libexe/libexe.hpp>
#include <libexe/resources/parsers/os2_resource_parser.hpp>
#include <failsafe/failsafe.hh>
#include <fstream>
#include <cstring>

#include "loader/loaders.hh"

// Windows headers define RT_FONT and RT_FD as macros that conflict with libexe enums
#ifdef RT_FONT
#undef RT_FONT
#endif
#ifdef RT_FD
#undef RT_FD
#endif

namespace onyx_font {

    namespace {
        // TTF/OTF magic signatures
        constexpr uint8_t TTF_MAGIC[] = {0x00, 0x01, 0x00, 0x00};
        constexpr uint8_t OTF_MAGIC[] = {'O', 'T', 'T', 'O'};
        constexpr uint8_t TTC_MAGIC[] = {'t', 't', 'c', 'f'};
        constexpr uint8_t BGI_MAGIC[] = {'P', 'K'};

        // Windows FNT version signatures (little-endian)
        constexpr uint16_t FNT_VERSION_1 = 0x0100;  // Windows 1.x
        constexpr uint16_t FNT_VERSION_2 = 0x0200;  // Windows 2.x
        constexpr uint16_t FNT_VERSION_3 = 0x0300;  // Windows 3.x

        bool starts_with(std::span<const uint8_t> data, std::span<const uint8_t> prefix) {
            if (data.size() < prefix.size()) return false;
            return std::equal(prefix.begin(), prefix.end(), data.begin());
        }

        // Check if data looks like a raw FNT file
        bool is_fnt_file(std::span<const uint8_t> data) {
            if (data.size() < 6) return false;

            // Check version field (first 2 bytes, little-endian)
            uint16_t version = static_cast<uint16_t>(static_cast<uint16_t>(data[0]) |
                              static_cast<uint16_t>(static_cast<uint16_t>(data[1]) << 8));

            if (version != FNT_VERSION_1 && version != FNT_VERSION_2 && version != FNT_VERSION_3) {
                return false;
            }

            // Additional sanity check: file size field at offset 2 (4 bytes, little-endian)
            uint32_t file_size = static_cast<uint32_t>(data[2]) |
                                (static_cast<uint32_t>(data[3]) << 8) |
                                (static_cast<uint32_t>(data[4]) << 16) |
                                (static_cast<uint32_t>(data[5]) << 24);

            // File size should be reasonable and match actual data size
            return file_size > 0 && file_size <= data.size() + 16;  // Allow some slack
        }

        // Helper to read file into vector
        std::vector<uint8_t> read_file(const std::filesystem::path& path) {
            std::ifstream file(path, std::ios::binary | std::ios::ate);
            THROW_IF(!file, std::runtime_error, "Cannot open file:", path.string());

            auto size = file.tellg();
            file.seekg(0, std::ios::beg);

            std::vector<uint8_t> data(static_cast<size_t>(size));
            THROW_IF(!file.read(reinterpret_cast<char*>(data.data()), size),
                     std::runtime_error, "Failed to read file:", path.string());

            return data;
        }

        // Create font_entry from libexe::font_data
        font_entry make_entry_from_font_data(const libexe::font_data& fd) {
            font_entry entry;
            entry.name = fd.face_name;
            entry.type = fd.is_vector() ? font_type::VECTOR : font_type::BITMAP;
            entry.pixel_height = fd.pixel_height;
            entry.point_size = fd.points;
            entry.weight = fd.weight;
            entry.italic = fd.italic;
            return entry;
        }

        // Create font_entry from OS/2 GPI font
        font_entry make_entry_from_os2_font(const libexe::os2_font& font) {
            font_entry entry;
            entry.name = font.metrics.face_name;
            entry.type = font_type::BITMAP;  // OS/2 GPI fonts are bitmap fonts
            entry.pixel_height = static_cast<uint16_t>(font.cell_height);
            entry.point_size = static_cast<uint16_t>(font.metrics.nominal_point_size / 10);  // nominal_point_size is size * 10
            entry.weight = static_cast<uint16_t>(font.metrics.weight_class / 10);  // weight_class is 1000-9000
            entry.italic = false;  // OS/2 fonts don't have italic flag in metrics
            return entry;
        }

        // Convert OS/2 GPI font to Windows font_data for uniform handling
        libexe::font_data convert_os2_to_font_data(const libexe::os2_font& os2) {
            libexe::font_data fd;

            // Metadata
            fd.version = 0x0300;  // Treat as Windows 3.x equivalent
            fd.size = 0;
            fd.copyright = "";
            fd.type = libexe::font_type::RASTER;

            // Metrics from OS/2 font
            fd.points = static_cast<uint16_t>(os2.metrics.nominal_point_size / 10);
            fd.vertical_res = static_cast<uint16_t>(os2.metrics.device_res_y);
            fd.horizontal_res = static_cast<uint16_t>(os2.metrics.device_res_x);
            fd.ascent = static_cast<uint16_t>(os2.metrics.max_ascender);
            fd.internal_leading = static_cast<uint16_t>(os2.metrics.internal_leading);
            fd.external_leading = static_cast<uint16_t>(os2.metrics.external_leading);

            // Appearance
            fd.italic = false;
            fd.underline = false;
            fd.strikeout = false;
            fd.weight = static_cast<uint16_t>(os2.metrics.weight_class / 10);  // Convert 1000-9000 to 100-900
            fd.charset = 0;  // Default charset

            // Dimensions
            fd.pixel_width = static_cast<uint16_t>(os2.metrics.ave_char_width);
            fd.pixel_height = static_cast<uint16_t>(os2.cell_height);
            fd.avg_width = static_cast<uint16_t>(os2.metrics.ave_char_width);
            fd.max_width = static_cast<uint16_t>(os2.metrics.max_char_inc);
            fd.width_bytes = 0;  // Will be calculated if needed

            // Character range
            fd.first_char = static_cast<uint8_t>(os2.metrics.first_char);
            fd.last_char = static_cast<uint8_t>(os2.metrics.last_char);
            fd.default_char = static_cast<uint8_t>(os2.metrics.default_char);
            fd.break_char = static_cast<uint8_t>(os2.metrics.break_char);

            // Font family & pitch
            fd.pitch = (os2.font_type == 1) ? libexe::font_pitch::FIXED : libexe::font_pitch::VARIABLE;
            fd.family = libexe::font_family::DONTCARE;
            fd.face_name = os2.metrics.face_name;

            // Convert glyph data
            for (const auto& ch : os2.characters) {
                libexe::glyph_entry ge;
                ge.width = ch.width;
                ge.offset = ch.bitmap_offset;
                fd.glyphs.push_back(ge);
            }

            // Copy bitmap data
            fd.bitmap_data = os2.bitmap_data;

            return fd;
        }

        // Analyze Windows NE file for fonts
        container_info analyze_ne(std::span<const uint8_t> data) {
            container_info info;
            info.format = container_format::FON_NE;

            auto ne = libexe::ne_file::from_memory(data);
            auto resources = ne.resources();
            if (!resources) {
                return info;
            }

            // Check if this is an OS/2 NE file
            bool is_os2 = (ne.target_os() == libexe::ne_target_os::OS2);

            if (is_os2) {
                // OS/2 stores font resources under type ID 7
                auto font_entries = resources->resources_by_type_id(7);
                for (const auto& res : font_entries) {
                    // Skip fontdir (ID 1), only process actual fonts
                    auto id = res.id();
                    if (id && *id == 1) {
                        continue;  // Skip fontdir
                    }
                    // Parse as OS/2 GPI font
                    auto os2_font = libexe::parse_os2_font(res.data());
                    if (os2_font) {
                        info.fonts.push_back(make_entry_from_os2_font(*os2_font));
                    }
                }
            } else {
                // Windows stores fonts under RT_FONT (8)
                auto font_entries = resources->resources_by_type(libexe::resource_type::RT_FONT);
                for (const auto& res : font_entries) {
                    auto opt_font = res.as_font();
                    if (opt_font) {
                        info.fonts.push_back(make_entry_from_font_data(*opt_font));
                    }
                }
            }

            return info;
        }

        // Analyze Windows PE file for fonts
        container_info analyze_pe(std::span<const uint8_t> data) {
            container_info info;
            info.format = container_format::FON_PE;

            auto pe = libexe::pe_file::from_memory(data);
            auto resources = pe.resources();
            if (!resources) {
                return info;
            }

            auto font_entries = resources->resources_by_type(libexe::resource_type::RT_FONT);
            for (const auto& res : font_entries) {
                auto opt_font = res.as_font();
                if (opt_font) {
                    info.fonts.push_back(make_entry_from_font_data(*opt_font));
                }
            }

            return info;
        }

        // Analyze OS/2 LX file for fonts
        container_info analyze_lx(std::span<const uint8_t> data) {
            container_info info;
            info.format = container_format::FON_LX;

            auto lx = libexe::le_file::from_memory(data);
            if (!lx.has_resources()) {
                return info;
            }

            // OS/2 font resources: RT_FONT (7) or RT_FD (21)
            auto font_resources = lx.resources_by_type(libexe::le_resource::RT_FONT);
            auto fd_resources = lx.resources_by_type(libexe::le_resource::RT_FD);

            // Combine both resource types
            std::vector<libexe::le_resource> all_fonts;
            all_fonts.insert(all_fonts.end(), font_resources.begin(), font_resources.end());
            all_fonts.insert(all_fonts.end(), fd_resources.begin(), fd_resources.end());

            for (const auto& res : all_fonts) {
                auto res_data = lx.read_resource_data(res);
                auto opt_font = libexe::font_parser::parse(res_data);
                if (opt_font) {
                    info.fonts.push_back(make_entry_from_font_data(*opt_font));
                }
            }

            return info;
        }

        // Analyze BGI font
        container_info analyze_bgi(std::span<const uint8_t> data) {
            container_info info;
            info.format = container_format::BGI;

            // BGI files contain exactly one font
            // Extract name from header if possible
            font_entry entry;
            entry.type = font_type::VECTOR;
            entry.pixel_height = 0;  // Scalable
            entry.point_size = 0;
            entry.weight = 400;
            entry.italic = false;

            // Try to extract name from BGI header (after 0x1A terminator)
            // Structure: 0x1A | header_size(2) | font_name(4) | ...
            for (size_t i = 2; i < std::min(data.size(), size_t{255}); ++i) {
                if (data[i] == 0x1A && i + 7 < data.size()) {
                    // Name is at offset +2 from header info start (after 2-byte header_size)
                    size_t name_offset = i + 1 + 2;
                    if (name_offset + 4 <= data.size()) {
                        entry.name = std::string(
                            reinterpret_cast<const char*>(data.data() + name_offset),
                            strnlen(reinterpret_cast<const char*>(data.data() + name_offset), 4)
                        );
                    }
                    break;
                }
            }

            if (entry.name.empty()) {
                entry.name = "BGI Font";
            }

            info.fonts.push_back(entry);
            return info;
        }

        // Analyze TTF/OTF/TTC
        container_info analyze_ttf(std::span<const uint8_t> data) {
            container_info info;

            if (starts_with(data, TTC_MAGIC)) {
                info.format = container_format::TTC;
            } else if (starts_with(data, OTF_MAGIC)) {
                info.format = container_format::OTF;
            } else {
                info.format = container_format::TTF;
            }

            int font_count = ttf_font::get_font_count(data);
            for (int i = 0; i < font_count; ++i) {
                font_entry entry;
                entry.type = font_type::OUTLINE;
                entry.pixel_height = 0;  // Scalable
                entry.point_size = 0;
                entry.weight = 400;
                entry.italic = false;

                // Try to get font name by loading it briefly
                try {
                    ttf_font font(data, i);
                    if (font.is_valid()) {
                        // stb_truetype doesn't provide a direct way to get the name
                        // We could parse the name table, but for now use a placeholder
                        entry.name = "Font " + std::to_string(i);
                    }
                } catch (...) {
                    entry.name = "Font " + std::to_string(i);
                }

                info.fonts.push_back(entry);
            }

            return info;
        }

        // Analyze raw FNT file
        container_info analyze_fnt(std::span<const uint8_t> data) {
            container_info info;
            info.format = container_format::FNT;

            auto opt_font = libexe::font_parser::parse(data);
            if (opt_font) {
                info.fonts.push_back(make_entry_from_font_data(*opt_font));
            }

            return info;
        }

        // Load font resources from NE file
        std::vector<libexe::font_data> load_ne_fonts(std::span<const uint8_t> data) {
            std::vector<libexe::font_data> result;

            auto ne = libexe::ne_file::from_memory(data);
            auto resources = ne.resources();
            if (!resources) {
                return result;
            }

            // Check if this is an OS/2 NE file
            bool is_os2 = (ne.target_os() == libexe::ne_target_os::OS2);

            if (is_os2) {
                // OS/2 stores font resources under type ID 7
                auto font_entries = resources->resources_by_type_id(7);
                for (const auto& res : font_entries) {
                    // Skip fontdir (ID 1), only process actual fonts
                    auto id = res.id();
                    if (id && *id == 1) {
                        continue;  // Skip fontdir
                    }
                    // Parse as OS/2 GPI font and convert to font_data
                    auto os2_font = libexe::parse_os2_font(res.data());
                    if (os2_font) {
                        result.push_back(convert_os2_to_font_data(*os2_font));
                    }
                }
            } else {
                // Windows stores fonts under RT_FONT (8)
                auto font_entries = resources->resources_by_type(libexe::resource_type::RT_FONT);
                for (const auto& res : font_entries) {
                    auto opt_font = res.as_font();
                    if (opt_font) {
                        result.push_back(std::move(*opt_font));
                    }
                }
            }

            return result;
        }

        // Load font resources from PE file
        std::vector<libexe::font_data> load_pe_fonts(std::span<const uint8_t> data) {
            std::vector<libexe::font_data> result;

            auto pe = libexe::pe_file::from_memory(data);
            auto resources = pe.resources();
            if (!resources) {
                return result;
            }

            auto font_entries = resources->resources_by_type(libexe::resource_type::RT_FONT);
            for (const auto& res : font_entries) {
                auto opt_font = res.as_font();
                if (opt_font) {
                    result.push_back(std::move(*opt_font));
                }
            }

            return result;
        }

        // Load font resources from LX file
        std::vector<libexe::font_data> load_lx_fonts(std::span<const uint8_t> data) {
            std::vector<libexe::font_data> result;

            auto lx = libexe::le_file::from_memory(data);
            if (!lx.has_resources()) {
                return result;
            }

            auto font_resources = lx.resources_by_type(libexe::le_resource::RT_FONT);
            auto fd_resources = lx.resources_by_type(libexe::le_resource::RT_FD);

            std::vector<libexe::le_resource> all_fonts;
            all_fonts.insert(all_fonts.end(), font_resources.begin(), font_resources.end());
            all_fonts.insert(all_fonts.end(), fd_resources.begin(), fd_resources.end());

            for (const auto& res : all_fonts) {
                auto res_data = lx.read_resource_data(res);
                auto opt_font = libexe::font_parser::parse(res_data);
                if (opt_font) {
                    result.push_back(std::move(*opt_font));
                }
            }

            return result;
        }
    }

    // =========================================================================
    // Container Analysis
    // =========================================================================

    container_info font_factory::analyze(std::span<const uint8_t> data) {
        if (data.size() < 4) {
            return {container_format::UNKNOWN, {}};
        }

        // Check for TTF/OTF/TTC
        if (starts_with(data, TTF_MAGIC) ||
            starts_with(data, OTF_MAGIC) ||
            starts_with(data, TTC_MAGIC)) {
            return analyze_ttf(data);
        }

        // Check for BGI
        if (starts_with(data, BGI_MAGIC)) {
            return analyze_bgi(data);
        }

        // Check for Windows/OS2 executable (MZ header)
        if (data[0] == 'M' && data[1] == 'Z') {
            try {
                auto exe_format = libexe::executable_factory::detect_format(data);

                switch (exe_format) {
                    case libexe::format_type::NE_WIN16:
                        return analyze_ne(data);

                    case libexe::format_type::PE_WIN32:
                    case libexe::format_type::PE_PLUS_WIN64:
                        return analyze_pe(data);

                    case libexe::format_type::LX_OS2_BOUND:
                    case libexe::format_type::LX_OS2_RAW:
                        return analyze_lx(data);

                    default:
                        break;
                }
            } catch (...) {
                // Fall through to check for FNT
            }
        }

        // Check for raw FNT file (Windows font without executable wrapper)
        if (is_fnt_file(data)) {
            return analyze_fnt(data);
        }

        return {container_format::UNKNOWN, {}};
    }

    container_info font_factory::analyze(const std::filesystem::path& path) {
        auto data = read_file(path);
        return analyze(data);
    }

    // =========================================================================
    // Type-Specific Loading
    // =========================================================================

    bitmap_font font_factory::load_bitmap(std::span<const uint8_t> data, size_t index) {
        THROW_IF(data.size() < 4, std::runtime_error, "Invalid font data: too small");

        // Check for raw FNT file first
        if (is_fnt_file(data)) {
            THROW_IF(index != 0, std::invalid_argument,
                     "FNT files contain only one font (index must be 0)");
            auto opt_font = libexe::font_parser::parse(data);
            THROW_IF(!opt_font, std::runtime_error, "Failed to parse FNT file");
            THROW_IF(!opt_font->is_raster(), std::runtime_error,
                     "FNT file contains a vector font, not a bitmap font");
            return internal::win_bitmap_fon_loader::load(*opt_font);
        }

        // Must be a Windows/OS2 executable
        THROW_IF(data[0] != 'M' || data[1] != 'Z', std::runtime_error,
                 "Not a Windows/OS2 font file or FNT file");

        auto exe_format = libexe::executable_factory::detect_format(data);
        std::vector<libexe::font_data> fonts;

        switch (exe_format) {
            case libexe::format_type::NE_WIN16:
                fonts = load_ne_fonts(data);
                break;

            case libexe::format_type::PE_WIN32:
            case libexe::format_type::PE_PLUS_WIN64:
                fonts = load_pe_fonts(data);
                break;

            case libexe::format_type::LX_OS2_BOUND:
            case libexe::format_type::LX_OS2_RAW:
                fonts = load_lx_fonts(data);
                break;

            default:
                THROW_RUNTIME("Unsupported executable format for fonts");
        }

        // Filter to bitmap fonts only
        std::vector<libexe::font_data> bitmap_fonts;
        for (auto& f : fonts) {
            if (f.is_raster()) {
                bitmap_fonts.push_back(std::move(f));
            }
        }

        THROW_IF(index >= bitmap_fonts.size(), std::out_of_range,
                 "Font index", index, "out of range (have", bitmap_fonts.size(), "bitmap fonts)");

        return internal::win_bitmap_fon_loader::load(bitmap_fonts[index]);
    }

    vector_font font_factory::load_vector(std::span<const uint8_t> data, size_t index) {
        THROW_IF(data.size() < 4, std::runtime_error, "Invalid font data: too small");

        // Check for BGI font
        if (starts_with(data, BGI_MAGIC)) {
            THROW_IF(index != 0, std::invalid_argument,
                     "BGI fonts contain only one font (index must be 0)");
            return internal::bgi_font_loader::load(data);
        }

        // Check for raw FNT file
        if (is_fnt_file(data)) {
            THROW_IF(index != 0, std::invalid_argument,
                     "FNT files contain only one font (index must be 0)");
            auto opt_font = libexe::font_parser::parse(data);
            THROW_IF(!opt_font, std::runtime_error, "Failed to parse FNT file");
            THROW_IF(!opt_font->is_vector(), std::runtime_error,
                     "FNT file contains a bitmap font, not a vector font");
            return internal::win_vector_fon_loader::load(*opt_font);
        }

        // Must be a Windows/OS2 executable
        THROW_IF(data[0] != 'M' || data[1] != 'Z', std::runtime_error,
                 "Not a Windows/OS2 font file, FNT file, or BGI font");

        auto exe_format = libexe::executable_factory::detect_format(data);
        std::vector<libexe::font_data> fonts;

        switch (exe_format) {
            case libexe::format_type::NE_WIN16:
                fonts = load_ne_fonts(data);
                break;

            case libexe::format_type::PE_WIN32:
            case libexe::format_type::PE_PLUS_WIN64:
                fonts = load_pe_fonts(data);
                break;

            case libexe::format_type::LX_OS2_BOUND:
            case libexe::format_type::LX_OS2_RAW:
                fonts = load_lx_fonts(data);
                break;

            default:
                THROW_RUNTIME("Unsupported executable format for fonts");
        }

        // Filter to vector fonts only
        std::vector<libexe::font_data> vector_fonts;
        for (auto& f : fonts) {
            if (f.is_vector()) {
                vector_fonts.push_back(std::move(f));
            }
        }

        THROW_IF(index >= vector_fonts.size(), std::out_of_range,
                 "Font index", index, "out of range (have", vector_fonts.size(), "vector fonts)");

        return internal::win_vector_fon_loader::load(vector_fonts[index]);
    }

    ttf_font font_factory::load_ttf(std::span<const uint8_t> data, size_t index) {
        THROW_IF(data.size() < 4, std::runtime_error, "Invalid font data: too small");

        THROW_IF(!starts_with(data, TTF_MAGIC) &&
                 !starts_with(data, OTF_MAGIC) &&
                 !starts_with(data, TTC_MAGIC),
                 std::runtime_error, "Not a TTF/OTF/TTC font file");

        int font_count = ttf_font::get_font_count(data);
        THROW_IF(index >= static_cast<size_t>(font_count), std::out_of_range,
                 "Font index", index, "out of range (have", font_count, "fonts)");

        return ttf_font(data, static_cast<int>(index));
    }

    // =========================================================================
    // Bulk Loading
    // =========================================================================

    std::vector<bitmap_font> font_factory::load_all_bitmaps(std::span<const uint8_t> data) {
        std::vector<bitmap_font> result;

        if (data.size() < 4) {
            return result;
        }

        // Check for raw FNT file
        if (is_fnt_file(data)) {
            auto opt_font = libexe::font_parser::parse(data);
            if (opt_font && opt_font->is_raster()) {
                result.push_back(internal::win_bitmap_fon_loader::load(*opt_font));
            }
            return result;
        }

        // Windows/OS2 executable
        if (data[0] != 'M' || data[1] != 'Z') {
            return result;
        }

        auto exe_format = libexe::executable_factory::detect_format(data);
        std::vector<libexe::font_data> fonts;

        switch (exe_format) {
            case libexe::format_type::NE_WIN16:
                fonts = load_ne_fonts(data);
                break;

            case libexe::format_type::PE_WIN32:
            case libexe::format_type::PE_PLUS_WIN64:
                fonts = load_pe_fonts(data);
                break;

            case libexe::format_type::LX_OS2_BOUND:
            case libexe::format_type::LX_OS2_RAW:
                fonts = load_lx_fonts(data);
                break;

            default:
                return result;
        }

        for (auto& f : fonts) {
            if (f.is_raster()) {
                result.push_back(internal::win_bitmap_fon_loader::load(f));
            }
        }

        return result;
    }

    std::vector<vector_font> font_factory::load_all_vectors(std::span<const uint8_t> data) {
        std::vector<vector_font> result;

        if (data.size() < 4) {
            return result;
        }

        // Check for BGI font
        if (starts_with(data, BGI_MAGIC)) {
            result.push_back(internal::bgi_font_loader::load(data));
            return result;
        }

        // Check for raw FNT file
        if (is_fnt_file(data)) {
            auto opt_font = libexe::font_parser::parse(data);
            if (opt_font && opt_font->is_vector()) {
                result.push_back(internal::win_vector_fon_loader::load(*opt_font));
            }
            return result;
        }

        // Windows/OS2 executable
        if (data[0] != 'M' || data[1] != 'Z') {
            return result;
        }

        auto exe_format = libexe::executable_factory::detect_format(data);
        std::vector<libexe::font_data> fonts;

        switch (exe_format) {
            case libexe::format_type::NE_WIN16:
                fonts = load_ne_fonts(data);
                break;

            case libexe::format_type::PE_WIN32:
            case libexe::format_type::PE_PLUS_WIN64:
                fonts = load_pe_fonts(data);
                break;

            case libexe::format_type::LX_OS2_BOUND:
            case libexe::format_type::LX_OS2_RAW:
                fonts = load_lx_fonts(data);
                break;

            default:
                return result;
        }

        for (auto& f : fonts) {
            if (f.is_vector()) {
                result.push_back(internal::win_vector_fon_loader::load(f));
            }
        }

        return result;
    }

    // =========================================================================
    // Utility Functions
    // =========================================================================

    std::string_view font_factory::format_name(container_format fmt) {
        switch (fmt) {
            case container_format::UNKNOWN: return "Unknown";
            case container_format::TTF: return "TrueType";
            case container_format::OTF: return "OpenType";
            case container_format::TTC: return "TrueType Collection";
            case container_format::FNT: return "Windows Font";
            case container_format::FON_NE: return "Windows 16-bit Font Resource";
            case container_format::FON_PE: return "Windows 32/64-bit Font Resource";
            case container_format::FON_LX: return "OS/2 Font Resource";
            case container_format::BGI: return "Borland Graphics Interface";
            default: return "Unknown";
        }
    }

    std::string_view font_factory::type_name(font_type type) {
        switch (type) {
            case font_type::UNKNOWN: return "Unknown";
            case font_type::BITMAP: return "Bitmap";
            case font_type::VECTOR: return "Vector";
            case font_type::OUTLINE: return "Outline";
            default: return "Unknown";
        }
    }

    // =========================================================================
    // Raw BIOS Font Loading
    // =========================================================================

    bitmap_font font_factory::load_raw(std::span<const uint8_t> data,
                                       const raw_font_options& options) {
        // Calculate expected data size
        size_t bytes_per_char = static_cast<size_t>((options.char_width + 7) / 8) * static_cast<size_t>(options.char_height);
        size_t expected_size = bytes_per_char * options.char_count;

        THROW_IF(data.size() < expected_size, std::runtime_error,
                 "Raw font data too small: expected", expected_size, "bytes, got", data.size());

        bitmap_font result;

        // Set font metadata
        result.m_name = options.name;
        result.m_first_char = options.first_char;
        result.m_last_char = static_cast<uint8_t>(options.first_char + options.char_count - 1);
        result.m_default_char = '?';
        result.m_break_char = ' ';

        // Set metrics
        result.m_metrics.pixel_height = options.char_height;
        result.m_metrics.ascent = options.char_height;  // Assume full height is ascent
        result.m_metrics.internal_leading = 0;
        result.m_metrics.external_leading = 0;
        result.m_metrics.avg_width = options.char_width;
        result.m_metrics.max_width = options.char_width;

        // Prepare spacing (all characters have the same width)
        result.m_spacing.resize(options.char_count);
        for (auto& sp : result.m_spacing) {
            sp.b_space = options.char_width;
        }

        // Build bitmap storage
        bit_order order = options.msb_first ? bit_order::msb_first : bit_order::lsb_first;
        bitmap_builder builder(order);
        builder.reserve_glyphs(options.char_count);
        builder.reserve_bytes(expected_size);

        for (size_t i = 0; i < options.char_count; ++i) {
            // Get pointer to this character's data
            const uint8_t* char_data = data.data() + i * bytes_per_char;

            // Add glyph with packed data
            std::span<const std::byte> rows(
                reinterpret_cast<const std::byte*>(char_data),
                bytes_per_char
            );
            (void)builder.add_glyph_packed(options.char_width, options.char_height, rows);
        }

        result.m_storage = std::move(builder).build();
        return result;
    }

}  // namespace onyx_font
