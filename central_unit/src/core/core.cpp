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

    bool Core::initialize(const Config &configStruct, const std::shared_ptr<Utils::Logger> &logger) {
        if (mIsInitialized.load()) {
            logger->error("[CORE] Core already initialized");
            return false;
        }


        // Initialize AsyncLogger, using Logger for further initialization
        mLogger = std::make_shared<Utils::AsyncLogger>(logger, mCoreIoContext);

        mpService = Service::ServiceManager::create(logger, Utils::ServiceType::AUTO);
        if (!mpService->onInitialize()) {
            logger->error("[CORE] Failed to initialize service");
            stopCoreThread();
            return false;
        }
        // Enable file logging after service init to avoid overwriting logs of already running instance
        logger->enableFileLoggingIfConfigured();

        // Create thread running io context for signal handling and service manager
        mCoreGuard.emplace(ba::make_work_guard(mCoreIoContext));
        mCoreThread = std::thread([this] {
            mCoreIoContext.run();
        });


        logger->debug("[CORE] Starting Core initialization");
        // Save config
        mConfig = configStruct;

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
                if (mConfig.ipcServerThreads > ms_HIGH_THREAD_COUNT_LIMIT) {
                    logger->warningf("[CORE] IPC server thread count exceeds recommended limit (%d active threads)",
                                    mConfig.ipcServerThreads);
                }
                threadCount = mConfig.ipcServerThreads;
                break;
        }

        if (threadCount < 1) {
            logger->error("[CORE] IPC server thread count less than 1, setting value to 1");
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
        logger->debug("[CORE] Core successfully initialized");

        return true;
    }

    void Core::run() {
        if (!mIsInitialized.load()) {
            throw std::runtime_error("[CORE] Core is not initialized");
        }
        if (mIsRunning.load()) {
            mLogger->error("[CORE] Core is already running");
            return;
        }

        // Begin core
        mIsRunning.store(true);
        mLogger->debug("[CORE] Starting Core run loop");

        mpService->onStart();

        // Start signal handling
        mSignals.emplace(mCoreIoContext, SIGINT, SIGTERM); //TODO add more signals
        mSignals->async_wait([this](const bs::error_code &ec, const int sig) {
            signalHandler(ec, sig);
        });


        auto &socketServer = IPC::SocketServer::Instance();
        IPC::SocketServer::Config config = {mConfig.tcp, mConfig.uds};

        if (!socketServer.initializeSocketServer(&mSocketServerIoContext, config, mLogger)) {
            mLogger->critical("[CORE] Failed to initialize IPC socket server");
            shutdown();
            return;
        }
        socketServer.runSocketServer(&mSocketServerIoContext);


        // Main loop
        while (mIsRunning.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            //TODO implement watchdog
        }
        mLogger->debug("[CORE] Exiting");
        stopCoreThread();
    }

    void Core::shutdown() {
        //TODO add is shutting down check
        mLogger->debug("[CORE] Starting core shutdown");

        // Stop core main loop
        mIsRunning.store(false);

        mpService->onStop();

        // Stop TCP server
        if (mConfig.tcp.isEnabled) {
            mLogger->debug("[CORE] Shutting down IPC socket server");
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

        mLogger->debug("[CORE] Shutdown complete");
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
                    mLogger->debug("[CORE] Not implemented");
                    //TODO implement reload
                    break;
                default:
                    //TODO handle default
                    break;
            }
        } else {
            // Handle signal error
            mLogger->errorf("[CORE] Signal handler error: %s", ec.message().c_str());
        }
    }

    void Core::stopCoreThread() {
        if (mCoreThread.has_value()) {
            mCoreGuard.reset();
            mCoreThread->join();
            mCoreThread.reset();
        }
    }
}
