#include "actions_test.h"

#include <gmock/gmock.h>

namespace SmartHome::Tests {
    void ActionsTest::SetUp() {
        mpLogger = std::make_shared<Utils::Logger>();
        mpLogger->disableConsoleLogging();
        mpLogger->disableFileLogging();

        mIsCoreRunning = true;
    }

    void ActionsTest::TearDown() {
        SCOPED_TRACE("TearDown");
        EXPECT_TRUE(mIsActionsInitialized);
        mIsActionsInitialized = false;

        mReleaseSignal.cancel();
        drain();
        mIsCoreRunning = false;
        Actions::reset();
    }

    // region Helpers
    std::size_t ActionsTest::drain() {
        mIoContext.restart();
        return mIoContext.poll();
    }

    void ActionsTest::initActions(const Actions::CommandsRegistry &registry) {
        SCOPED_TRACE("initActions");
        ASSERT_FALSE(registry.empty()) << "initActions with empty registry";

        const Actions::Config config{
            .isCoreRunning = [this] { return mIsCoreRunning; },
            .logger = mpLogger,
            .coreExecutor = mIoContext.get_executor(),
            .workerExecutor = mIoContext.get_executor(),
            .utilityExecutor = mIoContext.get_executor(),
            .commandsRegistry = registry,
            .timeProvider = mTimeProvider,
            .handleOutgoingRequests = [this](const connectionId_t conId, std::string &&message) {
                mCapturedOutgoingRequests.emplace_back(conId, message);
            },
        };

        const auto actionsInitResult = Actions::initialize(config);
        ASSERT_TRUE(actionsInitResult.has_value()) << "Action init failed: " << actionsInitResult.error();

        mIsActionsInitialized = true;
    }

    void ActionsTest::initActionsWithDefaultHandler() {
        SCOPED_TRACE("initActionsWithDefaultHandler");
        initActions({{{sai::TargetTypes::CORE, sai::MethodTypes::GET}, makeDefaultHandler()}});
    }

    ActionsTest::Handler ActionsTest::makeDefaultHandler() {
        return [this](const cmdMetaPtr pCmdMeta) -> awaitOptApiResponse {
            mHandlersExecutedCounter++;
            API::ApiResponse commandResult;
            commandResult.id = pCmdMeta->command.commandId;

            if (!pCmdMeta) {
                commandResult.error = {API::ErrorCodes::INTERNAL_ERROR, "nullptr"};
                co_return commandResult;
            }

            const auto params = ActionHelpers::requireParams(*pCmdMeta);
            if (!params.has_value()) {
                commandResult.error = params.error();
                co_return commandResult;
            }

            commandResult.result = params.value()->dump();
            co_return commandResult;
        };
    }

    ActionsTest::Handler ActionsTest::makeFailingHandler(const API::ErrorCodes code, std::string data) {
        return [this, code, data = std::move(data)](const cmdMetaPtr pCmdMeta) -> awaitOptApiResponse {
            mHandlersExecutedCounter++;
            API::ApiResponse commandResult;
            commandResult.id = pCmdMeta->command.commandId;
            commandResult.error = API::ApiError(code, data);
            co_return commandResult;
        };
    }

    ActionsTest::Handler ActionsTest::makeThrowingHandler() {
        return [this](const cmdMetaPtr) -> awaitOptApiResponse {
            mHandlersExecutedCounter++;
            throw std::runtime_error("Handler exception");
            co_return std::nullopt;
        };
    }

    ActionsTest::Handler ActionsTest::makeStallingHandler(bool startCommandTimer) {
        return [this, startCommandTimer](const cmdMetaPtr pCmdMeta) -> awaitOptApiResponse {
            mHandlersExecutedCounter++;
            if (startCommandTimer) Actions::startCommandTimeoutTimer(pCmdMeta);
            mReleaseSignal.expires_at(ba::steady_timer::time_point::max());

            bs::error_code ec;
            co_await mReleaseSignal.async_wait(ba::redirect_error(ba::use_awaitable, ec));

            if (!pCmdMeta->isPending()) co_return std::nullopt;

            API::ApiResponse response;
            response.id = pCmdMeta->command.commandId;
            response.result = "released";
            co_return response;
        };
    }

    ActionsTest::Handler ActionsTest::makeNullResponseHandler() {
        return [this](const cmdMetaPtr) -> awaitOptApiResponse {
            mHandlersExecutedCounter++;
            co_return std::nullopt;
        };
    }

    Actions::InternalApiHandler ActionsTest::captureCallback() {
        return [this](const connectionId_t conId, std::string &&res) {
            mCapturedOutgoingResponses.emplace_back(conId, res);
        };
    }

    API::InternalApi::Command ActionsTest::defaultHandlerCommand(nlohmann::json params, apiId_t id) {
        const nlohmann::json jsonRpc = {
            {"jsonrpc", "2.0"},
            {"method", "core.get"},
            {"params", params},
            {"id", id}
        };

        const API::ApiRequest apiRequest(jsonRpc);

        return API::InternalApi::Command(apiRequest);
    }

    API::InternalApi::Command ActionsTest::parseErrorCommand(const std::string &what) {
        API::InternalApi::Command command{
            API::ApiError(
                API::ErrorCodes::PARSE_ERROR,
                API::errorCodeToString(API::ErrorCodes::PARSE_ERROR).data(),
                what
            ).to_json(),
            API::ApiId(nullptr),
            API::InternalApi::Method(sai::MethodTypes::UNKNOWN),
            API::InternalApi::Target(sai::TargetTypes::UNKNOWN)
        };
        command.isNotification = false;
        return command;
    }

