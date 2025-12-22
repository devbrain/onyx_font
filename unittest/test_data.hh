//
// Created by igor on 21/12/2025.
//
// Unified test data accessor for onyx_font unit tests
//

#pragma once

#include <filesystem>
#include <fstream>
#include <vector>
#include <cstdint>
#include <stdexcept>
#include <span>

namespace onyx_font::test {

    /// Test data directory accessor
    /// Provides unified access to test font files
    class test_data {
    public:
        /// Get the base path to test data directory
        [[nodiscard]] static std::filesystem::path base_path() {
            // CMake will define TESTDATA_DIR, or we fall back to relative path
#ifdef TESTDATA_DIR
            return std::filesystem::path(TESTDATA_DIR);
#else
            // Fallback: assume running from build directory
            return std::filesystem::path(__FILE__).parent_path() / "testdata";
#endif
        }

        // =====================================================================
        // Path accessors for specific test files
        // =====================================================================

        /// Windows FNT file (raw bitmap font)
        [[nodiscard]] static std::filesystem::path fnt_romanp() {
            return base_path() / "winfonts" / "ROMANP.FNT";
        }

        /// Windows FON file (NE executable with embedded fonts)
        [[nodiscard]] static std::filesystem::path fon_helva() {
            return base_path() / "winfonts" / "HELVA.FON";
        }

        /// BGI vector font (.CHR format)
        [[nodiscard]] static std::filesystem::path bgi_litt() {
            return base_path() / "bgi" / "LITT.CHR";
        }

        /// Windows FON file (VGAOEM - OEM bitmap font)
        [[nodiscard]] static std::filesystem::path fon_vgaoem() {
            return base_path() / "winfonts" / "VGAOEM.FON";
        }

        /// Windows FOT file (font resource file pointing to TTF)
        [[nodiscard]] static std::filesystem::path fot_arial() {
            return base_path() / "winfonts" / "ARIAL.FOT";
        }

        /// TrueType font file (Marlett - symbol font)
        [[nodiscard]] static std::filesystem::path ttf_marlett() {
            return base_path() / "ttf" / "marlett.ttf";
        }

        /// TrueType font file (Arial - standard font)
        [[nodiscard]] static std::filesystem::path ttf_arial() {
            return base_path() / "winfonts" / "ARIAL.TTF";
        }

        /// OS/2 system font (NE format)
        [[nodiscard]] static std::filesystem::path fon_os2_sysfont() {
            return base_path() / "os2" / "SYSFONT.DLL";
        }

        // =====================================================================
        // File loading utilities
        // =====================================================================

        /// Load a file into a byte vector
        [[nodiscard]] static std::vector<uint8_t> load_file(const std::filesystem::path& path) {
            std::ifstream file(path, std::ios::binary | std::ios::ate);
            if (!file) {
                throw std::runtime_error("Failed to open file: " + path.string());
            }

            auto size = file.tellg();
            if (size <= 0) {
                throw std::runtime_error("Empty or invalid file: " + path.string());
            }

            std::vector<uint8_t> data(static_cast<size_t>(size));
            file.seekg(0);
            file.read(reinterpret_cast<char*>(data.data()), size);

            if (!file) {
                throw std::runtime_error("Failed to read file: " + path.string());
            }

            return data;
        }

        /// Load Windows FNT file
        [[nodiscard]] static std::vector<uint8_t> load_fnt_romanp() {
            return load_file(fnt_romanp());
        }

        /// Load Windows FON file
        [[nodiscard]] static std::vector<uint8_t> load_fon_helva() {
            return load_file(fon_helva());
        }

        /// Load BGI vector font
        [[nodiscard]] static std::vector<uint8_t> load_bgi_litt() {
            return load_file(bgi_litt());
        }

        /// Load VGAOEM FON file
        [[nodiscard]] static std::vector<uint8_t> load_fon_vgaoem() {
            return load_file(fon_vgaoem());
        }

        /// Load Arial FOT file
        [[nodiscard]] static std::vector<uint8_t> load_fot_arial() {
            return load_file(fot_arial());
        }

        /// Load Marlett TTF file
        [[nodiscard]] static std::vector<uint8_t> load_ttf_marlett() {
            return load_file(ttf_marlett());
        }

        /// Load Arial TTF file
        [[nodiscard]] static std::vector<uint8_t> load_ttf_arial() {
            return load_file(ttf_arial());
        }

        /// Load OS/2 system font
        [[nodiscard]] static std::vector<uint8_t> load_fon_os2_sysfont() {
            return load_file(fon_os2_sysfont());
        }

        // =====================================================================
        // Existence checks
        // =====================================================================

        /// Check if all test data files exist
        [[nodiscard]] static bool all_files_exist() {
            return std::filesystem::exists(fnt_romanp()) &&
                   std::filesystem::exists(fon_helva()) &&
                   std::filesystem::exists(bgi_litt()) &&
                   std::filesystem::exists(fon_vgaoem()) &&
                   std::filesystem::exists(fot_arial()) &&
                   std::filesystem::exists(ttf_marlett());
        }

        /// Check if a specific file exists
        [[nodiscard]] static bool file_exists(const std::filesystem::path& path) {
            return std::filesystem::exists(path);
        }
    };

    /// RAII wrapper for loaded font data
    /// Keeps data alive and provides span access
    class font_data_holder {
    public:
        explicit font_data_holder(std::vector<uint8_t> data)
            : m_data(std::move(data)) {}

        [[nodiscard]] std::span<const uint8_t> span() const {
            return {m_data.data(), m_data.size()};
        }

        [[nodiscard]] const uint8_t* data() const {
            return m_data.data();
        }

        [[nodiscard]] size_t size() const {
            return m_data.size();
        }

    private:
        std::vector<uint8_t> m_data;
    };

}  // namespace onyx_font::test
