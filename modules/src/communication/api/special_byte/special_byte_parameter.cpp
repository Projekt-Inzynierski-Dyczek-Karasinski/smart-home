#include "special_byte_parameter.h"

namespace Comms::API {
    SpecialByteParameter::SpecialByteParameter(const uint8_t specialByte)
    : mSpecialByte(specialByte) {}

    uint8_t SpecialByteParameter::getSpecialByte() const {
        return mSpecialByte;
    }

    uint8_t SpecialByteParameter::getType() const {
        return (mSpecialByte & ms_TYPE_PART) >> ms_BYTES_FOR_PART;
    }

    uint8_t SpecialByteParameter::getLength() const {
        return (mSpecialByte & ms_LENGTH_PART) + 1;
    }
}
