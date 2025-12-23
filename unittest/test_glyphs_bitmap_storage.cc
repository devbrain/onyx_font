//
// Created by igor on 14/12/2025.
//

#include <stdexcept>
#include <doctest/doctest.h>
#include <../include/onyx_font/utils/bitmap_glyphs_storage.hh>

// --------------------------------------------------------------------------------
// Helper Function for Bitmap Data Generation
// --------------------------------------------------------------------------------
namespace {
    // Create a simple 8x8 bitmap with a diagonal line for testing
    std::vector <std::byte> create_test_bitmap_8x8_msb() {
        // 8x8 bitmap, stride is 1 byte, 8 rows = 8 bytes total
        // Diagonal pixels (0,0), (1,1), (2,2), ..., (7,7) are ON.
        // MSB_FIRST: (0,0) is bit 7, (7,7) is bit 0.
        return {
            std::byte{0b10000000}, // (0,0) is set
            std::byte{0b01000000}, // (1,1) is set
            std::byte{0b00100000}, // (2,2) is set
            std::byte{0b00010000}, // (3,3) is set
            std::byte{0b00001000}, // (4,4) is set
            std::byte{0b00000100}, // (5,5) is set
            std::byte{0b00000010}, // (6,6) is set
            std::byte{0b00000001} // (7,7) is set
        };
    }

    // Create a simple 8x8 bitmap with a diagonal line for testing (LSB_FIRST)
    [[maybe_unused]] std::vector <std::byte> create_test_bitmap_8x8_lsb() {
        // LSB_FIRST: (0,0) is bit 0, (7,7) is bit 7.
        return {
            std::byte{0b00000001}, // (0,0) is set
            std::byte{0b00000010}, // (1,1) is set
            std::byte{0b00000100}, // (2,2) is set
            std::byte{0b00001000}, // (3,3) is set
            std::byte{0b00010000}, // (4,4) is set
            std::byte{0b00100000}, // (5,5) is set
            std::byte{0b01000000}, // (6,6) is set
            std::byte{0b10000000} // (7,7) is set
        };
    }

}
// --------------------------------------------------------------------------------
// Unit Tests for bitmap_builder
// --------------------------------------------------------------------------------

