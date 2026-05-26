#include "static_routes.h"

namespace SmartHomeWebServer {
    void registerStaticRoutes(crow::App<crow::CORSHandler> &app, const std::string &webRoot) {
        auto serveFile = [webRoot](const std::string &relativePath, crow::response &res) {
            const auto resolvedRoot = std::filesystem::weakly_canonical(webRoot);
            const auto resolvedPath = std::filesystem::weakly_canonical(webRoot + "/" + relativePath);

            const std::string rootStr = resolvedRoot.string();
            const std::string pathStr = resolvedPath.string();
            if (!pathStr.starts_with(rootStr) ||
                (pathStr.size() > rootStr.size() && pathStr[rootStr.size()] != '/')) {
                res.code = 403;
                res.body = nlohmann::json({{sjp::ERROR, "Forbidden"}});
                res.set_header("Content-Type", "application/json");
                res.end();
                return;
                }

            if (!std::filesystem::exists(resolvedPath)) {
                res.code = 404;
                res.body = nlohmann::json({{sjp::ERROR, "File not found"}});
                res.set_header("Content-Type", "application/json");
                res.end();
                return;
            }

            // Sanitized by weakly_canonical and path checks
            res.set_static_file_info_unsafe(resolvedPath.string());
            res.end();
        };

        // Special route for next.js files
        CROW_ROUTE(app, "/_next/<path>")
        ([serveFile](const crow::request &, crow::response &res, const std::string &path) {
            serveFile("_next/" + path, res);
        });

        // Special route for favicon
        CROW_ROUTE(app, "/favicon.ico")
        ([serveFile](crow::response &res) {
            serveFile("favicon.ico", res);
        });

        // SPA fallback route - serve index.html for all non-API routes
        CROW_CATCHALL_ROUTE(app)
        ([webRoot](const crow::request &req, crow::response &res) {
            if (req.url.starts_with("/api/")) {
                res.code = 404;
                res.body = nlohmann::json({{sjp::ERROR, "Invalid endpoint"}});
                res.set_header("Content-Type", "application/json");
                res.end();
                return;
            }

            // No sanitization for hardcoded index path
            res.set_static_file_info_unsafe(webRoot + "/index.html");
            res.end();
        });
    }
}
