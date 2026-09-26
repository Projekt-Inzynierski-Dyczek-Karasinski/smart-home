#include "api_test.h"

#include <thread>
#include <unordered_set>

#include <gmock/gmock.h>


namespace SmartHome::Tests {
    using namespace std::string_view_literals;

    // region Helpers

    std::vector<nlohmann::json> ApiTest::validErrorJsons() {
        return {
            nlohmann::json{{"code", -32700}, {"message", "Parse error"}, {"data", "parse error: details"}},
            nlohmann::json{{"code", -32603}, {"message", "Internal error"}},
            nlohmann::json{{"code", 1}, {"message", "Custom error"}},
        };
    }

    std::vector<nlohmann::json> ApiTest::validRequestJsons() {
        return {
            R"json({
                "id":33,"jsonrpc":"2.0","method":"core.get",
                "params":{"args":[2,1],"module_id":1,"type":"device_readings"}
            })json"_json,
            R"json({
                "id":79,"jsonrpc":"2.0","method":"database.get","params":{"columns":["id","name"],"table":"modules"}
            })json"_json,
            R"json({
                "id":null,"jsonrpc":"2.0","method":"core.get","params":{"args":[1],"module_id":1,"type":"sensor_value"}
                })json"_json,
            // Notification
            R"json({
                "jsonrpc":"2.0","method":"core.notify",
                "params":{"data":{"operation":"UPDATE"},"type":"modules_changed"}
            })json"_json,
        };
    }

    std::vector<nlohmann::json> ApiTest::validResponseJsons() {
        return {
            R"json({
                "id":33,"jsonrpc":"2.0","result":{
                "device_readings":[{"device_id":1,"stale":true,"value":[51.599609375,984.2906494140625]}]}
            })json"_json,
            R"json({
                "id":56,"jsonrpc":"2.0","result":[51.56640625,984.2255249023438,24.6299991607666]
            })json"_json,
            R"json({
                "id":54,"jsonrpc":"2.0","result":100
            })json"_json,
            // String results are stored without dump() - separate path in setValues()
            R"json({"id":54,"jsonrpc":"2.0","result":"ok"})json"_json,
            R"json({
                "id":79,"jsonrpc":"2.0","error":{
                "code":-32001,"message":"Module runtime error","data":"module 2 not responding"}
            })json"_json,
            R"json({"id":null,"jsonrpc":"2.0","error":{"code":-32700,"message":"Parse error"}})json"_json,
        };
    }


    // endregion

    // region ApiId
    TEST_F(ApiTest, ApiIdHasValue) {
        const auto apiId = API::ApiId(msTEST_ID);

        ASSERT_TRUE(apiId.hasValue());
        EXPECT_FALSE(apiId.isUndefined());
        EXPECT_FALSE(apiId.isNull());

        EXPECT_EQ(apiId.value(), msTEST_ID);
    }

    TEST_F(ApiTest, ApiIdIsNull) {
        const auto apiId = API::ApiId(nullptr);

        EXPECT_FALSE(apiId.hasValue());
        EXPECT_FALSE(apiId.isUndefined());
        EXPECT_TRUE(apiId.isNull());

        EXPECT_THAT([&]{std::ignore = apiId.value();},
                    ::testing::ThrowsMessage<std::runtime_error>(::testing::HasSubstr("value")));
    }

    TEST_F(ApiTest, ApiIdIsUndefined) {
        constexpr API::ApiId apiId;

        EXPECT_FALSE(apiId.hasValue());
        EXPECT_TRUE(apiId.isUndefined());
        EXPECT_FALSE(apiId.isNull());

        EXPECT_THAT([&]{std::ignore = apiId.value();},
                    ::testing::ThrowsMessage<std::runtime_error>(::testing::HasSubstr("value")));
    }

    TEST_F(ApiTest, ApiIdToJson) {
        const auto apiId = API::ApiId(msTEST_ID);
        const auto idJson = apiId.toJson();

        ASSERT_TRUE(idJson.contains("id"));
        EXPECT_EQ(idJson.at("id"), msTEST_ID);
    }

    TEST_F(ApiTest, NullApiIdToJson) {
        const auto apiId = API::ApiId(nullptr);
        const auto idJson = apiId.toJson();

        ASSERT_TRUE(idJson.contains("id"));
        EXPECT_EQ(idJson.at("id"), nullptr);
    }

    TEST_F(ApiTest, UndefinedApiIdToJsonThrows) {
        constexpr API::ApiId apiId;

        EXPECT_THAT([&]{std::ignore = apiId.toJson();},
                    ::testing::ThrowsMessage<std::runtime_error>(::testing::HasSubstr("undefined")));
    }

    TEST_F(ApiTest, ApiIdFromJson) {
        const auto idJson = nlohmann::json({{"id", msTEST_ID}});
        const auto apiId = API::ApiId().fromJson(idJson);

        ASSERT_TRUE(apiId.hasValue());
        EXPECT_EQ(apiId.value(), msTEST_ID);
    }

    TEST_F(ApiTest, NullApiIdFromJson) {
        const auto idJson = nlohmann::json({{"id", nullptr}});
        const auto apiId = API::ApiId().fromJson(idJson);

        EXPECT_TRUE(apiId.isNull());
    }

    TEST_F(ApiTest, UndefinedApiIdFromJson) {
        const auto idJson = nlohmann::json({{"not_id", 123}});
        const auto apiId = API::ApiId().fromJson(idJson);

        EXPECT_TRUE(apiId.isUndefined());
    }

    TEST_F(ApiTest, ApiIdFromJsonInvalidType) {
        // String "null" is a valid JSON-RPC string ID, not a null ID - rejected as unsupported string ID
        const std::array invalidIds{
            nlohmann::json::object(), nlohmann::json::array(), nlohmann::json("5"),
            nlohmann::json("null"), nlohmann::json(1.5), nlohmann::json(true)
        };

        for (const auto &invalidId: invalidIds) {
            SCOPED_TRACE("Invalid id: " + invalidId.dump());
            EXPECT_THAT([&]{std::ignore = API::ApiId().fromJson(nlohmann::json{{"id", invalidId}});},
                        ::testing::ThrowsMessage<std::runtime_error>(::testing::HasSubstr("Invalid")));
        }
    }

    TEST_F(ApiTest, ApiIdToJsonFromJsonRoundTrip) {
        const std::array apiIds{
            API::ApiId(msTEST_ID), API::ApiId(nullptr), API::ApiId(std::numeric_limits<apiId_t>::max())
        };

        for (size_t i = 0; i < apiIds.size(); i++) {
            SCOPED_TRACE("ApiIds array index " + std::to_string(i));
            EXPECT_EQ(API::ApiId().fromJson(apiIds[i].toJson()), apiIds[i]);
        }
    }

    TEST_F(ApiTest, ApiIdFromJsonOverwritesPreviousState) {
        auto apiId = API::ApiId(msTEST_ID);

        apiId.fromJson(nlohmann::json{{"id", nullptr}});
        EXPECT_EQ(apiId, API::ApiId(nullptr));

        apiId.fromJson(nlohmann::json{{"not_id", 1}});
        EXPECT_EQ(apiId, API::ApiId());
    }

    TEST_F(ApiTest, ApiIdAssignment) {
        API::ApiId apiId;

        apiId = msTEST_ID;
        EXPECT_EQ(apiId, API::ApiId(msTEST_ID));

        apiId = nullptr;
        EXPECT_EQ(apiId, API::ApiId(nullptr));
    }

    // TODO implement needed fixes and remove GTEST_SKIP()
    TEST_F(ApiTest, ApiIdFromJsonRejectsNegativeValue) {
        GTEST_SKIP() << "Fixes are needed: " << std::endl
                     << "fromJson accepts negative integers and wraps them to apiId_t";
        EXPECT_THROW(std::ignore = API::ApiId().fromJson(nlohmann::json{{"id", -1}}), std::runtime_error);
    }

    //endregion

    //region ApiError

    TEST_F(ApiTest, ApiErrorFromJsonValid) {
        const nlohmann::json errorJson = {
            {"code", -32700},
            {"message", "Parse error"},
            {"data", "parse error: details"}
        };
        API::ApiError error;

        ASSERT_NO_THROW(error = API::ApiError(errorJson));

        EXPECT_EQ(error.code, API::ErrorCodes::PARSE_ERROR);
        EXPECT_EQ(error.message, errorJson["message"].get<std::string>());
        EXPECT_EQ(error.data, errorJson["data"].get<std::string>());
    }

    TEST_F(ApiTest, ApiErrorFromJsonMissingCode) {
        const nlohmann::json errorJson = {
            {"message", "parse error"},
            {"data", "parse error: details"}
        };

        EXPECT_THAT([&]{std::ignore = API::ApiError(errorJson);}, ::testing::ThrowsMessage<std::invalid_argument>(
                        ::testing::HasSubstr("missing 'code' field")));
    }

    TEST_F(ApiTest, ApiErrorFromJsonCodeMustBeAnInteger) {
        const std::array invalidCodes{nlohmann::json("1"), nlohmann::json(1.5), nlohmann::json(nullptr)};

        for (const auto &invalidCode: invalidCodes) {
            SCOPED_TRACE("Invalid code: " + invalidCode.dump());
            const nlohmann::json errorJson = {{"code", invalidCode}, {"message", "Parse error"}};

            EXPECT_THAT([&]{std::ignore = API::ApiError(errorJson);},
                        ::testing::ThrowsMessage<std::invalid_argument>(
                            ::testing::HasSubstr("'code' must be integer")));
        }
    }

    TEST_F(ApiTest, ApiErrorFromJsonMissingMessage) {
        const nlohmann::json errorJson = {
            {"code", -32700},
            {"data", "parse error: details"}
        };

        EXPECT_THAT([&]{std::ignore = API::ApiError(errorJson);}, ::testing::ThrowsMessage<std::invalid_argument>(
                        ::testing::HasSubstr("missing 'message' field")));
    }

    TEST_F(ApiTest, ApiErrorFromJsonNonStringMessage) {
        const nlohmann::json errorJson = {
            {"code", -32700},
            {"message", 1},
            {"data", "parse error: details"}
        };

        EXPECT_THAT([&]{std::ignore = API::ApiError(errorJson);}, ::testing::ThrowsMessage<std::invalid_argument>(
                        ::testing::HasSubstr("non-empty string")));
    }

    TEST_F(ApiTest, ApiErrorFromJsonEmptyStringMessage) {
        const nlohmann::json errorJson = {
            {"code", -32700},
            {"message", ""},
            {"data", "parse error: details"}
        };

        EXPECT_THAT([&]{std::ignore = API::ApiError(errorJson);}, ::testing::ThrowsMessage<std::invalid_argument>(
                        ::testing::HasSubstr("non-empty string")));
    }

    TEST_F(ApiTest, ApiErrorFromJsonDataIsOptional) {
        const API::ApiError error(nlohmann::json{{"code", -32700}, {"message", "Parse error"}});

        EXPECT_TRUE(error.data.empty());
    }

    TEST_F(ApiTest, ApiErrorFromStringValid) {
        const nlohmann::json errorJson = {
            {"code", -32700},
            {"message", "Parse error"},
            {"data", "parse error: details"}
        };

        EXPECT_NO_THROW(API::ApiError(std::string_view(errorJson.dump())));
    }

    TEST_F(ApiTest, ApiErrorFromStringInvalidFormat) {
        constexpr std::string_view invalidErrorJson = "{Not A Valid JSON}";

        EXPECT_THAT([&]{std::ignore = API::ApiError(invalidErrorJson);},
                    ::testing::ThrowsMessage<std::invalid_argument>( ::testing::HasSubstr("not a valid JSON")));
    }

    TEST_F(ApiTest, ApiErrorFromJsonAcceptsUnknownCode) {
        const API::ApiError error(nlohmann::json{{"code", 1}, {"message", "Custom error"}});

        EXPECT_EQ(static_cast<int>(error.code), 1);
        EXPECT_EQ(error.message, "Custom error");
    }

    TEST_F(ApiTest, ApiErrorToJsonFromJsonRoundTrip) {
        for (const auto &errorJson: validErrorJsons()) {
            SCOPED_TRACE(errorJson.dump());
            EXPECT_EQ(API::ApiError(errorJson).to_json(), errorJson);
        }
    }

    TEST_F(ApiTest, ApiErrorToStringFromStringRoundTrip) {
        for (const auto &errorJson: validErrorJsons()) {
            const std::string errorString = errorJson.dump();
            SCOPED_TRACE(errorString);

            // Compare parsed JSON - string comparison would depend on key ordering
            EXPECT_EQ(nlohmann::json::parse(API::ApiError(std::string_view(errorString)).to_string()), errorJson);
        }
    }

    TEST_F(ApiTest, ApiErrorFromJsonDataMustBeAString) {
        const std::array invalidData{nlohmann::json(1), nlohmann::json::object(), nlohmann::json(nullptr)};

        for (const auto &invalidDatum: invalidData) {
            SCOPED_TRACE("Invalid data: " + invalidDatum.dump());
            const nlohmann::json errorJson = {{"code", -32700}, {"message", "Parse error"}, {"data", invalidDatum}};

            EXPECT_THAT([&]{std::ignore = API::ApiError(errorJson);},
                        ::testing::ThrowsMessage<std::invalid_argument>(::testing::HasSubstr("'data' must be a string")
                        ));
        }
    }

    TEST_F(ApiTest, ApiErrorFromJsonNonObject) {
        const std::array nonObjects{nlohmann::json("Parse error"), nlohmann::json::array({-32700, "Parse error"})};

        for (const auto &nonObject: nonObjects) {
            SCOPED_TRACE(nonObject.dump());
            EXPECT_THAT([&]{std::ignore = API::ApiError(nonObject);},
                        ::testing::ThrowsMessage<std::invalid_argument>(::testing::HasSubstr("'code'")));
        }
    }

    TEST_F(ApiTest, ApiErrorFromCodeUsesDefaultMessage) {
        const API::ApiError error(API::ErrorCodes::METHOD_NOT_FOUND, "invalid.method");

        EXPECT_EQ(error.code, API::ErrorCodes::METHOD_NOT_FOUND);
        EXPECT_EQ(error.message, API::errorCodeToString(API::ErrorCodes::METHOD_NOT_FOUND));
        EXPECT_EQ(error.data, "invalid.method");
    }

    TEST_F(ApiTest, ApiErrorToJsonOmitsEmptyData) {
        const auto errorJson = API::ApiError(API::ErrorCodes::INTERNAL_ERROR, "").to_json();

        EXPECT_FALSE(errorJson.contains("data"));
    }

    //endregion

    //region ApiRequest

    TEST_F(ApiTest, ApiRequestFromJson) {
        const API::ApiRequest request(nlohmann::json{
            {"jsonrpc", "2.0"}, {"method", "core.get"}, {"params", {{"module_id", 2}}}, {"id", msTEST_ID}
        });

        EXPECT_EQ(request.jsonrpc, "2.0");
        EXPECT_EQ(request.method, "core.get");
        ASSERT_TRUE(request.params.has_value());
        EXPECT_EQ(*request.params, (nlohmann::json{{"module_id", 2}}));
        EXPECT_EQ(request.id, API::ApiId(msTEST_ID));
    }

    TEST_F(ApiTest, ApiRequestFromJsonWithoutOptionalFields) {
        const API::ApiRequest request(nlohmann::json{{"jsonrpc", "2.0"}, {"method", "core.get"}});

        EXPECT_FALSE(request.params.has_value());
        EXPECT_TRUE(request.id.isUndefined());
    }

    TEST_F(ApiTest, ApiRequestFromJsonNullId) {
        const API::ApiRequest request(nlohmann::json{{"jsonrpc", "2.0"}, {"method", "core.get"}, {"id", nullptr}});

        EXPECT_TRUE(request.id.isNull());
    }

    TEST_F(ApiTest, ApiRequestFromJsonInvalid) {
        const std::vector<InvalidJsonCase> cases{
            {nlohmann::json{{"method", "core.get"}}, "missing 'jsonrpc' field"},
            {nlohmann::json{{"jsonrpc", 2}, {"method", "core.get"}}, "'jsonrpc' must be a string"},
            {nlohmann::json{{"jsonrpc", "1.0"}, {"method", "core.get"}}, "'jsonrpc' must be equal '2.0'"},
            {nlohmann::json{{"jsonrpc", "2.0"}}, "missing 'method' field"},
            {nlohmann::json{{"jsonrpc", "2.0"}, {"method", 5}}, "'method' must be a non-empty string"},
            {nlohmann::json{{"jsonrpc", "2.0"}, {"method", ""}}, "'method' must be a non-empty string"},
            // Batch requests have to be split by the caller
            {
                nlohmann::json::array({nlohmann::json{{"jsonrpc", "2.0"}, {"method", "core.get"}}}),
                "missing 'jsonrpc' field"
            },
        };

        for (const auto &[json, expectedMessage]: cases) {
            SCOPED_TRACE(json.dump());
            EXPECT_THAT([&]{std::ignore = API::ApiRequest(json);},
                        ::testing::ThrowsMessage<std::invalid_argument>(::testing::HasSubstr(expectedMessage)));
        }
    }

    TEST_F(ApiTest, ApiRequestFromJsonInvalidIdThrows) {
        const nlohmann::json requestJson = {{"jsonrpc", "2.0"}, {"method", "core.get"}, {"id", "xyz123"}};

        EXPECT_THAT([&]{std::ignore = API::ApiRequest(requestJson);},
                    ::testing::ThrowsMessage<std::runtime_error>(::testing::HasSubstr("Invalid ID")));
    }

    // TODO implement needed fixes and remove GTEST_SKIP()
    TEST_F(ApiTest, ApiRequestFromJsonParamsMustBeStructured) {
        GTEST_SKIP() << "Fixes are needed" << std::endl
                     << "params must be a structured value (object or array) according to JSON-RPC";
        const nlohmann::json requestJson = {{"jsonrpc", "2.0"}, {"method", "core.get"}, {"params", 5}};

        EXPECT_THROW(std::ignore = API::ApiRequest(requestJson), std::invalid_argument);
    }

    TEST_F(ApiTest, ApiRequestFromJsonString) {
        const std::string requestString =
                nlohmann::json{{"jsonrpc", "2.0"}, {"method", "core.get"}, {"id", msTEST_ID}}.dump();

        const API::ApiRequest request{std::string_view(requestString)};

        EXPECT_EQ(request.method, "core.get");
        EXPECT_EQ(request.id, API::ApiId(msTEST_ID));
    }

    // TODO implement needed fixes and remove GTEST_SKIP()
    TEST_F(ApiTest, ApiRequestFromMalformedJsonStringThrows) {
        GTEST_SKIP() << "Fixes are needed" << std::endl
             << "malformed JSON falls back to raw format - '2.0' contains a dot, so the whole text becomes the method";
        EXPECT_THROW(std::ignore = API::ApiRequest(R"({"jsonrpc":"2.0","method":"core.get")"sv), std::invalid_argument);
    }

    TEST_F(ApiTest, ApiRequestFromRawString) {
        const API::ApiRequest request{"core.get"sv};

        EXPECT_EQ(request.jsonrpc, "2.0");
        EXPECT_EQ(request.method, "core.get");
        // Raw format request always has params object
        ASSERT_TRUE(request.params.has_value());
        EXPECT_EQ(*request.params, nlohmann::json::object());
        EXPECT_TRUE(request.id.hasValue());
    }

    TEST_F(ApiTest, ApiRequestFromRawStringSeparateTargetAndMethod) {
        const API::ApiRequest request{"core get"sv};

        EXPECT_EQ(request.method, "core.get");
    }

    TEST_F(ApiTest, ApiRequestFromRawStringWithParams) {
        // Double and trailing spaces are intentional
        const API::ApiRequest request{"core get  1 module_id=2 "sv};

        EXPECT_EQ(request.method, "core.get");
        ASSERT_TRUE(request.params.has_value());
        EXPECT_EQ(*request.params, (nlohmann::json{{"args", nlohmann::json::array({1})}, {"module_id", 2}}));
    }

    TEST_F(ApiTest, ApiRequestFromRawStringInvalid) {
        const std::array invalidRequests{""sv, "   "sv, "core"sv};

        for (const auto invalidRequest: invalidRequests) {
            SCOPED_TRACE("Input: \"" + std::string(invalidRequest) + "\"");
            EXPECT_THAT([&]{std::ignore = API::ApiRequest(invalidRequest);},
                        ::testing::ThrowsMessage<std::invalid_argument>(::testing::HasSubstr("target.method")));
        }
    }

    TEST_F(ApiTest, ApiRequestFromRawStringAssignsIncreasingIds) {
        const API::ApiRequest first{"core.get"sv};
        const API::ApiRequest second{"core.set"sv};

        ASSERT_TRUE(first.id.hasValue());
        ASSERT_TRUE(second.id.hasValue());
        // getNextApiId() counter is shared by all tests - assert relative order only
        EXPECT_GT(second.id.value(), first.id.value());
    }

    TEST_F(ApiTest, ApiRequestFromJsonToJsonRoundTrip) {
        for (const auto &requestJson: validRequestJsons()) {
            SCOPED_TRACE(requestJson.dump());
            EXPECT_EQ(API::ApiRequest(requestJson).to_json(), requestJson);
        }
    }

    TEST_F(ApiTest, ApiRequestFromStringToStringRoundTrip) {
        for (const auto &requestJson: validRequestJsons()) {
            const std::string requestString = requestJson.dump();
            SCOPED_TRACE(requestString);

            const API::ApiRequest request{std::string_view(requestString)};

            EXPECT_EQ(nlohmann::json::parse(request.to_string()), requestJson);
        }
    }

    TEST_F(ApiTest, ApiRequestReparseClearsPreviousState) {
        API::ApiRequest request(nlohmann::json{
            {"jsonrpc", "2.0"}, {"method", "core.get"}, {"params", {{"module_id", 2}}}, {"id", 7}
        });

        std::ignore = request(nlohmann::json{{"jsonrpc", "2.0"}, {"method", "core.set"}});

        EXPECT_EQ(request.method, "core.set");
        EXPECT_FALSE(request.params.has_value());
        EXPECT_TRUE(request.id.isUndefined());
    }

    TEST_F(ApiTest, ApiRequestReparseFromRawString) {
        API::ApiRequest request(nlohmann::json{{"jsonrpc", "2.0"}, {"method", "core.get"}});

        std::ignore = request("core set"sv);

        EXPECT_EQ(request.method, "core.set");
        ASSERT_TRUE(request.params.has_value());
        EXPECT_TRUE(request.id.hasValue());
    }

    //endregion

    //region ApiResponse

    TEST_F(ApiTest, ApiResponseFromJsonResult) {
        const API::ApiResponse response(R"json({"id":54,"jsonrpc":"2.0","result":100})json"_json);

        EXPECT_EQ(response.jsonrpc, "2.0");
        EXPECT_TRUE(response.result.has_value());
        EXPECT_FALSE(response.error.has_value());
        EXPECT_EQ(response.id, API::ApiId(54));
    }

    TEST_F(ApiTest, ApiResponseFromJsonError) {
        const API::ApiResponse response(
            R"json({"id":null,"jsonrpc":"2.0","error":{"code":-32700,"message":"Parse error"}})json"_json);

        EXPECT_FALSE(response.result.has_value());
        ASSERT_TRUE(response.error.has_value());
        EXPECT_EQ(response.error->code, API::ErrorCodes::PARSE_ERROR);
        EXPECT_EQ(response.error->message, "Parse error");
        EXPECT_TRUE(response.id.isNull());
    }

    TEST_F(ApiTest, ApiResponseFromJsonInvalid) {
        const std::vector<InvalidJsonCase> cases{
            {R"json({"id":54,"result":100})json"_json, "missing 'jsonrpc' field"},
            {R"json({"id":54,"jsonrpc":2,"result":100})json"_json, "'jsonrpc' must be a string"},
            {R"json({"id":54,"jsonrpc":"1.0","result":100})json"_json, "'jsonrpc' must be equal '2.0'"},
            {R"json({"jsonrpc":"2.0","result":100})json"_json, "'id'"},
            {R"json({"id":54,"jsonrpc":"2.0"})json"_json, "either result or error"},
            {
                R"json({
                    "id":54,"jsonrpc":"2.0","result":100,"error":{"code":-32603,"message":"Internal error"}
                })json"_json,
                "either result or error"
            },
            // Batch responses have to be split by the caller
            {R"json([{"id":54,"jsonrpc":"2.0","result":100}])json"_json, "missing 'jsonrpc' field"},
        };

        for (const auto &[json, expectedMessage]: cases) {
            SCOPED_TRACE(json.dump());
            EXPECT_THAT([&]{std::ignore = API::ApiResponse(json);},
                        ::testing::ThrowsMessage<std::invalid_argument>(::testing::HasSubstr(expectedMessage)));
        }
    }

    TEST_F(ApiTest, ApiResponseFromJsonInvalidErrorObjectThrows) {
        const auto responseJson = R"json({"id":54,"jsonrpc":"2.0","error":{"message":"Internal error"}})json"_json;

        EXPECT_THAT([&]{std::ignore = API::ApiResponse(responseJson);},
                    ::testing::ThrowsMessage<std::invalid_argument>(::testing::HasSubstr("missing 'code' field")));
    }

    TEST_F(ApiTest, ApiResponseFromJsonInvalidIdThrows) {
        const auto responseJson = R"json({"id":"abc","jsonrpc":"2.0","result":100})json"_json;

        EXPECT_THAT([&]{std::ignore = API::ApiResponse(responseJson);},
                    ::testing::ThrowsMessage<std::runtime_error>(::testing::HasSubstr("Invalid ID")));
    }

    TEST_F(ApiTest, ApiResponseFromStringInvalidFormat) {
        API::ApiResponse response;

        EXPECT_THAT([&]{std::ignore = response("{Not A Valid JSON}"sv);},
                    ::testing::ThrowsMessage<std::invalid_argument>(::testing::HasSubstr("not a valid JSON")));
    }

    TEST_F(ApiTest, ApiResponseToJsonFromFields) {
        API::ApiResponse response;
        response.id = msTEST_ID;
        response.error = API::ApiError(API::ErrorCodes::NOT_FOUND, "module 3");

        EXPECT_EQ(response.to_json(), (nlohmann::json{
                      {"jsonrpc", "2.0"},
                      {"error", {
                      {"code", -32005},
                      {"message", API::errorCodeToString(API::ErrorCodes::NOT_FOUND)},
                      {"data", "module 3"}
                      }},
                      {"id", msTEST_ID}
                      }));
    }

    TEST_F(ApiTest, ApiResponseToJsonWithUndefinedIdThrows) {
        API::ApiResponse response;
        response.result = "ok";

        EXPECT_THAT([&]{std::ignore = response.to_json();},
                    ::testing::ThrowsMessage<std::invalid_argument>(::testing::HasSubstr("ID")));
    }

    TEST_F(ApiTest, ApiResponseToJsonWithoutResultOrErrorThrows) {
        API::ApiResponse response;
        response.id = msTEST_ID;

        EXPECT_THAT([&]{std::ignore = response.to_json();},
                    ::testing::ThrowsMessage<std::invalid_argument>(::testing::HasSubstr("result or error")));
    }


    // TODO implement needed fixes and remove GTEST_SKIP()
    TEST_F(ApiTest, ApiResponseToJsonWithResultAndErrorThrows) {
        GTEST_SKIP() << "Fixes are needed" << std::endl
                     << "to_json() silently prefers result when both result and error are set";
        API::ApiResponse response;
        response.id = msTEST_ID;
        response.result = "ok";
        response.error = API::ApiError(API::ErrorCodes::INTERNAL_ERROR, "");

        EXPECT_THROW(std::ignore = response.to_json(), std::invalid_argument);
    }

    TEST_F(ApiTest, ApiResponseFromJsonToJsonRoundTrip) {
        for (const auto &responseJson: validResponseJsons()) {
            SCOPED_TRACE(responseJson.dump());
            EXPECT_EQ(API::ApiResponse(responseJson).to_json(), responseJson);
        }
    }

    TEST_F(ApiTest, ApiResponseFromStringToStringRoundTrip) {
        for (const auto &responseJson: validResponseJsons()) {
            const std::string responseString = responseJson.dump();
            SCOPED_TRACE(responseString);
            API::ApiResponse response;

            std::ignore = response(std::string_view(responseString));

            EXPECT_EQ(nlohmann::json::parse(response.to_string()), responseJson);
        }
    }

    // TODO implement needed fixes and remove GTEST_SKIP()
    TEST_F(ApiTest, ApiResponseRoundTripPreservesStringResultType) {
        GTEST_SKIP() << "Fixes are needed" << std::endl
             << "string result that is valid JSON text changes type in to_json() (see ApiResponse::result)";
        for (const auto *stringResult: {"123", "true", "null", "[1,2]"}) {
            const nlohmann::json responseJson = {{"jsonrpc", "2.0"}, {"result", stringResult}, {"id", 54}};
            SCOPED_TRACE(responseJson.dump());

            EXPECT_EQ(API::ApiResponse(responseJson).to_json(), responseJson);
        }
    }

    TEST_F(ApiTest, ApiResponseReparseClearsPreviousState) {
        API::ApiResponse response(R"json({"id":54,"jsonrpc":"2.0","result":100})json"_json);

        std::ignore = response(
            R"json({"id":null,"jsonrpc":"2.0","error":{"code":-32700,"message":"Parse error"}})json"_json);

        EXPECT_FALSE(response.result.has_value());
        EXPECT_TRUE(response.error.has_value());
        EXPECT_TRUE(response.id.isNull());
    }

    //endregion

    //region ApiHelperFunctions

    TEST_F(ApiTest, GetTargetMethodString) {
        const std::string undefined = "<undefined>"; // Constants::Common::UNDEFINED_BRACKETS

        EXPECT_EQ(API::getTargetMethodString("core", "get"), "core.get");
        EXPECT_EQ(API::getTargetMethodString("", "get"), undefined + ".get");
        EXPECT_EQ(API::getTargetMethodString("core", ""), "core." + undefined);
    }

    TEST_F(ApiTest, ParseTargetMethodStringValidUse) {
        const auto parsedTargetMethodStr = std::pair<std::string, std::string>{"core", "get"};
        EXPECT_EQ(API::parseTargetMethodString("core.get"), parsedTargetMethodStr);
    }

    TEST_F(ApiTest, ParseTargetMethodStringInvalidFormat) {
        for (const auto invalid: {""sv, "."sv, "coreget"sv, ".get"sv, "core."sv}) {
            SCOPED_TRACE("Input: \"" + std::string(invalid) + "\"");
            EXPECT_THROW(std::ignore = API::parseTargetMethodString(invalid), std::invalid_argument);
        }
    }

    TEST_F(ApiTest, ErrorCodeToString) {
        EXPECT_EQ(API::errorCodeToString(API::ErrorCodes::METHOD_NOT_FOUND), "Method not found");
        EXPECT_EQ(API::errorCodeToString(static_cast<API::ErrorCodes>(1)), "Undefined error");
    }

    TEST_F(ApiTest, ParseValue) {
        struct ParseValueCase {
            std::string_view input;
            nlohmann::json expected;
        };
        const std::vector<ParseValueCase> cases{
            {"42", 42}, {"-7", -7}, {"0", 0},
            {"1.5", 1.5}, {"-0.25", -0.25}, {".5", 0.5}, {"5.", 5.0},
            {"true", true}, {"false", false},
            {"True", "True"}, {"sensor_value", "sensor_value"}, {"1.2.3", "1.2.3"}, {"-", "-"}, {"", ""},
        };

        for (const auto &[input, expected]: cases) {
            SCOPED_TRACE("Input: \"" + std::string(input) + "\"");
            const auto result = API::parseValue(input);

            ASSERT_EQ(result.type(), expected.type());
            EXPECT_EQ(result, expected);
        }
    }

    TEST_F(ApiTest, ParseVector) {
        EXPECT_EQ(API::parseVector({"1", "id", "2.5", "true"}), nlohmann::json::array({1, "id", 2.5, true}));
        EXPECT_EQ(API::parseVector({}), nlohmann::json::array());
    }

    TEST_F(ApiTest, EmplaceParameter) {
        struct EmplaceParameterCase {
            std::vector<std::string_view> parameters;
            nlohmann::json expected;
        };
        const std::vector<EmplaceParameterCase> cases{
            // Positional arguments are appended to "args"
            {{"2", "1"}, R"json({"args":[2,1]})json"_json},
            // key=value: JSON first, then comma-separated list, then parseValue
            {{"module_id=1"}, R"json({"module_id":1})json"_json},
            {
                {R"(module_info={"logic_address":2,"rf_channel":2})"},
                R"json({"module_info":{"logic_address":2,"rf_channel":2}})json"_json
            },
            {{"columns=id,name"}, R"json({"columns":["id","name"]})json"_json},
            {{"type=sensor_value"}, R"json({"type":"sensor_value"})json"_json},
            // Edge cases
            {{"name="}, R"json({"name":""})json"_json},
            {{"path=a=b"}, R"json({"path":"a=b"})json"_json},
            {{"module_id=1", "module_id=2"}, R"json({"module_id":2})json"_json},
            {{""}, nlohmann::json::object()},
        };

        for (size_t i = 0; i < cases.size(); i++) {
            SCOPED_TRACE("Case index " + std::to_string(i));
            auto params = nlohmann::json::object();

            for (const auto parameter: cases[i].parameters) {
                API::emplaceParameter(params, parameter);
            }

            EXPECT_EQ(params, cases[i].expected);
        }
    }

    TEST_F(ApiTest, EmplaceParameterEmptyKeyThrows) {
        auto params = nlohmann::json::object();

        EXPECT_THAT([&]{API::emplaceParameter(params, "=5");},
                    ::testing::ThrowsMessage<std::invalid_argument>(::testing::HasSubstr("empty key")));
    }

    TEST_F(ApiTest, GetNextApiIdIsStrictlyIncreasing) {
        const auto first = API::getNextApiId();
        const auto second = API::getNextApiId();

        // Counter is shared by all tests - assert relative order only
        EXPECT_GT(second, first);
    }

    TEST_F(ApiTest, GetNextApiIdIsUniqueAcrossThreads) {
        constexpr size_t THREAD_COUNT = 4;
        constexpr size_t IDS_PER_THREAD = 1000;

        std::vector<std::vector<apiId_t> > idsPerThread(THREAD_COUNT);

        // anonymous block for threads
        {
            std::vector<std::jthread> threads;

            for (auto &ids: idsPerThread) {
                threads.emplace_back([&ids] {
                    for (size_t i = 0; i < IDS_PER_THREAD; i++) ids.push_back(API::getNextApiId());
                });
            }
        }

        std::unordered_set<apiId_t> uniqueIds;
        for (const auto &ids: idsPerThread) uniqueIds.insert(ids.begin(), ids.end());

        EXPECT_EQ(uniqueIds.size(), THREAD_COUNT * IDS_PER_THREAD);
    }

    //endregion
}
