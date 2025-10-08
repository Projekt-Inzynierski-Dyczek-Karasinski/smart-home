#include <Arduino.h>
#include <memory>

#include "universal_module_system/debug_led.h"
#include "universal_module_system/pairing_button.h"
#include "communication/communication.h"
#include "utils/logger.h"

// TODO !BEFORE PULL REQUEST! remove
#include "universal_module_system/power_manager.h"

namespace ul = Utils::Logging;
namespace ums = UniversalModuleSystem;

void setup() {
    const auto logger = std::make_shared<ul::Logger>();

    // TODO !BEFORE PULL REQUEST! remove
    auto & powerManager = ums::PowerManager::getInstance(logger);

    const auto debugLed = std::make_shared<ums::DebugLED>(logger);
    auto & communication = Comms::Communication::getInstance(debugLed, logger);
    auto & pairingButton = ums::PairingButton::getInstance(debugLed, &communication, logger);

    // TODO !BEFORE PULL REQUEST! remove
    // vTaskDelay(pdMS_TO_TICKS(10000));
    // powerManager.enterSleep(30, true);

    logger->info("Main", "Deleting functions setup() and loop().");
    vTaskDelete(nullptr);

    // everything below this comment should never be executed
    logger->error("Main", "Failed to delete setup().");
}

void loop() {
    ul::Logger logger;
    logger.error("Main", "Failed to delete loop().");
    vTaskDelay(pdMS_TO_TICKS(1000));
}