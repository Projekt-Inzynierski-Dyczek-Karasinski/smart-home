#include "api.h"

#include <iostream>

#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>

namespace SmartHome::API {
    ApiId::ApiId(const apiId_t value) : mState(State::HAS_VALUE), mValue(value) {
    }

    ApiId::ApiId(std::nullptr_t) : mState(State::NULL_VALUE) {
    }

    bool ApiId::isUndefined() const { return mState == State::UNDEFINED; }

    bool ApiId::isNull() const { return mState == State::NULL_VALUE; }

    bool ApiId::hasValue() const { return mState == State::HAS_VALUE; }

    apiId_t ApiId::value() const {
        if (hasValue()) {
            return mValue;
        }
        throw std::runtime_error("No value specified");
    }

    nlohmann::json ApiId::toJson() const {
        if (mState == State::UNDEFINED)
            throw std::runtime_error(
                "Cannot cast ApiId to nlohmann::json - ID undefined");

        nlohmann::json result;

        if (hasValue()) result[JsonRpcStrings::Keys::ID] = mValue;
        else result[JsonRpcStrings::Keys::ID] = JsonRpcStrings::Constants::NULL_VALUE;

        return result;
    }

    ApiId ApiId::fromJson(const nlohmann::json &json) {
        const auto iter = json.find(JsonRpcStrings::Keys::ID);
        if (iter == json.end()) {
            mState = State::UNDEFINED;
        } else if (iter.value().is_number()) {
            mValue = json[JsonRpcStrings::Keys::ID];
            mState = State::HAS_VALUE;
        } else if (iter.value().is_string() && iter.value() == JsonRpcStrings::Constants::NULL_VALUE) {
            mState = State::NULL_VALUE;
        } else {
            throw std::runtime_error("Cannot cast json to ApiId - Invalid ID value");
        }
        return *this;
    }

    ApiId &ApiId::operator=(const apiId_t value) {
        mState = State::HAS_VALUE;
        mValue = value;
        return *this;
    }

    ApiId &ApiId::operator=(std::nullptr_t) {
        mState = State::NULL_VALUE;
        mValue = {};
        return *this;
    }

    ApiError::ApiError(const nlohmann::json &value) {
        setValues(value);
    }

    ApiError::ApiError(const std::string_view value) {
        setValues(nlohmann::json::parse(value));
    }

    ApiError::ApiError(const ErrorCodes newCode, const std::string_view newMessage, const std::string_view newData) {
        code = newCode;
        message = newMessage;
        data = newData;
    }

    nlohmann::json ApiError::to_json() {
        nlohmann::json json;

        json[JsonRpcStrings::ErrorKeys::CODE] = code;
        json[JsonRpcStrings::ErrorKeys::MESSAGE] = message;
        if (!data.empty()) json[JsonRpcStrings::ErrorKeys::DATA] = data;

        return json;
    }

    std::string ApiError::to_string() {
        return nlohmann::to_string(to_json());
    }

    void ApiError::setValues(nlohmann::json json) {
        char errorMessage[ERROR_MESSAGE_BUFFER_SIZE];
        if (!json.contains(JsonRpcStrings::ErrorKeys::CODE)) {
            sprintf(errorMessage, "Invalid JSON-RPC error: missing '%s' field",
                    JsonRpcStrings::ErrorKeys::CODE.data());
            throw std::invalid_argument(errorMessage);
        }
        if (!json[JsonRpcStrings::ErrorKeys::CODE].is_number_integer()) {
            sprintf(errorMessage, "Invalid JSON-RPC error: '%s' must be integer",
                    JsonRpcStrings::ErrorKeys::CODE.data());
            throw std::invalid_argument(errorMessage);
        }

        if (!json.contains(JsonRpcStrings::ErrorKeys::MESSAGE)) {
            sprintf(errorMessage, "Invalid JSON-RPC error: missing '%s' field",
                    JsonRpcStrings::ErrorKeys::MESSAGE.data());
            throw std::invalid_argument(errorMessage);
        }
        if (json[JsonRpcStrings::ErrorKeys::MESSAGE].get<std::string>().empty()) {
            sprintf(errorMessage, "Invalid JSON-RPC error: '%s' cannot be empty",
                    JsonRpcStrings::ErrorKeys::MESSAGE.data());
            throw std::invalid_argument(errorMessage);
        }

        code = static_cast<ErrorCodes>(json[JsonRpcStrings::ErrorKeys::CODE].get<int>());
        message = json[JsonRpcStrings::ErrorKeys::MESSAGE].get<std::string>();
        if (json.contains(JsonRpcStrings::ErrorKeys::DATA) && json[JsonRpcStrings::ErrorKeys::DATA].is_string())
            data = json[JsonRpcStrings::ErrorKeys::DATA].get<std::string>();
    }

    ApiRequest::ApiRequest(const nlohmann::json &value) {
        setValues(value);
    }

