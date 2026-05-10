/**
 * @file font_registry.hh
 * @brief Central registry for font format decoders.
 *
 * The font_registry is a singleton that manages all registered font decoders.
 * Built-in decoders for standard formats are registered automatically.
 * Users can register additional decoders for custom formats.
 *
 * @section registry_usage Usage
 *
 * @subsection registry_builtin Using Built-in Decoders
 *
 * @code{.cpp}
 * #include <onyx_font/font_registry.hh>
 *
 * // Find decoder by sniffing file data
 * auto& registry = onyx_font::font_registry::instance();
 *
 * if (auto* decoder = registry.find_bitmap_decoder(file_data)) {
 *     auto entries = decoder->enumerate(file_data);
 *     auto font = decoder->load(file_data, 0);
 * }
 *
 * // Find decoder by name
 * if (auto* decoder = registry.find_bitmap_decoder("gem")) {
 *     // Use GEM decoder directly
 * }
 * @endcode
 *
 * @subsection registry_custom Registering Custom Decoders
 *
 * @code{.cpp}
 * #include <onyx_font/font_registry.hh>
 * #include <onyx_font/decoder.hh>
 *
 * class my_decoder : public onyx_font::bitmap_font_decoder {
 *     // ... implementation
 * };
 *
 * // Register at program startup
 * void init() {
 *     onyx_font::font_registry::instance()
 *         .register_decoder(std::make_unique<my_decoder>());
 * }
 * @endcode
 *
 * @section registry_order Decoder Order
 *
 * When searching by sniff, decoders are tried in registration order.
 * Built-in decoders are registered first, so custom decoders act as
 * fallbacks. Use register_decoder_first() to insert at the front.
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string_view>
#include <vector>

#include <onyx_font/export.h>
#include <onyx_font/decoder.hh>

namespace onyx_font {

    /**
     * @brief Central registry for font format decoders.
     *
     * Singleton class managing bitmap, vector, and outline font decoders.
     * Built-in decoders are registered on first access to instance().
     */
    class ONYX_FONT_EXPORT font_registry {
    public:
        /**
         * @brief Access the global registry instance.
         *
         * On first call, creates the registry and registers all built-in
         * decoders for standard font formats.
         *
         * @return Reference to the singleton registry
         */
        [[nodiscard]] static font_registry& instance();

        // Non-copyable, non-movable
        font_registry(const font_registry&) = delete;
        font_registry& operator=(const font_registry&) = delete;
        font_registry(font_registry&&) = delete;
        font_registry& operator=(font_registry&&) = delete;

        /**
         * @name Decoder Registration
         * @{
         */

        /**
         * @brief Register a bitmap font decoder.
         *
         * Adds the decoder to the end of the bitmap decoder list.
         * The registry takes ownership of the decoder.
         *
         * @param decoder Decoder instance to register
         */
        void register_decoder(bitmap_font_decoder_ptr decoder);

        /**
         * @brief Register a vector font decoder.
         * @param decoder Decoder instance to register
         */
        void register_decoder(vector_font_decoder_ptr decoder);

        /**
         * @brief Register an outline font decoder.
         * @param decoder Decoder instance to register
         */
        void register_decoder(outline_font_decoder_ptr decoder);

        /**
         * @brief Register a bitmap decoder at the front of the list.
         *
         * Use this to override built-in decoders with custom implementations.
         *
         * @param decoder Decoder instance to register
         */
        void register_decoder_first(bitmap_font_decoder_ptr decoder);

        /**
         * @brief Register a vector decoder at the front of the list.
         * @param decoder Decoder instance to register
         */
        void register_decoder_first(vector_font_decoder_ptr decoder);

        /**
         * @brief Register an outline decoder at the front of the list.
         * @param decoder Decoder instance to register
         */
        void register_decoder_first(outline_font_decoder_ptr decoder);

        /** @} */

        /**
         * @name Decoder Lookup by Sniffing
         * @{
         */

        /**
         * @brief Find a bitmap decoder that can handle the data.
         *
         * Iterates through registered bitmap decoders and returns the
         * first one whose sniff() method returns true.
         *
         * @param data Raw file data to examine
         * @return Pointer to decoder, or nullptr if none found
         */
        [[nodiscard]] const bitmap_font_decoder* find_bitmap_decoder(
            std::span<const std::uint8_t> data) const noexcept;

        /**
         * @brief Find a vector decoder that can handle the data.
         * @param data Raw file data to examine
         * @return Pointer to decoder, or nullptr if none found
         */
        [[nodiscard]] const vector_font_decoder* find_vector_decoder(
            std::span<const std::uint8_t> data) const noexcept;

        /**
         * @brief Find an outline decoder that can handle the data.
         * @param data Raw file data to examine
         * @return Pointer to decoder, or nullptr if none found
         */
        [[nodiscard]] const outline_font_decoder* find_outline_decoder(
            std::span<const std::uint8_t> data) const noexcept;

        /** @} */

        /**
         * @name Decoder Lookup by Name
         * @{
         */

        /**
         * @brief Find a bitmap decoder by name.
         * @param name Decoder name (e.g., "gem", "win_fnt")
         * @return Pointer to decoder, or nullptr if not found
         */
        [[nodiscard]] const bitmap_font_decoder* find_bitmap_decoder(
            std::string_view name) const noexcept;

        /**
         * @brief Find a vector decoder by name.
         * @param name Decoder name (e.g., "bgi", "win_vector")
         * @return Pointer to decoder, or nullptr if not found
         */
        [[nodiscard]] const vector_font_decoder* find_vector_decoder(
            std::string_view name) const noexcept;

        /**
         * @brief Find an outline decoder by name.
         * @param name Decoder name (e.g., "ttf", "otf")
         * @return Pointer to decoder, or nullptr if not found
         */
        [[nodiscard]] const outline_font_decoder* find_outline_decoder(
            std::string_view name) const noexcept;

        /** @} */

        /**
         * @name Decoder Enumeration
         * @{
         */

        /**
         * @brief Get the number of registered bitmap decoders.
         * @return Decoder count
         */
        [[nodiscard]] std::size_t bitmap_decoder_count() const noexcept;

        /**
         * @brief Get a bitmap decoder by index.
         * @param index Decoder index (0 to bitmap_decoder_count()-1)
         * @return Pointer to decoder, or nullptr if index out of range
         */
        [[nodiscard]] const bitmap_font_decoder* bitmap_decoder_at(
            std::size_t index) const noexcept;

        /**
         * @brief Get the number of registered vector decoders.
         * @return Decoder count
         */
        [[nodiscard]] std::size_t vector_decoder_count() const noexcept;

        /**
         * @brief Get a vector decoder by index.
         * @param index Decoder index
         * @return Pointer to decoder, or nullptr if index out of range
         */
        [[nodiscard]] const vector_font_decoder* vector_decoder_at(
            std::size_t index) const noexcept;

        /**
         * @brief Get the number of registered outline decoders.
         * @return Decoder count
         */
        [[nodiscard]] std::size_t outline_decoder_count() const noexcept;

        /**
         * @brief Get an outline decoder by index.
         * @param index Decoder index
         * @return Pointer to decoder, or nullptr if index out of range
         */
        [[nodiscard]] const outline_font_decoder* outline_decoder_at(
            std::size_t index) const noexcept;

        /** @} */

    private:
        font_registry();
        ~font_registry();

        void register_builtin_decoders();

        struct impl;
        std::unique_ptr<impl> m_impl;
    };

} // namespace onyx_font
