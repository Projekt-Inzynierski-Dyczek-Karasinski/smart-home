#include "database_routes.h"

namespace SmartHomeWebServer {
    namespace sc = SmartHome::Constants;


    std::expected<nlohmann::json, crow::response> parseBody(const crow::request &req) {
        if (req.body.empty()) {
            crow::response res(400, R"({"error":"Request body is required"})");
            res.set_header("Content-Type", "application/json");
            return std::unexpected(std::move(res));
        }

        try {
            auto body = nlohmann::json::parse(req.body);
            if (!body.is_object()) {
                crow::response res(400, R"({"error":"Request body must be a JSON object"})");
                res.set_header("Content-Type", "application/json");
                return std::unexpected(std::move(res));
            }
            if (!body.contains("table") || !body["table"].is_string()) {
                crow::response res(400, R"({"error":"'table' field is required"})");
                res.set_header("Content-Type", "application/json");
                return std::unexpected(std::move(res));
            }
            return body;
        } catch (...) {
            crow::response res(400, R"({"error":"Invalid JSON body"})");
            res.set_header("Content-Type", "application/json");
            return std::unexpected(std::move(res));
        }
    }

    // TODO replace with more specific routes after core set handler is reworked
    void registerDatabaseRoutes(crow::App<crow::CORSHandler> &app, ApiClient &apiClient) {
        // POST /api/database/get
        // Body: {"table": "...", "columns": [...], "where": {...}, "order_by": [...], "limit": N}
        CROW_ROUTE(app, "/api/database/get").methods("POST"_method)(
            [&apiClient](const crow::request &req) {
                auto parsed = parseBody(req);
                if (!parsed.has_value())
                    return std::move(parsed).error();

                return forwardToCore(apiClient,
                                     sa::getTargetMethodString(sc::Targets::DATABASE, sc::Methods::GET),
                                     std::move(parsed).value());
            });

        // POST /api/database/set
        // Body: {"table": "...", "values": {...}, "where": {...}} — INSERT (no where) or UPDATE (with where)
        CROW_ROUTE(app, "/api/database/set").methods("POST"_method)(
            [&apiClient](const crow::request &req) {
                auto parsed = parseBody(req);
                if (!parsed.has_value())
                    return std::move(parsed).error();

                auto body = std::move(parsed).value();
                if (!body.contains("values") || !body["values"].is_object()) {
                    crow::response res(400, R"({"error":"'values' object is required for set operations"})");
                    res.set_header("Content-Type", "application/json");
                    return res;
                }

                return forwardToCore(apiClient,
                                     sa::getTargetMethodString(sc::Targets::DATABASE, sc::Methods::SET),
                                     body);
            });

        // POST /api/database/delete
        // Body: {"table": "...", "where": {...}}
        CROW_ROUTE(app, "/api/database/delete").methods("POST"_method)(
            [&apiClient](const crow::request &req) {
                auto parsed = parseBody(req);
                if (!parsed.has_value())
                    return std::move(parsed).error();;

                auto body = std::move(parsed).value();
                if (!body.contains("where") || !body["where"].is_object()) {
                    crow::response res(400, R"({"error":"'where' object is required for delete operations"})");
                    res.set_header("Content-Type", "application/json");
                    return res;
                }

                return forwardToCore(apiClient,
                                     sa::getTargetMethodString(sc::Targets::DATABASE, sc::Methods::DELETE),
                                     body);
            });
    }
}