    ApiRequest::ApiRequest(std::string_view value) {
        if (nlohmann::json::accept(value)) {
            setValues(nlohmann::json::parse(value));
        } else {
            setValues(value);
        }
    }

    ApiRequest ApiRequest::operator()(const nlohmann::json &value) {
        setValues(value);
        return *this;
    }

    ApiRequest ApiRequest::operator()(std::string_view value) {
        if (nlohmann::json::accept(value)) {
            setValues(nlohmann::json::parse(value));
        } else {
            setValues(value);
        }
        return *this;
    }

    nlohmann::json ApiRequest::to_json() {
        nlohmann::json json;

        json[JsonRpcStrings::Keys::JSONRPC] = jsonrpc;
        json[JsonRpcStrings::RequestKeys::METHOD] = method;
        if (params.has_value()) json[JsonRpcStrings::RequestKeys::PARAMS] = *params;
        if (!id.isUndefined()) json.update(id.toJson());

        return json;
    }

    std::string ApiRequest::to_string() {
        return nlohmann::to_string(to_json());
    }

    void ApiRequest::setValues(const nlohmann::json &json) {
        char errorMessage[ERROR_MESSAGE_BUFFER_SIZE];
        if (!json.contains(JsonRpcStrings::Keys::JSONRPC)) {
            sprintf(errorMessage, "Invalid JSON-RPC request: missing '%s' field",
                    JsonRpcStrings::Keys::JSONRPC.data());
            throw std::invalid_argument(errorMessage);
        }
        if (json[JsonRpcStrings::Keys::JSONRPC].get<std::string>() != JsonRpcStrings::Constants::VERSION.data()) {
            sprintf(errorMessage, "Invalid JSON-RPC request: %s must be equal '%s'",
                    JsonRpcStrings::Keys::JSONRPC.data(),
                    JsonRpcStrings::Constants::VERSION.data());
            throw std::invalid_argument(errorMessage);
        }

        if (!json.contains(JsonRpcStrings::RequestKeys::METHOD)) {
            sprintf(errorMessage, "Invalid JSON-RPC request: missing '%s' field",
                    JsonRpcStrings::RequestKeys::METHOD.data());
            throw std::invalid_argument(errorMessage);
        }
        if (json[JsonRpcStrings::RequestKeys::METHOD].get<std::string>().empty()) {
            sprintf(errorMessage, "Invalid JSON-RPC request: '%s' cannot be empty",
                    JsonRpcStrings::RequestKeys::METHOD.data());
            throw std::invalid_argument(errorMessage);
        }

        jsonrpc = json[JsonRpcStrings::Keys::JSONRPC];
        method = json[JsonRpcStrings::RequestKeys::METHOD];
        if (json.contains(JsonRpcStrings::RequestKeys::PARAMS)) params = json[JsonRpcStrings::RequestKeys::PARAMS];
        if (json.contains(JsonRpcStrings::Keys::ID)) id = json[JsonRpcStrings::Keys::ID];
    }

    void ApiRequest::setValues(const std::string_view string) {
        std::vector<std::string> splitRequest = {};
        boost::split(splitRequest, string, boost::is_any_of(" "));

        if (splitRequest.size() < 2) {
            throw std::invalid_argument("Invalid raw string request: request must have at least target and method");
        }

        jsonrpc = JsonRpcStrings::Constants::VERSION.data();
        params.emplace(nlohmann::json());
        auto &paramsJsonObject = params.value();
        paramsJsonObject[JsonRpcStrings::ParamsKeys::TARGET] = splitRequest[0];
        method = splitRequest[1];
        id = static_cast<apiId_t>(0);

        if (splitRequest.size() > 2) {
            paramsJsonObject[JsonRpcStrings::ParamsKeys::METHOD_PARAMS] = nlohmann::json::array();
            for (size_t i = 2; i < splitRequest.size(); i++) {
                emplaceParameter(paramsJsonObject[JsonRpcStrings::ParamsKeys::METHOD_PARAMS], splitRequest[i]);
            }
        }
    }

    ApiResponse ApiResponse::operator()(const nlohmann::json &value) {
        setValues(value);
        return *this;
    }

    ApiResponse ApiResponse::operator()(std::string_view value) {
        const nlohmann::json json = nlohmann::json::parse(value);
        setValues(json);
        return *this;
    }

    nlohmann::json ApiResponse::to_json() {
        nlohmann::json json;

        json[JsonRpcStrings::Keys::JSONRPC] = jsonrpc;
        if (result.has_value()) {
            if (nlohmann::json::accept(result.value()))
                json[JsonRpcStrings::ResponseKeys::RESULT] = nlohmann::json::parse(result.value());
            else json[JsonRpcStrings::ResponseKeys::RESULT] = result.value();
        } else if (error.has_value()) {
            json[JsonRpcStrings::ResponseKeys::ERROR] = error.value().to_json();
        } else {
            throw std::invalid_argument("Invalid JSON-RPC response");
        }

        if (!id.isUndefined()) json.update(id.toJson());

        return json;
    }

