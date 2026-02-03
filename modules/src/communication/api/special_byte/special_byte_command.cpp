#include "special_byte_command.h"

namespace Comms::API {
    SpecialByteCommand::SpecialByteCommand(const uint8_t specialByte) : SpecialByte(specialByte) {}

    SpecialByteCommand::SpecialByteCommand(const commandTypes commandType, uint8_t numOfArguments) : SpecialByte(0) {
        if (numOfArguments > ms_MAX_LENGTH) numOfArguments = ms_MAX_LENGTH;

        mSpecialByte = static_cast<uint8_t>(commandType) << ms_BITS_PER_PART;
        mSpecialByte = mSpecialByte | numOfArguments;
    }

    commandTypes SpecialByteCommand::getCommandType() const {
        return static_cast<commandTypes>((mSpecialByte & ms_TYPE_PART) >> ms_BITS_PER_PART);
    }

    uint8_t SpecialByteCommand::getNumOfArguments() const {
        return mSpecialByte & ms_ARGS_PART;
    }
}