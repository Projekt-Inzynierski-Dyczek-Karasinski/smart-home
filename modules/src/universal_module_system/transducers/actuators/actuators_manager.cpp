#include "actuators_manager.h"

#include "universal_module_system/data_manager.h"

// ADD ACTUATOR 4: here include actuator class
// e.g. <new_actuator_class>/<new_actuator_class>.h
#include "relay/relay.h"

namespace UniversalModuleSystem::Transducers {
    ActuatorsManager &ActuatorsManager::getInstance(const std::shared_ptr<ul::Logger> &logger) {
        static ActuatorsManager instance(logger);
        return instance;
    }

    ActuatorsManager::ActuatorsManager(const std::shared_ptr<ul::Logger> &logger) :
        mpLogger(logger) {
        if (logger == nullptr) {
            mpLogger = std::make_shared<ul::Logger>();
            mpLogger->error("ActuatorsManager", "ActuatorsManager's constructor didn't get pointer to logger instance.");
        }
        mpLogger->verbose("ActuatorsManager", "ActuatorsManager initialized.");

        handleActuatorsOnBoot();
    }

    apiPv ActuatorsManager::getActuatorState(const uint8_t actuatorId) {
        auto [actuator, error] = handleCreatingActuator(actuatorId);
        if (error.has_value()) {
            return std::move(*error);
        }

        return actuator->getState();
    }

    apiPv ActuatorsManager::toggleActuator(const uint8_t actuatorId) {
        auto [actuator, error] = handleCreatingActuator(actuatorId);
        if (error.has_value()) {
            return std::move(*error);
        }

        return actuator->toggle();
    }

    apiPv ActuatorsManager::actuatorDoOperation(const uint8_t actuatorId, const apiPv& operation) {
        auto [actuator, error] = handleCreatingActuator(actuatorId);
        if (error.has_value()) {
            return std::move(*error);
        }

        return actuator->doOperation(operation);
    }

    std::vector<API::APIParameterVariant> ActuatorsManager::getActuatorsIds() {
        using ET = API::errorTypes;

        const auto &dataManager = DataManager::getInstance();
        nl::json jsonData = dataManager.loadJson(dataManager.s_BASE_CONFIG_PATH);
        nl::json &sensorData = jsonData[ms_ALL_ACTUATORS_DATA][ms_ACTUATORS_ARRAY];
        if (jsonData.empty())
            return std::vector<API::APIParameterVariant>{API::APIParameter(static_cast<uint8_t>(ET::INTERNAL_ERROR), true)};


        std::vector<API::APIParameterVariant> result;
        result.reserve(sensorData.size());
        for (auto &jsonSensor: sensorData) {
            result.emplace_back(API::APIParameter<uint8_t>(jsonSensor[ms_ACTUATOR_DATA][ms_ACTUATOR_ID].get<uint8_t>()));
        }
        return result;
    }

    void ActuatorsManager::handleActuatorsOnBoot() {
        const auto &dataManager = DataManager::getInstance();

        nl::json jsonData = dataManager.loadJson(dataManager.s_BASE_CONFIG_PATH);
        nl::json &actuatorsData = jsonData[ms_ALL_ACTUATORS_DATA];

        for (const auto &actuatorData : actuatorsData[ms_ACTUATORS_ARRAY]) {
            if (
                std::unique_ptr<Actuator> actuator = createActuator(actuatorData[ms_ACTUATOR_TYPE].get<std::string>().c_str());
                actuator != nullptr
            ) {
                actuator->onBoot(actuatorData[ms_ACTUATOR_DATA]);
            }
        }
    }

    ActuatorsManager::ActuatorCreationResult ActuatorsManager::handleCreatingActuator(const uint8_t actuatorId) {
        using ET = API::errorTypes;
        ActuatorCreationResult result;

        const std::optional<nl::json> actuatorJsonDataOpt = getActuatorJsonData(actuatorId);
        if (!actuatorJsonDataOpt.has_value()) {
            mpLogger->errorv("ActuatorsManager", "Not found actuator data, actuator id: ", actuatorId);
            result.error.emplace(apiPv{API::APIParameter(static_cast<uint8_t>(ET::BAD_ARGUMENT),true)});
            return result;
        }

        std::unique_ptr<Actuator> actuator = createActuator(actuatorJsonDataOpt.value()[ms_ACTUATOR_TYPE].get<std::string>().c_str());
        if (actuator == nullptr) {
            mpLogger->error("ActuatorsManager", "Failed to create actuator");
            result.error.emplace(apiPv{API::APIParameter(static_cast<uint8_t>(ET::INTERNAL_ERROR),true)});
            return result;
        }

        if (!actuator->init(actuatorJsonDataOpt.value()[ms_ACTUATOR_DATA])) {
            mpLogger->error("ActuatorsManager", "Failed to init actuator.");
            result.error.emplace(apiPv{API::APIParameter(static_cast<uint8_t>(ET::INTERNAL_ERROR),true)});
            return result;
        }

        result.actuator = std::move(actuator);
        return result;
    }

    std::optional<nl::json> ActuatorsManager::getActuatorJsonData(const uint8_t actuatorId) const {
        const auto &dataManager = DataManager::getInstance();

        nl::json jsonData = dataManager.loadJson(dataManager.s_BASE_CONFIG_PATH);
        nl::json &actuatorsData = jsonData[ms_ALL_ACTUATORS_DATA];

        for (const auto &actuator : actuatorsData[ms_ACTUATORS_ARRAY]) {
            if (actuator[ms_ACTUATOR_DATA][ms_ACTUATOR_ID] == actuatorId) return actuator;
        }

        return std::nullopt;
    }

    std::unique_ptr<Actuator> ActuatorsManager::createActuator(const char *actuatorName) {
        using AT = ActuatorType;
        using ATE = ActuatorType::ActuatorTypeEnum;

        // ADD ACTUATOR 5 (final): here add case for new actuator
        /* e.g.:
            case STE::<NEW_ACTUATOR>:
                return std::make_unique<NewActuatorClass>(mpLogger);
        */
        switch (
            const auto it = AT::actuatorMap.find(actuatorName);
            it != AT::actuatorMap.end() ? it->second : ATE::UNKNOWN
        ) {
            case ATE::RELAY:
                return std::make_unique<Relay>(mpLogger);

            default:
                mpLogger->error("ActuatorsManager", "Got unknown type of actuator.");
                return nullptr;
        }
    }
}
