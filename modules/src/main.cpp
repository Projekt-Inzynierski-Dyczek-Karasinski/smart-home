#include <Arduino.h>
#include <memory>

#include "universal_module_system/debug_led.h"
#include "universal_module_system/pairing_button.h"
#include "universal_module_system/power_manager/power_manager.h"
#include "communication/communication.h"
#include "utils/logger.h"

namespace ul = Utils::Logging;
namespace ums = UniversalModuleSystem;

void setup() {
    const auto logger = std::make_shared<ul::Logger>();

    auto & powerManager = ums::PowerManager::getInstance(logger);

    const auto debugLed = std::make_shared<ums::DebugLED>(logger);
    auto & communication = Comms::Communication::getInstance(debugLed, logger);
    auto & pairingButton = ums::PairingButton::getInstance(debugLed, &communication, logger);

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