    API::InternalApi::Command ActionsTest::makeCommand(const std::string &method,
                                                       const nlohmann::json &params,
                                                       std::optional<apiId_t> id) {
        nlohmann::json jsonRpc = {
            {"jsonrpc", "2.0"},
            {"method", method},
            {"params", params}
        };
        if (id.has_value()) jsonRpc["id"] = id.value();

        return API::InternalApi::Command(API::ApiRequest(jsonRpc));
    }

    void ActionsTest::receiveIncomingRequest(std::vector<API::InternalApi::Command> commands,
                                             const bool isResultStructured,
                                             const connectionId_t connectionId) {
        const API::InternalApi::Request request = {
            .connectionId = connectionId,
            .commands = std::move(commands),
            .isResultStructured = isResultStructured
        };
        Actions::handleIncomingRequest(request, captureCallback());
    }

    void ActionsTest::expectSingleParsedResponse(API::ApiResponse &outResponse,
                                                 const connectionId_t expectedConnectionId) {
        ASSERT_EQ(mCapturedOutgoingResponses.size(), 1);
        const auto &[conId, raw] = mCapturedOutgoingResponses.front();
        EXPECT_EQ(conId, expectedConnectionId);
        ASSERT_TRUE(nlohmann::json::accept(raw)) << "raw: " << raw;
        outResponse = API::ApiResponse(nlohmann::json::parse(raw));
    }

    void ActionsTest::expectSingleBatchResponse(std::vector<API::ApiResponse> &outResponses,
                                                size_t expectedSize,
                                                connectionId_t expectedConnectionId) {
        ASSERT_EQ(mCapturedOutgoingResponses.size(), 1);
        const auto &[conId, raw] = mCapturedOutgoingResponses.front();
        EXPECT_EQ(conId, expectedConnectionId);

        ASSERT_TRUE(nlohmann::json::accept(raw)) << "raw batch: " << raw;
        const auto batchJson = nlohmann::json::parse(raw);
        ASSERT_TRUE(batchJson.is_array()) << "raw batch: " << raw;
        ASSERT_EQ(batchJson.size(), expectedSize) << "raw batch: " << raw;

        outResponses.clear();
        outResponses.reserve(expectedSize);
        for (const auto &entry: batchJson) {
            SCOPED_TRACE("entry: " + entry.dump());
            API::ApiResponse response;
            ASSERT_NO_THROW(response(entry));
            outResponses.push_back(response);
        }
    }

    std::future<API::ApiResponse> ActionsTest::sendOutgoingRequest(const connectionId_t connectionId,
                                                                   API::ApiRequest &&apiRequest) {
        const auto promise = std::make_shared<std::promise<API::ApiResponse> >();
        auto future = promise->get_future();
        Actions::handleOutgoingRequest(connectionId, std::move(apiRequest), promise);
        return future;
    }

    API::ApiRequest ActionsTest::makeApiRequest(const std::string &method,
                                                const nlohmann::json &params,
                                                const std::optional<apiId_t> id) {
        nlohmann::json jsonRpc = {
            {"jsonrpc", "2.0"},
            {"method", method},
            {"params", params}
        };
        if (id.has_value()) jsonRpc["id"] = id.value();
        return API::ApiRequest(jsonRpc);
    }

    API::ApiResponse ActionsTest::makeApiResponse(const apiId_t id, const std::string &result) {
        API::ApiResponse response;
        response.id = API::ApiId(id);
        response.result = result;
        return response;
    }

    //endregion

    // region Request and responses
    TEST_F(ActionsTest, ValidIncomingRequest) {
        ASSERT_NO_FATAL_FAILURE(initActionsWithDefaultHandler());
        const nlohmann::json params = {{"value", 2}};
        constexpr apiId_t id = 12;

        receiveIncomingRequest({defaultHandlerCommand(params, id)});
        ASSERT_GT(drain(), 0) << "No async work was done";
        EXPECT_GT(mHandlersExecutedCounter, 0) << "No handler run";

        API::ApiResponse response;
        ASSERT_NO_FATAL_FAILURE(expectSingleParsedResponse(response));

        EXPECT_FALSE(response.error.has_value());

        ASSERT_TRUE(response.result.has_value());
        EXPECT_EQ(response.result, params.dump());

        ASSERT_TRUE(response.id.hasValue());
        EXPECT_EQ(response.id.value(), id);
    }

    TEST_F(ActionsTest, ErrorResponse) {
        constexpr auto errorCode = API::ErrorCodes::INTERNAL_ERROR;
        const std::string errorData = "handler failed";

        ASSERT_NO_FATAL_FAILURE(initActions({
            {{sai::TargetTypes::CORE, sai::MethodTypes::GET}, makeFailingHandler(errorCode, errorData)}
            }));

        constexpr apiId_t id = 12;
        const nlohmann::json params = {{"value", 2}};

        receiveIncomingRequest({defaultHandlerCommand(params, id)});
        ASSERT_GT(drain(), 0) << "No async work was done";
        EXPECT_GT(mHandlersExecutedCounter, 0) << "No handler run";

        API::ApiResponse response;
        ASSERT_NO_FATAL_FAILURE(expectSingleParsedResponse(response));

        EXPECT_FALSE(response.result.has_value());

        ASSERT_TRUE(response.error.has_value());
        EXPECT_EQ(response.error.value().code, errorCode);
        EXPECT_EQ(response.error.value().message, API::errorCodeToString(errorCode));
        EXPECT_EQ(response.error.value().data, errorData);

        ASSERT_TRUE(response.id.hasValue());
        EXPECT_EQ(response.id.value(), id);
    }

