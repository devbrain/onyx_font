//
// Created by igor on 01/11/2026.
//

#include <onyx_font/font_registry.hh>
#include <onyx_font/font_factory.hh>
#include <failsafe/failsafe.hh>
#include <mutex>
#include <algorithm>

#include "loader/loaders.hh"

#if defined(ONYX_FONT_HAS_LOADER_FON)
#include <libexe/libexe.hpp>
#include <libexe/resources/parsers/os2_resource_parser.hpp>

// Windows headers define RT_FONT as macro that conflicts with libexe enums
#ifdef RT_FONT
#undef RT_FONT
#endif
#ifdef RT_FD
#undef RT_FD
#endif
#endif  // ONYX_FONT_HAS_LOADER_FON

namespace onyx_font {

    // =========================================================================
    // Helper functions (from font_factory.cc, made available here)
    // =========================================================================

    namespace {
        // TTF/OTF magic signatures
        constexpr uint8_t TTF_MAGIC[] = {0x00, 0x01, 0x00, 0x00};
        constexpr uint8_t OTF_MAGIC[] = {'O', 'T', 'T', 'O'};
        constexpr uint8_t TTC_MAGIC[] = {'t', 't', 'c', 'f'};
        constexpr uint8_t BGI_MAGIC[] = {'P', 'K'};

        // Windows FNT version signatures (little-endian)
        constexpr uint16_t FNT_VERSION_1 = 0x0100;
        constexpr uint16_t FNT_VERSION_2 = 0x0200;
        constexpr uint16_t FNT_VERSION_3 = 0x0300;

        bool starts_with(std::span<const uint8_t> data, std::span<const uint8_t> prefix) {
            if (data.size() < prefix.size()) return false;
            return std::equal(prefix.begin(), prefix.end(), data.begin());
        }

#if defined(ONYX_FONT_HAS_LOADER_FON)
        bool is_fnt_file(std::span<const uint8_t> data) {
            if (data.size() < 6) return false;

            uint16_t version = static_cast<uint16_t>(data[0]) |
                              (static_cast<uint16_t>(data[1]) << 8);

            if (version != FNT_VERSION_1 && version != FNT_VERSION_2 && version != FNT_VERSION_3) {
                return false;
            }

            uint32_t file_size = static_cast<uint32_t>(data[2]) |
                                (static_cast<uint32_t>(data[3]) << 8) |
                                (static_cast<uint32_t>(data[4]) << 16) |
                                (static_cast<uint32_t>(data[5]) << 24);

            return file_size > 0 && file_size <= data.size() + 16;
        }

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

        libexe::font_data convert_os2_to_font_data(const libexe::os2_font& os2) {
            libexe::font_data fd;

            fd.version = 0x0300;
            fd.size = 0;
            fd.copyright = "";
            fd.type = libexe::font_type::RASTER;

            fd.points = static_cast<uint16_t>(os2.metrics.nominal_point_size / 10);
            fd.vertical_res = static_cast<uint16_t>(os2.metrics.device_res_y);
            fd.horizontal_res = static_cast<uint16_t>(os2.metrics.device_res_x);
            fd.ascent = static_cast<uint16_t>(os2.metrics.max_ascender);
            fd.internal_leading = static_cast<uint16_t>(os2.metrics.internal_leading);
            fd.external_leading = static_cast<uint16_t>(os2.metrics.external_leading);

            fd.italic = false;
            fd.underline = false;
            fd.strikeout = false;
            fd.weight = static_cast<uint16_t>(os2.metrics.weight_class / 10);
            fd.charset = 0;

            fd.pixel_width = static_cast<uint16_t>(os2.metrics.ave_char_width);
            fd.pixel_height = static_cast<uint16_t>(os2.cell_height);
            fd.avg_width = static_cast<uint16_t>(os2.metrics.ave_char_width);
            fd.max_width = static_cast<uint16_t>(os2.metrics.max_char_inc);
            fd.width_bytes = 0;

            fd.first_char = static_cast<uint8_t>(os2.metrics.first_char);
            fd.last_char = static_cast<uint8_t>(os2.metrics.last_char);
            fd.default_char = static_cast<uint8_t>(os2.metrics.default_char);
            fd.break_char = static_cast<uint8_t>(os2.metrics.break_char);

            fd.pitch = (os2.font_type == 1) ? libexe::font_pitch::FIXED : libexe::font_pitch::VARIABLE;
            fd.family = libexe::font_family::DONTCARE;
            fd.face_name = os2.metrics.face_name;

            for (const auto& ch : os2.characters) {
                libexe::glyph_entry ge;
                ge.width = ch.width;
                ge.offset = ch.bitmap_offset;
                fd.glyphs.push_back(ge);
            }

            fd.bitmap_data = os2.bitmap_data;

            return fd;
        }

