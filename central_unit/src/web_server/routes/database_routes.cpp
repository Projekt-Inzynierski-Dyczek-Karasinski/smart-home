#include "database_routes.h"
#include "route_helpers.h"

namespace SmartHomeWebServer {
    using namespace std::string_literals;

    namespace sc = SmartHome::Constants;

    // TODO Insecure direct database access.
    //      Consider implementing database set/get handler to limit direct database access,
    //      or implement admin user authentication
    void registerDatabaseRoutes(crow::App<crow::CORSHandler> &app, ApiClient &apiClient) {
        // GET /api/database/get
        // Query: table=<str>&columns=["..."]&where={...}&order_by=[...]&limit=N
        CROW_ROUTE(app, "/api/database/get").methods("GET"_method)(
            [&apiClient](const crow::request &req) {
                auto table = req.url_params.get(sjp::TABLE.data());
                if (!table) return RouteHelpers::badRequest("'"s + sjp::TABLE.data() + "' is required");

                nlohmann::json params;
                params[sjp::TABLE] = table;

                // Optional JSON params - parse from query string
                if (const auto columns = req.url_params.get(sjp::COLUMNS.data())) {
                    if (!nlohmann::json::accept(columns))
                        return RouteHelpers::badRequest("'"s + sjp::COLUMNS.data() + "' must be valid JSON");
                    params[sjp::COLUMNS] = nlohmann::json::parse(columns);
                }

                if (const auto where = req.url_params.get(sjp::WHERE.data())) {
                    if (!nlohmann::json::accept(where))
                        return RouteHelpers::badRequest("'"s + sjp::WHERE.data() + "' must be valid JSON");
                    params[sjp::WHERE] = nlohmann::json::parse(where);
                }

                if (const auto orderBy = req.url_params.get(sjp::ORDER_BY.data())) {
                    if (!nlohmann::json::accept(orderBy))
                        return RouteHelpers::badRequest("'"s + sjp::ORDER_BY.data() + "' must be valid JSON");
                    params[sjp::ORDER_BY] = nlohmann::json::parse(orderBy);
                }

                if (const auto limit = req.url_params.get(sjp::LIMIT.data())) {
                    try { params[sjp::LIMIT] = std::stoi(limit); } catch (...) {
                        return RouteHelpers::badRequest("'"s + sjp::LIMIT.data() + "' must be an integer");
                    }
                }

                return forwardToCore(apiClient,
                                     sa::getTargetMethodString(sc::Targets::DATABASE, sc::Methods::GET),
                                     params);
            });


        // POST /api/database/insert
        // Body: {"table": "...", "values": {...}, "returning"?: "*"|[...]}
        CROW_ROUTE(app, "/api/database/insert").methods("POST"_method)(
            [&apiClient](const crow::request &req) {
                auto body = RouteHelpers::requireBody(req);
                if (!body) return std::move(body.error());

                auto table = RouteHelpers::requireStringField(*body, sjp::TABLE);
                if (!table) return std::move(table.error());

                auto values = RouteHelpers::requireObjectField(*body, sjp::VALUES);
                if (!values) return std::move(values.error());

                return forwardToCore(apiClient,
                                     sa::getTargetMethodString(sc::Targets::DATABASE, sc::Methods::SET),
                                     *body);
            });

        // PATCH /api/database/update
        // Body: {"table": "...", "values": {...}, "where": {...}, "returning"?: "*"|[...]}
        CROW_ROUTE(app, "/api/database/update").methods("PATCH"_method)(
            [&apiClient](const crow::request &req) {
                auto body = RouteHelpers::requireBody(req);
                if (!body) return std::move(body.error());

                auto table = RouteHelpers::requireStringField(*body, sjp::TABLE);
                if (!table) return std::move(table.error());

                auto values = RouteHelpers::requireObjectField(*body, sjp::VALUES);
                if (!values) return std::move(values.error());

                auto where = RouteHelpers::requireObjectField(*body, sjp::WHERE);
                if (!where) return std::move(where.error());

                return forwardToCore(apiClient,
                                     sa::getTargetMethodString(sc::Targets::DATABASE, sc::Methods::SET),
                                     *body);
            });

        // DELETE /api/database/delete
        // Body: {"table": "...", "where": {...}, "columns"?: [...]}
        CROW_ROUTE(app, "/api/database/delete").methods("DELETE"_method)(
             [&apiClient](const crow::request &req) {
                 auto body = RouteHelpers::requireBody(req);
                 if (!body) return std::move(body.error());

                 auto table = RouteHelpers::requireStringField(*body, sjp::TABLE);
                 if (!table) return std::move(table.error());

                 auto where = RouteHelpers::requireObjectField(*body, sjp::WHERE);
                 if (!where) return std::move(where.error());

                 return forwardToCore(apiClient,
                                      sa::getTargetMethodString(sc::Targets::DATABASE, sc::Methods::DELETE),
                                      *body);
             });
    }
}
