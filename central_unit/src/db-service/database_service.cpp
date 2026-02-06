#include "database_service.h"
#include "database_client.h"

namespace SmartHomeDB {
    DatabaseService &DatabaseService::Instance() {
        static DatabaseService instance;
        return instance;
    }

    bool DatabaseService::initialize(const Config &configStruct, const std::shared_ptr<su::Logger> &logger) {
        if (mIsRunning.load(std::memory_order_acquire)) {
            logger->error("[DB_SERVICE] Initialize called while database service is already initialized");
            return false;
        }

        // Initialize AsyncLogger, using Logger for further initialization
        pLogger = std::make_shared<su::AsyncLogger>(logger, mUtilityIoContext);

        // Start service
        mpService = su::ServiceManager::create(logger, configStruct.serviceName, su::ServiceType::AUTO);
        mpService->setIoContext(mMainIoContext);
        if (!mpService->onInitialize()) {
            logger->error("[DB_SERVICE] Failed to initialize service");
            return false;
        }

        // Enable file logging after service init to avoid overwriting logs of already running instance
        logger->enableFileLoggingIfConfigured();

        mConfig = configStruct;

        // Attempt establishing connection with SmartHome daemon
        mpSocketClient = std::make_unique<si::SocketClient>(&mSocketClientIoContext, pLogger,
                                                            msCLIENT_TARGET_TYPE.data());
        bool isConnectionEstablished = false;
        if (mConfig.uds.isEnabled) {
            isConnectionEstablished = mpSocketClient->connectToServer(mConfig.uds.endpointPath);
            if (isConnectionEstablished)
                pLogger->info("[DB_SERVICE] Connection established via UDS");
        }
        if (!isConnectionEstablished && mConfig.tcp.isEnabled) {
            isConnectionEstablished = mpSocketClient->connectToServer(mConfig.tcp.endpointAddress,
                                                                      mConfig.tcp.endpointPort);
            if (isConnectionEstablished)
                pLogger->info("[DB_SERVICE] Connection established via TCP");
        }
        if (!isConnectionEstablished) {
            logger->error("[DB_SERVICE] Failed to connect with SmartHome daemon");
            return false;
        }

        // Create thread running io context for signal handling and service manager
        mUtilityGuard.emplace(ba::make_work_guard(mUtilityIoContext));
        mUtilityThread = std::thread([this] {
            mUtilityIoContext.run();
        });

        uint dbApiThreadCount = configStruct.dbConnConfig.dbConnections;

        if (dbApiThreadCount > 2) dbApiThreadCount -= 2; // dbApi threads are in n+2 relation to db connections

        mDbApiThreadPool.emplace(dbApiThreadCount);
        mDbApiGuard.emplace(ba::make_work_guard(mDbApiIoContext));
        for (size_t i = 0; i < dbApiThreadCount; i++) {
            ba::post(*mDbApiThreadPool, [this] { mDbApiIoContext.run(); });
        }

        mSocketClientGuard.emplace(ba::make_work_guard(mSocketClientIoContext));
        mSocketClientThread = std::thread([this] {
            mSocketClientIoContext.run();
        });

        mMainGuard.emplace(ba::make_work_guard(mMainIoContext));
        mMainThread = std::thread([this] {
            mMainIoContext.run();
        });

        try {
            mpDatabaseConnectionManager = std::make_shared<DatabaseConnectionManager>(pLogger, mConfig.dbConnConfig);
        } catch (const std::exception &e) {
            logger->criticalf("[DB_SERVICE] Failed to initialize database connection manager: %s", e.what());
            return false;
        }


        mIsInitialized.store(true, std::memory_order_release);
        logger->info("[DB_SERVICE] Database service successfully initialized");

        return true;
    }

    void DatabaseService::run() {
        if (!mIsInitialized.load(std::memory_order_acquire)) {
            pLogger->error("[DB_SERVICE] Database service not initialized");
            return;
        }
        if (mIsRunning.load(std::memory_order_acquire)) {
            pLogger->error("[DB_SERVICE] Database service is already running");
            return;
        }

        mIsRunning.store(true, std::memory_order_release);
        pLogger->info("[DB_SERVICE] Database service starting");

        mpService->onStart();

        // Start signal handling
        mSignals.emplace(mUtilityIoContext);
        for (const auto &sig: msSIGNALS_TO_HANDLE) {
            mSignals->add(sig);
        }

        mSignals->async_wait([this](const bs::error_code &ec, const int sig) {
            signalHandler(ec, sig);
        });

        mpDatabaseClient = std::make_shared<DatabaseClient>(mDbApiIoContext, mpDatabaseConnectionManager);

        mpDatabaseApi = std::make_unique<DatabaseApi>(mpDatabaseClient);

        static constexpr SmartHome::connectionId_t nullConnection = 0;
        mDbApiIoContext.post([this] {
            // Pass DbApi outgoing to SocketClient outgoing
            mpDatabaseApi->initialize([this](const std::string &message) {
                mpSocketClient->handleOutgoing(nullConnection, message.data());
            });

            auto triggerCallback = [this](const std::string &triggerName, const std::string &triggerData) {
                mpDatabaseApi->handleIncomingDbTrigger(triggerName.data(), triggerData.data());
            };

            mpDatabaseClient->initialize(mConfig.dbTriggersToListen, triggerCallback);
        });

        mSocketClientIoContext.post([this] {
            // Pass SocketClient incoming to DbApi incoming
            mpSocketClient->initialize([this](const std::string &message) {
                mpDatabaseApi->handleIncoming(nullConnection, message.data());
            });
        });

        pLogger->info("[DB_SERVICE] Database service running");

        if (mMainThread && mMainThread->joinable()) {
            mMainThread->join();
            mMainThread.reset();
        }

        pLogger->debug("[DB-SERVICE] Main thread finished");

        if (mDbApiThreadPool) {
            mDbApiThreadPool->join();
            mDbApiThreadPool.reset();
        }

        if (mSocketClientThread && mSocketClientThread->joinable()) {
            mSocketClientThread->join();
            mSocketClientThread.reset();
        }

        pLogger->debug("[DB-SERVICE] Threads joined, stopping utils");

        if (!mUtilityIoContext.stopped()) {
            mUtilityIoContext.stop();
        }

        if (mUtilityThread && mUtilityThread->joinable()) {
            mUtilityThread->join();
            mUtilityThread.reset();
        }
    }