    TEST_F(ActionsTest, MultiCommandRequest) {
        ASSERT_NO_FATAL_FAILURE(initActions({
            {{sai::TargetTypes::CORE, sai::MethodTypes::GET}, makeDefaultHandler()},
            {{sai::TargetTypes::MODULE_MEDIATOR, sai::MethodTypes::SET}, makeDefaultHandler()},
            {{sai::TargetTypes::DATABASE, sai::MethodTypes::DELETE}, makeDefaultHandler()}
            }));

        const std::vector<std::string> methods = {"core.get", "mediator.set", "database.delete"};

        std::vector<API::InternalApi::Command> commands;
        std::map<apiId_t, nlohmann::json> expectedParams;

        for (apiId_t i = 0; i < methods.size(); i++) {
            const apiId_t id = 1 + i;
            const nlohmann::json params = {{"value", id * 2}};
            commands.push_back(makeCommand(methods[i], params, id));
            expectedParams.emplace(id, params);
        }

        receiveIncomingRequest(commands);
        ASSERT_GT(drain(), 0) << "No async work was done";
        EXPECT_EQ(mHandlersExecutedCounter, methods.size());

        std::vector<API::ApiResponse> responses;
        ASSERT_NO_FATAL_FAILURE(expectSingleBatchResponse(responses, methods.size()));

        for (const auto &response: responses) {
            ASSERT_TRUE(response.id.hasValue());
            const auto resId = response.id.value();
            SCOPED_TRACE("response id: " + std::to_string(resId));

            const auto iter = expectedParams.find(resId);
            ASSERT_NE(iter, expectedParams.end()) << "no command with matching id";

            EXPECT_FALSE(response.error.has_value());
            ASSERT_TRUE(response.result.has_value());
            EXPECT_EQ(response.result.value(), iter->second.dump());
        }
    }

    TEST_F(ActionsTest, EmptyCommandsRequestRejected) {
        ASSERT_NO_FATAL_FAILURE(initActionsWithDefaultHandler());

        receiveIncomingRequest({});
        ASSERT_EQ(mCapturedOutgoingResponses.size(), 1) << "Must be rejected synchronously";
        EXPECT_EQ(mHandlersExecutedCounter, 0) << "No handler should run";

        API::ApiResponse response;
        ASSERT_NO_FATAL_FAILURE(expectSingleParsedResponse(response));

        ASSERT_TRUE(response.error.has_value());
        EXPECT_EQ(response.error.value().code, API::ErrorCodes::INVALID_REQUEST);
        EXPECT_FALSE(response.id.hasValue());

        EXPECT_EQ(drain(), 0) << "Async work was done";
    }

    TEST_F(ActionsTest, RequestWithUnknownTarget) {
        ASSERT_NO_FATAL_FAILURE(initActionsWithDefaultHandler());
        constexpr apiId_t id = 12;
        const nlohmann::json params = {{"value", 2}};

        receiveIncomingRequest({makeCommand("InvalidTarget.get", params, id)});
        ASSERT_GT(drain(), 0) << "No async work was done";
        EXPECT_EQ(mHandlersExecutedCounter, 0) << "No handler should run";

        API::ApiResponse response;
        ASSERT_NO_FATAL_FAILURE(expectSingleParsedResponse(response));

        ASSERT_TRUE(response.error.has_value());
        EXPECT_EQ(response.error.value().code, API::ErrorCodes::INVALID_PARAMS);
        EXPECT_THAT(response.error.value().data, ::testing::HasSubstr("target"));

        ASSERT_TRUE(response.id.hasValue());
        EXPECT_EQ(response.id.value(), id);
    }

    TEST_F(ActionsTest, RequestWithUnknownMethod) {
        ASSERT_NO_FATAL_FAILURE(initActionsWithDefaultHandler());
        constexpr apiId_t id = 12;
        const nlohmann::json params = {{"value", 2}};

        receiveIncomingRequest({makeCommand("core.InvalidMethod", params, id)});
        ASSERT_GT(drain(), 0) << "No async work was done";
        EXPECT_EQ(mHandlersExecutedCounter, 0) << "No handler should run";

        API::ApiResponse response;
        ASSERT_NO_FATAL_FAILURE(expectSingleParsedResponse(response));

        ASSERT_TRUE(response.error.has_value());
        EXPECT_EQ(response.error.value().code, API::ErrorCodes::METHOD_NOT_FOUND);
        EXPECT_THAT(response.error.value().data, ::testing::HasSubstr("method"));

        ASSERT_TRUE(response.id.hasValue());
        EXPECT_EQ(response.id.value(), id);
    }

    TEST_F(ActionsTest, RequestWithUndefinedCommand) {
        ASSERT_NO_FATAL_FAILURE(initActionsWithDefaultHandler());
        constexpr apiId_t id = 12;
        const nlohmann::json params = {{"value", 2}};

        receiveIncomingRequest({makeCommand("core.set", params, id)});
        ASSERT_GT(drain(), 0) << "No async work was done";
        EXPECT_EQ(mHandlersExecutedCounter, 0) << "No handler should run";

        API::ApiResponse response;
        ASSERT_NO_FATAL_FAILURE(expectSingleParsedResponse(response));

        ASSERT_TRUE(response.error.has_value());
        EXPECT_EQ(response.error.value().code, API::ErrorCodes::INTERNAL_ERROR);
        EXPECT_THAT(response.error.value().data, ::testing::HasSubstr("command"));

        ASSERT_TRUE(response.id.hasValue());
        EXPECT_EQ(response.id.value(), id);
    }

