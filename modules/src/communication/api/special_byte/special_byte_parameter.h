#pragma once

#include <stdexcept>

#include "../api_types.h"

namespace Comms::API {
    class SpecialByteParameter {
    public:
        explicit SpecialByteParameter(uint8_t specialByte);

        template<typename T>
        SpecialByteParameter(T parameter, const uint8_t arrayLength, const bool isError = false) {
            mSpecialByte = 0;

            if (isError) {
                mSpecialByte = (uint8_t)parametersTypes::ERROR << ms_BYTES_FOR_PART;
                mSpecialByte = mSpecialByte | (sizeof(uint8_t) - 1);
                return;
            }

            const parameterVariant pv = parameter;

            using PVE = ParameterVariantEnum;
            using PT = parametersTypes;

            uint8_t type;

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
            } else {
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

            const uint8_t length = arrayLength > 0 ? arrayLength - 1 : sizeof(T) - 1;
            mSpecialByte = type << ms_BYTES_FOR_PART;
            mSpecialByte = mSpecialByte | length;
        }


        uint8_t getSpecialByte() const;
        uint8_t getType() const;
        uint8_t getLength() const;

    private:
        uint8_t mSpecialByte;

        static constexpr uint8_t ms_TYPE_PART = 0b11110000;
        static constexpr uint8_t ms_LENGTH_PART = 0b00001111;
        static constexpr uint8_t ms_BYTES_FOR_PART = 4;
    };
}
