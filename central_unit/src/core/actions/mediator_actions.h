#pragma once
#include "actions.h"

#include <string_view>
#include <memory>

namespace SmartHome {
    /**
     * @brief Mediator module command handlers
     *
     * @details Implements handlers for commands targeted at smart home modules via the Mediator:
     *          - Get: Retrieves module values
     *          - Set: Updates module values
     *          - Execute: Triggers module actions
     *          - Ping: Tests module connectivity and measures response time
     */
    class MediatorActions {
        using cmdMetaPtr = std::shared_ptr<Actions::CommandMetadata>;

    public:
        /**
         * @brief Handle GET command for mediator/module.
         *
         * @details Routes GET requests to mediator or specific module based on parameters.
         *          If parameters contains \c "module_id" object, fetches module info and
         *          forwards request with RF addressing. Otherwise sends directly to mediator.
         *
         * @param commandMetadata Command execution metadata.
         *
         * @return API response with requested value or error.
         *
         * @note Expected params format: {(optional)module_id: uint, type: <string>, (optional)args: [<any>...]}
         */
        static awaitOptApiResponse mediatorGetHandler(const cmdMetaPtr &commandMetadata);


        /**
         * @brief Handle SET command for mediator/module.
         *
         * @details Routes SET requests to mediator or specific module based on parameters.
         *          If parameters contains \c "module_id" object, fetches module info and
         *          forwards request with RF addressing. Otherwise sends directly to mediator.
         *
         * @param commandMetadata Command execution metadata.
         *
         * @return API response with confirmation or error.
         *
         * @note Expected params format: {(optional)module_id: uint, type: <string>, args: [<any>...]}
         */
        static awaitOptApiResponse mediatorSetHandler(const cmdMetaPtr &commandMetadata);

        /**
         * @brief Handle EXECUTE command for mediator/module.
         *
         * @details Routes EXECUTE requests to mediator or specific module based on parameters.
         *          If parameters contains \c "module_id" object, fetches module info and
         *          forwards request with RF addressing. Otherwise sends directly to mediator.
         *
         * @param commandMetadata Command execution metadata.
         *
         * @return API response with execution result or error.
         *
         * @note Expected params format: {(optional)module_id: uint, type: <string>, (optional)args: [<any>...]}
         */
        static awaitOptApiResponse mediatorExecuteHandler(const cmdMetaPtr &commandMetadata);

        /**
         * @brief Handle PING command for module connectivity check.
         *
         * @details Sends PING to specified module and measures round-trip time.
         *          Requires \c "module_id" in parameters to identify target module.
         *
         * @param commandMetadata Command execution metadata.
         *
         * @return API response with round-trip time in milliseconds or error.
         *
         * @note Expected params format: {module_id: uint}
         */
        static awaitOptApiResponse mediatorPingHandler(const cmdMetaPtr &commandMetadata);

        /**
         * @brief Send request to mediator and await response.
         *
         * @details Finds mediator connection, posts request to core worker context,
         *          starts timeout timer, and waits for response asynchronously.
         *          Polls future status while command remains pending.
         *
         * @param request API request to send to mediator.
         * @param commandMetadata Command execution metadata.
         *
         * @return API response from mediator or error.
         *
         * @note Blocks asynchronously until response received or timeout.
         */
        static ba::awaitable<API::ApiResponse> sendRequestToMediator(API::ApiRequest &&request,
                                                                     cmdMetaPtr commandMetadata);

    private:
        struct MediatorRequestParams {
            std::optional<uint> moduleId;
            std::optional<std::string> type;
            std::optional<nlohmann::json> args;
        };


        /**
         * @brief Prepare API request structure for mediator.
         *
         * @details Initializes request with command ID and method, creates params object,
         *          and sets target to module_mediator.
         *
         * @param request API request structure to populate.
         * @param command Source command with ID and method.
         *
         * @return Reference to params JSON object for further population.
         */
        static nlohmann::json &prepareRequestToMediator(API::ApiRequest &request,
                                                        const API::InternalApi::Command &command);

        /**
         * @brief Parse incoming params for mediator forwarding.
         *
         * @details Extracts optional module id, type and args from incoming params and
         *          mirrors applicable values into the prepared mediator params object.
         *
         * @param incomingParams JSON object received with the original command.
         * @param rtmParams Prepared mediator params to be populated.
         *
         * @return Parsed MediatorRequestParams with optional fields set.
         */
        static MediatorRequestParams parseMediatorParams(const nlohmann::json &incomingParams,
                                                         nlohmann::json &rtmParams);

        /**
         * @brief Retrieve RF addressing info for a module from the database.
         *
         * @details Calls \c DatabaseActions::getModuleAddressingInfo and inserts returned
         *          addressing fields into \p preparedParams. On failure, sets \p error.
         *
         * @param preparedParams Prepared mediator params JSON to receive \c "module_info" object.
         * @param moduleId Numeric module id to look up.
         * @param error Output string set on failure with error reason.
         *
         * @return true on success, false on failure.
         */
        static ba::awaitable<bool> getModuleAddressingInfo(nlohmann::json &preparedParams, uint moduleId,
                                                           std::string &error);

        /**
         * @brief Optionally post sensor reading to database when mediator result applies.
         *
         * @details Based on parsed params, checks whether the mediator result should be
         *          recorded as a sensor reading and posts it to the database.
         *
         * @param parsedParams Parsed mediator params.
         * @param result JSON result returned from mediator.
         * @param applicableTypes Set of mediator method types that should trigger posting.
         */
        static void postSensorReadingIfApplicable(const MediatorRequestParams &parsedParams,
                                                  const nlohmann::json &result,
                                                  const std::set<std::string_view> &applicableTypes);

        /**
         * @brief Post error log entry for a module into database.
         *
         * @param moduleId Module identifier for which to log the error.
         * @param result API response containing error details to be recorded.
         */
        static void postErrorLog(uint moduleId, const API::ApiResponse &result);
    };
}
