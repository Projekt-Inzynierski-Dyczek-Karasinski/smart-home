#pragma once
#include "routes.h"


namespace SmartHomeWebServer {
    /**
     * @brief Registers static file serving routes for the web server, including a catch-all route for SPA support.
     *
     * @details Endpoints:
     *          - \code `GET /` \endcode
     *            Serves the `index.html` file from the specified web root directory.
     *          - \code `GET / *` \endcode
     *            A catch-all route that serves static files from the web root directory.
     *            If the requested file does not exist, it serves `index.html` to support SPA routing.
     *
     * @param app The Crow application instance to which the routes will be registered.
     * @param webRoot The root directory from which static files will be served.
     *                This should be an absolute path to the directory containing the web assets.
     */
    void registerStaticRoutes(crow::App<crow::CORSHandler> &app, const std::string &webRoot);
}
