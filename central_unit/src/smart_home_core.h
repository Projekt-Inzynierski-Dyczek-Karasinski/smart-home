#ifndef SMART_HOME_CORE_H
#define SMART_HOME_CORE_H
#include <atomic>
#include <memory>
#include <optional>
#include <thread>
#include <utility>

#include <boost/asio.hpp>

#include "smart_home_tcp_server.h"

namespace ba = boost::asio;
namespace bs = boost::system;

namespace SmartHome {
    class Core {
    public:
        static Core &Instance();

        Core(const Core &) = delete;

        Core &operator=(const Core &) = delete;

        bool initialize();

        void run();

        void shutdown();

    private:
        Core();

        ~Core();

        void signalHandler(const bs::error_code &ec, int signal);

        ba::io_context mTcpServerIoContext;
        std::optional<ba::thread_pool> mTcpServerThreadPool;
        std::optional<ba::executor_work_guard<ba::io_context::executor_type>> mTcpServerGuard;

        ba::io_context mSignalIoContext;
        std::optional<std::thread> mSignalThread;
        std::optional<ba::signal_set> mSignals;
        std::optional<ba::executor_work_guard<ba::io_context::executor_type>> mSignalGuard;

        std::atomic<bool> mInitialized{false};
        std::atomic<bool> mRunning{false};
        std::atomic<bool> mFastShutdown{false};
    };
}


#endif //SMART_HOME_CORE_H