        // Load font resources from NE file
        std::vector<libexe::font_data> load_ne_fonts(std::span<const uint8_t> data, bool want_bitmap) {
            std::vector<libexe::font_data> result;

            auto ne = libexe::ne_file::from_memory(data);
            auto resources = ne.resources();
            if (!resources) {
                return result;
            }

            bool is_os2 = (ne.target_os() == libexe::ne_target_os::OS2);

            if (is_os2) {
                auto font_entries = resources->resources_by_type_id(7);
                for (const auto& res : font_entries) {
                    auto id = res.id();
                    if (id && *id == 1) {
                        continue;
                    }
                    auto os2_font = libexe::parse_os2_font(res.data());
                    if (os2_font) {
                        auto fd = convert_os2_to_font_data(*os2_font);
                        if ((want_bitmap && fd.is_raster()) || (!want_bitmap && fd.is_vector())) {
                            result.push_back(std::move(fd));
                        }
                    }
                }
            } else {
                auto font_entries = resources->resources_by_type(libexe::resource_type::RT_FONT);
                for (const auto& res : font_entries) {
                    auto opt_font = res.as_font();
                    if (opt_font) {
                        if ((want_bitmap && opt_font->is_raster()) || (!want_bitmap && opt_font->is_vector())) {
                            result.push_back(std::move(*opt_font));
                        }
                    }
                }
            }

            return result;
        }

        // Load font resources from PE file
        std::vector<libexe::font_data> load_pe_fonts(std::span<const uint8_t> data, bool want_bitmap) {
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
                    if ((want_bitmap && opt_font->is_raster()) || (!want_bitmap && opt_font->is_vector())) {
                        result.push_back(std::move(*opt_font));
                    }
                }
            }

            return result;
        }

        // Load font resources from LX file
        std::vector<libexe::font_data> load_lx_fonts(std::span<const uint8_t> data, bool want_bitmap) {
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
                    if ((want_bitmap && opt_font->is_raster()) || (!want_bitmap && opt_font->is_vector())) {
                        result.push_back(std::move(*opt_font));
                    }
                }
            }

            return result;
        }
