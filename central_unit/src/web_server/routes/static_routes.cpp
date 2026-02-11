#include "static_routes.h"

namespace SmartHomeWebServer {
    void registerStaticRoutes(crow::App<crow::CORSHandler> &app, const std::string &webRoot) {
        // Serve index.html for root path
        CROW_ROUTE(app, "/")([webRoot](crow::response &res) {
            res.set_static_file_info_unsafe(webRoot + "/index.html");
            res.end();
        });

        // Catch-all route for static files and SPA routing
        CROW_CATCHALL_ROUTE(app)([webRoot](const crow::request &req, crow::response &res) {
            // Ignore API routes
            if (req.url.starts_with("/api/")) {
                res.code = 404;
                res.end();
                return;
            }

            std::string rel = req.url;
            // Remove leading slash if present
            if (!rel.empty() && rel.front() == '/') rel.erase(0, 1);

            // Resolve the requested path against the web root and ensure it doesn't escape the directory
            const auto resolvedPath = std::filesystem::weakly_canonical(webRoot + "/" + rel);
            const auto resolvedRoot = std::filesystem::weakly_canonical(webRoot);

            // Check if the resolved path starts with the resolved web root path to prevent directory traversal
            if (resolvedPath.string().rfind(resolvedRoot.string(), 0) != 0) {
                res.code = 403;
                res.end();
                return;
            }

            // If the path is empty (path was '/'), serve index.html
            if (rel.empty()) rel = "index.html";

            const std::string path = webRoot + "/" + rel;

            // Check if file exists, if not serve index.html for SPA routing
            if (!std::filesystem::exists(path)) {
                res.set_static_file_info_unsafe(webRoot + "/index.html");
                res.end();
                return;
            }

            res.set_static_file_info_unsafe(path);
            res.end();
        });
    }
}
