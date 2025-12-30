//
// Created by igor on 30/12/2025.
//
// Shared FreeType library singleton with RAII cleanup.
//

#pragma once

#include <ft2build.h>
#include FT_FREETYPE_H

namespace onyx_font::detail {

    /**
     * @brief RAII singleton for FreeType library instance.
     *
     * Ensures FreeType is initialized on first use and properly
     * cleaned up at program exit. Thread-safe via C++11 static
     * initialization guarantees.
     */
    class freetype_library {
    public:
        /**
         * @brief Get the shared FreeType library instance.
         * @return FreeType library handle, or nullptr on init failure
         */
        static FT_Library get() {
            static freetype_library instance;
            return instance.m_library;
        }

    private:
        freetype_library() {
            FT_Init_FreeType(&m_library);
        }

        ~freetype_library() {
            if (m_library) {
                FT_Done_FreeType(m_library);
            }
        }

        freetype_library(const freetype_library&) = delete;
        freetype_library& operator=(const freetype_library&) = delete;

        FT_Library m_library = nullptr;
    };

} // namespace onyx_font::detail
