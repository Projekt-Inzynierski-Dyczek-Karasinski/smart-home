#pragma once
#include "routes.h"


namespace SmartHomeWebServer {
    /**
     * @brief Registers sensor-related API routes for the web server.
     *
     * @details Registers REST endpoints under `/api/sensors` for sensor data, readings,
     *          and current sensor values.
     *
     * @details Endpoints:
     *          - \code GET /api/sensors \endcode
     *            Returns all sensors.
     *          - \code GET /api/sensors/<id> \endcode
     *            Returns a single sensor by ID.
     *          - \code GET /api/sensors/<id>/readings?limit=N \endcode
     *            Returns the last N readings (default 1) for a sensor.
     *          - \code GET /api/sensors/<id>/value?force=bool \endcode
     *            Returns the current sensor value, when force=true, triggers a fresh read.
     *
     * @param app The Crow application instance to which the routes will be registered.
     * @param apiClient The API client used to communicate with the core service.
     */
    void registerDevicesRoutes(crow::App<crow::CORSHandler> &app, ApiClient &apiClient);
}
