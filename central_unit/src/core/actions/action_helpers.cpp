#include "action_helpers.h"
#include "actions.h"
#include "mediator_actions.h"
#include "database_actions.h"
#include "../core.h"

#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>

namespace SmartHome {
    ActionHelpers::CommandMetadata::CommandMetadata(API::InternalApi::Command command,
                                                    std::shared_ptr<ba::steady_timer> commandTimeoutTimer,
                                                    const apiId_t requestId)
        : command(std::move(command)),
          commandTimeoutTimer(std::move(commandTimeoutTimer)),
          requestId(requestId) {
        isNotification = this->command.isNotification;
    }

    bool ActionHelpers::CommandMetadata::cancel() {
        if (const auto timer = commandTimeoutTimer.exchange(nullptr)) timer->cancel();
        auto expected = State::PENDING;
        if (state.compare_exchange_strong(expected, State::CANCELLED)) return true;
        return false;
    }

    bool ActionHelpers::CommandMetadata::isPending() const {
        return state.load(std::memory_order::relaxed) == State::PENDING;
    }

    ValidationResult<jsonPtr> ActionHelpers::requireParams(const CommandMetadata &cmdMetadata) {
        if (!cmdMetadata.command.params.has_value() ||
            !cmdMetadata.command.params.value().is_object()) {
            return std::unexpected(API::ApiError(
                API::ErrorCodes::INVALID_PARAMS,
                errorCodeToString(API::ErrorCodes::INVALID_PARAMS),
                "Command requires parameters object in params field"
            ));
        }
        return std::make_shared<nlohmann::json>(cmdMetadata.command.params.value());
    }

    ValidationResult<std::string> ActionHelpers::requireType(const nlohmann::json &params) {
        if (!params.contains(jp::TYPE) ||
            !params.at(jp::TYPE).is_string()) {
            return std::unexpected(API::ApiError(
                API::ErrorCodes::INVALID_PARAMS,
                errorCodeToString(API::ErrorCodes::INVALID_PARAMS),
                "Type of command is required and must be string in 'type' parameter"
            ));
        }
        return params.at(jp::TYPE).get<std::string>();
    }


    ValidationResult<uint> ActionHelpers::requireModuleId(const nlohmann::json &params) {
        if (!params.contains(jp::MODULE_ID) ||
            !params.at(jp::MODULE_ID).is_number_integer()) {
            return std::unexpected(API::ApiError(
                API::ErrorCodes::INVALID_PARAMS,
                errorCodeToString(API::ErrorCodes::INVALID_PARAMS),
                "Module ID is required and must be an unsigned integer in 'module_id' parameter"));
        }

        const auto value = params.at(jp::MODULE_ID).get<int>();
        return value >= 0 ? static_cast<uint>(value) : 0;
    }

    ValidationResult<uint> ActionHelpers::requireLimitArg(const nlohmann::json &params) {
        if (!params.contains(jp::ARGS) ||
            !params.at(jp::ARGS).is_array() ||
            params.at(jp::ARGS).empty() ||
            !params.at(jp::ARGS).back().is_number_integer()) {
            return std::unexpected(API::ApiError(
                API::ErrorCodes::INVALID_PARAMS,
                errorCodeToString(API::ErrorCodes::INVALID_PARAMS),
                "Requires limit argument as unsigned integer in last element of 'args' array parameter"));
        }

        const auto value = params.at(jp::ARGS).back().get<int>();
        return value >= 0 ? static_cast<uint>(value) : 0;
    }

    ValidationResult<uint> ActionHelpers::requireDeviceLogicIdArg(const nlohmann::json &params) {
        if (!params.contains(jp::ARGS) ||
            !params.at(jp::ARGS).is_array() ||
            params.at(jp::ARGS).empty() ||
            !params.at(jp::ARGS).front().is_number_integer()) {
            return std::unexpected(API::ApiError(
                API::ErrorCodes::INVALID_PARAMS,
                errorCodeToString(API::ErrorCodes::INVALID_PARAMS),
                "Requires device logic ID argument as unsigned integer in first element of 'args' array parameter"));
        }

        const auto value = params.at(jp::ARGS).front().get<int>();
        return value >= 0 ? static_cast<uint>(value) : 0;
    }

