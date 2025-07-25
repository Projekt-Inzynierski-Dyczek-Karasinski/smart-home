#include "core.h"
#include "config_manager.h"
#include "service/service_manager.h"

#include <chrono>
#include <cmath>
#include <iostream>

#include <boost/asio.hpp>

namespace ba = boost::asio;
namespace bai = boost::asio::ip;
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

        mpService = Service::ServiceManager::create();
        if (!mpService->onInitialize()) {
            std::cerr << "Failed to initialize service" << std::endl;
            return false;
        }

        std::cout << "Initializing core" << std::endl;
        // Save config
        mConfig = configStruct;

        // Create thread running io context for signal handling and service manager
        mCoreGuard.emplace(ba::make_work_guard(mCoreIoContext));
        mCoreThread = std::thread([this] {
            mCoreIoContext.run();
        });


        // Set IPC server thread count
        uint threadCount;

        switch (mConfig.ipcServerThreads) {
            case Config::HALF_CPU_CORES:
                threadCount = std::ceil(std::thread::hardware_concurrency() / 2);
                break;
            case Config::ALL_CPU_CORES:
                threadCount = std::thread::hardware_concurrency();
                break;
            default:
                if (mConfig.ipcServerThreads > mHighThreadCountLimit) {
                    std::cerr << "Core init warning: IPC server possible excessive thread count (" <<
                            mConfig.ipcServerThreads << " threads)" << std::endl;
                }
                threadCount = mConfig.ipcServerThreads;
                break;
        }

        if (threadCount < 1) {
            std::cerr << "Core init error: IPC server thread count less than 1, setting value to 1" << std::endl;
            threadCount = 1;
        }

        //Create TCP server threads with io context
        mSocketServerThreadPool.emplace(threadCount);
        mSocketServerGuard.emplace(ba::make_work_guard(mSocketServerIoContext));
        for (size_t i = 0; i < threadCount; i++) {
            ba::post(*mSocketServerThreadPool, [this] {
                mSocketServerIoContext.run();
            });
        }
        IPC::SocketServer::Instance();


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

        mpService->onStart();

        // Start signal handling
        mSignals.emplace(mCoreIoContext, SIGINT, SIGTERM); //TODO add more signals
        mSignals->async_wait([this](const bs::error_code &ec, const int sig) {
            signalHandler(ec, sig);
        });


        auto &socketServer = IPC::SocketServer::Instance();
        IPC::SocketServer::Config config = {mConfig.tcp, mConfig.uds};

        if (!socketServer.initializeSocketServer(&mSocketServerIoContext, config)) {
            std::cerr << "Error IPC server failed to start" << std::endl;
            shutdown();
            return;
        }
        socketServer.runSocketServer(&mSocketServerIoContext);


        // Main loop
        while (mIsRunning.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        std::cout << "Core finished running" << std::endl;

        // Stopping signal thread
        if (mCoreThread.has_value()) {
            mCoreGuard.reset();
            mCoreIoContext.stop();
            mCoreThread->join();
            mCoreThread.reset();
        }
        std::cout << "Core cleanup complete" << std::endl;
    }

    void Core::shutdown() {
        std::cout << "Shutting down core" << std::endl;

        // Stop core main loop
        mIsRunning.store(false);

        mpService->onStop();

        // Stop TCP server
        if (mConfig.tcp.isEnabled) {
            std::cout << "Stopping IPC server" << std::endl;
            auto &tcpServer = IPC::SocketServer::Instance();
            if (tcpServer.isRunning()) {
                tcpServer.stopSocketServer();
            }
        }

        // Waiting for IPC server threads to finish
        if (mSocketServerThreadPool.has_value()) {
            mSocketServerGuard.reset();
            mSocketServerThreadPool->stop();
            mSocketServerThreadPool->join();
            mSocketServerThreadPool.reset();
        }

        // Stop handling signals
        if (mSignals.has_value()) {
            mSignals->cancel();
            mSignals.reset();
        }

        std::cout << "Shutdown complete" << std::endl;
    }

    bool Core::isRunning() const {
        return mIsRunning.load();
    }

    ba::io_context &Core::getCoreIoContext() {
        return mCoreIoContext;
    }


    void Core::signalHandler(const boost::system::error_code &ec, const int signal) {
        if (!ec) {
            // Handle signal
            switch (signal) {
                case SIGINT:
                case SIGTERM:
                    shutdown();
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
