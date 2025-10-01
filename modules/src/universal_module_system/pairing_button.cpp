#include "pairing_button.h"

#include "utils/logger.h"
#include "config/univarsal_module_system_config.h"

namespace UniversalModuleSystem {
    PairingButton* PairingButton::mspPairingButton = nullptr;

    PairingButton& PairingButton::getInstance(DebugLED *debugLED, Comms::Communication *communication, const std::shared_ptr<ul::Logger> &logger) {
        static PairingButton instance(debugLED, communication, logger);
        return instance;
    }

    PairingButton::PairingButton(DebugLED *debugLED, Comms::Communication *communication, const std::shared_ptr<ul::Logger> &logger)
        : mpDebugLED(debugLED), mpCommunication(communication), mpLogger(logger) {
        mspPairingButton = this;
        pinMode(BUTTON_PIN, INPUT_PULLUP);
        attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), buttonISR, FALLING);

        mpLogger->info("PairingButton Class", "PairingButton initialized.");
    }

    PairingButton::~PairingButton() {
        detachInterrupt(digitalPinToInterrupt(BUTTON_PIN));
        deleteButtonPressTimer();
    }

    void IRAM_ATTR PairingButton::buttonISR() {
        auto &pb = *mspPairingButton;
        detachInterrupt(digitalPinToInterrupt(BUTTON_PIN));
        pb.mpLogger->debug("PairingButton ISR", "Button pressed.");
        pb.startButtonPressTimer();

        // Force context switch if higher priority task was woken
        portYIELD_FROM_ISR(pdFALSE);
    }

    // ====================== Button Press Timer ======================
    void PairingButton::buttonPressTimerCallback(TimerHandle_t xTimer) {
        auto &pb = *mspPairingButton;
        if (digitalRead(BUTTON_PIN) == LOW) {
            pb.mButtonPressCounter.fetch_add(1);
            pb.mButtonNotPressedCounter.store(3);

            // after pressing button for 10 seconds call createResetBlinkTask()
            if (DEBOUNCING_COUNTER_TO_SECONDS(pb.mButtonPressCounter.load()) >= 10 && pb.mButtonMode.load() != ButtonModes::RESET) {
                pb.mButtonMode.store(ButtonModes::RESET);
                pb.mpDebugLED->createResetBlinkTask();
                // give time for blink DebugLED, clear data and reboot
                pb.mpLogger->warning("PairingButton Timer", "Clearing data...");
                // TODO add handler for clear data in flash memory
                pb.mpLogger->error("PairingButton Timer", "Clearing data not implemented!");
                vTaskDelay(pdMS_TO_TICKS(BUTTON_REBOOT_DELAY));
                pb.mpLogger->warning("PairingButton Timer", "Rebooting...");
                ESP.restart();
            }
            // after pressing button for 3 seconds call createPairingBlinkTask()
            else if (DEBOUNCING_COUNTER_TO_SECONDS(pb.mButtonPressCounter.load()) >= 3 && pb.mButtonMode.load() == ButtonModes::IDLE) {
                if (pb.mButtonMode.load() != ButtonModes::PAIR) {
                    pb.mpDebugLED->createPairingBlinkTask();
                    pb.mpLogger->debug("PairingButton Timer", "ButtonModes::PAIR");
                }
                pb.mButtonMode.store(ButtonModes::PAIR);
            }
        } else {
            pb.mButtonNotPressedCounter.fetch_sub(1);

            // if button will not be press for 3*DEBOUNCING_TIME (0.3 seconds) timer will stop
            if (pb.mButtonNotPressedCounter.load() <= 0) {
                if (pb.mButtonMode.load() == ButtonModes::PAIR) {
                    pb.mpCommunication->startAddressingAlgorithm();
                    pb.mpLogger->debug("PairingButton Timer", "startAddressingAlgorithm()");
                }
                pb.deleteButtonPressTimer();
            }
        }
    }

    void PairingButton::startButtonPressTimer() {
        if (mButtonPressTimer == nullptr) {
            mButtonPressTimer = xTimerCreate(
                "Button Press Timer",
                pdMS_TO_TICKS(DEBOUNCING_TIME),
                pdTRUE,
                nullptr,
                buttonPressTimerCallback
            );
        }
        if (mButtonPressTimer != nullptr) {
            xTimerStart(mButtonPressTimer, portMAX_DELAY);
        }
    }
    void PairingButton::deleteButtonPressTimer() {
        if (mButtonPressTimer != nullptr) {
            xTimerDelete(mButtonPressTimer, portMAX_DELAY);
            mButtonPressTimer = nullptr;
        }
        mButtonMode.store(ButtonModes::IDLE);
        mButtonPressCounter.store(0);
        mButtonNotPressedCounter.store(3);
        attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), buttonISR, FALLING);
    }
}