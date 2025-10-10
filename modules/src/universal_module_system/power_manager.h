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

        void enterSleep(uint32_t milliSeconds, bool enableWakeUpWithRfModule = false);

        void safeRestart(const char *source) const;

        uint16_t getBatteryRead() const;

        void disableAutoSleep();

    private:
        explicit PowerManager(const std::shared_ptr<ul::Logger> &logger);
        ~PowerManager();

        void waitAndDisableCriticalFeatures() const;

        void handleDefaultWakeUpAction();

        void handleWakeUpReason();

        void readBattery();

        static void batteryReadTask(void* parameters);

        void enableAutoSleep();
        static void goToAutoSleep(TimerHandle_t xTimer);

        uint32_t getCurrentTime() const;

        std::shared_ptr<ul::Logger> mpLogger;
        std::atomic<uint16_t> mBatteryRead{0};

        SemaphoreHandle_t mReadCompleteSemaphore = nullptr;

        TimerHandle_t mAutoSleepTimer = nullptr;

        // auto sleep
        static uint32_t msSleepStart;
        static int64_t msIntendedSleepTime;
    };
}
