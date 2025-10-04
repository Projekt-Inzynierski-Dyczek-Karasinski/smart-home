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
// struct Test {
//     bool a;
//     uint8_t b;
//
//     void printnl() const {
//         char res[30];
//         sprintf(res, "Test(a: %i, b: %i)", a, b);
//         Serial.println(res);
//     }
// };


void setup() {
    // Test tests[3];
    // tests[0] = {false,0};
    // tests[1] = {true,1};
    // tests[2] = {false,2};
    // nl::json jsonTest = nl::json::array();
    // for (auto & test : tests) {
    //     nl::json tmpJson;
    //     tmpJson["a"] = test.a;
    //     tmpJson["b"] = test.b;
    //     jsonTest.push_back(tmpJson);
    // }

    const auto logger = std::make_shared<ul::Logger>();

    // Serial.println(jsonTest.dump().c_str());
    // Test result[3];
    // uint8_t i = 0;
    // for (auto & tmpJson : jsonTest) {
    //     result[i].a = tmpJson["a"];
    //     result[i].b = tmpJson["b"];
    //     result[i].printnl();
    //     i++;
    // }

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