//
// Created by igor on 21/12/2025.
//

#include <onyx_font/text/utf8.hh>

namespace onyx_font {

namespace {

// UTF-8 replacement character (used for invalid sequences)
constexpr char32_t REPLACEMENT_CHAR = 0xFFFD;

// Check if byte is a UTF-8 continuation byte (10xxxxxx)
constexpr bool is_continuation(unsigned char c) {
    return (c & 0xC0) == 0x80;
}

} // anonymous namespace

utf8_decode_result utf8_decode_one(std::string_view str) {
    if (str.empty()) {
        return {REPLACEMENT_CHAR, 0};
    }

    const auto* data = reinterpret_cast<const unsigned char*>(str.data());
    const std::size_t len = str.size();

    unsigned char first = data[0];

    // ASCII (0xxxxxxx)
    if (first < 0x80) {
        return {static_cast<char32_t>(first), 1};
    }

    // Invalid: continuation byte at start
    if ((first & 0xC0) == 0x80) {
        return {REPLACEMENT_CHAR, 1};
    }

    // Invalid: starts with 11111xxx
    if (first >= 0xF8) {
        return {REPLACEMENT_CHAR, 1};
    }

    // 2-byte sequence (110xxxxx 10xxxxxx)
    if ((first & 0xE0) == 0xC0) {
        if (len < 2 || !is_continuation(data[1])) {
            return {REPLACEMENT_CHAR, 1};
        }
        char32_t cp = static_cast<char32_t>(((first & 0x1F) << 6) | (data[1] & 0x3F));
        // Check for overlong encoding
        if (cp < 0x80) {
            return {REPLACEMENT_CHAR, 2};
        }
        return {cp, 2};
    }

    // 3-byte sequence (1110xxxx 10xxxxxx 10xxxxxx)
    if ((first & 0xF0) == 0xE0) {
        if (len < 3 || !is_continuation(data[1]) || !is_continuation(data[2])) {
            return {REPLACEMENT_CHAR, 1};
        }
        char32_t cp = static_cast<char32_t>(((first & 0x0F) << 12) |
                      ((data[1] & 0x3F) << 6) |
                      (data[2] & 0x3F));
        // Check for overlong encoding
        if (cp < 0x800) {
            return {REPLACEMENT_CHAR, 3};
        }
        // Check for surrogate pairs (invalid in UTF-8)
        if (cp >= 0xD800 && cp <= 0xDFFF) {
            return {REPLACEMENT_CHAR, 3};
        }
        return {cp, 3};
    }

    // 4-byte sequence (11110xxx 10xxxxxx 10xxxxxx 10xxxxxx)
    if ((first & 0xF8) == 0xF0) {
        if (len < 4 || !is_continuation(data[1]) ||
            !is_continuation(data[2]) || !is_continuation(data[3])) {
            return {REPLACEMENT_CHAR, 1};
        }
        char32_t cp = static_cast<char32_t>(((first & 0x07) << 18) |
                      ((data[1] & 0x3F) << 12) |
                      ((data[2] & 0x3F) << 6) |
                      (data[3] & 0x3F));
        // Check for overlong encoding
        if (cp < 0x10000) {
            return {REPLACEMENT_CHAR, 4};
        }
        // Check for codepoint beyond Unicode range
        if (cp > 0x10FFFF) {
            return {REPLACEMENT_CHAR, 4};
        }
        return {cp, 4};
    }

    // Should not reach here
    return {REPLACEMENT_CHAR, 1};
}

std::size_t utf8_length(std::string_view str) {
    std::size_t count = 0;
    while (!str.empty()) {
        auto [cp, bytes] = utf8_decode_one(str);
        if (bytes == 0) break;
        str.remove_prefix(static_cast<std::size_t>(bytes));
        ++count;
    }
    return count;
}

utf8_iterator::utf8_iterator(std::string_view str)
    : m_remaining(str)
    , m_at_end(str.empty()) {
    if (!m_at_end) {
        auto [cp, bytes] = utf8_decode_one(m_remaining);
        m_current = cp;
        if (bytes > 0) {
            m_remaining.remove_prefix(static_cast<std::size_t>(bytes));
        }
    }
}

void utf8_iterator::decode_next() {
    if (m_remaining.empty()) {
        m_at_end = true;
        m_current = 0;
        return;
    }

    auto [cp, bytes] = utf8_decode_one(m_remaining);
    m_current = cp;
    if (bytes > 0) {
        m_remaining.remove_prefix(static_cast<std::size_t>(bytes));
    } else {
        // Safety: if somehow bytes is 0, mark as end to avoid infinite loop
        m_at_end = true;
    }
}

utf8_iterator& utf8_iterator::operator++() {
    decode_next();
    return *this;
}

utf8_iterator utf8_iterator::operator++(int) {
    utf8_iterator tmp = *this;
    ++(*this);
    return tmp;
}

bool utf8_iterator::operator==(const utf8_iterator& other) const {
    // Both at end are equal
    if (m_at_end && other.m_at_end) {
        return true;
    }
    // One at end, one not - not equal
    if (m_at_end != other.m_at_end) {
        return false;
    }
    // Neither at end - compare positions
    return m_remaining.data() == other.m_remaining.data() &&
           m_remaining.size() == other.m_remaining.size();
}

} // namespace onyx_font
