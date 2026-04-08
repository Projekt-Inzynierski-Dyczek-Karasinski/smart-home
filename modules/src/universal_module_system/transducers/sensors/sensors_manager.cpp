#include "sensors_manager.h"

#include <nlohmann/json.hpp>

#include "universal_module_system/data_manager.h"
#include "communication/communication.h"
#include "communication/api/command_handler.h"

// ADD SENSOR 4: here include sensor class
// e.g. <new_sensor_class>/<new_sensor_class>.h
#include "battery_sensor/battery_sensor.h"
#include "light_sensor/light_sensor.h"
#include "dht_22_sensor/dht_22_sensor.h"
#include "bme_280/bme_280.h"
#include "window_sensor/window_sensor.h"

namespace nl = nlohmann;

namespace UniversalModuleSystem::Transducers {
    SensorsManager &SensorsManager::getInstance(const std::shared_ptr<ul::Logger> &logger) {
        static SensorsManager instance(logger);
        return instance;
    }

    SensorsManager::SensorsManager(const std::shared_ptr<ul::Logger> &logger) : mpLogger(logger),
        mSensorMutex(xSemaphoreCreateMutex()) {
        if (logger == nullptr) {
            mpLogger = std::make_shared<ul::Logger>();
            mpLogger->error("SensorsManager", "SensorsManager's constructor didn't get pointer to logger instance.");
        }

        const auto &dataManager = DataManager::getInstance();
        nl::json jsonData = dataManager.loadJson(dataManager.s_BASE_CONFIG_PATH);
        const size_t numOfSensors = jsonData[ms_ALL_SENSORS_DATA][ms_SENSORS_ARRAY].size();
        const uint32_t sensorReadingsTimeout =
                jsonData[ms_ALL_SENSORS_DATA][ms_SENSORS_READINGS_TIMEOUT].get<uint32_t>();
        const uint8_t powerPin = jsonData[ms_ALL_SENSORS_DATA][ms_POWER_PIN].get<uint8_t>();

        mSensorsPowerPin = powerPin;
        pinMode(mSensorsPowerPin, OUTPUT);

        if (sensorReadingsTimeout != 0) {
            if (sensorReadingsTimeout < 5000)
                mpLogger->warningv(
                    "SensorsManager",
                    "Sensors readings timeout is set only to (ms):",
                    (int) sensorReadingsTimeout
                );
            mSensorTimeoutTimer = xTimerCreate(
                "Sensor Timeout",
                pdMS_TO_TICKS(sensorReadingsTimeout),
                pdFALSE,
                this,
                sensorTimeoutTimerCallback
            );
            xSemaphoreTake(mSensorMutex, portMAX_DELAY);
            mSensors.reserve(numOfSensors);
            readAllSensors();
            xSemaphoreGive(mSensorMutex);
        }

        mpLogger->verbose("SensorsManager", "SensorsManager initialized.");
    }

    SensorsManager::~SensorsManager() {
        if (mSensorTimeoutTimer != nullptr) {
            xTimerDelete(mSensorTimeoutTimer, portMAX_DELAY);
            mSensorTimeoutTimer = nullptr;
        }
        if (mSensorMutex != nullptr) {
            vSemaphoreDelete(mSensorMutex);
            mSensorMutex = nullptr;
        }
    }

    std::vector<API::APIParameterVariant> SensorsManager::getSensorReading(const uint8_t sensorId) {
        if (mSensorTimeoutTimer == nullptr) {
            digitalWrite(mSensorsPowerPin, HIGH);
            vTaskDelay(pdMS_TO_TICKS(ms_DELAY_AFTER_POWERING_ON_SENSORS));

            std::vector<API::APIParameterVariant> result = getSensorCurrentReading(sensorId);

            digitalWrite(mSensorsPowerPin, LOW);
            return result;
        } else {
            return getSensorCachedReading(sensorId);
        }
    }

    void SensorsManager::clearCachedReadings() {
        if (mSensorTimeoutTimer == nullptr) return;

        xTimerStop(mSensorTimeoutTimer, portMAX_DELAY);
        xSemaphoreTake(mSensorMutex, portMAX_DELAY);
        mSensors.clear();
        xSemaphoreGive(mSensorMutex);
    }

    std::vector<API::APIParameterVariant> SensorsManager::getSensorsIds() const {
        using ET = API::errorTypes;

        const auto &dataManager = DataManager::getInstance();
        nl::json jsonData = dataManager.loadJson(dataManager.s_BASE_CONFIG_PATH);
        nl::json &sensorData = jsonData[ms_ALL_SENSORS_DATA][ms_SENSORS_ARRAY];
        if (jsonData.empty())
            return std::vector<API::APIParameterVariant>{API::APIParameter((uint8_t) ET::INTERNAL_ERROR, true)};


        std::vector<API::APIParameterVariant> result;
        result.reserve(sensorData.size());
        for (auto &jsonSensor: sensorData) {
            result.emplace_back(API::APIParameter<uint8_t>(jsonSensor[ms_SENSOR_DATA][ms_SENSOR_ID].get<uint8_t>()));
        }
        return result;
    }

    void SensorsManager::onSleep() {
        const auto &dataManager = DataManager::getInstance();
        nl::json jsonData = dataManager.loadJson(dataManager.s_BASE_CONFIG_PATH);
        nl::json &sensorData = jsonData[ms_ALL_SENSORS_DATA][ms_SENSORS_ARRAY];

        for (const auto &jsonSensor: sensorData) {
            if (
                const std::unique_ptr<Sensor> sensor = createSensor(
                    jsonSensor[ms_SENSOR_TYPE].get<std::string>().c_str()
                );
                sensor->init(jsonSensor[ms_SENSOR_DATA])
            ) {
                sensor->onSleep();
            }
        }
    }

