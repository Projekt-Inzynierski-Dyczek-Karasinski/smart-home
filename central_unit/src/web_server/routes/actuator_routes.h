#pragma once

#pragma once
#include "routes.h"

namespace SmartHomeWebServer {
    //TODO rework needed
    /**
     * @brief Register actuator-related routes to the Crow app.
     *
     * @param app Crow application instance to register routes on.
     * @param apiClient API client for forwarding requests to core.
     */
    void registerActuatorRoutes(crow::App<crow::CORSHandler> &app, ApiClient &apiClient);
}
