#pragma once
#include "special_byte.h"

#include "../api_types.h"

namespace Comms::API {

    /**
     * @brief Encodes/decodes the command "special byte".
     * @details The special byte is split into two 4-bit parts (nibbles):
     * - High nibble: command type.
     * - Low nibble: number of arguments.
     */
    class SpecialByteCommand final : public SpecialByte {
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

    };
}
