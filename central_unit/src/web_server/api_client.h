#pragma once
#include "socket_client.h"
#include "socket_server.h"

namespace SmartHomeWebServer {
    namespace si = SmartHome::IPC;
    namespace sc = SmartHome::Constants;

    using namespace std::chrono_literals;

    /**
     * @brief ApiClient manages the connection to the SmartHome daemon, sending API requests and handling responses.
     */
    class ApiClient {
    public:
        /**
         * @brief Configuration struct for ApiClient,
         *        containing settings for TCP and UDS connections to the SmartHome daemon.
         */
        struct Config {
            /// Default TCP config from socket server
            si::SocketServer::Config::Tcp tcp{
                .isEnabled = true, .endpointAddress = "127.0.0.1", .endpointPort = 43321
            };
            /// Default UDS config from socket server
            si::SocketServer::Config::Uds uds{
                .isEnabled = true, .endpointPath = sc::DefaultPaths::UDS.data()
            };
        };

        /**
         * @brief Construct ApiClient with logger and io_context for async operations.
         *
         * @param pLogger Shared pointer to asynchronous logger for logging within ApiClient.
         * @param ioContext Reference to Boost.Asio io_context for managing asynchronous tasks and timers.
         */
        ApiClient(const std::shared_ptr<su::AsyncLogger> &pLogger, ba::io_context &ioContext);

        /**
         * @brief Destructor for ApiClient, ensures proper cleanup of socket client and pending requests.
         */
        ~ApiClient();

        /**
         * @brief Initialize ApiClient by connecting to SmartHome daemon via UDS or TCP and setting up message handler.
         *
         * @param config Configuration for connection (UDS/TCP).
         *
         * @return true if connection established successfully, false otherwise.
         */
        bool initialize(const Config &config);

        /**
         * @brief Send a JSON-RPC request to the SmartHome daemon and return a future for the response string.
         *
         * @param request The API request to send, containing method, params, and optional id for response correlation.
         *
         * @return A future that will hold the response string when the response is received,
         *         or an exception if an error occurs or timeout happens.
         */
        std::future<std::string> sendRequest(const sa::ApiRequest &request);

        /**
         * @brief Handle incoming messages from the SmartHome daemon, parse them as JSON-RPC requests or responses,
         *
         * @param message The raw message string received from the daemon, expected to be a JSON-RPC request or response.
         */
        void handleIncoming(const std::string &message);

        /**
         * @brief Handle incoming API responses from the SmartHome daemon, match them to pending requests using the id,
         *
         * @param response The parsed API response object received from the daemon,
         *        used to fulfill pending promises or handle notifications.
         */
        void handleIncomingResponse(const SmartHome::API::ApiResponse &response);

        /**
         * @brief Handle incoming API requests from the SmartHome daemon, used for notifications.
         *
         * @param request The parsed API request object received from the daemon,
         *                used to trigger appropriate actions or route to handlers.
         *
         * @note Not implemented yet. TODO implement
         */
        void handleIncomingRequest(const SmartHome::API::ApiRequest &request);

    private:
        ba::io_context &mIoContext;
        std::shared_ptr<su::AsyncLogger> mpLogger;

        std::unique_ptr<SmartHome::IPC::SocketClient> mpSocketClient;

        std::unordered_map<SmartHome::apiId_t, std::promise<std::string> > mPending;
        std::mutex mMutex;

        static constexpr auto msREQUEST_TIMEOUT = 15s;
    };
}
