#pragma once
#include "routes.h"

namespace SmartHomeWebServer {
    // TODO Insecure direct database access.
    //      Consider implementing database method/type action handler to limit direct database access,
    //      or implement admin user authentication
    /**
     * @brief Register database-related routes to the Crow app.
     *
     * @details Endpoints:
     *          - \code GET /api/database/get?table=<str>&columns=["..."]&where={...}&order_by=[...]&limit=<int> \endcode
     *            Query database table with optional filters, column selection, ordering and limit.
     *          - \code POST /api/database/insert \endcode
     *            Insert a new record. Body: {"table": <str>, "values": {...}, "returning"?: "*"|["..."]}.
     *          - \code PATCH /api/database/update \endcode
     *            Update existing records. Body: {"table": <str>, "values": {...}, "where": {...},
     *            "returning"?: "*"|["..."]}.
     *          - \code DELETE /api/database/delete \endcode
     *            Delete records or column/JSONB values. Body: {"table": <str>, "where": {...}, "columns"?: ["..."]}.
     *
     * @param app Crow application instance to register routes on.
     * @param apiClient API client for forwarding requests to core.
     */
    void registerDatabaseRoutes(crow::App<crow::CORSHandler> &app, ApiClient &apiClient);
}
