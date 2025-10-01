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
     * @note This class should be initialized at the beginning of setup() (but after DebugLED and Communication classes). Serial.begin() have to be initialized separately before this class to see debug messages.
     * This class is a singleton.
     */
    class PairingButton {
    public:
        /**
         * @brief Method that initializes PairingButton and returns a pointer to the instance of PairingButton.
         * @param debugLED Pointer to DebugLED object.
         * @param communication Pointer to Communication object.
         * @param logger Shared pointer to the Logger instance.
         * @return PairingButton* pointer to the instance of PairingButton.
         */
        static PairingButton& getInstance(DebugLED *debugLED, Comms::Communication *communication, const std::shared_ptr<ul::Logger> &logger);

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
        PairingButton(DebugLED *debugLED, Comms::Communication *communication, const std::shared_ptr<ul::Logger> &logger);

        /**
         * @brief Destructor of PairingButton class. Detaches interrupt from BUTTON_PIN and deletes Button Press Timer if exists.
         * @warning Destructor of this class exists only for programming principles. This class should never be deleted.
         * @note Destructor of this class is private, because this class is a singleton.
         */
        ~PairingButton();

        /**
         * @brief Method that is called when the button is pressed (and interrupt is attached) and calls startButtonPressTimer().
         * @note This method detaches interrupt for debouncing reasons. Interrupt is reattached at the end of buttonPressTimerCallback().
         */
        static void IRAM_ATTR buttonISR();

        /**
         * @brief Method that is called when the Button Press Timer expires. It debounces a button and controls its logic.
         * @param xTimer FreeRTOS software timer (not used).
         */
        static void buttonPressTimerCallback(TimerHandle_t xTimer);

        /**
         * @brief Method that starts the Button Press Timer. If timer is already started, it will be restarted.
         */
        void startButtonPressTimer();

        /**
         * @brief Method that deletes the Button Press Timer and resets button's variables to default values.
         * If Button Press Timer doesn't exist, it only reset variables.
         * @note Button's variables are: msButtonPressCounter, msButtonNotPressedCounter.
         */
        void deleteButtonPressTimer();

        enum class ButtonModes : uint8_t {
            IDLE = 0,
            PAIR,
            RESET
        };

        static PairingButton* mspPairingButton;
        DebugLED *mpDebugLED;
        Comms::Communication *mpCommunication;
        std::shared_ptr<ul::Logger> mpLogger;

        std::atomic<ButtonModes> mButtonMode{ButtonModes::IDLE}; ///< State of button.
        std::atomic<uint8_t> mButtonPressCounter{0}; ///< Counter how long is button pressed in <code>DEBOUNCING_TIME</code> (0.1) s.
        std::atomic<int8_t> mButtonNotPressedCounter{3}; ///< Counter how long is not button pressed (for debouncing).

        TimerHandle_t mButtonPressTimer = nullptr; ///< FreeRTOS software timer for measure how long is button pressed.
    };
}