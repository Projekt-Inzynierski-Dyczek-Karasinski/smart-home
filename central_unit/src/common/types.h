#pragma once

#include <cstdint>

namespace SmartHome {
    // TODO Currently connectionId_t must be of the same type as apiId_t consider either:
    //       - merging them into one type
    //       - modifying code and changing variables of these types to make those types fully separate from each other
    using apiId_t = uint64_t;
    using connectionId_t = apiId_t;
}
