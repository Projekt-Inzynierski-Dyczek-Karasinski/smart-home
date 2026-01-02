#pragma once

#include "../api_types.h"

namespace Comms::API {

    /**
     * @brief Encodes/decodes the command "special byte".
     * @details The special byte is split into two 4-bit parts (nibbles):
     * - High nibble: command type.
     * - Low nibble: number of arguments.
     */
    class SpecialByteCommand {
    public:
        /**
         * @brief Construct from an already encoded special byte.
         *
         * @param specialByte Raw command special byte received in a message.
         */
        explicit SpecialByteCommand(uint8_t specialByte);

        /**
         * @brief Construct and encode a special byte from command type and argument count.
         *
         * @param commandType Command type to encode.
         * @param numOfArguments Number of arguments to encode.
         */
        SpecialByteCommand(commandTypes commandType, uint8_t numOfArguments);

        /**
         * @brief Decode and return the command type from the special byte.
         *
         * @return Decoded command type.
         */
        [[nodiscard]] commandTypes getCommandType() const;

        /**
         * @brief Decode and return number of arguments from the special byte.
         *
         * @return Decoded number of arguments.
         */
        [[nodiscard]] uint8_t getNumOfArguments() const;

        /**
         * @brief Return the raw encoded special byte value.
         *
         * @return Encoded special byte.
         */
        [[nodiscard]] uint8_t getSpecialByte() const;

    private:
        uint8_t mSpecialByte;
        static constexpr uint8_t ms_TYPE_PART =   0b11110000;
        static constexpr uint8_t ms_ARGS_PART = 0b00001111;
        static constexpr uint8_t ms_BITS_PER_PART = 4;
        static constexpr uint8_t ms_MAX_LENGTH = 15;
    };
}
