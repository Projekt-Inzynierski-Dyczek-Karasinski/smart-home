#pragma once

#include "ipc/smart_home_tcp_server.h"

#include <atomic>
#include <memory>
#include <optional>
#include <thread>
#include <utility>

#include <boost/asio.hpp>


namespace ba = boost::asio;
namespace bs = boost::system;

namespace SmartHome {
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
            /// Enable/disable TCP server for IPC.
            bool tcpServerEnabled = true;
            /// TCP server address.
            std::string tcpServerEndpointAddress = "127.0.0.1";
            /// TCP server port number.
            int tcpServerEndpointPort = 43321;
            /// Number of TCP server threads (-1: half CPU cores, 0: all CPU cores, n >= 1: exact thread count).
            int tcpServerThreads = -1;
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
         * @return true if successful, false on error.
         */
        bool initialize(const Config &configStruct);

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

        // Configuration
        Config mConfig;

        // TCP server resources
        ba::io_context mTcpServerIoContext;
        std::optional<ba::thread_pool> mTcpServerThreadPool;
        std::optional<ba::executor_work_guard<ba::io_context::executor_type> > mTcpServerGuard;

        // Signal handling resources
        ba::io_context mSignalIoContext;
        std::optional<std::thread> mSignalThread;
        std::optional<ba::signal_set> mSignals;
        std::optional<ba::executor_work_guard<ba::io_context::executor_type> > mSignalGuard;

        // State flags
        std::atomic<bool> mInitialized{false}; ///< Core initialization state.
        std::atomic<bool> mRunning{false}; ///< Main loop running state.
    };
}
