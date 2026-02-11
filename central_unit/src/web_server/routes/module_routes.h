#pragma once
#include "routes.h"

namespace SmartHomeWebServer {
    /**
     * @brief Registers module-related API routes for the web server.
     *
     * @details Registers REST endpoints under `/api/modules` for module data, sensors, readings,
     *          current sensor values, logs, battery level, and sensor list.
     *
     * @details Endpoints:
     *          - \code GET /api/modules \endcode
     *            Returns all modules.
     *          - \code GET /api/modules/<id> \endcode
     *            Returns a single module by ID.
     *          - \code GET /api/modules/<id>/sensors \endcode
     *            Returns sensors for a module.
     *          - \code GET /api/modules/<mid>/sensors/<lid> \endcode
     *            Returns a sensor by module ID and logic ID.
     *          - \code GET /api/modules/<mid>/sensors/<lid>/readings?limit=N \endcode
     *            Returns the last N readings (default 1) for a sensor.
     *          - \code GET /api/modules/<mid>/sensors/<lid>/value?force=bool \endcode
     *            Returns the current sensor value, when force=true, triggers a fresh read.
     *          - \code GET /api/modules/<id>/logs?limit=N \endcode
     *            Returns the last N log entries (default 1) for a module.
     *          - \code GET /api/modules/<id>/battery \endcode
     *            Returns the module battery level via the mediator.
     *          - \code GET /api/modules/<id>/sensor-list \endcode
     *            Returns the module sensor list via the mediator.
     *
     * @param app The Crow application instance to which the routes will be registered.
     * @param apiClient The API client used to communicate with the core service.
     */
    void registerModuleRoutes(crow::App<crow::CORSHandler> &app, ApiClient &apiClient);
}
