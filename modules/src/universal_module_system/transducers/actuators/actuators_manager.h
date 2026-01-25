#pragma once

// TODO !pr add comments and remove unused #includes for actuators folder

#include <Arduino.h>
#include <memory>
#include <map>

#include "utils/logger.h"
#include "actuator.h"
#include "communication/api/api_parameter.h"

namespace nl = nlohmann;
namespace ul = Utils::Logging;
namespace API = Comms::API;

using apiPv = API::APIParameterVariant;

namespace UniversalModuleSystem::Transducers {
    class ActuatorsManager {
    public:
        static ActuatorsManager& getInstance(const std::shared_ptr<ul::Logger> &logger = nullptr);

        // Delete copy constructor and assignment operator
        ActuatorsManager(const ActuatorsManager&) = delete;
        ActuatorsManager& operator = (const ActuatorsManager&) = delete;

        apiPv getActuatorState(uint8_t actuatorId);

        apiPv toggleActuator(uint8_t actuatorId);

        apiPv actuatorDoOperation(uint8_t actuatorId, const apiPv& operation);
        
    private:
        /**
         * @brief Struct used for converting actuator names to actuator objects.
         * @details When adding a new derived actuator class, add new values to the enum and map here.
         */
        struct ActuatorType {
            enum class ActuatorTypeEnum : uint8_t {
                RELAY,
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

            // actuator types
            static constexpr char s_RELAY[] = "relay";
            // Lookup table mapping actuator type strings to internal enumerator values.
            inline static const std::map<const char*, ActuatorTypeEnum, Comparator> actuatorMap {
                {s_RELAY, ActuatorTypeEnum::RELAY},
            };
        };

        struct ActuatorCreationResult {
            std::unique_ptr<Actuator> actuator = nullptr;
            std::optional<apiPv> error;
        };

        explicit ActuatorsManager(const std::shared_ptr<ul::Logger> &logger);

        ~ActuatorsManager() = default;

        ActuatorCreationResult handleCreatingActuator(uint8_t actuatorId);

        [[nodiscard]] std::optional<nl::json> getActuatorJsonData(uint8_t actuatorId) const;
        
        std::unique_ptr<Actuator> createActuator(const char* actuatorName);
        
        std::shared_ptr<ul::Logger> mpLogger;

        // JSON keys
        static constexpr char ms_ACTUATORS_ARRAY[] = "actuators";
        static constexpr char ms_ACTUATOR_TYPE[] = "type";
        static constexpr char ms_ALL_ACTUATORS_DATA[] = "actuatorsData";
        static constexpr char ms_ACTUATOR_DATA[] = "data";
        static constexpr char ms_ACTUATOR_ID[] = "id";
    };
}
