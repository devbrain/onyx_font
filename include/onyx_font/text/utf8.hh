/**
 * @file utf8.hh
 * @brief UTF-8 decoding utilities for text rendering.
 *
 * This file provides utilities for decoding UTF-8 encoded strings into
 * Unicode codepoints. These utilities are used throughout the text rendering
 * system to handle multi-byte character sequences.
 *
 * @section utf8_overview Overview
 *
 * UTF-8 is a variable-width encoding where each Unicode codepoint is
 * represented by 1-4 bytes:
 *
 * | Bytes | Range | Description |
 * |-------|-------|-------------|
 * | 1 | U+0000 - U+007F | ASCII compatible |
 * | 2 | U+0080 - U+07FF | Latin, Greek, Cyrillic, etc. |
 * | 3 | U+0800 - U+FFFF | Most other scripts, BMP |
 * | 4 | U+10000 - U+10FFFF | Emoji, rare scripts |
 *
 * @section utf8_usage Usage Examples
 *
 * @subsection utf8_iterate Iterating Over Characters
 *
 * @code{.cpp}
 * std::string text = "Hello, 世界! 🎉";
 *
 * // Range-based for loop
 * for (char32_t cp : utf8_view(text)) {
 *     std::cout << "Codepoint: U+" << std::hex << cp << "\n";
 * }
 *
 * // Iterator-based
 * for (auto it = utf8_iterator(text); it != utf8_iterator(); ++it) {
 *     render_glyph(*it);
 * }
 * @endcode
 *
 * @subsection utf8_decode Single Decoding
 *
 * @code{.cpp}
 * std::string_view text = "Hello";
 *
 * while (!text.empty()) {
 *     auto [codepoint, bytes] = utf8_decode_one(text);
 *     if (bytes == 0) break;  // Empty or error
 *
 *     process_codepoint(codepoint);
 *     text.remove_prefix(bytes);
 * }
 * @endcode
 *
 * @section utf8_errors Error Handling
 *
 * Invalid UTF-8 sequences are replaced with U+FFFD (replacement character).
 * This provides robust handling of malformed input without exceptions.
 *
 * @author Igor
 * @date 21/12/2025
 */

#pragma once

#include <onyx_font/export.h>
#include <cstdint>
#include <cstddef>
#include <string_view>
#include <iterator>

namespace onyx_font {
    /**
     * @brief Result of decoding a single UTF-8 codepoint.
     *
     * Contains both the decoded codepoint and the number of bytes
     * consumed from the input. This allows efficient sequential
     * decoding without repeatedly scanning the input.
     */
    struct ONYX_FONT_EXPORT utf8_decode_result {
        /**
         * @brief Decoded Unicode codepoint.
         *
         * Contains the decoded value on success, or U+FFFD (0xFFFD)
         * on error or empty input. Check bytes_consumed to distinguish
         * between error conditions.
         */
        char32_t codepoint;

        /**
         * @brief Number of bytes consumed from input.
         *
         * - **1-4**: Normal decoding, bytes consumed
         * - **0**: Empty input (nothing to decode)
         *
         * @warning Always check for bytes_consumed == 0 when iterating
         *          to avoid infinite loops on empty input.
         */
        int bytes_consumed;
    };

    /**
     * @brief Decode a single UTF-8 codepoint from the beginning of a string.
     *
     * Decodes the first UTF-8 codepoint from the input string view.
     * Handles all valid UTF-8 sequences (1-4 bytes) and detects
     * invalid sequences.
     *
     * @param str UTF-8 encoded string view
     * @return Decode result containing codepoint and bytes consumed
     *
     * @note If str is empty, returns {0xFFFD, 0}. Callers iterating should
     *       check for bytes_consumed == 0 to avoid infinite loops.
     *
     * @section decode_example Example
     *
     * @code{.cpp}
     * std::string_view text = "αβγ";
     *
     * while (!text.empty()) {
     *     auto result = utf8_decode_one(text);
     *     if (result.bytes_consumed == 0) break;
     *
     *     std::cout << "U+" << std::hex << result.codepoint << "\n";
     *     text.remove_prefix(result.bytes_consumed);
     * }
     * // Output: U+3b1 U+3b2 U+3b3 (Greek letters)
     * @endcode
     */
    ONYX_FONT_EXPORT utf8_decode_result utf8_decode_one(std::string_view str);

