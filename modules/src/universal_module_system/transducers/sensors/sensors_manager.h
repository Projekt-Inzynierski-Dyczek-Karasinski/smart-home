#pragma once

#include <Arduino.h>
#include <memory>
#include <map>

#include "utils/logger.h"
#include "sensor.h"
#include "communication/api/api_parameter.h"

namespace ul = Utils::Logging;
namespace API = Comms::API;

namespace UniversalModuleSystem::Transducers {
    /**
     * @brief Singleton manager class that handles sensors.
     *
     * @details This class is responsible for maintaining the lifecycle of sensor objects,
     * managing sensor creation based on sensor types, and providing consolidated sensor data reports.
     */
    class SensorsManager {
    public:
        /**
         * @brief Gets the singleton instance of SensorsManager.
         *
         * @param logger Shared pointer to the logger instance, default: nullptr.
         * @return Reference to the SensorClientCode instance.
         *
         * @warning First call must pass a pointer to a logger.
         */
        static SensorsManager& getInstance(const std::shared_ptr<ul::Logger> &logger = nullptr);

        // Delete copy constructor and assignment operator
        SensorsManager(const SensorsManager&) = delete;
        SensorsManager& operator = (const SensorsManager&) = delete;

        /**
         * @brief Get a reading of sensor.
         * @param sensorId Sensor ID.
         * @return Reading of the sensor.
         */
        std::vector<API::APIParameterVariant> getSensorReading(uint8_t sensorId);

    private:
        /**
         * @brief Factory method to create a sensor instance from its type name.
         * @details When adding new sensor derived class, add here case for it.
         *
         * @param sensorName The string identifier for the sensor type.
         *
         * @return Unique pointer to the created Sensor instance, or nullptr if type is unknown.
         */
        std::unique_ptr<Sensor> createSensor(const char* sensorName);

        /**
         * @brief Struct used for converting sensor names to sensor objects.
         * @details When adding a new derived sensor class, add new values to the enum and map here.
         */
        struct SensorType {
            enum class SensorTypeEnum : uint8_t {
                BATTERY,
                LIGHT,
                DHT22,
                UNKNOWN
            };

            /**
            * @brief Functor for comparing C-string message identifiers in std::map.
            */
            struct Comparator {
                bool operator()(const char* a, const char* b) const {
                    return strncmp(a, b, strlen(a)) < 0;
                }
            };

            // sensor types
            static constexpr char s_BATTERY_SENSOR[] = "batterySensor";
            static constexpr char s_LIGHT_SENSOR[] = "lightSensor";
            static constexpr char s_DHT22_SENSOR[] = "DHT22";

            // Lookup table mapping sensor type strings to internal enumerator values.
            inline static const std::map<const char*, SensorTypeEnum, Comparator> sensorMap {
                {s_BATTERY_SENSOR, SensorTypeEnum::BATTERY},
                {s_LIGHT_SENSOR, SensorTypeEnum::LIGHT},
                {s_DHT22_SENSOR, SensorTypeEnum::DHT22},
            };
        };

        /**
        * @brief Private constructor for singleton pattern.
        *
        * @param logger Shared pointer to the logger instance.
        */
        explicit SensorsManager(const std::shared_ptr<ul::Logger> &logger);

        /**
         * @brief Private destructor for singleton pattern
         */
        ~SensorsManager() = default;

        std::shared_ptr<ul::Logger> mpLogger;

        // JSON keys
        static constexpr char ms_SENSORS_ARRAY[] = "sensors";
        static constexpr char ms_SENSOR_TYPE[] = "type";
        static constexpr char ms_ALL_SENSORS_DATA[] = "sensorsData";
        static constexpr char ms_POWER_PIN[] = "powerPin";
        static constexpr char ms_SENSOR_DATA[] = "data";
        static constexpr char ms_SENSOR_ID[] = "id";
    };
}