    TEST_F(ActionsTest, ParseErrorPropagation) {
        ASSERT_NO_FATAL_FAILURE(initActionsWithDefaultHandler());
        constexpr apiId_t validId = 1;
        const nlohmann::json params = {{"value", 2}};
        const std::string parseErrorWhat = "invalid JSON-RPC";

        receiveIncomingRequest({defaultHandlerCommand(params, validId), parseErrorCommand(parseErrorWhat)});
        ASSERT_GT(drain(), 0) << "No async work was done";
        EXPECT_EQ(mHandlersExecutedCounter, 1) << "Only one command should be handled";


        std::vector<API::ApiResponse> responses;
        ASSERT_NO_FATAL_FAILURE(expectSingleBatchResponse(responses, 2));

        bool foundParseError = false;
        for (const auto &response: responses) {
            SCOPED_TRACE("response: " + response.to_string());

            if (!response.error.has_value()) continue;
            foundParseError = true;

            EXPECT_EQ(response.error.value().code, API::ErrorCodes::PARSE_ERROR);
            EXPECT_EQ(response.error.value().data, parseErrorWhat)
                << "original exception message must be passed to data";
            EXPECT_FALSE(response.id.hasValue()) << "parse error response must have null id";
        }
        EXPECT_TRUE(foundParseError);
    }

    TEST_F(ActionsTest, ThrowingHandler) {
        ASSERT_NO_FATAL_FAILURE(initActions({
            {{sai::TargetTypes::CORE, sai::MethodTypes::GET}, makeThrowingHandler()}
            }));

        constexpr apiId_t id = 1;
        const nlohmann::json params = {{"value", 2}};

        receiveIncomingRequest({defaultHandlerCommand(params, id)});
        ASSERT_GT(drain(), 0) << "No async work was done";
        EXPECT_EQ(mHandlersExecutedCounter, 1) << "Only one command should be handled";

        API::ApiResponse response;
        ASSERT_NO_FATAL_FAILURE(expectSingleParsedResponse(response));

        ASSERT_TRUE(response.error.has_value());
        EXPECT_EQ(response.error.value().code, API::ErrorCodes::INTERNAL_ERROR);
        EXPECT_THAT(response.error.value().data, ::testing::HasSubstr("Handler"));

        ASSERT_TRUE(response.id.hasValue());
        EXPECT_EQ(response.id.value(), id);
    }

    TEST_F(ActionsTest, NullResponseHandlerOmittedFromBatch) {
        ASSERT_NO_FATAL_FAILURE(initActions({
            {{sai::TargetTypes::CORE, sai::MethodTypes::GET}, makeDefaultHandler()},
            {{sai::TargetTypes::CORE, sai::MethodTypes::SET}, makeNullResponseHandler()}
            }));
        const nlohmann::json params = {{"value", 2}};

        receiveIncomingRequest({
            makeCommand("core.get", params, 1),
            makeCommand("core.set", params, 2)
        });
        ASSERT_GT(drain(), 0) << "No async work was done";
        EXPECT_EQ(mHandlersExecutedCounter, 2);

        // 2 commands but 1 response entry serialized as single object, not array
        API::ApiResponse response;
        ASSERT_NO_FATAL_FAILURE(expectSingleParsedResponse(response));
        ASSERT_TRUE(response.id.hasValue());
        EXPECT_EQ(response.id.value(), 1);
        EXPECT_EQ(response.result, params.dump());
    }

    TEST_F(ActionsTest, NotificationCommandProducesNoResponseEntry) {
        ASSERT_NO_FATAL_FAILURE(initActionsWithDefaultHandler());
        const nlohmann::json params = {{"value", 2}};

        receiveIncomingRequest({
            makeCommand("core.get", params, 1),
            makeCommand("core.get", params, std::nullopt) // notification
        });
        ASSERT_GT(drain(), 0) << "No async work was done";
        EXPECT_EQ(mHandlersExecutedCounter, 2) << "notification must still be executed";

        API::ApiResponse response;
        ASSERT_NO_FATAL_FAILURE(expectSingleParsedResponse(response));
        ASSERT_TRUE(response.id.hasValue());
        EXPECT_EQ(response.id.value(), 1);
    }

    TEST_F(ActionsTest, AllNotificationsRequestNoCallback) {
        ASSERT_NO_FATAL_FAILURE(initActionsWithDefaultHandler());
        const nlohmann::json params = {{"value", 2}};

        receiveIncomingRequest({makeCommand("core.get", params, std::nullopt)});
        ASSERT_GT(drain(), 0) << "No async work was done";
        EXPECT_EQ(mHandlersExecutedCounter, 1);
        EXPECT_TRUE(
            mCapturedOutgoingResponses.empty()) << "request with only notifications must not produce a response";
    }

