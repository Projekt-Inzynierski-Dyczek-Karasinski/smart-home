#pragma once

#include "special_byte.h"

#include <stdexcept>

namespace Comms::API {
    /**
     * @brief Encodes/decodes a single “special byte” that packs parameter metadata.
     * @details The byte is split into two nibbles:
     * - Upper 4 bits: parameter type.
     * - Lower 4 bits: length field stored as (length - 1), so values 0..15 represent lengths 1..16.
     */
    class SpecialByteParameter final : public SpecialByte {
    public:
        /**
         * @brief Construct directly from an encoded special byte.
         * @param specialByte Raw packed byte.
         */
        explicit SpecialByteParameter(uint8_t specialByte);

        /**
         * @brief Build the packed special byte based on a parameter value and array length.
         *
         * @tparam T Type of the provided parameter.
         * @param parameter Value used to infer the parameter type.
         * @param arrayLength Array element count:
         * - 0 means the parameter is treated as a scalar (length is inferred from sizeof(T)).
         * - >0 means the parameter is treated as an array (length is arrayLength).
         * @param isError When true, forces the type to ERROR and length to 1 byte.
         *
         * @throws std::invalid_argument If the parameter variant type is not supported.
         * @throws std::length_error If given arrayLength is too big.
         */
        template<typename T>
        SpecialByteParameter(T parameter, uint8_t arrayLength, bool isError = false);

        /**
         * @brief Decode the parameter type from the high nibble.
         * @return Type value.
         */
        [[nodiscard]] uint8_t getType() const;

        /**
         * @brief Decode the parameter length.
         * @return Decoded length in bytes.
         */
        [[nodiscard]] uint8_t getLength() const;
    };
}

#include "special_byte_parameter.tpp"