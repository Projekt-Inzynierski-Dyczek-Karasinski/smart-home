#include <Arduino.h>
#include <memory>

#include "universal_module_system/debug_led.h"
#include "universal_module_system/pairing_button.h"
#include "universal_module_system/power_manager/power_manager.h"
#include "universal_module_system/transducers/sensors/sensors_manager.h"
#include "communication/communication.h"
#include "utils/logger.h"
// #include "universal_module_system/ota/ota.h"
#include "universal_module_system/data_manager.h"

#include <driver/rtc_io.h>

namespace ul = Utils::Logging;
namespace ums = UniversalModuleSystem;

void setup() {
    // TODO !pr remove VERBOSE
    const auto logger = std::make_shared<ul::Logger>(ul::Level::VERBOSE);

    auto &dataManager = ums::DataManager::getInstance(logger);

    // start Ota if CONFIG_VERSION and "version" in /data/base_config.json do not match.
    // ums::Ota ota(logger);
    // ota.autoEnableOta();

    auto &powerManager = ums::PowerManager::getInstance(logger);

    const auto debugLed = std::make_shared<ums::DebugLED>(logger);
    auto &communication = Comms::Communication::getInstance(debugLed, logger);
    auto &pairingButton = ums::PairingButton::getInstance(debugLed, &communication, logger);
    auto &sensorManager = ums::Transducers::SensorsManager::getInstance(logger);

    // TODO !pr remove
    // auto bme280V = sensorManager.getSensorReading(4);
    // for (auto &param: bme280V) {
    //     if (auto *p = std::get_if<API::APIParameter<float> >(&param)) {
    //         logger->errorv("Main TMP", "bme : ", p->getValue());
    //     }
    // }
    // auto dhtParamV = sensorManager.getSensorReading(3);
    // for (auto &param: dhtParamV) {
    //     if (auto *p = std::get_if<API::APIParameter<float> >(&param)) {
    //         logger->errorv("Main TMP", "dht : ", p->getValue());
    //     }
    // }
    //
    // auto batteryReadParamV = sensorManager.getSensorReading(1)[0];
    // if (auto *batteryReadParam = std::get_if<API::APIParameter<uint8_t> >(&batteryReadParamV)) {
    //     logger->errorv("Main TMP", "battery %: ", batteryReadParam->getValue());
    // }
    //
    // auto lightReadParamV = sensorManager.getSensorReading(2)[0];
    // if (auto *lightReadParam = std::get_if<API::APIParameter<uint8_t> >(&lightReadParamV)) {
    //     logger->errorv("Main TMP", "light %: ", lightReadParam->getValue());
    // }
    //
    // logger->error("Main TMP", "Waiting 10s...");
    // vTaskDelay(pdMS_TO_TICKS(10000)); // 10s
    //
    // // auto &sensorManager = ums::Transducers::SensorsManager::getInstance(logger);
    // auto bme280V2 = sensorManager.getSensorReading(4);
    // for (auto &param: bme280V2) {
    //     if (auto *p = std::get_if<API::APIParameter<float> >(&param)) {
    //         logger->errorv("Main TMP", "bme : ", p->getValue());
    //     }
    // }
    // auto dhtParamV2 = sensorManager.getSensorReading(3);
    // for (auto &param: dhtParamV2) {
    //     if (auto *p = std::get_if<API::APIParameter<float> >(&param)) {
    //         logger->errorv("Main TMP", "dht : ", p->getValue());
    //     }
    // }
    //
    // auto batteryReadParamV2 = sensorManager.getSensorReading(1)[0];
    // if (auto *batteryReadParam = std::get_if<API::APIParameter<uint8_t> >(&batteryReadParamV2)) {
    //     logger->errorv("Main TMP", "battery %: ", batteryReadParam->getValue());
    // }
    //
    // auto lightReadParamV2 = sensorManager.getSensorReading(2)[0];
    // if (auto *lightReadParam = std::get_if<API::APIParameter<uint8_t> >(&lightReadParamV2)) {
    //     logger->errorv("Main TMP", "light %: ", lightReadParam->getValue());
    // }
    // TODO !pr remove


    logger->info("Main", "All components initialized. Deleting functions setup() and loop()...");
    vTaskDelete(nullptr);

    // everything below this comment should never be executed
    logger->error("Main", "Failed to delete setup().");
}

void loop() {
    ul::Logger logger;
    logger.error("Main", "Failed to delete loop().");
    vTaskDelay(pdMS_TO_TICKS(1000));
}
