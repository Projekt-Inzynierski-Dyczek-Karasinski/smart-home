#pragma once

#include "../types.h"

#include <optional>
#include <string_view>
#include <string>

#include <nlohmann/json.hpp>

namespace SmartHome::JsonRpcStrings {
    /// JSON-RPC 2.0 protocol constants
    namespace Constants {
        inline constexpr std::string_view VERSION = "2.0";
        inline constexpr std::string_view NULL_VALUE = "null";
    }

    /// Common JSON-RPC keys
    namespace Keys {
        inline constexpr std::string_view JSONRPC = "jsonrpc";
        inline constexpr std::string_view ID = "id";
    }

    /// JSON-RPC request-specific keys
    namespace RequestKeys {
        inline constexpr std::string_view METHOD = "method";
        inline constexpr std::string_view PARAMS = "params";
    }

    // TODO consider moving target to method value (eg. "core.get", "module_mediator.set"), for params object simplification
    /// Custom parameter keys for SmartHome API
    namespace ParamsKeys {
        inline constexpr std::string_view TARGET = "target";
        inline constexpr std::string_view METHOD_PARAMS = "method_params";
    }

    /// JSON-RPC response-specific keys
    namespace ResponseKeys {
        inline constexpr std::string_view RESULT = "result";
        inline constexpr std::string_view ERROR = "error";
    }

    /// JSON-RPC error object keys
    namespace ErrorKeys {
        inline constexpr std::string_view CODE = "code";
        inline constexpr std::string_view MESSAGE = "message";
        inline constexpr std::string_view DATA = "data";
    }
}


namespace SmartHome::API {
    /**
     * @brief Three-state API identifier container.
     *
     * @details Manages JSON-RPC id field with three possible states:
     *          undefined (no id field), null value, or numeric value.
     */
    struct ApiId {
    private:
        enum class State {
            UNDEFINED, ///< ID field not present
            NULL_VALUE, ///< ID field is null
            HAS_VALUE ///< ID field has numeric value
        };

        State mState = State::UNDEFINED;
        apiId_t mValue = {};

    public:
        ApiId() = default;

        /**
         * @brief Sets numeric value for ApiId.
         *
         * @param value Numeric value to set ID to.
         */
        explicit ApiId(apiId_t value);

        /**
         * @brief Sets numeric value to null.
         */
        explicit ApiId(std::nullptr_t);

        /**
         * @brief Check if ID is undefined (not present).
         *
         * @return true if ID is undefined, false otherwise.
         */
        bool isUndefined() const;

        /**
         * @brief Check if ID is null.
         *
         * @return true if ID has null value, false otherwise.
         */
        bool isNull() const;

        /**
         * @brief Check if ID has numeric value.
         *
         * @return true if ID has numeric value, false otherwise.
         */
        bool hasValue() const;

        /**
         * @brief Get numeric value of ID.
         *
         * @return Stored ID value.
         * @throws std::runtime_error If ID has no value.
         */
        apiId_t value() const;

        /**
         * @brief Convert ID to JSON object.
         *
         * @return JSON object with id field.
         * @throws std::runtime_error If ID is undefined.
         */
        nlohmann::json toJson() const;

        /**
         * @brief Parse ID from JSON object.
         *
         * @param json JSON object to parse.
         * @return Reference to this object.
         * @throws std::runtime_error If JSON contains invalid ID value.
         */
        ApiId fromJson(const nlohmann::json &json);

        /// Assign numeric value to ID
        ApiId &operator=(apiId_t value);

        /// Assign null value to ID
        ApiId &operator=(std::nullptr_t);
    };

    /**
     * @brief JSON-RPC error codes for API.
     */
    enum class ErrorCodes: int {
        NO_ERROR = 0,
        PARSE_ERROR = -32700,
        INVALID_REQUEST = -32600,
        METHOD_NOT_FOUND = -32601,
        INVALID_PARAMS = -32602,
        INTERNAL_ERROR = -32603,
        // custom errors
        UNKNOWN_ERROR = -32000,
        MODULE_RUNTIME_ERROR = -32001,
        MEDIATOR_COMMUNICATION_ERROR = -32002,
        MEDIATOR_RUNTIME_ERROR = -32003,
    };

    /**
     * @brief JSON-RPC error struct for API.
     */
    struct ApiError {
        ErrorCodes code = ErrorCodes::NO_ERROR; ///< JSON-RPC error code
        std::string message; ///< Message explaining error code
        std::string data; ///< Optional additional error data

        ApiError() = default;

        /**
         * @brief Construct error from JSON object.
         *
         * @param value JSON object containing error fields.
         *
         * @throws nlohmann::json::parse_error Throws parse error on failed parse.
         * @throws std::invalid_argument Throws invalid argument when passed JSON object is not in JSON-RPC 2.0 error format.
         */
        explicit ApiError(const nlohmann::json &value);

        /**
         * @brief Construct error from JSON string.
         *
         * @param value JSON string containing error object.
         *
         * @throws nlohmann::json::parse_error Throws parse error on failed parse.
         * @throws std::invalid_argument Throws invalid argument when passed JSON string is not in JSON-RPC 2.0 error format.
         */
        explicit ApiError(std::string_view value);

        /**
         * @brief Construct error with specific values.
         *
         * @param newCode Error code.
         * @param newMessage Error message.
         * @param newData Additional error data.
         */
        ApiError(ErrorCodes newCode, std::string_view newMessage, std::string_view newData);

