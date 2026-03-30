#pragma once

#include <memory>

#include "utils/logger.h"

namespace ul = Utils::Logging;

namespace UniversalModuleSystem {
    /**
     * @brief Class that controls the LED.
     * @details Blinking patterns:
     * - slowly (0.5s ON, 0.5s OFF) indicates pairing process
     * - quickly (0.1s ON, 0.1s OFF) indicates reset process
     */
    class DebugLED {
    public:
        /**
         * @brief Constructor of DebugLED class. Sets LED_PIN to OUTPUT and its state to LOW.
         * @param logger Shared pointer to the Logger instance.
         */
        explicit DebugLED(const std::shared_ptr<ul::Logger> &logger);

        /**
         * @brief Destructor of DebugLED class. Deletes all class's tasks and timers.
         * @warning Destructor of this class exists only for programming principles. This class should never be deleted.
         */
        ~DebugLED();

        /**
         * @brief Creates Pairing Blink Task and starts Blink Timeout Timer. If the task is already exist, only starts timer.
         * @warning If Reset Blink Task exist, it will be deleted before creating Pairing, only one of them is allowed at the same time.
         */
        void createPairingBlinkTask();

        /**
         * @brief Deletes Pairing Blink Task and deletes Blink Timeout Timer if exists.
         */
        void deleteBlinkTask();

        /**
         * @brief Creates Reset Blink Task and starts Blink Timeout Timer. If the task is already exist, only starts timer.
         * @warning If Pairing Blink Task exist, it will be deleted before creating Reset, only one of them is allowed at the same time.
         */
        void createResetBlinkTask();

        /**
         * @brief Deletes Pairing Blink Task and deletes Blink Timeout Timer if exists.
         */
        void deleteResetBlinkTask();

        /**
         * @brief Indicate that the module is connected to Wi-Fi.
         * @details Stops blinking and turns the LED on.
         */
        void wifiConnected();

        /**
         * @brief Indicate that the module is disconnected from Wi-Fi.
         * @details Stops blinking and turns the LED off.
         */
        void wifiDisconnected();

        // TODO !pr add comment
        void powerOnBlink() const;

    private:
        /**
         * @brief Make LED blink for a given times.
         * @param ledOnDuration Time in milliseconds for which the LED will be on.
         * @param ledOffDuration Time in milliseconds for which the LED will be off.
         */
        void blink(uint32_t ledOnDuration, uint32_t ledOffDuration) const;

        /**
         * @brief FreeRTOS task that blinks LED signalizing pairing process.
         * @param parameters FreeRTOS task parameters.
         */
        static void pairingBlink(void *parameters);

        /**
         * @brief FreeRTOS task that blinks LED signalizing reset process.
         * @param parameters FreeRTOS task parameters.
         */
        static void resetBlink(void *parameters);

        /**
         * @brief Method that is called when the Blink Timeout Timer expires. Deletes Timer and Pairing Blink and Reset Blink tasks if exists.
         */
        static void blinkTimeoutCallback(TimerHandle_t xTimer);

        /**
         * @brief Creates and starts Blink Timeout Timer. If timer is already started, it will be restarted.
         * @param maxBlinkTime Time in milliseconds after which the timer will expire.
         */
        void startBlinkTimeout(uint32_t maxBlinkTime);

        /**
         * @brief Deletes Blink Timeout Timer if exists.
         */
        void deleteBlinkTimeout();

        TaskHandle_t mBlinkHandle = nullptr; ///< FreeRTOS task handle to responsible for blinking LED.
        TimerHandle_t mBlinkTimeout = nullptr; ///< FreeRTOS software timer for automatic deleting <code>mBlinkHandle</code> task when timeout occurs.

        uint8_t mLedPin;
        std::shared_ptr<ul::Logger> mpLogger;

        // JSON key
        static constexpr char ms_LED_PIN[] = "ledPin";
    };
}