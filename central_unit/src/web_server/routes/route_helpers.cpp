#include "route_helpers.h"

namespace SmartHomeWebServer::RouteHelpers {
    using namespace std::string_literals;

    BodyResult requireBody(const crow::request &req) {
        if (req.body.empty()) {
            return std::unexpected(badRequest("Request body is required"));
        }

        try {
            auto body = nlohmann::json::parse(req.body);
            if (!body.is_object()) {
                return std::unexpected(badRequest("Request body must be a JSON object"));
            }
            return body;
        } catch (const std::exception &e) {
            return std::unexpected(badRequest("Invalid JSON body: "s + e.what()));
        }
    }

    BodyResult requireField(const nlohmann::json &body, const std::string_view field) {
        if (!body.contains(field)) {
            return std::unexpected(badRequest("'"s + field.data() + "' field is required"));
        }
        return body.at(field);
    }

    StringResult requireStringField(const nlohmann::json &body, const std::string_view field) {
        if (!body.contains(field) || !body.at(field).is_string()) {
            return std::unexpected(badRequest("'"s + field.data() + "' must be a string"));
        }
        return body.at(field).get<std::string>();
    }

    BodyResult requireObjectField(const nlohmann::json &body, const std::string_view field) {
        if (!body.contains(field) || !body.at(field).is_object()) {
            return std::unexpected(badRequest("'"s + field.data() + "' must be a JSON object"));
        }
        return body.at(field);
    }

    std::optional<nlohmann::json> optionalField(const nlohmann::json &body, const std::string_view field) {
        if (!body.contains(field)) return std::nullopt;
        return body.at(field);
    }

    crow::response badRequest(const std::string_view message) {
        crow::response res(400, nlohmann::json({{scc::ERROR, message}}).dump());
        res.set_header("Content-Type", "application/json");
        return res;
    }

    crow::response gatewayTimeout(const std::string_view message) {
        crow::response res(504, nlohmann::json({{scc::ERROR, message}}).dump());
        res.set_header("Content-Type", "application/json");
        return res;
    }
}
