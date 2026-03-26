#pragma once

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

    /**
     * @brief Singleton responsible for creating and operating actuators based on configuration.
     * @details The manager loads actuator definitions from JSON, creates the correct derived actuator type,
     *          initializes it, and exposes a uniform API to higher layers.
     *
     * @note To add a new actuator class, do all steps under Ctrl+F "ADD ACTUATOR" in this file and actuator_manager.cpp.
     */
    class ActuatorsManager {
    public:
        /**
         * @brief Get singleton instance of ActuatorsManager.
         * @details On first construction, the manager also performs boot-time handling for all configured actuators.
         *
         * @param logger Shared pointer to the logger instance, default: nullptr.
         * @return Reference to the singleton manager.
         *
         * @warning First call have to pass a pointer to logger.
         */
        static ActuatorsManager& getInstance(const std::shared_ptr<ul::Logger> &logger = nullptr);

        // Delete copy constructor and assignment operator
        ActuatorsManager(const ActuatorsManager&) = delete;
        ActuatorsManager& operator = (const ActuatorsManager&) = delete;

        /**
         * @brief Get the state of an actuator.
         *
         * @param actuatorId Actuator identifier (from config).
         * @return ActuatorState or an error code.
         */
        apiPv getActuatorState(uint8_t actuatorId);

        /**
         * @brief Toggle an actuator.
         *
         * @param actuatorId Actuator identifier (from config).
         * @return New actuatorState or an error code.
         */
        apiPv toggleActuator(uint8_t actuatorId);

        /**
         * @brief Execute an operation on an actuator by its ID.
         *
         * @param actuatorId Numeric actuator identifier (from config).
         * @param operation Operation request.
         * @return Operation result or an error code.
         */
        apiPv actuatorDoOperation(uint8_t actuatorId, const apiPv& operation);

        /**
         * @brief Get actuators IDs.
         * @return Actuators IDs.
         */
        [[nodiscard]] static std::vector<API::APIParameterVariant> getActuatorsIds();
        
    private:
        /**
         * @brief Struct used for converting actuator names to actuator objects.
         * @details When adding a new derived actuator class, add new values to the enum and map here.
         */
        struct ActuatorType {
            // ADD ACTUATOR 1: here add enum value for /c createActuator() method
            // e.g. <NEW_ACTUATOR>
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

            // ADD ACTUATOR 2: here add constexpr with actuator type (must be same as "type" in base_config.json)
            // e.g. static constexpr char s_<NEW_ACTUATOR>[] = "<newActuator>";
            // actuator types
            static constexpr char s_RELAY[] = "relay";

            // ADD ACTUATOR 3: here connect constexpr with actuator type and ActuatorTypeEnum
            // e.g. {s_<NEW_ACTUATOR>, ActuatorTypeEnum::<NEW_ACTUATOR>}
            // Lookup table mapping actuator type strings to internal enumerator values.
            inline static const std::map<const char*, ActuatorTypeEnum, Comparator> actuatorMap {
                {s_RELAY, ActuatorTypeEnum::RELAY},
            };
        };

        /**
         * @brief Result of creating an actuator.
         * @details Contains either a valid actuator instance or an API error.
         */
        struct ActuatorCreationResult {
            std::unique_ptr<Actuator> actuator = nullptr;
            std::optional<apiPv> error;
        };

        /**
         * @brief Construct manager with a logger.
         *
         * @param logger Shared pointer to the logger instance, default: nullptr.
         */
        explicit ActuatorsManager(const std::shared_ptr<ul::Logger> &logger);

        ~ActuatorsManager() = default;

        /**
         * @brief Perform boot-time handling for all actuators listed in configuration.
         * @details Loads the configuration and calls onBoot() on each actuator.
         */
        void handleActuatorsOnBoot();

        /**
         * @brief Create and initialize an actuator instance for a given actuator ID.
         *
         * @param actuatorId Actuator identifier (from config).
         * @return  Actuator instance or error.
         */
        ActuatorCreationResult handleCreatingActuator(uint8_t actuatorId);

        /**
         * @brief Find JSON configuration entry for a given actuator ID.
         * @param actuatorId Numeric actuator identifier (from config).
         * @return Actuator JSON entry when found, otherwise std::nullopt.
         */
        [[nodiscard]] std::optional<nl::json> getActuatorJsonData(uint8_t actuatorId) const;

        /**
         * @brief Factory method creating a derived actuator object by actuator type name.
         *
         * @param actuatorName Type name.
         * @return New derived actuator instance, or nullptr if type is unknown.
         */
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
