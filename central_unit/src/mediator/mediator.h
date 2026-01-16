#pragma once

#include "async_logger.h"
#include "socket_server.h"
#include "service_manager/service_manager.h"
#include "api_client.h"
#include "rf_client.h"
#include "rf_api.h"

#include <memory>


namespace su = SmartHome::Utils;
namespace si = SmartHome::IPC;
namespace bs = boost::system;


namespace SmartHomeMediator {
    using namespace std::chrono_literals;

    class Mediator {
    public:
        /**
         * @brief Configuration structure for Mediator initialization.
         */
        struct Config {
            /// Default TCP config from socket server
            si::SocketServer::Config::Tcp tcp{
                .isEnabled = true, .endpointAddress = "127.0.0.1", .endpointPort = 43321
            };
            /// Default UDS config from socket server
            si::SocketServer::Config::Uds uds{.isEnabled = true, .endpointPath = "/var/run/smarthomed.sock"};

            RfClient::Config rfClient{};
        };


        /**
         * @brief Get singleton instance of Mediator.
         *
         * @details Thread-safe initialization using static local variable.
         *          Instance is created on first call and reused after.
         *
         * @return Reference to Mediator singleton instance.
         */
        static Mediator &Instance();

        // Prevent copying
        Mediator(const Mediator &) = delete;

        // Prevent assignment
        Mediator &operator=(const Mediator &) = delete;

        /**
         * @brief Initialize Mediator with provided configuration.
         *
         * @details TODO !pr Sets up thread pools, IO contexts and IPC based on config.
         *              Must be called before run().! Can only be initialized once.
         *
         * @param configStruct Configuration parameters.
         * @param logger Shared pointer instance reference of configured logger.
         * @return true if successful, false on error.
         */
        bool initialize(const Config &configStruct, const std::shared_ptr<su::Logger> &logger);

        /**
         * @brief Starts Mediator and runs main loop.
         *
         * @details TODO !pr Begins signal handling, starts IPC and enters main loop.
         *          Must be called after initialize(). Block until shutdown() is called.
         *
         * @pre initialize() must be successfully called first.
         */
        void run();

        /**
         * @brief Request graceful shutdown.
         *
         * @details TODO !pr Stops main loop, signal handlers, IPC and waits for running threads to finish.
         */
        void shutdown();

        /**
         * @brief Check if Mediator is currently running.
         *
         * @return true if Mediator is running, false otherwise.
         */
        bool isRunning() const;

        ba::io_context &getIoContext();

        std::shared_ptr<su::Logger> mpLogger;

    private:
        Mediator();


        ~Mediator();

        void signalHandler(const boost::system::error_code &ec, int signal);

        Config mConfig;

        std::unique_ptr<su::ServiceManager> mpService;
        std::unique_ptr<RfApi> mpRfApi;
        std::shared_ptr<ApiClient> mpApiClient;
        std::shared_ptr<RfClient> mpRfClient;

        // IPC API client resources
        ba::io_context mApiClientIoContext;
        std::optional<std::thread> mApiClientThread;
        std::optional<ba::executor_work_guard<ba::io_context::executor_type> > mApiClientGuard;

        // RF client resources
        ba::io_context mRfClientIoContext;
        std::optional<std::thread> mRfClientThread;
        std::optional<ba::executor_work_guard<ba::io_context::executor_type> > mRfClientGuard;

        // Mediator utilities
        /// Utility worker IO_context used for handling signals, logging and timeout timers.
        ba::io_context mMediatorUtilityIoContext;
        /// Thread running utility IO_context
        std::optional<std::thread> mMediatorUtilityThread;
        std::optional<ba::executor_work_guard<ba::io_context::executor_type> > mMediatorUtilityGuard;
        std::optional<ba::signal_set> mSignals;
        /// Signals defined to handle in signalHandler
        static constexpr std::array ms_SIGNALS_TO_HANDLE = {SIGINT, SIGTERM, SIGHUP};
        static constexpr auto ms_SHUTDOWN_TIMEOUT = 5000ms;

        // Mediator main loop
        /// Main event loop used for internal logic
        ba::io_context mMediatorIoContext;
        /// Thread running main event loop IO_context
        std::optional<std::thread> mMediatorThread;
        std::optional<ba::executor_work_guard<ba::io_context::executor_type> > mMediatorGuard;

        std::atomic_bool mIsRunning{false};
        std::atomic_bool mIsInitialized{false};
    };
}
