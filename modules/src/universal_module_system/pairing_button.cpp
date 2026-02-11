#include "pairing_button.h"

#include "../../config/system_config/universal_module_system_config.h"
#include "../config/user_config/critical_config.h"

#include "data_manager.h"
#include "power_manager/power_manager.h"
#include "universal_module_system/ota/ota.h"

namespace UniversalModuleSystem {
    PairingButton &PairingButton::getInstance(const std::shared_ptr<DebugLED> &debugLED,
                                              Comms::Communication *communication,
                                              const std::shared_ptr<ul::Logger> &logger) {
        static PairingButton instance(debugLED, communication, logger);
        return instance;
    }

    PairingButton::PairingButton(const std::shared_ptr<DebugLED> &debugLED, Comms::Communication *communication,
                                 const std::shared_ptr<ul::Logger> &logger)
        : mpDebugLED(debugLED), mpLogger(logger), mpCommunication(communication) {
        mButtonPin = getButtonPin(mpLogger);
        pinMode(BUTTON_PIN, INPUT_PULLUP);

        attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), buttonISR, FALLING);
        mpLogger->verbose("PairingButton Class", "PairingButton initialized.");
    }

    PairingButton::~PairingButton() {
        detachInterrupt(digitalPinToInterrupt(BUTTON_PIN));
        deleteButtonPressTimer();
    }

    bool PairingButton::isButtonPressed() {
        const uint8_t buttonPin = getButtonPin();
        pinMode(buttonPin, INPUT_PULLUP);
        return !digitalRead(buttonPin);
    }

    uint8_t PairingButton::getButtonPin(std::shared_ptr<ul::Logger> logger) {
        if (logger == nullptr) {
            logger = std::make_shared<ul::Logger>();
        }

        uint8_t buttonPin;
        try {
            const auto &dataManager = DataManager::getInstance();
            nl::json jsonData = dataManager.loadJson(dataManager.s_BASE_CONFIG_PATH);
            buttonPin = jsonData[ms_BUTTON_PIN].get<uint8_t>();
        } catch (...) {
            logger->error("PairingButton", "Can not load button pin from base_config.json, loading from critical_config.h");
            buttonPin = BUTTON_PIN;
        }
        if (buttonPin != BUTTON_PIN) {
            logger->warning(
                "PairingButton",
                "Button pin defined in critical_config.h and base_config.json do not match, loading pin defined in base_config.json"
            );
        }
        return buttonPin;
    }

    void PairingButton::buttonISR() {
        const auto pb = &getInstance(nullptr, nullptr, nullptr);
        detachInterrupt(digitalPinToInterrupt(pb->mButtonPin));
        pb->startButtonPressTimer();

        // Force context switch if higher priority task was woken
        portYIELD_FROM_ISR(pdFALSE);
    }

    // ====================== Button Press Timer ======================
    void PairingButton::factoryResetTask(void *parameters) {
        auto& pb = *static_cast<PairingButton*>(parameters);

        pb.mButtonMode.store(ButtonModes::RESET);
        pb.mpDebugLED->createResetBlinkTask();
        pb.mpLogger->warning("PairingButton", "Clearing data...");

        // give time for blink DebugLED, clear data and reboot
        const auto dm = &DataManager::getInstance();
        dm->loadBaseConfig(true);
        vTaskDelay(pdMS_TO_TICKS(BUTTON_REBOOT_DELAY));

        const auto &powerManager = PowerManagerESP32::getInstance(pb.mpLogger);
        powerManager.safeRestart("PairingButton");

        for (;;)
            vTaskDelay(pdMS_TO_TICKS(1000));
    }

    void PairingButton::toggleOtaTask(void *parameters) {
        const auto &pb = *static_cast<PairingButton *>(parameters);

        auto &ota = ums::Ota::getInstance();
        ota.toggleOta();
        pb.mpLogger->debug("PairingButton", "Ota Toggled");

        vTaskDelay(pdMS_TO_TICKS(1000));

        pb.mpLogger->debug("PairingButton", "ToggleOtaTask delete");
        vTaskDelete(nullptr);
    }

    void PairingButton::buttonPressTimerCallback(TimerHandle_t xTimer) {
        auto &pb = *static_cast<PairingButton *>(pvTimerGetTimerID(xTimer));
        if (digitalRead(BUTTON_PIN) == LOW) {
            pb.mButtonPressCounter.fetch_add(1);
            pb.mButtonNotPressedCounter.store(3);

            // after pressing button for 10 seconds call createResetBlinkTask()
            if (DEBOUNCING_COUNTER_TO_SECONDS(pb.mButtonPressCounter.load()) >= 10 && pb.mButtonMode.load() !=
                ButtonModes::RESET) {
                xTaskCreate(
                    factoryResetTask,
                    "Factory Reset Task",
                    FACTORY_RESET_TASK_SIZE,
                    &pb,
                    CRITICAL_TASK_PRIORITY,
                    nullptr
                );
            }
            // after pressing button for 3 seconds call createPairingBlinkTask()
            else if (DEBOUNCING_COUNTER_TO_SECONDS(pb.mButtonPressCounter.load()) >= 3 && pb.mButtonMode.load() ==
                     ButtonModes::IDLE) {
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
                    xTaskCreate(
                        toggleOtaTask,
                        "Toggle Ota",
                        TOGGLE_OTA_TASK_SIZE,
                        &pb,
                        HIGH_TASK_PRIORITY,
                        nullptr
                    );
                    pb.mpLogger->debug("PairingButton Timer", "Toggle ota.");
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
                this,
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
