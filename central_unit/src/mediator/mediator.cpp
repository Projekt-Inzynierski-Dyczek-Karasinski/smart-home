#include "mediator.h"


namespace SmartHomeMediator {
    Mediator &Mediator::Instance() {
        static Mediator instance;
        return instance;
    }

    bool Mediator::initialize(const Config &configStruct, const std::shared_ptr<su::Logger> &logger) {
        if (mIsRunning.load(std::memory_order_acquire)) {
            logger->error("[MEDIATOR] Initialize called while mediator is already initialized");
            return false;
        }

        // Initialize AsyncLogger, using Logger for further initialization
        mpLogger = std::make_shared<su::AsyncLogger>(logger, mMediatorUtilityIoContext);

        // Start service
        mpService = su::ServiceManager::create(logger, msSERVICE_NAME, su::ServiceType::AUTO);
        mpService->setIoContext(mMediatorIoContext);
        if (!mpService->onInitialize()) {
            logger->error("[MEDIATOR] Failed to initialize service");
            return false;
        }

        // Enable file logging after service init to avoid overwriting logs of already running instance
        logger->enableFileLoggingIfConfigured();

        mConfig = configStruct;

        // Attempt establishing connection with SmartHome daemon
        mpApiClient = std::make_unique<si::SocketClient>(&mApiClientIoContext, mpLogger, msCLIENT_TARGET_TYPE.data());
        bool isConnectionEstablished = false;
        if (mConfig.uds.isEnabled) {
            isConnectionEstablished = mpApiClient->connectToServer(mConfig.uds.endpointPath);
            if (isConnectionEstablished)
                mpLogger->info("[MEDIATOR] Connection established via UDS");
        }
        if (!isConnectionEstablished && mConfig.tcp.isEnabled) {
            isConnectionEstablished = mpApiClient->connectToServer(mConfig.tcp.endpointAddress,
                                                                   mConfig.tcp.endpointPort);
            if (isConnectionEstablished)
                mpLogger->info("[MEDIATOR] Connection established via TCP");
        }
        if (!isConnectionEstablished) {
            logger->error("[MEDIATOR] Failed to connect with SmartHome daemon");
            return false;
        }

        // Create thread running io context for signal handling and service manager
        mMediatorUtilityGuard.emplace(ba::make_work_guard(mMediatorUtilityIoContext));
        mMediatorUtilityThread = std::thread([this] {
            mMediatorUtilityIoContext.run();
        });

        mRfClientGuard.emplace(ba::make_work_guard(mRfClientIoContext));
        mRfClientThread = std::thread([this] {
            mRfClientIoContext.run();
        });

        mApiClientGuard.emplace(ba::make_work_guard(mApiClientIoContext));
        mApiClientThread = std::thread([this] {
            mApiClientIoContext.run();
        });

        mMediatorGuard.emplace(ba::make_work_guard(mMediatorIoContext));
        mMediatorThread = std::thread([this] {
            mMediatorIoContext.run();
        });

        try {
            mpRfClient = std::make_unique<RfClient>(
                mRfClientIoContext,
                mpLogger,
                mConfig.rfClient
            );
            mpLogger->debug("[MEDIATOR] RF client created");
        } catch (const std::exception &e) {
            mpLogger->errorf("[MEDIATOR] Failed to create RF client: %s", e.what());
            return false;
        }

        bool isRfClientInitialized = false;
        mpRfClient->initialize([this, &isRfClientInitialized](const bool isSuccessful) {
            if (isSuccessful) {
                mpLogger->debug("[MEDIATOR] RF initialized");
            } else {
                mpLogger->errorf("[MEDIATOR] RF initialization failed");
            }
            isRfClientInitialized = isSuccessful;
        });
        std::this_thread::sleep_for(1s);
        if (!isRfClientInitialized) return false;

        mIsInitialized.store(true, std::memory_order_release);
        logger->info("[MEDIATOR] Mediator successfully initialized");

        return true;
    }

