#pragma once

#include "async_logger.h"
#include "socket_server.h"
#include "service_manager/service_manager.h"
#include "socket_client.h"
#include "rf_api/rf_client.h"
#include "rf_api/rf_api.h"

#include <memory>


namespace su = SmartHome::Utils;
namespace si = SmartHome::IPC;
namespace bs = boost::system;


namespace SmartHomeMediator {
    using namespace std::chrono_literals;

    /**
     * @brief Central mediator singleton managing RF communication system.
     *
     * @details Coordinates RF client, API client, service lifecycle, and signal handling.
     *          Runs multiple IO contexts on separate threads for RF operations, API communication, and system utilities.
     *          Implements graceful shutdown with configurable timeout.
     */
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
         * @details Creates and initializes all subsystems:
         *          - AsyncLogger with file output
         *          - Service manager
         *          - API client connection (UDS or TCP)
         *          - Worker threads for each IO context
         *          - RF client with hardware driver
         *          Validates all components before marking initialized.
         *
         * @param configStruct Configuration parameters.
         * @param logger Shared pointer instance reference of configured logger.
         * @return true if successful, false on error.
         */
        bool initialize(const Config &configStruct, const std::shared_ptr<su::Logger> &logger);

        /**
         * @brief Starts Mediator and runs main loop.
         *
         * @details Spawns signal handlers, initializes API routing between RfApi and ApiClient,
         *          starts RF client execution loop. Blocks on main thread until shutdown requested,
         *          then joins all worker threads in proper order.
         *
         * @pre initialize() must be successfully called first.
         */
        void run();

        /**
         * @brief Request graceful shutdown.
         *
         * @details Requests graceful shutdown of all subsystems with timeout protection.
         *          Resets work guards to allow IO contexts to finish, cancels signals, stops service,
         *          and destroys objects in dependency order. Forces IO context stop if timeout expires.
         */
        void shutdown();

        /**
         * @brief Check if Mediator is currently running.
         *
         * @return true if Mediator is running, false otherwise.
         */
        [[nodiscard]] bool isRunning() const;

        /**
         * @brief Main IO context getter.
         *
         * @return IO context reference.
         */
        ba::io_context &getIoContext();

        std::shared_ptr<su::Logger> mpLogger;

    private:
        Mediator();


        ~Mediator();

        /**
         * @brief Handle POSIX signals (SIGINT, SIGTERM, SIGHUP).
         *
         * @param ec Error code from signal wait.
         * @param signal Signal number received.
         *
         * @note Re-arms signal handler for next signal unless shutting down.
         */
        void signalHandler(const boost::system::error_code &ec, int signal);

        /// Signals handled by mediator
        static constexpr std::array msSIGNALS_TO_HANDLE = {SIGINT, SIGTERM, SIGHUP};
        /// Maximum time to wait for graceful shutdown
        static constexpr auto msSHUTDOWN_TIMEOUT = 2500ms;
        /// Service name
        static constexpr std::string_view msSERVICE_NAME = "smarthome-radiod";
        static constexpr std::string_view msCLIENT_TARGET_TYPE = "module_mediator";

        Config mConfig;

        std::unique_ptr<su::ServiceManager> mpService;
        std::unique_ptr<RfApi> mpRfApi;
        std::shared_ptr<si::SocketClient> mpApiClient;
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
