#pragma once

#include <stdexcept>

#include "../api_types.h"

namespace Comms::API {
    /**
     * @brief Encodes/decodes a single “special byte” that packs parameter metadata.
     * @details The byte is split into two nibbles:
     * - Upper 4 bits: parameter type.
     * - Lower 4 bits: length field stored as (length - 1), so values 0..15 represent lengths 1..16.
     */
    class SpecialByteParameter {
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
        SpecialByteParameter(T parameter, const uint8_t arrayLength, const bool isError = false) {
            mSpecialByte = 0;

            if (isError) {
                mSpecialByte = (uint8_t)parametersTypes::ERROR << ms_BITS_PER_PART;
                mSpecialByte = mSpecialByte | (sizeof(uint8_t) - 1);
                return;
            }

            const parameterVariant pv = parameter;

            using PVE = ParameterVariantEnum;
            using PT = parametersTypes;

            uint8_t type;

            // scalars
            if (arrayLength == 0) {
                switch (pv.index()) {
                    case (uint8_t)PVE::UINT8:
                    case (uint8_t)PVE::UINT16:
                    case (uint8_t)PVE::UINT32:
                    case (uint8_t)PVE::UINT64:
                        type = (uint8_t)PT::UINT;
                        break;
                    case (uint8_t)PVE::INT8:
                    case (uint8_t)PVE::INT16:
                    case (uint8_t)PVE::INT32:
                    case (uint8_t)PVE::INT64:
                        type = (uint8_t)PT::INT;
                        break;
                    case (uint8_t)PVE::FLOAT:
                    case (uint8_t)PVE::DOUBLE:
                        type = (uint8_t)PT::FLOAT;
                        break;
                    case (uint8_t)PVE::MONOSTATE:
                    case (uint8_t)PVE::ASCII:
                    case (uint8_t)PVE::RAW:
                    default:
                        throw std::invalid_argument("SpecialByteParameter() invalid type");
                }
            }
            // arrays
            else {
                switch (pv.index()) {
                    case (uint8_t)PVE::UINT8:
                    case (uint8_t)PVE::RAW:
                        type = (uint8_t)PT::RAW;
                        break;
                    case (uint8_t)PVE::ASCII:
                        type = (uint8_t)PT::ASCII;
                        break;
                    case (uint8_t)PVE::MONOSTATE:
                    case (uint8_t)PVE::UINT16:
                    case (uint8_t)PVE::UINT32:
                    case (uint8_t)PVE::UINT64:
                    case (uint8_t)PVE::INT8:
                    case (uint8_t)PVE::INT16:
                    case (uint8_t)PVE::INT32:
                    case (uint8_t)PVE::INT64:
                    case (uint8_t)PVE::FLOAT:
                    case (uint8_t)PVE::DOUBLE:
                    default:
                        throw std::invalid_argument("SpecialByteParameter(*) invalid type");
                }
            }
            if (arrayLength > 16) throw std::length_error("arrayLength too big");
            const uint8_t length = arrayLength > 0 ? arrayLength - 1 : sizeof(T) - 1;
            mSpecialByte = type << ms_BITS_PER_PART;
            mSpecialByte = mSpecialByte | length;
        }


        /**
         * @brief Get the raw special byte.
         * @return Encoded byte.
         */
        [[nodiscard]] uint8_t getSpecialByte() const;

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

    private:
        uint8_t mSpecialByte;

        static constexpr uint8_t ms_TYPE_PART = 0b11110000;
        static constexpr uint8_t ms_ARGS_PART = 0b00001111;
        static constexpr uint8_t ms_BITS_PER_PART = 4;
    };
}
