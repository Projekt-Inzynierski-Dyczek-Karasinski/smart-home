#include "sensor_routes.h"

namespace SmartHomeWebServer {
    void registerSensorRoutes(crow::App<crow::CORSHandler> &app,
                              ApiClient &apiClient) {
        // GET /api/sensors — all sensors
        CROW_ROUTE(app, "/api/sensors")([&apiClient] {
            return coreGet(apiClient, scc::SENSORS);
        });

        // GET /api/sensors/<id> — single sensor by sensor id
        CROW_ROUTE(app, "/api/sensors/<uint>")([&apiClient](unsigned int sensorId) {
            nlohmann::json params;
            params[sjp::ARGS.data()] = nlohmann::json::array({sensorId});
            return coreGet(apiClient, scc::SENSOR, params);
        });

        // GET /api/sensors/<id>/readings?limit=N — readings by sensor id
        CROW_ROUTE(app, "/api/sensors/<uint>/readings")(
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

                return coreGet(apiClient, scc::SENSOR_READINGS, params);
            });

        // GET /api/sensors/<id>/value?force=bool — live sensor value
        CROW_ROUTE(app, "/api/sensors/<uint>/value")(
            [&apiClient](const crow::request &req, unsigned int sensorId) {
                nlohmann::json params;
                params[sjp::ARGS.data()] = nlohmann::json::array({sensorId});

                std::string_view type = scmt::SENSOR_VALUE;
                if (const auto forceParam = req.url_params.get(sjp::FORCE.data())) {
                    if (std::string_view(forceParam) == "true") {
                        type = scmt::FORCE_READ_SENSOR_VALUE;
                        params[sjp::FORCE.data()] = true;
                    }
                }
                return coreGet(apiClient, type, params);
            });
    }
}
