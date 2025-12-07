#include "special_byte_command.h"

namespace Comms::API {
    SpecialByteCommand::SpecialByteCommand(const uint8_t specialByte) : mSpecialByte(specialByte) {}

    SpecialByteCommand::SpecialByteCommand(const commandTypes commandType, const uint8_t numOfArguments) {
        mSpecialByte = 0;
        mSpecialByte = (uint8_t)commandType << ms_BYTES_FOR_PART;
        mSpecialByte = mSpecialByte | numOfArguments;
    }

    commandTypes SpecialByteCommand::getCommandType() const {
        return (commandTypes)((mSpecialByte & ms_TYPE_PART) >> ms_BYTES_FOR_PART);
    }

    uint8_t SpecialByteCommand::getNumOfArguments() const {
        return mSpecialByte & ms_LENGTH_PART;
    }

    uint8_t SpecialByteCommand::getSpecialByte() const {
        return mSpecialByte;
    }
}