        /**
         * @brief Convert error to JSON object.
         *
         * @return JSON object representation of error.
         */
        nlohmann::json to_json();

        /**
         * @brief Convert error to JSON string.
         *
         * @return JSON string representation of error.
         */
        std::string to_string();

    private:
        /**
         * @brief Setter for struct variables.
         *
         * @param json JSON object to parse.
         * @throws std::invalid_argument Throws invalid argument when passed JSON object is not in JSON-RPC 2.0 error format
         */
        void setValues(nlohmann::json json);
    };

    /**
     * @brief JSON-RPC request struct for API.
     */
    struct ApiRequest {
        std::string jsonrpc = JsonRpcStrings::Constants::VERSION.data(); ///< JSON-RPC version
        std::string method; ///< Method name to call
        std::optional<nlohmann::json> params; ///< Optional method parameters
        ApiId id; ///< Request identifier


        ApiRequest() = default;

        /**
         * @brief Construct request from JSON object.
         *
         * @param value JSON object containing request fields.
         *
         * @throws nlohmann::json::parse_error Throws parse error on failed parse.
         * @throws std::invalid_argument Throws invalid argument when passed JSON object is not in JSON-RPC 2.0 format.
         */
        explicit ApiRequest(const nlohmann::json &value);

        /**
         * @brief Construct request from string (JSON or raw format).
         *
         * @details Accepts either JSON-RPC formatted string or space-separated raw format:
         *          "target method [params...]"
         *
         * @param value JSON string or raw command string.
         *
         * @throws nlohmann::json::parse_error Throws parse error on failed parse.
         * @throws std::invalid_argument Throws invalid argument when passed JSON string is not in JSON-RPC 2.0 format
         *         or raw string formated request does not contain target and method.
         */
        explicit ApiRequest(std::string_view value);

        nlohmann::json to_json();

        std::string to_string();

        /// Update request from JSON object.
        ApiRequest operator()(const nlohmann::json &value);

        /// Update request from string.
        ApiRequest operator()(std::string_view value);

    private:
        /**
         * @brief Setter for struct variables.
         *
         * @param json Json object to parse.
         * @throws std::invalid_argument Throws invalid argument when passed JSON object is not in JSON-RPC 2.0 format.
         */
        void setValues(const nlohmann::json &json);

        /**
         * @brief Setter for struct variables.
         *
         * @param string Raw string formatted request.
         * @throws std::invalid_argument Throws invalid argument when passed string does not contain target and method.
         */
        void setValues(std::string_view string);
    };


    /**
     * @brief JSON-RPC response struct for API.
     */
    struct ApiResponse {
        std::string jsonrpc = JsonRpcStrings::Constants::VERSION.data(); ///< JSON-RPC version
        std::optional<std::string> result; ///< Result string or JSON object stored as string
        std::optional<ApiError> error; ///< Error object if request failed
        ApiId id; ///< Response identifier matching request

        /**
         * @brief Convert response to JSON object.
         *
         * @return JSON object representation of response.
         * @throws std::invalid_argument If neither result nor error is set.
         */
        nlohmann::json to_json();

        /**
         * @brief Convert response to JSON string.
         *
         * @return JSON string representation of response.
         * @throws std::invalid_argument If neither result nor error is set.
         */
        std::string to_string();

        /// Update response from JSON object.
        ApiResponse operator()(const nlohmann::json &value);

        /// Update response from JSON string.
        ApiResponse operator()(std::string_view value);

    private:
        /**
         * @brief Setter for struct variables.
         *
         * @param json JSON object to parse.
         * @throws std::invalid_argument Throws invalid argument when passed JSON object is not in JSON-RPC 2.0 format.
         */
        void setValues(const nlohmann::json &json);
    };


    /**
     * @brief Abstract base class for API implementations.
     */
    class Api {
    public:
        virtual ~Api() = default;

        /**
         * @brief Handle incoming API request.
         *
         * @param connectionId Connection identifier for response routing.
         * @param message Message string to process.
         */
        virtual void handleIncoming(connectionId_t connectionId, std::string &&message) = 0;

        /**
         * @brief Handle outgoing API message.
         *
         * @param connectionId Connection identifier.
         * @param message Message string to process.
         */
        virtual void handleOutgoing(connectionId_t connectionId, std::string &&message) = 0;
    };


    /**
     * @brief Get string representation of API error codes.
     *
     * @param errorCode Error code to get string representation for.
     * @return String representation of error code.
     */
    std::string_view errorCodeToString(ErrorCodes errorCode);

    /**
     * @brief Parse string value to appropriate JSON type.
     *
     * @details Attempts to parse as int, float, bool, defaulting to string.
     *
     * @param value String value to parse.
     * @return JSON object with appropriate type.
     */
    nlohmann::json parseValue(std::string_view value);

    /**
     * @brief Parse vector of strings to JSON array.
     *
     * @param values Vector of string values.
     * @return JSON array with parsed values.
     */
    nlohmann::json parseVector(const std::vector<std::string> &values);

    /**
     * @brief Add parameter to JSON params object.
     *
     * @details Parses parameter as key=value pair or single value.
     *          Supports JSON objects, comma-separated lists, and basic types.
     *
     * @param params JSON params object to modify.
     * @param parameter Parameter string to parse and add.
     */
    void emplaceParameter(nlohmann::json &params, std::string_view parameter);

    /// Buffer size for error message formatting
    inline constexpr uint16_t ERROR_MESSAGE_BUFFER_SIZE = 1024;
}