    void Mediator::run() {
        if (!mIsInitialized.load(std::memory_order_acquire)) {
            mpLogger->error("[MEDIATOR] Mediator not initialized");
            return;
        }
        if (mIsRunning.load(std::memory_order_acquire)) {
            mpLogger->error("[MEDIATOR] Mediator is already running");
            return;
        }

        mIsRunning.store(true, std::memory_order_release);
        mpLogger->info("[MEDIATOR] Mediator starting");

        mpService->onStart();

        // Start signal handling
        mSignals.emplace(mMediatorUtilityIoContext);
        for (const auto &sig: msSIGNALS_TO_HANDLE) {
            mSignals->add(sig);
        }

        mSignals->async_wait([this](const bs::error_code &ec, const int sig) {
            signalHandler(ec, sig);
        });

        static constexpr int nullConnection = 0; // Connection ID is not needed when communicating only with IPC server
        mpRfApi = std::make_unique<RfApi>(mpRfClient);
        mpRfApi->initialize([this](const std::string &message) {
            mpApiClient->handleOutgoing(nullConnection, message.data());
        });

        mApiClientIoContext.post([this] {
            mpApiClient->initialize([this](const std::string &message) {
                mpRfApi->handleIncoming(nullConnection, message.data());
            });
        });

        ba::co_spawn(mRfClientIoContext, mpRfClient->run([this](const std::string &message) {
            mpRfApi->handleOutgoing(nullConnection, message.data());
        }), ba::detached);

        mpLogger->info("[MEDIATOR] Mediator running");


        if (mMediatorThread && mMediatorThread->joinable()) {
            mMediatorThread->join();
            mMediatorThread.reset();
        }

        mpLogger->debug("[MEDIATOR] Main thread finished");

        // Join other threads
        if (mRfClientThread && mRfClientThread->joinable()) {
            mRfClientThread->join();
            mRfClientThread.reset();
        }

        if (mApiClientThread && mApiClientThread->joinable()) {
            mApiClientThread->join();
            mApiClientThread.reset();
        }

        mpLogger->debug("[MEDIATOR] Threads joined, joining utils thread");

        if (!mMediatorUtilityIoContext.stopped()) {
            mMediatorUtilityIoContext.stop();
        }

        // Join utilities thread last
        if (mMediatorUtilityThread && mMediatorUtilityThread->joinable()) {
            mMediatorUtilityThread->join();
            mMediatorUtilityThread.reset();
        }
    }

    void Mediator::shutdown() {
        mpLogger->info("[MEDIATOR] Shutdown requested");

        bool expected = true;
        constexpr bool desired = false;
        if (!mIsRunning.compare_exchange_strong(expected, desired)) {
            mpLogger->error("[MEDIATOR] Shutdown called while mediator is not running");
            return;
        }

        mMediatorGuard.reset();
        mApiClientGuard.reset();
        mRfClientGuard.reset();


        if (mSignals.has_value()) {
            mSignals->cancel();
            mSignals.reset();
        }

        if (mpService) {
            mpService->onStop();
        }

        // Start shutdown timeout timer
        const auto timeoutTimer = std::make_shared<ba::steady_timer>(mMediatorUtilityIoContext, msSHUTDOWN_TIMEOUT);
        timeoutTimer->async_wait([this](const bs::error_code &ec) {
            if (!ec) {
                mpLogger->warning("[MEDIATOR] Shutdown timeout - force stopping IO contexts");

                if (!mRfClientIoContext.stopped()) {
                    mRfClientIoContext.stop();
                }
                if (!mApiClientIoContext.stopped()) {
                    mApiClientIoContext.stop();
                }
                if (!mMediatorIoContext.stopped()) {
                    mMediatorIoContext.stop();
                }
            }
        });

        // Reset objects
        mpRfApi.reset();
        mpRfClient.reset();
        mpApiClient.reset();
        mpService.reset();

        // Reset utility guard
        mMediatorUtilityGuard.reset();

        mpLogger->debug("[MEDIATOR] Shutdown complete - waiting for threads to join in run()");
    }

    bool Mediator::isRunning() const {
        return mIsRunning.load(std::memory_order_acquire);
    }

    ba::io_context &Mediator::getIoContext() {
        return mMediatorIoContext;
    }

    Mediator::Mediator() = default;

    Mediator::~Mediator() {
        // FIXME rework shutdown - implement shutdown from db-service
        if (isRunning()) {
            shutdown();
        }
    }

    void Mediator::signalHandler(const boost::system::error_code &ec, const int signal) {
        mpLogger->debug("[MEDIATOR] signalHandler called");
        if (!ec) {
            // Handle signal
            switch (signal) {
                case SIGINT:
                case SIGTERM:
                    ba::post(mMediatorUtilityIoContext, [this] {
                        shutdown();
                    });
                    return;
                case SIGHUP:
                    mpLogger->debug("[MEDIATOR] Signal not implemented");
                    //TODO implement reload
                    break;
                default:
                    mpLogger->debug("[MEDIATOR] Undefined signal");
                    break;
            }
        } else {
            // Handle signal error
            if (ec.value() == ba::error::operation_aborted) {
                mpLogger->debug("[MEDIATOR] Signal handler aborted");
            } else {
                mpLogger->errorf("[MEDIATOR] Signal handler error: %s", ec.message().c_str());
            }
        }
        if (isRunning()) {
            mSignals->async_wait([this](const bs::error_code &e, const int sig) {
                signalHandler(e, sig);
            });
        }
    }
}
