#pragma once

#include <Arduino.h>
#include <memory>
#include <vector>

#include <nlohmann/json.hpp>

#include "utils/logger.h"
#include "communication/api/api_parameter.h"

namespace nl = nlohmann;
namespace ul = Utils::Logging;
namespace API = Comms::API;

using apiPv = API::APIParameterVariant;

namespace UniversalModuleSystem::Transducers {
    enum class actuatorState : uint8_t {
        UNKNOWN = 0,
        ON = 1,
        OFF = 2
    };

    class Actuator {
    public:
        explicit Actuator(const std::shared_ptr<ul::Logger> &logger);

        virtual ~Actuator();

        virtual apiPv doOperation(apiPv operation) = 0;

        virtual apiPv toggle();

        virtual apiPv getState();

        [[nodiscard]] uint8_t getId() const;

        bool init(const nl::json &jsonData);

        virtual void onBoot(const nl::json &jsonData);

    protected:
        virtual bool loadAdditionalData(const nl::json &jsonData);

        bool loadData(const nl::json &jsonData);

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
