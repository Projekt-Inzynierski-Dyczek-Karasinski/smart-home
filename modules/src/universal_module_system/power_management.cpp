#include "power_management.h"

#include <driver/rtc_io.h>

#include "data_manager.h"
#include "../config/common/smart_home_config.h"

#include "utils/logger.h"
#include "communication/communication.h"

namespace ul = Utils::Logging;

namespace UniversalModuleSystem {
    namespace PowerManagement {
        void waitAndDisableCriticalFeatures() {
            const auto &communication = Comms::Communication::getInstance(nullptr, nullptr);
            const auto &dataManager = DataManager::getInstance();
            const ul::Logger logger;

            communication.waitAndDisableRfModule();
            dataManager.waitAndDisable();
            logger.waitAndDisable();
        }

        void safeRestart(const char *source) {
            ul::Logger logger;
            logger.warning(source, "Safe Rebooting...");
            waitAndDisableCriticalFeatures();
            ESP.restart();
        }

        void enterSleep(const uint32_t seconds) {
            constexpr uint32_t US_TO_SECONDS_FACTOR = 1000000;

            // button wake up
            rtc_gpio_pulldown_dis(BUTTON_PIN_AS_GPIO);
            rtc_gpio_pullup_en(BUTTON_PIN_AS_GPIO);
            esp_sleep_enable_ext0_wakeup(BUTTON_PIN_AS_GPIO, LOW);

            // timer wake up
            esp_sleep_enable_timer_wakeup(seconds * US_TO_SECONDS_FACTOR);

            ul::Logger logger;
            logger.infov("PowerManagement", "Going to sleep for (s): ", seconds);

            waitAndDisableCriticalFeatures();
            esp_deep_sleep_start();
        }

        // TODO !BEFORE PULL REQUEST! remove
        void printWakeUpReason() {
            const esp_sleep_wakeup_cause_t wakeupReason = esp_sleep_get_wakeup_cause();

            switch (wakeupReason) {
                case ESP_SLEEP_WAKEUP_EXT0:     Serial.println("Wakeup caused by external signal using RTC_IO"); break;
                case ESP_SLEEP_WAKEUP_EXT1:     Serial.println("Wakeup caused by external signal using RTC_CNTL"); break;
                case ESP_SLEEP_WAKEUP_TIMER:    Serial.println("Wakeup caused by timer"); break;
                case ESP_SLEEP_WAKEUP_TOUCHPAD: Serial.println("Wakeup caused by touchpad"); break;
                case ESP_SLEEP_WAKEUP_ULP:      Serial.println("Wakeup caused by ULP program"); break;
                default:                        Serial.printf("Wakeup was not caused by deep sleep: %d\n", wakeupReason); break;
            }
        }
    }
}
