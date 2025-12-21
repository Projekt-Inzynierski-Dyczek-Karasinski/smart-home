#include "sensors_manager.h"

#include <nlohmann/json.hpp>

#include "batterySensor/battery_sensor.h"
#include "lightSensor/light_sensor.h"
#include "dht_22_sensor/dht_22_sensor.h"
#include "bme_280/bme_280.h"

#include "universal_module_system/data_manager.h"

namespace nl = nlohmann;
namespace UniversalModuleSystem::Transducers {
    SensorsManager &SensorsManager::getInstance(const std::shared_ptr<ul::Logger> &logger) {
        static SensorsManager instance(logger);
        return instance;
    }

    SensorsManager::SensorsManager(const std::shared_ptr<ul::Logger> &logger) : mpLogger(logger) {
        if (logger == nullptr) {
            mpLogger = std::make_shared<ul::Logger>();
            mpLogger->error("SensorsManager", "SensorsManager's constructor didn't get pointer to logger instance.");
        }

        const auto &dataManager = DataManager::getInstance();
        nl::json jsonData = dataManager.loadJson(dataManager.s_BASE_CONFIG_PATH);
        const uint8_t powerPin = jsonData[ms_ALL_SENSORS_DATA][ms_POWER_PIN].get<uint8_t>();
        pinMode(powerPin, OUTPUT);
        //TODO !pr move this to method that init reading of the sensor
        digitalWrite(powerPin, HIGH);

        mpLogger->verbose("SensorsManager", "SensorsManager initialized.");
    }

    // TODO !pr add powering on sensors
    std::vector<API::APIParameterVariant> SensorsManager::getSensorReading(const uint8_t sensorId) {
        using ET = API::errorTypes;

        const auto &dataManager = DataManager::getInstance();
        nl::json jsonData = dataManager.loadJson(dataManager.s_BASE_CONFIG_PATH);
        nl::json &sensorData = jsonData[ms_ALL_SENSORS_DATA];
        if (jsonData.empty())
            return std::vector<API::APIParameterVariant> {API::APIParameter((uint8_t)ET::INTERNAL_ERROR, true)};

        for (auto &jsonSensor : sensorData[ms_SENSORS_ARRAY]) {
            if (jsonSensor[ms_SENSOR_DATA][ms_SENSOR_ID].get<uint8_t>() != sensorId) continue;

            std::unique_ptr<Sensor> sensor = createSensor(jsonSensor[ms_SENSOR_TYPE].get<std::string>().c_str());
            if (sensor == nullptr)
                return std::vector<API::APIParameterVariant> {API::APIParameter((uint8_t)ET::BAD_ARGUMENT, true)};

            if (sensor->init(jsonSensor[ms_SENSOR_DATA])) {
                sensor->startReading();
                return sensor->getApiFormattedReading();
            }
        }

        return std::vector<API::APIParameterVariant> {API::APIParameter((uint8_t)ET::BAD_ARGUMENT, true)};
    }

    std::unique_ptr<Sensor> SensorsManager::createSensor(const char *sensorName) {
        using ST = SensorType;
        using STE = SensorType::SensorTypeEnum;
        switch (
            const auto it = ST::sensorMap.find(sensorName);
            it != ST::sensorMap.end() ? it->second : STE::UNKNOWN
        ) {
            case STE::BATTERY:
                return std::make_unique<BatterySensor>(mpLogger);

            case STE::LIGHT:
                return std::make_unique<LightSensor>(mpLogger);

            case STE::DHT22:
                return std::make_unique<Dht22Sensor>(mpLogger);

            case STE::BME280:
                return std::make_unique<BME280>(mpLogger);

            default:
                mpLogger->error("SensorsManager", "Got unknown type of sensor.");
                return nullptr;
        }
    }
}