    ValidationResult<uint> ActionHelpers::resolveDeviceId(const nlohmann::json &params) {
        if (!params.contains(jp::ARGS) ||
            !params.at(jp::ARGS).is_array() ||
            params.at(jp::ARGS).empty() ||
            !params.at(jp::ARGS).front().is_number_integer()) {
            return std::unexpected(API::ApiError(
                API::ErrorCodes::INVALID_PARAMS,
                errorCodeToString(API::ErrorCodes::INVALID_PARAMS),
                "Requires device ID as unsigned integer in first element of 'args' array parameter, "
                "or module ID in 'module_id' parameter with device logic ID as first element of 'args' array"));
        }
        int value = 0;

        if (params.contains(jp::MODULE_ID) &&
            params.at(jp::MODULE_ID).is_number_integer()) {
            value = params.at(jp::MODULE_ID).get<int>();
            const auto moduleId = value >= 0 ? static_cast<uint>(value) : 0;

            value = params.at(jp::ARGS).front().get<int>();
            const auto logicId = value >= 0 ? static_cast<uint>(value) : 0;
            const auto deviceIdOpt = Core::Instance().configCache().findDeviceId(moduleId, logicId);

            if (!deviceIdOpt.has_value()) {
                return std::unexpected(API::ApiError(
                    API::ErrorCodes::NOT_FOUND,
                    errorCodeToString(API::ErrorCodes::NOT_FOUND),
                    "Device logic_id [" + std::to_string(logicId) +
                    "] not found for module [" + std::to_string(moduleId) + "]"));
            }
            return deviceIdOpt.value();
        }

        value = params.at(jp::ARGS).front().get<int>();
        return value >= 0 ? static_cast<uint>(value) : 0;
    }

    ValidationResult<std::string> ActionHelpers::requireMode(const nlohmann::json &params) {
        if (!params.contains(jp::MODE) ||
            !params.at(jp::MODE).is_string()) {
            return std::unexpected(API::ApiError(
                API::ErrorCodes::INVALID_PARAMS,
                errorCodeToString(API::ErrorCodes::INVALID_PARAMS),
                "Set mode is required as string in 'mode' parameter."));
        }

        auto mode = params.at(jp::MODE).get<std::string>();

        if (!co::MODES.contains(mode)) {
            return std::unexpected(API::ApiError(
                API::ErrorCodes::INVALID_PARAMS,
                errorCodeToString(API::ErrorCodes::INVALID_PARAMS),
                "Set mode must be one of values defined in constants.h."));
        }

        return mode;
    }

    ValidationResult<std::string> ActionHelpers::requirePath(const nlohmann::json &params) {
        if (!params.contains(jp::PATH) ||
            !params.at(jp::PATH).is_string()) {
            return std::unexpected(API::ApiError(
                API::ErrorCodes::INVALID_PARAMS,
                errorCodeToString(API::ErrorCodes::INVALID_PARAMS),
                "Set path is required as string in 'path' parameter. Path must be in dot separated format"));
        }

        auto path = params.at(jp::PATH).get<std::string>();

        std::vector<std::string_view> keys = {};
        boost::split(keys, path, boost::is_any_of("."));

        if (keys.empty()) {
            return std::unexpected(API::ApiError(
                API::ErrorCodes::INVALID_PARAMS,
                errorCodeToString(API::ErrorCodes::INVALID_PARAMS),
                "Set path must be a non empty, when more then one key is present keys must be dot separated."));
        }

        // Check base key
        if (!cdi::ALL_IDENTIFIERS.contains(keys.front())) {
            return std::unexpected(API::ApiError(
                API::ErrorCodes::INVALID_PARAMS,
                errorCodeToString(API::ErrorCodes::INVALID_PARAMS),
                "First key in path must be an database identifier defined in constants.h"));
        }

        // TODO consider adding deeply nested JSON values validation
        // Validate only the top-level config key (e.g. "schedule" in "config.schedule.enabled").
        // Deeper nested keys are not checked against constants to avoid complexity with deeply nested JSON structures
        // like events, schedule actions, etc.
        if (keys.size() > 1 &&
            !cdck::CONFIG_KEYS.contains(keys[1]) &&
            !cmck::CONFIG_KEYS.contains(keys[1])) {
            return std::unexpected(API::ApiError(
                API::ErrorCodes::INVALID_PARAMS,
                errorCodeToString(API::ErrorCodes::INVALID_PARAMS),
                "Nested keys are used in JSON config column."
                " First nested key (second in path) must be defined in constants.h"));
        }

        return path;
    }

