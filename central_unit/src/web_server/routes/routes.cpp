#include "routes.h"

namespace SmartHomeWebServer {
    crow::response unwrapResponse(const std::string_view jsonRpcResponse) {
        nlohmann::json json;
        try {
            json = nlohmann::json::parse(jsonRpcResponse);
        } catch (const nlohmann::json::exception &e) {
            crow::response res(500, nlohmann::json({{scc::ERROR, "Invalid JSON response from core"}}));
            res.set_header("Content-Type", "application/json");
            return res;
        }

        // Handle error
        if (json.contains(sjr::ERROR) && json[sjr::ERROR].is_object()) {
            const auto &error = json[sjr::ERROR];
            int httpCode = 500;
            if (error.contains(sje::CODE) && error[sje::CODE].is_number_integer()) {
                httpCode = mapErrorToHttpStatus(error[sje::CODE].get<int>());
            }
            crow::response res(httpCode, error.dump());
            res.set_header("Content-Type", "application/json");
            return res;
        }

        // Handle result
        if (json.contains(sjr::RESULT)) {
            const auto &result = json[sjr::RESULT];
            std::string body;

            if (result.is_string()) {
                const auto &resultStr = result.get<std::string>();
                // Try to parse result string as JSON
                if (nlohmann::json::accept(resultStr)) {
                    auto parsed = nlohmann::json::parse(resultStr);
                    if (parsed.is_object()) {
                        body = parsed.dump();
                    } else {
                        body = nlohmann::json({{sjr::RESULT, parsed}}).dump();
                    }
                } else {
                    body = nlohmann::json({{sjr::RESULT, resultStr}}).dump();
                }
            } else {
                // Result is already a JSON type
                if (result.is_object()) {
                    body = result.dump();
                } else {
                    body = nlohmann::json({{sjr::RESULT, result}}).dump();
                }
            }

            crow::response res(200, body);
            res.set_header("Content-Type", "application/json");
            return res;
        }

        crow::response res(500, nlohmann::json({{scc::ERROR, "Empty response"}}));
        res.set_header("Content-Type", "application/json");
        return res;
    }

    crow::response forwardToCore(ApiClient &apiClient, const std::string_view method, const nlohmann::json &params) {
        sa::ApiRequest request;
        request.id = sa::getNextApiId();
        request.method = method;
        if (!params.empty()) {
            request.params = params;
        }

        try {
            auto response = apiClient.sendRequest(request).get();
            return unwrapResponse(response);
        } catch (const std::exception &e) {
            crow::response res(504, nlohmann::json({{scc::ERROR, e.what()}}).dump());
            res.set_header("Content-Type", "application/json");
            return res;
        }
    }

    crow::response coreGet(ApiClient &apiClient, std::string_view type, nlohmann::json params) {
        params[sjp::TYPE] = type;
        return forwardToCore(apiClient,
                             sa::getTargetMethodString(sc::Targets::CORE, sc::Methods::GET),
                             params);
    }

    crow::response coreSet(ApiClient &apiClient, std::string_view type, nlohmann::json params) {
        params[sjp::TYPE] = type;
        return forwardToCore(apiClient,
                             sa::getTargetMethodString(sc::Targets::CORE, sc::Methods::SET),
                             params);
    }

    crow::response coreDelete(ApiClient &apiClient, std::string_view type, nlohmann::json params) {
        params[sjp::TYPE] = type;
        return forwardToCore(apiClient,
                             sa::getTargetMethodString(sc::Targets::CORE, sc::Methods::DELETE),
                             params);
    }
}