    TEST_F(ActionsTest, MixedBatchOutcome) {
        constexpr auto failCode = API::ErrorCodes::INVALID_PARAMS;
        ASSERT_NO_FATAL_FAILURE(initActions({
            {{sai::TargetTypes::CORE, sai::MethodTypes::GET}, makeDefaultHandler()},
            {{sai::TargetTypes::CORE, sai::MethodTypes::SET}, makeFailingHandler(failCode, "error")},
            {{sai::TargetTypes::CORE, sai::MethodTypes::EXECUTE}, makeStallingHandler()}
            }));
        const nlohmann::json params = {{"value", 2}};

        receiveIncomingRequest({
            makeCommand("core.get", params, 1),
            makeCommand("core.set", params, 2),
            makeCommand("core.execute", params, 3),
            makeCommand("core.get", params, std::nullopt) // notification
        });
        ASSERT_GT(drain(), 0) << "No async work was done";
        EXPECT_EQ(mHandlersExecutedCounter, 4);

        mTimeProvider.advanceBy(msCOMMAND_TIMEOUT_TESTS + 1s);
        ASSERT_GT(drain(), 0) << "clock advanced but no timeout handler ran";

        std::vector<API::ApiResponse> responses;
        ASSERT_NO_FATAL_FAILURE(expectSingleBatchResponse(responses, 3)) << "Invalid batch response";

        for (const auto &response: responses) {
            SCOPED_TRACE("response: " + response.to_string());
            ASSERT_TRUE(response.id.hasValue());
            switch (response.id.value()) {
                case 1: EXPECT_EQ(response.result, params.dump());
                    break;
                case 2:
                    ASSERT_TRUE(response.error.has_value());
                    EXPECT_EQ(response.error.value().code, failCode);
                    break;
                case 3:
                    ASSERT_TRUE(response.error.has_value());
                    EXPECT_THAT(response.error.value().data, ::testing::HasSubstr("timeout"));
                    break;
                default: FAIL() << "unexpected response id " << response.id.value();
            }
        }
    }

    // endregion

    // region Unstructured request and responses
    TEST_F(ActionsTest, UnstructuredRequestAndResponse) {
        ASSERT_NO_FATAL_FAILURE(initActionsWithDefaultHandler());
        const std::string params = "value=2";
        const nlohmann::json paramsJson = {{"value", 2}};

        API::ApiRequest apiRequest;
        const std::string unstructuredRawApiRequest = "core get " + params;
        ASSERT_NO_THROW(apiRequest(std::string_view(unstructuredRawApiRequest)));

        receiveIncomingRequest({API::InternalApi::Command(apiRequest)}, false);
        ASSERT_GT(drain(), 0) << "No async work was done";
        EXPECT_GT(mHandlersExecutedCounter, 0) << "No handler run";
        ASSERT_EQ(mCapturedOutgoingResponses.size(), 1);

        const auto &[capturedResponseConnectionId, capturedResponseStr] = mCapturedOutgoingResponses.front();
        EXPECT_EQ(capturedResponseConnectionId, msCONNECTION_ID);

        EXPECT_EQ(capturedResponseStr, paramsJson.dump())
            << "unstructured response contains bare result, no JSON-RPC structure";
    }

    TEST_F(ActionsTest, UnstructuredErrorResponse) {
        constexpr auto errorCode = API::ErrorCodes::INTERNAL_ERROR;
        const std::string errorData = "handler failed";

        ASSERT_NO_FATAL_FAILURE(initActions({
            {{sai::TargetTypes::CORE, sai::MethodTypes::GET}, makeFailingHandler(errorCode, errorData)}
            }));

        constexpr apiId_t id = 12;
        const nlohmann::json params = {{"value", 2}};

        receiveIncomingRequest({defaultHandlerCommand(params, id)}, false);
        ASSERT_GT(drain(), 0) << "No async work was done";
        EXPECT_GT(mHandlersExecutedCounter, 0) << "No handler run";
        ASSERT_EQ(mCapturedOutgoingResponses.size(), 1);

        const auto &[capturedResponseConnectionId, capturedResponseStr] = mCapturedOutgoingResponses.front();
        EXPECT_EQ(capturedResponseConnectionId, msCONNECTION_ID);

        const std::string expected = std::string(API::errorCodeToString(errorCode)) + ": " + errorData;
        EXPECT_EQ(capturedResponseStr, expected)
            << "unstructured error is formatted as \"message: data\"";
    }

    TEST_F(ActionsTest, UnstructuredResponseMissingResultAndError) {
        ASSERT_NO_FATAL_FAILURE(initActions({
            {
            {sai::TargetTypes::CORE, sai::MethodTypes::GET},
            [this](const cmdMetaPtr pCmdMeta) -> awaitOptApiResponse {
            mHandlersExecutedCounter++;
            API::ApiResponse res;
            res.id = pCmdMeta->command.commandId;
            co_return res;
            }
            }
            }));

        receiveIncomingRequest({makeCommand("core.get", {{"value", 2}}, 1)}, false);
        ASSERT_GT(drain(), 0) << "No async work was done";

        ASSERT_EQ(mCapturedOutgoingResponses.size(), 1);
        EXPECT_EQ(mCapturedOutgoingResponses.front().message,
                  "Internal error: invalid response (missing response or error data)");
    }

    // endregion

    // region Timeouts
    TEST_F(ActionsTest, CommandTimeout) {
        ASSERT_NO_FATAL_FAILURE(
            initActions({{{sai::TargetTypes::CORE, sai::MethodTypes::GET}, makeStallingHandler()}}));

        receiveIncomingRequest({makeCommand("core.get", {{"value", 2}}, 1)});
        ASSERT_GT(drain(), 0) << "handler did not start";
        EXPECT_EQ(mHandlersExecutedCounter, 1);
        EXPECT_TRUE(mCapturedOutgoingResponses.empty()) << "response before timeout";

        mTimeProvider.advanceBy(msCOMMAND_TIMEOUT_TESTS - 1s);
        ASSERT_EQ(drain(), 0) << "timeout handler ran prematurely";
        mTimeProvider.advanceBy(2s);
        ASSERT_GT(drain(), 0) << "clock advanced but no timeout handler ran";

        API::ApiResponse response;
        ASSERT_NO_FATAL_FAILURE(expectSingleParsedResponse(response));

        ASSERT_TRUE(response.error.has_value());
        EXPECT_EQ(response.error.value().code, API::ErrorCodes::INTERNAL_ERROR);
        EXPECT_THAT(response.error.value().data, ::testing::HasSubstr("timeout"));

        ASSERT_TRUE(response.id.hasValue());
        EXPECT_EQ(response.id.value(), 1);
    }

