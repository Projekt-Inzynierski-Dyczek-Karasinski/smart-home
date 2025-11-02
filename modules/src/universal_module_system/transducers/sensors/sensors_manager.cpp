#include "sensors_manager.h"

#include <nlohmann/json.hpp>

#include "batterySensor/battery_sensor.h"
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

        mpLogger->verbose("SensorsManager", "SensorsManager initialized.");
    }
    
    String SensorsManager::getAllSensorsReport() {
        const auto &dataManager = DataManager::getInstance();
        nl::json jsonData = dataManager.loadJson(dataManager.s_BASE_CONFIG_PATH);

        const size_t numOfSensors = jsonData[ms_SENSORS_ARRAY].size();
        std::unique_ptr<Sensor> sensors[numOfSensors];
        uint8_t sensorsIndex = 0;

        for (auto &jsonSensor : jsonData[ms_SENSORS_ARRAY]) {
            sensors[sensorsIndex] = createSensor(jsonSensor[ms_SENSOR_TYPE].get<std::string>().c_str());

            if (sensors[numOfSensors] != nullptr) {
                if (sensors[sensorsIndex]->init(jsonSensor[ms_SENSOR_DATA])) {
                    sensors[sensorsIndex]->startReading();
                }
                sensorsIndex++;
            }
        }

        String result;
        const uint8_t numOfLoadedSensors = sensorsIndex;
        for (sensorsIndex = 0; sensorsIndex < numOfLoadedSensors; sensorsIndex++) {
            result.concat(sensors[sensorsIndex]->getApiFormattedReading());
            result.concat('|');
        }
        result.remove(result.length() - 1, 1);
        return result;
    }

    String SensorsManager::getSensorReport(const uint8_t sensorId) {
        const auto &dataManager = DataManager::getInstance();
        nl::json jsonData = dataManager.loadJson(dataManager.s_BASE_CONFIG_PATH);

        for (auto &jsonSensor : jsonData[ms_SENSORS_ARRAY]) {
            if (jsonSensor[ms_SENSOR_DATA][ms_SENSOR_ID] != sensorId) continue;

            std::unique_ptr<Sensor> sensor = createSensor(jsonSensor[ms_SENSOR_TYPE].get<std::string>().c_str());
            if (sensor == nullptr) return "";

            if (sensor->init(jsonSensor[ms_SENSOR_DATA])) {
                sensor->startReading();
                return sensor->getApiFormattedReading();
            }
        }
        return "";
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

            default:
                mpLogger->error("SensorsManager", "Got unknown type of sensor.");
                return nullptr;
        }
    }
}