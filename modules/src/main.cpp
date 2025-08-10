#include <Arduino.h>
#include <HardwareSerial.h>

#include "universal_module_system/debug_led.h"
#include "universal_module_system/pairing_button.h"

#include "communication/communication.h"

#include "utils/logger.h"

namespace ul = Utils::Logging;
void setup() {
    vTaskDelay(pdTICKS_TO_MS(1000));
    ul::Logger logger = ul::Logger();
    DebugLED* debugLed = DebugLED::getInstance(&logger);
    Communication& communication = Communication::getInstance(debugLed);
    PairingButton* pairingButton = PairingButton::getInstance(debugLed, &communication);

    logger.info("Main", "Deleting functions setup() and loop().");
    vTaskDelete(nullptr);
    logger.error("Main", "Failed to delete setup().");
}

void loop() {
    ul::Logger logger = ul::Logger();
    logger.error("Main", "Failed to delete loop().");
    vTaskDelay(pdMS_TO_TICKS(1000));
}