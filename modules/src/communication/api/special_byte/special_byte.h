#pragma once

#include "Arduino.h"

namespace Comms::API {
    class SpecialByte {
    public:
        /**
         * @brief Construct directly from an encoded special byte.
         *
         * @param specialByte Raw packed byte.
         */
        explicit SpecialByte(uint8_t specialByte);

        virtual ~SpecialByte() = default;

        /**
         * @brief Return the raw encoded special byte value.
         *
         * @return Encoded special byte.
         */
        [[nodiscard]] uint8_t getSpecialByte() const;

    protected:
        uint8_t mSpecialByte;

        static constexpr uint8_t ms_TYPE_PART =   0b11110000;
        static constexpr uint8_t ms_ARGS_PART = 0b00001111;
        static constexpr uint8_t ms_BITS_PER_PART = 4;
        static constexpr uint8_t ms_MAX_LENGTH = 15;
    };
}