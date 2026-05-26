#pragma once
#include "routes.h"


namespace SmartHomeWebServer {
    /**
     * TODO update or remove
     * @brief Registers core API routes for the web server.
     *
     * @param app The Crow application instance to which the routes will be registered.
     * @param apiClient The API client used to communicate with the core service.
     */
    void registerCoreRoutes(crow::App<crow::CORSHandler> &app, ApiClient &apiClient);
}