TEST_SUITE("bitmap_builder") {
    using namespace onyx_font;

    TEST_CASE("packed_stride calculation") {
        CHECK(bitmap_builder::packed_stride(1) == 1);
        CHECK(bitmap_builder::packed_stride(8) == 1);
        CHECK(bitmap_builder::packed_stride(9) == 2);
        CHECK(bitmap_builder::packed_stride(16) == 2);
        CHECK(bitmap_builder::packed_stride(17) == 3);
        CHECK(bitmap_builder::packed_stride(0) == 0); // Although not practical for glyphs
    }

    TEST_CASE("Initialization and Reservations") {
        bitmap_builder b(bit_order::lsb_first);
        CHECK(b.order() == bit_order::lsb_first);

        b.reserve_bytes(100); // Check no crash
        b.reserve_glyphs(5); // Check no crash
    }

    TEST_CASE("reserve_glyph and glyph_writer basics") {
        bitmap_builder b;
        CHECK(b.order() == bit_order::msb_first);

        auto w = b.reserve_glyph(10, 5); // 10x5 bitmap, stride 2

        CHECK(w.glyph_index() == 0);
        CHECK(w.width() == 10);
        CHECK(w.height() == 5);
        CHECK(w.stride_bytes() == 2);
        CHECK(w.order() == bit_order::msb_first);

        // Check a second glyph
        auto w2 = b.reserve_glyph(1, 1); // 1x1 bitmap, stride 1
        CHECK(w2.glyph_index() == 1);
    }

    TEST_CASE("MSB_FIRST pixel setting and reading via view") {
        bitmap_builder b(bit_order::msb_first);

        auto w = b.reserve_glyph(8, 2); // 8x2, stride 1, 2 bytes total.

        // (0,0) -> bit 7 of byte 0
        w.set_pixel(0, 0, true);
        // (7,0) -> bit 0 of byte 0
        w.set_pixel(7, 0, true);
        // (4,1) -> bit 3 of byte 1
        w.set_pixel(4, 1, true);

        bitmap_storage storage = std::move(b).build();
        auto view = storage.view(0);

        CHECK(view.width() == 8);
        CHECK(view.height() == 2);
        CHECK(view.stride_bytes() == 1);
        CHECK(view.order() == bit_order::msb_first);

        CHECK(view.pixel(0, 0) == true);
        CHECK(view.pixel(7, 0) == true);
        CHECK(view.pixel(4, 1) == true);

        CHECK(view.pixel(1, 0) == false);
        CHECK(view.pixel(4, 0) == false);
        CHECK(view.pixel(0, 1) == false);
        CHECK(view.pixel(7, 1) == false);

        // Verify raw data via row()
        std::span <const std::byte> row0 = view.row(0);
        std::span <const std::byte> row1 = view.row(1);
        CHECK(row0.size() == 1);
        CHECK(row1.size() == 1);
        CHECK(std::to_integer<std::uint8_t>(row0[0]) == 0b10000001); // bit 7 and bit 0 set
        CHECK(std::to_integer<std::uint8_t>(row1[0]) == 0b00001000); // bit 3 set
    }

    TEST_CASE("LSB_FIRST pixel setting and reading via view") {
        bitmap_builder b(bit_order::lsb_first);

        auto w = b.reserve_glyph(8, 2); // 8x2, stride 1, 2 bytes total.

        // (0,0) -> bit 0 of byte 0
        w.set_pixel(0, 0, true);
        // (7,0) -> bit 7 of byte 0
        w.set_pixel(7, 0, true);
        // (4,1) -> bit 4 of byte 1
        w.set_pixel(4, 1, true);

        bitmap_storage storage = std::move(b).build();
        auto view = storage.view(0);

        CHECK(view.order() == bit_order::lsb_first);

        CHECK(view.pixel(0, 0) == true);
        CHECK(view.pixel(7, 0) == true);
        CHECK(view.pixel(4, 1) == true);

        CHECK(view.pixel(1, 0) == false);
        CHECK(view.pixel(4, 0) == false);
        CHECK(view.pixel(0, 1) == false);
        CHECK(view.pixel(7, 1) == false);

        // Verify raw data via row()
        std::span <const std::byte> row0 = view.row(0);
        std::span <const std::byte> row1 = view.row(1);
        CHECK(std::to_integer<std::uint8_t>(row0[0]) == 0b10000001); // bit 7 and bit 0 set
        CHECK(std::to_integer<std::uint8_t>(row1[0]) == 0b00010000); // bit 4 set
    }

    TEST_CASE("add_glyph_packed and set_row_packed") {
        bitmap_builder b; // MSB_FIRST default
        std::vector <std::byte> packed_rows = create_test_bitmap_8x8_msb(); // 8 bytes

        // Test add_glyph_packed
        std::size_t index = b.add_glyph_packed(8, 8, packed_rows);
        CHECK(index == 0);

        // Test set_row_packed via reserve_glyph
        auto w = b.reserve_glyph(8, 8);
        w.set_row_packed(0, {packed_rows.data() + 0, 1});
        w.set_row_packed(1, {packed_rows.data() + 1, 1});
        // Row 0 and 1 are identical to the first glyph's.

        bitmap_storage storage = std::move(b).build();
        auto view0 = storage.view(0);
        auto view1 = storage.view(1);

        // Check diagonal for view0
        for (std::uint16_t i = 0; i < 8; ++i) {
            CHECK(view0.pixel(i, i) == true);
            CHECK(view0.pixel(i, (i+1)%8) == false); // Check an adjacent pixel is off
        }

        // Check first two rows for view1 (set_row_packed)
        CHECK(view1.pixel(0, 0) == true); // (0,0) set
        CHECK(view1.pixel(7, 0) == false);
        CHECK(view1.pixel(1, 1) == true); // (1,1) set
        CHECK(view1.pixel(0, 1) == false);
        CHECK(view1.pixel(0, 2) == false); // Remaining rows should be 0 from reserve_glyph
    }

    TEST_CASE("add_glyph_from_fn") {
        bitmap_builder b(bit_order::lsb_first);

        std::size_t index = b.add_glyph_from_fn(4, 4, [](std::uint16_t x, std::uint16_t y) {
            return x == y; // Diagonal
        });

        CHECK(index == 0);

        bitmap_storage storage = std::move(b).build();
        auto view = storage.view(0);

        auto dims = storage.dimensions(0);
        CHECK(dims.width == 4);
        CHECK(dims.height == 4);

        for (std::uint16_t y = 0; y < 4; ++y) {
            for (std::uint16_t x = 0; x < 4; ++x) {
                if (x == y) {
                    CHECK(view.pixel(x, y) == true);
                } else {
                    CHECK(view.pixel(x, y) == false);
                }
            }
        }
    }
}

