#pragma once
#include "routes.h"

namespace SmartHomeWebServer {
    /**
     * @brief Registers module-related API routes for the web server.
     *
     * @details Endpoints:
     *          - \code GET /api/modules \endcode
     *            Returns all modules.
     *          - \code GET /api/modules/<id> \endcode
     *            Returns a single module by ID.
     *          - \code GET /api/modules/<id>/devices \endcode
     *            Returns devices for a module.
     *          - \code GET /api/modules/<mid>/devices/<lid> \endcode
     *            Returns a device by module ID and logic ID.
     *          - \code GET /api/modules/<mid>/devices/<lid>/readings?limit=N&from=<ts>&to=<ts> \endcode
     *            Returns the last N readings (default 1) for a device.
     *          - \code GET /api/modules/<mid>/devices/<lid>/value?force=bool \endcode
     *            Returns the current device value, when force=true triggers a fresh read.
     *          - \code GET /api/modules/<id>/logs?limit=<int> \endcode
     *            Returns the last N log entries (default 1) for a module.
     *          - \code GET /api/modules/<id>/battery \endcode
     *            Returns the module battery level via the mediator.
     *          - \code GET /api/modules/<id>/device-list \endcode
     *            Returns the module device list via the mediator.
     *          - \code POST /api/modules \endcode
     *            Create a new module. Body: {"values": {...}, "returning"?: "*"|["..."]}.
     *          - \code PATCH /api/modules/<id> \endcode
     *            Update module field. Body: {"mode": "overwrite"|"append|, "path": <str>, "value": <any>,
     *            "returning"?: "*"|["..."]}.
     *          - \code DELETE /api/modules/<id> \endcode
     *            Delete module record or JSONB value. Body?: {"path": <str>}.
     *
     * @param app The Crow application instance to which the routes will be registered.
     * @param apiClient The API client used to communicate with the core service.
     */
    void registerModuleRoutes(crow::App<crow::CORSHandler> &app, ApiClient &apiClient);
}
