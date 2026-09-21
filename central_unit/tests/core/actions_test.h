#pragma once
#include "actions/actions.h"
#include "common/time/testing/fake_time_provider.h"

#include <gtest/gtest.h>

namespace SmartHome::Tests {
    class ActionsTest : public ::testing::Test {
    protected:
        struct CapturedTraffic {
            connectionId_t connectionId{0};
            std::string message;
        };

        void SetUp() override;

        void TearDown() override;

        std::size_t drain();

        using Handler = Actions::CommandHandler;

        void initActions(const Actions::CommandsRegistry &registry);

        void initActionsWithDefaultHandler();

        Handler makeDefaultHandler();

        Handler makeFailingHandler(API::ErrorCodes code, std::string data);

        Handler makeThrowingHandler();

        Handler makeStallingHandler(bool startCommandTimer = true);

        Handler makeNullResponseHandler();

        Actions::InternalApiHandler captureCallback();

        static API::InternalApi::Command defaultHandlerCommand(nlohmann::json params, apiId_t id);

        static API::InternalApi::Command parseErrorCommand(const std::string &what);

        static API::InternalApi::Command makeCommand(const std::string &method,
                                                     const nlohmann::json &params,
                                                     std::optional<apiId_t> id);

        void receiveIncomingRequest(std::vector<API::InternalApi::Command> commands,
                                    bool isResultStructured = true,
                                    connectionId_t connectionId = msCONNECTION_ID);

        void expectSingleParsedResponse(API::ApiResponse &outResponse,
                                        connectionId_t expectedConnectionId = msCONNECTION_ID);

        void expectSingleBatchResponse(std::vector<API::ApiResponse> &outResponses,
                                       size_t expectedSize,
                                       connectionId_t expectedConnectionId = msCONNECTION_ID);

        static std::future<API::ApiResponse> sendOutgoingRequest(connectionId_t connectionId,
                                                                 API::ApiRequest &&apiRequest);

        static API::ApiRequest makeApiRequest(const std::string &method,
                                              const nlohmann::json &params,
                                              std::optional<apiId_t> id);

        static API::ApiResponse makeApiResponse(apiId_t id, const std::string &result);


        static constexpr auto msSTART_TIME_POINT =
                std::chrono::system_clock::time_point{
                    std::chrono::sys_days{std::chrono::April / 22 / 2026} + 12h + 34min
                };
        static constexpr connectionId_t msCONNECTION_ID = 123;
        static constexpr auto msAGGREGATE_OUTGOING_TIMEOUT_TESTS = 10ms; // msAGGREGATE_OUTGOING_TIMEOUT = 10ms
        static constexpr auto msCOMMAND_TIMEOUT_TESTS = 60s; // msCOMMAND_TIMEOUT = 60s
        static constexpr auto msREQUEST_TIMEOUT_TESTS = 5min; // msREQUEST_TIMEOUT = 5min
        static constexpr auto msCLEANUP_TIMEOUT_TESTS = 5s; // msCLEANUP_TIMEOUT = 5s


        std::shared_ptr<Utils::Logger> mpLogger;
        ba::io_context mIoContext;
        Time::Testing::FakeTimeProvider mTimeProvider{msSTART_TIME_POINT};

        size_t mHandlersExecutedCounter{0};
        std::vector<CapturedTraffic> mCapturedOutgoingResponses;
        std::vector<CapturedTraffic> mCapturedOutgoingRequests;
        bool mIsActionsInitialized{false};
        bool mIsCoreRunning{false};
        ba::steady_timer mReleaseSignal{mIoContext};
    };
}