    TEST_F(ActionsTest, RequestTimeout) {
        ASSERT_NO_FATAL_FAILURE(initActions({
            {
            {sai::TargetTypes::CORE, sai::MethodTypes::GET},
            makeStallingHandler(false)
            }
            }));

        receiveIncomingRequest({makeCommand("core.get", {{"value", 2}}, 1)});
        ASSERT_GT(drain(), 0) << "handler did not start";
        EXPECT_EQ(mHandlersExecutedCounter, 1);

        mTimeProvider.advanceBy(msREQUEST_TIMEOUT_TESTS - 1s);
        ASSERT_EQ(drain(), 0) << "timeout handler ran prematurely";
        mTimeProvider.advanceBy(2s);
        ASSERT_GT(drain(), 0) << "clock advanced but no timeout handler ran";

        API::ApiResponse response;
        ASSERT_NO_FATAL_FAILURE(expectSingleParsedResponse(response));
        ASSERT_TRUE(response.error.has_value());
        EXPECT_THAT(response.error.value().data, ::testing::HasSubstr("cancelled"));
    }

    TEST_F(ActionsTest, CommandTimeoutDoesNotAffectOtherCommands) {
        ASSERT_NO_FATAL_FAILURE(initActions({
            {{sai::TargetTypes::CORE, sai::MethodTypes::GET}, makeDefaultHandler()},
            {{sai::TargetTypes::CORE, sai::MethodTypes::EXECUTE}, makeStallingHandler()}
            }));
        const nlohmann::json params = {{"value", 2}};

        receiveIncomingRequest({
            makeCommand("core.get", params, 1),
            makeCommand("core.execute", params, 2)
        });
        ASSERT_GT(drain(), 0) << "handler did not start";
        EXPECT_EQ(mHandlersExecutedCounter, 2);
        EXPECT_TRUE(mCapturedOutgoingResponses.empty()) << "request completed before stalled command resolved";

        mTimeProvider.advanceBy(msCOMMAND_TIMEOUT_TESTS + 1s);
        ASSERT_GT(drain(), 0) << "clock advanced but no timeout handler ran";

        std::vector<API::ApiResponse> responses;
        ASSERT_NO_FATAL_FAILURE(expectSingleBatchResponse(responses, 2)) << "Invalid batch response";

        for (const auto &response: responses) {
            SCOPED_TRACE("response: " + response.to_string());
            ASSERT_TRUE(response.id.hasValue());
            if (response.id.value() == 1) {
                EXPECT_EQ(response.result, params.dump()) << "non-stalling command must complete normally";
            } else {
                ASSERT_TRUE(response.error.has_value());
                EXPECT_THAT(response.error.value().data, ::testing::HasSubstr("timeout"));
            }
        }
    }

    TEST_F(ActionsTest, TimeoutVsResultRace) {
        ASSERT_NO_FATAL_FAILURE(
            initActions({{{sai::TargetTypes::CORE, sai::MethodTypes::GET}, makeStallingHandler()}}));

        receiveIncomingRequest({makeCommand("core.get", {{"value", 2}}, 1)});
        ASSERT_GT(drain(), 0) << "handler did not start";
        EXPECT_EQ(mHandlersExecutedCounter, 1);

        mTimeProvider.advanceBy(msCOMMAND_TIMEOUT_TESTS + 1s);
        EXPECT_GT(drain(), 0) << "clock advanced but no timeout handler ran";
        mReleaseSignal.cancel(); // release coroutine - finishes stalled handler
        EXPECT_GT(drain(), 0) << "stalled handler did not finish";

        EXPECT_EQ(mCapturedOutgoingResponses.size(), 1)
            << "timeout and stalled handler completion both produced output - CAS on state failed";
    }

    //endregion

    // region Outgoing traffic
    TEST_F(ActionsTest, SingleOutgoingRequestSentAsObject) {
        ASSERT_NO_FATAL_FAILURE(initActionsWithDefaultHandler());
        constexpr apiId_t requestId = 1;

        auto future = sendOutgoingRequest(msCONNECTION_ID, makeApiRequest("core.get", {{"value", 2}}, requestId));

        drain();
        EXPECT_TRUE(mCapturedOutgoingRequests.empty()) << "sent before aggregation window elapsed";

        mTimeProvider.advanceBy(msAGGREGATE_OUTGOING_TIMEOUT_TESTS + 1ms);

        ASSERT_EQ(mCapturedOutgoingRequests.size(), 1);
        EXPECT_EQ(mCapturedOutgoingRequests.front().connectionId, msCONNECTION_ID);

        const auto sentJson = nlohmann::json::parse(mCapturedOutgoingRequests.front().message);
        EXPECT_TRUE(sentJson.is_object()) << "single outgoing request must not be wrapped in an array";
        EXPECT_EQ(sentJson["id"], requestId);
    }

