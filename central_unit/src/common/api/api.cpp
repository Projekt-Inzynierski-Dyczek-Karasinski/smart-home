#include "api.h"
#include "../constants/constants.h"

#include <atomic>
#include <iostream>

#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>

namespace SmartHome::API {
    using namespace std::string_literals;

    namespace jc = JsonRpcStrings::Constants;
    namespace jk = JsonRpcStrings::Keys;
    namespace jek = JsonRpcStrings::ErrorKeys;
    namespace jrqk = JsonRpcStrings::RequestKeys;
    namespace jrsk = JsonRpcStrings::ResponseKeys;

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

        if (hasValue()) result[jk::ID] = mValue;
        else result[jk::ID] = nullptr;
        return result;
    }

    ApiId ApiId::fromJson(const nlohmann::json &json) {
        *this = ApiId();
        if (!json.contains(jk::ID)) {
            mState = State::UNDEFINED;
            return *this;
        }

        const auto &idJson = json.at(jk::ID);

        if (idJson.is_null()) {
            mState = State::NULL_VALUE;
            return *this;
        }
        if (idJson.is_number_integer()) {
            mState = State::HAS_VALUE;
            mValue = idJson.get<apiId_t>();
            return *this;
        }

        throw std::runtime_error("Cannot cast json to ApiId - Invalid ID value");
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
        const auto parsedValue = nlohmann::json::parse(value, nullptr, false);
        if (parsedValue.is_discarded())
            throw std::invalid_argument("ApiError parsing failed: string was not a valid JSON");
        setValues(parsedValue);
    }

    ApiError::ApiError(const ErrorCodes newCode, const std::string_view newMessage, const std::string_view newData) {
        code = newCode;
        message = newMessage;
        data = newData;
    }

    ApiError::ApiError(const ErrorCodes newCode, const std::string_view newData) {
        code = newCode;
        message = errorCodeToString(newCode);
        data = newData;
    }

    nlohmann::json ApiError::to_json() const {
        nlohmann::json json;

        json[jek::CODE] = code;
        json[jek::MESSAGE] = message;
        if (!data.empty()) json[jek::DATA] = data;

        return json;
    }

    std::string ApiError::to_string() const {
        return nlohmann::to_string(to_json());
    }

    void ApiError::setValues(nlohmann::json json) {
        constexpr auto errPrefix = "Invalid JSON-RPC error: ";
        clear();

        if (!json.contains(jek::CODE)) {
            throw std::invalid_argument(errPrefix + "missing '"s.append(jek::CODE).append("' field"));
        }
        const auto codeJson = json[jek::CODE];
        if (!codeJson.is_number_integer()) {
            throw std::invalid_argument(errPrefix + "'"s.append(jek::CODE).append("' must be integer"));
        }
        code = static_cast<ErrorCodes>(codeJson.get<int>());

        if (!json.contains(jek::MESSAGE)) {
            throw std::invalid_argument(errPrefix + "missing '"s.append(jek::MESSAGE).append("' field"));
        }
        const auto messageJson = json[jek::MESSAGE];
        if (!messageJson.is_string() || messageJson.get<std::string>().empty()) {
            throw std::invalid_argument(errPrefix + "'"s.append(jek::MESSAGE).append("' must be a non-empty string"));
        }
        message = messageJson.get<std::string>();

        if (json.contains(jek::DATA)) {
            if (const auto &dataJson = json[jek::DATA]; dataJson.is_string()) {
                data = dataJson.get<std::string>();
                return;
            }
            throw std::invalid_argument(errPrefix + "'"s.append(jek::DATA).append("' must be a string"));
        }
    }

    void ApiError::clear() {
        code = ErrorCodes::NO_ERROR;
        message = "";
        data = "";
    }

    ApiRequest::ApiRequest(const nlohmann::json &value) {
        setValues(value);
    }

    ApiRequest::ApiRequest(std::string_view value) {
        const auto parsedValue = nlohmann::json::parse(value, nullptr, false);
        if (parsedValue.is_discarded()) setValues(value);
        else setValues(parsedValue);
    }

    nlohmann::json ApiRequest::to_json() const {
        nlohmann::json json;

        json[jk::JSONRPC] = jsonrpc;
        json[jrqk::METHOD] = method;
        if (params.has_value()) json[jrqk::PARAMS] = *params;
        if (!id.isUndefined()) json.update(id.toJson());

        return json;
    }

    std::string ApiRequest::to_string() const {
        return nlohmann::to_string(to_json());
    }

    ApiRequest ApiRequest::operator()(const nlohmann::json &value) {
        setValues(value);
        return *this;
    }

    ApiRequest ApiRequest::operator()(std::string_view value) {
        const auto parsedValue = nlohmann::json::parse(value, nullptr, false);
        if (parsedValue.is_discarded()) setValues(value);
        else setValues(parsedValue);
        return *this;
    }

    void ApiRequest::setValues(const nlohmann::json &json) {
        constexpr auto errPrefix = "Invalid JSON-RPC request: ";
        clear();

        if (!json.contains(jk::JSONRPC)) {
            throw std::invalid_argument(errPrefix + "missing '"s.append(jk::JSONRPC).append("' field"));
        }
        const auto &jsonRpc = json[jk::JSONRPC];
        if (!jsonRpc.is_string()) {
            throw std::invalid_argument(errPrefix + "'"s.append(jk::JSONRPC).append("' must be a string"));
        }
        const auto jsonRpcStr = jsonRpc.get<std::string>();
        if (jsonRpcStr != jc::VERSION) {
            throw std::invalid_argument(
                errPrefix + "'"s.append(jk::JSONRPC).append("' must be equal '").append(jc::VERSION).append("'"));
        }

        if (!json.contains(jrqk::METHOD)) {
            throw std::invalid_argument(errPrefix + "missing '"s.append(jrqk::METHOD).append("' field"));
        }
        const auto methodJson = json[jrqk::METHOD];
        if (!methodJson.is_string() || methodJson.get<std::string>().empty()) {
            throw std::invalid_argument(errPrefix + "'"s.append(jrqk::METHOD).append("' must be a non-empty string"));
        }

        jsonrpc = jsonRpc;
        method = methodJson.get<std::string>();
        if (json.contains(jrqk::PARAMS)) params = json[jrqk::PARAMS];
        id.fromJson(json);
    }

    void ApiRequest::setValues(const std::string_view string) {
        clear();
        std::vector<std::string> splitRequest = {};
        boost::split(splitRequest, string, boost::is_any_of(" "), boost::token_compress_on);

        if (splitRequest.empty() || splitRequest[0].empty()) {
            throw std::invalid_argument("Invalid raw string request: request must have target.method or target method");
        }

        jsonrpc = jc::VERSION.data();
        params.emplace(nlohmann::json::object());
        auto &paramsJsonObject = params.value();

        size_t paramsStartIndex = 0;
        if (splitRequest[0].find('.') != std::string::npos) {
            method = splitRequest[0];
            paramsStartIndex = 1;
        } else {
            if (splitRequest.size() < 2) {
                throw std::invalid_argument(
                    "Invalid raw string request: request must have target.method or target method");
            }
            method = getTargetMethodString(splitRequest[0], splitRequest[1]);
            paramsStartIndex = 2;
        }

        id = getNextApiId();

        if (splitRequest.size() > paramsStartIndex) {
            for (size_t i = paramsStartIndex; i < splitRequest.size(); i++) {
                if (!splitRequest[i].empty()) {
                    emplaceParameter(paramsJsonObject, splitRequest[i]);
                }
            }
        }
    }

    void ApiRequest::clear() {
        jsonrpc.clear();
        method.clear();
        params.reset();
        id = ApiId();
    }

    ApiResponse::ApiResponse(const nlohmann::json &json) {
        setValues(json);
    }

    nlohmann::json ApiResponse::to_json() const {
        nlohmann::json json;

        if (id.isUndefined()) {
            throw std::invalid_argument("Invalid JSON-RPC response: response must have an ID");
        }

        json[jk::JSONRPC] = jsonrpc;
        if (result.has_value()) {
            const auto parsedValue = nlohmann::json::parse(result.value(), nullptr, false);
            if (parsedValue.is_discarded()) {
                json[jrsk::RESULT] = result.value();
            } else {
                json[jrsk::RESULT] = parsedValue;
            }
        } else if (error.has_value()) {
            json[jrsk::ERROR] = error.value().to_json();
        } else {
            throw std::invalid_argument("Invalid JSON-RPC response: response must have result or error");
        }

        if (id.hasValue()) {
            json[jk::ID] = id.value();
        } else if (id.isNull()) {
            json[jk::ID] = nullptr;
        }

        return json;
    }

    std::string ApiResponse::to_string() const {
        return nlohmann::to_string(to_json());
    }

    ApiResponse ApiResponse::operator()(const nlohmann::json &value) {
        setValues(value);
        return *this;
    }

    ApiResponse ApiResponse::operator()(std::string_view value) {
        const auto parsedValue = nlohmann::json::parse(value, nullptr, false);
        if (parsedValue.is_discarded())
            throw std::invalid_argument("ApiResponse parsing failed: string was not a valid JSON");
        setValues(parsedValue);
        return *this;
    }

    void ApiResponse::setValues(const nlohmann::json &json) {
        constexpr auto errPrefix = "Invalid JSON-RPC response: ";
        clear();

        if (!json.contains(jk::JSONRPC)) {
            throw std::invalid_argument(errPrefix + "missing '"s.append(jk::JSONRPC).append("' field"));
        }
        const auto &jsonRpc = json[jk::JSONRPC];
        if (!jsonRpc.is_string()) {
            throw std::invalid_argument(errPrefix + "'"s.append(jk::JSONRPC).append("' must be a string"));
        }
        const auto jsonRpcStr = jsonRpc.get<std::string>();
        if (jsonRpcStr != jc::VERSION) {
            throw std::invalid_argument(
                errPrefix + "'"s.append(jk::JSONRPC).append("' must be equal '").append(jc::VERSION).append("'"));
        }

        if (!json.contains(jk::ID)) {
            throw std::invalid_argument(errPrefix + "response must contain '"s.append(jk::ID).append("' field"));
        }

        if (json.contains(jrsk::RESULT) && !json.contains(jrsk::ERROR)) {
            auto &jsonResult = json[jrsk::RESULT];
            if (jsonResult.is_string()) result = jsonResult.get<std::string>();
            else result = jsonResult.dump();
        } else if (json.contains(jrsk::ERROR) && !json.contains(
                       jrsk::RESULT))
            error.emplace(
                json[jrsk::ERROR]);
        else throw std::invalid_argument(errPrefix + "response must contain either result or error"s);
        jsonrpc = jsonRpcStr;
        id.fromJson(json);
    }

    void ApiResponse::clear() {
        jsonrpc.clear();
        result.reset();
        error.reset();
        id = ApiId();
    }

    std::string getTargetMethodString(std::string_view target, std::string_view method) {
        if (target.empty()) {
            target = Constants::Common::UNDEFINED_BRACKETS;
        }
        if (method.empty()) {
            method = Constants::Common::UNDEFINED_BRACKETS;
        }
        return std::string(target) + "." + std::string(method);
    }

    std::pair<std::string, std::string> parseTargetMethodString(std::string_view targetMethodStr) {
        const auto dotPos = targetMethodStr.find('.');

        if (dotPos == std::string_view::npos || dotPos == 0 || dotPos == targetMethodStr.size() - 1) {
            throw std::invalid_argument("Invalid target.method string: "s.append(targetMethodStr));
        }

        const auto target = std::string(targetMethodStr.substr(0, dotPos));
        const auto method = std::string(targetMethodStr.substr(dotPos + 1));

        return {target, method};
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
            case ErrorCodes::NOT_IMPLEMENTED:
                return "Not implemented";
            case ErrorCodes::NOT_FOUND:
                return "Not found";
            case ErrorCodes::UNKNOWN_ERROR:
                return "Unknown error";
            default:
                return "Undefined error";
        }
    }

    nlohmann::json parseValue(std::string_view value) {
        auto isDigitChar = [](const unsigned char character) {
            return std::isdigit(character) != 0;
        };

        auto isInteger = [&](const std::string_view string) {
            if (string.empty()) return false;

            size_t iter = 0;
            if (string[iter] == '-') {
                if (string.size() == 1) return false;
                iter = 1;
            }

            for (; iter < string.size(); ++iter) {
                if (!isDigitChar(static_cast<unsigned char>(string[iter]))) return false;
            }

            return true;
        };

        auto isFloat = [&](const std::string_view string) {
            if (string.empty()) return false;

            size_t iter = 0;
            if (string[iter] == '-') {
                if (string.size() == 1) return false;
                iter = 1;
            }

            bool hasDot = false;
            bool hasDigit = false;

            for (; iter < string.size(); ++iter) {
                const auto character = static_cast<unsigned char>(string[iter]);
                if (character == '.') {
                    if (hasDot) return false; // Multiple dots not allowed
                    hasDot = true;
                    continue;
                }
                if (!isDigitChar(character)) return false;
                hasDigit = true;
            }
            return hasDot && hasDigit;
        };

        // Check int
        if (isInteger(value)) {
            try {
                return std::stoll(std::string(value));
            } catch (...) {
                // Value is not a valid int, continuing parsing attempts
            }
        }

        // Check float
        if (isFloat(value)) {
            try {
                return std::stod(std::string(value));
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
            if (!params.contains(JsonRpcStrings::ParamsKeys::ARGS) ||
                !params[JsonRpcStrings::ParamsKeys::ARGS].is_array()) {
                params[JsonRpcStrings::ParamsKeys::ARGS] = nlohmann::json::array();
            }
            params[JsonRpcStrings::ParamsKeys::ARGS].push_back(parseValue(parameter));
            return;
        }

        const std::string_view key = parameter.substr(0, equalsPos);
        std::string_view value = parameter.substr(equalsPos + 1);

        if (key.empty()) {
            throw std::invalid_argument("Invalid raw string request: empty key in key=value parameter");
        }

        const auto parsedValue = nlohmann::json::parse(value, nullptr, false);
        if (parsedValue.is_discarded() && value.find(',') != std::string_view::npos) {
            std::vector<std::string> values;
            boost::split(values, value, boost::is_any_of(","));
            params[key] = parseVector(values);
        } else if (parsedValue.is_discarded()) {
            params[key] = parseValue(value);
        } else {
            params[key] = parsedValue;
        }
    }

    static_assert(std::is_same_v<apiId_t, std::uint64_t>,
                  "API::getNextApiId() relies on 64-bit apiId_t to make wrap-around practically impossible. "
                  "Modify the implementation if the type changes");

    static_assert(std::atomic<apiId_t>::is_always_lock_free,
                  "Unsupported platform: std::atomic<apiId_t> must be lock-free, "
                  "as required by the current API::getNextApiId() implementation");

    apiId_t getNextApiId() {
        static std::atomic<apiId_t> id = 1;
        return id.fetch_add(1, std::memory_order::relaxed);
    }
}