// --------------------------------------------------------------------------------
// Unit Tests for bitmap_storage
// --------------------------------------------------------------------------------

TEST_SUITE("bitmap_storage") {
    using namespace onyx_font;

    // Helper to build a storage object for testing
    bitmap_storage create_test_storage() {
        bitmap_builder b;

        // Glyph 0: 8x8 MSB, Diagonal
        (void)b.add_glyph_packed(8, 8, create_test_bitmap_8x8_msb());

        // Glyph 1: 10x1 LSB, "L.O." (0,0) and (2,0) set. Stride 2.
        bitmap_builder b_lsb(bit_order::lsb_first);
        auto w = b_lsb.reserve_glyph(10, 1);
        w.set_pixel(0, 0, true);
        w.set_pixel(2, 0, true);

        // Merge LSB glyph data (only the blob/internal matters for storage)
        // Since the current builder (b) is MSB, we'll build a complete LSB storage
        // and test storage/view functionality separately.

        return std::move(b).build(); // Return the MSB storage
    }

    TEST_CASE("Default constructor and count") {
        bitmap_storage s;
        CHECK(s.glyph_count() == 0);
        CHECK(s.order() == bit_order::msb_first); // Default is MSB from builder default
    }

    TEST_CASE("dimensions access") {
        bitmap_storage s = create_test_storage();

        CHECK(s.glyph_count() == 1);

        auto dims = s.dimensions(0);
        CHECK(dims.width == 8);
        CHECK(dims.height == 8);
    }

    TEST_CASE("view access and data integrity") {
        bitmap_storage s = create_test_storage();

        auto view = s.view(0);

        CHECK(view.width() == 8);
        CHECK(view.height() == 8);
        CHECK(view.stride_bytes() == 1);
        CHECK(view.order() == bit_order::msb_first);

        // Check diagonal pixels (0,0) to (7,7)
        for (std::uint16_t i = 0; i < 8; ++i) {
            CHECK(view.pixel(i, i) == true);
        }

        // Check off-diagonal (0,1)
        CHECK(view.pixel(0, 1) == false);

        // Check row access
        std::span <const std::byte> row_1 = view.row(1);
        CHECK(row_1.size() == 1);
        // For (1,1) set (MSB), this should be 0b01000000
        CHECK(std::to_integer<std::uint8_t>(row_1[0]) == 0b01000000);
    }

    TEST_CASE("blob_bytes access") {
        bitmap_storage s = create_test_storage();

        std::span <const std::byte> blob = s.blob_bytes();
        // 8x8, stride 1 = 8 bytes total
        CHECK(blob.size() == 8);

        // Verify contents
        std::vector <std::byte> expected = create_test_bitmap_8x8_msb();
        REQUIRE(blob.size() == expected.size());
        for (size_t i = 0; i < blob.size(); ++i) {
            CHECK(blob[i] == expected[i]);
        }
    }
}

// --------------------------------------------------------------------------------
// Unit Tests for bitmap_view
// --------------------------------------------------------------------------------