    TEST_F(ActionsTest, OutgoingRequestsAggregatedIntoBatch) {
        ASSERT_NO_FATAL_FAILURE(initActionsWithDefaultHandler());

        auto f1 = sendOutgoingRequest(msCONNECTION_ID, makeApiRequest("core.get", {{"value", 1}}, 1));
        mTimeProvider.advanceBy(5ms); // within the aggregation window - resets sendTimer
        drain();
        auto f2 = sendOutgoingRequest(msCONNECTION_ID, makeApiRequest("core.get", {{"value", 2}}, 2));

        mTimeProvider.advanceBy(msAGGREGATE_OUTGOING_TIMEOUT_TESTS + 1ms);

        ASSERT_EQ(mCapturedOutgoingRequests.size(), 1) << "both requests must go out in one batch";
        const auto sentJson = nlohmann::json::parse(mCapturedOutgoingRequests.front().message);
        ASSERT_TRUE(sentJson.is_array());
        EXPECT_EQ(sentJson.size(), 2);
    }

    TEST_F(ActionsTest, OutgoingRequestsSeparatedByTimeSentIndividually) {
        ASSERT_NO_FATAL_FAILURE(initActionsWithDefaultHandler());

        auto f1 = sendOutgoingRequest(msCONNECTION_ID, makeApiRequest("core.get", {{"value", 1}}, 1));
        mTimeProvider.advanceBy(msAGGREGATE_OUTGOING_TIMEOUT_TESTS + 1ms);
        ASSERT_EQ(mCapturedOutgoingRequests.size(), 1);

        auto f2 = sendOutgoingRequest(msCONNECTION_ID, makeApiRequest("core.get", {{"value", 2}}, 2));
        mTimeProvider.advanceBy(msAGGREGATE_OUTGOING_TIMEOUT_TESTS + 1ms);

        ASSERT_EQ(mCapturedOutgoingRequests.size(), 2);
        for (const auto &[_, message]: mCapturedOutgoingRequests) {
            SCOPED_TRACE("sent: " + message);
            EXPECT_TRUE(nlohmann::json::parse(message).is_object());
        }
    }

    TEST_F(ActionsTest, OutgoingAggregationIsPerConnection) {
        ASSERT_NO_FATAL_FAILURE(initActionsWithDefaultHandler());
        constexpr connectionId_t otherConnectionId = 456;

        auto f1 = sendOutgoingRequest(msCONNECTION_ID, makeApiRequest("core.get", {{"value", 1}}, 1));
        auto f2 = sendOutgoingRequest(otherConnectionId, makeApiRequest("core.get", {{"value", 2}}, 2));

        mTimeProvider.advanceBy(msAGGREGATE_OUTGOING_TIMEOUT_TESTS + 1ms);

        ASSERT_EQ(mCapturedOutgoingRequests.size(), 2) << "requests to different connections must not be merged";
        std::set<connectionId_t> connectionIds;
        for (const auto &[connectionId, _]: mCapturedOutgoingRequests) connectionIds.insert(connectionId);
        EXPECT_THAT(connectionIds, ::testing::UnorderedElementsAre(msCONNECTION_ID, otherConnectionId));
    }

    // endregion

    // region Incoming traffic
    TEST_F(ActionsTest, ValidIncomingResponseFulfillsPromise) {
        ASSERT_NO_FATAL_FAILURE(initActionsWithDefaultHandler());
        constexpr apiId_t requestId = 1;
        const std::string resultValue = "ok";

        auto future = sendOutgoingRequest(msCONNECTION_ID, makeApiRequest("core.get", {{"value", 2}}, requestId));
        mTimeProvider.advanceBy(msAGGREGATE_OUTGOING_TIMEOUT_TESTS + 1ms);
        ASSERT_EQ(mCapturedOutgoingRequests.size(), 1);

        Actions::handleIncomingResponse(msCONNECTION_ID, makeApiResponse(requestId, resultValue));
        drain();

        ASSERT_EQ(future.wait_for(0s), std::future_status::ready) << "promise was not fulfilled";
        const auto response = future.get();
        ASSERT_TRUE(response.result.has_value());
        EXPECT_EQ(response.result.value(), resultValue);
    }

    TEST_F(ActionsTest, IncomingResponseWithoutIdIgnored) {
        ASSERT_NO_FATAL_FAILURE(initActionsWithDefaultHandler());
        constexpr apiId_t requestId = 1;

        const auto future = sendOutgoingRequest(msCONNECTION_ID, makeApiRequest("core.get", {{"value", 2}}, requestId));
        mTimeProvider.advanceBy(msAGGREGATE_OUTGOING_TIMEOUT_TESTS + 1ms);

        API::ApiResponse response;
        response.id = API::ApiId(nullptr);
        response.result = "ok";
        Actions::handleIncomingResponse(msCONNECTION_ID, response);
        drain();

        EXPECT_EQ(future.wait_for(0s), std::future_status::timeout)
        << "response without id must not fulfill any promise";
    }

    TEST_F(ActionsTest, UnexpectedIncomingResponseIgnored) {
        ASSERT_NO_FATAL_FAILURE(initActionsWithDefaultHandler());
        constexpr connectionId_t unknownConnectionId = 999;

        // No pending outgoing request
        Actions::handleIncomingResponse(unknownConnectionId, makeApiResponse(1, "ok"));
        drain();

        const auto future = sendOutgoingRequest(msCONNECTION_ID, makeApiRequest("core.get", {{"value", 2}}, 1));
        mTimeProvider.advanceBy(msAGGREGATE_OUTGOING_TIMEOUT_TESTS + 1ms);

        Actions::handleIncomingResponse(msCONNECTION_ID, makeApiResponse(42, "ok"));
        drain();

        EXPECT_EQ(future.wait_for(0s), std::future_status::timeout)
        << "mismatched response must not fulfill the pending promise";
    }

