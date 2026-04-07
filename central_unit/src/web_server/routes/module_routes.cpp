#include "module_routes.h"
#include "route_helpers.h"

void SmartHomeWebServer::registerModuleRoutes(crow::App<crow::CORSHandler> &app, ApiClient &apiClient) {
    // GET /api/modules — all modules
    CROW_ROUTE(app, "/api/modules")([&apiClient] {
        return coreGet(apiClient, scc::MODULES);
    });

    // GET /api/modules/<id> — single module
    CROW_ROUTE(app, "/api/modules/<uint>")([&apiClient](unsigned int moduleId) {
        nlohmann::json params;
        params[sjp::MODULE_ID] = moduleId;
        return coreGet(apiClient, scc::MODULE, params);
    });

    // GET /api/modules/<id>/devices — devices for module
    CROW_ROUTE(app, "/api/modules/<uint>/devices")([&apiClient](unsigned int moduleId) {
        nlohmann::json params;
        params[sjp::MODULE_ID] = moduleId;
        return coreGet(apiClient, scc::MODULE_DEVICES, params);
    });

    // GET /api/modules/<mid>/devices/<lid> — device by module + logic id
    CROW_ROUTE(app, "/api/modules/<uint>/devices/<uint>")(
        [&apiClient](unsigned int moduleId, unsigned int logicId) {
            nlohmann::json params;
            params[sjp::MODULE_ID] = moduleId;
            params[sjp::ARGS] = nlohmann::json::array({logicId});
            return coreGet(apiClient, scc::DEVICE, params);
        });

    // GET /api/modules/<mid>/sensors/<lid>/readings?limit=N — readings by module + logic id
    CROW_ROUTE(app, "/api/modules/<uint>/devices/<uint>/readings")(
        [&apiClient](const crow::request &req, unsigned int moduleId, unsigned int logicId) {
            int limit = 1;
            if (const auto p = req.url_params.get(sjp::LIMIT.data()))
                limit = std::stoi(p);

            nlohmann::json params;
            params[sjp::MODULE_ID] = moduleId;
            params[sjp::ARGS] = nlohmann::json::array({logicId, limit});

            if (const auto p = req.url_params.get(sjp::FROM.data()))
                params[sjp::FROM] = std::string(p);
            if (const auto p = req.url_params.get(sjp::TO.data()))
                params[sjp::TO] = std::string(p);

            return coreGet(apiClient, scc::DEVICE_READINGS, params);
        });

    // GET /api/modules/<mid>/sensors/<lid>/value?force=bool — current value by module + logic id
    CROW_ROUTE(app, "/api/modules/<uint>/devices/<uint>/value")(
        [&apiClient](const crow::request &req, unsigned int moduleId, unsigned int logicId) {
            nlohmann::json params;
            params[sjp::MODULE_ID] = moduleId;
            params[sjp::ARGS] = nlohmann::json::array({logicId});

            std::string_view type = scmt::SENSOR_VALUE;
            if (const auto p = req.url_params.get(sjp::FORCE.data())) {
                if (std::string_view(p) == "true") {
                    type = scmt::FORCE_READ_SENSOR_VALUE;
                    params[sjp::FORCE] = true;
                }
            }
            return coreGet(apiClient, type, params);
        });

    // GET /api/modules/<id>/logs?limit=N — module logs from database
    CROW_ROUTE(app, "/api/modules/<uint>/logs")(
        [&apiClient](const crow::request &req, unsigned int moduleId) {
            int limit = 1;
            if (const auto p = req.url_params.get(sjp::LIMIT.data()))
                limit = std::stoi(p);

            nlohmann::json params;
            params[sjp::MODULE_ID] = moduleId;
            params[sjp::ARGS] = nlohmann::json::array({limit});
            return coreGet(apiClient, scc::LOGS, params);
        });

    // GET /api/modules/<id>/battery — battery level via mediator
    CROW_ROUTE(app, "/api/modules/<uint>/battery")([&apiClient](unsigned int moduleId) {
        nlohmann::json params;
        params[sjp::MODULE_ID] = moduleId;
        return coreGet(apiClient, scmt::BATTERY_LEVEL, params);
    });

    // GET /api/modules/<id>/device-list — device list via mediator
    CROW_ROUTE(app, "/api/modules/<uint>/device-list")([&apiClient](unsigned int moduleId) {
        nlohmann::json params;
        params[sjp::MODULE_ID] = moduleId;
        return coreGet(apiClient, scmt::DEVICE_LIST, params);
    });

    // POST /api/modules — create new module
    // Body: {"values": {...}, "returning"?: "*"|["..."]}
    CROW_ROUTE(app, "/api/modules").methods("POST"_method)(
        [&apiClient](const crow::request &req) {
            auto body = RouteHelpers::requireBody(req);
            if (!body) return std::move(body.error());

            auto values = RouteHelpers::requireObjectField(*body, sjp::VALUES);
            if (!values) return std::move(values.error());

            nlohmann::json params;
            params[sjp::VALUES] = *values;
            if (const auto ret = RouteHelpers::optionalField(*body, sjp::RETURNING))
                params[sjp::RETURNING] = *ret;

            return coreSet(apiClient, scc::MODULE, params);
        });

    // PATCH /api/modules/<id> — update module field
    // Body: {"mode": "overwrite"|"append", "path": <str>, "value": <any>, "returning"?: "*"|["..."]}
    CROW_ROUTE(app, "/api/modules/<uint>").methods("PATCH"_method)(
        [&apiClient](const crow::request &req, unsigned int moduleId) {
            auto body = RouteHelpers::requireBody(req);
            if (!body) return std::move(body.error());

            auto mode = RouteHelpers::requireStringField(*body, sjp::MODE);
            if (!mode) return std::move(mode.error());

            auto path = RouteHelpers::requireStringField(*body, sjp::PATH);
            if (!path) return std::move(path.error());

            auto value = RouteHelpers::requireField(*body, sjp::VALUE);
            if (!value) return std::move(value.error());

            nlohmann::json params;
            params[sjp::MODULE_ID] = moduleId;
            params[sjp::MODE] = *mode;
            params[sjp::PATH] = *path;
            params[sjp::VALUE] = *value;
            if (const auto ret = RouteHelpers::optionalField(*body, sjp::RETURNING))
                params[sjp::RETURNING] = *ret;

            return coreSet(apiClient, scc::MODULE, params);
        });

    // DELETE /api/modules/<id> — delete module record or column/JSONB value
    // Body?: {"path": <str>}
    CROW_ROUTE(app, "/api/modules/<uint>").methods("DELETE"_method)(
        [&apiClient](const crow::request &req, unsigned int moduleId) {
            nlohmann::json params;
            params[sjp::MODULE_ID] = moduleId;

            if (!req.body.empty()) {
                auto body = RouteHelpers::requireBody(req);
                if (!body) return std::move(body.error());
                if (const auto path = RouteHelpers::optionalField(*body, sjp::PATH))
                    params[sjp::PATH] = *path;
            }

            return coreDelete(apiClient, scc::MODULE, params);
        });
}
