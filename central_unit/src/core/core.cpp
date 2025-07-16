#include "core.h"
#include "config.h"
#include "tcp/tcp_server.h"

#include <chrono>
#include <cmath>
#include <iostream>

#include <boost/asio.hpp>

namespace ba = boost::asio;
namespace bip = boost::asio::ip;
namespace bs = boost::system;

namespace SmartHome {
    Core &Core::Instance() {
        static Core CoreInstance;
        return CoreInstance;
    }

    Core::Core() = default;

    Core::~Core() {
        if (mIsRunning.load()) {
            shutdown();
        }
    }

    bool Core::initialize(const Config &configStruct) {
        if (mIsInitialized.load()) {
            std::cerr << "Core already initialized" << std::endl;
            return false;
        }

        std::cout << "Initializing core" << std::endl;
        // Save config
        mConfig = configStruct;

        // Create thread running io context for signal handling
        mSignalGuard.emplace(ba::make_work_guard(mSignalIoContext));
        mSignalThread = std::thread([this]() {
            mSignalIoContext.run();
        });

        // TCP server initialization if enabled in config
        if (mConfig.isTcpServerEnabled) {
            // Set tcp server thread count
            uint threadCount;

            switch (mConfig.tcpServerThreads) {
                case Config::HALF_CPU_CORES:
                    threadCount = std::ceil(std::thread::hardware_concurrency() / 2);
                    break;
                case Config::ALL_CPU_CORES:
                    threadCount = std::thread::hardware_concurrency();
                    break;
                default:
                    if (mConfig.tcpServerThreads > 128) {
                        std::cerr << "Core init warning: TCP server possible excessive thread count (" <<
                                mConfig.tcpServerThreads << " threads)" << std::endl;
                    }
                    threadCount = mConfig.tcpServerThreads;
                    break;
            }

            if (threadCount < 1) {
                std::cerr << "Core init error: TCP server thread count less than 1, setting value to 1" << std::endl;
                threadCount = 1;
            }

            //Create TCP server threads with io context
            mTcpServerThreadPool.emplace(threadCount);
            mTcpServerGuard.emplace(ba::make_work_guard(mTcpServerIoContext));
            for (size_t i = 0; i < threadCount; i++) {
                ba::post(*mTcpServerThreadPool, [this]() {
                    mTcpServerIoContext.run();
                });
            }
            IPC::TcpServer::Instance();
        }

        mIsInitialized.store(true);
        std::cout << "Initialization complete" << std::endl;

        return true;
    }

    void Core::run() {
        if (!mIsInitialized.load()) {
            std::cerr << "Core not initialized" << std::endl;
            return;
        }
        if (mIsRunning.load()) {
            std::cerr << "Core already running" << std::endl;
            return;
        }

        // Begin core
        mIsRunning.store(true);
        std::cout << "Running core" << std::endl;

        // Start signal handling
        mSignals.emplace(mSignalIoContext, SIGINT, SIGTERM); //TODO add more signals
        mSignals->async_wait([this](const bs::error_code &ec, const int sig) {
            signalHandler(ec, sig);
        });

        // Start tcp server if enabled in config
        if (mConfig.isTcpServerEnabled) {
            auto &tcpServer = IPC::TcpServer::Instance();
            if (!tcpServer.startTcpServer(&mTcpServerIoContext, mConfig.tcpServerEndpointAddress,
                                          mConfig.tcpServerEndpointPort)) {
                std::cerr << "Error tcp server failed to start" << std::endl;
                shutdown();
                return;
            }
            tcpServer.runTcpServer(&mTcpServerIoContext);
        }

        // Main loop
        while (mIsRunning.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        // Waiting for tcp server threads to finish
        if (mTcpServerThreadPool.has_value()) {
            mTcpServerThreadPool->join();
            mTcpServerThreadPool.reset();
        }

        // Stopping signal thread
        if (mSignalThread.has_value()) {
            mSignalThread->join();
            mSignalThread.reset();
        }

        std::cout << "Core finished" << std::endl;
    }

    void Core::shutdown() {
        std::cout << "Shutting down core" << std::endl;

        // Stop core main loop
        mIsRunning.store(false);

        // Stop handling signals
        if (mSignals.has_value()) {
            mSignals->cancel();
            mSignals.reset();
        }
        mSignalGuard.reset();
        mSignalIoContext.stop();

        // Stop tcp server
        if (mConfig.isTcpServerEnabled) {
            std::cout << "Stopping tcp server" << std::endl;
            auto &tcpServer = IPC::TcpServer::Instance();
            if (tcpServer.isRunning()) {
                tcpServer.stopTcpServer();
            }

            // Stop handling queued tcp server tasks
            mTcpServerGuard.reset();

            std::cout << "Waiting for threads to finish" << std::endl;
            while (mTcpServerThreadPool.has_value()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }

        std::cout << "Shutdown complete" << std::endl;
    }

    void Core::signalHandler(const boost::system::error_code &ec, const int signal) {
        if (!ec) {
            // Handle signal
            switch (signal) {
                case SIGINT:
                    Core::Instance().shutdown();
                    break;
                case SIGTERM:
                    Core::Instance().shutdown();
                    break;
                case SIGHUP:
                    std::cout << "HUP - not implemented" << std::endl;
                    //TODO implement reload
                    break;
                default:
                    //TODO handle default
                    break;
            }
        } else {
            // Handle signal error
            std::cerr << "Signal handler error: " << ec.value() << std::endl;
        }
    }
}
