#include "core.h"
#include "config.h"
#include "tcp/tcp_server.h"

#include <chrono>
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
        if (mRunning.load() == true) {
            shutdown();
        }
    }

    bool Core::initialize(const Config &configStruct) {
        if (mInitialized.load() == true) {
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
        if (mConfig.tcpServerEnabled) {
            // Set tcp server thread count
            uint tcpServerThreads = std::thread::hardware_concurrency();
            if (mConfig.tcpServerThreads == -1) {
                tcpServerThreads /= 2;
            } else if (mConfig.tcpServerThreads >= 1) {
                tcpServerThreads = mConfig.tcpServerThreads;
            }
            // Check for value errors
            if (tcpServerThreads < 1) {
                std::cerr << "Tcp server threads value invalid: " << tcpServerThreads << std::endl <<
                        "Changing value to 1"
                        << std::endl;
                tcpServerThreads = 1;
            }

            //Create tcp server threads with io context
            mTcpServerThreadPool.emplace(tcpServerThreads);
            mTcpServerGuard.emplace(ba::make_work_guard(mTcpServerIoContext));
            for (size_t i = 0; i < tcpServerThreads; i++) {
                ba::post(*mTcpServerThreadPool, [this]() {
                    mTcpServerIoContext.run();
                });
            }
            IPC::TcpServer::Instance();
        }

        mInitialized.store(true);
        std::cout << "Initialization complete" << std::endl;

        return true;
    }

    void Core::run() {
        if (mInitialized.load() == false) {
            std::cerr << "Core not initialized" << std::endl;
            return;
        } else if (mRunning.load() == true) {
            std::cerr << "Core already running" << std::endl;
            return;
        }

        // Begin core
        mRunning.store(true);
        std::cout << "Running core" << std::endl;

        // Start signal handling
        mSignals.emplace(mSignalIoContext, SIGINT, SIGTERM); //TODO add more signals
        mSignals->async_wait([this](const bs::error_code &ec, const int sig) {
            signalHandler(ec, sig);
        });

        // Start tcp server if enabled in config
        if (mConfig.tcpServerEnabled) {
            auto &tcpServer = IPC::TcpServer::Instance();
            if (tcpServer.startTcpServer(&mTcpServerIoContext, mConfig.tcpServerEndpointAddress,
                                         mConfig.tcpServerEndpointPort) == false) {
                std::cerr << "Error tcp server failed to start" << std::endl;
                shutdown();
                return;
            }
            tcpServer.runTcpServer(&mTcpServerIoContext);
        }

        // Main loop
        while (mRunning.load() == true) {
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
        mRunning.store(false);

        // Stop handling signals
        if (mSignals.has_value()) {
            mSignals->cancel();
            mSignals.reset();
        }
        mSignalGuard.reset();
        mSignalIoContext.stop();

        // Stop tcp server
        if (mConfig.tcpServerEnabled) {
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
