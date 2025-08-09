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

    DebugLED* debugLed = DebugLED::getInstance();
    Communication& communication = Communication::getInstance(debugLed);
    PairingButton* pairingButton = PairingButton::getInstance(debugLed, &communication);

    logger.info("MAIN", "Deleting functions setup() and loop().");
    vTaskDelete(nullptr);
    logger.error("MAIN", "Failed to delete setup().");
}

void loop() {
    ul::Logger logger = ul::Logger();
    logger.error("MAIN", "Failed to delete loop().");
    vTaskDelay(pdMS_TO_TICKS(1000));
}