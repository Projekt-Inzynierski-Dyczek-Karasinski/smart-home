#pragma once

#include <Arduino.h>
#include <memory>
#include <array>

#include "utils/logger.h"
#include "../config/ota_config.h"

namespace ul = Utils::Logging;

// TODO !pr add comments for OTA
namespace UniversalModuleSystem {
    class IOta {
    public:
        IOta() = default;

        virtual ~IOta() = default;

        // Delete copy constructor and assignment operator
        IOta(const IOta&) = delete;
        IOta& operator = (const IOta&) = delete;

        virtual std::array<uint8_t, 4> beginOta() = 0;

        virtual void endOta() = 0;

        virtual void autoBeginOta() = 0;
    };
}