    /**
     * @brief Count the number of codepoints in a UTF-8 string.
     *
     * Counts Unicode codepoints (not bytes). This is useful for
     * determining the logical length of a string for display purposes.
     *
     * @param str UTF-8 encoded string
     * @return Number of Unicode codepoints
     *
     * @note This is O(n) where n is the byte length of the string,
     *       as each byte must be examined to determine codepoint boundaries.
     *
     * @code{.cpp}
     * std::string text = "Hello, 世界!";
     * std::cout << "Bytes: " << text.size() << "\n";      // 16 bytes
     * std::cout << "Chars: " << utf8_length(text) << "\n"; // 10 codepoints
     * @endcode
     */
    ONYX_FONT_EXPORT std::size_t utf8_length(std::string_view str);

    /**
     * @brief Forward iterator for UTF-8 codepoints.
     *
     * Provides STL-compatible forward iteration over Unicode codepoints
     * in a UTF-8 encoded string. Automatically handles multi-byte
     * sequences and invalid UTF-8 (with replacement character).
     *
     * @section iterator_usage Usage
     *
     * @code{.cpp}
     * std::string text = "Café ☕";
     *
     * for (auto it = utf8_iterator(text); it != utf8_iterator(); ++it) {
     *     char32_t cp = *it;
     *     // Process each codepoint
     * }
     * @endcode
     *
     * @note Prefer utf8_view for cleaner range-based iteration.
     *
     * @see utf8_view For range-based for loop support
     */
    class ONYX_FONT_EXPORT utf8_iterator {
    public:
        /// @name Type Aliases (STL Iterator Requirements)
        /// @{
        using value_type = char32_t;              ///< Codepoint type
        using difference_type = std::ptrdiff_t;   ///< Difference type
        using pointer = const char32_t*;          ///< Pointer type
        using reference = char32_t;               ///< Reference type (value, not ref)
        using iterator_category = std::forward_iterator_tag;  ///< Iterator category
        /// @}

        /**
         * @brief Default constructor creates end iterator.
         *
         * The default-constructed iterator compares equal to the end
         * of any utf8_iterator sequence.
         */
        utf8_iterator() = default;

        /**
         * @brief Construct iterator at beginning of string.
         *
         * @param str UTF-8 encoded string to iterate over
         *
         * @note The string_view must remain valid for the iterator's lifetime.
         */
        explicit utf8_iterator(std::string_view str);

        /**
         * @brief Get current codepoint.
         * @return Current Unicode codepoint
         * @pre Iterator is not at end
         */
        char32_t operator*() const { return m_current; }

        /**
         * @brief Advance to next codepoint.
         * @return Reference to this iterator
         */
        utf8_iterator& operator++();

        /**
         * @brief Post-increment (less efficient, prefer pre-increment).
         * @return Copy of iterator before increment
         */
        utf8_iterator operator++(int);

        /**
         * @brief Equality comparison.
         * @param other Iterator to compare with
         * @return true if both iterators are at the same position
         */
        bool operator==(const utf8_iterator& other) const;

        /**
         * @brief Inequality comparison.
         * @param other Iterator to compare with
         * @return true if iterators are at different positions
         */
        bool operator!=(const utf8_iterator& other) const { return !(*this == other); }

    private:
        std::string_view m_remaining;  ///< Remaining string to decode
        char32_t m_current = 0;        ///< Current decoded codepoint
        bool m_at_end = true;          ///< True if no more codepoints to yield

        /// Decode next codepoint from remaining string
        void decode_next();
    };

    /**
     * @brief Range wrapper for iterating UTF-8 codepoints.
     *
     * Provides begin() and end() methods for range-based for loops,
     * making UTF-8 iteration clean and intuitive.
     *
     * @section view_usage Usage
     *
     * @code{.cpp}
     * std::string text = "Привет мир! 🌍";
     *
     * for (char32_t codepoint : utf8_view(text)) {
     *     // Handle each codepoint
     *     if (codepoint >= 0x1F300 && codepoint <= 0x1F9FF) {
     *         std::cout << "Found emoji!\n";
     *     }
     * }
     * @endcode
     *
     * @note The underlying string must remain valid for the view's lifetime.
     */
    class ONYX_FONT_EXPORT utf8_view {
    public:
        /**
         * @brief Construct view over UTF-8 string.
         * @param str UTF-8 encoded string (must remain valid)
         */
        explicit utf8_view(std::string_view str)
            : m_str(str) {
        }

        /**
         * @brief Get iterator to beginning.
         * @return Iterator pointing to first codepoint
         */
        [[nodiscard]] utf8_iterator begin() const { return utf8_iterator(m_str); }

        /**
         * @brief Get iterator to end.
         * @return End sentinel iterator
         */
        [[nodiscard]] utf8_iterator end() const { return utf8_iterator(); }

    private:
        std::string_view m_str;  ///< Underlying UTF-8 string
    };
} // namespace onyx_font
