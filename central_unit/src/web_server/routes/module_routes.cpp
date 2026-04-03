#include "module_routes.h"

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

    // TODO !pr change sensors -> devices
    // GET /api/modules/<id>/sensors — sensors for module
    CROW_ROUTE(app, "/api/modules/<uint>/sensors")([&apiClient](unsigned int moduleId) {
        nlohmann::json params;
        params[sjp::MODULE_ID] = moduleId;
        return coreGet(apiClient, scc::MODULE_DEVICES, params);
    });

    // GET /api/modules/<mid>/sensors/<lid> — sensor by module + logic id
    CROW_ROUTE(app, "/api/modules/<uint>/sensors/<uint>")(
        [&apiClient](unsigned int moduleId, unsigned int logicId) {
            nlohmann::json params;
            params[sjp::MODULE_ID] = moduleId;
            params[sjp::ARGS] = nlohmann::json::array({logicId});
            return coreGet(apiClient, scc::DEVICE, params);
        });

    // GET /api/modules/<mid>/sensors/<lid>/readings?limit=N — readings by module + logic id
    CROW_ROUTE(app, "/api/modules/<uint>/sensors/<uint>/readings")(
        [&apiClient](const crow::request &req, unsigned int moduleId, unsigned int logicId) {
            int limit = 1;
            if (const auto limitParam = req.url_params.get(sjp::LIMIT.data()))
                limit = std::stoi(limitParam);

            nlohmann::json params;
            params[sjp::MODULE_ID] = moduleId;
            params[sjp::ARGS] = nlohmann::json::array({logicId, limit});

            if (const auto p = req.url_params.get(sjp::FROM.data()))
                params[sjp::FROM.data()] = std::string(p);
            if (const auto p = req.url_params.get(sjp::TO.data()))
                params[sjp::TO.data()] = std::string(p);

            return coreGet(apiClient, scc::DEVICE_READINGS, params);
        });

    // GET /api/modules/<mid>/sensors/<lid>/value?force=bool — current value by module + logic id
    CROW_ROUTE(app, "/api/modules/<uint>/sensors/<uint>/value")(
        [&apiClient](const crow::request &req, unsigned int moduleId, unsigned int logicId) {
            nlohmann::json params;
            params[sjp::MODULE_ID] = moduleId;
            params[sjp::ARGS] = nlohmann::json::array({logicId});

            std::string_view type = scmt::SENSOR_VALUE;
            if (const auto forceParam = req.url_params.get(sjp::FORCE.data())) {
                if (std::string_view(forceParam) == "true") {
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
            if (const auto limitParam = req.url_params.get(sjp::LIMIT.data()))
                limit = std::stoi(limitParam);

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

    // GET /api/modules/<id>/sensor-list — sensor list via mediator
    CROW_ROUTE(app, "/api/modules/<uint>/sensor-list")([&apiClient](unsigned int moduleId) {
        nlohmann::json params;
        params[sjp::MODULE_ID] = moduleId;
        return coreGet(apiClient, scmt::DEVICE_LIST, params);
    });
}
