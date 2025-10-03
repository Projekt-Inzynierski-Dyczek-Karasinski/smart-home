#include <Arduino.h>
#include <memory>

// TODO !BEFORE PULL REQUEST! remove
// #include <nlohmann/json.hpp>
// #include <SPIFFS.h>

#include "universal_module_system/debug_led.h"
#include "universal_module_system/pairing_button.h"
#include "universal_module_system/data_manager.h"
#include "communication/communication.h"
#include "utils/logger.h"

namespace ul = Utils::Logging;
namespace ums = UniversalModuleSystem;
// TODO !BEFORE PULL REQUEST! remove
namespace nl = nlohmann;

void setup() {
    const auto logger = std::make_shared<ul::Logger>();

    const auto debugLed = std::make_shared<ums::DebugLED>(logger);

    Comms::Communication& communication = Comms::Communication::getInstance(debugLed, logger);
    ums::PairingButton& pairingButton = ums::PairingButton::getInstance(debugLed, &communication, logger);

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