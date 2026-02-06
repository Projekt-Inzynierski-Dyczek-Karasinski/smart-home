#pragma once

#include "socket_server.h"
#include "service_manager/service_manager.h"
#include "async_logger.h"
#include "cache.h"
#include "api/internal_api.h"

#include <atomic>
#include <memory>
#include <optional>
#include <thread>
#include <utility>

#include <boost/asio.hpp>

namespace ba = boost::asio;
namespace bs = boost::system;

namespace SmartHome {
    using namespace std::chrono_literals;

    /**
     * @brief Core system component managing smart home central unit.
     *
     * @details Thread-safe singleton class responsible for:
     *          - System initialization and configuration
     *          - Thread pool management
     *          - IPC management
     *          - Signal handling
     *          - Core system life cycle
     */
    class Core {
    public:
        /**
         * @brief Configuration structure for Core initialization.
         */
        struct Config {
            enum IpcServerThreadCount : int {
                QUARTER_CPU_CORES = -3,
                THIRD_CPU_CORES = -2,
                HALF_CPU_CORES = -1,
                ALL_CPU_CORES = 0,
                SINGLE_THREAD = 1,
                TWO_THREADS = 2,
            };

            /** Number of TCP server threads
             * \li value < 0: ceil(all CPU cores / (|value| + 1))
             * \li value == 0: all CPU cores
             * \li value >= 1: exact thread count
             */
            int ipcServerThreads = TWO_THREADS;
            /** Number of Core main threads
             * \li value < 0: ceil(all CPU cores / (|value| + 1))
             * \li value == 0: all CPU cores
             * \li value >= 1: exact thread count
             */
            int coreMainThreads = TWO_THREADS;
            /** Number of Core worker threads
             * \li value < 0: ceil(all CPU cores / (|value| + 1))
             * \li value == 0: all CPU cores
             * \li value >= 1: exact thread count
             */
            int coreWorkerThreads = SINGLE_THREAD;

            /// Default TCP config from socket server
            IPC::SocketServer::Config::Tcp tcp;
            /// Default UDS config from socket server
            IPC::SocketServer::Config::Uds uds;
        };

        /**
         * @brief Get singleton instance of Core.
         *
         * @details Thread-safe initialization using static local variable.
         *          Instance is created on first call and reused after.
         *
         * @return Reference to Core singleton instance.
         */
        static Core &Instance();

        // Prevent copying
        Core(const Core &) = delete;

        // Prevent assignment
        Core &operator=(const Core &) = delete;

        /**
         * @brief Initialize Core with provided configuration.
         *
         * @details Sets up thread pools, IO contexts and IPC based on config.
         *          Must be called before run(). Can only be initialized once.
         *
         * @param configStruct Configuration parameters.
         * @param logger Shared pointer instance reference of configured logger.
         * @return true if successful, false on error.
         */
        bool initialize(const Config &configStruct, const std::shared_ptr<Utils::Logger> &logger);

        /**
         * @brief Starts Core subsystems and runs main loop.
         *
         * @details Begins signal handling, starts IPC and enters main loop.
         *          Must be called after initialize(). Block until shutdown() is called.
         *
         * @pre initialize() must be successfully called first.
         */
        void run();

        /**
         * @brief Request graceful shutdown.
         *
         * @details Stops main loop, signal handlers, IPC and waits for running threads to finish.
         */
        void shutdown();

        /**
         * @brief Check if core is currently running.
         *
         * @return true if core is running, false otherwise.
         */
        bool isRunning() const;


        /**
         * @brief Configuration cache getter.
         *
         * @return Reference to configuration cache instance.
         */
        ConfigCache &configCache();

        /**
         * @brief Readings cache getter.
         *
         * @return Reference to readings cache instance.
         */
        ReadingsCache &readingsCache();

        /**
         * @brief Core utility IO context getter.
         *
         * @return Core utility IO context.
         */
        ba::io_context &coreUtilityIoContext();

        /**
         * @brief Core worker IO context getter.
         *
         * @return Core worker IO context.
         */
        ba::io_context &coreWorkerIoContext();

        /**
         * @brief Core IO context getter.
         *
         * @return Core IO context.
         */
        ba::io_context &coreIoContext();

        std::shared_ptr<Utils::AsyncLogger> mpLogger; ///< Logger instance

    private:
        /**
         * @brief Private constructor for singleton pattern.
         */
        Core();

        /**
         * @brief Private destructor for singleton pattern, starts graceful shutdown.
         */
        ~Core();

        /**
         * @brief Handle system signals.
         *
         * @param ec Error code from signal handler.
         * @param signal Signal number.
         */
        void signalHandler(const bs::error_code &ec, int signal);

        /**
         * @brief Stops core thread.
         */
        void stopCoreUtilityThread();

        /**
         * @brief Parse thread count value from config to usable value for initializing thread pools.
         *
         * @details Parses values accordingly:
         *          \li value < 0: ceil(all CPU cores / (|value| + 1))
         *          \li value == 0: all CPU cores
         *          \li value >= 1: exact thread count
         *
         * @param threadCountConfigValue Thread count value read from config file.
         *
         * @return Parsed thread count value.
         */
        static uint getThreadCount(int threadCountConfigValue);

        static constexpr std::string_view ms_ServiceName = "smarthomed";

        // Configuration
        Config mConfig;

        std::unique_ptr<Utils::ServiceManager> mpService;
        std::shared_ptr<API::InternalApi> mpApi;

        // Cache
        ConfigCache mConfigCache;
        ReadingsCache mReadingsCache{mConfigCache};

        // Socket server resources
        ba::io_context mSocketServerIoContext;
        std::optional<ba::thread_pool> mSocketServerThreadPool;
        std::optional<ba::executor_work_guard<ba::io_context::executor_type> > mSocketServerGuard;
        static constexpr uint ms_HIGH_THREAD_COUNT_LIMIT = 128; ///< Limit for high thread count warning

        // Core utilities
        /// Utility worker IO_context used for handling signals, logging and timeout timers.
        ba::io_context mCoreUtilityIoContext;
        /// Thread running utility IO_context
        std::optional<std::thread> mCoreUtilityThread;
        std::optional<ba::executor_work_guard<ba::io_context::executor_type> > mCoreUtilityGuard;
        std::optional<ba::signal_set> mSignals;
        /// Signals defined to handle in signalHandler
        static constexpr std::array ms_SIGNALS_TO_HANDLE = {SIGINT, SIGTERM, SIGHUP};
        /// Shutdown timeout timer value in ms
        static constexpr auto ms_SHUTDOWN_TIMEOUT = 5000ms;

        //Core workers
        ba::io_context mCoreWorkerIoContext;
        std::optional<ba::thread_pool> mCoreWorkerThreadPool;
        std::optional<ba::executor_work_guard<ba::io_context::executor_type> > mCoreWorkerGuard;

        // Core main loop
        /// Main event loop used for async operations, request routing, dispatching work
        ba::io_context mCoreIoContext;
        /// Thread pool running main event loop IO_context
        std::optional<ba::thread_pool> mCoreThreadPool;
        std::optional<ba::executor_work_guard<ba::io_context::executor_type> > mCoreGuard;

        // State flags
        std::atomic<bool> mIsInitialized{false}; ///< Core initialization state.
        std::atomic<bool> mIsRunning{false}; ///< Main loop running state.
    };
}
