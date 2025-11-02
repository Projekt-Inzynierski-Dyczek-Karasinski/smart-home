#pragma once

#include <memory>
#include <atomic>

#include "debug_led.h"
#include "communication/communication.h"
#include "utils/logger.h"

namespace ul = Utils::Logging;

namespace UniversalModuleSystem {
    /**
     * @brief Class that controls the Pairing Button by attaching interrupt to it.
     * @details Pressing button for:
     * - 3 seconds initializes a pairing process.
     * - 10 seconds initializes a reset process.
     */
    class PairingButton {
    public:
        /**
         * @brief Method that gets the singleton instance of PairingButton and returns a reference to it.
         * @param debugLED Shared pointer to DebugLED object.
         * @param communication Pointer to Communication object.
         * @param logger Shared pointer to the Logger instance.
         * @return PairingButton& reference to the singleton instance of PairingButton.
         */
        static PairingButton& getInstance(const std::shared_ptr<DebugLED> &debugLED, Comms::Communication *communication, const std::shared_ptr<ul::Logger> &logger);

        // Delete copy constructor and assignment operator
        PairingButton(const PairingButton&) = delete;
        PairingButton& operator=(const PairingButton&) = delete;

    private:
        /**
         * @brief Constructor of PairingButton class. Sets BUTTON_PIN to INPUT_PULLUP and attaches interrupt to it.
         * @param debugLED Pointer to DebugLED object.
         * @param communication Pointer to Communication object.
         * @param logger Shared pointer to the Logger instance.
         * @note Constructor of this class is private, because this class is a singleton.
         */
        PairingButton(const std::shared_ptr<DebugLED> &debugLED, Comms::Communication *communication, const std::shared_ptr<ul::Logger> &logger);

        /**
         * @brief Destructor of PairingButton class. Detaches interrupt from BUTTON_PIN and deletes Button Press Timer if exists.
         * @warning Destructor of this class exists only for programming principles. This class should never be deleted.
         * @note Destructor of this class is private, because this class is a singleton.
         */
        ~PairingButton();

        /**
         * @brief ISR method that is called when the button is pressed (and interrupt is attached) and starts the button press timer.
         * @details This method detaches interrupt for debouncing reasons. Interrupt is reattached when the timer is deleted.
         */
        static void IRAM_ATTR buttonISR();

        /**
         * @brief FreeRTOS Task handling resting module to factory settings.
         * @details This task exists only because features needed to perform a factory reset
         * cause a "stack canary watchpoint trigger" in <code>buttonPressTimerCallback</code>.
         * @param parameters FreeRTOS task parameters.
         */
        static void factoryResetTask(void *parameters);

        /**
         * @brief Callback method that is called periodically by the Button Press Timer to handle button debouncing and logic.
         * @details This method:
         * - Increments press counter when button is held down.
         * - Triggers pairing mode after 3 seconds of continuous press.
         * - Triggers reset mode after 10 seconds of continuous press (clears data and reboots).
         * @param xTimer FreeRTOS software timer handle.
         * @note Pairing process starts only after releasing button, but LED starts blinking immediately after 3 seconds
         * to indicate user that can release button.
         */
        static void buttonPressTimerCallback(TimerHandle_t xTimer);

        /**
         * @brief Method that starts the Button Press Timer. If timer is already started, it will be restarted.
         */
        void startButtonPressTimer();

        /**
         * @brief Method that deletes the Button Press Timer, resets button's variables to default values and reattaches the button interrupt.
         */
        void deleteButtonPressTimer();

        enum class ButtonModes : uint8_t {
            IDLE = 0,
            PAIR,
            RESET
        };

        std::shared_ptr<DebugLED> mpDebugLED;
        std::shared_ptr<ul::Logger> mpLogger;
        Comms::Communication *mpCommunication;

        std::atomic<ButtonModes> mButtonMode{ButtonModes::IDLE}; ///< State of button.
        std::atomic<uint8_t> mButtonPressCounter{0}; ///< Counter for how long is button pressed in <code>DEBOUNCING_TIME</code> (0.1) s.
        std::atomic<int8_t> mButtonNotPressedCounter{3}; ///< Counter for how long is not button pressed (for debouncing).

        TimerHandle_t mButtonPressTimer = nullptr; ///< FreeRTOS software timer to measure how long is button pressed.
    };
}