    ValidationResult<nlohmann::json> ActionHelpers::requireValue(const nlohmann::json &params) {
        if (!params.contains(jp::VALUE)) {
            return std::unexpected(API::ApiError(
                API::ErrorCodes::INVALID_PARAMS,
                errorCodeToString(API::ErrorCodes::INVALID_PARAMS),
                "Set value is required as primitive/object in 'value' parameter."));
        }

        return params.at(jp::VALUE);
    }

    ba::awaitable<ValidationResult<nlohmann::json> > ActionHelpers::queryDatabase(
        const std::string_view method,
        nlohmann::json &&dbQuery) {
        Core::Instance().mpLogger->debugf("[CORE_ACTIONS] [QUERY_DB] called");

        API::ApiRequest request;
        const API::InternalApi::Target target(API::InternalApi::TargetTypes::DATABASE);
        request.method = API::getTargetMethodString(target.to_string(), method);
        request.params = dbQuery;
        request.id = Actions::getNextId();
        auto response = co_await DatabaseActions::sendRequestToDbService(std::move(request));

        // Handle db response
        if (response.error.has_value()) {
            co_return std::unexpected(API::ApiError(
                API::ErrorCodes::INTERNAL_ERROR,
                errorCodeToString(API::ErrorCodes::INTERNAL_ERROR),
                "Database query failed: " + response.error.value().data));
        }

        if (!response.result.has_value() || !nlohmann::json::accept(response.result.value())) {
            co_return std::unexpected(API::ApiError(
                API::ErrorCodes::INTERNAL_ERROR,
                errorCodeToString(API::ErrorCodes::INTERNAL_ERROR),
                "Invalid database response"));
        }

        auto parsedResult = nlohmann::json::parse(response.result.value());

        if (method == Constants::Methods::GET) {
            if (!parsedResult.contains(jp::ROWS) || !parsedResult[jp::ROWS].is_array()) {
                co_return std::unexpected(API::ApiError(
                    API::ErrorCodes::INTERNAL_ERROR,
                    errorCodeToString(API::ErrorCodes::INTERNAL_ERROR),
                    "Database get response missing rows array"));
            }
        } else if (method == Constants::Methods::SET) {
            if (!parsedResult.contains(jp::AFFECTED_ROWS) || !parsedResult[jp::AFFECTED_ROWS].is_number()) {
                co_return std::unexpected(API::ApiError(
                    API::ErrorCodes::INTERNAL_ERROR,
                    errorCodeToString(API::ErrorCodes::INTERNAL_ERROR),
                    "Database set response missing affected rows"));
            }
        }

        co_return parsedResult;
    }

    awaitOptApiResponse ActionHelpers::delegateToMediator(const cmdMetaPtr pCommandMetadata, jsonPtr) {
        API::ApiResponse handlerResult;
        handlerResult.id = pCommandMetadata->command.commandId;

        // Delegate command to mediator actions for resolving (for commands that are targeted at module or mediator)
        co_return co_await MediatorActions::mediatorSetHandler(pCommandMetadata);
    }
}
