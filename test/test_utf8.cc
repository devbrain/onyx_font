//
// Created by igor on 21/12/2025.
//
// Unit tests for UTF-8 decoding
//

#include <doctest/doctest.h>
#include <onyx_font/text/utf8.hh>
#include <vector>

using namespace onyx_font;

TEST_SUITE("utf8") {

    TEST_CASE("decode ASCII") {
        auto [cp, len] = utf8_decode_one("Hello");
        CHECK(cp == 'H');
        CHECK(len == 1);

        auto [cp2, len2] = utf8_decode_one("A");
        CHECK(cp2 == 'A');
        CHECK(len2 == 1);

        auto [cp3, len3] = utf8_decode_one(" ");
        CHECK(cp3 == ' ');
        CHECK(len3 == 1);
    }

    TEST_CASE("decode 2-byte UTF-8") {
        // é (U+00E9) = C3 A9
        auto [cp, len] = utf8_decode_one("\xC3\xA9");
        CHECK(cp == 0x00E9);
        CHECK(len == 2);

        // ñ (U+00F1) = C3 B1
        auto [cp2, len2] = utf8_decode_one("\xC3\xB1");
        CHECK(cp2 == 0x00F1);
        CHECK(len2 == 2);

        // © (U+00A9) = C2 A9
        auto [cp3, len3] = utf8_decode_one("\xC2\xA9");
        CHECK(cp3 == 0x00A9);
        CHECK(len3 == 2);
    }

    TEST_CASE("decode 3-byte UTF-8") {
        // 中 (U+4E2D) = E4 B8 AD
        auto [cp, len] = utf8_decode_one("\xE4\xB8\xAD");
        CHECK(cp == 0x4E2D);
        CHECK(len == 3);

        // € (U+20AC) = E2 82 AC
        auto [cp2, len2] = utf8_decode_one("\xE2\x82\xAC");
        CHECK(cp2 == 0x20AC);
        CHECK(len2 == 3);

        // ∞ (U+221E) = E2 88 9E
        auto [cp3, len3] = utf8_decode_one("\xE2\x88\x9E");
        CHECK(cp3 == 0x221E);
        CHECK(len3 == 3);
    }

    TEST_CASE("decode 4-byte UTF-8") {
        // 😀 (U+1F600) = F0 9F 98 80
        auto [cp, len] = utf8_decode_one("\xF0\x9F\x98\x80");
        CHECK(cp == 0x1F600);
        CHECK(len == 4);

        // 🎉 (U+1F389) = F0 9F 8E 89
        auto [cp2, len2] = utf8_decode_one("\xF0\x9F\x8E\x89");
        CHECK(cp2 == 0x1F389);
        CHECK(len2 == 4);

        // 𝄞 (U+1D11E) = F0 9D 84 9E
        auto [cp3, len3] = utf8_decode_one("\xF0\x9D\x84\x9E");
        CHECK(cp3 == 0x1D11E);
        CHECK(len3 == 4);
    }

    TEST_CASE("decode empty string") {
        auto [cp, len] = utf8_decode_one("");
        CHECK(cp == 0xFFFD);  // Replacement character
        CHECK(len == 0);
    }

    TEST_CASE("invalid UTF-8 returns replacement character") {
        // Invalid start byte
        auto [cp1, len1] = utf8_decode_one("\xFF\xFE");
        CHECK(cp1 == 0xFFFD);
        CHECK(len1 == 1);

        // Continuation byte at start
        auto [cp2, len2] = utf8_decode_one("\x80\x80");
        CHECK(cp2 == 0xFFFD);
        CHECK(len2 == 1);

        // Incomplete 2-byte sequence
        auto [cp3, len3] = utf8_decode_one("\xC3");
        CHECK(cp3 == 0xFFFD);
        CHECK(len3 == 1);

        // Incomplete 3-byte sequence
        auto [cp4, len4] = utf8_decode_one("\xE4\xB8");
        CHECK(cp4 == 0xFFFD);
        CHECK(len4 == 1);

        // Incomplete 4-byte sequence
        auto [cp5, len5] = utf8_decode_one("\xF0\x9F\x98");
        CHECK(cp5 == 0xFFFD);
        CHECK(len5 == 1);
    }

    TEST_CASE("overlong encoding rejected") {
        // Overlong encoding of '/' (U+002F)
        // Should be 2F, but encoded as C0 AF
        auto [cp, len] = utf8_decode_one("\xC0\xAF");
        CHECK(cp == 0xFFFD);
    }

    TEST_CASE("surrogate pairs rejected") {
        // U+D800 (high surrogate) = ED A0 80
        auto [cp, len] = utf8_decode_one("\xED\xA0\x80");
        CHECK(cp == 0xFFFD);
    }

    TEST_CASE("iterate UTF-8 string") {
        std::vector<char32_t> codepoints;
        for (char32_t cp : utf8_view("Hello")) {
            codepoints.push_back(cp);
        }
        REQUIRE(codepoints.size() == 5);
        CHECK(codepoints[0] == 'H');
        CHECK(codepoints[1] == 'e');
        CHECK(codepoints[2] == 'l');
        CHECK(codepoints[3] == 'l');
        CHECK(codepoints[4] == 'o');
    }

    TEST_CASE("iterate UTF-8 string with accents") {
        std::vector<char32_t> codepoints;
        // "Héllo" - H, é, l, l, o
        for (char32_t cp : utf8_view("H\xC3\xA9llo")) {
            codepoints.push_back(cp);
        }
        REQUIRE(codepoints.size() == 5);
        CHECK(codepoints[0] == 'H');
        CHECK(codepoints[1] == 0x00E9);  // é
        CHECK(codepoints[2] == 'l');
        CHECK(codepoints[3] == 'l');
        CHECK(codepoints[4] == 'o');
    }

    TEST_CASE("iterate UTF-8 string with CJK") {
        std::vector<char32_t> codepoints;
        // "世界" = U+4E16 U+754C
        for (char32_t cp : utf8_view("\xE4\xB8\x96\xE7\x95\x8C")) {
            codepoints.push_back(cp);
        }
        REQUIRE(codepoints.size() == 2);
        CHECK(codepoints[0] == 0x4E16);  // 世
        CHECK(codepoints[1] == 0x754C);  // 界
    }

    TEST_CASE("iterate UTF-8 string with emoji") {
        std::vector<char32_t> codepoints;
        // "Hi😀" = H, i, 😀
        for (char32_t cp : utf8_view("Hi\xF0\x9F\x98\x80")) {
            codepoints.push_back(cp);
        }
        REQUIRE(codepoints.size() == 3);
        CHECK(codepoints[0] == 'H');
        CHECK(codepoints[1] == 'i');
        CHECK(codepoints[2] == 0x1F600);  // 😀
    }

    TEST_CASE("iterate empty string") {
        std::vector<char32_t> codepoints;
        for (char32_t cp : utf8_view("")) {
            codepoints.push_back(cp);
        }
        CHECK(codepoints.empty());
    }

    TEST_CASE("iterate mixed script string") {
        std::vector<char32_t> codepoints;
        // "A中😀" - ASCII, CJK, emoji
        for (char32_t cp : utf8_view("A\xE4\xB8\xAD\xF0\x9F\x98\x80")) {
            codepoints.push_back(cp);
        }
        REQUIRE(codepoints.size() == 3);
        CHECK(codepoints[0] == 'A');
        CHECK(codepoints[1] == 0x4E2D);  // 中
        CHECK(codepoints[2] == 0x1F600); // 😀
    }

    TEST_CASE("utf8_length") {
        CHECK(utf8_length("") == 0);
        CHECK(utf8_length("Hello") == 5);
        CHECK(utf8_length("H\xC3\xA9llo") == 5);  // Héllo
        CHECK(utf8_length("\xE4\xB8\x96\xE7\x95\x8C") == 2);  // 世界
        CHECK(utf8_length("Hi\xF0\x9F\x98\x80") == 3);  // Hi😀
        CHECK(utf8_length("A\xE4\xB8\xAD\xF0\x9F\x98\x80") == 3);  // A中😀
    }

    TEST_CASE("utf8_length with invalid sequences") {
        // Invalid bytes are counted as 1 codepoint each (replacement char)
        CHECK(utf8_length("\xFF") == 1);
        CHECK(utf8_length("\xFF\xFF") == 2);
    }

    TEST_CASE("iterator equality") {
        utf8_view view("AB");
        auto it1 = view.begin();
        auto it2 = view.begin();
        auto end = view.end();

        CHECK(it1 == it2);
        CHECK(it1 != end);

        ++it1;
        CHECK(it1 != it2);

        ++it2;
        CHECK(it1 == it2);

        ++it1;
        ++it2;
        CHECK(it1 == end);
        CHECK(it2 == end);
        CHECK(it1 == it2);
    }

    TEST_CASE("iterator post-increment") {
        utf8_view view("AB");
        auto it = view.begin();

        auto old = it++;
        CHECK(*old == 'A');
        CHECK(*it == 'B');
    }
}
