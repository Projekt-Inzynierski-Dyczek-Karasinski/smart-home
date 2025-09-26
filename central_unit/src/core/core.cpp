#include "core.h"
#include "config_manager.h"
#include "service/service_manager.h"
#include "core_actions.h"

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
        mpLogger = std::make_shared<Utils::AsyncLogger>(logger, mCoreUtilityIoContext);

        mpService = Service::ServiceManager::create(mpLogger, Utils::ServiceType::AUTO);
        if (!mpService->onInitialize()) {
            logger->error("[CORE] Failed to initialize service");
            stopCoreUtilityThread();
            return false;
        }
        // Enable file logging after service init to avoid overwriting logs of already running instance
        logger->enableFileLoggingIfConfigured();

        // Create thread running io context for signal handling and service manager
        mCoreUtilityGuard.emplace(ba::make_work_guard(mCoreUtilityIoContext));
        mCoreUtilityThread = std::thread([this] {
            mCoreUtilityIoContext.run();
        });


        logger->debug("[CORE] Starting Core initialization");
        // Save config
        mConfig = configStruct;

        // get IPC server thread count
        uint ipcThreadCount = getThreadCount(mConfig.ipcServerThreads);
        if (ipcThreadCount > ms_HIGH_THREAD_COUNT_LIMIT) {
            mpLogger->warningf("[CORE] IPC server thread count exceeds recommended limit (%d active threads)",
                              ipcThreadCount);
        }
        if (ipcThreadCount < 1) {
            mpLogger->error("[CORE] Invalid IPC server thread count value (less than 1), setting value to default");
            ipcThreadCount = Config::TWO_THREADS;
        }
        //Create TCP server threads with io context
        mSocketServerThreadPool.emplace(ipcThreadCount);
        mSocketServerGuard.emplace(ba::make_work_guard(mSocketServerIoContext));
        for (size_t i = 0; i < ipcThreadCount; i++) {
            ba::post(*mSocketServerThreadPool, [this] {
                mSocketServerIoContext.run();
            });
        }

        // get Core main thread count
        uint coreThreadCount = getThreadCount(mConfig.coreMainThreads);
        if (coreThreadCount > ms_HIGH_THREAD_COUNT_LIMIT) {
            mpLogger->warningf("[CORE] Core main thread count exceeds recommended limit (%d active threads)",
                              coreThreadCount);
        }
        if (coreThreadCount < 1) {
            mpLogger->error("[CORE] Invalid Core main thread count value (less than 1), setting value to default");
            coreThreadCount = Config::TWO_THREADS;
        }
        // Create Core main thread pool with io context
        mCoreThreadPool.emplace(coreThreadCount);
        mCoreGuard.emplace(ba::make_work_guard(mCoreIoContext));
        for (size_t i = 0; i < coreThreadCount; i++) {
            ba::post(*mCoreThreadPool, [this] { mCoreIoContext.run(); });
        }

        // get Core worker thread count
        uint coreWorkerThreadCount = getThreadCount(mConfig.coreWorkerThreads);
        if (coreWorkerThreadCount > ms_HIGH_THREAD_COUNT_LIMIT) {
            mpLogger->warningf("[CORE] Core worker thread count exceeds recommended limit (%d active threads)",
                              coreWorkerThreadCount);
        }
        if (coreWorkerThreadCount < 1) {
            mpLogger->error("[CORE] Invalid Core worker thread count value (less than 1), setting value to default");
            coreWorkerThreadCount = Config::SINGLE_THREAD;
        }
        // Create Core worker thread pool with io context
        mCoreWorkerThreadPool.emplace(coreWorkerThreadCount);
        mCoreWorkerGuard.emplace(ba::make_work_guard(mCoreWorkerIoContext));
        for (size_t i = 0; i < coreWorkerThreadCount; i++) {
            ba::post(*mCoreWorkerThreadPool, [this] { mCoreWorkerIoContext.run(); });
        }
        
        mIsInitialized.store(true);
        logger->debug("[CORE] Core successfully initialized");

        return true;
    }

    void Core::run() {
        if (!mIsInitialized.load()) {
            throw std::runtime_error("[CORE] Core is not initialized");
        }
        if (mIsRunning.load()) {
            mpLogger->error("[CORE] Core is already running");
            return;
        }

        // Begin core
        mIsRunning.store(true);
        mpLogger->debug("[CORE] Starting Core run loop");

        mpService->onStart();

        // Start signal handling
        mSignals.emplace(mCoreUtilityIoContext); //TODO add more signals
        for (const auto &sig: ms_SIGNALS_TO_HANDLE) {
            mSignals->add(sig);
        }

        mSignals->async_wait([this](const bs::error_code &ec, const int sig) {
            signalHandler(ec, sig);
        });

        auto &socketServer = IPC::SocketServer::Instance();
        IPC::SocketServer::Config config = {mConfig.tcp, mConfig.uds};
        mpApi = std::make_shared<API::InternalApi>();

        if (!socketServer.initializeSocketServer(&mSocketServerIoContext, config, mpApi, mpLogger)) {
            mpLogger->critical("[CORE] Failed to initialize IPC socket server");
            shutdown();
            return;
        }
        socketServer.runSocketServer();

        //TODO implement watchdog working in CoreThread
        mCoreThreadPool->join();
        mCoreThreadPool.reset();

        mpLogger->debug("[CORE] Exiting");
        stopCoreUtilityThread();
    }

    void Core::shutdown() {
        //TODO add is shutting down check
        mpLogger->debug("[CORE] Starting core shutdown");

        // Signal and initialize shutdown
        mIsRunning.store(false);
        auto &ipcServer = IPC::SocketServer::Instance();
        ipcServer.stopAcceptors();

        CoreActions::onCoreShutdown();

        // Start shutdown timeout timer
        ba::steady_timer shutdownTimeout(mCoreUtilityIoContext, ms_SHUTDOWN_TIMEOUT);
        shutdownTimeout.async_wait([this](const bs::error_code &ec) {
            if (!ec) {
                mCoreWorkerThreadPool->stop();
                mCoreThreadPool->stop();
                mSocketServerThreadPool->stop();
            }
        });

        // Join worker thread
        mCoreWorkerGuard.reset();
        mCoreWorkerThreadPool->join();

        // Disable core thread guard for later join
        mCoreGuard->reset();

        // Stop IPC server
        if (mConfig.tcp.isEnabled || mConfig.uds.isEnabled) {
            mpLogger->debug("[CORE] Shutting down IPC socket server");
            if (ipcServer.isRunning()) {
                ipcServer.stopSocketServer();
            }
        }

        // Waiting for IPC server threads to finish
        if (mSocketServerThreadPool.has_value()) {
            mSocketServerGuard.reset();
            mSocketServerThreadPool->join();
            mSocketServerThreadPool.reset();
        }

        mpService->onStop();

        // Stop handling signals
        if (mSignals.has_value()) {
            mSignals->cancel();
            mSignals.reset();
        }

        mpLogger->debug("[CORE] Shutdown complete");
    }

    bool Core::isRunning() const {
        return mIsRunning.load();
    }

    ba::io_context &Core::getCoreUtilityIoContext() {
        return mCoreUtilityIoContext;
    }


    ba::io_context &Core::getCoreWorkerIoContext() {
        return mCoreWorkerIoContext;
    }

    ba::io_context &Core::getCoreIoContext() {
        return mCoreIoContext;
    }

    void Core::signalHandler(const boost::system::error_code &ec, const int signal) {
        mpLogger->debug("[CORE] signalHandler called");
        if (!ec) {
            // Handle signal
            switch (signal) {
                case SIGINT:
                case SIGTERM:
                    ba::post(mCoreUtilityIoContext, [this] {
                        shutdown();
                    });
                    return;
                case SIGHUP:
                    mpLogger->debug("[CORE] Not implemented");
                    //TODO implement reload
                    break;
                default:
                    mpLogger->debug("[CORE] Undefined signal");
                    //TODO handle default
                    break;
            }
        } else {
            // Handle signal error
            if (ec.value() == ba::error::operation_aborted) {
                mpLogger->debug("Signal handler aborted");
            } else {
                mpLogger->errorf("[CORE] Signal handler error: %s", ec.message().c_str());
            }
        }
        if (isRunning()) {
            mSignals->async_wait([this](const bs::error_code &e, const int sig) {
                signalHandler(e, sig);
            });
        }
    }

    void Core::stopCoreUtilityThread() {
        if (mCoreUtilityThread.has_value()) {
            mCoreUtilityGuard.reset();
            mCoreUtilityThread->join();
            mCoreUtilityThread.reset();
        }
    }

    uint Core::getThreadCount(const int threadCountConfigValue) {
        static const uint coreCount = std::thread::hardware_concurrency();

        if (threadCountConfigValue == Config::ALL_CPU_CORES) {
            return coreCount;
        }
        if (threadCountConfigValue < 0) {
            // For negative values divide coreCount by |value| + 1
            return std::ceil(coreCount / static_cast<double>(abs(threadCountConfigValue) + 1));
        }

        return threadCountConfigValue;
    }
}
