#pragma once

#include <memory>
#include <nlohmann/json.hpp>

#include "utils/logger.h"

namespace nl = nlohmann;
namespace ul = Utils::Logging;

namespace UniversalModuleSystem::Transducers {
    /**
     * @brief Abstract base class for all sensors.
     * @details Defines the required interface and common data for sensors.
     */
    class Sensor {
    public:
        /**
         * @brief Construct the Sensor object, creates FreeRTOS resources common for all sensors.
         * @param logger Shared pointer to logger.
         */
        explicit Sensor(const std::shared_ptr<ul::Logger> &logger);

        /**
         * @brief Virtual destructor for proper cleanup in derived classes, deletes common FreeRTOS resources.
         */
        virtual ~Sensor();

        /**
         * @brief Getter for reading from the sensor.
         * @return Sensor reading.
         *
         * @note This method must be implemented by derived class.
         */
        virtual uint32_t getReading() = 0;

        /**
         * @brief Start acquiring data from the sensor.
         *
         * @note This method must be implemented by derived class.
         */
        virtual void startReading() = 0;

        /**
         * @brief Get the sensor ID (loaded from JSON config file).
         * @return Sensor ID.
         *
         * @note Thread-safe.
         */
        uint8_t getId() const;

        /**
         * @brief Initialize sensor using JSON data.
         * @param jsonData JSON object containing sensor parameters.
         * @return True if initialization succeeds, false otherwise.
         */
        bool init(const nl::json &jsonData);

        /**
         * @brief Get formatted reading ("id:value").
         * @return Reading in API format.
         */
        String getApiFormattedReading();

    protected:
        /**
         * @brief Load sensor-specific configuration from JSON.
         *
         * @param jsonData JSON object containing sensor data.
         * @return True if loading succeeds, false otherwise.
         *
         * @note This method must be implemented by derived class.
         * @warning Do <b>not</b> take <code>mSensorDataMutex</code> inside this method.
         * This method is called in <code>loadData()</code>, where this mutex is already taken.
         */
        virtual bool loadAdditionalData(const nl::json& jsonData) = 0;

        /**
         * @brief Load common sensor parameters from JSON.
         *
         * @param jsonData JSON object containing sensor data.
         * @return True if loading succeeds, false otherwise.
         *
         * @note Thread-safe.
         */
        bool loadData(const nl::json &jsonData);

        /**
         * @brief Structure holding common parameters for all sensors.
         */
        struct CommonSensorData {
            /**
             * @brief Construct CommonSensorData from JSON parameters.
             * @param json JSON object with sensor data.
             */
            explicit CommonSensorData(const nl::json& json);

            CommonSensorData() = default;

            // Sensor common parameters
            uint8_t id = 0;
            uint8_t readPin = 0;
            // TODO object with type e.g.:
            /*
             *  "readPin": 12, ->
             *  "connection": {
             *      "type": "single_pin",
             *      "pin": 12
             *  }
             */
            uint8_t powerPin = 0;
            bool canAwake = false;
            bool isLoaded = false;

        private:
            // JSON keys
            static constexpr char ms_ID[] = "id";
            static constexpr char ms_READ_PIN[] = "readPin";
            static constexpr char ms_POWER_PIN[] = "powerPin";
            static constexpr char ms_CAN_AWAKE[] = "canAwake";
        };

        std::shared_ptr<ul::Logger> mpLogger;
        CommonSensorData mCommonSensorData{};

        SemaphoreHandle_t mSensorDataMutex = nullptr; ///< FreeRTOS mutex protecting sensor data.
        SemaphoreHandle_t mReadingCompleteSemaphore = nullptr; ///< Semaphore indicating if sensor reading is complete.
    };
}