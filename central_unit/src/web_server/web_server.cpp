#include "web_server.h"
#include "routes/core_routes.h"
#include "routes/sensor_routes.h"
#include "routes/actuator_routes.h"
#include "routes/module_routes.h"
#include "routes/database_routes.h"
#include "routes/static_routes.h"

namespace SmartHomeWebServer {
    WebServer &WebServer::Instance() {
        static WebServer instance;
        return instance;
    }

    bool WebServer::initialize(const Config &config, const std::shared_ptr<su::Logger> &logger) {
        if (mIsRunning.load(std::memory_order_acquire)) {
            logger->error("[WEB_SERVER] Initialize called while web server is already running");
            return false;
        }

        mConfig = config;

        // Initialize AsyncLogger on utility context
        pLogger = std::make_shared<su::AsyncLogger>(logger, mUtilityIoContext);

        // Initialize service manager
        mpService = su::ServiceManager::create(logger, config.serviceName, su::ServiceType::AUTO);

        // Utility thread (signals, logger)
        mUtilityGuard.emplace(ba::make_work_guard(mUtilityIoContext));
        mUtilityThread = std::thread([this] {
            mUtilityIoContext.run();
        });

        // Socket client thread (ApiClient IPC)
        mSocketClientGuard.emplace(ba::make_work_guard(mSocketClientIoContext));
        mSocketClientThread = std::thread([this] {
            mSocketClientIoContext.run();
        });

        // Initialize ApiClient
        mpApiClient = std::make_unique<ApiClient>(pLogger, mSocketClientIoContext);
        if (!mpApiClient->initialize(config.apiClient)) {
            logger->critical("[WEB_SERVER] Failed to connect to SmartHome daemon");
            return false;
        }

        // Initialize Crow app
        mpApp = std::make_unique<crow::App<crow::CORSHandler> >();
        mpApp->signal_clear(); // Disable Crow's signal handling

        // Configure CORS
        auto &cors = mpApp->get_middleware<crow::CORSHandler>();
        cors.global()
                .origin(config.cors.allowOrigin)
                .methods("GET"_method, "POST"_method, "PUT"_method, "DELETE"_method, "OPTIONS"_method)
                .headers("*");

        // Register routes
        registerRoutes();

        mIsInitialized.store(true, std::memory_order_release);
        pLogger->info("[WEB_SERVER] Web server successfully initialized");

        return true;
    }

    void WebServer::run() {
        if (!mIsInitialized.load(std::memory_order_acquire)) {
            pLogger->error("[WEB_SERVER] Web server not initialized");
            return;
        }
        if (mIsRunning.load(std::memory_order_acquire)) {
            pLogger->error("[WEB_SERVER] Web server is already running");
            return;
        }

        mIsRunning.store(true, std::memory_order_release);
        pLogger->info("[WEB_SERVER] Web server starting");

        mpService->onStart();

        // Start signal handling
        mSignals.emplace(mUtilityIoContext);
        for (const auto &sig: msSIGNALS_TO_HANDLE) {
            mSignals->add(sig);
        }
        mSignals->async_wait([this](const bs::error_code &ec, const int sig) {
            signalHandler(ec, sig);
        });

        pLogger->infof("[WEB_SERVER] Listening on port %d", mConfig.http.port);

        // Blocking - runs Crow event loop on main thread
        mpApp->port(mConfig.http.port).multithreaded().run();

        // After app.stop() returns, join worker threads
        pLogger->debug("[WEB_SERVER] Crow stopped, joining threads");

        if (mSocketClientThread && mSocketClientThread->joinable()) {
            mSocketClientThread->join();
            mSocketClientThread.reset();
        }

        if (!mUtilityIoContext.stopped()) {
            mUtilityIoContext.stop();
        }

        if (mUtilityThread && mUtilityThread->joinable()) {
            mUtilityThread->join();
            mUtilityThread.reset();
        }
    }