#endif  // ONYX_FONT_HAS_LOADER_FON

    }  // anonymous namespace

    // =========================================================================
    // Built-in Bitmap Decoders
    // =========================================================================

    namespace {

        // GEM font decoder
        class gem_font_decoder_impl : public bitmap_font_decoder {
        public:
            [[nodiscard]] std::string_view name() const noexcept override {
                return "gem";
            }

            [[nodiscard]] std::span<const std::string_view> extensions() const noexcept override {
                static constexpr std::string_view ext[] = {".gft", ".fnt"};
                return ext;
            }

            [[nodiscard]] bool sniff(std::span<const std::uint8_t> data) const noexcept override {
                return internal::gem_font_loader::is_gem_font(data);
            }

            [[nodiscard]] std::vector<font_entry> enumerate(
                std::span<const std::uint8_t> data) const override {
                std::vector<font_entry> result;

                font_entry entry;
                entry.type = font_type::BITMAP;
                entry.weight = 400;
                entry.italic = false;

                if (data.size() >= 84) {
                    entry.point_size = static_cast<uint16_t>(data[2] | (data[3] << 8));
                    entry.name = std::string(
                        reinterpret_cast<const char*>(data.data() + 4),
                        strnlen(reinterpret_cast<const char*>(data.data() + 4), 32)
                    );
                    entry.pixel_height = static_cast<uint16_t>(data[82] | (data[83] << 8));
                }

                if (entry.name.empty()) {
                    entry.name = "GEM Font";
                }

                result.push_back(std::move(entry));
                return result;
            }

            [[nodiscard]] bitmap_font load(
                std::span<const std::uint8_t> data,
                std::size_t index) const override {
                THROW_IF(index != 0, std::out_of_range, "GEM fonts contain only one font");
                return internal::gem_font_loader::load(data);
            }
        };

#if defined(ONYX_FONT_HAS_LOADER_FON)
        // Windows bitmap FON/FNT decoder
        class win_bitmap_fon_decoder_impl : public bitmap_font_decoder {
        public:
            [[nodiscard]] std::string_view name() const noexcept override {
                return "win_bitmap";
            }

            [[nodiscard]] std::span<const std::string_view> extensions() const noexcept override {
                static constexpr std::string_view ext[] = {".fon", ".fnt"};
                return ext;
            }

            [[nodiscard]] bool sniff(std::span<const std::uint8_t> data) const noexcept override {
                if (data.size() < 4) return false;

                // Check for raw FNT file
                if (is_fnt_file(data)) {
                    auto opt_font = libexe::font_parser::parse(data);
                    return opt_font && opt_font->is_raster();
                }

                // Check for MZ executable
                if (data[0] != 'M' || data[1] != 'Z') return false;

                try {
                    auto exe_format = libexe::executable_factory::detect_format(data);
                    switch (exe_format) {
                        case libexe::format_type::NE_WIN16:
                        case libexe::format_type::PE_WIN32:
                        case libexe::format_type::PE_PLUS_WIN64:
                        case libexe::format_type::LX_OS2_BOUND:
                        case libexe::format_type::LX_OS2_RAW:
                            return true;
                        default:
                            return false;
                    }
                } catch (...) {
                    return false;
                }
            }

            [[nodiscard]] std::vector<font_entry> enumerate(
                std::span<const std::uint8_t> data) const override {
                std::vector<font_entry> result;

                if (is_fnt_file(data)) {
                    auto opt_font = libexe::font_parser::parse(data);
                    if (opt_font && opt_font->is_raster()) {
                        result.push_back(make_entry_from_font_data(*opt_font));
                    }
                    return result;
                }

                auto exe_format = libexe::executable_factory::detect_format(data);
                std::vector<libexe::font_data> fonts;

                switch (exe_format) {
                    case libexe::format_type::NE_WIN16:
                        fonts = load_ne_fonts(data, true);
                        break;
                    case libexe::format_type::PE_WIN32:
                    case libexe::format_type::PE_PLUS_WIN64:
                        fonts = load_pe_fonts(data, true);
                        break;
                    case libexe::format_type::LX_OS2_BOUND:
                    case libexe::format_type::LX_OS2_RAW:
                        fonts = load_lx_fonts(data, true);
                        break;
                    default:
                        break;
                }

                for (const auto& f : fonts) {
                    result.push_back(make_entry_from_font_data(f));
                }

                return result;
            }

            [[nodiscard]] bitmap_font load(
                std::span<const std::uint8_t> data,
                std::size_t index) const override {

                if (is_fnt_file(data)) {
                    THROW_IF(index != 0, std::out_of_range, "FNT files contain only one font");
                    auto opt_font = libexe::font_parser::parse(data);
                    THROW_IF(!opt_font || !opt_font->is_raster(), std::runtime_error,
                             "Not a bitmap FNT file");
                    return internal::win_bitmap_fon_loader::load(*opt_font);
                }

                auto exe_format = libexe::executable_factory::detect_format(data);
                std::vector<libexe::font_data> fonts;

                switch (exe_format) {
                    case libexe::format_type::NE_WIN16:
                        fonts = load_ne_fonts(data, true);
                        break;
                    case libexe::format_type::PE_WIN32:
                    case libexe::format_type::PE_PLUS_WIN64:
                        fonts = load_pe_fonts(data, true);
                        break;
                    case libexe::format_type::LX_OS2_BOUND:
                    case libexe::format_type::LX_OS2_RAW:
                        fonts = load_lx_fonts(data, true);
                        break;
                    default:
                        THROW_RUNTIME("Unsupported executable format");
                }

                THROW_IF(index >= fonts.size(), std::out_of_range,
                         "Font index out of range");

                return internal::win_bitmap_fon_loader::load(fonts[index]);
            }
        };
#endif  // ONYX_FONT_HAS_LOADER_FON

    }  // anonymous namespace

    // =========================================================================
    // Built-in Vector Decoders
    // =========================================================================

    namespace {

        // BGI font decoder
        class bgi_font_decoder_impl : public vector_font_decoder {
        public:
            [[nodiscard]] std::string_view name() const noexcept override {
                return "bgi";
            }

            [[nodiscard]] std::span<const std::string_view> extensions() const noexcept override {
                static constexpr std::string_view ext[] = {".chr"};
                return ext;
            }

            [[nodiscard]] bool sniff(std::span<const std::uint8_t> data) const noexcept override {
                return starts_with(data, BGI_MAGIC);
            }

            [[nodiscard]] std::vector<font_entry> enumerate(
                std::span<const std::uint8_t> data) const override {
                std::vector<font_entry> result;

                font_entry entry;
                entry.type = font_type::VECTOR;
                entry.pixel_height = 0;
                entry.point_size = 0;
                entry.weight = 400;
                entry.italic = false;

                // Extract name from BGI header
                for (size_t i = 2; i < std::min(data.size(), size_t{255}); ++i) {
                    if (data[i] == 0x1A && i + 7 < data.size()) {
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

                result.push_back(std::move(entry));
                return result;
            }

            [[nodiscard]] vector_font load(
                std::span<const std::uint8_t> data,
                std::size_t index) const override {
                THROW_IF(index != 0, std::out_of_range, "BGI fonts contain only one font");
                return internal::bgi_font_loader::load(data);
            }
        };

#if defined(ONYX_FONT_HAS_LOADER_FON)
        // Windows vector FON/FNT decoder
        class win_vector_fon_decoder_impl : public vector_font_decoder {
        public:
            [[nodiscard]] std::string_view name() const noexcept override {
                return "win_vector";
            }

            [[nodiscard]] std::span<const std::string_view> extensions() const noexcept override {
                static constexpr std::string_view ext[] = {".fon", ".fnt"};
                return ext;
            }

            [[nodiscard]] bool sniff(std::span<const std::uint8_t> data) const noexcept override {
                if (data.size() < 4) return false;

                // Check for raw FNT file
                if (is_fnt_file(data)) {
                    auto opt_font = libexe::font_parser::parse(data);
                    return opt_font && opt_font->is_vector();
                }

                // Check for MZ executable
                if (data[0] != 'M' || data[1] != 'Z') return false;

                try {
                    auto exe_format = libexe::executable_factory::detect_format(data);
                    switch (exe_format) {
                        case libexe::format_type::NE_WIN16:
                        case libexe::format_type::PE_WIN32:
                        case libexe::format_type::PE_PLUS_WIN64:
                        case libexe::format_type::LX_OS2_BOUND:
                        case libexe::format_type::LX_OS2_RAW:
                            return true;
                        default:
                            return false;
                    }
                } catch (...) {
                    return false;
                }
            }

            [[nodiscard]] std::vector<font_entry> enumerate(
                std::span<const std::uint8_t> data) const override {
                std::vector<font_entry> result;

                if (is_fnt_file(data)) {
                    auto opt_font = libexe::font_parser::parse(data);
                    if (opt_font && opt_font->is_vector()) {
                        result.push_back(make_entry_from_font_data(*opt_font));
                    }
                    return result;
                }

                auto exe_format = libexe::executable_factory::detect_format(data);
                std::vector<libexe::font_data> fonts;

                switch (exe_format) {
                    case libexe::format_type::NE_WIN16:
                        fonts = load_ne_fonts(data, false);
                        break;
                    case libexe::format_type::PE_WIN32:
                    case libexe::format_type::PE_PLUS_WIN64:
                        fonts = load_pe_fonts(data, false);
                        break;
                    case libexe::format_type::LX_OS2_BOUND:
                    case libexe::format_type::LX_OS2_RAW:
                        fonts = load_lx_fonts(data, false);
                        break;
                    default:
                        break;
                }

                for (const auto& f : fonts) {
                    result.push_back(make_entry_from_font_data(f));
                }

                return result;
            }

            [[nodiscard]] vector_font load(
                std::span<const std::uint8_t> data,
                std::size_t index) const override {

                if (is_fnt_file(data)) {
                    THROW_IF(index != 0, std::out_of_range, "FNT files contain only one font");
                    auto opt_font = libexe::font_parser::parse(data);
                    THROW_IF(!opt_font || !opt_font->is_vector(), std::runtime_error,
                             "Not a vector FNT file");
                    return internal::win_vector_fon_loader::load(*opt_font);
                }

                auto exe_format = libexe::executable_factory::detect_format(data);
                std::vector<libexe::font_data> fonts;

                switch (exe_format) {
                    case libexe::format_type::NE_WIN16:
                        fonts = load_ne_fonts(data, false);
                        break;
                    case libexe::format_type::PE_WIN32:
                    case libexe::format_type::PE_PLUS_WIN64:
                        fonts = load_pe_fonts(data, false);
                        break;
                    case libexe::format_type::LX_OS2_BOUND:
                    case libexe::format_type::LX_OS2_RAW:
                        fonts = load_lx_fonts(data, false);
                        break;
                    default:
                        THROW_RUNTIME("Unsupported executable format");
                }

                THROW_IF(index >= fonts.size(), std::out_of_range,
                         "Font index out of range");

                return internal::win_vector_fon_loader::load(fonts[index]);
            }
        };
#endif  // ONYX_FONT_HAS_LOADER_FON

    }  // anonymous namespace

    // =========================================================================
    // Built-in Outline Decoders
    // =========================================================================

    namespace {

#if defined(ONYX_FONT_HAS_LOADER_TTF)
        // TTF/OTF/TTC decoder
        class ttf_decoder_impl : public outline_font_decoder {
        public:
            [[nodiscard]] std::string_view name() const noexcept override {
                return "ttf";
            }

            [[nodiscard]] std::span<const std::string_view> extensions() const noexcept override {
                static constexpr std::string_view ext[] = {".ttf", ".otf", ".ttc"};
                return ext;
            }

            [[nodiscard]] bool sniff(std::span<const std::uint8_t> data) const noexcept override {
                return starts_with(data, TTF_MAGIC) ||
                       starts_with(data, OTF_MAGIC) ||
                       starts_with(data, TTC_MAGIC);
            }

            [[nodiscard]] std::vector<font_entry> enumerate(
                std::span<const std::uint8_t> data) const override {
                std::vector<font_entry> result;

                int font_count = ttf_font::get_font_count(data);
                for (int i = 0; i < font_count; ++i) {
                    font_entry entry;
                    entry.type = font_type::OUTLINE;
                    entry.pixel_height = 0;
                    entry.point_size = 0;
                    entry.weight = 400;
                    entry.italic = false;

                    try {
                        ttf_font font(data, i);
                        if (font.is_valid()) {
                            entry.name = "Font " + std::to_string(i);
                        }
                    } catch (...) {
                        entry.name = "Font " + std::to_string(i);
                    }

                    result.push_back(std::move(entry));
                }

                return result;
            }

            [[nodiscard]] ttf_font load(
                std::span<const std::uint8_t> data,
                std::size_t index) const override {
                int font_count = ttf_font::get_font_count(data);
                THROW_IF(index >= static_cast<std::size_t>(font_count), std::out_of_range,
                         "Font index out of range");
                return ttf_font(data, static_cast<int>(index));
            }
        };
#endif  // ONYX_FONT_HAS_LOADER_TTF

    }  // anonymous namespace

    // =========================================================================
    // Default load_all implementations
    // =========================================================================

    std::vector<bitmap_font> bitmap_font_decoder::load_all(
        std::span<const std::uint8_t> data) const {
        std::vector<bitmap_font> result;
        auto entries = enumerate(data);
        for (std::size_t i = 0; i < entries.size(); ++i) {
            result.push_back(load(data, i));
        }
        return result;
    }

    std::vector<vector_font> vector_font_decoder::load_all(
        std::span<const std::uint8_t> data) const {
        std::vector<vector_font> result;
        auto entries = enumerate(data);
        for (std::size_t i = 0; i < entries.size(); ++i) {
            result.push_back(load(data, i));
        }
        return result;
    }

#if defined(ONYX_FONT_HAS_LOADER_TTF)
    std::vector<ttf_font> outline_font_decoder::load_all(
        std::span<const std::uint8_t> data) const {
        std::vector<ttf_font> result;
        auto entries = enumerate(data);
        for (std::size_t i = 0; i < entries.size(); ++i) {
            result.push_back(load(data, i));
        }
        return result;
    }
#endif  // ONYX_FONT_HAS_LOADER_TTF

    // =========================================================================
    // Registry Implementation
    // =========================================================================

    struct font_registry::impl {
        std::vector<bitmap_font_decoder_ptr> bitmap_decoders;
        std::vector<vector_font_decoder_ptr> vector_decoders;
        std::vector<outline_font_decoder_ptr> outline_decoders;
        std::mutex mutex;
    };

    font_registry::font_registry() : m_impl(std::make_unique<impl>()) {
        register_builtin_decoders();
    }

    font_registry::~font_registry() = default;

    font_registry& font_registry::instance() {
        static font_registry registry;
        return registry;
    }

    void font_registry::register_builtin_decoders() {
        // Bitmap decoders
        m_impl->bitmap_decoders.push_back(std::make_unique<gem_font_decoder_impl>());
#if defined(ONYX_FONT_HAS_LOADER_FON)
        m_impl->bitmap_decoders.push_back(std::make_unique<win_bitmap_fon_decoder_impl>());
#endif

        // Vector decoders
        m_impl->vector_decoders.push_back(std::make_unique<bgi_font_decoder_impl>());
#if defined(ONYX_FONT_HAS_LOADER_FON)
        m_impl->vector_decoders.push_back(std::make_unique<win_vector_fon_decoder_impl>());
#endif

        // Outline decoders
#if defined(ONYX_FONT_HAS_LOADER_TTF)
        m_impl->outline_decoders.push_back(std::make_unique<ttf_decoder_impl>());
#endif
    }

    // Registration methods
    void font_registry::register_decoder(bitmap_font_decoder_ptr decoder) {
        std::lock_guard<std::mutex> lock(m_impl->mutex);
        m_impl->bitmap_decoders.push_back(std::move(decoder));
    }

    void font_registry::register_decoder(vector_font_decoder_ptr decoder) {
        std::lock_guard<std::mutex> lock(m_impl->mutex);
        m_impl->vector_decoders.push_back(std::move(decoder));
    }

    void font_registry::register_decoder(outline_font_decoder_ptr decoder) {
        std::lock_guard<std::mutex> lock(m_impl->mutex);
        m_impl->outline_decoders.push_back(std::move(decoder));
    }

    void font_registry::register_decoder_first(bitmap_font_decoder_ptr decoder) {
        std::lock_guard<std::mutex> lock(m_impl->mutex);
        m_impl->bitmap_decoders.insert(m_impl->bitmap_decoders.begin(), std::move(decoder));
    }

    void font_registry::register_decoder_first(vector_font_decoder_ptr decoder) {
        std::lock_guard<std::mutex> lock(m_impl->mutex);
        m_impl->vector_decoders.insert(m_impl->vector_decoders.begin(), std::move(decoder));
    }

    void font_registry::register_decoder_first(outline_font_decoder_ptr decoder) {
        std::lock_guard<std::mutex> lock(m_impl->mutex);
        m_impl->outline_decoders.insert(m_impl->outline_decoders.begin(), std::move(decoder));
    }

    // Lookup by sniffing
    const bitmap_font_decoder* font_registry::find_bitmap_decoder(
        std::span<const std::uint8_t> data) const noexcept {
        std::lock_guard<std::mutex> lock(m_impl->mutex);
        for (const auto& dec : m_impl->bitmap_decoders) {
            if (dec->sniff(data)) {
                return dec.get();
            }
        }
        return nullptr;
    }

    const vector_font_decoder* font_registry::find_vector_decoder(
        std::span<const std::uint8_t> data) const noexcept {
        std::lock_guard<std::mutex> lock(m_impl->mutex);
        for (const auto& dec : m_impl->vector_decoders) {
            if (dec->sniff(data)) {
                return dec.get();
            }
        }
        return nullptr;
    }

    const outline_font_decoder* font_registry::find_outline_decoder(
        std::span<const std::uint8_t> data) const noexcept {
        std::lock_guard<std::mutex> lock(m_impl->mutex);
        for (const auto& dec : m_impl->outline_decoders) {
            if (dec->sniff(data)) {
                return dec.get();
            }
        }
        return nullptr;
    }

    // Lookup by name
    const bitmap_font_decoder* font_registry::find_bitmap_decoder(
        std::string_view name) const noexcept {
        std::lock_guard<std::mutex> lock(m_impl->mutex);
        for (const auto& dec : m_impl->bitmap_decoders) {
            if (dec->name() == name) {
                return dec.get();
            }
        }
        return nullptr;
    }

    const vector_font_decoder* font_registry::find_vector_decoder(
        std::string_view name) const noexcept {
        std::lock_guard<std::mutex> lock(m_impl->mutex);
        for (const auto& dec : m_impl->vector_decoders) {
            if (dec->name() == name) {
                return dec.get();
            }
        }
        return nullptr;
    }

    const outline_font_decoder* font_registry::find_outline_decoder(
        std::string_view name) const noexcept {
        std::lock_guard<std::mutex> lock(m_impl->mutex);
        for (const auto& dec : m_impl->outline_decoders) {
            if (dec->name() == name) {
                return dec.get();
            }
        }
        return nullptr;
    }

    // Enumeration
    std::size_t font_registry::bitmap_decoder_count() const noexcept {
        std::lock_guard<std::mutex> lock(m_impl->mutex);
        return m_impl->bitmap_decoders.size();
    }

    const bitmap_font_decoder* font_registry::bitmap_decoder_at(
        std::size_t index) const noexcept {
        std::lock_guard<std::mutex> lock(m_impl->mutex);
        if (index >= m_impl->bitmap_decoders.size()) {
            return nullptr;
        }
        return m_impl->bitmap_decoders[index].get();
    }

    std::size_t font_registry::vector_decoder_count() const noexcept {
        std::lock_guard<std::mutex> lock(m_impl->mutex);
        return m_impl->vector_decoders.size();
    }

    const vector_font_decoder* font_registry::vector_decoder_at(
        std::size_t index) const noexcept {
        std::lock_guard<std::mutex> lock(m_impl->mutex);
        if (index >= m_impl->vector_decoders.size()) {
            return nullptr;
        }
        return m_impl->vector_decoders[index].get();
    }

    std::size_t font_registry::outline_decoder_count() const noexcept {
        std::lock_guard<std::mutex> lock(m_impl->mutex);
        return m_impl->outline_decoders.size();
    }

    const outline_font_decoder* font_registry::outline_decoder_at(
        std::size_t index) const noexcept {
        std::lock_guard<std::mutex> lock(m_impl->mutex);
        if (index >= m_impl->outline_decoders.size()) {
            return nullptr;
        }
        return m_impl->outline_decoders[index].get();
    }

}  // namespace onyx_font
