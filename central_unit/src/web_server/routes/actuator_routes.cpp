#include "actuator_routes.h"

namespace SmartHomeWebServer {
    namespace sjp = SmartHome::JsonRpcStrings::ParamsKeys;
    namespace sc = SmartHome::Constants;
    namespace scmt = SmartHome::Constants::MediatorTypes;

    // TODO rework after core set handler is reworked
    void registerActuatorRoutes(crow::App<crow::CORSHandler> &app, ApiClient &apiClient) {
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
                params[sjp::TYPE] = scmt::TOGGLE_ACTUATOR;
                params[sjp::MODULE_ID] = moduleId;
                params[sjp::ARGS] = nlohmann::json::array({body["logic_id"].get<unsigned int>()});

                return forwardToCore(apiClient,
                                     sa::getTargetMethodString(sc::Targets::MODULE_MEDIATOR, sc::Methods::SET),
                                     params);
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
                params[sjp::TYPE] = scmt::SET_ACTUATOR_VALUE;
                params[sjp::MODULE_ID] = moduleId;
                params[sjp::ARGS] = nlohmann::json::array({body["logic_id"].get<unsigned int>(), body["value"]});

                return forwardToCore(apiClient,
                                     sa::getTargetMethodString(sc::Targets::MODULE_MEDIATOR, sc::Methods::SET),
                                     params);
            });
    }
}
