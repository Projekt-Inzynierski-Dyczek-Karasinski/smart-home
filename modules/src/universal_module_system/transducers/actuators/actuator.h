#pragma once

#include <Arduino.h>
#include <memory>

#include <nlohmann/json.hpp>

#include "utils/logger.h"
#include "communication/api/api_parameter.h"

namespace nl = nlohmann;
namespace ul = Utils::Logging;
namespace API = Comms::API;

using apiPv = API::APIParameterVariant;

namespace UniversalModuleSystem::Transducers {
    /**
     * @brief Standardized actuator state returned by actuators.
     */
    enum class actuatorState : uint8_t {
        UNKNOWN = 0,
        ON = 1,
        OFF = 2
    };

    /**
     * @brief Abstract base class for all actuators.
     * @details Provides common JSON configuration loading, thread-safe access to common actuator data,
     *          and a small API surface used by higher-level managers.
     */
    class Actuator {
    public:
        /**
         * @brief Construct the actuator base, creates a FreeRTOS mutex.
         *
         * @param logger Shared pointer to the logger instance.
         */
        explicit Actuator(const std::shared_ptr<ul::Logger> &logger);

        /**
         * @brief Virtual destructor, deletes the FreeRTOS mutex created in the constructor.
         */
        virtual ~Actuator();

        /**
         * @brief Execute an actuator-specific operation.
         *
         * @param operation Operation request.
         * @return Operation result or an error.
         *
         * @note Must be implemented by derived class.
         */
        virtual apiPv doOperation(apiPv operation) = 0;

        /**
         * @brief Toggle actuator state.
         * @details Default implementation returns BAD_ARGUMENT; derived classes should override if supported.
         *
         * @return New state or an error.
         */
        virtual apiPv toggle();

        /**
         * @brief Read current actuator state.
         * @details Default implementation returns actuatorState::UNKNOWN; derived classes should override if supported.
         *
         * @return ActuatorState or an error.
         */
        virtual apiPv getState();

        /**
         * @brief Callback executed on system boot to apply boot-time behavior.
         * @details Default implementation does nothing; derived classes should override if needed.
         *
         * @param jsonData JSON object containing actuator configuration.
         */
        virtual void onBoot(const nl::json &jsonData);

        /**
         * @brief Get actuator id.
         *
         * @return Actuator ID loaded from configuration.
         *
         * @note Thread-safe.
         */
        [[nodiscard]] uint8_t getId() const;

        /**
         * @brief Initialize actuator from JSON configuration.
         * @details Calls loadData(), which loads actuator data.
         *
         * @param jsonData JSON object containing actuator configuration.
         * @return True if configuration was loaded successfully, otherwise false.
         */
        bool init(const nl::json &jsonData);

    protected:
        /**
         * @brief Load actuator-type-specific configuration from JSON.
         *
         * @param jsonData JSON object containing actuator configuration.
         * @return True on success, otherwise false.
         *
         * @warning Do not take actuator data mutex in this method, because it is already taken in loadData().
         * This method should be called only inside loadData().
         */
        virtual bool loadAdditionalData(const nl::json &jsonData);

        /**
         * @brief Load common actuator configuration and then call loadAdditionalData() for actuator-type-specific configuration.
         *
         * @param jsonData JSON object containing actuator configuration.
         * @return True on success, otherwise false.
         *
         * @note Thread-safe.
         */
        bool loadData(const nl::json &jsonData);

        /**
         * @brief Common configuration shared by all actuators.
         * @details Parsed from JSON and stored in mCommonActuatorData.
         */
        struct CommonActuatorData {
            /**
             * @brief Construct CommonActuatorData from JSON parameters.
             * @param json JSON object with actuator data.
             */
            explicit CommonActuatorData(const nl::json &json);

            CommonActuatorData() = default;

            // Actuator common parameters
            uint8_t id = 0;
            uint8_t logicPin = 0;
            bool isLoaded = false;

        private:
            // JSON keys
            static constexpr char ms_ID[] = "id";
            static constexpr char ms_LOGIC_PIN[] = "logicPin";
        };
        std::shared_ptr<ul::Logger> mpLogger;
        CommonActuatorData mCommonActuatorData{};

        SemaphoreHandle_t mActuatorDataMutex = nullptr; ///< FreeRTOS mutex protecting actuator data.
    };
}