    std::string ApiResponse::to_string() {
        return nlohmann::to_string(to_json());
    }

    void ApiResponse::setValues(const nlohmann::json &json) {
        if (!(json.contains(JsonRpcStrings::Keys::JSONRPC) &&
              json[JsonRpcStrings::Keys::JSONRPC].get<std::string>() == JsonRpcStrings::Constants::VERSION.data())) {
            char errorMessage[256];
            sprintf(errorMessage, "Invalid JSON-RPC request: %s must be equal '%s'",
                    JsonRpcStrings::Keys::JSONRPC.data(),
                    JsonRpcStrings::Constants::VERSION.data());
            throw std::invalid_argument(errorMessage);
        }
        if (!json.contains(JsonRpcStrings::Keys::ID)) {
            throw std::invalid_argument("Invalid JSON-RPC request: response must contain id");
        }

        if (json.contains(JsonRpcStrings::ResponseKeys::RESULT) && !json.contains(JsonRpcStrings::ResponseKeys::ERROR)) {
            auto &jsonResult = json[JsonRpcStrings::ResponseKeys::RESULT];
            if (jsonResult.is_string()) result = jsonResult.get<std::string>();
            else result = jsonResult.dump();
        }
        else if (json.contains(JsonRpcStrings::ResponseKeys::ERROR) && !json.contains(
                     JsonRpcStrings::ResponseKeys::RESULT))
            error.emplace(
                json[JsonRpcStrings::ResponseKeys::ERROR]);
        else throw std::invalid_argument("Invalid JSON-RPC request: response must contain either result or error");

        jsonrpc = json[JsonRpcStrings::Keys::JSONRPC];
        id = json[JsonRpcStrings::Keys::ID];
    }

    std::string_view errorCodeToString(const ErrorCodes errorCode) {
        switch (errorCode) {
            case ErrorCodes::NO_ERROR:
                return "No error";
            case ErrorCodes::PARSE_ERROR:
                return "Parse error";
            case ErrorCodes::INVALID_REQUEST:
                return "Invalid request";
            case ErrorCodes::METHOD_NOT_FOUND:
                return "Method not found";
            case ErrorCodes::INVALID_PARAMS:
                return "Invalid params";
            case ErrorCodes::INTERNAL_ERROR:
                return "Internal error";
            case ErrorCodes::MODULE_RUNTIME_ERROR:
                return "Module runtime error";
            case ErrorCodes::MEDIATOR_COMMUNICATION_ERROR:
                return "Mediator communication error";
            case ErrorCodes::MEDIATOR_RUNTIME_ERROR:
                return "Mediator runtime error";
            case ErrorCodes::UNKNOWN_ERROR:
                return "Unknown error";
            default:
                return "Undefined error";
        }
    }

    nlohmann::json parseValue(std::string_view value) {
        // Check int
        if (std::ranges::all_of(value, isdigit)) {
            try {
                long result = std::stoi(value.data());
                return result;
            } catch (...) {
                // Value is not a valid int, continuing parsing attempts
            }
        }

        // Check float
        if (value.find('.') != std::string_view::npos) {
            try {
                return std::stod(value.data());
            } catch (...) {
                // Value is not a valid float, continuing parsing attempts
            }
        }

        // Check booleans
        if (value == "true") return true;
        if (value == "false") return false;

        // Default to string
        return value;
    }

    nlohmann::json parseVector(const std::vector<std::string> &values) {
        nlohmann::json array = nlohmann::json::array();

        for (const auto &value: values) {
            array.push_back(parseValue(value));
        }

        return array;
    }

    void emplaceParameter(nlohmann::json &params, std::string_view parameter) {
        if (parameter.empty()) return;
        // Check if parameter is key=value pair
        const auto equalsPos = parameter.find('=');

        if (equalsPos == std::string_view::npos) {
            params.emplace_back(parseValue(parameter));
            return;
        }

        std::string_view key = parameter.substr(0, equalsPos);
        std::string_view value = parameter.substr(equalsPos + 1);

        nlohmann::json param;

        bool isJsonParseSuccessful = false;
        //Check for an JSON object
        if (nlohmann::json::accept(value)) {
            try {
                param[key] = nlohmann::json::parse(value);
                isJsonParseSuccessful = true;
            } catch (...) {
            }
        }
        // Check for a value list
        if (!isJsonParseSuccessful && value.find(',') != std::string_view::npos) {
            std::vector<std::string> values;
            boost::split(values, value, boost::is_any_of(","));
            param[key] = parseVector(values);
        } else {
            param[key] = parseValue(value);
        }
        params.emplace_back(param);
    }
}
