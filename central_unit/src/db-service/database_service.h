#pragma once

#include "database_connection_manager.h"
#include "database_api.h"
#include "socket_client.h"
#include "socket_server.h"
#include "service_manager/service_manager.h"
#include "async_logger.h"

#include <atomic>
#include <memory>
#include <optional>
#include <thread>
#include <utility>

#include <boost/asio.hpp>

namespace SmartHomeDB {
    namespace ba = boost::asio;
    namespace bs = boost::system;
    namespace si = SmartHome::IPC;
    namespace su = SmartHome::Utils;

    using namespace std::chrono_literals;

    class DatabaseClient;

    class DatabaseService {
        /// Service name
        static constexpr std::string_view msDEFAULT_SERVICE_NAME = "smarthome-databased";
    public:
        /**
         * @brief Configuration structure for DatabaseService initialization.
         */
        struct Config {
            std::string serviceName = msDEFAULT_SERVICE_NAME.data();

            DatabaseConnectionManager::Config dbConnConfig{};

            /// Default TCP config from socket server
            si::SocketServer::Config::Tcp tcp{
                .isEnabled = true, .endpointAddress = "127.0.0.1", .endpointPort = 43321
            };
            /// Default UDS config from socket server
            si::SocketServer::Config::Uds uds{
                .isEnabled = true, .endpointPath = "/var/run/smarthomed.sock"
            };

        };


        /**
        * @brief Get singleton instance of DatabaseService.
        *
        * @details Thread-safe initialization using static local variable.
        *          Instance is created on first call and reused after.
        *
        * @return Reference to DatabaseService singleton instance.
        */
        static DatabaseService &Instance();

        // Prevent copying
        DatabaseService(const DatabaseService &) = delete;

        // Prevent assignment
        DatabaseService &operator=(const DatabaseService &) = delete;

        /**
         * @brief Initialize DatabaseService with provided configuration.
         *
         * @details Creates and initializes all subsystems:
         *          - AsyncLogger with file output
         *          - Service manager
         *          - Socket client connection (UDS or TCP)
         *          - Database connection manager
         *          - Worker threads for each IO context
         *          Validates all components before marking initialized.
         *
         * @param configStruct Configuration parameters.
         * @param logger Shared pointer instance reference of configured logger.
         * @return true if successful, false on error.
         */
        bool initialize(const Config &configStruct, const std::shared_ptr<su::Logger> &logger);

        /**
         * @brief Starts DatabaseService and runs main loop.
         *
         * @details TODO !pr
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
         * @brief Check if DatabaseService is currently running.
         *
         * @return true if DatabaseService is running, false otherwise.
         */
        bool isRunning() const;

        /**
         * @brief Utility IO context getter.
         *
         * @return IO context reference.
         */
        ba::io_context &getUtilityIoContext();

        std::shared_ptr<su::Logger> mpLogger;

    private:
        DatabaseService() = default;


        ~DatabaseService();

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
        /// IPC target type for handshake
        static constexpr std::string_view msCLIENT_TARGET_TYPE = "database";

        Config mConfig;

        std::unique_ptr<su::ServiceManager> mpService;
        std::unique_ptr<si::SocketClient> mpSocketClient;
        std::unique_ptr<DatabaseApi> mpDatabaseApi;
        std::shared_ptr<DatabaseConnectionManager> mpDatabaseConnectionManager;
        std::shared_ptr<DatabaseClient> mpDatabaseClient;

        // IPC socket client resources
        ba::io_context mSocketClientIoContext;
        std::optional<std::thread> mSocketClientThread;
        std::optional<ba::executor_work_guard<ba::io_context::executor_type> > mSocketClientGuard;


        // Database API resources
        ba::io_context mDbApiIoContext;
        std::optional<std::thread> mDbApiThread;
        std::optional<ba::executor_work_guard<ba::io_context::executor_type> > mDbApiGuard;


        // DatabaseService utilities
        /// Utility worker IO_context used for handling signals, logging and timeout timers.
        ba::io_context mUtilityIoContext;
        /// Thread running utility IO_context
        std::optional<std::thread> mUtilityThread;
        std::optional<ba::executor_work_guard<ba::io_context::executor_type> > mUtilityGuard;
        std::optional<ba::signal_set> mSignals;

        /// Main event loop used for internal logic
        ba::io_context mMainIoContext;
        /// Thread running main event loop IO_context
        std::optional<std::thread> mMainThread;
        std::optional<ba::executor_work_guard<ba::io_context::executor_type> > mMainGuard;

        std::atomic_bool mIsRunning{false};
        std::atomic_bool mIsInitialized{false};
    };
}
