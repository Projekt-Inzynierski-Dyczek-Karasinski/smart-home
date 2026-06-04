#pragma once
#include "routes.h"

namespace SmartHomeWebServer::RouteHelpers {
    using BodyResult = std::expected<nlohmann::json, crow::response>;
    using StringResult = std::expected<std::string, crow::response>;

    /**
     * @brief Parse request body as a JSON object.
     *
     * @details Fails if the body is empty or cannot be parsed as a JSON object.
     *
     * @param req Incoming HTTP request.
     *
     * @return Parsed JSON object on success, 400 \c crow::response on failure.
     */
    BodyResult requireBody(const crow::request &req);

    /**
     * @brief Require a field of any JSON type in body.
     *
     * @param body Parsed JSON request body.
     * @param field Field name to look up.
     *
     * @return JSON value on success, 400 \c crow::response if the field is absent.
     */
    BodyResult requireField(const nlohmann::json &body, std::string_view field);

    /**
     * @brief Require a string field in body.
     *
     * @param body Parsed JSON request body.
     * @param field Field name to look up.
     *
     * @return String value on success, 400 \c crow::response if the field is absent or not a string.
     */
    StringResult requireStringField(const nlohmann::json &body, std::string_view field);

    /**
     * @brief Require an object field in body.
     *
     * @param body Parsed JSON request body.
     * @param field Field name to look up.
     *
     * @return JSON object on success, \c 400 crow::response if the field is absent or not an object.
     */
    BodyResult requireObjectField(const nlohmann::json &body, std::string_view field);

    /**
     * @brief Optionally extract a field from body.
     *
     * @param body Parsed JSON request body.
     * @param field Field name to look up.
     *
     * @return JSON value if the field exists, \c std::nullopt otherwise.
     */
    std::optional<nlohmann::json> optionalField(const nlohmann::json &body, std::string_view field);

    /**
     * @brief Build a 400 Bad Request response.
     *
     * @param message Error description included in the JSON body under "error" key.
     *
     * @return \c crow::response with status 400 and "Content-Type: application/json".
     */
    crow::response badRequest(std::string_view message);

    /**
     * @brief Build a 504 Gateway Timeout response.
     *
     * @param message Error description included in the JSON body under "error" key.
     *
     * @return \c crow::response with status 504 and "Content-Type: application/json".
     */
    crow::response gatewayTimeout(std::string_view message);
}