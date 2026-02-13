#pragma once

#include <Arduino.h>
#include <array>

#include "utils/logger.h"

namespace ul = Utils::Logging;

namespace UniversalModuleSystem {
    /**
     * @brief OTA state change operations.
     */
    enum class otaOperations : uint8_t {
        END = 0,  ///< Stop OTA mode.
        BEGIN = 1 ///< Start OTA mode.
    };

    /**
     * @brief Generic OTA interface for the module.
     */
    class IOta {
    public:
        IOta() = default;
        virtual ~IOta() = default;

        // Delete copy constructor and assignment operator
        IOta(const IOta&) = delete;
        IOta& operator = (const IOta&) = delete;

        /**
         * @brief Start OTA mode.
         * @details Connects to Wi-Fi and starts OTA handling.
         *
         * @return Array containing the device IPv4 address.
         *
         * @note If OTA is already started, returns the already assigned IP address.
         */
        virtual std::array<uint8_t, 4> beginOta() = 0;

        /**
         * @brief Stop OTA mode.
         * @details Stops OTA handling and disconnects from Wi-Fi.
         */
        virtual void endOta() = 0;

        /**
         * @brief Toggle OTA mode.
         */
        virtual void toggleOta() = 0;

        /**
         * @brief Checks if the module is connected to Wi-Fi.
         *
         * @return True if the module is connected to Wi-Fi, false otherwise.
         */
        virtual bool isConnectedToWifi() const = 0;

        /**
         * @brief Auto-start OTA under special conditions and halt the rest of the program.
         */
        virtual void autoBeginOta() = 0;

        static constexpr uint8_t s_IP_ADDRESS_LENGTH = 4;
    };
}