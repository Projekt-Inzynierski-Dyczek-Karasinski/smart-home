#pragma once


#include "logger.h"
#include "async_logger.h"
#include "utils.h"

#include <memory>
#include <optional>
#include <string_view>

#include <boost/asio.hpp>

namespace bs = boost::system;
namespace ba = boost::asio;
namespace su = SmartHome::Utils;

namespace SmartHomeGui {
    class Gui {
    public:
        Gui();

        bool initialize(const std::shared_ptr<su::Logger> &logger);

        void run();

        void shutdown();

    private:
        /**
         * @brief Handle system signals.
         *
         * @param ec Error code from signal handler.
         * @param signal Signal number.
         */
        void signalHandler(const bs::error_code &ec, int signal);

        std::unique_ptr<SmartHome::Utils::AsyncLogger> mpLogger;

        ba::io_context mMainIoContext;
        std::optional<std::thread> mMainThread;
        std::optional<ba::executor_work_guard<ba::io_context::executor_type> > mMainGuard;

        ba::io_context mUtilityIoContext;
        std::optional<std::thread> mUtilityThread;
        std::optional<ba::executor_work_guard<ba::io_context::executor_type> > mUtilityGuard;
        std::optional<ba::signal_set> mSignals;
        /// Signals defined to handle in signalHandler
        static constexpr std::array ms_SIGNALS_TO_HANDLE = {SIGINT, SIGTERM, SIGHUP, SIGUSR1};

        /// Path to lock file
        static constexpr std::string_view ms_LOCK_FILE_PATH = "/var/lock/smarthomegui.lock";
        /// Lock file RAII wrapper instance
        std::optional<SmartHome::Utils::FileLock> lockFile;

        std::atomic_bool mIsInitialized = {false};
        std::atomic_bool mIsRunning = {false};
        std::atomic_bool mIsShuttingDown = {false};
    };
}
