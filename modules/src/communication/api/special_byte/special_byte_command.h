#pragma once

#include "../api_types.h"

namespace Comms::API {
    class SpecialByteCommand {
    public:
        explicit SpecialByteCommand(uint8_t specialByte);
        SpecialByteCommand(commandTypes commandType, uint8_t numOfArguments);

        commandTypes getCommandType() const;

        uint8_t getNumOfArguments() const;

        uint8_t getSpecialByte() const;

    private:
        uint8_t mSpecialByte;
        static constexpr uint8_t ms_TYPE_PART =   0b11110000;
        static constexpr uint8_t ms_LENGTH_PART = 0b00001111;
        static constexpr uint8_t ms_BYTES_FOR_PART = 4;
    };
}
