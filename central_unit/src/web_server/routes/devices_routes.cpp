#include "devices_routes.h"

namespace SmartHomeWebServer {
    void registerDevicesRoutes(crow::App<crow::CORSHandler> &app, ApiClient &apiClient) {
        // GET /api/devices — all devices
        CROW_ROUTE(app, "/api/devices")([&apiClient] {
            return coreGet(apiClient, scc::DEVICES);
        });

        // GET /api/devices/<id> — single device by device id
        CROW_ROUTE(app, "/api/devices/<uint>")([&apiClient](unsigned int sensorId) {
            nlohmann::json params;
            params[sjp::ARGS.data()] = nlohmann::json::array({sensorId});
            return coreGet(apiClient, scc::DEVICE, params);
        });

        // GET /api/devices/<id>/readings?limit=N — readings by device id
        CROW_ROUTE(app, "/api/devices/<uint>/readings")(
            [&apiClient](const crow::request &req, unsigned int sensorId) {
                int limit = 10;
                if (const auto p = req.url_params.get(sjp::LIMIT.data()))
                    limit = std::stoi(p);

                nlohmann::json params;
                params[sjp::ARGS] = nlohmann::json::array({sensorId, limit});

                if (const auto p = req.url_params.get(sjp::FROM.data()))
                    params[sjp::FROM.data()] = std::string(p);
                if (const auto p = req.url_params.get(sjp::TO.data()))
                    params[sjp::TO.data()] = std::string(p);

                return coreGet(apiClient, scc::DEVICE_READINGS, params);
            });

        // GET /api/devices/<id>/value?force=bool — live device value
        CROW_ROUTE(app, "/api/devices/<uint>/value")(
            [&apiClient](const crow::request &req, unsigned int sensorId) {
                nlohmann::json params;
                params[sjp::ARGS.data()] = nlohmann::json::array({sensorId});

                constexpr auto type = scc::DEVICE_VALUE;
                if (const auto forceParam = req.url_params.get(sjp::FORCE.data())) {
                    if (std::string_view(forceParam) == "true") {
                        params[sjp::FORCE.data()] = true;
                    }
                }
                return coreGet(apiClient, type, params);
            });

        // POST /api/modules/<id>/actuators/toggle
        // Body: {"logic_id": N}
        CROW_ROUTE(app, "/api/modules/<uint>/actuators/toggle").methods("POST"_method)(
            [&apiClient](const crow::request &req, unsigned int moduleId) {
                nlohmann::json body;
                try {
                    body = nlohmann::json::parse(req.body);
                } catch (...) {
                    crow::response res(400, R"({"error":"Invalid JSON body"})");
                    res.set_header("Content-Type", "application/json");
                    return res;
                }

                if (!body.contains("logic_id") || !body["logic_id"].is_number_integer()) {
                    crow::response res(400, R"({"error":"'logic_id' is required and must be an integer"})");
                    res.set_header("Content-Type", "application/json");
                    return res;
                }

                nlohmann::json params;
                params[sjp::MODULE_ID] = moduleId;
                params[sjp::ARGS] = nlohmann::json::array({body["logic_id"].get<int>()});

                return coreSet(apiClient, scmt::TOGGLE_ACTUATOR, params);
            });

        // POST /api/modules/<id>/actuators/value
        // Body: {"logic_id": N, "value": V}
        CROW_ROUTE(app, "/api/modules/<uint>/actuators/value").methods("POST"_method)(
            [&apiClient](const crow::request &req, unsigned int moduleId) {
                nlohmann::json body;
                try {
                    body = nlohmann::json::parse(req.body);
                } catch (...) {
                    crow::response res(400, R"({"error":"Invalid JSON body"})");
                    res.set_header("Content-Type", "application/json");
                    return res;
                }

                if (!body.contains("logic_id") || !body["logic_id"].is_number_integer()) {
                    crow::response res(400, R"({"error":"'logic_id' is required and must be an integer"})");
                    res.set_header("Content-Type", "application/json");
                    return res;
                }

                if (!body.contains("value")) {
                    crow::response res(400, R"({"error":"'value' is required"})");
                    res.set_header("Content-Type", "application/json");
                    return res;
                }

                nlohmann::json params;
                params[sjp::MODULE_ID] = moduleId;
                params[sjp::ARGS] = nlohmann::json::array({body["logic_id"].get<int>(), body["value"]});

                return coreSet(apiClient, scmt::SET_ACTUATOR_VALUE, params);

            });
    }
}
