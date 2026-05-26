#include "devices_routes.h"
#include "route_helpers.h"

namespace SmartHomeWebServer {
    using namespace std::string_literals;

    void registerDevicesRoutes(crow::App<crow::CORSHandler> &app, ApiClient &apiClient) {
        // GET /api/devices — all devices
        CROW_ROUTE(app, "/api/devices")([&apiClient] {
            return coreGet(apiClient, scct::DEVICES);
        });

        // GET /api/devices/<id> — single device by device id
        CROW_ROUTE(app, "/api/devices/<uint>")([&apiClient](unsigned int sensorId) {
            nlohmann::json params;
            params[sjp::ARGS.data()] = nlohmann::json::array({sensorId});
            return coreGet(apiClient, scct::DEVICE, params);
        });

        // GET /api/devices/<id>/readings?limit=<int> — readings by device id
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

                return coreGet(apiClient, scct::DEVICE_READINGS, params);
            });

        // GET /api/devices/<id>/value?force=<bool> — live device value
        CROW_ROUTE(app, "/api/devices/<uint>/value")(
            [&apiClient](const crow::request &req, unsigned int sensorId) {
                nlohmann::json params;
                params[sjp::ARGS.data()] = nlohmann::json::array({sensorId});

                if (const auto p = req.url_params.get(sjp::FORCE.data()))
                    if (std::string_view(p) == "true")
                        params[sjp::FORCE] = true;

                return coreGet(apiClient, scct::DEVICE_VALUE, params);
            });

        // POST /api/devices — create new device
        // Body: {"values": {...}, "returning"?: "*"|["..."]}
        CROW_ROUTE(app, "/api/devices").methods("POST"_method)(
            [&apiClient](const crow::request &req) {
                auto body = RouteHelpers::requireBody(req);
                if (!body) return std::move(body.error());

                auto values = RouteHelpers::requireObjectField(*body, sjp::VALUES);
                if (!values) return std::move(values.error());

                nlohmann::json params;
                params[sjp::VALUES] = *values;
                if (const auto ret = RouteHelpers::optionalField(*body, sjp::RETURNING))
                    params[sjp::RETURNING] = *ret;

                return coreSet(apiClient, scct::DEVICE, params);
            });

        // PATCH /api/devices/<id> — update device field
        // Body: {"mode": "overwrite"|"append", "path": <str>, "value": <any>, "returning"?: "*"|["..."]}
        CROW_ROUTE(app, "/api/devices/<uint>").methods("PATCH"_method)(
            [&apiClient](const crow::request &req, unsigned int deviceId) {
                auto body = RouteHelpers::requireBody(req);
                if (!body) return std::move(body.error());

                auto mode = RouteHelpers::requireStringField(*body, sjp::MODE);
                if (!mode) return std::move(mode.error());

                auto path = RouteHelpers::requireStringField(*body, sjp::PATH);
                if (!path) return std::move(path.error());

                auto value = RouteHelpers::requireField(*body, sjp::VALUE);
                if (!value) return std::move(value.error());

                nlohmann::json params;
                params[sjp::ARGS] = nlohmann::json::array({deviceId});
                params[sjp::MODE] = *mode;
                params[sjp::PATH] = *path;
                params[sjp::VALUE] = *value;
                if (const auto ret = RouteHelpers::optionalField(*body, sjp::RETURNING))
                    params[sjp::RETURNING] = *ret;

                return coreSet(apiClient, scct::DEVICE, params);
            });

        // DELETE /api/devices/<id> — delete device record or JSONB value
        // Body?: {"path": <str>"}
        CROW_ROUTE(app, "/api/devices/<uint>").methods("DELETE"_method)(
            [&apiClient](const crow::request &req, unsigned int deviceId) {
                nlohmann::json params;
                params[sjp::ARGS] = nlohmann::json::array({deviceId});

                if (!req.body.empty()) {
                    auto body = RouteHelpers::requireBody(req);
                    if (!body) return std::move(body.error());
                    if (const auto path = RouteHelpers::optionalField(*body, sjp::PATH))
                        params[sjp::PATH] = *path;
                }

                return coreDelete(apiClient, scct::DEVICE, params);
            });

        // POST /api/modules/<id>/actuators/toggle
        // Body: {"logic_id": <int?}
        CROW_ROUTE(app, "/api/modules/<uint>/actuators/toggle").methods("POST"_method)(
            [&apiClient](const crow::request &req, unsigned int moduleId) {
                auto body = RouteHelpers::requireBody(req);
                if (!body) return std::move(body.error());

                auto logicId = RouteHelpers::requireField(*body, scdi::LOGIC_ID);
                if (!logicId) return std::move(logicId.error());
                if (!logicId->is_number_integer())
                    return RouteHelpers::badRequest("'"s + scdi::LOGIC_ID.data() + "' must be an integer");

                nlohmann::json params;
                params[sjp::MODULE_ID] = moduleId;
                params[sjp::ARGS] = nlohmann::json::array({logicId->get<int>()});

                return coreSet(apiClient, scmt::TOGGLE_ACTUATOR, params);
            });

        // POST /api/modules/<id>/actuators/value
        // Body: {"logic_id": <int>, "value": <any>}
        CROW_ROUTE(app, "/api/modules/<uint>/actuators/value").methods("POST"_method)(
            [&apiClient](const crow::request &req, unsigned int moduleId) {
                auto body = RouteHelpers::requireBody(req);
                if (!body) return std::move(body.error());

                auto logicId = RouteHelpers::requireField(*body, scdi::LOGIC_ID);
                if (!logicId) return std::move(logicId.error());
                if (!logicId->is_number_integer())
                    return RouteHelpers::badRequest("'"s + scdi::LOGIC_ID.data() + "' must be an integer");

                auto value = RouteHelpers::requireField(*body, sjp::VALUE);
                if (!value) return std::move(value.error());

                nlohmann::json params;
                params[sjp::MODULE_ID] = moduleId;
                params[sjp::ARGS] = nlohmann::json::array({logicId->get<int>(), *value});

                return coreSet(apiClient, scmt::SET_ACTUATOR_VALUE, params);
            });
    }
}
