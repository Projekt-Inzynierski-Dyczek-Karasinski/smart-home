#include "special_byte_parameter.h"

namespace Comms::API {
    SpecialByteParameter::SpecialByteParameter(const uint8_t specialByte)
    : SpecialByte(specialByte) {}

    uint8_t SpecialByteParameter::getType() const {
        return (mSpecialByte & ms_TYPE_PART) >> ms_BITS_PER_PART;
    }

    uint8_t SpecialByteParameter::getLength() const {
        return (mSpecialByte & ms_ARGS_PART) + 1;
    }
}
