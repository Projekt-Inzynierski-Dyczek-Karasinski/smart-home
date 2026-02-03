#include "special_byte.h"

namespace Comms::API {
    SpecialByte::SpecialByte(const uint8_t specialByte) : mSpecialByte(specialByte) {
    }

    uint8_t SpecialByte::getSpecialByte() const {
        return mSpecialByte;
    }
}
