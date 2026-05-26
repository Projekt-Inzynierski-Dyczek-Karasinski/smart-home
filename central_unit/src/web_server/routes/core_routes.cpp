#include "core_routes.h"

namespace SmartHomeWebServer {
    namespace sc = SmartHome::Constants;

    // TODO Add more endpoints if more Core actions should be handled by web API or consider removing
    void registerCoreRoutes(crow::App<crow::CORSHandler> &app, ApiClient &apiClient) {
        CROW_ROUTE(app, "/api/status")([] {
            return nlohmann::json({{scc::STATUS, scc::OK}}).dump();
        });


        CROW_ROUTE(app, "/api/core/echo/<str>")([&apiClient](const std::string &message) {
            sa::ApiRequest request;
            request.id = sa::getNextApiId();
            request.method = sa::getTargetMethodString(sc::Targets::CORE, sc::Methods::ECHO_STR);
            auto paramsJson = nlohmann::json::object();

            paramsJson[sc::Methods::ECHO_STR] = message;

            request.params = paramsJson;

            try {
                return apiClient.sendRequest(request).get();
            } catch (const std::exception &e) {
                return nlohmann::json({{scc::ERROR, e.what()}}).dump();
            }
        });
    }
}
