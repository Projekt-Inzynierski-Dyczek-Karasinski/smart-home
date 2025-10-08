#pragma once

// functions in PowerManager are exclusively for ESP32
#ifndef ESP32_BOARD
#error "Power management not implemented"
#endif

#include <Arduino.h>
#include <memory>

#include "utils/logger.h"
namespace ul = Utils::Logging;

namespace UniversalModuleSystem {
    class PowerManager {
    public:
        /**
         * @brief Gets the singleton instance of PowerManager.
         *
         * @return Reference to the PowerManager instance.
         */
        static PowerManager& getInstance(const std::shared_ptr<ul::Logger> &logger);

        // Delete copy constructor and assignment operator
        PowerManager(const PowerManager&) = delete;
        PowerManager& operator = (const PowerManager&) = delete;

        void enterSleep(uint32_t seconds, bool enableWakeUpWithRfModule = false);

        void safeRestart(const char *source) const;

        uint16_t getBatteryRead() const;

    private:
        explicit PowerManager(const std::shared_ptr<ul::Logger> &logger);
        ~PowerManager();

        void waitAndDisableCriticalFeatures() const;

        void printWakeUpReason() const;

        void readBattery();

        static void batteryReadTask(void* parameters);

        std::shared_ptr<ul::Logger> mpLogger;
        std::atomic<uint16_t> mBatteryRead{0};

        SemaphoreHandle_t mReadCompleteSemaphore = nullptr;
    };
}
