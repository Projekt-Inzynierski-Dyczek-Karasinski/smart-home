#include <Arduino.h>
#include <HardwareSerial.h>
#include <memory>

#include "universal_module_system/debug_led.h"
#include "universal_module_system/pairing_button.h"

#include "communication/communication.h"

#include "utils/logger.h"

namespace ul = Utils::Logging;

void setup() {
    // TODO remove delay before merge with main
    vTaskDelay(pdTICKS_TO_MS(1000));
    const auto logger = std::make_shared<ul::Logger>(ul::Level::DEBUG);

    DebugLED* debugLed = DebugLED::getInstance(logger);
    Communication& communication = Communication::getInstance(debugLed, logger);
    PairingButton* pairingButton = PairingButton::getInstance(debugLed, &communication, logger);

    logger->info("Main", "Deleting functions setup() and loop().");
    vTaskDelete(nullptr);

    // everything below this comment should never be executed
    logger->error("Main", "Failed to delete setup().");
}

void loop() {
    ul::Logger logger = ul::Logger();
    logger.error("Main", "Failed to delete loop().");
    vTaskDelay(pdMS_TO_TICKS(1000));
}