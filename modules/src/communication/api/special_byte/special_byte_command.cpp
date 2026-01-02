#include "special_byte_command.h"

namespace Comms::API {
    SpecialByteCommand::SpecialByteCommand(const uint8_t specialByte) : mSpecialByte(specialByte) {}

    SpecialByteCommand::SpecialByteCommand(const commandTypes commandType, uint8_t numOfArguments) {
        if (numOfArguments > ms_MAX_LENGTH) numOfArguments = ms_MAX_LENGTH;

        mSpecialByte = 0;
        mSpecialByte = (uint8_t)commandType << ms_BITS_PER_PART;
        mSpecialByte = mSpecialByte | numOfArguments;
    }

    commandTypes SpecialByteCommand::getCommandType() const {
        return (commandTypes)((mSpecialByte & ms_TYPE_PART) >> ms_BITS_PER_PART);
    }

    uint8_t SpecialByteCommand::getNumOfArguments() const {
        return mSpecialByte & ms_ARGS_PART;
    }

    uint8_t SpecialByteCommand::getSpecialByte() const {
        return mSpecialByte;
    }
}