TEST_SUITE("bitmap_view (standalone)") {
    using namespace onyx_font;

    TEST_CASE("MSB_FIRST pixel retrieval") {
        // Bitmap: 10x1, Stride 2. (0,0), (7,0), (8,0) are ON.
        // Byte 0: MSB -> 0b10000001 ((0,0) and (7,0) set)
        // Byte 1: MSB -> 0b01000000 ((8,0) set)
        std::vector <std::byte> data = {
            std::byte{0b10000001},
            std::byte{0b10000000}
        };

        // Note: bitmap_view constructor is private, so we rely on bitmap_storage::view
        // or a friend/helper to create it directly. For doctest, we'll rely on the
        // `bitmap_builder` -> `bitmap_storage` -> `bitmap_view` chain to ensure
        // proper initialization, which is the intended usage.

        bitmap_builder b; // MSB_FIRST
        (void)b.add_glyph_packed(10, 1, {data.data(), data.size()});
        bitmap_storage s = std::move(b).build();
        bitmap_view view = s.view(0);

        CHECK(view.width() == 10);
        CHECK(view.height() == 1);
        CHECK(view.stride_bytes() == 2);
        CHECK(view.order() == bit_order::msb_first);

        // Check ON pixels
        CHECK(view.pixel(0, 0) == true);
        CHECK(view.pixel(7, 0) == true);
        CHECK(view.pixel(8, 0) == true);

        // Check OFF pixels
        CHECK(view.pixel(1, 0) == false);
        CHECK(view.pixel(9, 0) == false);
    //    CHECK(view.pixel(10, 0) == false); // Should technically assert, but we check one beyond width.

        // Check row access
        std::span <const std::byte> row = view.row(0);
        REQUIRE(row.size() == 2);
        CHECK(row[0] == std::byte{0b10000001});
        // CHECK(row[1] == std::byte{0b01000000});
        CHECK(row[1] == std::byte{0b10000000});
    }

    TEST_CASE("LSB_FIRST pixel retrieval") {
        // Bitmap: 10x1, Stride 2. (0,0), (7,0), (8,0) are ON.
        // Byte 0: LSB -> 0b10000001 ((7,0) and (0,0) set)
        // Byte 1: LSB -> 0b00000001 ((8,0) set)
        std::vector <std::byte> data = {
            std::byte{0b10000001}, std::byte{0b00000001}
        };

        bitmap_builder b(bit_order::lsb_first);
        (void)b.add_glyph_packed(10, 1, {data.data(), data.size()});
        bitmap_storage s = std::move(b).build();
        bitmap_view view = s.view(0);

        CHECK(view.order() == bit_order::lsb_first);

        // Check ON pixels
        CHECK(view.pixel(0, 0) == true); // bit 0 of byte 0
        CHECK(view.pixel(7, 0) == true); // bit 7 of byte 0
        CHECK(view.pixel(8, 0) == true); // bit 0 of byte 1

        // Check OFF pixels
        CHECK(view.pixel(1, 0) == false);
        CHECK(view.pixel(6, 0) == false);
        CHECK(view.pixel(9, 0) == false);

        // Check row access
        std::span <const std::byte> row = view.row(0);
        REQUIRE(row.size() == 2);
        CHECK(row[0] == std::byte{0b10000001});
        CHECK(row[1] == std::byte{0b00000001});
    }

    TEST_CASE("MSB_FIRST pixel retrieval with Stride 3 (20x1)") {
        // Bitmap: 20x1, Stride 3. (20+7)/8 = 3.
        // We set pixels x=5, x=13, x=19. Order is MSB_FIRST.

        // x=5: Byte 0, Bit Index 7-5=2. Mask: 0b00000100
        // x=13: Byte 1, Bit Index 7-5=2. Mask: 0b00000100
        // x=19: Byte 2, Bit Index 7-3=4. Mask: 0b00010000
        std::vector<std::byte> data = {
            std::byte{0b00000100}, // x=5 (Bit 2)
            std::byte{0b00000100}, // x=13 (Bit 2)
            std::byte{0b00010000}  // x=19 (Bit 4)
        };

        bitmap_builder b; // MSB_FIRST
        (void)b.add_glyph_packed(20, 1, {data.data(), data.size()});
        bitmap_storage s = std::move(b).build();
        bitmap_view view = s.view(0);

        CHECK(view.width() == 20);
        CHECK(view.stride_bytes() == 3);

        // Check ON pixels (These should now pass)
        CHECK(view.pixel(5, 0) == true);
        CHECK(view.pixel(13, 0) == true);
        CHECK(view.pixel(19, 0) == true);

        // Check off
        CHECK(view.pixel(0, 0) == false);
        CHECK(view.pixel(6, 0) == false);
        CHECK(view.pixel(14, 0) == false);

        CHECK_THROWS_AS(view.pixel(20, 0), std::runtime_error); // Out of bounds
    }
}


