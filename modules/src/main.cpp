#include <Arduino.h>
#include <memory>

#include "universal_module_system/debug_led.h"
#include "universal_module_system/pairing_button.h"
#include "universal_module_system/data_manager.h"
#include "universal_module_system/power_manager/power_manager.h"
#include "universal_module_system/transducers/sensors/sensors_manager.h"
#include "universal_module_system/transducers/actuators/actuators_manager.h"
#include "communication/communication.h"
#include "utils/logger.h"
#include "universal_module_system/ota/ota.h"

namespace ul = Utils::Logging;
namespace ums = UniversalModuleSystem;

void setup() {
    // TODO !pr remove VERBOSE
    const auto logger = std::make_shared<ul::Logger>(ul::Level::VERBOSE);

    auto &dataManager = ums::DataManager::getInstance(logger);

    // start Ota if OTA_CHECK and "version" in /data/base_config.json do not match.
    auto &ota = ums::Ota::getInstance(logger);
    ota.autoBeginOta();

    const auto debugLed = std::make_shared<ums::DebugLED>(logger);
    auto &communication = Comms::Communication::getInstance(debugLed, logger);
    auto &pairingButton = ums::PairingButton::getInstance(debugLed, &communication, logger);

    auto &powerManager = ums::PowerManager::getInstance(logger);
    auto &sensorManager = ums::Transducers::SensorsManager::getInstance(logger);
    auto &actuatorManager = ums::Transducers::ActuatorsManager::getInstance(logger);

    logger->info("Main", "All components initialized.");
    logger->verbose("Main", "Deleting functions setup() and loop()...");

    vTaskDelete(nullptr);

    // everything below this comment should never be executed
    logger->error("Main", "Failed to delete setup().");
}

void loop() {
    ul::Logger logger;
    logger.error("Main", "Failed to delete loop().");
    vTaskDelay(pdMS_TO_TICKS(1000));
}
