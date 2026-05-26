#pragma once
#include "api.h"
#include "../api_client.h"

#include <utility>
#include <expected>

#include <crow.h>
#include <crow/middlewares/cors.h>

namespace SmartHomeWebServer {
    namespace sa = SmartHome::API;
    namespace sj = SmartHome::JsonRpcStrings;
    namespace sjr = SmartHome::JsonRpcStrings::ResponseKeys;
    namespace sje = SmartHome::JsonRpcStrings::ErrorKeys;
    namespace sjp = SmartHome::JsonRpcStrings::ParamsKeys;

    namespace sc = SmartHome::Constants;
    namespace scc = SmartHome::Constants::Common;
    namespace scct = SmartHome::Constants::CoreTypes;
    namespace scmt = SmartHome::Constants::MediatorTypes;
    namespace scdi= SmartHome::Constants::DatabaseIdentifiers;

    /**
     * @brief Map JSON-RPC error code to HTTP status code.
     */
    inline int mapErrorToHttpStatus(int jsonRpcCode) {
        switch (static_cast<sa::ErrorCodes>(jsonRpcCode)) {
            case sa::ErrorCodes::NOT_FOUND:
            case sa::ErrorCodes::METHOD_NOT_FOUND:
                return 404;
            case sa::ErrorCodes::INVALID_PARAMS:
            case sa::ErrorCodes::PARSE_ERROR:
            case sa::ErrorCodes::INVALID_REQUEST:
                return 400;
            case sa::ErrorCodes::NOT_IMPLEMENTED:
                return 501;
            default:
                return 500;
        }
    }

    /**
     * @brief Unwrap JSON-RPC response to REST response.
     *
     * @details Result handling:
     *          - JSON object string: parsed and returned flat
     *          - Other JSON string: wrapped in {"result": parsed}
     *          - Plain string: wrapped in {"result": "string"}
     *
     * @details Error: returned as unwrapped error object with mapped HTTP code.
     */
    crow::response unwrapResponse(std::string_view jsonRpcResponse);

    /**
     * @brief Build JSON-RPC request, send via ApiClient, and return unwrapped REST response.
     *
     * @param apiClient ApiClient connected to core.
     * @param method JSON-RPC method string (e.g. "core.get").
     * @param params JSON params object.
     *
     * @return \c crow::response with unwrapped result or error.
     */
    crow::response forwardToCore(ApiClient &apiClient,
                                 std::string_view method,
                                 const nlohmann::json &params = nlohmann::json::object());

    /**
     * @brief Helper for GET requests to core with simplified params construction.
     *
     * @param apiClient ApiClient connected to core.
     * @param type Core type string (e.g. "devices", "modules").
     * @param params Optional JSON params object.
     *
     * @return \c crow::response with unwrapped result or error.
     */
    crow::response coreGet(ApiClient &apiClient,
                           std::string_view type,
                           nlohmann::json params = nlohmann::json::object());

    /**
     * @brief Helper for SET requests to core with simplified params construction.
     *
     * @param apiClient ApiClient connected to core.
     * @param type Core type string (e.g. "devices", "modules").
     * @param params Optional JSON params object.
     *
     * @return \c crow::response with unwrapped result or error.
     */
    crow::response coreSet(ApiClient &apiClient,
                           std::string_view type,
                           nlohmann::json params = nlohmann::json::object());

    /**
     * @brief Helper for DELETE requests to core with simplified params construction.
     *
     * @param apiClient ApiClient connected to core.
     * @param type Core type string (e.g. "devices", "modules").
     * @param params Optional JSON params object.
     *
     * @return \c crow::response with unwrapped result or error.
     */
    crow::response coreDelete(ApiClient &apiClient,
                          std::string_view type,
                          nlohmann::json params = nlohmann::json::object());
}
