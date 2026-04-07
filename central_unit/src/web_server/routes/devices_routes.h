#pragma once
#include "routes.h"


namespace SmartHomeWebServer {
    /**
     * @brief Registers device-related API routes for the web server.
     *
     * @details Endpoints:
     *          - \code GET /api/devices \endcode
     *            Returns all devices.
     *          - \code GET /api/devices/<id> \endcode
     *            Returns a single device by ID.
     *          - \code GET /api/devices/<id>/readings?limit=<int>&from=<ts>&to=<ts> \endcode
     *            Returns the last N readings (default 10) for a device.
     *          - \code GET /api/devices/<id>/value?force=<bool> \endcode
     *            Returns the current device value, when force=true triggers a fresh read.
     *          - \code POST /api/devices \endcode
     *            Create a new device. Body: {"values": {...}, "returning"?: "*"|["..."]}.
     *          - \code PATCH /api/devices/<id> \endcode
     *            Update device field. Body: {"mode": "overwrite"|"append", "path": <str>, "value": <any>,
     *            "returning"?: "*"|["..."]}.
     *          - \code DELETE /api/devices/<id> \endcode
     *            Delete device record or JSONB value. Body?: {"path": <str>}.
     *          - \code POST /api/modules/<id>/actuators/toggle \endcode
     *            Toggle actuator state. Body: {"logic_id": <int>}.
     *          - \code POST /api/modules/<id>/actuators/value \endcode
     *            Set actuator value. Body: {"logic_id": <int>, "value": <any>}.
     *
     * @param app The Crow application instance to which the routes will be registered.
     * @param apiClient The API client used to communicate with the core service.
     */
    void registerDevicesRoutes(crow::App<crow::CORSHandler> &app, ApiClient &apiClient);
}
