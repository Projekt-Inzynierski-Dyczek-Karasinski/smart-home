#pragma once

#include "api_client.h"
#include "socket_client.h"
#include "service_manager/service_manager.h"
#include "async_logger.h"
#include "constants.h"

#include <atomic>
#include <memory>
#include <optional>
#include <thread>

#include <boost/asio.hpp>
#include <crow.h>
#include <crow/middlewares/cors.h>

namespace SmartHomeWebServer {
    namespace ba = boost::asio;
    namespace bs = boost::system;
    namespace si = SmartHome::IPC;
    namespace su = SmartHome::Utils;
    namespace sc = SmartHome::Constants;

    using namespace std::chrono_literals;

    class WebServer {
    public:
        struct Config {
            std::string serviceName = sc::DefaultServiceNames::WEB.data();

            /// HTTP server config
            struct Http {
                int port = 80;
                std::string staticRoot = sc::DefaultPaths::WEB_ROOT.data();
            } http;

            /// IPC connection to core
            ApiClient::Config apiClient{};

            /// CORS config
            struct Cors {
                std::string allowedOrigin = "http://localhost";
            } cors;
        };

        /**
         * @brief Get singleton instance.
         */
        static WebServer &Instance();

        WebServer(const WebServer &) = delete;

        WebServer &operator=(const WebServer &) = delete;

        /**
         * @brief Initialize WebServer with provided configuration.
         *
         * @param config Configuration parameters.
         * @param logger Shared pointer to configured logger.
         *
         * @return true if successful, false on error.
         */
        bool initialize(const Config &config, const std::shared_ptr<su::Logger> &logger);

        /**
         * @brief Start WebServer and block on Crow main loop.
         *
         * @pre initialize() must be called successfully first.
         */
        void run();

        /**
         * @brief Request graceful shutdown.
         */
        void shutdown();

        /**
         * @brief Check if WebServer is currently running.
         */
        [[nodiscard]] bool isRunning() const;

        std::shared_ptr<su::AsyncLogger> pLogger;

    private:
        /**
         * @brief Private constructor for singleton pattern.
         */
        WebServer() = default;

        /**
         * @brief Private destructor for singleton pattern, starts graceful shutdown.
         */
        ~WebServer();

        /**
         * @brief Handle signals for graceful shutdown.
         *
         * @param ec Error code from signal wait operation.
         * @param signal Signal number that was received.
         */
        void signalHandler(const bs::error_code &ec, int signal);

        /**
         * @brief Register HTTP routes to Crow app.
         */
        void registerRoutes() const;

        static constexpr std::array msSIGNALS_TO_HANDLE = {SIGINT, SIGTERM, SIGHUP};
        static constexpr auto msSHUTDOWN_TIMEOUT = 2500ms;

        Config mConfig;

        std::unique_ptr<su::ServiceManager> mpService;
        std::unique_ptr<ApiClient> mpApiClient;
        std::unique_ptr<crow::App<crow::CORSHandler> > mpApp;

        // ApiClient socket IO
        ba::io_context mSocketClientIoContext;
        std::optional<std::thread> mSocketClientThread;
        std::optional<ba::executor_work_guard<ba::io_context::executor_type> > mSocketClientGuard;

        // Utility IO (signals, logger, timers)
        ba::io_context mUtilityIoContext;
        std::optional<std::thread> mUtilityThread;
        std::optional<ba::executor_work_guard<ba::io_context::executor_type> > mUtilityGuard;
        std::optional<ba::signal_set> mSignals;

        std::atomic_bool mIsRunning{false};
        std::atomic_bool mIsInitialized{false};
    };
}