    void DatabaseService::shutdown() {
        pLogger->info("[DB_SERVICE] Shutdown requested");

        bool expected = true;
        constexpr bool desired = false;
        if (!mIsRunning.compare_exchange_strong(expected, desired)) {
            pLogger->error("[DB_SERVICE] Shutdown called while database service is not running");
            return;
        }

        mMainGuard.reset();
        mSocketClientGuard.reset();
        mDbApiGuard.reset();


        if (mSignals.has_value()) {
            mSignals->cancel();
            mSignals.reset();
        }

        if (mpService) {
            mpService->onStop();
        }

        if (mpDatabaseClient) {
            mpDatabaseClient->stop();
        }

        // Start shutdown timeout timer
        const auto timeoutTimer = std::make_shared<ba::steady_timer>(mUtilityIoContext, msSHUTDOWN_TIMEOUT);
        timeoutTimer->async_wait([this, timeoutTimer](const bs::error_code &ec) {
            if (!ec) {
                pLogger->warning("[DB_SERVICE] Shutdown timeout - force stopping IO contexts");

                if (!mDbApiIoContext.stopped()) {
                    mDbApiIoContext.stop();
                }
                if (!mSocketClientIoContext.stopped()) {
                    mSocketClientIoContext.stop();
                }
                if (!mMainIoContext.stopped()) {
                    mMainIoContext.stop();
                }
            }
        });

        // Reset objects
        mpDatabaseApi.reset();
        mpDatabaseConnectionManager.reset();
        mpSocketClient.reset();
        mpService.reset();
        mpDatabaseClient.reset();


        pLogger->debug("[DB_SERVICE] Shutdown complete - waiting for threads to join in run()");

        // Reset utility guard
        mUtilityGuard.reset();
    }

    bool DatabaseService::isRunning() const {
        return mIsRunning.load(std::memory_order_acquire);
    }

    ba::io_context &DatabaseService::getUtilityIoContext() {
        return mUtilityIoContext;
    }

    DatabaseService::~DatabaseService() {
        if (isRunning()) {
            shutdown();
            return;
        }

        // Shutdown after failed initialization
        if (!isRunning() && !mIsInitialized.load(std::memory_order_acquire)) {
            if (pLogger) pLogger->warning("[DB-SERVICE] Running cleanup after failed initialization");

            if (mSignals.has_value()) {
                mSignals->cancel();
                mSignals.reset();
            }

            if (mpService) {
                mpService->onStop();
            }

            mMainGuard.reset();
            mSocketClientGuard.reset();
            mDbApiGuard.reset();
            mUtilityGuard.reset();

            if (!mMainIoContext.stopped()) {
                mMainIoContext.stop();
            }

            if (!mDbApiIoContext.stopped()) {
                mDbApiIoContext.stop();
            }

            if (!mSocketClientIoContext.stopped()) {
                mSocketClientIoContext.stop();
            }

            if (!mUtilityIoContext.stopped()) {
                mUtilityIoContext.stop();
            }

            if (mMainThread && mMainThread->joinable()) {
                mMainThread->join();
            }
            if (mDbApiThreadPool) {
                mDbApiThreadPool->join();
            }
            if (mSocketClientThread && mSocketClientThread->joinable()) {
                mSocketClientThread->join();
            }
            if (mUtilityThread && mUtilityThread->joinable()) {
                mUtilityThread->join();
            }
        }
    }

    void DatabaseService::signalHandler(const boost::system::error_code &ec, const int signal) {
        pLogger->debug("[DB_SERVICE] signalHandler called");
        if (!ec) {
            // Handle signal
            switch (signal) {
                case SIGINT:
                case SIGTERM:
                    ba::post(mUtilityIoContext, [this] {
                        shutdown();
                    });
                    return;
                case SIGHUP:
                    pLogger->debug("[DB_SERVICE] Signal not implemented");
                    //TODO implement reload
                    break;
                default:
                    pLogger->debug("[DB_SERVICE] Undefined signal");
                    break;
            }
        } else {
            // Handle signal error
            if (ec.value() == ba::error::operation_aborted) {
                pLogger->debug("[DB_SERVICE] Signal handler aborted");
            } else {
                pLogger->errorf("[DB_SERVICE] Signal handler error: %s", ec.message().c_str());
            }
        }
        if (isRunning()) {
            mSignals->async_wait([this](const bs::error_code &e, const int sig) {
                signalHandler(e, sig);
            });
        }
    }
}