    TEST_F(ActionsTest, OutgoingRequestTimeoutSetsException) {
        ASSERT_NO_FATAL_FAILURE(initActionsWithDefaultHandler());
        constexpr apiId_t requestId = 1;

        auto future = sendOutgoingRequest(msCONNECTION_ID, makeApiRequest("core.get", {{"value", 2}}, requestId));
        mTimeProvider.advanceBy(msAGGREGATE_OUTGOING_TIMEOUT_TESTS + 1ms);

        mTimeProvider.advanceBy(msREQUEST_TIMEOUT_TESTS + 1s);

        ASSERT_EQ(future.wait_for(0s), std::future_status::ready);
        EXPECT_THROW(future.get(), std::runtime_error) << "timed out promise must carry an exception";
    }

    TEST_F(ActionsTest, ResponseAfterTimeoutIgnored) {
        ASSERT_NO_FATAL_FAILURE(initActionsWithDefaultHandler());
        constexpr apiId_t requestId = 1;

        auto future = sendOutgoingRequest(msCONNECTION_ID, makeApiRequest("core.get", {{"value", 2}}, requestId));
        mTimeProvider.advanceBy(msAGGREGATE_OUTGOING_TIMEOUT_TESTS + 1ms);

        mTimeProvider.advanceBy(msREQUEST_TIMEOUT_TESTS + 1s);

        // Late response must not attempt a second set on the already broken promise
        Actions::handleIncomingResponse(msCONNECTION_ID, makeApiResponse(requestId, "late"));
        drain();

        EXPECT_THROW(future.get(), std::runtime_error);
    }


    // endregion

    // region Lifecycle
    TEST_F(ActionsTest, CoreNotRunningRequestIgnored) {
        ASSERT_NO_FATAL_FAILURE(initActionsWithDefaultHandler());
        mIsCoreRunning = false;

        receiveIncomingRequest({makeCommand("core.get", {{"value", 2}}, 1)});
        drain();

        EXPECT_EQ(mHandlersExecutedCounter, 0) << "no command may run while core is stopped";
        EXPECT_TRUE(mCapturedOutgoingResponses.empty()) << "stopped core must not produce a response";
    }

    TEST_F(ActionsTest, CoreShutdownCancelsActiveRequest) {
        ASSERT_NO_FATAL_FAILURE(
            initActions({{{sai::TargetTypes::CORE, sai::MethodTypes::EXECUTE}, makeStallingHandler()}}));

        receiveIncomingRequest({makeCommand("core.execute", {{"value", 2}}, 1)});
        ASSERT_GT(drain(), 0) << "handler did not start";
        ASSERT_TRUE(mCapturedOutgoingResponses.empty());

        Actions::onCoreShutdown();
        drain();
        mTimeProvider.advanceBy(msCLEANUP_TIMEOUT_TESTS + 1s);
        drain();

        API::ApiResponse response;
        ASSERT_NO_FATAL_FAILURE(expectSingleParsedResponse(response));
        ASSERT_TRUE(response.error.has_value());
        EXPECT_EQ(response.error.value().code, API::ErrorCodes::INTERNAL_ERROR);
        EXPECT_THAT(response.error.value().data, ::testing::HasSubstr("shutdown"));
    }

    TEST_F(ActionsTest, CoreShutdownBreaksOutgoingPromises) {
        ASSERT_NO_FATAL_FAILURE(initActionsWithDefaultHandler());

        auto future = sendOutgoingRequest(msCONNECTION_ID, makeApiRequest("core.get", {{"value", 2}}, 1));
        mTimeProvider.advanceBy(msAGGREGATE_OUTGOING_TIMEOUT_TESTS + 1ms);
        ASSERT_EQ(mCapturedOutgoingRequests.size(), 1) << "request was not sent";

        Actions::onCoreShutdown();

        ASSERT_EQ(future.wait_for(0s), std::future_status::ready);
        EXPECT_THROW(future.get(), std::runtime_error) << "pending promise must be broken on shutdown";
    }

    TEST_F(ActionsTest, ConcurrentRequestsIsolation) {
        ASSERT_NO_FATAL_FAILURE(initActions({
            {{sai::TargetTypes::CORE, sai::MethodTypes::GET}, makeDefaultHandler()},
            {{sai::TargetTypes::CORE, sai::MethodTypes::EXECUTE}, makeStallingHandler(false)}
            }));
        constexpr connectionId_t otherConnectionId = 456;
        const nlohmann::json stalledParams = {{"value", 1}};
        const nlohmann::json fastParams = {{"value", 2}};

        // Request A stalls, request B completes while A is still pending
        receiveIncomingRequest({makeCommand("core.execute", stalledParams, 1)}, true, msCONNECTION_ID);
        ASSERT_GT(drain(), 0);
        ASSERT_TRUE(mCapturedOutgoingResponses.empty());

        receiveIncomingRequest({makeCommand("core.get", fastParams, 2)}, true, otherConnectionId);
        ASSERT_GT(drain(), 0);

        ASSERT_EQ(mCapturedOutgoingResponses.size(), 1) << "only the completed request may respond";
        API::ApiResponse response;
        ASSERT_NO_FATAL_FAILURE(expectSingleParsedResponse(response, otherConnectionId));
        ASSERT_TRUE(response.id.hasValue());
        EXPECT_EQ(response.id.value(), 2);
        EXPECT_EQ(response.result, fastParams.dump()) << "result must not leak between concurrent requests";

        // Releasing A must complete it independently, on its own connection
        mReleaseSignal.cancel();
        ASSERT_GT(drain(), 0);

        ASSERT_EQ(mCapturedOutgoingResponses.size(), 2);
        EXPECT_EQ(mCapturedOutgoingResponses.back().connectionId, msCONNECTION_ID);
    }

    // endregion
}