TEST_SUITE("bitmap_builder non-trivial stride") {

    using namespace onyx_font;


    TEST_CASE("Stride 2 (10x1) - Boundary Test MSB_FIRST") {
        bitmap_builder b(bit_order::msb_first);

        auto w = b.reserve_glyph(10, 1); // Stride must be 2 bytes: (10 + 7) / 8 = 2

        CHECK(w.stride_bytes() == 2);

        // Byte 0 (8 bits): Indices 0 to 7
        w.set_pixel(0, 0, true); // Bit 7 of Byte 0
        w.set_pixel(7, 0, true); // Bit 0 of Byte 0

        // Byte 1 (2 bits used): Indices 8 to 9
        w.set_pixel(8, 0, true); // Bit 7 of Byte 1
        w.set_pixel(9, 0, true); // Bit 6 of Byte 1

        bitmap_storage storage = std::move(b).build();
        auto view = storage.view(0);

        // Verify dimensions
        CHECK(view.width() == 10);
        CHECK(view.stride_bytes() == 2);

        // Verify raw data via row()
        std::span<const std::byte> row0 = view.row(0);
        CHECK(row0.size() == 2);
        // Byte 0: 0b10000001
        CHECK(std::to_integer<std::uint8_t>(row0[0]) == 0b10000001);
        // Byte 1: 0b11000000
        CHECK(std::to_integer<std::uint8_t>(row0[1]) == 0b11000000);

        // Verify pixel access
        CHECK(view.pixel(0, 0) == true);
        CHECK(view.pixel(7, 0) == true);
        CHECK(view.pixel(8, 0) == true);
        CHECK(view.pixel(9, 0) == true);
        CHECK(view.pixel(1, 0) == false);
    }

    TEST_CASE("Stride 2 (10x1) - Boundary Test LSB_FIRST") {
        bitmap_builder b(bit_order::lsb_first);

        auto w = b.reserve_glyph(10, 1); // Stride must be 2 bytes

        // Byte 0 (8 bits): Indices 0 to 7
        w.set_pixel(0, 0, true); // Bit 0 of Byte 0
        w.set_pixel(7, 0, true); // Bit 7 of Byte 0

        // Byte 1 (2 bits used): Indices 8 to 9
        w.set_pixel(8, 0, true); // Bit 0 of Byte 1
        w.set_pixel(9, 0, true); // Bit 1 of Byte 1

        bitmap_storage storage = std::move(b).build();
        auto view = storage.view(0);

        // Verify raw data via row()
        std::span<const std::byte> row0 = view.row(0);
        CHECK(row0.size() == 2);
        // Byte 0: 0b10000001
        CHECK(std::to_integer<std::uint8_t>(row0[0]) == 0b10000001);
        // Byte 1: 0b00000011
        CHECK(std::to_integer<std::uint8_t>(row0[1]) == 0b00000011);

        // Verify pixel access
        CHECK(view.pixel(0, 0) == true);
        CHECK(view.pixel(7, 0) == true);
        CHECK(view.pixel(8, 0) == true);
        CHECK(view.pixel(9, 0) == true);
        CHECK(view.pixel(1, 0) == false);
    }

    TEST_CASE("Stride 3 (17x1) - Multi-byte Boundary Test") {
        bitmap_builder b; // MSB_FIRST

        auto w = b.reserve_glyph(17, 1); // Stride must be 3 bytes: (17 + 7) / 8 = 3

        CHECK(w.stride_bytes() == 3);

        // Set pixels that test byte start/end points
        w.set_pixel(0, 0, true);  // x=0, Byte 0, Bit 7
        w.set_pixel(7, 0, true);  // x=7, Byte 0, Bit 0
        w.set_pixel(8, 0, true);  // x=8, Byte 1, Bit 7
        w.set_pixel(15, 0, true); // x=15, Byte 1, Bit 0
        w.set_pixel(16, 0, true); // x=16, Byte 2, Bit 7 (last valid pixel)

        bitmap_storage storage = std::move(b).build();
        auto view = storage.view(0);

        // Verify raw data
        std::span<const std::byte> row0 = view.row(0);
        CHECK(row0.size() == 3);

        // Byte 0: 0b10000001 (x=0, x=7)
        CHECK(std::to_integer<std::uint8_t>(row0[0]) == 0b10000001);
        // Byte 1: 0b10000001 (x=8, x=15)
        CHECK(std::to_integer<std::uint8_t>(row0[1]) == 0b10000001);
        // Byte 2: 0b10000000 (x=16)
        CHECK(std::to_integer<std::uint8_t>(row0[2]) == 0b10000000);

        // Verify pixel access
        CHECK(view.pixel(0, 0) == true);
        CHECK(view.pixel(7, 0) == true);
        CHECK(view.pixel(8, 0) == true);
        CHECK(view.pixel(15, 0) == true);
        CHECK(view.pixel(16, 0) == true);
        CHECK(view.pixel(1, 0) == false);
        CHECK(view.pixel(9, 0) == false);
        CHECK_THROWS(view.pixel(17, 0)); // Out of bounds - should throw
    }
}