    void SensorsManager::sendSensorNotification() {
        try {
            API::CommandHandler commandHandler(API::commandTypes::NOTIFY);
            API::APIParameter notify(static_cast<uint8_t>(API::notifyTypes::SENSOR_ALERT));
            commandHandler.addParameter(notify);

            uint8_t message[MESSAGE_SIZE] = {};
            commandHandler.generateMessage(message);

            const auto &communication = Comms::Communication::getInstance();
            communication.sendMessage(message);
        } catch (std::exception &e) {
            const auto &sm = getInstance();
            sm.mpLogger->error(
                "SensorsManager",
                "Failed to send SENSOR_ALERT notification in sendSensorNotificationTask"
            );
            sm.mpLogger->error("SensorsManager", e.what());
        }
    }

    std::vector<API::APIParameterVariant> SensorsManager::getSensorCurrentReading(const uint8_t sensorId) {
        using ET = API::errorTypes;
        mpLogger->debug("SensorsManager", "getSensorCurrentReading");

        const auto &dataManager = DataManager::getInstance();
        nl::json jsonData = dataManager.loadJson(dataManager.s_BASE_CONFIG_PATH);
        nl::json &sensorData = jsonData[ms_ALL_SENSORS_DATA][ms_SENSORS_ARRAY];
        if (jsonData.empty())
            return std::vector<API::APIParameterVariant>{API::APIParameter((uint8_t) ET::INTERNAL_ERROR, true)};

        for (auto &jsonSensor: sensorData) {
            if (jsonSensor[ms_SENSOR_DATA][ms_SENSOR_ID].get<uint8_t>() != sensorId) continue;

            std::unique_ptr<Sensor> sensor = createSensor(jsonSensor[ms_SENSOR_TYPE].get<std::string>().c_str());
            if (sensor == nullptr)
                return std::vector<API::APIParameterVariant>{API::APIParameter((uint8_t) ET::BAD_ARGUMENT, true)};

            if (sensor->init(jsonSensor[ms_SENSOR_DATA])) {
                sensor->startReading();
                return sensor->getApiFormattedReading();
            }
        }

        return std::vector<API::APIParameterVariant>{API::APIParameter(static_cast<uint8_t>(ET::BAD_ARGUMENT), true)};
    }

    std::vector<API::APIParameterVariant> SensorsManager::getSensorCachedReading(const uint8_t sensorId) {
        using ET = API::errorTypes;
        mpLogger->debug("SensorsManager", "getSensorCachedReading");

        xSemaphoreTake(mSensorMutex, portMAX_DELAY);
        if (mSensors.empty()) readAllSensors();

        std::vector<API::APIParameterVariant> result = std::vector<API::APIParameterVariant>{
            API::APIParameter((uint8_t) ET::BAD_ARGUMENT, true)
        };

        for (const auto &sensor: mSensors) {
            if (sensor->getId() == sensorId) {
                result = sensor->getApiFormattedReading();
                break;
            }
        }
        xSemaphoreGive(mSensorMutex);

        return result;
    }

    void SensorsManager::readAllSensors() {
        mpLogger->verbose("SensorsManager", "New sensors reading...");
        if (!mSensors.empty()) mSensors.clear();

        const auto &dataManager = DataManager::getInstance();
        nl::json jsonData = dataManager.loadJson(dataManager.s_BASE_CONFIG_PATH);
        nl::json &sensorsData = jsonData[ms_ALL_SENSORS_DATA][ms_SENSORS_ARRAY];

        digitalWrite(mSensorsPowerPin, HIGH);
        vTaskDelay(pdMS_TO_TICKS(ms_DELAY_AFTER_POWERING_ON_SENSORS));

        // initialize sensors and start readings
        for (auto sensorData: sensorsData) {
            std::unique_ptr<Sensor> sensor = createSensor(sensorData[ms_SENSOR_TYPE].get<std::string>().c_str());
            if (sensor != nullptr && sensor->init(sensorData[ms_SENSOR_DATA])) {
                sensor->startReading();
                mSensors.push_back(std::move(sensor));
            }
        }

        // wait until reading ends before powering off sensors
        for (const auto &sensor: mSensors) {
            sensor->waitUntilReadEnds();
        }
        digitalWrite(mSensorsPowerPin, LOW);

        xTimerStart(mSensorTimeoutTimer, portMAX_DELAY);
    }

    std::unique_ptr<Sensor> SensorsManager::createSensor(const char *sensorName) {
        using ST = SensorType;
        using STE = SensorType::SensorTypeEnum;

        // ADD SENSOR 5 (final): here add case for new sensor
        /* e.g.:
            case STE::<NEW_SENSOR>:
                return std::make_unique<NewSensorClass>(mpLogger);
        */
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

            case STE::WINDOW:
                return std::make_unique<WindowSensor>(mpLogger);

            default:
                mpLogger->error("SensorsManager", "Got unknown type of sensor.");
                return nullptr;
        }
    }

    void SensorsManager::sensorTimeoutTimerCallback(TimerHandle_t xTimer) {
        auto &sm = *static_cast<SensorsManager *>(pvTimerGetTimerID(xTimer));
        sm.mpLogger->debug("SensorsManager", "sensorTimeoutTimerCallback");

        xSemaphoreTake(sm.mSensorMutex, portMAX_DELAY);
        sm.mSensors.clear();
        xSemaphoreGive(sm.mSensorMutex);
    }
}
