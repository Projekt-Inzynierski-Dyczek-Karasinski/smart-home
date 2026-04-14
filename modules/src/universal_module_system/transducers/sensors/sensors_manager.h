#pragma once

#include <Arduino.h>
#include <memory>
#include <map>
#include <atomic>

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
     *
     * @note To add a new sensor class, do all steps under Ctrl+F "ADD SENSOR" in this file and sensor_manager.cpp.
     */
    class SensorsManager {
    public:
        /**
         * @brief Gets the singleton instance of SensorsManager.
         *
         * @param logger Shared pointer to the logger instance, default: nullptr.
         * @return Reference to the SensorsManager instance.
         *
         * @warning First call must pass a pointer to a logger.
         */
        static SensorsManager &getInstance(const std::shared_ptr<ul::Logger> &logger = nullptr);

        // Delete copy constructor and assignment operator
        SensorsManager(const SensorsManager &) = delete;

        SensorsManager &operator =(const SensorsManager &) = delete;

        /**
         * @brief Get a reading of sensor. Automatically handles new reading if needed.
         * @param sensorId Sensor ID.
         * @return Reading of the sensor.
         */
        std::vector<API::APIParameterVariant> getSensorReading(uint8_t sensorId);

        /**
         * @brief Clears cached readings (if caching is disabled, does nothing).
         * @note Thread-safe.
         */
        void clearCachedReadings();

        /**
         * @brief Get sensors IDs.
         * @return Sensors IDs.
         */
        [[nodiscard]] std::vector<API::APIParameterVariant> getSensorsIds() const;

        /**
         * @brief Called in PowerManager before the device goes to sleep.
         */
        void onSleep();

        /**
         * @brief Sends RF notification about important sensor reading.
         */
        static void sendSensorNotification();

        /**
         * @brief Enables sending sensor alert RF notification. The armed state
         * is retained in RTC memory across reboots.
         */
        static void armSensorNotification();

    private:
        /**
         * @brief Get a reading of the sensor.
         * @details It is used when sensors readings caching is disabled.
         * @param sensorId Sensor ID.
         * @return Reading of the sensor.
         */
        std::vector<API::APIParameterVariant> getSensorCurrentReading(uint8_t sensorId);

        /**
         * @brief Get a reading of sensor.
         * @details Used when sensor reading caching is disabled.
         * @param sensorId Sensor ID.
         * @return Reading of the sensor.
         */
        std::vector<API::APIParameterVariant> getSensorCachedReading(uint8_t sensorId);

        /**
         * @brief Create sensor instances, start their readings, and start mSensorTimeoutTimer.
         * @warning <b>Not thread-safe</b>. Must be protected externally with <code>mSensorMutex</code> before calling.
         */
        void readAllSensors();

        /**
         * @brief Factory method to create a sensor instance from its type name.
         * @details When adding new sensor derived class, add here case for it.
         *
         * @param sensorName The string identifier for the sensor type.
         *
         * @return Unique pointer to the created Sensor instance, or nullptr if type is unknown.
         */
        std::unique_ptr<Sensor> createSensor(const char *sensorName);

        /**
         * @brief Struct used for converting sensor names to sensor objects.
         * @details When adding a new derived sensor class, add new values to the enum and map here.
         */
        struct SensorType {
            // ADD SENSOR 1: here add enum value for /c createSensor() method
            // e.g. <NEW_SENSOR>
            enum class SensorTypeEnum : uint8_t {
                BATTERY,
                LIGHT,
                DHT22,
                BME280,
                WINDOW,
                UNKNOWN
            };

            /**
            * @brief Functor for comparing C-string message identifiers in std::map.
            */
            struct Comparator {
                bool operator()(const char *a, const char *b) const {
                    return strncmp(a, b, strlen(a)) < 0;
                }
            };

            // ADD SENSOR 2: here add constexpr with sensor type (must be same as "type" in base_config.json)
            // e.g. static constexpr char s_<NEW_SENSOR>[] = "<newSensor>";
            // sensor types
            static constexpr char s_BATTERY_SENSOR[] = "batterySensor";
            static constexpr char s_LIGHT_SENSOR[] = "lightSensor";
            static constexpr char s_DHT22_SENSOR[] = "DHT22";
            static constexpr char s_BME280_SENSOR[] = "BME280";
            static constexpr char s_WINDOW_SENSOR[] = "windowSensor";

            // ADD SENSOR 3: here connect constexpr with sensor type and SensorTypeEnum
            // e.g. {s_<NEW_SENSOR>, SensorTypeEnum::<NEW_SENSOR>}
            // Lookup table mapping sensor type strings to internal enumerator values.
            inline static const std::map<const char *, SensorTypeEnum, Comparator> sensorMap{
                {s_BATTERY_SENSOR, SensorTypeEnum::BATTERY},
                {s_LIGHT_SENSOR, SensorTypeEnum::LIGHT},
                {s_DHT22_SENSOR, SensorTypeEnum::DHT22},
                {s_BME280_SENSOR, SensorTypeEnum::BME280},
                {s_WINDOW_SENSOR, SensorTypeEnum::WINDOW},
            };
        };

        /**
        * @brief Private constructor for singleton pattern.
        *
        * @param logger Shared pointer to the logger instance.
        */
        explicit SensorsManager(const std::shared_ptr<ul::Logger> &logger);

        /**
         * @brief Private destructor for singleton pattern.
         */
        ~SensorsManager();

        /**
         * @brief Clears readings of sensors.
         * @param xTimer Handle to the expired FreeRTOS timer.
         * @note Thread-safe.
         */
        static void sensorTimeoutTimerCallback(TimerHandle_t xTimer);

        std::vector<std::unique_ptr<Sensor> > mSensors;

        std::shared_ptr<ul::Logger> mpLogger;

        SemaphoreHandle_t mSensorMutex = nullptr;
        TimerHandle_t mSensorTimeoutTimer = nullptr;

        uint8_t mSensorsPowerPin;

        static constexpr uint16_t ms_DELAY_AFTER_POWERING_ON_SENSORS = 100; // ms

        static std::atomic<bool> msIsSensorNotificationArmed; ///< Saved in RTC memory

        // JSON keys
        static constexpr char ms_SENSORS_ARRAY[] = "sensors";
        static constexpr char ms_SENSOR_TYPE[] = "type";
        static constexpr char ms_ALL_SENSORS_DATA[] = "sensorsData";
        static constexpr char ms_POWER_PIN[] = "powerPin";
        static constexpr char ms_SENSOR_DATA[] = "data";
        static constexpr char ms_SENSOR_ID[] = "id";
        static constexpr char ms_SENSORS_READINGS_TIMEOUT[] = "readingsTimeout";
    };
}
