#pragma once
#include "routes.h"

namespace SmartHomeWebServer {
    /**
     * @brief Parse and validate JSON body from request.
     *
     * @param req Incoming HTTP request.
     *
     * @return Parsed JSON or error response.
     */
    std::expected<nlohmann::json, crow::response> parseBody(const crow::request &req);

    // TODO rework needed
    /**
     * @brief Register database-related routes to the Crow app.
     *
     * @param app Crow application instance to register routes on.
     * @param apiClient API client for forwarding requests to core.
     */
    void registerDatabaseRoutes(crow::App<crow::CORSHandler> &app, ApiClient &apiClient);
}