    void WebServer::shutdown() {
        pLogger->info("[WEB_SERVER] Shutdown requested");

        bool expected = true;
        if (!mIsRunning.compare_exchange_strong(expected, false)) {
            pLogger->error("[WEB_SERVER] Shutdown called while web server is not running");
            return;
        }

        // Release work guards so IO contexts can finish
        mSocketClientGuard.reset();

        if (mSignals.has_value()) {
            mSignals->cancel();
            mSignals.reset();
        }

        if (mpService) {
            mpService->onStop();
        }

        // Start shutdown timeout timer
        auto timeoutTimer = std::make_shared<ba::steady_timer>(mUtilityIoContext, msSHUTDOWN_TIMEOUT);
        timeoutTimer->async_wait([this, timeoutTimer](const bs::error_code &ec) {
            if (!ec) {
                pLogger->warning("[WEB_SERVER] Shutdown timeout - force stopping IO contexts");
                if (!mSocketClientIoContext.stopped()) {
                    mSocketClientIoContext.stop();
                }
            }
        });

        // Stop Crow (unblocks run())
        if (mpApp) {
            mpApp->stop();
        }

        // Reset owned objects
        mpApiClient.reset();

        pLogger->debug("[WEB_SERVER] Shutdown complete - waiting for threads to join in run()");

        // Utility guard last - keeps logger alive
        mUtilityGuard.reset();
    }

    bool WebServer::isRunning() const {
        return mIsRunning.load(std::memory_order_acquire);
    }

    WebServer::~WebServer() {
        if (isRunning()) {
            shutdown();
            return;
        }

        // Cleanup after failed initialization
        if (!mIsRunning.load(std::memory_order_acquire) && !mIsInitialized.load(std::memory_order_acquire)) {
            if (pLogger) pLogger->warning("[WEB_SERVER] Running cleanup after failed initialization");

            if (mSignals.has_value()) {
                mSignals->cancel();
                mSignals.reset();
            }

            if (mpService) {
                mpService->onStop();
            }

            mSocketClientGuard.reset();
            mUtilityGuard.reset();

            if (mpApp) {
                mpApp->stop();
            }

            if (!mSocketClientIoContext.stopped()) {
                mSocketClientIoContext.stop();
            }
            if (!mUtilityIoContext.stopped()) {
                mUtilityIoContext.stop();
            }

            if (mSocketClientThread && mSocketClientThread->joinable()) {
                mSocketClientThread->join();
            }
            if (mUtilityThread && mUtilityThread->joinable()) {
                mUtilityThread->join();
            }
        }
    }

    void WebServer::signalHandler(const bs::error_code &ec, const int signal) {
        pLogger->debug("[WEB_SERVER] signalHandler called");
        if (!ec) {
            switch (signal) {
                case SIGINT:
                case SIGTERM:
                    ba::post(mUtilityIoContext, [this] {
                        shutdown();
                    });
                    return;
                case SIGHUP:
                    pLogger->debug("[WEB_SERVER] SIGHUP - reload not implemented");
                    break;
                default:
                    pLogger->debug("[WEB_SERVER] Undefined signal");
                    break;
            }
        } else {
            if (ec.value() == ba::error::operation_aborted) {
                pLogger->debug("[WEB_SERVER] Signal handler aborted");
            } else {
                pLogger->errorf("[WEB_SERVER] Signal handler error: %s", ec.message().c_str());
            }
        }
        if (isRunning()) {
            mSignals->async_wait([this](const bs::error_code &e, const int sig) {
                signalHandler(e, sig);
            });
        }
    }

    void WebServer::registerRoutes() const {
        registerCoreRoutes(*mpApp, *mpApiClient);
        registerSensorRoutes(*mpApp, *mpApiClient);
        registerActuatorRoutes(*mpApp, *mpApiClient);
        registerModuleRoutes(*mpApp, *mpApiClient);
        registerDatabaseRoutes(*mpApp, *mpApiClient);
        registerStaticRoutes(*mpApp, mConfig.http.staticRoot);
    }
}
